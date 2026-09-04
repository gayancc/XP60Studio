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
    connect(&m_session, &DeviceSession::patchFetchChanged, this, &PatchTransfer::onPatchFetchChanged);
    connect(&m_session, &DeviceSession::dataSetBatchFinished, this, &PatchTransfer::onBatchFinished);
    connect(&m_session, &DeviceSession::connectionStateChanged, this, [this] {
        if (m_session.connectionState() != DeviceSession::ConnectionState::Connected) {
            // Arming must never survive a disconnect.
            if (m_armed) {
                m_armed = false;
                emit changed();
            }
            if (isBusy()) {
                fail("Disconnected during the transfer");
            }
        }
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
        && !m_armed;
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

bool PatchTransfer::begin(const xpmodel::Xp60Patch& patch, std::string what)
{
    if (!m_armed) {
        setState(State::Failed, "Refused: writing to the XP-60 must be armed immediately beforehand");
        return false;
    }
    if (m_session.connectionState() != DeviceSession::ConnectionState::Connected) {
        fail("Refused: not connected");
        return false;
    }
    if (isBusy()) {
        return false;
    }

    m_what = std::move(what);
    m_intended = patch;
    m_safetySnapshot.reset();
    m_readBack.reset();
    m_diff.reset();
    m_messagesSent = 0;
    m_awaiting = Awaiting::SafetySnapshot;
    setState(State::CapturingSafetySnapshot, "Reading the current temporary Patch so it can be restored");

    if (!m_session.fetchTemporaryPatch()) {
        fail("Could not start the safety snapshot read");
        return false;
    }
    return true;
}

void PatchTransfer::cancel()
{
    if (!isBusy()) {
        return;
    }
    m_session.cancelPatchFetch();
    m_awaiting = Awaiting::Nothing;
    m_armed = false;
    setState(State::Cancelled, "Transfer cancelled. The temporary Patch may hold partially written data; "
                              "restore the safety snapshot or power-cycle the XP-60.");
}

void PatchTransfer::sendIntendedPatch()
{
    const auto base = Xp60PatchLayout::temporaryPatchAddress();
    const auto messages = Xp60PatchCodec::encodeToDataSets(*m_intended, m_session.deviceId(), m_session.modelId(), base,
                                                           m_session.pacing().maxDataSetPayloadBytes);
    if (messages.empty()) {
        fail("Could not encode the patch for transmission");
        return;
    }
    m_messagesSent = messages.size();
    m_awaiting = Awaiting::SendBatch;
    setState(State::Sending, "Sending " + std::to_string(messages.size()) + " DT1 message(s) to " + base.toHexString());

    m_batch = m_session.sendDataSets(messages);
    if (!m_batch.isValid()) {
        fail("Could not queue the DT1 messages");
    }
}

void PatchTransfer::compareReadBack()
{
    setState(State::Comparing, "Comparing the read-back against what was sent");
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
        setState(State::Mismatch, "Read-back differs from what was sent: " + m_diff->summary()
                                      + ". Investigate before trusting the codec; nothing has been normalised.");
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
    m_awaiting = Awaiting::ReadBack;
    m_armed = false; // the arming is spent by this attempt
    setState(State::ReadingBack, "Sent. Reading the temporary Patch back to verify it");
    if (!m_session.fetchTemporaryPatch()) {
        fail("The data was sent but the read-back could not be started, so it is unverified");
    }
}

} // namespace xp60studio::services
