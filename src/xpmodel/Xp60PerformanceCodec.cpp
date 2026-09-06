#include "xpmodel/Xp60PerformanceCodec.h"

#include "protocol/TransferPacing.h"

#include <algorithm>
#include <utility>

namespace xp60studio::xpmodel {

std::size_t Xp60PerformanceDecodeResult::errorCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(issues.begin(), issues.end(), [](const BlockIssueAt& i) {
        return isBlockIssueError(i.issue.kind);
    })) + missing.size();
}

bool Xp60PerformanceDecodeResult::hasWarnings() const noexcept
{
    return std::any_of(issues.begin(), issues.end(),
                       [](const BlockIssueAt& i) { return !isBlockIssueError(i.issue.kind); });
}

std::string Xp60PerformanceDecodeResult::describe() const
{
    std::string out;
    for (const auto& m : missing) {
        out += m.block + ": " + std::to_string(m.coverage.covered) + " / " + std::to_string(m.coverage.requested)
            + " bytes present at " + m.address.toHexString();
        if (m.coverage.firstMissing) {
            out += " (first missing " + m.coverage.firstMissing->toHexString() + ")";
        }
        out += "\n";
    }
    for (const auto& i : issues) {
        out += i.block + ": " + std::string(blockIssueKindName(i.issue.kind)) + " " + i.issue.detail + "\n";
    }
    return out;
}

Xp60PerformanceDecodeResult Xp60PerformanceCodec::decode(const MemoryImage& image, const roland::RolandAddress& base)
{
    Xp60PerformanceDecodeResult result;
    std::optional<BlockValues> common;
    std::array<std::optional<BlockValues>, PartIndex::kCount> parts;

    for (const auto& block : Xp60PerformanceLayout::blocks()) {
        const auto address = base.plus(block.offset);
        if (!address) {
            result.missing.push_back(
                {std::string(block.name), base, MemoryImage::Coverage{block.size, 0, base}});
            continue;
        }
        const auto bytes = image.read(*address, block.size);
        if (!bytes) {
            result.missing.push_back({std::string(block.name), *address, image.coverage(*address, block.size)});
            continue;
        }
        auto decoded = BlockCodec::decode(*block.table, *bytes);
        for (auto& issue : decoded.issues) {
            result.issues.push_back({std::string(block.name), std::move(issue)});
        }
        if (!decoded.ok()) {
            continue;
        }
        if (block.part) {
            parts[block.part->index()] = std::move(decoded.values);
        } else {
            common = std::move(decoded.values);
        }
    }

    // All seventeen blocks or nothing: a Performance missing a Part is not a
    // Performance, and half of one would be worse than a reported failure.
    if (common && std::all_of(parts.begin(), parts.end(), [](const auto& p) { return p.has_value(); })) {
        // BlockValues has no default constructor by design — an empty one would
        // be a block with no table behind it — so the array is built by moving
        // all sixteen in at once rather than default-constructing and filling.
        auto build = [&parts]<std::size_t... I>(std::index_sequence<I...>) {
            return std::array<BlockValues, PartIndex::kCount>{std::move(*parts[I])...};
        };
        result.performance.emplace(std::move(*common),
                                   build(std::make_index_sequence<PartIndex::kCount>{}));
    }
    return result;
}

std::array<roland::ByteVector, Xp60PerformanceCodec::kBlockCount>
Xp60PerformanceCodec::blockBytes(const Xp60Performance& performance)
{
    std::array<roland::ByteVector, kBlockCount> bytes;
    bytes[0] = BlockCodec::encode(performance.common());
    for (const auto part : PartIndex::all()) {
        bytes[static_cast<std::size_t>(part.number())] = BlockCodec::encode(performance.part(part));
    }
    return bytes;
}

MemoryImage Xp60PerformanceCodec::encodeToImage(const Xp60Performance& performance,
                                                const roland::RolandAddress& base)
{
    MemoryImage image;
    const auto bytes = blockBytes(performance);
    const auto blocks = Xp60PerformanceLayout::blocks();
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        if (const auto address = base.plus(blocks[i].offset)) {
            image.write(*address, bytes[i]);
        }
    }
    return image;
}

std::vector<roland::RolandSysExMessage> Xp60PerformanceCodec::encodeToDataSets(
    const Xp60Performance& performance, roland::RolandDeviceId deviceId, const roland::RolandModelId& modelId,
    const roland::RolandAddress& base, std::size_t maxPayloadBytes)
{
    std::vector<roland::RolandSysExMessage> messages;
    const auto bytes = blockBytes(performance);
    const auto blocks = Xp60PerformanceLayout::blocks();
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const auto address = base.plus(blocks[i].offset);
        if (!address) {
            return {};
        }
        const auto whole = roland::RolandSysExMessage::dataSet(deviceId, modelId, *address, bytes[i]);
        if (!whole) {
            return {};
        }
        auto chunks = protocol::chunkDataSet(*whole, maxPayloadBytes);
        if (chunks.empty()) {
            return {};
        }
        messages.insert(messages.end(), chunks.begin(), chunks.end());
    }
    return messages;
}

std::vector<roland::RolandSysExMessage> Xp60PerformanceCodec::encodeChangesToDataSets(
    const Xp60Performance& before, const Xp60Performance& after, roland::RolandDeviceId deviceId,
    const roland::RolandModelId& modelId, const roland::RolandAddress& base, std::size_t maxPayloadBytes)
{
    const auto oldBytes = blockBytes(before);
    const auto newBytes = blockBytes(after);
    const auto blocks = Xp60PerformanceLayout::blocks();
    std::vector<roland::RolandSysExMessage> messages;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const auto& bytes = newBytes[i];
        std::size_t first = 0, last = bytes.size();
        while (first < last && bytes[first] == oldBytes[i][first]) ++first;
        if (first == last) continue;
        while (bytes[last - 1] == oldBytes[i][last - 1]) --last;
        // Widen to whole parameters: half of a nibble pair is not a value.
        for (const auto& parameter : blocks[i].table->parameters()) {
            if (parameter.offset < first && parameter.endOffset() > first) first = parameter.offset;
            if (parameter.offset < last && parameter.endOffset() > last) last = parameter.endOffset();
        }
        while (first < last) {
            auto end = std::min(last, first + maxPayloadBytes);
            for (const auto& parameter : blocks[i].table->parameters()) {
                if (parameter.offset < end && parameter.endOffset() > end) end = parameter.offset;
            }
            if (end <= first) return {}; // payload cap cannot hold one parameter
            const auto address = base.plus(blocks[i].offset + static_cast<std::uint32_t>(first));
            if (!address) return {};
            const auto message = roland::RolandSysExMessage::dataSet(
                deviceId, modelId, *address, roland::ByteVector(bytes.data() + first, bytes.data() + end));
            if (!message) return {};
            messages.push_back(*message);
            first = end;
        }
    }
    return messages;
}

} // namespace xp60studio::xpmodel
