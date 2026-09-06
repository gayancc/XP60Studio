#include "services/SnapshotRestore.h"

#include "services/SnapshotStore.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/MemoryImage.h"
#include "xpmodel/SysExStream.h"

#include <QFile>

#include <algorithm>

namespace xp60studio::services {

namespace {

QString areaName(RestoreArea area)
{
    const auto name = restoreAreaName(area);
    return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
}

std::vector<roland::RolandSysExMessage> decodeMessages(
    const std::vector<roland::ByteVector>& raw)
{
    const std::vector<roland::RolandModelId> models{xp60::modelId()};
    std::vector<roland::RolandSysExMessage> out;
    out.reserve(raw.size());
    for (const auto& message : raw) {
        const auto stream = xpmodel::parseSysExStream(message, models);
        for (const auto& item : stream.items) {
            if (item.roland && item.roland->isDataSet()) {
                out.push_back(*item.roland);
            }
        }
    }
    return out;
}

} // namespace

SnapshotRestore::SnapshotRestore(DeviceSession& session, QObject* parent)
    : QObject(parent)
    , m_session(session)
    , m_capture(std::make_unique<SnapshotCapture>(session))
{
    connect(m_capture.get(), &SnapshotCapture::finished, this, [this](bool ok) {
        if (m_verifying) {
            onVerificationFinished(ok);
        } else if (m_state == State::SafetySnapshot) {
            onSafetySnapshotFinished(ok);
        }
    });
    connect(&m_session, &DeviceSession::dataSetBatchFinished, this,
            &SnapshotRestore::onBatchFinished);
    connect(&m_session, &DeviceSession::connectionStateChanged, this, [this] {
        if (m_session.connectionState() == DeviceSession::ConnectionState::Connected) {
            return;
        }
        if (m_armed) {
            m_armed = false;
            emit changed();
        }
        if (isBusy()) {
            finish(State::Failed,
                   m_safetySaved
                       ? tr("Disconnected part-way through restoring. What was written is unverified; "
                            "the safety snapshot at %1 holds what was there before.")
                             .arg(m_safetyPath)
                       : tr("Disconnected before anything was written."));
        }
    });
}

SnapshotRestore::~SnapshotRestore() = default;

bool SnapshotRestore::isBusy() const noexcept
{
    switch (m_state) {
    case State::SafetySnapshot:
    case State::Sending:
    case State::Verifying:
        return true;
    default:
        return false;
    }
}

std::string_view SnapshotRestore::stateName() const noexcept
{
    switch (m_state) {
    case State::Idle:
        return "Idle";
    case State::SafetySnapshot:
        return "SafetySnapshot";
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
    }
    return "Unknown";
}

QString SnapshotRestore::stateLabel() const
{
    switch (m_state) {
    case State::Idle:
        return tr("Idle");
    case State::SafetySnapshot:
        return tr("Backing up what is about to be overwritten");
    case State::Sending:
        return tr("Restoring");
    case State::Verifying:
        return tr("Verifying");
    case State::Completed:
        return tr("Restored and verified");
    case State::Mismatch:
        return tr("The XP-60 did not keep what was restored");
    case State::Failed:
        return tr("Failed");
    case State::Cancelled:
        return tr("Stopped");
    }
    return {};
}

bool SnapshotRestore::canArm() const
{
    return !m_armed && !isBusy()
        && m_session.connectionState() == DeviceSession::ConnectionState::Connected;
}

bool SnapshotRestore::arm()
{
    if (!canArm()) {
        return false;
    }
    m_armed = true;
    emit changed();
    return true;
}

void SnapshotRestore::disarm()
{
    if (!m_armed) {
        return;
    }
    m_armed = false;
    emit changed();
}

QStringList SnapshotRestore::concerns(const RestorePlan& plan, const QString& safetyPath) const
{
    QStringList out = plan.warnings();
    for (const auto& step : plan.steps()) {
        if (!SnapshotCapture::isCapturable(step.area)) {
            out << tr("%1 cannot be backed up by this build, so restoring it could not be undone. "
                      "It will be refused.")
                       .arg(areaName(step.area));
        }
    }
    if (safetyPath.isEmpty()) {
        out << tr("No path was given for the safety snapshot, so the restore will be refused.");
    } else if (QFile::exists(safetyPath)) {
        out << tr("%1 already exists. Overwriting one backup to make another is how both get lost.")
                   .arg(safetyPath);
    }
    return out;
}

bool SnapshotRestore::restore(const RestorePlan& plan, const QString& safetySnapshotPath)
{
    if (isBusy()) {
        return false;
    }
    if (!m_armed) {
        setState(State::Failed, tr("Restoring must be armed first."));
        return false;
    }
    if (m_session.connectionState() != DeviceSession::ConnectionState::Connected) {
        setState(State::Failed, tr("Not connected to an XP-60."));
        return false;
    }
    if (!plan.writesAnything()) {
        setState(State::Failed, tr("This plan writes nothing, so there is nothing to restore."));
        return false;
    }
    if (safetySnapshotPath.isEmpty()) {
        setState(State::Failed,
                 tr("A restore needs somewhere to save the safety snapshot before it overwrites "
                    "anything."));
        return false;
    }
    if (QFile::exists(safetySnapshotPath)) {
        setState(State::Failed,
                 tr("%1 already exists. Overwriting one backup to make another is how both get "
                    "lost.")
                     .arg(safetySnapshotPath));
        return false;
    }

    m_areas.clear();
    for (const auto& step : plan.steps()) {
        if (!SnapshotCapture::isCapturable(step.area)) {
            // Without a safety snapshot there is no undo. Refuse rather than
            // write unprotected.
            setState(State::Failed,
                     tr("%1 cannot be backed up by this build, so restoring it could not be undone. "
                        "Nothing was written.")
                         .arg(areaName(step.area)));
            return false;
        }
        m_areas.push_back(step.area);
    }

    m_plan = plan;
    m_safetyPath = safetySnapshotPath;
    m_safetySaved = false;
    m_verifying = false;
    m_cancelRequested = false;
    m_messagesSent = 0;
    m_mismatches.clear();
    // The arming is spent by this attempt, whatever happens to it.
    m_armed = false;
    beginSafetySnapshot();
    return true;
}

void SnapshotRestore::beginSafetySnapshot()
{
    setState(State::SafetySnapshot,
             tr("Reading what is about to be overwritten, so it can be put back"));
    library::SnapshotMetadata metadata;
    metadata.label = "Safety snapshot";
    metadata.note = "Taken automatically before a restore.";
    if (!m_capture->capture(m_areas, metadata)) {
        finish(State::Failed,
               tr("Could not back up what was about to be overwritten, so nothing was written: %1")
                   .arg(m_capture->message()));
    }
}

void SnapshotRestore::onSafetySnapshotFinished(bool ok)
{
    if (!ok) {
        finish(State::Failed,
               tr("Could not back up what was about to be overwritten, so nothing was written: %1")
                   .arg(m_capture->message()));
        return;
    }
    // Saved to disk, not merely held: a backup that dies with the process is
    // not a backup, and the moment it is needed is usually the moment
    // something has gone wrong.
    const auto saved = SnapshotStore::save(m_capture->snapshot(), m_safetyPath);
    if (!saved.ok) {
        finish(State::Failed,
               tr("Could not save the safety snapshot to %1, so nothing was written: %2")
                   .arg(m_safetyPath, saved.error));
        return;
    }
    m_safetySaved = true;
    if (m_cancelRequested) {
        finish(State::Cancelled, tr("Stopped before anything was written."));
        return;
    }
    beginSending();
}

void SnapshotRestore::beginSending()
{
    const auto messages = decodeMessages(m_plan.messages());
    if (messages.empty()) {
        finish(State::Failed, tr("The plan's messages could not be read back for sending."));
        return;
    }
    setState(State::Sending,
             tr("Restoring %n message(s)", "", static_cast<int>(messages.size())));
    m_batch = m_session.sendDataSets(messages);
    if (!m_batch.isValid()) {
        finish(State::Failed, tr("Could not queue the restore."));
    }
}

void SnapshotRestore::onBatchFinished(quint64 batchId, bool ok, const QString& error)
{
    if (m_state != State::Sending || batchId != m_batch.value) {
        return;
    }
    if (!ok) {
        finish(State::Failed,
               tr("Restoring failed part-way through: %1. The safety snapshot at %2 holds what was "
                  "there before.")
                   .arg(error, m_safetyPath));
        return;
    }
    m_messagesSent = m_plan.messageCount();
    beginVerification();
}

void SnapshotRestore::beginVerification()
{
    m_verifying = true;
    setState(State::Verifying, tr("Reading back what was restored"));
    if (!m_capture->capture(m_areas)) {
        finish(State::Failed,
               tr("What was restored could not be read back, so it is unverified: %1")
                   .arg(m_capture->message()));
    }
}

void SnapshotRestore::onVerificationFinished(bool ok)
{
    m_verifying = false;
    if (!ok) {
        finish(State::Failed,
               tr("What was restored could not be read back, so it is unverified: %1")
                   .arg(m_capture->message()));
        return;
    }
    compareVerification();
}

void SnapshotRestore::compareVerification()
{
    const std::vector<roland::RolandModelId> models{xp60::modelId()};
    const auto actual
        = xpmodel::imageFromStream(xpmodel::parseSysExStream(m_capture->snapshot().toSysEx(), models));

    // Compare only what the plan actually wrote. Anything else on the
    // instrument is none of this run's business, and reporting it would be
    // reporting a difference nobody caused.
    m_mismatches.clear();
    std::size_t comparedBytes = 0;
    for (const auto& intended : decodeMessages(m_plan.messages())) {
        const auto data = intended.data();
        const auto readBack = actual.read(intended.address(), static_cast<std::uint32_t>(data.size()));
        comparedBytes += data.size();
        if (!readBack || !std::equal(data.begin(), data.end(), readBack->begin(), readBack->end())) {
            m_mismatches.push_back(intended.address());
        }
    }

    if (!m_mismatches.empty()) {
        finish(State::Mismatch,
               tr("%n of the restored block(s) read back differently from what was sent, so they "
                  "were not stored. The most likely cause is User Memory Protect being ON "
                  "(UTILITY → Protect on the instrument). Nothing was lost: the safety snapshot at "
                  "%1 holds what was there before.",
                  "", static_cast<int>(m_mismatches.size()))
                   .arg(m_safetyPath));
        return;
    }
    finish(State::Completed,
           tr("Restored %1 message(s) and read every one of them back unchanged (%2 bytes "
              "compared). The previous contents are at %3.")
               .arg(m_messagesSent)
               .arg(comparedBytes)
               .arg(m_safetyPath));
}

void SnapshotRestore::cancel()
{
    if (!isBusy()) {
        return;
    }
    // Only honoured before the send begins. Once DT1s are going out, stopping
    // half-way would leave the instrument holding a mixture of two states —
    // which is the one outcome this whole class exists to avoid. After that
    // point the run is seen through and verified, and the safety snapshot is
    // the way back.
    m_cancelRequested = true;
    if (m_state == State::SafetySnapshot) {
        m_capture->cancel();
        setState(m_state, tr("Stopping before anything is written."));
        return;
    }
    setState(m_state,
             tr("The restore is already under way; it will be finished and verified. Use the safety "
                "snapshot to undo it."));
}

void SnapshotRestore::setState(State state, QString message)
{
    m_state = state;
    m_message = std::move(message);
    emit changed();
}

void SnapshotRestore::finish(State state, QString message)
{
    m_batch = {};
    m_verifying = false;
    setState(state, std::move(message));
    emit finished(state == State::Completed);
}

} // namespace xp60studio::services
