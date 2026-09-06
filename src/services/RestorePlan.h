#pragma once

#include "library/InstrumentSnapshot.h"
#include "roland/RolandTypes.h"

#include <QString>
#include <QStringList>

#include <string>
#include <string_view>
#include <vector>

namespace xp60studio::services {

// One area of instrument memory a restore may be asked to put back.
//
// Named areas rather than raw addresses, because "restore the User Patches" is
// the request a musician makes, and because an address typed by hand is a way
// to write somewhere nobody meant to.
enum class RestoreArea {
    UserPatches,       // USER:001..128 at 11 00 00 00, stride 00 01 00 00
    UserPerformances,  // USER:01..32 at 10 00 00 00, stride 00 01 00 00
    UserRhythmSetups,  // USER:1..2 at 10 40 00 00, 10 41 00 00
    System,            // 00 00 00 00
};

[[nodiscard]] std::string_view restoreAreaName(RestoreArea area) noexcept;

// What restoring one area would involve, and whether it can be done at all.
struct RestoreStep
{
    RestoreArea area;
    roland::RolandAddress begin;
    // Messages from the snapshot that fall inside this area, in the order the
    // instrument sent them.
    std::vector<roland::ByteVector> messages;

    // Coverage is counted in the area's own units — Patch slots, Performance
    // slots — not in bytes. Roland leaves large gaps of unused address space
    // between slots, so "bytes present out of the address span" would report a
    // complete 128-Patch bank as a small fraction of itself and be useless as a
    // warning.
    int expectedEntries = 0;     // 128 User Patches, 32 Performances, 2 Rhythm Setups, 1 System
    int entriesWithData = 0;     // slots the snapshot holds anything for
    // Slots whose every documented block is present. -1 when this build has no
    // block layout for the area and therefore cannot check — Rhythm Setups and
    // System are transcribed but have no layout class yet, so their
    // completeness is honestly unknown rather than assumed.
    int completeEntries = -1;
    [[nodiscard]] bool completenessChecked() const noexcept { return completeEntries >= 0; }
    // Every expected slot is present, and complete where completeness could be
    // checked at all.
    bool covered = false;

    [[nodiscard]] QString describe() const;
};

// Everything that must be true, and everything that is not, before a snapshot
// is written back to an instrument.
//
// A restore is the most destructive thing this application can do: it
// overwrites memory a musician may have spent years filling. So the plan is a
// separate, inspectable step rather than an argument to a write call. The user
// sees exactly which areas will be touched, how many bytes and messages that
// is, and what the snapshot cannot answer for — before anything is sent.
//
// Three rules the plan enforces rather than merely documents:
//
//  1. **An area is restored whole or not at all.** A snapshot holding 90 of
//     128 User Patches cannot restore "the User Patches"; it can restore the
//     90 it holds, and it must say that is what it is doing. Half a restore
//     leaves an instrument in a state that never existed on it.
//  2. **Nothing outside the requested areas is written**, even if the snapshot
//     contains it. A snapshot of everything, restored to the Performances, is
//     a Performance restore.
//  3. **A safety snapshot is required.** `requiresSafetySnapshot()` is true for
//     every plan that writes anything, and the caller is expected to take one
//     first. The snapshot being restored is not the safety net — it is the
//     thing that will replace what is there now.
//
// Pure: builds and inspects a plan. Sending it is a separate service, so a plan
// can be shown, exported and reasoned about without a device in the room.
class RestorePlan
{
public:
    [[nodiscard]] static RestorePlan build(const library::InstrumentSnapshot& snapshot,
                                           const std::vector<RestoreArea>& areas);

    // Everything the snapshot can restore, whether or not it was asked for.
    // Convenience for "put the whole thing back".
    [[nodiscard]] static RestorePlan buildForEverythingCovered(
        const library::InstrumentSnapshot& snapshot);

    [[nodiscard]] const std::vector<RestoreStep>& steps() const noexcept { return m_steps; }
    // Requested areas the snapshot holds nothing at all for. These are refused,
    // not silently skipped.
    [[nodiscard]] const std::vector<RestoreArea>& unavailableAreas() const noexcept
    {
        return m_unavailable;
    }
    // Requested areas the snapshot holds only part of.
    [[nodiscard]] const std::vector<RestoreArea>& partialAreas() const noexcept
    {
        return m_partial;
    }

    [[nodiscard]] std::size_t messageCount() const noexcept;
    [[nodiscard]] std::size_t byteCount() const noexcept;
    [[nodiscard]] bool writesAnything() const noexcept { return messageCount() > 0; }
    [[nodiscard]] bool isComplete() const noexcept
    {
        return m_unavailable.empty() && m_partial.empty();
    }
    [[nodiscard]] bool requiresSafetySnapshot() const noexcept { return writesAnything(); }

    // The messages to send, in order, across every step. This is the only thing
    // a restore writes.
    [[nodiscard]] std::vector<roland::ByteVector> messages() const;

    // What the user must read before confirming: what will be overwritten, and
    // what this snapshot cannot put back.
    [[nodiscard]] QStringList warnings() const;
    [[nodiscard]] QString summary() const;

private:
    std::vector<RestoreStep> m_steps;
    std::vector<RestoreArea> m_unavailable;
    std::vector<RestoreArea> m_partial;
};

} // namespace xp60studio::services
