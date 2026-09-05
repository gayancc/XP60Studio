#pragma once

#include "services/DeviceSession.h"
#include "xpmodel/Xp60Patch.h"

#include <QObject>
#include <QString>

#include <cstddef>
#include <optional>
#include <vector>

namespace xp60studio::services {

// Writes Patches into the XP-60's permanent USER Patch memory (`11 nn 00 00`).
//
// This is the operation the whole librarian exists for: a bank built in
// XP60Studio has to end up on the keyboard. It is also the only destructive
// thing the application does, so it is a separate service from `PatchTransfer`
// rather than a flag on it — that class hard-codes the temporary address and
// cannot be pointed here, and this class cannot be pointed there.
//
// ── What the XP-60 documentation establishes ────────────────────────────────
//
// The Parameter Address Map (MIDI Implementation p.221) places User Patch
// USER:001-128 at `11 00 00 00`…`11 7F 00 00`, and Data Set 1 is documented as
// the message "used when you wish to set the data of the receiving device".
// Roland documents no read-only regions. This project has additionally read all
// 128 User Patches from those addresses on a physical XP-60 (128/128, every
// parameter in range, every Patch re-encoding byte-exactly — see
// `HARDWARE_VALIDATION_XP60.md`), so the addresses are right and the region is
// live. `.syx` bank files in the wild — including this project's own golden
// fixture — are exactly these addresses, and exist to be sent back.
//
// What is **not** yet confirmed on hardware is the write direction itself, and
// how the instrument's **User Memory Protect** setting (Owner's Manual p.46,
// `UTILITY/Protect`) interacts with it. That is not a reason to withhold the
// feature: it is a reason to prove every single write at runtime instead of
// trusting it. Which is what this class does.
//
// ── Why this is safe to use before that confirmation ────────────────────────
//
//  1. **Nothing is written until its destination has been read.** Each
//     destination's existing Patch is fetched and kept first. A snapshot that
//     fails aborts before any byte is sent.
//  2. **Every write is verified.** The destination is read back and compared
//     byte for byte. A write that did not take is reported as a mismatch — which
//     is also exactly what User Memory Protect being ON would look like, so the
//     unknown surfaces as a clear message rather than as silent data loss.
//  3. **The snapshots are the undo.** `restore()` puts every Patch this run
//     overwrote back, in reverse order, verifying each.
//  4. **Arming is separate and single-use.** Arming `PatchTransfer` does not
//     authorise anything here, and one arming permits one run.
//  5. **The destination is always given explicitly.** A Patch that came from
//     USER:007 does not default to going back to USER:007.
//
// Nothing in the editing or synchronization path can reach this class. It is
// driven only by an explicit, confirmed user action.
class UserMemoryWrite : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        // Reading the Patch currently in the next destination, so it can be put
        // back. Nothing has been written yet.
        Snapshotting,
        Sending,
        Verifying,
        // Every destination was written and read back equal.
        Completed,
        // A destination read back differently from what was sent. The most
        // likely cause is User Memory Protect being ON.
        Mismatch,
        Failed,
        Cancelled,
        // Putting the snapshots back after a failed or unwanted run.
        Restoring,
    };

    // One Patch and the USER slot it is to be written to. `userNumber` is
    // 1..128 as the instrument numbers User Patches.
    struct Destination
    {
        int userNumber = 0;
        xpmodel::Xp60Patch patch;
    };

    explicit UserMemoryWrite(DeviceSession& session, QObject* parent = nullptr);

    [[nodiscard]] State state() const noexcept { return m_state; }
    [[nodiscard]] bool isBusy() const noexcept;
    [[nodiscard]] std::string_view stateName() const noexcept;
    [[nodiscard]] QString stateLabel() const;
    [[nodiscard]] QString message() const { return m_message; }

    // Arming ------------------------------------------------------------------
    // Refused unless connected. Spent by one run, whatever its outcome, so a
    // second bank write needs a second deliberate arming.
    [[nodiscard]] bool canArm() const;
    [[nodiscard]] bool isArmed() const noexcept { return m_armed; }
    bool arm();
    void disarm();

    // What a run would do, for the confirmation the user sees. Names the number
    // of destinations and the slots at the edges of the range.
    [[nodiscard]] QString writePlanDescription(const std::vector<Destination>& destinations) const;

    // Operations --------------------------------------------------------------
    // Writes each destination in the order given: snapshot, send, read back,
    // compare, next. False when refused (not armed, not connected, already busy,
    // empty list, a slot outside 1..128, or the same slot twice).
    bool write(std::vector<Destination> destinations);
    bool writeOne(const xpmodel::Xp60Patch& patch, int userNumber);
    // Puts back every Patch this run overwrote, most recent first. Only
    // available once a run has finished and only while the snapshots are held.
    bool restore();
    // The destinations of the last run that were not written and verified: the
    // one that failed or mismatched, and everything after it.
    [[nodiscard]] std::vector<Destination> unwritten() const;
    [[nodiscard]] bool canRetry() const;
    // Writes those again, **keeping** the snapshots already taken so the whole
    // run — the part that succeeded before the failure included — can still be
    // put back. Needs arming, like any other write: a retry is the same
    // destructive act as the attempt it repeats.
    bool retry();
    // Stops at the next safe point. Destinations already written stay written —
    // and their snapshots stay held, so `restore()` can still undo them.
    void cancel();

    // Progress ----------------------------------------------------------------
    [[nodiscard]] std::size_t total() const noexcept { return m_destinations.size(); }
    [[nodiscard]] std::size_t completed() const noexcept { return m_completed; }
    // 1..128 while a destination is being worked on, 0 otherwise.
    [[nodiscard]] int currentUserNumber() const noexcept;
    // The Patches that were in the destinations before this run, in the order
    // they were replaced. This is the backup, and it is what `restore()` sends.
    [[nodiscard]] const std::vector<Destination>& snapshots() const noexcept { return m_snapshots; }
    [[nodiscard]] bool canRestore() const;

Q_SIGNALS:
    void changed();
    // Emitted once per destination as it completes, so a long bank write can
    // report progress without the caller polling.
    void progressed(std::size_t completed, std::size_t total);
    void finished(bool ok);

private:
    enum class Awaiting { Nothing, Snapshot, SendBatch, ReadBack };

    void beginNextDestination();
    void sendCurrent();
    void beginReadBack();
    void compareReadBack();
    void finishRun(State state, QString message);
    void setState(State state, QString message);
    void fail(QString message);
    void onPatchFetchChanged();
    void onBatchFinished(quint64 batchId, bool ok, const QString& error);
    [[nodiscard]] const Destination& current() const { return m_destinations[m_index]; }

    DeviceSession& m_session;
    State m_state = State::Idle;
    Awaiting m_awaiting = Awaiting::Nothing;
    QString m_message;
    bool m_armed = false;
    bool m_cancelRequested = false;
    // True while `restore()` is driving the run, so the snapshots are not
    // collected again over themselves.
    bool m_restoring = false;

    std::vector<Destination> m_destinations;
    std::vector<Destination> m_snapshots;
    std::size_t m_index = 0;
    std::size_t m_completed = 0;
    DeviceSession::DataSetBatchId m_batch;
    std::optional<xpmodel::Xp60Patch> m_readBack;
};

} // namespace xp60studio::services
