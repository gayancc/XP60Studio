#include "library/SyxExport.h"

#include "xp60/Xp60Device.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <algorithm>

namespace xp60studio::library {
namespace {

using xpmodel::Xp60PatchLayout;

constexpr int kFirstUserNumber = 1;
constexpr int kLastUserNumber = 128;

std::string paddedUserNumber(int userNumber)
{
    const std::string digits = std::to_string(userNumber);
    return std::string(digits.size() < 3 ? 3 - digits.size() : 0, '0') + digits;
}

SyxExportResult failure(std::string error)
{
    SyxExportResult result;
    result.ok = false;
    result.error = std::move(error);
    return result;
}

// The address each entry is written to, and a note when that moves it.
struct Placement
{
    roland::RolandAddress address;
    std::string note; // empty when the Patch stayed where it was
};

std::optional<Placement> placementFor(const LibraryEntry& entry, const SyxExportTarget& target, std::size_t index,
                                      std::string& errorOut)
{
    const auto& provenance = entry.provenance();
    switch (target.kind) {
    case SyxExportTarget::Kind::AsImported:
        return Placement{provenance.address, {}};

    case SyxExportTarget::Kind::TemporaryPatch: {
        Placement placement{Xp60PatchLayout::temporaryPatchAddress(), {}};
        if (provenance.address != placement.address) {
            placement.note = "'" + entry.displayName() + "' written to the temporary Patch area, not "
                             + provenance.address.toHexString();
        }
        return placement;
    }

    case SyxExportTarget::Kind::UserBankFrom:
    case SyxExportTarget::Kind::UserBankSlots: {
        const int userNumber = target.kind == SyxExportTarget::Kind::UserBankSlots
                                   ? target.userNumbers[index]
                                   : target.firstUserNumber + static_cast<int>(index);
        const auto address = Xp60PatchLayout::userPatchAddress(userNumber);
        if (!address) {
            errorOut = "User slot " + std::to_string(userNumber) + " is outside the 128-slot User bank.";
            return std::nullopt;
        }
        Placement placement{*address, {}};
        // A note reports what the export did that the caller did not literally
        // ask for. With UserBankSlots the caller named this exact destination
        // for this exact Patch, so landing there is the request being honoured
        // rather than a deviation from it — and a built bank would otherwise
        // produce a note per Patch, burying any real one.
        if (target.kind == SyxExportTarget::Kind::UserBankSlots) {
            return placement;
        }
        if (!provenance.userNumber || *provenance.userNumber != userNumber) {
            placement.note = "'" + entry.displayName() + "' written to USER:" + paddedUserNumber(userNumber);
            if (provenance.userNumber) {
                placement.note += ", imported from USER:" + paddedUserNumber(*provenance.userNumber);
            }
        }
        return placement;
    }
    }
    errorOut = "Unknown export target.";
    return std::nullopt;
}

} // namespace

std::string_view syxExportSourceName(SyxExportSource source) noexcept
{
    switch (source) {
    case SyxExportSource::OriginalBytes:
        return "original bytes";
    case SyxExportSource::ReencodedFromModel:
        return "re-encoded from the model";
    }
    return "unknown source";
}

std::string SyxExportResult::summary() const
{
    if (!ok) {
        return "export failed: " + error;
    }
    std::string out = std::to_string(patchCount) + (patchCount == 1 ? " patch" : " patches") + ", "
                      + std::to_string(messageCount) + (messageCount == 1 ? " message" : " messages") + ", "
                      + std::to_string(bytes.size()) + " bytes";
    if (!notes.empty()) {
        out += ", " + std::to_string(notes.size()) + (notes.size() == 1 ? " note" : " notes");
    }
    return out;
}

SyxExportResult exportEntries(const std::vector<LibraryEntry>& entries, const SyxExportOptions& options)
{
    if (entries.empty()) {
        return failure("Nothing to export.");
    }
    if (options.maxPayloadBytes == 0 || options.maxPayloadBytes > xp60::transferDefaults().maxDataSetPayloadBytes) {
        return failure("A DT1 payload of " + std::to_string(options.maxPayloadBytes)
                       + " bytes is outside the documented limit of "
                       + std::to_string(xp60::transferDefaults().maxDataSetPayloadBytes) + ".");
    }

    const bool movesPatches = options.target.kind != SyxExportTarget::Kind::AsImported;
    if (options.source == SyxExportSource::OriginalBytes && movesPatches) {
        return failure("Original bytes can only be exported to the addresses they came from. "
                       "Re-addressing requires re-encoding from the model, which changes the bytes; "
                       "choose ReencodedFromModel to say so explicitly.");
    }
    if (options.source == SyxExportSource::OriginalBytes && options.deviceId) {
        return failure("Original bytes carry the device ID they arrived with. Changing it requires "
                       "re-encoding from the model.");
    }
    if (options.target.kind == SyxExportTarget::Kind::TemporaryPatch && entries.size() > 1) {
        return failure("The temporary Patch area holds one Patch. Exporting "
                       + std::to_string(entries.size())
                       + " Patches there would leave only the last one; export them to User slots instead.");
    }
    if (options.target.kind == SyxExportTarget::Kind::UserBankFrom) {
        const int first = options.target.firstUserNumber;
        const auto last = first + static_cast<int>(entries.size()) - 1;
        if (first < kFirstUserNumber || last > kLastUserNumber) {
            return failure("USER:" + paddedUserNumber(std::max(first, 0)) + " plus "
                           + std::to_string(entries.size()) + " patches runs past the 128-slot User bank.");
        }
    }
    if (options.target.kind == SyxExportTarget::Kind::UserBankSlots) {
        const auto& numbers = options.target.userNumbers;
        if (numbers.size() != entries.size()) {
            return failure("Exporting to chosen User slots needs one slot per patch: "
                           + std::to_string(entries.size()) + " patches, " + std::to_string(numbers.size())
                           + " slots.");
        }
        std::vector<int> seen = numbers;
        std::sort(seen.begin(), seen.end());
        if (seen.front() < kFirstUserNumber || seen.back() > kLastUserNumber) {
            return failure("USER:" + paddedUserNumber(std::max(seen.front(), 0)) + " to USER:"
                           + paddedUserNumber(seen.back()) + " falls outside the 128-slot User bank.");
        }
        const auto duplicate = std::adjacent_find(seen.begin(), seen.end());
        if (duplicate != seen.end()) {
            // Writing two Patches to one slot leaves only the second, which is
            // a silent loss on the instrument rather than in this file.
            return failure("Two patches are addressed to USER:" + paddedUserNumber(*duplicate)
                           + "; only the second would survive on the instrument.");
        }
    }

    SyxExportResult result;
    result.ok = true;
    result.patchCount = entries.size();

    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto& entry = entries[index];

        if (options.source == SyxExportSource::OriginalBytes) {
            // Verbatim. Not parsed, not re-chunked, not re-checksummed.
            const auto& original = entry.originalSysEx();
            if (original.empty()) {
                return failure("'" + entry.displayName() + "' has no preserved original bytes to export.");
            }
            result.bytes.insert(result.bytes.end(), original.begin(), original.end());
            // Counted by SysEx start markers, since the bytes are opaque here.
            for (const auto byte : original) {
                if (byte == roland::kSysExStart) {
                    ++result.messageCount;
                }
            }
            continue;
        }

        std::string error;
        const auto placement = placementFor(entry, options.target, index, error);
        if (!placement) {
            return failure(std::move(error));
        }
        if (!placement->note.empty()) {
            result.notes.push_back(placement->note);
        }

        auto deviceId = options.deviceId;
        if (!deviceId) {
            deviceId = entry.provenance().deviceId;
            if (!deviceId) {
                deviceId = roland::RolandDeviceId::factoryDefault();
                result.notes.push_back("'" + entry.displayName()
                                       + "' had no recorded device ID; wrote the factory default "
                                       + std::to_string(deviceId->displayNumber()) + ".");
            }
        }
        const auto& modelId = options.modelId ? *options.modelId : xp60::modelId();

        const auto messages = xpmodel::Xp60PatchCodec::encodeToDataSets(entry.patch(), *deviceId, modelId,
                                                                        placement->address, options.maxPayloadBytes);
        if (messages.empty()) {
            return failure("'" + entry.displayName() + "' could not be encoded at "
                           + placement->address.toHexString() + ".");
        }
        for (const auto& message : messages) {
            const auto encoded = message.encode();
            result.bytes.insert(result.bytes.end(), encoded.begin(), encoded.end());
        }
        result.messageCount += messages.size();
    }

    return result;
}

SyxExportResult exportEntry(const LibraryEntry& entry, const SyxExportOptions& options)
{
    return exportEntries({entry}, options);
}

} // namespace xp60studio::library
