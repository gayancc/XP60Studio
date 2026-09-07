#pragma once

#include <QString>
#include <QStringList>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace xp60studio::library {

// What a live cue points at.
//
// A cue names a sound. It never holds one — not the bytes, not a copy. That is
// the same rule a saved bank follows, and for the same reason: a setlist is an
// arrangement of references, so building one changes nothing and deleting one
// loses nothing. It also means a cue can outlive what it points at, which is
// handled by keeping the name (see `LiveTarget::name`) rather than by pretending
// the cue is empty.
enum class LiveTargetKind {
    // Deliberately no change: this section keeps whatever the previous one
    // selected. A real thing a musician wants — a verse and its chorus on one
    // sound — and distinct from a cue nobody has filled in, which is why
    // `SetlistProblem` reports the unfilled case separately.
    CarryPrevious,
    // A Patch in this library. Going to it means sending those bytes to the
    // instrument's temporary area, which is the one selection mechanism this
    // project has verified.
    LibraryPatch,
    // A Patch already in the instrument's USER memory, USER:001..128.
    UserPatchSlot,
    // A Performance already in the instrument's USER memory, USER:01..32.
    UserPerformanceSlot,
};

[[nodiscard]] std::string_view liveTargetKindName(LiveTargetKind kind) noexcept;

struct LiveTarget
{
    LiveTargetKind kind = LiveTargetKind::CarryPrevious;
    // LibraryPatch: the library row. 0 when the row has since been deleted —
    // the cue keeps its name and reports itself missing rather than silently
    // becoming an empty slot in the middle of a show.
    std::int64_t libraryId = 0;
    // UserPatchSlot: 1..128. UserPerformanceSlot: 1..32.
    int userNumber = 0;
    // What to put on the stage display. Recorded when the cue is made, so it
    // survives the library row being deleted and works before anything has been
    // read from the instrument.
    std::string name;

    [[nodiscard]] bool selectsSomething() const noexcept
    {
        return kind != LiveTargetKind::CarryPrevious;
    }
    // A LibraryPatch cue whose row is gone. Still shown, still named, not
    // playable.
    [[nodiscard]] bool isMissing() const noexcept
    {
        return kind == LiveTargetKind::LibraryPatch && libraryId == 0;
    }
    [[nodiscard]] bool isValid() const noexcept;
    // "USER:009", "Library: GrandPiano", "(carry previous)". For the display,
    // not for a file format.
    [[nodiscard]] QString describe() const;

    friend bool operator==(const LiveTarget&, const LiveTarget&) = default;
};

// One cue: a named part of a song, and the sound it calls for.
struct SetlistSection
{
    std::string name;
    LiveTarget target;
    // The musician's own words. Shown large on stage.
    std::string note;

    friend bool operator==(const SetlistSection&, const SetlistSection&) = default;
};

struct SetlistSong
{
    std::string name;
    std::string note;
    std::vector<SetlistSection> sections;

    friend bool operator==(const SetlistSong&, const SetlistSong&) = default;
};

// One cue, flattened out of the song/section tree with its position kept.
//
// Navigation works on this list rather than on the tree: "next" on stage means
// the next cue, whether or not it is in the same song, and a flat list makes
// that one increment instead of two nested cursors that can disagree.
struct SetlistCue
{
    int index = 0;  // position in the flattened list
    int songIndex = 0;
    int sectionIndex = 0;
    std::string songName;
    std::string sectionName;
    std::string note;
    LiveTarget target;
    // The sound actually in force here, which for a CarryPrevious cue is the
    // one an earlier cue selected. Resolved when the list is built so the stage
    // display never has to walk backwards to answer "what am I playing?".
    LiveTarget effectiveTarget;

    [[nodiscard]] QString label() const;
};

enum class SetlistProblemKind {
    NoSongs,
    SongWithNoSections,
    UnnamedSong,
    UnnamedSection,
    // A cue that carries the previous sound with nothing before it.
    NothingToCarry,
    // A LibraryPatch cue whose library row has been deleted.
    MissingLibraryPatch,
    // A USER slot number outside what the instrument has.
    SlotOutOfRange,
};

struct SetlistProblem
{
    SetlistProblemKind kind = SetlistProblemKind::NoSongs;
    int songIndex = -1;     // -1 when the problem is the setlist's own
    int sectionIndex = -1;
    QString message;
};

// A running order: songs, their sections, and the sound each section calls for.
//
// ── What it is for ──────────────────────────────────────────────────────────
//
// Stage use. Everything here is shaped by two facts about a stage: the musician
// is looking at the screen for half a second between bars, and a wrong move is
// heard by the audience. So the model resolves as much as it can in advance —
// the flattened cue list, the effective sound at every position, the problems
// with the whole list — rather than making the display work them out while
// somebody is waiting.
//
// ── What it is not ──────────────────────────────────────────────────────────
//
// It is not a sequencer and holds no timing. A cue advances because a person
// (or, later, a MIDI message) says so. Nothing here transmits: going to a cue
// is `services::LiveSession`'s job, and what "going" costs depends on the
// target kind.
//
// ── Problems are reported, never repaired ───────────────────────────────────
//
// `problems()` lists everything wrong with the running order. Nothing here
// removes a broken cue, renumbers around a missing Patch or fills in a blank.
// A setlist quietly shortened between soundcheck and the show is worse than one
// that says a cue is broken.
class Setlist
{
public:
    std::int64_t id = 0;  // 0 until saved
    std::string name;
    std::string note;
    std::vector<SetlistSong> songs;
    std::chrono::system_clock::time_point createdAt{};
    std::chrono::system_clock::time_point updatedAt{};

    [[nodiscard]] int songCount() const noexcept { return static_cast<int>(songs.size()); }
    [[nodiscard]] int cueCount() const noexcept;

    // Every section in running order, with its position and the sound in force
    // at it. Empty when there are no sections at all.
    [[nodiscard]] std::vector<SetlistCue> cues() const;
    // The cue at `index`, or nullopt when out of range.
    [[nodiscard]] std::optional<SetlistCue> cueAt(int index) const;

    [[nodiscard]] std::vector<SetlistProblem> problems() const;
    // Every problem's message, for a screen that just wants to list them.
    [[nodiscard]] QStringList problemMessages() const;
    // True when nothing is wrong. Not the same as "every cue selects a sound":
    // a setlist made entirely of CarryPrevious cues after an opening one is
    // perfectly playable.
    [[nodiscard]] bool isPlayable() const;

    friend bool operator==(const Setlist&, const Setlist&) = default;
};

} // namespace xp60studio::library
