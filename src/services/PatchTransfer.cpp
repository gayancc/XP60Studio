#include "services/PatchTransfer.h"

#include "xp60/Xp60Device.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

namespace xp60studio::services {

using xpmodel::Xp60PatchCodec;
using xpmodel::Xp60PatchDiff;
using xpmodel::Xp60PatchLayout;

PatchTransfer::PatchTransfer(DeviceSession& session, QObject* parent)
    : QObject(parent)
    , m_session(session)
{
    m_liveTimer.setSingleShot(true);
    m_liveTimer.setInterval(120);
    connect(&m_liveTimer, &QTimer::timeout, this, &PatchTransfer::drainLivePreview);
    // The gesture has stopped: prove what was sent, now that the cost of a full
    // read-back no longer lands in the middle of a knob drag.
    m_settleTimer.setSingleShot(true);
    m_settleTimer.setInterval(static_cast<int>(m_settleDelay.count()));
    connect(&m_settleTimer, &QTimer::timeout, this, [this] { verifyNow(); });
    connect(&m_session, &DeviceSession::patchFetchChanged, this, &PatchTransfer::onPatchFetchChanged);
    connect(&m_session, &DeviceSession::dataSetBatchFinished, this, &PatchTransfer::onBatchFinished);
    connect(&m_session, &DeviceSession::connectionStateChanged, this, [this] {
        if (m_session.connectionState() != DeviceSession::ConnectionState::Connected) {
            m_readVerified = false;
            // Arming must never survive a disconnect.
            if (m_armed) {
                m_armed = false;
                emit changed();
            }
            if (isBusy() || m_liveActive) {
                fail("Disconnected during the transfer");
            }
            emit changed();
        }
    });
    connect(&m_session, &DeviceSession::deviceIdChanged, this, [this] {
        if (isBusy() || m_liveActive) {
            cancel();
        }
        m_session.cancelPatchFetch(); // an old device's pending read proves nothing about the new one
        m_readVerified = false;
        disarm();
        emit changed();
    });
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

bool PatchTransfer::isBusy() const noexcept
{
    switch (m_state) {
    case State::CapturingSafetySnapshot:
    case State::Sending:
    case State::ReadingBack:
    case State::Comparing:
        return true;
    default:
        return false;
    }
}

std::string_view PatchTransfer::stateName() const noexcept
{
    switch (m_state) {
    case State::Idle:
        return "Idle";
    case State::CapturingSafetySnapshot:
        return "CapturingSafetySnapshot";
    case State::Sending:
        return "Sending";
    case State::ReadingBack:
        return "ReadingBack";
    case State::Comparing:
        return "Comparing";
    case State::Sent:
        return "Sent";
    case State::Verified:
        return "Verified";
    case State::Mismatch:
        return "Mismatch";
    case State::Failed:
        return "Failed";
    case State::Cancelled:
        return "Cancelled";
    }
    return "Unknown";
}

std::string PatchTransfer::stateLabel() const
{
    switch (m_state) {
    case State::Idle:
        return "Idle";
    case State::CapturingSafetySnapshot:
        return "Capturing safety snapshot";
    case State::Sending:
        return "Sending";
    case State::ReadingBack:
        return "Reading back";
    case State::Comparing:
        return "Comparing";
    case State::Sent:
        return "Sent, not yet verified";
    case State::Verified:
        return "Verified";
    case State::Mismatch:
        return "Read-back mismatch";
    case State::Failed:
        return "Failed";
    case State::Cancelled:
        return "Cancelled";
    }
    return "Unknown";
}

void PatchTransfer::setState(State state, std::string message)
{
    m_state = state;
    m_message = std::move(message);
    emit changed();
}

void PatchTransfer::fail(std::string message)
{
    clearLivePreview();
    m_awaiting = Awaiting::Nothing;
    m_armed = false;
    setState(State::Failed, std::move(message));
}

// ---------------------------------------------------------------------------
// Arming
// ---------------------------------------------------------------------------

bool PatchTransfer::canArm() const
{
    return m_session.connectionState() == DeviceSession::ConnectionState::Connected && m_readVerified && !isBusy()
        && !m_armed && !m_liveActive && !m_session.tracker().hasOutstanding();
}

bool PatchTransfer::arm()
{
    if (!canArm()) {
        return false;
    }
    m_armed = true;
    emit changed();
    return true;
}

void PatchTransfer::disarm()
{
    if (!m_armed) {
        return;
    }
    m_armed = false;
    emit changed();
}

std::string PatchTransfer::writePlanDescription() const
{
    const auto base = Xp60PatchLayout::temporaryPatchAddress();
    return "Writes " + std::to_string(Xp60PatchLayout::patchCommonSize() + 4 * Xp60PatchLayout::toneSize())
        + " bytes to the temporary Patch area at " + base.toHexString()
        + " (the edit buffer, not saved memory), then reads it back and compares every parameter. The XP-60's "
          "permanent User patches are not touched, and a power cycle clears the temporary area.";
}

// ---------------------------------------------------------------------------
// Operations
// ---------------------------------------------------------------------------

bool PatchTransfer::writeAndVerifyTemporaryPatch(const xpmodel::Xp60Patch& patch)
{
    return begin(patch, "patch");
}

bool PatchTransfer::restoreSafetySnapshot()
{
    if (!m_safetySnapshot) {
        return false;
    }
    // Copy first: begin() clears the snapshot before re-capturing.
    const auto snapshot = *m_safetySnapshot;
    return begin(snapshot, "safety snapshot");
}

bool PatchTransfer::begin(const xpmodel::Xp60Patch& patch, std::string what, bool live)
{
    // A second request must not corrupt an operation already in flight.
    if (isBusy() || m_liveActive || m_session.tracker().hasOutstanding()) {
        return false;
    }
    if (!m_armed) {
        setState(State::Failed, "Refused: writing to the XP-60 must be armed immediately beforehand");
        return false;
    }
    if (m_session.connectionState() != DeviceSession::ConnectionState::Connected) {
        fail("Refused: not connected");
        return false;
    }
    m_armed = false; // one attempt consumes arming, including snapshot failures
    m_liveActive = live;
    m_liveStopping = false;
    m_livePrevious.reset();
    m_livePending.reset();
    m_what = std::move(what);
    m_intended = patch;
    m_safetySnapshot.reset();
    m_readBack.reset();
    m_diff.reset();
    m_messagesSent = 0;
    m_awaiting = Awaiting::SafetySnapshot;
    setState(State::CapturingSafetySnapshot, "Reading the current temporary Patch so it can be restored");

    if (m_awaiting != Awaiting::SafetySnapshot) {
        return false;
    }

    if (!m_session.fetchTemporaryPatch(DeviceSession::PatchFetchPurpose::Transfer)) {
        fail("Could not start the safety snapshot read");
        return false;
    }
    return true;
}

void PatchTransfer::cancel()
{
    if (!isBusy() && !m_liveActive) {
        return;
    }
    m_awaiting = Awaiting::Nothing;
    clearLivePreview();
    // Clear the awaiting state first: cancellation emits synchronous signals.
    m_session.cancelPatchFetch();
    m_session.cancelDataSetBatch(m_batch);
    m_armed = false;
    setState(State::Cancelled, "Transfer cancelled. The temporary Patch may hold partially written data; "
                              "restore the safety snapshot or power-cycle the XP-60.");
}

void PatchTransfer::sendIntendedPatch()
{
    const auto base = Xp60PatchLayout::temporaryPatchAddress();
    const auto& previous = m_livePrevious ? m_livePrevious : m_safetySnapshot;
    if (m_liveActive && previous && *previous == *m_intended) {
        m_messagesSent = 0;
        beginReadBack();
        return;
    }
    const auto messages = m_liveActive && previous
        ? Xp60PatchCodec::encodeChangesToDataSets(*previous, *m_intended, m_session.deviceId(), m_session.modelId(), base,
                                                m_session.pacing().maxDataSetPayloadBytes)
        : Xp60PatchCodec::encodeToDataSets(*m_intended, m_session.deviceId(), m_session.modelId(), base,
                                          m_session.pacing().maxDataSetPayloadBytes);
    if (messages.empty()) {
        fail("Could not encode the patch for transmission");
        return;
    }
    m_messagesSent = messages.size();
    m_awaiting = Awaiting::SendBatch;
    setState(State::Sending, "Sending " + std::to_string(messages.size()) + " DT1 message(s) to " + base.toHexString());

    if (m_awaiting != Awaiting::SendBatch) {
        return;
    }

    m_batch = m_session.sendDataSets(messages);
    if (!m_batch.isValid()) {
        fail("Could not queue the DT1 messages");
    }
}

void PatchTransfer::compareReadBack()
{
    setState(State::Comparing, "Comparing the read-back against what was sent");
    if (m_awaiting != Awaiting::ReadBack) {
        return;
    }
    auto difference = Xp60PatchDiff::compare(*m_intended, *m_readBack);
    const bool identical = difference.identical();
    m_diff = std::move(difference);
    m_awaiting = Awaiting::Nothing;

    if (identical) {
        setState(State::Verified, "Verified: the XP-60 read back exactly the " + m_what + " that was sent ("
                                      + std::to_string(m_messagesSent) + " message(s), all "
                                      + std::to_string(Xp60PatchLayout::patchCommonSize() + 4 * Xp60PatchLayout::toneSize())
                                      + " bytes)");
    } else {
        clearLivePreview();
        setState(State::Mismatch, "Read-back differs from what was sent: " + m_diff->summary()
                                      + ". Investigate before trusting the codec; nothing has been normalised.");
    }
    if (m_liveActive && (m_livePending || m_liveStopping)) m_liveTimer.start();
}

bool PatchTransfer::startLivePreview(const xpmodel::Xp60Patch& patch)
{
    return begin(patch, "live audition Patch", true);
}

void PatchTransfer::queueLivePreview(const xpmodel::Xp60Patch& patch)
{
    if (!m_liveActive || m_liveStopping) return;
    if (m_livePending && *m_livePending == patch) return;
    if (!m_livePending && m_intended && *m_intended == patch) return;
    m_livePending = patch;
    // Throttle rather than restarting on every pointer move. Only the latest
    // desired Patch survives while a verified transfer is in flight.
    if (!isBusy() && !m_liveTimer.isActive()) m_liveTimer.start();
}

void PatchTransfer::stopLivePreview(const xpmodel::Xp60Patch& finalPatch)
{
    if (!m_liveActive || m_liveStopping) return;
    m_liveStopping = true;
    m_livePending = finalPatch;
    if (!isBusy()) m_liveTimer.start();
    emit changed();
}

void PatchTransfer::clearLivePreview()
{
    // Deferred verification is a concession to a gesture in progress. A one-shot
    // armed write is a deliberate act with nothing to interrupt, so leaving live
    // mode restores immediate verification rather than letting the relaxed
    // policy leak into it.
    m_settleTimer.stop();
    m_verification = Verification::EveryUpdate;
    m_liveTimer.stop();
    m_liveActive = false;
    m_liveStopping = false;
    m_livePending.reset();
    m_livePrevious.reset();
}

void PatchTransfer::drainLivePreview()
{
    if (!m_liveActive || isBusy()) return;
    if (m_session.connectionState() != DeviceSession::ConnectionState::Connected || !m_readBack) {
        fail("Live audition stopped: the previous Patch state is unverified");
        return;
    }
    if (m_session.tracker().hasOutstanding()) {
        fail("Live audition stopped because another read is in progress");
        return;
    }
    if (m_livePending && !(*m_livePending == *m_readBack)) {
        m_livePrevious = m_readBack;
        m_intended = m_livePending;
        m_livePending.reset();
        m_readBack.reset();
        m_diff.reset();
        sendIntendedPatch();
        return;
    }
    m_livePending.reset();
    if (m_liveStopping) {
        clearLivePreview();
        setState(State::Verified, "Live audition stopped. The final temporary Patch was read back and verified.");
    }
}

// ---------------------------------------------------------------------------
// DeviceSession events
// ---------------------------------------------------------------------------

void PatchTransfer::onPatchFetchChanged()
{
    const auto& fetch = m_session.patchFetch();

    // A successful temporary-Patch read is what unlocks arming, whoever
    // started it.
    if (fetch.state == DeviceSession::PatchFetchState::Completed && fetch.patch
        && fetch.base == Xp60PatchLayout::temporaryPatchAddress() && !m_readVerified) {
        m_readVerified = true;
        emit changed();
    }

    if (m_awaiting == Awaiting::SafetySnapshot) {
        if (fetch.state == DeviceSession::PatchFetchState::Completed && fetch.patch) {
            m_safetySnapshot = fetch.patch;
            sendIntendedPatch();
        } else if (fetch.state == DeviceSession::PatchFetchState::Failed) {
            fail("Safety snapshot failed, so nothing was written: " + fetch.message);
        }
        return;
    }

    if (m_awaiting == Awaiting::ReadBack) {
        if (fetch.state == DeviceSession::PatchFetchState::Completed && fetch.patch) {
            m_readBack = fetch.patch;
            compareReadBack();
        } else if (fetch.state == DeviceSession::PatchFetchState::Failed) {
            fail("The data was sent but could not be read back, so it is unverified: " + fetch.message);
        }
    }
}

void PatchTransfer::onBatchFinished(quint64 batchId, bool ok, const QString& error)
{
    if (m_awaiting != Awaiting::SendBatch || batchId != m_batch.value) {
        return;
    }
    if (!ok) {
        fail("Sending failed after " + std::to_string(m_messagesSent) + " queued message(s): " + error.toStdString());
        return;
    }
    if (m_liveActive && !m_liveStopping && m_verification == Verification::WhenSettled) {
        // Deliberately not read back. The next update may go out immediately,
        // and the state says exactly what is true: transmitted, unproved. The
        // next diff is computed against what was sent, which is sound because
        // the transport reported the batch delivered -- and is precisely why
        // this is not called Verified.
        m_awaiting = Awaiting::Nothing;
        m_readBack = m_intended;
        m_diff.reset();
        setState(State::Sent, "Sent " + std::to_string(m_messagesSent)
            + " DT1 message(s). Not read back yet; verification follows when editing stops.");
        m_settleTimer.start();
        if (m_livePending) {
            m_liveTimer.start();
        }
        return;
    }
    beginReadBack();
}

void PatchTransfer::setVerification(Verification verification)
{
    if (m_verification == verification) {
        return;
    }
    m_verification = verification;
    if (m_verification == Verification::EveryUpdate) {
        m_settleTimer.stop();
    }
    emit changed();
}

void PatchTransfer::setSettleDelay(std::chrono::milliseconds delay)
{
    m_settleDelay = delay < std::chrono::milliseconds{0} ? std::chrono::milliseconds{0} : delay;
    m_settleTimer.setInterval(static_cast<int>(m_settleDelay.count()));
}

bool PatchTransfer::verifyNow()
{
    m_settleTimer.stop();
    // Only meaningful when something was sent and not proved. Reading while a
    // transfer is in flight would collide with it, and the XP-60 drops requests
    // that arrive while it is transmitting.
    if (m_state != State::Sent || isBusy() || !m_liveActive) {
        return false;
    }
    if (m_livePending) {
        // A newer edit is already queued. Verifying the superseded one would
        // prove something nobody is listening to; let the update go out and the
        // settle timer come round again.
        return false;
    }
    m_readBack.reset();
    beginReadBack();
    return true;
}

void PatchTransfer::beginReadBack()
{
    m_awaiting = Awaiting::ReadBack;
    m_armed = false; // the arming is spent by this attempt
    setState(State::ReadingBack, "Sent. Reading the temporary Patch back to verify it");
    if (m_awaiting != Awaiting::ReadBack) {
        return;
    }
    if (!m_session.fetchTemporaryPatch(DeviceSession::PatchFetchPurpose::Transfer)) {
        fail("The data was sent but the read-back could not be started, so it is unverified");
    }
}

} // namespace xp60studio::services
