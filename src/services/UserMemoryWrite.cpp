#include "services/UserMemoryWrite.h"

#include "xpmodel/Xp60BankLocation.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <algorithm>
#include <set>

namespace xp60studio::services {

using xpmodel::Xp60BankLocation;
using xpmodel::Xp60PatchCodec;
using xpmodel::Xp60PatchLayout;

namespace {

QString userLabel(int userNumber)
{
    return QStringLiteral("USER:%1").arg(userNumber, 3, 10, QLatin1Char('0'));
}

// "A35 · USER:021" — the panel identity leads, as it does everywhere else in
// the application, with the linear number beside it.
QString destinationLabel(int userNumber)
{
    const auto location = Xp60BankLocation::fromUserNumber(userNumber);
    if (!location) {
        return userLabel(userNumber);
    }
    return QStringLiteral("%1 · %2").arg(QString::fromStdString(location->panelLabel()), userLabel(userNumber));
}

} // namespace

UserMemoryWrite::UserMemoryWrite(DeviceSession& session, QObject* parent)
    : QObject(parent)
    , m_session(session)
{
    connect(&m_session, &DeviceSession::patchFetchChanged, this, &UserMemoryWrite::onPatchFetchChanged);
    connect(&m_session, &DeviceSession::dataSetBatchFinished, this, &UserMemoryWrite::onBatchFinished);
    connect(&m_session, &DeviceSession::connectionStateChanged, this, [this] {
        if (m_session.connectionState() == DeviceSession::ConnectionState::Connected) {
            return;
        }
        // Arming must never survive a disconnect, and a run interrupted by one
        // has left permanent memory in a state nobody has verified.
        if (m_armed) {
            m_armed = false;
            emit changed();
        }
        if (isBusy()) {
            fail(tr("Disconnected part-way through writing USER memory. %1 of %2 destination(s) were written and "
                    "verified; the rest were not touched.")
                     .arg(m_completed)
                     .arg(total()));
        }
    });
    connect(&m_session, &DeviceSession::deviceIdChanged, this, [this] {
        if (isBusy()) {
            fail(tr("The device ID changed part-way through writing USER memory, so the rest of the run was "
                    "abandoned."));
        }
        disarm();
    });
}

bool UserMemoryWrite::isBusy() const noexcept
{
    switch (m_state) {
    case State::Snapshotting:
    case State::Sending:
    case State::Verifying:
    case State::Restoring:
        return true;
    default:
        return false;
    }
}

std::string_view UserMemoryWrite::stateName() const noexcept
{
    switch (m_state) {
    case State::Idle:
        return "Idle";
    case State::Snapshotting:
        return "Snapshotting";
    case State::Sending:
        return "Sending";
    case State::Verifying:
        return "Verifying";
    case State::Completed:
        return "Completed";
    case State::Mismatch:
        return "Mismatch";
    case State::Failed:
        return "Failed";
    case State::Cancelled:
        return "Cancelled";
    case State::Restoring:
        return "Restoring";
    }
    return "Unknown";
}

QString UserMemoryWrite::stateLabel() const
{
    switch (m_state) {
    case State::Idle:
        return tr("Idle");
    case State::Snapshotting:
        return tr("Backing up the destination");
    case State::Sending:
        return tr("Writing");
    case State::Verifying:
        return tr("Verifying");
    case State::Completed:
        return tr("Written and verified");
    case State::Mismatch:
        return tr("The XP-60 did not keep what was written");
    case State::Failed:
        return tr("Failed");
    case State::Cancelled:
        return tr("Stopped");
    case State::Restoring:
        return tr("Putting the previous Patches back");
    }
    return {};
}

// ---------------------------------------------------------------------------
// Arming
// ---------------------------------------------------------------------------

bool UserMemoryWrite::canArm() const
{
    return !m_armed && !isBusy() && m_session.connectionState() == DeviceSession::ConnectionState::Connected;
}

bool UserMemoryWrite::arm()
{
    if (!canArm()) {
        return false;
    }
    m_armed = true;
    emit changed();
    return true;
}

void UserMemoryWrite::disarm()
{
    if (!m_armed) {
        return;
    }
    m_armed = false;
    emit changed();
}

QString UserMemoryWrite::writePlanDescription(const std::vector<Destination>& destinations) const
{
    if (destinations.empty()) {
        return tr("Nothing to write.");
    }
    auto numbers = destinations;
    std::sort(numbers.begin(), numbers.end(),
              [](const Destination& a, const Destination& b) { return a.userNumber < b.userNumber; });
    const auto first = destinationLabel(numbers.front().userNumber);
    const auto last = destinationLabel(numbers.back().userNumber);
    const QString range = destinations.size() == 1 ? first : tr("%1 to %2").arg(first, last);
    return tr("Overwrites %n Patch(es) in the XP-60's permanent USER memory, at %1. This cannot be undone on the "
              "instrument; XP60Studio reads each destination before writing it so it can put it back. If the "
              "instrument's User Memory Protect is ON the write will be refused and reported as a mismatch.",
              "", static_cast<int>(destinations.size()))
        .arg(range);
}

// ---------------------------------------------------------------------------
// Running
// ---------------------------------------------------------------------------

bool UserMemoryWrite::writeOne(const xpmodel::Xp60Patch& patch, int userNumber)
{
    return write({Destination{userNumber, patch}});
}

bool UserMemoryWrite::write(std::vector<Destination> destinations)
{
    if (isBusy() || destinations.empty()) {
        return false;
    }
    if (!m_armed) {
        setState(State::Failed, tr("Writing to USER memory must be armed first."));
        return false;
    }
    if (m_session.connectionState() != DeviceSession::ConnectionState::Connected) {
        setState(State::Failed, tr("Not connected to an XP-60."));
        return false;
    }
    // Validate the whole plan before touching anything. A run that turns out to
    // be partly illegal must not leave permanent memory half rewritten.
    std::set<int> seen;
    for (const auto& destination : destinations) {
        if (!Xp60BankLocation::isValidUserNumber(destination.userNumber)) {
            setState(State::Failed,
                     tr("USER:%1 is outside the 128-slot User bank; nothing was written.")
                         .arg(destination.userNumber));
            return false;
        }
        if (!seen.insert(destination.userNumber).second) {
            // Two Patches at one destination leaves only the second, which is a
            // silent loss on the instrument rather than an error anyone sees.
            setState(State::Failed,
                     tr("Two Patches are addressed to %1; only the second would survive, so nothing was written.")
                         .arg(destinationLabel(destination.userNumber)));
            return false;
        }
    }

    m_destinations = std::move(destinations);
    m_snapshots.clear();
    m_index = 0;
    m_completed = 0;
    m_cancelRequested = false;
    m_restoring = false;
    m_readBack.reset();
    // The arming is spent by this attempt, whatever happens to it.
    m_armed = false;
    beginNextDestination();
    return true;
}

void UserMemoryWrite::beginNextDestination()
{
    if (m_cancelRequested) {
        finishRun(State::Cancelled,
                  tr("Stopped after %n destination(s). What was already written stays written, and can be put back.",
                     "", static_cast<int>(m_completed)));
        return;
    }
    if (m_index >= m_destinations.size()) {
        if (m_restoring) {
            finishRun(State::Completed,
                      tr("Put back %n Patch(es); every one was read back and verified.", "",
                         static_cast<int>(m_completed)));
        } else {
            finishRun(State::Completed,
                      tr("Wrote %n Patch(es) to the XP-60's USER memory; every one was read back and verified.", "",
                         static_cast<int>(m_completed)));
        }
        return;
    }

    if (m_restoring) {
        // Restoring already knows what was there; there is nothing to back up.
        sendCurrent();
        return;
    }

    m_awaiting = Awaiting::Snapshot;
    setState(State::Snapshotting,
             tr("Reading %1 so it can be put back").arg(destinationLabel(current().userNumber)));
    if (m_awaiting != Awaiting::Snapshot) {
        return;
    }
    const auto address = Xp60PatchLayout::userPatchAddress(current().userNumber);
    if (!address || !m_session.fetchPatch(*address, DeviceSession::PatchFetchPurpose::Transfer)) {
        fail(tr("Could not read %1, so nothing was written to it.").arg(destinationLabel(current().userNumber)));
    }
}

void UserMemoryWrite::sendCurrent()
{
    const auto address = Xp60PatchLayout::userPatchAddress(current().userNumber);
    if (!address) {
        fail(tr("USER:%1 has no address.").arg(current().userNumber));
        return;
    }
    const auto messages = Xp60PatchCodec::encodeToDataSets(current().patch, m_session.deviceId(), m_session.modelId(),
                                                           *address, m_session.pacing().maxDataSetPayloadBytes);
    if (messages.empty()) {
        fail(tr("Could not encode the Patch for %1.").arg(destinationLabel(current().userNumber)));
        return;
    }
    m_awaiting = Awaiting::SendBatch;
    setState(State::Sending, m_restoring
                 ? tr("Putting back %1").arg(destinationLabel(current().userNumber))
                 : tr("Writing %1").arg(destinationLabel(current().userNumber)));
    if (m_awaiting != Awaiting::SendBatch) {
        return;
    }
    m_batch = m_session.sendDataSets(messages);
    if (!m_batch.isValid()) {
        fail(tr("Could not queue the write for %1.").arg(destinationLabel(current().userNumber)));
    }
}

void UserMemoryWrite::beginReadBack()
{
    m_awaiting = Awaiting::ReadBack;
    m_readBack.reset();
    setState(State::Verifying, tr("Reading %1 back").arg(destinationLabel(current().userNumber)));
    if (m_awaiting != Awaiting::ReadBack) {
        return;
    }
    const auto address = Xp60PatchLayout::userPatchAddress(current().userNumber);
    if (!address || !m_session.fetchPatch(*address, DeviceSession::PatchFetchPurpose::Transfer)) {
        fail(tr("%1 was written but could not be read back, so it is unverified.")
                 .arg(destinationLabel(current().userNumber)));
    }
}

void UserMemoryWrite::compareReadBack()
{
    if (!m_readBack) {
        fail(tr("%1 was written but nothing came back to compare.").arg(destinationLabel(current().userNumber)));
        return;
    }
    if (!(*m_readBack == current().patch)) {
        // This is also exactly what User Memory Protect being ON looks like, so
        // the message names it rather than leaving the user to guess.
        finishRun(State::Mismatch,
                  tr("%1 read back differently from what was sent, so it was not stored. The most likely cause is "
                     "User Memory Protect being ON (UTILITY → Protect on the instrument). %2 destination(s) before "
                     "it were written and verified.")
                      .arg(destinationLabel(current().userNumber))
                      .arg(m_completed));
        return;
    }

    ++m_completed;
    ++m_index;
    m_awaiting = Awaiting::Nothing;
    emit progressed(m_completed, total());
    beginNextDestination();
}

bool UserMemoryWrite::canRestore() const
{
    return !isBusy() && !m_snapshots.empty()
        && m_session.connectionState() == DeviceSession::ConnectionState::Connected;
}

bool UserMemoryWrite::restore()
{
    if (!canRestore()) {
        return false;
    }
    // Most recent first, so the instrument passes back through the states it
    // came through.
    auto destinations = m_snapshots;
    std::reverse(destinations.begin(), destinations.end());

    m_destinations = std::move(destinations);
    m_snapshots.clear();
    m_index = 0;
    m_completed = 0;
    m_cancelRequested = false;
    m_restoring = true;
    m_readBack.reset();
    beginNextDestination();
    return true;
}

void UserMemoryWrite::cancel()
{
    if (!isBusy()) {
        return;
    }
    // Honoured between destinations, never in the middle of one: stopping
    // half-way through a Patch would leave a destination holding a mixture of
    // two sounds.
    m_cancelRequested = true;
    setState(m_state, tr("Stopping after the destination being written."));
}

// ---------------------------------------------------------------------------
// Session callbacks
// ---------------------------------------------------------------------------

void UserMemoryWrite::onPatchFetchChanged()
{
    const auto& fetch = m_session.patchFetch();

    if (m_awaiting == Awaiting::Snapshot) {
        if (fetch.state == DeviceSession::PatchFetchState::Completed && fetch.patch) {
            m_snapshots.push_back(Destination{current().userNumber, *fetch.patch});
            m_awaiting = Awaiting::Nothing;
            sendCurrent();
        } else if (fetch.state == DeviceSession::PatchFetchState::Failed) {
            fail(tr("Could not read %1 before writing it, so nothing was written to it: %2")
                     .arg(destinationLabel(current().userNumber), QString::fromStdString(fetch.message)));
        }
        return;
    }

    if (m_awaiting == Awaiting::ReadBack) {
        if (fetch.state == DeviceSession::PatchFetchState::Completed && fetch.patch) {
            m_readBack = fetch.patch;
            m_awaiting = Awaiting::Nothing;
            compareReadBack();
        } else if (fetch.state == DeviceSession::PatchFetchState::Failed) {
            fail(tr("%1 was written but could not be read back, so it is unverified: %2")
                     .arg(destinationLabel(current().userNumber), QString::fromStdString(fetch.message)));
        }
    }
}

void UserMemoryWrite::onBatchFinished(quint64 batchId, bool ok, const QString& error)
{
    if (m_awaiting != Awaiting::SendBatch || batchId != m_batch.value) {
        return;
    }
    m_awaiting = Awaiting::Nothing;
    if (!ok) {
        fail(tr("Writing %1 failed: %2").arg(destinationLabel(current().userNumber), error));
        return;
    }
    beginReadBack();
}

// ---------------------------------------------------------------------------

int UserMemoryWrite::currentUserNumber() const noexcept
{
    if (!isBusy() || m_index >= m_destinations.size()) {
        return 0;
    }
    return m_destinations[m_index].userNumber;
}

void UserMemoryWrite::setState(State state, QString message)
{
    m_state = state;
    m_message = std::move(message);
    emit changed();
}

void UserMemoryWrite::fail(QString message)
{
    finishRun(State::Failed, std::move(message));
}

void UserMemoryWrite::finishRun(State state, QString message)
{
    m_awaiting = Awaiting::Nothing;
    m_batch = {};
    m_readBack.reset();
    const bool ok = state == State::Completed;
    m_restoring = false;
    setState(state, std::move(message));
    emit finished(ok);
}

} // namespace xp60studio::services
