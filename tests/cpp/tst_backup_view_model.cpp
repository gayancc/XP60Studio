// The backup and restore workflow as a screen would drive it.
//
// The services underneath are tested elsewhere; what is tested here is the
// sequencing and the refusals — that a plan cannot outlive the snapshot it
// describes, that a partial capture says so, that arming and restoring cannot
// be reached out of order, and that what QML reads back is true.

#include "presentation/BackupViewModel.h"
#include "services/SnapshotStore.h"
#include "support/FakeXp60.h"

#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QtTest>

#include <chrono>
#include <memory>

using namespace xp60studio;
using namespace std::chrono_literals;
using presentation::BackupViewModel;
using testsupport::FakeXp60;

namespace {

QVariantMap at(const QVariantList& list, int index)
{
    return list.at(index).toMap();
}

struct Fixture
{
    protocol::TimePoint now{std::chrono::duration_cast<protocol::Clock::duration>(1000ms)};
    midi::LoopbackMidiTransport* transport = nullptr;
    std::unique_ptr<services::DeviceSession> session;
    std::unique_ptr<BackupViewModel> backup;
    std::unique_ptr<FakeXp60> device;
    QTemporaryDir dir;

    explicit Fixture(bool connect = true)
    {
        auto loopback = std::make_unique<midi::LoopbackMidiTransport>();
        loopback->addInput("in-1", "XP-60 IN");
        loopback->addOutput("out-1", "XP-60 OUT");
        transport = loopback.get();
        session = std::make_unique<services::DeviceSession>(std::move(loopback));
        session->setAutomaticTimeoutPolling(false);
        session->setClocks([this] { return now; }, {});
        auto pacing = session->pacing();
        pacing.interMessageDelay = 0ms;
        session->setPacing(pacing);
        backup = std::make_unique<BackupViewModel>(*session);
        device = std::make_unique<FakeXp60>(testsupport::fixtureImage());
        if (connect) {
            session->connectEndpoints("in-1", "out-1");
        }
    }

    void pump(int rounds = 60000)
    {
        for (int i = 0; i < rounds; ++i) {
            QCoreApplication::processEvents();
            const auto replies = device->exchange(*transport);
            for (const auto& reply : replies) {
                const auto bytes = reply.encode();
                transport->injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
            }
            QCoreApplication::processEvents();
            if (replies.empty() && transport->sentMessages().empty()
                && session->pendingSendCount() == 0 && !backup->capturing()
                && !backup->restoring()) {
                return;
            }
        }
    }

    // A Performance-bank capture: 32 slots, the smallest whole area this build
    // can read, so the tests stay fast without pretending anything.
    void capturePerformances()
    {
        QVERIFY(backup->startCapture({int(BackupViewModel::UserPerformances)},
                                     QStringLiteral("before the gig"),
                                     QStringLiteral("safety")));
        pump();
    }
};

} // namespace

class TestBackupViewModel : public QObject
{
    Q_OBJECT

private slots:
    // --- what the screen can offer before anything happens ------------------

    void onlyTheAreasThisBuildCanReadAreOfferedAndTheRestAreNamed()
    {
        Fixture f;
        const auto areas = f.backup->capturableAreas();
        QVERIFY(!areas.isEmpty());
        for (int i = 0; i < areas.size(); ++i) {
            const auto row = at(areas, i);
            QVERIFY(!row.value(QStringLiteral("name")).toString().isEmpty());
            QVERIFY(row.value(QStringLiteral("slots")).toInt() > 0);
            QVERIFY(services::SnapshotCapture::isCapturable(
                static_cast<services::RestoreArea>(row.value(QStringLiteral("area")).toInt())));
        }
        // Rhythm Setups and System have no block layout yet. They must be said
        // out loud, not left off a list the user reads as complete.
        const auto note = f.backup->unreadableAreaNote();
        QVERIFY(!note.isEmpty());
        QVERIFY(note.contains(QStringLiteral("Rhythm")));
        QVERIFY(note.contains(QStringLiteral("System")));
    }

    void nothingIsHeldOrPlannedBeforeACapture()
    {
        Fixture f;
        QVERIFY(!f.backup->hasSnapshot());
        QVERIFY(!f.backup->hasPlan());
        QVERIFY(!f.backup->canArmRestore());
        QCOMPARE(f.backup->captureProgress(), -1.0);
        QVERIFY(!f.backup->armRestore());
        QVERIFY(!f.backup->startRestore(f.dir.filePath(QStringLiteral("safety.syx"))));
        QVERIFY(!f.backup->planRestoreEverythingCovered());
        QVERIFY(f.backup->restoreConcerns(f.dir.filePath(QStringLiteral("safety.syx"))).isEmpty());
    }

    void anAreaThisBuildDoesNotKnowIsRefusedRatherThanDropped()
    {
        Fixture f;
        // Silently narrowing a request is how a backup ends up missing the one
        // area the user cared about.
        QVERIFY(!f.backup->startCapture({99}));
        QVERIFY(!f.backup->startCapture({int(BackupViewModel::UserPatches), 99}));
        QVERIFY(!f.backup->startCapture({}));
        QVERIFY(!f.backup->capturing());
    }

    // --- capture ------------------------------------------------------------

    void aCaptureIsHeldWithTheProvenanceTheUserGaveIt()
    {
        Fixture f;
        QSignalSpy finished(f.backup.get(), &BackupViewModel::captureFinished);
        f.capturePerformances();

        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.at(0).at(0).toBool(), true);
        QVERIFY(f.backup->hasSnapshot());
        QVERIFY(!f.backup->capturing());
        QCOMPARE(f.backup->captureState(), QStringLiteral("Completed"));
        QCOMPARE(f.backup->captureProgress(), 1.0);
        QCOMPARE(f.backup->captureCompleted(), f.backup->captureTotal());
        QVERIFY(f.backup->snapshotWarnings().isEmpty());
        QVERIFY(!f.backup->snapshotSaved());
        QVERIFY(f.backup->snapshotSummary().contains(QStringLiteral("messages")));
    }

    void aCancelledCaptureIsKeptButSaysItDidNotFinish()
    {
        Fixture f;
        QVERIFY(f.backup->startCapture({int(BackupViewModel::UserPerformances)}));
        f.backup->cancelCapture();
        f.pump();

        // Kept, because a partial read of 32 slots may still be worth saving —
        // but never presented as the backup that was asked for.
        QCOMPARE(f.backup->captureState(), QStringLiteral("Cancelled"));
        const auto warnings = f.backup->snapshotWarnings();
        QCOMPARE(warnings.size(), 1);
        QVERIFY(warnings.at(0).contains(QStringLiteral("did not finish")));
    }

    // --- files --------------------------------------------------------------

    void savingWritesBothFilesAndRefusesToOverwriteWithoutBeingTold()
    {
        Fixture f;
        f.capturePerformances();
        const auto path = f.dir.filePath(QStringLiteral("bank.syx"));

        QVERIFY(f.backup->saveSnapshot(path));
        QVERIFY(QFile::exists(path));
        QVERIFY(QFile::exists(f.dir.filePath(QStringLiteral("bank.json"))));
        QVERIFY(f.backup->snapshotSaved());
        QCOMPARE(f.backup->snapshotPath(), path);

        // A snapshot is what stands between the user and lost work. Replacing
        // one takes saying so.
        QVERIFY(!f.backup->saveSnapshot(path));
        QVERIFY(!f.backup->snapshotWarnings().isEmpty());
        QVERIFY(f.backup->saveSnapshot(path, /*overwrite=*/true));
    }

    void theFolderListingCarriesTheProvenanceNeededToChooseBetweenBackups()
    {
        Fixture f;
        f.capturePerformances();
        QVERIFY(f.backup->saveSnapshot(f.dir.filePath(QStringLiteral("bank.syx"))));

        f.backup->refreshStoredSnapshots(f.dir.path());
        const auto rows = f.backup->storedSnapshots();
        QCOMPARE(rows.size(), 1);
        const auto row = at(rows, 0);
        QCOMPARE(row.value(QStringLiteral("fileName")).toString(), QStringLiteral("bank.syx"));
        QCOMPARE(row.value(QStringLiteral("label")).toString(), QStringLiteral("before the gig"));
        QCOMPARE(row.value(QStringLiteral("note")).toString(), QStringLiteral("safety"));
        QCOMPARE(row.value(QStringLiteral("digestMatched")).toBool(), true);
        QVERIFY(row.value(QStringLiteral("messages")).toInt() > 0);
        QVERIFY(!row.value(QStringLiteral("capturedAt")).toString().isEmpty());
        QVERIFY(!row.value(QStringLiteral("deviceName")).toString().isEmpty());
    }

    void aPlainSyxWithNoManifestIsNotListedAsASnapshot()
    {
        Fixture f;
        QFile plain(f.dir.filePath(QStringLiteral("someone-elses.syx")));
        QVERIFY(plain.open(QIODevice::WriteOnly));
        plain.write("\xF0\x41\x10\x6A\x12\x00\x00\x00\x00\x00\x00\xF7", 12);
        plain.close();

        f.backup->refreshStoredSnapshots(f.dir.path());
        QVERIFY(f.backup->storedSnapshots().isEmpty());
    }

    void loadingASnapshotReplacesTheHeldOneAndItsPlan()
    {
        Fixture f;
        f.capturePerformances();
        const auto path = f.dir.filePath(QStringLiteral("bank.syx"));
        QVERIFY(f.backup->saveSnapshot(path));
        QVERIFY(f.backup->planRestoreEverythingCovered());
        QVERIFY(f.backup->hasPlan());

        // A plan describes one snapshot. Loading another must not leave a plan
        // built from bytes that are no longer held.
        QVERIFY(f.backup->loadSnapshot(path));
        QVERIFY(!f.backup->hasPlan());
        QVERIFY(f.backup->hasSnapshot());
        QVERIFY(f.backup->snapshotOrigin().contains(QStringLiteral("bank.syx")));
    }

    void loadingSomethingThatIsNotASnapshotFailsWithAReason()
    {
        Fixture f;
        QVERIFY(!f.backup->loadSnapshot(f.dir.filePath(QStringLiteral("missing.syx"))));
        QVERIFY(!f.backup->hasSnapshot());
        QVERIFY(!f.backup->snapshotWarnings().isEmpty());
    }

    // --- plan ---------------------------------------------------------------

    void aPlanReportsCoverageInSlotsAndCanBeReadBeforeAnythingIsSent()
    {
        Fixture f;
        f.capturePerformances();
        QVERIFY(f.backup->planRestore({int(BackupViewModel::UserPerformances)}));

        QVERIFY(f.backup->planWritesAnything());
        QVERIFY(f.backup->planIsComplete());
        const auto steps = f.backup->planSteps();
        QCOMPARE(steps.size(), 1);
        const auto step = at(steps, 0);
        QCOMPARE(step.value(QStringLiteral("expectedEntries")).toInt(), 32);
        QCOMPARE(step.value(QStringLiteral("entriesWithData")).toInt(), 32);
        QCOMPARE(step.value(QStringLiteral("covered")).toBool(), true);
        QCOMPARE(step.value(QStringLiteral("completenessChecked")).toBool(), true);
        QVERIFY(step.value(QStringLiteral("messages")).toInt() > 0);
        QVERIFY(!f.backup->planSummary().isEmpty());
    }

    void planningAnAreaTheSnapshotDoesNotHoldReportsItRatherThanWritingNothingQuietly()
    {
        Fixture f;
        f.capturePerformances();
        QVERIFY(f.backup->planRestore({int(BackupViewModel::UserPatches)}));
        QVERIFY(!f.backup->planWritesAnything());
        QVERIFY(!f.backup->planIsComplete());
        QVERIFY(!f.backup->planWarnings().isEmpty());
    }

    void forgettingTheSnapshotForgetsThePlanToo()
    {
        Fixture f;
        f.capturePerformances();
        QVERIFY(f.backup->planRestoreEverythingCovered());
        f.backup->forgetSnapshot();
        QVERIFY(!f.backup->hasSnapshot());
        QVERIFY(!f.backup->hasPlan());
        QVERIFY(!f.backup->canArmRestore());
    }

    // --- restore ------------------------------------------------------------

    void armingNeedsAPlanAndIsSingleUse()
    {
        Fixture f;
        f.capturePerformances();
        QVERIFY(!f.backup->armRestore()); // no plan yet
        QVERIFY(f.backup->planRestoreEverythingCovered());
        QVERIFY(f.backup->canArmRestore());
        QVERIFY(f.backup->armRestore());
        QVERIFY(f.backup->restoreArmed());
        f.backup->disarmRestore();
        QVERIFY(!f.backup->restoreArmed());
    }

    void concernsAreReadableBeforeConfirmingAndNameTheSafetySnapshot()
    {
        Fixture f;
        f.capturePerformances();
        QVERIFY(f.backup->planRestore({int(BackupViewModel::UserPerformances),
                                       int(BackupViewModel::UserPatches)}));
        const auto concerns = f.backup->restoreConcerns(f.dir.filePath(QStringLiteral("s.syx")));
        QVERIFY(!concerns.isEmpty());
        QVERIFY(std::any_of(concerns.begin(), concerns.end(), [](const QString& c) {
            return c.contains(QStringLiteral("Patch"), Qt::CaseInsensitive);
        }));
    }

    void arestoreTakesASafetySnapshotToDiskBeforeItWritesAnything()
    {
        Fixture f;
        f.capturePerformances();
        QVERIFY(f.backup->planRestoreEverythingCovered());
        QVERIFY(f.backup->armRestore());

        const auto safety = f.dir.filePath(QStringLiteral("safety.syx"));
        QSignalSpy finished(f.backup.get(), &BackupViewModel::restoreFinished);
        QVERIFY(f.backup->startRestore(safety));
        f.pump();

        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.at(0).at(0).toBool(), true);
        QCOMPARE(f.backup->restoreState(), QStringLiteral("Completed"));
        QVERIFY(f.backup->safetySnapshotSaved());
        QCOMPARE(f.backup->safetySnapshotPath(), safety);
        QVERIFY(QFile::exists(safety));
        QVERIFY(f.backup->restoreMismatches().isEmpty());
        // And the safety snapshot is a real, loadable one rather than a file
        // that merely exists.
        const auto reloaded = services::SnapshotStore::load(safety);
        QVERIFY(reloaded.ok);
        QVERIFY(reloaded.snapshot);
        QVERIFY(!reloaded.snapshot->isEmpty());
    }

    void arestoreWithoutASafetyPathIsRefusedBeforeAnyByteGoesOut()
    {
        Fixture f;
        f.capturePerformances();
        QVERIFY(f.backup->planRestoreEverythingCovered());
        QVERIFY(f.backup->armRestore());
        QVERIFY(!f.backup->startRestore(QString()));
        QVERIFY(!f.backup->safetySnapshotSaved());
        QVERIFY(!f.backup->restoring());
    }
};

QTEST_MAIN(TestBackupViewModel)
#include "tst_backup_view_model.moc"
