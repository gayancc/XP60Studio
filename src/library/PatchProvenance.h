#pragma once

#include "roland/RolandAddress.h"
#include "roland/RolandDeviceId.h"
#include "roland/RolandModelId.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace xp60studio::library {

// Where a Patch in the library came from.
//
// Provenance answers "can I trust this, and where would I go to check?". It is
// recorded once at import and never rewritten by editing: a Patch that was
// imported from a file and then edited is still a Patch that came from that
// file, with a version history on top (Phase 5 deliverable, ARCHITECTURE.md
// §12). Unknown facts stay unknown; nothing here is inferred.
enum class PatchOrigin {
    Unknown,
    // Read out of a .syx file the user supplied.
    ImportedFile,
    // Fetched from a connected instrument through the transfer path.
    FetchedFromDevice,
    // Created inside XP60Studio.
    CreatedLocally,
};

[[nodiscard]] std::string_view patchOriginName(PatchOrigin origin) noexcept;

struct PatchProvenance
{
    PatchOrigin origin = PatchOrigin::Unknown;

    // Verbatim as the user supplied it (a file name, or a device description).
    // Never parsed for meaning, never used as an identity.
    std::string sourceName;
    // SHA-256 of the whole source, so the same file can be recognised later
    // even if it is renamed or moved. Empty when the source is not a file.
    std::string sourceDigest;
    // Byte range of the contributing messages inside that source. Both zero
    // when the source is not a byte stream.
    std::uint64_t sourceByteOffset = 0;
    std::uint64_t sourceByteCount = 0;

    // The Roland address the data was found at, and the User bank slot it
    // occupied when it came from one. A Patch read from the temporary area has
    // no user number.
    roland::RolandAddress address;
    std::optional<int> userNumber; // 1..128

    // Recorded exactly as the source SysEx carried them; they are part of how
    // the data was delivered, not of the Patch itself.
    std::optional<roland::RolandDeviceId> deviceId;
    std::optional<roland::RolandModelId> modelId;

    std::chrono::system_clock::time_point importedAt{};

    // One line for logs and inspectors, e.g.
    // "imported file 'amal.syx' USER:007 at 11 06 00 00, device 17".
    [[nodiscard]] std::string describe() const;
};

} // namespace xp60studio::library
