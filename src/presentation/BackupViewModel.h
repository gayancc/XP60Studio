#pragma once

#include "services/RestorePlan.h"
#include "services/SnapshotCapture.h"
#include "services/SnapshotRestore.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <memory>
#include <optional>

namespace xp60studio::presentation {

// Backup and restore of the XP-60's user memory, as a screen needs to see it.
//
// The engine underneath — `SnapshotCapture`, `SnapshotStore`, `RestorePlan`,
// `SnapshotRestore` — was finished and tested before anything could reach it.
// This is the layer that makes it reachable: it holds the workflow's state
// between steps, converts Roland types into things QML can display, and refuses
// the transitions that must not happen. It adds no rules of its own beyond
// sequencing; every safety decision still belongs to the service that owns it.
//
// ── The workflow it sequences ───────────────────────────────────────────────
//
//   capture → save → (later) load → plan → arm → restore
//
// Each arrow is a separate deliberate act, and the view model will not skip
// one. In particular a plan cannot be built from a capture that is still
// running, and a restore cannot start without a plan the user has seen.
//
// ── Why the capture is kept after saving ────────────────────────────────────
//
// A capture that has been written to disk stays loaded, because the most
// common thing to do next is restore part of it to a different instrument or
// check what it holds. Clearing it on save would make the user reload the file
// they just wrote.
//
// ── What it deliberately does not do ────────────────────────────────────────
//
// It chooses no file paths. A snapshot is the thing standing between a musician
// and lost work, so where it goes is their decision and arrives from a file
// dialog; the view model only refuses to overwrite silently. It also never
// auto-arms and never re-arms after a run: arming is single-use in
// `SnapshotRestore` and stays that way here.
class BackupViewModel : public QObject
{
    Q_OBJECT

    // ── Capture ─────────────────────────────────────────────────────────────
    Q_PROPERTY(bool capturing READ capturing NOTIFY captureChanged)
    Q_PROPERTY(QString captureState READ captureState NOTIFY captureChanged)
    Q_PROPERTY(QString captureMessage READ captureMessage NOTIFY captureChanged)
    Q_PROPERTY(int captureCompleted READ captureCompleted NOTIFY captureProgressed)
    Q_PROPERTY(int captureTotal READ captureTotal NOTIFY captureProgressed)
    Q_PROPERTY(double captureProgress READ captureProgress NOTIFY captureProgressed)
    Q_PROPERTY(QString captureSlotLabel READ captureSlotLabel NOTIFY captureProgressed)
    // One entry per area this build can read: {area, name, slots}. Areas that
    // cannot be read are absent rather than shown disabled — see
    // `unreadableAreaNote`.
    Q_PROPERTY(QVariantList capturableAreas READ capturableAreas CONSTANT)
    Q_PROPERTY(QString unreadableAreaNote READ unreadableAreaNote CONSTANT)

    // ── The snapshot currently held, from a capture or a file ───────────────
    Q_PROPERTY(bool hasSnapshot READ hasSnapshot NOTIFY snapshotChanged)
    Q_PROPERTY(QString snapshotSummary READ snapshotSummary NOTIFY snapshotChanged)
    Q_PROPERTY(QString snapshotOrigin READ snapshotOrigin NOTIFY snapshotChanged)
    Q_PROPERTY(QStringList snapshotWarnings READ snapshotWarnings NOTIFY snapshotChanged)
    Q_PROPERTY(bool snapshotSaved READ snapshotSaved NOTIFY snapshotChanged)
    Q_PROPERTY(QString snapshotPath READ snapshotPath NOTIFY snapshotChanged)

    // ── Snapshots on disk ───────────────────────────────────────────────────
    // {path, fileName, label, capturedAt, deviceName, note, messages, bytes,
    //  digestMatched, warnings}
    Q_PROPERTY(QVariantList storedSnapshots READ storedSnapshots NOTIFY storedSnapshotsChanged)
    Q_PROPERTY(QString snapshotFolder READ snapshotFolder NOTIFY storedSnapshotsChanged)

    // ── The plan ────────────────────────────────────────────────────────────
    Q_PROPERTY(bool hasPlan READ hasPlan NOTIFY planChanged)
    Q_PROPERTY(QString planSummary READ planSummary NOTIFY planChanged)
    Q_PROPERTY(QStringList planWarnings READ planWarnings NOTIFY planChanged)
    // {area, name, entriesWithData, expectedEntries, completeEntries,
    //  completenessChecked, covered, messages, description}
    Q_PROPERTY(QVariantList planSteps READ planSteps NOTIFY planChanged)
    Q_PROPERTY(bool planWritesAnything READ planWritesAnything NOTIFY planChanged)
    Q_PROPERTY(bool planIsComplete READ planIsComplete NOTIFY planChanged)

    // ── Restore ─────────────────────────────────────────────────────────────
    Q_PROPERTY(QString restoreState READ restoreState NOTIFY restoreChanged)
    Q_PROPERTY(QString restoreStateLabel READ restoreStateLabel NOTIFY restoreChanged)
    Q_PROPERTY(QString restoreMessage READ restoreMessage NOTIFY restoreChanged)
    Q_PROPERTY(bool restoring READ restoring NOTIFY restoreChanged)
    Q_PROPERTY(bool restoreArmed READ restoreArmed NOTIFY restoreChanged)
    Q_PROPERTY(bool canArmRestore READ canArmRestore NOTIFY restoreChanged)
    Q_PROPERTY(bool safetySnapshotSaved READ safetySnapshotSaved NOTIFY restoreChanged)
    Q_PROPERTY(QString safetySnapshotPath READ safetySnapshotPath NOTIFY restoreChanged)
    Q_PROPERTY(QStringList restoreMismatches READ restoreMismatches NOTIFY restoreChanged)

public:
    // Mirrors services::RestoreArea for QML, in the same order. Kept as a
    // separate enum because the service type is not a QObject and should not
    // grow Qt metatype machinery to be displayable.
    enum Area {
        UserPatches = 0,
        UserPerformances,
        UserRhythmSetups,
        System,
    };
    Q_ENUM(Area)

    explicit BackupViewModel(services::DeviceSession& session, QObject* parent = nullptr);
    ~BackupViewModel() override;

    [[nodiscard]] bool capturing() const;
    [[nodiscard]] QString captureState() const;
    [[nodiscard]] QString captureMessage() const;
    [[nodiscard]] int captureCompleted() const;
    [[nodiscard]] int captureTotal() const;
    [[nodiscard]] double captureProgress() const;
    [[nodiscard]] QString captureSlotLabel() const;
    [[nodiscard]] QVariantList capturableAreas() const;
    [[nodiscard]] QString unreadableAreaNote() const;

    [[nodiscard]] bool hasSnapshot() const;
    [[nodiscard]] QString snapshotSummary() const;
    [[nodiscard]] QString snapshotOrigin() const { return m_snapshotOrigin; }
    [[nodiscard]] QStringList snapshotWarnings() const { return m_snapshotWarnings; }
    [[nodiscard]] bool snapshotSaved() const { return !m_snapshotPath.isEmpty(); }
    [[nodiscard]] QString snapshotPath() const { return m_snapshotPath; }

    [[nodiscard]] QVariantList storedSnapshots() const { return m_stored; }
    [[nodiscard]] QString snapshotFolder() const { return m_folder; }

    [[nodiscard]] bool hasPlan() const { return m_plan.has_value(); }
    [[nodiscard]] QString planSummary() const;
    [[nodiscard]] QStringList planWarnings() const;
    [[nodiscard]] QVariantList planSteps() const;
    [[nodiscard]] bool planWritesAnything() const;
    [[nodiscard]] bool planIsComplete() const;

    [[nodiscard]] QString restoreState() const;
    [[nodiscard]] QString restoreStateLabel() const;
    [[nodiscard]] QString restoreMessage() const;
    [[nodiscard]] bool restoring() const;
    [[nodiscard]] bool restoreArmed() const;
    [[nodiscard]] bool canArmRestore() const;
    [[nodiscard]] bool safetySnapshotSaved() const;
    [[nodiscard]] QString safetySnapshotPath() const;
    [[nodiscard]] QStringList restoreMismatches() const;

    // ── Capture ─────────────────────────────────────────────────────────────
    // `areas` holds Area values. Returns false when refused — nothing selected,
    // an area this build cannot read, not connected, or already busy — and
    // `captureMessage` says which.
    Q_INVOKABLE bool startCapture(const QVariantList& areas, const QString& label = {},
                                  const QString& note = {});
    Q_INVOKABLE void cancelCapture();

    // ── Files ───────────────────────────────────────────────────────────────
    // Writes the held snapshot to `path` (a local path or a file: URL).
    // Refuses to overwrite unless told; the refusal is reported, not thrown.
    Q_INVOKABLE bool saveSnapshot(const QString& path, bool overwrite = false);
    // Re-reads the snapshot folder. Also sets it, when `directory` is given.
    Q_INVOKABLE void refreshStoredSnapshots(const QString& directory = {});
    Q_INVOKABLE bool loadSnapshot(const QString& path);
    // Drops the held snapshot and any plan built from it. Files are untouched.
    Q_INVOKABLE void forgetSnapshot();

    // ── Plan ────────────────────────────────────────────────────────────────
    Q_INVOKABLE bool planRestore(const QVariantList& areas);
    Q_INVOKABLE bool planRestoreEverythingCovered();
    Q_INVOKABLE void clearPlan();
    // What the user must read before confirming, including what the safety
    // snapshot at `safetyPath` cannot protect.
    Q_INVOKABLE QStringList restoreConcerns(const QString& safetyPath) const;

    // ── Restore ─────────────────────────────────────────────────────────────
    Q_INVOKABLE bool armRestore();
    Q_INVOKABLE void disarmRestore();
    // Runs the built plan. `safetyPath` is where what is about to be
    // overwritten is written first; it is required and must not already exist.
    Q_INVOKABLE bool startRestore(const QString& safetyPath);
    Q_INVOKABLE void cancelRestore();

Q_SIGNALS:
    void captureChanged();
    void captureProgressed();
    void snapshotChanged();
    void storedSnapshotsChanged();
    void planChanged();
    void restoreChanged();
    // A capture run ended. `ok` is false for failed and cancelled alike; the
    // partial snapshot is still held and still describes itself as partial.
    void captureFinished(bool ok);
    void restoreFinished(bool ok);

private:
    void onCaptureFinished(bool ok);
    void adoptSnapshot(library::InstrumentSnapshot snapshot, const QString& origin,
                       const QString& path, const QStringList& warnings);
    [[nodiscard]] static std::optional<std::vector<services::RestoreArea>> toAreas(
        const QVariantList& values);
    [[nodiscard]] static QString localPath(const QString& pathOrUrl);

    services::DeviceSession& m_session;
    std::unique_ptr<services::SnapshotCapture> m_capture;
    std::unique_ptr<services::SnapshotRestore> m_restore;

    std::optional<library::InstrumentSnapshot> m_snapshot;
    QString m_snapshotOrigin;
    QString m_snapshotPath;
    QStringList m_snapshotWarnings;

    std::optional<services::RestorePlan> m_plan;

    QString m_folder;
    QVariantList m_stored;
};

} // namespace xp60studio::presentation
