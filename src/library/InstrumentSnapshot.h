#pragma once

#include "roland/RolandTypes.h"
#include "xpmodel/SysExStream.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace xp60studio::library {

// One contiguous stretch of instrument memory a snapshot holds.
//
// A region is derived from the messages themselves rather than declared: what
// was captured is whatever arrived. Nothing here says a region *should* exist.
struct SnapshotRegion
{
    roland::RolandAddress begin;
    std::uint32_t byteCount = 0;
    int messageCount = 0;

    [[nodiscard]] std::optional<roland::RolandAddress> end() const noexcept
    {
        return begin.plus(byteCount);
    }
    [[nodiscard]] bool contains(const roland::RolandAddress& address,
                                std::uint32_t bytes) const noexcept;
    [[nodiscard]] std::string describe() const;
};

// Who took a snapshot, when, and what they called it.
struct SnapshotMetadata
{
    std::string label;        // the user's own name for it
    std::string deviceName;   // the port or device it was read from
    std::string appVersion;
    std::chrono::system_clock::time_point capturedAt{};
    // Free text: "before writing bank Live 2026", "safety snapshot".
    std::string note;
};

// What an XP-60 held at one moment, as the bytes it actually sent.
//
// A snapshot is the answer to "can I put the keyboard back exactly as it was?".
// That makes two properties non-negotiable, and they shape everything here:
//
// **It stores the instrument's own bytes, not a re-encoding of them.** The DT1
// messages are kept verbatim, in arrival order. A snapshot is therefore
// restorable byte for byte even for regions this application has no model for —
// and a future version that learns to decode more does not invalidate snapshots
// taken before it.
//
// **It knows exactly what it does not contain.** A snapshot of the User Patch
// bank is not a backup of the instrument, and saying so is the difference
// between a safety net and a false one. `covers()` answers per address, and a
// restore is expected to refuse anything the snapshot cannot answer for rather
// than writing part of what was asked and reporting success.
//
// The file format is a plain `.syx`: the same bytes any other librarian or
// even `amidi` can send back. A snapshot that could only be read by XP60Studio
// would be a worse backup than one that can be restored without it.
//
// Pure: no I/O, no Qt beyond what the codec already uses, no database.
class InstrumentSnapshot
{
public:
    using Metadata = SnapshotMetadata;

    InstrumentSnapshot() = default;

    // Builds a snapshot from a raw `.syx` stream. Everything that is not an
    // accepted Roland Data Set is counted and reported rather than dropped
    // quietly: a snapshot built from a damaged file must say so before anybody
    // relies on it.
    [[nodiscard]] static InstrumentSnapshot fromSysEx(roland::ByteSpan bytes,
                                                      std::span<const roland::RolandModelId> models,
                                                      SnapshotMetadata metadata = SnapshotMetadata());

    // Builds one directly from messages as they arrived from the instrument.
    [[nodiscard]] static InstrumentSnapshot fromDataSets(std::vector<roland::ByteVector> messages,
                                                         std::span<const roland::RolandModelId> models,
                                                         SnapshotMetadata metadata = SnapshotMetadata());

    [[nodiscard]] const Metadata& metadata() const noexcept { return m_metadata; }
    void setMetadata(Metadata metadata) { m_metadata = std::move(metadata); }

    // The captured messages, verbatim and in arrival order. This is what a
    // restore sends and what `toSysEx()` writes.
    [[nodiscard]] const std::vector<roland::ByteVector>& messages() const noexcept
    {
        return m_messages;
    }
    [[nodiscard]] std::size_t messageCount() const noexcept { return m_messages.size(); }
    [[nodiscard]] std::size_t byteCount() const noexcept;

    // Ascending, with touching and overlapping captures merged.
    //
    // In practice the XP-60's own layout leaves unused address space between
    // blocks — a Performance Common ends at offset `00 42` and its first Part
    // begins at `10 00` — so blocks captured from real memory stay separate
    // regions rather than collapsing into one. That is the map being reported
    // honestly, not a failure to merge: the gaps are addresses the instrument
    // sent nothing for, and a snapshot must not claim to hold them.
    [[nodiscard]] const std::vector<SnapshotRegion>& regions() const noexcept { return m_regions; }

    // True only when every byte of the range is present. A partially covered
    // range is not covered: restoring half of a Patch would leave the
    // instrument in a state that never existed.
    [[nodiscard]] bool covers(const roland::RolandAddress& address,
                              std::uint32_t byteCount) const noexcept;

    [[nodiscard]] bool isEmpty() const noexcept { return m_messages.empty(); }

    // Anomalies found while reading the stream this snapshot was built from.
    // Zero for one built from live messages.
    [[nodiscard]] std::size_t strayBytes() const noexcept { return m_strayBytes; }
    [[nodiscard]] std::size_t rejectedMessages() const noexcept { return m_rejected; }
    [[nodiscard]] bool isClean() const noexcept { return m_strayBytes == 0 && m_rejected == 0; }

    // SHA-256 over the captured bytes, as 64 hex digits. Identifies the
    // contents; a snapshot whose file no longer hashes to this has been altered
    // or damaged and must not be restored silently.
    [[nodiscard]] std::string digest() const;

    // The `.syx` bytes: every captured message, concatenated in order.
    [[nodiscard]] roland::ByteVector toSysEx() const;

    // "3993 bytes in 1 region (01 00 00 00–01 00 1F 19), 17 messages." Written
    // for a user deciding whether this snapshot is the one they want.
    [[nodiscard]] std::string summary() const;

private:
    void rebuildRegions();

    Metadata m_metadata;
    std::vector<roland::ByteVector> m_messages;
    std::vector<SnapshotRegion> m_regions;
    std::vector<roland::RolandModelId> m_models;
    std::size_t m_strayBytes = 0;
    std::size_t m_rejected = 0;
};

} // namespace xp60studio::library
