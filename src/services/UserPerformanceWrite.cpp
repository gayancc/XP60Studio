#include "services/UserPerformanceWrite.h"

#include "xpmodel/Xp60PerformanceCodec.h"
#include "xpmodel/Xp60PerformanceLayout.h"

#include <algorithm>
#include <set>

namespace xp60studio::services {

using xpmodel::Xp60PerformanceCodec;
using xpmodel::Xp60PerformanceLayout;

namespace {

// The instrument prints User Performances as USER:01..32, two digits.
QString destinationLabel(int userNumber)
{
    return QStringLiteral("USER:%1").arg(userNumber, 2, 10, QLatin1Char('0'));
}

} // namespace

UserPerformanceWrite::UserPerformanceWrite(DeviceSession& session, QObject* parent)
    : QObject(parent)
    , m_session(session)
{
    connect(&m_session, &DeviceSession::patchFetchChanged, this,
            &UserPerformanceWrite::onPerformanceFetchChanged);
    connect(&m_session, &DeviceSession::dataSetBatchFinished, this,
            &UserPerformanceWrite::onBatchFinished);
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
            fail(tr("Disconnected part-way through writing USER Performance memory. %1 of %2 "
                    "destination(s) were written and verified; the rest were not touched.")
                     .arg(m_completed)
                     .arg(total()));
        }
    });
    connect(&m_session, &DeviceSession::deviceIdChanged, this, [this] {
        if (isBusy()) {
            fail(tr("The device ID changed part-way through writing USER Performance memory, so the "
                    "rest of the run was abandoned."));
        }
        disarm();
    });
}

bool UserPerformanceWrite::isBusy() const noexcept
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

std::string_view UserPerformanceWrite::stateName() const noexcept
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

QString UserPerformanceWrite::stateLabel() const
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
        return tr("Putting the previous Performances back");
    }
    return {};
}

// ---------------------------------------------------------------------------
// Arming
// ---------------------------------------------------------------------------

bool UserPerformanceWrite::canArm() const
{
    return !m_armed && !isBusy()
        && m_session.connectionState() == DeviceSession::ConnectionState::Connected;
}

bool UserPerformanceWrite::arm()
{
    if (!canArm()) {
        return false;
    }
    m_armed = true;
    emit changed();
    return true;
}

void UserPerformanceWrite::disarm()
{
    if (!m_armed) {
        return;
    }
    m_armed = false;
    emit changed();
}

QString UserPerformanceWrite::writePlanDescription(const std::vector<Destination>& destinations) const
{
    if (destinations.empty()) {
        return tr("Nothing to write.");
    }
    auto sorted = destinations;
    std::sort(sorted.begin(), sorted.end(),
              [](const Destination& a, const Destination& b) { return a.userNumber < b.userNumber; });
    const QString range = destinations.size() == 1
        ? destinationLabel(sorted.front().userNumber)
        : tr("%1 to %2").arg(destinationLabel(sorted.front().userNumber),
                             destinationLabel(sorted.back().userNumber));
    return tr("Overwrites %n Performance(s) in the XP-60's permanent USER memory, at %1. A "
              "Performance names sixteen Patches by bank and number; it does not carry them, so "
              "the Patches those Parts refer to are not written and must already be on the "
              "instrument. This cannot be undone on the instrument; XP60Studio reads each "
              "destination before writing it so it can put it back. If the instrument's User "
              "Memory Protect is ON the write will be refused and reported as a mismatch.",
              "", static_cast<int>(destinations.size()))
        .arg(range);
}

// ---------------------------------------------------------------------------
// Running
// ---------------------------------------------------------------------------

bool UserPerformanceWrite::writeOne(const xpmodel::Xp60Performance& performance, int userNumber)
{
    return write({Destination{userNumber, performance}});
}

bool UserPerformanceWrite::write(std::vector<Destination> destinations)
{
    if (isBusy() || destinations.empty()) {
        return false;
    }
    if (!m_armed) {
        setState(State::Failed, tr("Writing to USER Performance memory must be armed first."));
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
        if (!isValidUserNumber(destination.userNumber)) {
            setState(State::Failed,
                     tr("USER:%1 is outside the 32-slot User Performance bank; nothing was written.")
                         .arg(destination.userNumber));
            return false;
        }
        if (!seen.insert(destination.userNumber).second) {
            setState(State::Failed,
                     tr("Two Performances are addressed to %1; only the second would survive, so "
                        "nothing was written.")
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

void UserPerformanceWrite::beginNextDestination()
{
    if (m_cancelRequested) {
        finishRun(State::Cancelled,
                  tr("Stopped after %n destination(s). What was already written stays written, and "
                     "can be put back.",
                     "", static_cast<int>(m_completed)));
        return;
    }
    if (m_index >= m_destinations.size()) {
        finishRun(State::Completed,
                  m_restoring
                      ? tr("Put back %n Performance(s); every one was read back and verified.", "",
                           static_cast<int>(m_completed))
                      : tr("Wrote %n Performance(s) to the XP-60's USER memory; every one was read "
                           "back and verified.",
                           "", static_cast<int>(m_completed)));
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
    const auto address = Xp60PerformanceLayout::userPerformanceAddress(current().userNumber);
    if (!address
        || !m_session.fetchPerformance(*address, DeviceSession::PatchFetchPurpose::Transfer)) {
        fail(tr("Could not read %1, so nothing was written to it.")
                 .arg(destinationLabel(current().userNumber)));
    }
}

void UserPerformanceWrite::sendCurrent()
{
    const auto address = Xp60PerformanceLayout::userPerformanceAddress(current().userNumber);
    if (!address) {
        fail(tr("USER:%1 has no address.").arg(current().userNumber));
        return;
    }
    const auto messages
        = Xp60PerformanceCodec::encodeToDataSets(current().performance, m_session.deviceId(),
                                                 m_session.modelId(), *address,
                                                 m_session.pacing().maxDataSetPayloadBytes);
    if (messages.empty()) {
        fail(tr("Could not encode the Performance for %1.")
                 .arg(destinationLabel(current().userNumber)));
        return;
    }
    m_awaiting = Awaiting::SendBatch;
    setState(State::Sending,
             m_restoring ? tr("Putting back %1").arg(destinationLabel(current().userNumber))
                         : tr("Writing %1").arg(destinationLabel(current().userNumber)));
    if (m_awaiting != Awaiting::SendBatch) {
        return;
    }
    m_batch = m_session.sendDataSets(messages);
    if (!m_batch.isValid()) {
        fail(tr("Could not queue the write for %1.").arg(destinationLabel(current().userNumber)));
    }
}

void UserPerformanceWrite::beginReadBack()
{
    m_awaiting = Awaiting::ReadBack;
    m_readBack.reset();
    setState(State::Verifying, tr("Reading %1 back").arg(destinationLabel(current().userNumber)));
    if (m_awaiting != Awaiting::ReadBack) {
        return;
    }
    const auto address = Xp60PerformanceLayout::userPerformanceAddress(current().userNumber);
    if (!address
        || !m_session.fetchPerformance(*address, DeviceSession::PatchFetchPurpose::Transfer)) {
        fail(tr("%1 was written but could not be read back, so it is unverified.")
                 .arg(destinationLabel(current().userNumber)));
    }
}

void UserPerformanceWrite::compareReadBack()
{
    if (!m_readBack) {
        fail(tr("%1 was written but nothing came back to compare.")
                 .arg(destinationLabel(current().userNumber)));
        return;
    }
    if (!(*m_readBack == current().performance)) {
        // This is also exactly what User Memory Protect being ON looks like, so
        // the message names it rather than leaving the user to guess.
        finishRun(State::Mismatch,
                  tr("%1 read back differently from what was sent, so it was not stored. The most "
                     "likely cause is User Memory Protect being ON (UTILITY → Protect on the "
                     "instrument). %2 destination(s) before it were written and verified.")
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

bool UserPerformanceWrite::canRestore() const
{
    return !isBusy() && !m_snapshots.empty()
        && m_session.connectionState() == DeviceSession::ConnectionState::Connected;
}

bool UserPerformanceWrite::restore()
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

std::vector<UserPerformanceWrite::Destination> UserPerformanceWrite::unwritten() const
{
    if (m_restoring || m_index >= m_destinations.size()) {
        return {};
    }
    return std::vector<Destination>(m_destinations.begin() + static_cast<std::ptrdiff_t>(m_index),
                                    m_destinations.end());
}

bool UserPerformanceWrite::canRetry() const
{
    return !isBusy() && !unwritten().empty()
        && m_session.connectionState() == DeviceSession::ConnectionState::Connected;
}

bool UserPerformanceWrite::retry()
{
    if (!canRetry()) {
        return false;
    }
    if (!m_armed) {
        setState(State::Failed, tr("Writing to USER Performance memory must be armed first."));
        return false;
    }
    // Deliberately not clearing m_snapshots: the destinations written before
    // the failure are still overwritten, and losing their backup because the
    // user pressed Retry would be the worst possible moment to lose it.
    m_destinations = unwritten();
    m_index = 0;
    m_completed = 0;
    m_cancelRequested = false;
    m_restoring = false;
    m_readBack.reset();
    m_armed = false;
    beginNextDestination();
    return true;
}

void UserPerformanceWrite::cancel()
{
    if (!isBusy()) {
        return;
    }
    // Honoured between destinations, never in the middle of one: stopping
    // half-way through a Performance would leave a destination holding a
    // mixture of two.
    m_cancelRequested = true;
    setState(m_state, tr("Stopping after the destination being written."));
}

// ---------------------------------------------------------------------------
// Session callbacks
// ---------------------------------------------------------------------------

void UserPerformanceWrite::onPerformanceFetchChanged()
{
    const auto& fetch = m_session.patchFetch();
    // The session's fetch slot is shared with Patch reads. A Patch fetch leaves
    // `performance` empty, so a reply meant for somebody else is ignored rather
    // than mistaken for this run's.
    if (fetch.kind != DeviceSession::FetchKind::Performance) {
        return;
    }

    if (m_awaiting == Awaiting::Snapshot) {
        if (fetch.state == DeviceSession::PatchFetchState::Completed && fetch.performance) {
            // Keep the *first* snapshot of a destination. A retry re-reads a
            // destination this run has already touched, and if the earlier
            // attempt had written part of it, the second read would capture
            // that half-written state — so restoring would put back something
            // the instrument never held before the run began.
            const auto existing
                = std::find_if(m_snapshots.begin(), m_snapshots.end(),
                               [number = current().userNumber](const Destination& snapshot) {
                                   return snapshot.userNumber == number;
                               });
            if (existing == m_snapshots.end()) {
                m_snapshots.push_back(Destination{current().userNumber, *fetch.performance});
            }
            m_awaiting = Awaiting::Nothing;
            sendCurrent();
        } else if (fetch.state == DeviceSession::PatchFetchState::Failed) {
            fail(tr("Could not read %1 before writing it, so nothing was written to it: %2")
                     .arg(destinationLabel(current().userNumber),
                          QString::fromStdString(fetch.message)));
        }
        return;
    }

    if (m_awaiting == Awaiting::ReadBack) {
        if (fetch.state == DeviceSession::PatchFetchState::Completed && fetch.performance) {
            m_readBack = fetch.performance;
            m_awaiting = Awaiting::Nothing;
            compareReadBack();
        } else if (fetch.state == DeviceSession::PatchFetchState::Failed) {
            fail(tr("%1 was written but could not be read back, so it is unverified: %2")
                     .arg(destinationLabel(current().userNumber),
                          QString::fromStdString(fetch.message)));
        }
    }
}

void UserPerformanceWrite::onBatchFinished(quint64 batchId, bool ok, const QString& error)
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

int UserPerformanceWrite::currentUserNumber() const noexcept
{
    if (!isBusy() || m_index >= m_destinations.size()) {
        return 0;
    }
    return m_destinations[m_index].userNumber;
}

void UserPerformanceWrite::setState(State state, QString message)
{
    m_state = state;
    m_message = std::move(message);
    emit changed();
}

void UserPerformanceWrite::fail(QString message)
{
    finishRun(State::Failed, std::move(message));
}

void UserPerformanceWrite::finishRun(State state, QString message)
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
