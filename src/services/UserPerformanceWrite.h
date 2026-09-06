#pragma once

#include "services/DeviceSession.h"
#include "xpmodel/Xp60Performance.h"

#include <QObject>
#include <QString>

#include <cstddef>
#include <optional>
#include <vector>

namespace xp60studio::services {

// Writes Performances into the XP-60's permanent USER Performance memory
// (`10 nn 00 00`, USER:01–32).
//
// The counterpart to `UserMemoryWrite` for the other half of the instrument's
// user memory, and the thing `PerformanceViewModel` deliberately did not offer:
// its Send writes the *temporary* Performance at `01 00 00 00` and nothing
// else, because a persistent write needs read-before-write, verify-by-read-back
// and restore. That machinery is here.
//
// ── Why this is a separate class rather than a mode of UserMemoryWrite ──────
//
// The two run the same state machine over the same session. Merging them would
// save perhaps two hundred lines and would cost the property that matters most
// about both: **each hard-codes the region it can reach, and cannot be pointed
// at the other.** `UserMemoryWrite` cannot write a Performance address and this
// cannot write a Patch address, whatever a caller passes. In the one place
// where a bug overwrites a musician's permanent memory, a wrong argument being
// unrepresentable is worth more than the duplication it costs. That is the same
// reasoning `UserMemoryWrite` gives for not being a flag on `PatchTransfer`.
//
// ── The safety rules, unchanged ─────────────────────────────────────────────
//
//  1. Nothing is written until its destination has been read and kept. A
//     snapshot that fails aborts before any byte is sent.
//  2. Every write is verified by reading the destination back and comparing.
//     A write that did not take is reported as a mismatch — which is also what
//     User Memory Protect being ON looks like, so the unknown surfaces as a
//     clear message rather than as silent data loss.
//  3. The snapshots are the undo. `restore()` puts back every Performance this
//     run overwrote, in reverse order, verifying each.
//  4. Arming is separate, single-use, and does not carry over from any other
//     writer. Arming this does not arm `UserMemoryWrite`, or the reverse.
//  5. The destination is always given explicitly.
//
// Nothing in the editing path can reach this class. It is driven only by an
// explicit, confirmed user action.
class UserPerformanceWrite : public QObject
{
    Q_OBJECT

public:
    static constexpr int kUserPerformanceCount = 32;

    enum class State {
        Idle,
        Snapshotting,
        Sending,
        Verifying,
        Completed,
        // A destination read back differently from what was sent.
        Mismatch,
        Failed,
        Cancelled,
        Restoring,
    };

    // One Performance and the USER slot it goes to. `userNumber` is 1..32 as
    // the instrument numbers User Performances.
    struct Destination
    {
        int userNumber = 0;
        xpmodel::Xp60Performance performance;
    };

    explicit UserPerformanceWrite(DeviceSession& session, QObject* parent = nullptr);

    [[nodiscard]] State state() const noexcept { return m_state; }
    [[nodiscard]] bool isBusy() const noexcept;
    [[nodiscard]] std::string_view stateName() const noexcept;
    [[nodiscard]] QString stateLabel() const;
    [[nodiscard]] QString message() const { return m_message; }

    // Arming ------------------------------------------------------------------
    [[nodiscard]] bool canArm() const;
    [[nodiscard]] bool isArmed() const noexcept { return m_armed; }
    bool arm();
    void disarm();

    [[nodiscard]] QString writePlanDescription(const std::vector<Destination>& destinations) const;

    // Operations --------------------------------------------------------------
    bool write(std::vector<Destination> destinations);
    bool writeOne(const xpmodel::Xp60Performance& performance, int userNumber);
    bool restore();
    [[nodiscard]] std::vector<Destination> unwritten() const;
    [[nodiscard]] bool canRetry() const;
    bool retry();
    void cancel();

    // Progress ----------------------------------------------------------------
    [[nodiscard]] std::size_t total() const noexcept { return m_destinations.size(); }
    [[nodiscard]] std::size_t completed() const noexcept { return m_completed; }
    // 1..32 while a destination is being worked on, 0 otherwise.
    [[nodiscard]] int currentUserNumber() const noexcept;
    [[nodiscard]] const std::vector<Destination>& snapshots() const noexcept { return m_snapshots; }
    [[nodiscard]] bool canRestore() const;

    [[nodiscard]] static bool isValidUserNumber(int userNumber) noexcept
    {
        return userNumber >= 1 && userNumber <= kUserPerformanceCount;
    }

Q_SIGNALS:
    void changed();
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
    void onPerformanceFetchChanged();
    void onBatchFinished(quint64 batchId, bool ok, const QString& error);
    [[nodiscard]] const Destination& current() const { return m_destinations[m_index]; }

    DeviceSession& m_session;
    State m_state = State::Idle;
    Awaiting m_awaiting = Awaiting::Nothing;
    QString m_message;
    bool m_armed = false;
    bool m_cancelRequested = false;
    bool m_restoring = false;

    std::vector<Destination> m_destinations;
    std::vector<Destination> m_snapshots;
    std::size_t m_index = 0;
    std::size_t m_completed = 0;
    DeviceSession::DataSetBatchId m_batch;
    std::optional<xpmodel::Xp60Performance> m_readBack;
};

} // namespace xp60studio::services
