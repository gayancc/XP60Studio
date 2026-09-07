#include "presentation/BackupViewModel.h"

#include "services/SnapshotStore.h"

#include <QDateTime>
#include <QFileInfo>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <chrono>

namespace xp60studio::presentation {

using services::RestoreArea;

namespace {

QString text(std::string_view value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString areaName(RestoreArea area)
{
    return text(services::restoreAreaName(area));
}

QString isoTime(std::chrono::system_clock::time_point when)
{
    if (when == std::chrono::system_clock::time_point{}) {
        return {};
    }
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(when.time_since_epoch());
    return QDateTime::fromMSecsSinceEpoch(ms.count(), Qt::UTC).toString(Qt::ISODate);
}

} // namespace

BackupViewModel::BackupViewModel(services::DeviceSession& session, QObject* parent)
    : QObject(parent)
    , m_session(session)
    , m_capture(std::make_unique<services::SnapshotCapture>(session))
    , m_restore(std::make_unique<services::SnapshotRestore>(session))
{
    connect(m_capture.get(), &services::SnapshotCapture::changed, this,
            [this] { emit captureChanged(); });
    connect(m_capture.get(), &services::SnapshotCapture::progressed, this,
            [this](std::size_t, std::size_t) { emit captureProgressed(); });
    connect(m_capture.get(), &services::SnapshotCapture::finished, this,
            &BackupViewModel::onCaptureFinished);
    connect(m_restore.get(), &services::SnapshotRestore::changed, this,
            [this] { emit restoreChanged(); });
    connect(m_restore.get(), &services::SnapshotRestore::finished, this, [this](bool ok) {
        emit restoreChanged();
        emit restoreFinished(ok);
    });
}

BackupViewModel::~BackupViewModel() = default;

// ---------------------------------------------------------------------------
// Capture
// ---------------------------------------------------------------------------

bool BackupViewModel::capturing() const { return m_capture->isBusy(); }
QString BackupViewModel::captureState() const { return text(m_capture->stateName()); }
QString BackupViewModel::captureMessage() const { return m_capture->message(); }
int BackupViewModel::captureCompleted() const { return static_cast<int>(m_capture->completedSlots()); }
int BackupViewModel::captureTotal() const { return static_cast<int>(m_capture->totalSlots()); }
QString BackupViewModel::captureSlotLabel() const { return m_capture->currentSlotLabel(); }

double BackupViewModel::captureProgress() const
{
    const auto total = m_capture->totalSlots();
    // No run, no progress. Reporting 0% for "nothing started" and for "started
    // and got nowhere" would be the same bar in two different situations.
    return total == 0 ? -1.0 : static_cast<double>(m_capture->completedSlots()) / double(total);
}

QVariantList BackupViewModel::capturableAreas() const
{
    QVariantList list;
    for (const auto area : services::SnapshotCapture::capturableAreas()) {
        list.append(QVariantMap{{QStringLiteral("area"), static_cast<int>(area)},
                                {QStringLiteral("name"), areaName(area)},
                                {QStringLiteral("slots"),
                                 services::SnapshotCapture::slotCount(area)}});
    }
    return list;
}

QString BackupViewModel::unreadableAreaNote() const
{
    QStringList missing;
    for (const auto area : {RestoreArea::UserPatches, RestoreArea::UserPerformances,
                            RestoreArea::UserRhythmSetups, RestoreArea::System}) {
        if (!services::SnapshotCapture::isCapturable(area)) {
            missing.append(areaName(area));
        }
    }
    if (missing.isEmpty()) {
        return {};
    }
    // Named rather than hidden. A backup that quietly omits an area is the
    // false safety net the whole workflow exists to prevent.
    return tr("Not backed up by this build: %1. Their parameter tables are "
              "transcribed but there is no block layout to read them slot by slot, "
              "so a snapshot cannot claim to hold them.")
        .arg(missing.join(QStringLiteral(", ")));
}

bool BackupViewModel::startCapture(const QVariantList& areas, const QString& label,
                                   const QString& note)
{
    const auto selected = toAreas(areas);
    if (!selected || selected->empty()) {
        return false;
    }
    library::SnapshotMetadata metadata;
    metadata.label = label.toStdString();
    metadata.note = note.toStdString();
    // The port it was actually read from, not a device model this application
    // has not asked the instrument to confirm.
    if (const auto output = m_session.connectedOutput()) {
        metadata.deviceName = output->displayName;
    }
    metadata.capturedAt = std::chrono::system_clock::now();
    const bool started = m_capture->capture(*selected, std::move(metadata));
    emit captureChanged();
    emit captureProgressed();
    return started;
}

void BackupViewModel::cancelCapture() { m_capture->cancel(); }

void BackupViewModel::onCaptureFinished(bool ok)
{
    QStringList warnings;
    const auto& snapshot = m_capture->snapshot();
    if (!ok) {
        // A run that stopped early still holds what arrived. That is a true
        // partial capture, and it is kept — but it must never be mistaken for
        // a backup of everything that was asked for.
        warnings.append(tr("This capture did not finish (%1). It holds only what "
                           "arrived before it stopped.")
                            .arg(m_capture->message().isEmpty() ? captureState()
                                                                : m_capture->message()));
    }
    if (!snapshot.isClean()) {
        warnings.append(tr("%1 message(s) and %2 stray byte(s) could not be read.")
                            .arg(snapshot.rejectedMessages())
                            .arg(snapshot.strayBytes()));
    }
    adoptSnapshot(snapshot, tr("Read from the instrument"), QString(), warnings);
    emit captureChanged();
    emit captureProgressed();
    emit captureFinished(ok);
}

// ---------------------------------------------------------------------------
// The held snapshot
// ---------------------------------------------------------------------------

bool BackupViewModel::hasSnapshot() const { return m_snapshot && !m_snapshot->isEmpty(); }

QString BackupViewModel::snapshotSummary() const
{
    return m_snapshot ? text(m_snapshot->summary()) : QString();
}

void BackupViewModel::adoptSnapshot(library::InstrumentSnapshot snapshot, const QString& origin,
                                    const QString& path, const QStringList& warnings)
{
    m_snapshot = std::move(snapshot);
    m_snapshotOrigin = origin;
    m_snapshotPath = path;
    m_snapshotWarnings = warnings;
    // A plan describes one snapshot. Keeping the old plan alive beside a new
    // snapshot is how the wrong bytes get written.
    clearPlan();
    emit snapshotChanged();
}

void BackupViewModel::forgetSnapshot()
{
    m_snapshot.reset();
    m_snapshotOrigin.clear();
    m_snapshotPath.clear();
    m_snapshotWarnings.clear();
    clearPlan();
    emit snapshotChanged();
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

QString BackupViewModel::localPath(const QString& pathOrUrl)
{
    if (!pathOrUrl.startsWith(QStringLiteral("file:"))) {
        return pathOrUrl;
    }
    return QUrl(pathOrUrl).toLocalFile();
}

bool BackupViewModel::saveSnapshot(const QString& path, bool overwrite)
{
    if (!hasSnapshot()) {
        return false;
    }
    const auto result = services::SnapshotStore::save(*m_snapshot, localPath(path), overwrite);
    if (!result.ok) {
        m_snapshotWarnings = QStringList{result.error};
        emit snapshotChanged();
        return false;
    }
    m_snapshotPath = result.sysExPath;
    emit snapshotChanged();
    if (!m_folder.isEmpty() && QFileInfo(result.sysExPath).absolutePath() == m_folder) {
        refreshStoredSnapshots();
    }
    return true;
}

void BackupViewModel::refreshStoredSnapshots(const QString& directory)
{
    if (!directory.isEmpty()) {
        m_folder = localPath(directory);
    }
    m_stored.clear();
    if (!m_folder.isEmpty()) {
        for (const auto& path : services::SnapshotStore::list(m_folder)) {
            const auto loaded = services::SnapshotStore::load(path);
            QVariantMap row{
                {QStringLiteral("path"), path},
                {QStringLiteral("fileName"), QFileInfo(path).fileName()},
                {QStringLiteral("digestMatched"), loaded.digestMatched},
                {QStringLiteral("warnings"), loaded.warnings},
            };
            if (loaded.snapshot) {
                const auto& metadata = loaded.snapshot->metadata();
                row.insert(QStringLiteral("label"), text(metadata.label));
                row.insert(QStringLiteral("deviceName"), text(metadata.deviceName));
                row.insert(QStringLiteral("note"), text(metadata.note));
                row.insert(QStringLiteral("capturedAt"), isoTime(metadata.capturedAt));
                row.insert(QStringLiteral("messages"),
                           static_cast<int>(loaded.snapshot->messageCount()));
                row.insert(QStringLiteral("bytes"), static_cast<int>(loaded.snapshot->byteCount()));
                row.insert(QStringLiteral("summary"), text(loaded.snapshot->summary()));
            }
            m_stored.append(row);
        }
    }
    emit storedSnapshotsChanged();
}

bool BackupViewModel::loadSnapshot(const QString& path)
{
    const auto result = services::SnapshotStore::load(localPath(path));
    if (!result.ok || !result.snapshot) {
        m_snapshotWarnings = result.warnings.isEmpty() ? QStringList{result.error} : result.warnings;
        emit snapshotChanged();
        return false;
    }
    adoptSnapshot(*result.snapshot, tr("Loaded from %1").arg(QFileInfo(localPath(path)).fileName()),
                  localPath(path), result.warnings);
    return true;
}

// ---------------------------------------------------------------------------
// Plan
// ---------------------------------------------------------------------------

std::optional<std::vector<RestoreArea>> BackupViewModel::toAreas(const QVariantList& values)
{
    std::vector<RestoreArea> areas;
    for (const auto& value : values) {
        bool ok = false;
        const int raw = value.toInt(&ok);
        if (!ok || raw < static_cast<int>(RestoreArea::UserPatches)
            || raw > static_cast<int>(RestoreArea::System)) {
            // An unrecognised area is refused outright rather than dropped: a
            // request to back up or restore something this build does not know
            // must not silently become a smaller request.
            return std::nullopt;
        }
        const auto area = static_cast<RestoreArea>(raw);
        if (std::find(areas.begin(), areas.end(), area) == areas.end()) {
            areas.push_back(area);
        }
    }
    return areas;
}

bool BackupViewModel::planRestore(const QVariantList& areas)
{
    const auto selected = toAreas(areas);
    if (!hasSnapshot() || !selected || selected->empty()) {
        return false;
    }
    m_plan = services::RestorePlan::build(*m_snapshot, *selected);
    emit planChanged();
    return true;
}

bool BackupViewModel::planRestoreEverythingCovered()
{
    if (!hasSnapshot()) {
        return false;
    }
    m_plan = services::RestorePlan::buildForEverythingCovered(*m_snapshot);
    emit planChanged();
    return true;
}

void BackupViewModel::clearPlan()
{
    if (!m_plan) {
        return;
    }
    m_plan.reset();
    emit planChanged();
}

QString BackupViewModel::planSummary() const { return m_plan ? m_plan->summary() : QString(); }

QStringList BackupViewModel::planWarnings() const
{
    return m_plan ? m_plan->warnings() : QStringList();
}

bool BackupViewModel::planWritesAnything() const { return m_plan && m_plan->writesAnything(); }
bool BackupViewModel::planIsComplete() const { return m_plan && m_plan->isComplete(); }

QVariantList BackupViewModel::planSteps() const
{
    QVariantList list;
    if (!m_plan) {
        return list;
    }
    for (const auto& step : m_plan->steps()) {
        list.append(QVariantMap{
            {QStringLiteral("area"), static_cast<int>(step.area)},
            {QStringLiteral("name"), areaName(step.area)},
            {QStringLiteral("entriesWithData"), step.entriesWithData},
            {QStringLiteral("expectedEntries"), step.expectedEntries},
            {QStringLiteral("completeEntries"), step.completeEntries},
            {QStringLiteral("completenessChecked"), step.completenessChecked()},
            {QStringLiteral("covered"), step.covered},
            {QStringLiteral("messages"), static_cast<int>(step.messages.size())},
            {QStringLiteral("description"), step.describe()},
        });
    }
    return list;
}

QStringList BackupViewModel::restoreConcerns(const QString& safetyPath) const
{
    if (!m_plan) {
        return {};
    }
    return m_restore->concerns(*m_plan, localPath(safetyPath));
}

// ---------------------------------------------------------------------------
// Restore
// ---------------------------------------------------------------------------

QString BackupViewModel::restoreState() const { return text(m_restore->stateName()); }
QString BackupViewModel::restoreStateLabel() const { return m_restore->stateLabel(); }
QString BackupViewModel::restoreMessage() const { return m_restore->message(); }
bool BackupViewModel::restoring() const { return m_restore->isBusy(); }
bool BackupViewModel::restoreArmed() const { return m_restore->isArmed(); }
bool BackupViewModel::canArmRestore() const { return hasPlan() && m_restore->canArm(); }
bool BackupViewModel::safetySnapshotSaved() const { return m_restore->safetySnapshotSaved(); }
QString BackupViewModel::safetySnapshotPath() const { return m_restore->safetySnapshotPath(); }

QStringList BackupViewModel::restoreMismatches() const
{
    QStringList list;
    for (const auto& address : m_restore->mismatches()) {
        list.append(QString::fromStdString(address.toHexString()));
    }
    return list;
}

bool BackupViewModel::armRestore()
{
    // Arming without a plan would arm for nothing in particular, and arming is
    // single-use: it would then have to be spent to get rid of.
    if (!hasPlan()) {
        return false;
    }
    const bool armed = m_restore->arm();
    emit restoreChanged();
    return armed;
}

void BackupViewModel::disarmRestore()
{
    m_restore->disarm();
    emit restoreChanged();
}

bool BackupViewModel::startRestore(const QString& safetyPath)
{
    if (!m_plan) {
        return false;
    }
    const bool started = m_restore->restore(*m_plan, localPath(safetyPath));
    emit restoreChanged();
    return started;
}

void BackupViewModel::cancelRestore() { m_restore->cancel(); }

} // namespace xp60studio::presentation
