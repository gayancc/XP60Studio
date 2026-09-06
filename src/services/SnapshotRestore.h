#pragma once

#include "services/DeviceSession.h"
#include "services/RestorePlan.h"
#include "services/SnapshotCapture.h"

#include <QObject>
#include <QString>

#include <cstddef>
#include <memory>
#include <vector>

namespace xp60studio::services {

// Writes a `RestorePlan` back to the instrument.
//
// The most destructive thing this application can do: it overwrites permanent
// memory a musician may have spent years filling. Building the plan and sending
// it are therefore separate — `RestorePlan` can be shown, argued with and
// exported without a device in the room, and this class does nothing until it
// is handed one that has already been inspected.
//
// ── The safety snapshot is not optional and not advisory ────────────────────
//
// Before a single byte is written, this reads the areas the plan will overwrite
// and **saves them to disk**. Saved, not held in memory: a backup that dies
// with the process is not a backup, and the moment it is needed is usually the
// moment something has gone wrong. If the capture fails, or the file cannot be
// written, the restore aborts before writing anything. There is no flag to skip
// it. The snapshot being restored is not the safety net — it is what will
// replace what is there now.
//
// ── Everything written is read back ─────────────────────────────────────────
//
// After sending, the same areas are read again and compared against what the
// plan intended, address by address. A restore that did not take is reported as
// a **mismatch**, never as success — and that is also exactly what the
// instrument's User Memory Protect being ON looks like from here, so the
// message names it rather than leaving the user to guess.
//
// ── What it refuses ─────────────────────────────────────────────────────────
//
//  * A plan whose areas cannot be captured. Without a safety snapshot there is
//    no undo, so a restore of an area this build cannot read is refused rather
//    than performed unprotected.
//  * A safety-snapshot path that already exists. Overwriting one backup to make
//    another is how both get lost.
//  * An unarmed run. Arming is single-use and separate from every other
//    writer's.
class SnapshotRestore : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        // Reading what is about to be overwritten. Nothing has been sent.
        SafetySnapshot,
        Sending,
        Verifying,
        Completed,
        // What was written did not read back. Nothing was lost: the safety
        // snapshot on disk holds what was there before.
        Mismatch,
        Failed,
        Cancelled,
    };

    explicit SnapshotRestore(DeviceSession& session, QObject* parent = nullptr);
    ~SnapshotRestore() override;

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

    // What a run would do and what it cannot protect, for the confirmation the
    // user sees. Includes the plan's own warnings.
    [[nodiscard]] QStringList concerns(const RestorePlan& plan, const QString& safetyPath) const;

    // Operations --------------------------------------------------------------
    // `safetySnapshotPath` is where the current contents are written before
    // anything is overwritten. Required, and must not already exist.
    bool restore(const RestorePlan& plan, const QString& safetySnapshotPath);
    void cancel();

    // Results -----------------------------------------------------------------
    [[nodiscard]] QString safetySnapshotPath() const { return m_safetyPath; }
    // True once the safety snapshot is on disk. From this moment the run can be
    // undone even if everything after it fails.
    [[nodiscard]] bool safetySnapshotSaved() const noexcept { return m_safetySaved; }
    [[nodiscard]] std::size_t messagesSent() const noexcept { return m_messagesSent; }
    // Addresses the read-back found holding something other than what was sent.
    [[nodiscard]] const std::vector<roland::RolandAddress>& mismatches() const noexcept
    {
        return m_mismatches;
    }

Q_SIGNALS:
    void changed();
    void finished(bool ok);

private:
    void beginSafetySnapshot();
    void onSafetySnapshotFinished(bool ok);
    void beginSending();
    void onBatchFinished(quint64 batchId, bool ok, const QString& error);
    void beginVerification();
    void onVerificationFinished(bool ok);
    void compareVerification();
    void finish(State state, QString message);
    void setState(State state, QString message);

    DeviceSession& m_session;
    std::unique_ptr<SnapshotCapture> m_capture;
    State m_state = State::Idle;
    QString m_message;
    bool m_armed = false;
    bool m_cancelRequested = false;

    RestorePlan m_plan;
    std::vector<RestoreArea> m_areas;
    QString m_safetyPath;
    bool m_safetySaved = false;
    bool m_verifying = false;
    std::size_t m_messagesSent = 0;
    DeviceSession::DataSetBatchId m_batch;
    std::vector<roland::RolandAddress> m_mismatches;
};

} // namespace xp60studio::services
