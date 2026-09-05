#pragma once

#include "library/LibraryEntry.h"
#include "roland/RolandDeviceId.h"
#include "roland/RolandModelId.h"
#include "roland/RolandTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace xp60studio::library {

// Which bytes an export writes.
enum class SyxExportSource {
    // Exactly the bytes that arrived, byte for byte: device IDs, chunking and
    // checksums as the source produced them. This is what "give me back what I
    // imported" means, and it is only meaningful when the Patches keep the
    // addresses they came from.
    OriginalBytes,
    // Re-encoded from the decoded Patch. Required whenever the export changes
    // where a Patch lives, which device ID it carries, or how it is chunked.
    ReencodedFromModel,
};

[[nodiscard]] std::string_view syxExportSourceName(SyxExportSource source) noexcept;

// Where the exported Patches are addressed.
struct SyxExportTarget
{
    enum class Kind {
        // Every Patch keeps the address it was imported from.
        AsImported,
        // Every Patch is written to the temporary Patch area (`03 00 00 00`).
        // Only meaningful for a single Patch: several would overwrite one
        // another in the same edit buffer, which the export refuses to do.
        TemporaryPatch,
        // Consecutive User bank slots from `firstUserNumber`, in list order.
        UserBankFrom,
    };

    Kind kind = Kind::AsImported;
    int firstUserNumber = 1; // 1..128, used by UserBankFrom
};

struct SyxExportOptions
{
    SyxExportSource source = SyxExportSource::OriginalBytes;
    SyxExportTarget target;

    // Re-encoding only. Unset keeps whatever device ID each entry's provenance
    // recorded, falling back to the Roland factory default when it has none.
    std::optional<roland::RolandDeviceId> deviceId;
    // Unset means the XP-60's own model ID.
    std::optional<roland::RolandModelId> modelId;
    // The XP-60 MIDI Implementation splits data longer than this into
    // several DT1 messages. Never raised silently above the documented limit.
    std::size_t maxPayloadBytes = 128;
};

struct SyxExportResult
{
    roland::ByteVector bytes;
    std::size_t patchCount = 0;
    std::size_t messageCount = 0;
    // Everything the export did that the caller did not literally ask for —
    // a Patch re-addressed to a different slot, a device ID substituted.
    // An export that changes something says so.
    std::vector<std::string> notes;

    bool ok = false;
    std::string error; // set exactly when ok is false

    [[nodiscard]] std::string summary() const;
};

// Writes library entries as a Roland `.syx` byte stream.
//
// `OriginalBytes` is the default because it is the only mode that can promise
// the user gets back precisely what the instrument or the file gave them.
// It is refused for any target that would move a Patch, rather than quietly
// re-encoding: an export that says "original bytes" and returns re-encoded
// ones would be a lie about provenance.
//
// No file I/O: the caller owns writing, so this stays deterministic and
// testable, and a service can run it off the UI thread.
[[nodiscard]] SyxExportResult exportEntries(const std::vector<LibraryEntry>& entries,
                                            const SyxExportOptions& options = {});

// One Patch, same rules.
[[nodiscard]] SyxExportResult exportEntry(const LibraryEntry& entry, const SyxExportOptions& options = {});

} // namespace xp60studio::library
