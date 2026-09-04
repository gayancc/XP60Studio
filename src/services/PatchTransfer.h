#pragma once

#include "services/DeviceSession.h"
#include "xpmodel/Xp60Patch.h"
#include "xpmodel/Xp60PatchDiff.h"

#include <QObject>
#include <QString>

#include <optional>

namespace xp60studio::services {

// Writes a Patch to the XP-60 and proves it arrived, per the Phase 3 loop:
//
//   safety snapshot -> send -> read back -> compare -> verified
//
// A completed byte transmission is not a verified hardware state, so this
// service never reports success on the strength of a send alone.
//
// Safety rules, deliberately narrow for Phase 3:
//  * the only writable target is the **temporary** Patch area (03 00 00 00).
//    Temporary memory is not saved across a power cycle, so a write here
//    cannot destroy the user's stored sounds. Permanent User memory is not
//    reachable through this class at all.
//  * a write must be armed immediately beforehand, and arming is spent by one
//    attempt.
//  * arming is refused until a temporary-Patch read has succeeded in this
//    session, which is what establishes that the address map is right (steps
//    1-7 of docs/HARDWARE_VALIDATION_XP60.md).
//  * the Patch present before the write is captured first, so the previous
//    state can be put back.
class PatchTransfer : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        CapturingSafetySnapshot,
        Sending,
        ReadingBack,
        Comparing,
        Verified,
        Mismatch,
        Failed,
        Cancelled,
    };

    explicit PatchTransfer(DeviceSession& session, QObject* parent = nullptr);

    [[nodiscard]] State state() const noexcept { return m_state; }
    [[nodiscard]] bool isBusy() const noexcept;
    [[nodiscard]] std::string_view stateName() const noexcept;
    [[nodiscard]] std::string stateLabel() const;
    [[nodiscard]] const std::string& message() const noexcept { return m_message; }

    // Arming -------------------------------------------------------------------
    // True once a temporary-Patch read has succeeded, i.e. the address map is
    // demonstrably correct for this instrument.
    [[nodiscard]] bool readVerified() const noexcept { return m_readVerified; }
    [[nodiscard]] bool canArm() const;
    [[nodiscard]] bool isArmed() const noexcept { return m_armed; }
    bool arm();
    void disarm();

    // What exactly a write would do, for the confirmation the user sees.
    [[nodiscard]] std::string writePlanDescription() const;

    // Operations ---------------------------------------------------------------
    // Writes `patch` to the temporary Patch area and verifies it. Consumes the
    // arming. False when refused (not armed, not connected, already busy).
    bool writeAndVerifyTemporaryPatch(const xpmodel::Xp60Patch& patch);
    // Writes the captured safety snapshot back, restoring the previous state.
    bool restoreSafetySnapshot();
    void cancel();

    // Results ------------------------------------------------------------------
    [[nodiscard]] const std::optional<xpmodel::Xp60Patch>& safetySnapshot() const noexcept { return m_safetySnapshot; }
    [[nodiscard]] const std::optional<xpmodel::Xp60Patch>& readBack() const noexcept { return m_readBack; }
    [[nodiscard]] const std::optional<xpmodel::Xp60PatchDiff>& diff() const noexcept { return m_diff; }
    [[nodiscard]] std::size_t messagesSent() const noexcept { return m_messagesSent; }

signals:
    void changed();

private:
    enum class Awaiting {
        Nothing,
        SafetySnapshot,
        SendBatch,
        ReadBack,
    };

    bool begin(const xpmodel::Xp60Patch& patch, std::string what);
    void onPatchFetchChanged();
    void onBatchFinished(quint64 batchId, bool ok, const QString& error);
    void sendIntendedPatch();
    void compareReadBack();
    void setState(State state, std::string message);
    void fail(std::string message);

    DeviceSession& m_session;
    State m_state = State::Idle;
    Awaiting m_awaiting = Awaiting::Nothing;
    bool m_armed = false;
    bool m_readVerified = false;
    std::string m_message;
    std::string m_what;
    std::optional<xpmodel::Xp60Patch> m_intended;
    std::optional<xpmodel::Xp60Patch> m_safetySnapshot;
    std::optional<xpmodel::Xp60Patch> m_readBack;
    std::optional<xpmodel::Xp60PatchDiff> m_diff;
    DeviceSession::DataSetBatchId m_batch;
    std::size_t m_messagesSent = 0;
};

} // namespace xp60studio::services
