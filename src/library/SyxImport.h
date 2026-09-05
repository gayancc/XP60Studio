#pragma once

#include "library/LibraryEntry.h"
#include "roland/RolandAddress.h"
#include "roland/RolandModelId.h"
#include "roland/RolandTypes.h"
#include "xpmodel/SysExStream.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace xp60studio::library {

struct SyxImportOptions
{
    // Recorded verbatim into every entry's provenance.
    std::string sourceName;
    // Which Roland model IDs to accept. Empty means the XP-60's own model ID.
    std::vector<roland::RolandModelId> knownModelIds;
    // Stamped into every entry's provenance. Left at its default the clock is
    // read once, so a whole import shares one timestamp.
    std::optional<std::chrono::system_clock::time_point> importedAt;
    // Also look for Patches in the Performance-mode temporary Part areas
    // (`02 00 00 00`..`02 0F 00 00`). Off by default: a Performance dump is a
    // Phase 8 subject, and Part 10 there is the Rhythm Setup, not a Patch.
    bool includePerformanceParts = false;
};

// A Patch base the file touched but did not fully cover.
//
// Reported rather than dropped: a truncated bank is exactly the case where
// silently importing "the patches that happened to be complete" would hide
// data loss from the user.
struct PartialPatch
{
    roland::RolandAddress address;
    std::optional<int> userNumber; // 1..128 when inside the User Patch bank
    std::vector<std::string> missingBlocks; // "Patch Common", "Tone 3", ...
    std::uint32_t coveredBytes = 0;
    std::uint32_t expectedBytes = 0;

    [[nodiscard]] std::string describe() const;
};

// A Patch whose blocks were all present but which the codec could not accept.
struct RejectedPatch
{
    roland::RolandAddress address;
    std::optional<int> userNumber;
    std::string reason; // Xp60PatchDecodeResult::describe()

    [[nodiscard]] std::string describe() const;
};

// Two entries in this import that hold identical parameters.
struct DuplicateWithinSource
{
    std::size_t firstIndex = 0;
    std::size_t secondIndex = 0;
};

struct SyxImportResult
{
    std::vector<LibraryEntry> entries;
    std::vector<PartialPatch> partial;
    std::vector<RejectedPatch> rejected;
    // Patches that decoded but whose values fell outside the transcribed
    // ranges. They are imported unchanged; the warning travels with them.
    std::vector<RejectedPatch> warnings;
    std::vector<DuplicateWithinSource> duplicates;

    // The complete stream report: stray bytes, aborted SysEx, non-Roland
    // messages and rejected Roland messages are all still visible here.
    xpmodel::SysExStreamResult stream;
    // SHA-256 of the whole input, recorded in every entry's provenance.
    std::string sourceDigest;
    // Data sets that landed at an address no candidate Patch covers — a
    // Performance dump, a System dump, or an area this project has not
    // transcribed. Counted, never guessed at.
    std::size_t unattributedDataSets = 0;

    [[nodiscard]] bool isClean() const noexcept
    {
        return stream.isClean() && partial.empty() && rejected.empty();
    }
    // "12 patches, 1 partial, 3 unattributed data sets" — one line for logs.
    [[nodiscard]] std::string summary() const;
};

// Reads every XP-60 Patch a .syx byte stream contains.
//
// The stream is parsed, its data sets are laid into a memory image, and every
// documented Patch base is then checked for coverage. Nothing is inferred from
// message order or file size: a Patch is imported when the Parameter Address
// Map's five blocks are all present at one of the bases the protocol document
// establishes, and every other outcome is reported instead of discarded.
//
// This function does no file I/O and no persistence. It is deterministic and
// has no Qt event-loop, timer or thread dependency, so a service can run it on
// a worker thread and own progress and cancellation itself (ARCHITECTURE.md §8).
[[nodiscard]] SyxImportResult importSyxStream(roland::ByteSpan bytes, const SyxImportOptions& options = {});

} // namespace xp60studio::library
