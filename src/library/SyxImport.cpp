#include "library/SyxImport.h"

#include "xp60/Xp60Device.h"
#include "xpmodel/MemoryImage.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QByteArray>
#include <QCryptographicHash>

#include <algorithm>
#include <utility>

namespace xp60studio::library {
namespace {

using xpmodel::Xp60PatchLayout;

// Half-open byte range in the linear address space.
struct AddressRange
{
    std::uint32_t begin = 0;
    std::uint32_t end = 0;

    [[nodiscard]] bool overlaps(const AddressRange& other) const noexcept
    {
        return begin < other.end && other.begin < end;
    }
};

// One Patch base worth checking, with the User bank slot it represents.
struct Candidate
{
    roland::RolandAddress address;
    std::optional<int> userNumber;
};

std::vector<Candidate> candidateBases(bool includePerformanceParts)
{
    std::vector<Candidate> candidates;
    candidates.push_back({Xp60PatchLayout::temporaryPatchAddress(), std::nullopt});
    for (int userNumber = 1; userNumber <= 128; ++userNumber) {
        if (const auto address = Xp60PatchLayout::userPatchAddress(userNumber)) {
            candidates.push_back({*address, userNumber});
        }
    }
    if (includePerformanceParts) {
        // Performance mode Parts 1..16 at 02 00 00 00 .. 02 0F 00 00. Part 10
        // is the Rhythm Setup (ROLAND_XP60_PROTOCOL_FACTS.md §3), which is not
        // a Patch and whose Address Map is not transcribed, so it is skipped.
        constexpr int kRhythmPart = 10;
        const roland::RolandAddress base{0x02, 0x00, 0x00, 0x00};
        for (int part = 1; part <= 16; ++part) {
            if (part == kRhythmPart) {
                continue;
            }
            const auto address = base.plus(static_cast<std::uint64_t>(part - 1) * Xp60PatchLayout::kUserPatchStride);
            if (address) {
                candidates.push_back({*address, std::nullopt});
            }
        }
    }
    return candidates;
}

std::vector<roland::RolandModelId> resolveModelIds(const SyxImportOptions& options)
{
    if (!options.knownModelIds.empty()) {
        return options.knownModelIds;
    }
    return {xp60::modelId()};
}

std::string sha256Hex(roland::ByteSpan bytes)
{
    const QByteArray digest = QCryptographicHash::hash(
        QByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size())),
        QCryptographicHash::Sha256);
    return digest.toHex().toStdString();
}

std::string paddedUserNumber(int userNumber)
{
    const std::string digits = std::to_string(userNumber);
    return std::string(digits.size() < 3 ? 3 - digits.size() : 0, '0') + digits;
}

std::string slotText(const roland::RolandAddress& address, const std::optional<int>& userNumber)
{
    return userNumber ? "USER:" + paddedUserNumber(*userNumber) + " (" + address.toHexString() + ")"
                      : address.toHexString();
}

// Ranges of the five documented blocks of the Patch at `base`.
std::vector<std::pair<Xp60PatchLayout::Block, AddressRange>> blockRanges(const roland::RolandAddress& base)
{
    std::vector<std::pair<Xp60PatchLayout::Block, AddressRange>> out;
    for (const auto& block : Xp60PatchLayout::blocks()) {
        const auto begin = base.plus(block.offset);
        if (!begin) {
            continue; // runs past the address space; reported as a missing block
        }
        out.push_back({block, AddressRange{begin->value(), begin->value() + block.size}});
    }
    return out;
}

} // namespace

std::string PartialPatch::describe() const
{
    std::string out = slotText(address, userNumber) + ": " + std::to_string(coveredBytes) + " of "
                      + std::to_string(expectedBytes) + " bytes";
    if (!missingBlocks.empty()) {
        out += ", missing";
        for (const auto& block : missingBlocks) {
            out += " " + block + ";";
        }
        out.pop_back();
    }
    return out;
}

std::string RejectedPatch::describe() const
{
    return slotText(address, userNumber) + ": " + reason;
}

std::string SyxImportResult::summary() const
{
    std::string out = std::to_string(entries.size()) + (entries.size() == 1 ? " patch" : " patches");
    if (!partial.empty()) {
        out += ", " + std::to_string(partial.size()) + " partial";
    }
    if (!rejected.empty()) {
        out += ", " + std::to_string(rejected.size()) + " rejected";
    }
    if (!warnings.empty()) {
        out += ", " + std::to_string(warnings.size()) + " with range warnings";
    }
    if (!duplicates.empty()) {
        out += ", " + std::to_string(duplicates.size()) + " duplicate pair"
               + (duplicates.size() == 1 ? "" : "s");
    }
    if (unattributedDataSets > 0) {
        out += ", " + std::to_string(unattributedDataSets) + " unattributed data set"
               + (unattributedDataSets == 1 ? "" : "s");
    }
    if (!stream.isClean()) {
        out += ", stream not clean";
    }
    return out;
}

SyxImportResult importSyxStream(roland::ByteSpan bytes, const SyxImportOptions& options)
{
    SyxImportResult result;
    const auto modelIds = resolveModelIds(options);
    result.stream = xpmodel::parseSysExStream(bytes, modelIds);
    result.sourceDigest = sha256Hex(bytes);

    const xpmodel::MemoryImage image = xpmodel::imageFromStream(result.stream);
    const auto importedAt = options.importedAt.value_or(std::chrono::system_clock::now());

    // Data sets in stream order, with their address ranges, so a Patch can be
    // given back the exact bytes that carried it.
    struct DataSetItem
    {
        const xpmodel::SysExStreamItem* item;
        AddressRange range;
        bool attributed = false;
    };
    std::vector<DataSetItem> dataSets;
    for (const auto& item : result.stream.items) {
        if (!item.roland || !item.roland->isDataSet()) {
            continue;
        }
        const std::uint32_t begin = item.roland->address().value();
        const auto size = static_cast<std::uint32_t>(item.roland->data().size());
        dataSets.push_back({&item, AddressRange{begin, begin + size}, false});
    }

    for (const auto& candidate : candidateBases(options.includePerformanceParts)) {
        const auto ranges = blockRanges(candidate.address);

        std::uint32_t covered = 0;
        std::uint32_t expected = 0;
        std::vector<std::string> missing;
        for (const auto& [block, range] : ranges) {
            const auto coverage = image.coverage(*candidate.address.plus(block.offset), block.size);
            covered += coverage.covered;
            expected += coverage.requested;
            if (!coverage.complete()) {
                missing.emplace_back(block.name);
            }
        }
        // Blocks whose address ran past the space never made it into `ranges`.
        for (std::size_t i = ranges.size(); i < Xp60PatchLayout::blocks().size(); ++i) {
            missing.emplace_back(Xp60PatchLayout::blocks()[i].name);
        }

        if (covered == 0) {
            continue; // the file says nothing about this slot
        }
        if (!missing.empty()) {
            result.partial.push_back({candidate.address, candidate.userNumber, std::move(missing), covered, expected});
            // The bytes are still accounted for, so they are not also counted
            // as unattributed.
            for (auto& dataSet : dataSets) {
                for (const auto& [block, range] : ranges) {
                    if (dataSet.range.overlaps(range)) {
                        dataSet.attributed = true;
                        break;
                    }
                }
            }
            continue;
        }

        auto decoded = xpmodel::Xp60PatchCodec::decode(image, candidate.address);
        if (!decoded.ok()) {
            result.rejected.push_back({candidate.address, candidate.userNumber, decoded.describe()});
            continue;
        }

        // The exact messages that carried this Patch, in stream order.
        roland::ByteVector originalSysEx;
        std::uint64_t firstOffset = 0;
        std::uint64_t lastEnd = 0;
        bool anyMessage = false;
        for (auto& dataSet : dataSets) {
            const bool touches = std::any_of(ranges.begin(), ranges.end(), [&](const auto& entry) {
                return dataSet.range.overlaps(entry.second);
            });
            if (!touches) {
                continue;
            }
            dataSet.attributed = true;
            originalSysEx.insert(originalSysEx.end(), dataSet.item->raw.begin(), dataSet.item->raw.end());
            if (!anyMessage) {
                firstOffset = dataSet.item->offset;
                anyMessage = true;
            }
            lastEnd = dataSet.item->offset + dataSet.item->raw.size();
        }

        PatchProvenance provenance;
        provenance.origin = PatchOrigin::ImportedFile;
        provenance.sourceName = options.sourceName;
        provenance.sourceDigest = result.sourceDigest;
        provenance.sourceByteOffset = firstOffset;
        provenance.sourceByteCount = anyMessage ? lastEnd - firstOffset : 0;
        provenance.address = candidate.address;
        provenance.userNumber = candidate.userNumber;
        provenance.importedAt = importedAt;
        // Device and model IDs come from the messages themselves; a Patch
        // assembled from messages that disagree keeps neither.
        for (const auto& dataSet : dataSets) {
            if (!dataSet.attributed) {
                continue;
            }
            const auto& message = *dataSet.item->roland;
            if (!std::any_of(ranges.begin(), ranges.end(),
                             [&](const auto& entry) { return dataSet.range.overlaps(entry.second); })) {
                continue;
            }
            if (!provenance.deviceId) {
                provenance.deviceId = message.deviceId();
                provenance.modelId = message.modelId();
            } else if (*provenance.deviceId != message.deviceId()) {
                provenance.deviceId.reset();
                provenance.modelId.reset();
                break;
            }
        }

        if (decoded.hasWarnings()) {
            result.warnings.push_back({candidate.address, candidate.userNumber, decoded.describe()});
        }
        result.entries.emplace_back(std::move(*decoded.patch), std::move(originalSysEx), std::move(provenance));
    }

    result.unattributedDataSets =
        static_cast<std::size_t>(std::count_if(dataSets.begin(), dataSets.end(),
                                               [](const DataSetItem& item) { return !item.attributed; }));

    // Exact duplicates inside this one file. The fingerprint decides what to
    // compare; the parameters decide the answer.
    for (std::size_t i = 0; i < result.entries.size(); ++i) {
        for (std::size_t j = i + 1; j < result.entries.size(); ++j) {
            if (result.entries[i].hasSameParameters(result.entries[j])) {
                result.duplicates.push_back({i, j});
            }
        }
    }

    return result;
}

} // namespace xp60studio::library
