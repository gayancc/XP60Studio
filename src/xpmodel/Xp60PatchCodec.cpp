#include "xpmodel/Xp60PatchCodec.h"

#include "protocol/TransferPacing.h"

#include <algorithm>

namespace xp60studio::xpmodel {

std::size_t Xp60PatchDecodeResult::errorCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(issues.begin(), issues.end(), [](const BlockIssueAt& i) {
        return isBlockIssueError(i.issue.kind);
    })) + missing.size();
}

bool Xp60PatchDecodeResult::hasWarnings() const noexcept
{
    return std::any_of(issues.begin(), issues.end(), [](const BlockIssueAt& i) { return !isBlockIssueError(i.issue.kind); });
}

std::string Xp60PatchDecodeResult::describe() const
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

Xp60PatchDecodeResult Xp60PatchCodec::decode(const MemoryImage& image, const roland::RolandAddress& patchBase)
{
    Xp60PatchDecodeResult result;
    std::optional<BlockValues> common;
    std::array<std::optional<BlockValues>, ToneIndex::kCount> tones;

    for (const auto& block : Xp60PatchLayout::blocks()) {
        const auto address = patchBase.plus(block.offset);
        if (!address) {
            result.missing.push_back({std::string(block.name), patchBase, MemoryImage::Coverage{block.size, 0, patchBase}});
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
        if (block.tone) {
            tones[block.tone->index()] = std::move(decoded.values);
        } else {
            common = std::move(decoded.values);
        }
    }

    if (common && std::all_of(tones.begin(), tones.end(), [](const auto& t) { return t.has_value(); })) {
        result.patch.emplace(std::move(*common), std::array<BlockValues, ToneIndex::kCount>{
                                                     std::move(*tones[0]), std::move(*tones[1]), std::move(*tones[2]),
                                                     std::move(*tones[3])});
    }
    return result;
}

std::array<roland::ByteVector, 5> Xp60PatchCodec::blockBytes(const Xp60Patch& patch)
{
    return {{
        BlockCodec::encode(patch.common()),
        BlockCodec::encode(patch.tone(ToneIndex::tone1())),
        BlockCodec::encode(patch.tone(ToneIndex::tone2())),
        BlockCodec::encode(patch.tone(ToneIndex::tone3())),
        BlockCodec::encode(patch.tone(ToneIndex::tone4())),
    }};
}

MemoryImage Xp60PatchCodec::encodeToImage(const Xp60Patch& patch, const roland::RolandAddress& patchBase)
{
    MemoryImage image;
    const auto bytes = blockBytes(patch);
    const auto blocks = Xp60PatchLayout::blocks();
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        if (const auto address = patchBase.plus(blocks[i].offset)) {
            image.write(*address, bytes[i]);
        }
    }
    return image;
}

std::vector<roland::RolandSysExMessage> Xp60PatchCodec::encodeToDataSets(const Xp60Patch& patch, roland::RolandDeviceId deviceId,
                                                                         const roland::RolandModelId& modelId,
                                                                         const roland::RolandAddress& patchBase,
                                                                         std::size_t maxPayloadBytes)
{
    std::vector<roland::RolandSysExMessage> messages;
    const auto bytes = blockBytes(patch);
    const auto blocks = Xp60PatchLayout::blocks();
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const auto address = patchBase.plus(blocks[i].offset);
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

} // namespace xp60studio::xpmodel
