#pragma once

#include "library/LibraryDatabase.h"
#include "library/Setlist.h"
#include "services/DeviceSession.h"
#include "services/PatchTransfer.h"

#include <QObject>
#include <QString>

#include <optional>
#include <vector>

namespace xp60studio::services {

// Why a cue cannot be switched to, if it cannot.
enum class LiveBlock {
    None,
    // A cue that deliberately keeps the previous sound. Not a fault.
    NothingToDo,
    NoSetlist,
    OutOfRange,
    NotConnected,
    Busy,
    NotArmed,
    // A LibraryPatch cue whose row has been deleted.
    PatchMissing,
    // The row is there but its bytes will not decode.
    PatchUnreadable,
    // A Performance cue. Navigable, not switchable — see LiveSession's comment.
    PerformanceNotSupported,
};

[[nodiscard]] std::string_view liveBlockName(LiveBlock block) noexcept;

// What switching to a cue involves, so a screen can say so before it happens.
struct LiveSwitchPlan
{
    LiveBlock block = LiveBlock::None;
    QString reason;   // set exactly when `block` is not None
    QString headline; // "GrandPiano → temporary area"
    // Blocks read from the instrument before anything is written. Zero for a
    // library cue, whose bytes are already held.
    int blocksToRead = 0;
    int blocksToWrite = 0;
    [[nodiscard]] bool possible() const noexcept { return block == LiveBlock::None; }
};

// Stage use: where in the running order we are, what sound that calls for, and
// getting the instrument to make it.
//
// ── Navigation is separate from switching, on purpose ───────────────────────
//
// Moving the cursor never touches the instrument. A musician scrolls ahead to
// see what is coming without changing what is currently playing, and the next
// cue is visible before it is reached — the "next-sound preview" a stage
// display needs. Only `goToCurrent()` transmits.
//
// ── Arming once, not once per cue ───────────────────────────────────────────
//
// `PatchTransfer` arming is single-use, which is right for a one-off write and
// wrong for a show. Live Mode therefore uses its session-scoped live preview:
// armed once as the session starts, open until `endLiveWrites()`. Each cue is a
// queued update on that session, which also means two fast advances land on the
// second sound instead of playing the first on the way past.
//
// ── How a sound is selected, and why it is done that way ────────────────────
//
// The XP-60 can be told to select a Patch by Bank Select + Program Change, and
// this application does not do that. Whether a Program Change *we* transmit
// changes the temporary area the way a panel press does is unverified
// (`PATCH_SYNCHRONIZATION.md` U5), and the preset Bank Select mapping is not
// documented anywhere this project has (`ROLAND_XP60_PROTOCOL_FACTS.md` §7).
// Guessing at either on stage means the wrong sound in front of an audience.
//
// So a cue is realised the one way this project has verified: the Patch is
// written into the temporary area, exactly as auditioning from the editor does,
// and read back. For a library cue the bytes are already held, so that is five
// blocks out. For a USER slot cue the slot is read first, so it is five blocks
// in and five out — call it under a second, which is worth knowing before a
// downbeat, which is why `planFor()` reports it.
//
// ── Performances are navigable but not switchable ───────────────────────────
//
// A Performance cue can be written down, seen and moved through; `goToCurrent()`
// refuses it with `PerformanceNotSupported`. Writing the temporary Performance
// is implemented, but in the Performance editor rather than as a service this
// can call, and inventing a second path to a seventeen-block destructive write
// for stage use is not something to do without a Performance transfer service
// that carries the same read-back rules `PatchTransfer` does.
//
// ── What a cue costs ────────────────────────────────────────────────────────
//
// `planFor()` reports blocks, and there is one more cost it cannot: the live
// preview throttles queued updates by 120 ms so a knob drag does not restart a
// transfer per pixel. On stage that throttle sits in front of every switch
// after the first. It is PatchTransfer's number, adjustable there, and worth
// knowing before somebody blames the MIDI cable.
//
// ── Nothing here is destructive ─────────────────────────────────────────────
//
// Everything written goes to the temporary area, which the XP-60 erases at
// power-off or when another sound is selected (Owner's Manual p.45). A live
// session cannot alter a stored Patch, so a wrong cue costs a bar, not a sound.
class LiveSession : public QObject
{
    Q_OBJECT

public:
    LiveSession(DeviceSession& session, PatchTransfer& transfer, QObject* parent = nullptr);

    // The library is where LibraryPatch cues are resolved from. Optional: a
    // setlist of USER slots works without one, and a session with no library
    // reports `PatchMissing` for library cues rather than pretending.
    void setLibrary(library::LibraryDatabase* library);

    // ── The running order ───────────────────────────────────────────────────
    void load(library::Setlist setlist);
    void clear();
    [[nodiscard]] bool hasSetlist() const noexcept { return m_setlist.has_value(); }
    [[nodiscard]] const library::Setlist& setlist() const { return *m_setlist; }
    [[nodiscard]] const std::vector<library::SetlistCue>& cues() const noexcept { return m_cues; }
    [[nodiscard]] int cueCount() const noexcept { return static_cast<int>(m_cues.size()); }

    // ── Navigation. None of this transmits. ─────────────────────────────────
    // -1 before the setlist has been started, which is a real state: nothing
    // has been played yet and the instrument holds whatever it held.
    [[nodiscard]] int position() const noexcept { return m_position; }
    [[nodiscard]] std::optional<library::SetlistCue> currentCue() const;
    // What is coming, for the preview. Nullopt at the end of the list.
    [[nodiscard]] std::optional<library::SetlistCue> nextCue() const;
    [[nodiscard]] bool canGoNext() const noexcept;
    [[nodiscard]] bool canGoPrevious() const noexcept;
    bool goToIndex(int index);
    bool goNext();
    bool goPrevious();
    // Back to before the first cue, without transmitting.
    void rewind();

    // ── Switching ───────────────────────────────────────────────────────────
    [[nodiscard]] LiveSwitchPlan planFor(int index) const;
    [[nodiscard]] LiveSwitchPlan planForCurrent() const { return planFor(m_position); }
    // Realises the cue at the cursor on the instrument. False when refused;
    // `planForCurrent()` says why before it is called, and `lastMessage()`
    // after.
    bool goToCurrent();
    // Moves to the next cue and realises it. The one action a stage foot switch
    // or the big button performs.
    bool advance();
    // Ends the run of live writes, verifying what the instrument actually
    // holds. Leaving Live Mode without this ends on an unproved send.
    void endLiveWrites();

    [[nodiscard]] bool switching() const noexcept { return m_switching; }
    [[nodiscard]] QString lastMessage() const { return m_message; }
    // The cue index the instrument was last confirmed to be playing, or -1.
    // Distinct from `position()`: scrolling ahead moves the cursor and leaves
    // this where the sound is.
    [[nodiscard]] int soundingIndex() const noexcept { return m_sounding; }

Q_SIGNALS:
    // The cursor moved, or the setlist changed. Nothing was transmitted.
    void positionChanged();
    void setlistChanged();
    // A switch started, finished or failed.
    void switchingChanged();
    void switchFinished(int index, bool ok);

private:
    void rebuild();
    void onTransferChanged();
    void onFetchChanged();
    void finishSwitch(bool ok, QString message);
    [[nodiscard]] std::optional<xpmodel::Xp60Patch> patchForLibraryCue(
        const library::LiveTarget& target, QString& error) const;
    bool sendPatch(const xpmodel::Xp60Patch& patch);

    // The last Patch handed to the instrument, so leaving the session can
    // verify against what was actually sent rather than against a cue.
    std::optional<xpmodel::Xp60Patch> m_lastSent;

    DeviceSession& m_session;
    PatchTransfer& m_transfer;
    library::LibraryDatabase* m_library = nullptr;

    std::optional<library::Setlist> m_setlist;
    std::vector<library::SetlistCue> m_cues;
    int m_position = -1;
    int m_sounding = -1;

    bool m_switching = false;
    // The cue a switch in flight is for, so a reply arriving after the musician
    // has moved on is attributed to what actually asked for it.
    int m_switchingIndex = -1;
    // Set while waiting for a USER slot read that a switch asked for.
    bool m_awaitingSlotRead = false;
    QString m_message;
};

} // namespace xp60studio::services
