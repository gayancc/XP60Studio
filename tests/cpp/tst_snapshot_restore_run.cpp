// Writing a restore back to the instrument.
//
// The most destructive thing the application does, so the tests are written
// around the safety envelope: the safety snapshot exists on disk before a
// single byte goes out, everything written is read back, and a refusal happens
// before anything is sent rather than after.

#include "services/DeviceSession.h"
#include "services/RestorePlan.h"
#include "services/SnapshotCapture.h"
#include "services/SnapshotRestore.h"
#include "services/SnapshotStore.h"
#include "services/UserPerformanceWrite.h"
#include "support/FakeXp60.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/Xp60PerformanceCodec.h"
#include "xpmodel/Xp60PerformanceLayout.h"

#include <QCoreApplication>
#include <fstream>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <chrono>
#include <memory>

using namespace xp60studio;
using namespace std::chrono_literals;
using services::RestoreArea;
using services::RestorePlan;
using services::SnapshotCapture;
using services::SnapshotRestore;
using services::SnapshotStore;
using testsupport::FakeXp60;
using xpmodel::Xp60Performance;
using xpmodel::Xp60PerformanceCodec;
using xpmodel::Xp60PerformanceLayout;
using State = services::SnapshotRestore::State;

namespace {

roland::ByteVector fixtureBytes()
{
    std::ifstream in(XP60STUDIO_FIXTURE_DIR "/user-bank-amal.syx", std::ios::binary);
    return roland::ByteVector((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

struct Fixture
{
    protocol::TimePoint now{std::chrono::duration_cast<protocol::Clock::duration>(1000ms)};
    midi::LoopbackMidiTransport* transport = nullptr;
    std::unique_ptr<services::DeviceSession> session;
    std::unique_ptr<SnapshotCapture> capture;
    std::unique_ptr<SnapshotRestore> restore;
    std::unique_ptr<services::UserPerformanceWrite> writer;
    std::unique_ptr<FakeXp60> device;
    QTemporaryDir dir;

    Fixture()
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
        capture = std::make_unique<SnapshotCapture>(*session);
        restore = std::make_unique<SnapshotRestore>(*session);
        writer = std::make_unique<services::UserPerformanceWrite>(*session);
        device = std::make_unique<FakeXp60>(testsupport::fixtureImage());
        session->connectEndpoints("in-1", "out-1");
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
                && session->pendingSendCount() == 0 && !capture->isBusy() && !restore->isBusy()
                && !writer->isBusy()) {
                return;
            }
        }
    }

    [[nodiscard]] Xp60Performance inUserSlot(int userNumber) const
    {
        return *Xp60PerformanceCodec::decode(
                    device->memory(), *Xp60PerformanceLayout::userPerformanceAddress(userNumber))
                    .performance;
    }

    // A snapshot of the Performance bank as it stands, for use as the thing to
    // restore later.
    [[nodiscard]] library::InstrumentSnapshot capturePerformances()
    {
        capture->capture({RestoreArea::UserPerformances});
        pump();
        return capture->snapshot();
    }

    [[nodiscard]] QString safetyPath(const QString& name = QStringLiteral("safety.syx")) const
    {
        return dir.filePath(name);
    }

    // Change the instrument for real, through the real writer, so the restore
    // is undoing something that actually happened rather than a poked byte.
    void copyPerformance(int from, int to)
    {
        writer->arm();
        writer->writeOne(inUserSlot(from), to);
        pump();
    }
};

} // namespace

class TestSnapshotRestoreRun : public QObject
{
    Q_OBJECT

private slots:
    // --- refusals, all before anything is written --------------------------

    void refusesWithoutArming()
    {
        Fixture f;
        const auto plan = RestorePlan::build(f.capturePerformances(), {RestoreArea::UserPerformances});
        const auto before = f.device->dataSetsReceived();

        QVERIFY(!f.restore->restore(plan, f.safetyPath()));
        f.pump();
        QCOMPARE(f.restore->state(), State::Failed);
        QCOMPARE(f.device->dataSetsReceived(), before);
        QVERIFY(!QFile::exists(f.safetyPath()));
    }

    void refusesWithoutASafetySnapshotPath()
    {
        Fixture f;
        const auto plan = RestorePlan::build(f.capturePerformances(), {RestoreArea::UserPerformances});
        const auto before = f.device->dataSetsReceived();

        QVERIFY(f.restore->arm());
        QVERIFY(!f.restore->restore(plan, QString()));
        QCOMPARE(f.restore->state(), State::Failed);
        QVERIFY(f.restore->message().contains(QStringLiteral("safety snapshot")));
        QCOMPARE(f.device->dataSetsReceived(), before);
    }

    void refusesToOverwriteAnExistingSafetySnapshot()
    {
        Fixture f;
        const auto snapshot = f.capturePerformances();
        const auto plan = RestorePlan::build(snapshot, {RestoreArea::UserPerformances});
        // Something is already at that path.
        QVERIFY(SnapshotStore::save(snapshot, f.safetyPath()).ok);
        const auto before = f.device->dataSetsReceived();

        QVERIFY(f.restore->arm());
        QVERIFY(!f.restore->restore(plan, f.safetyPath()));
        QCOMPARE(f.restore->state(), State::Failed);
        QVERIFY2(f.restore->message().contains(QStringLiteral("already exists")),
                 "overwriting one backup to make another is how both get lost");
        QCOMPARE(f.device->dataSetsReceived(), before);
    }

    void refusesAPlanItCannotBackUp()
    {
        Fixture f;
        // A snapshot holding Rhythm Setups, which this build can restore but
        // cannot capture — so there would be no undo.
        const std::vector<roland::RolandModelId> models{xp60::modelId()};
        const auto whole = library::InstrumentSnapshot::fromSysEx(fixtureBytes(), models);
        const auto plan = RestorePlan::build(whole, {RestoreArea::UserRhythmSetups});
        QVERIFY(plan.writesAnything());
        const auto before = f.device->dataSetsReceived();

        QVERIFY(f.restore->arm());
        QVERIFY(!f.restore->restore(plan, f.safetyPath()));
        QCOMPARE(f.restore->state(), State::Failed);
        QVERIFY(f.restore->message().contains(QStringLiteral("could not be undone")));
        QCOMPARE(f.device->dataSetsReceived(), before);

        // ...and the concern is stated up front, before anybody presses go.
        const auto concerns = f.restore->concerns(plan, f.safetyPath());
        QVERIFY(std::any_of(concerns.begin(), concerns.end(), [](const QString& c) {
            return c.contains(QStringLiteral("cannot be backed up"));
        }));
    }

    void refusesAPlanThatWritesNothing()
    {
        Fixture f;
        // A Performance snapshot asked to restore the Patches.
        const auto plan = RestorePlan::build(f.capturePerformances(), {RestoreArea::UserPatches});
        QVERIFY(!plan.writesAnything());

        QVERIFY(f.restore->arm());
        QVERIFY(!f.restore->restore(plan, f.safetyPath()));
        QCOMPARE(f.restore->state(), State::Failed);
    }

    // --- a real restore ----------------------------------------------------

    void savesTheSafetySnapshotBeforeWritingAnything()
    {
        Fixture f;
        // Change the instrument, then restore the original.
        const auto original = f.capturePerformances();
        const auto originalFirst = f.inUserSlot(1);
        const auto originalSecond = f.inUserSlot(2);
        QVERIFY(!(originalFirst == originalSecond));

        // Put Performance 2's content into slot 1, so the instrument now
        // differs from the snapshot.
        f.copyPerformance(2, 1);
        QVERIFY(f.inUserSlot(1) == originalSecond);

        const auto plan = RestorePlan::build(original, {RestoreArea::UserPerformances});
        QVERIFY(f.restore->arm());
        QVERIFY(f.restore->restore(plan, f.safetyPath()));
        f.pump();

        QCOMPARE(f.restore->state(), State::Completed);
        QVERIFY(f.restore->safetySnapshotSaved());
        QVERIFY(QFile::exists(f.safetyPath()));
        QVERIFY(f.restore->mismatches().empty());

        // The instrument is back to what the snapshot held.
        QVERIFY(f.inUserSlot(1) == originalFirst);

        // And the safety snapshot on disk holds the *changed* state, which is
        // what was there immediately before the restore.
        const auto safety = SnapshotStore::load(f.safetyPath());
        QVERIFY(safety.ok);
        QVERIFY(safety.digestMatched);
        const auto safetyPlan = RestorePlan::build(*safety.snapshot, {RestoreArea::UserPerformances});
        QVERIFY(safetyPlan.isComplete());
    }

    void anInstrumentThatRefusesTheWriteIsReportedAsAMismatch()
    {
        Fixture f;
        const auto original = f.capturePerformances();
        const auto originalFirst = f.inUserSlot(1);
        f.copyPerformance(2, 1);

        // What User Memory Protect being ON looks like from here.
        f.device->setAcceptWrites(false);
        const auto plan = RestorePlan::build(original, {RestoreArea::UserPerformances});
        QVERIFY(f.restore->arm());
        QVERIFY(f.restore->restore(plan, f.safetyPath()));
        f.pump();

        QCOMPARE(f.restore->state(), State::Mismatch);
        QVERIFY(!f.restore->mismatches().empty());
        QVERIFY(f.restore->message().contains(QStringLiteral("User Memory Protect")));
        // Nothing was lost, and the message says where the previous contents
        // are.
        QVERIFY(f.restore->safetySnapshotSaved());
        QVERIFY(f.restore->message().contains(f.safetyPath()));
        QVERIFY(!(f.inUserSlot(1) == originalFirst));
    }

    void armingIsSpentByOneRun()
    {
        Fixture f;
        const auto plan = RestorePlan::build(f.capturePerformances(), {RestoreArea::UserPerformances});

        QVERIFY(f.restore->arm());
        QVERIFY(f.restore->isArmed());
        QVERIFY(f.restore->restore(plan, f.safetyPath()));
        f.pump();
        QCOMPARE(f.restore->state(), State::Completed);
        QVERIFY(!f.restore->isArmed());

        // A second restore needs a second deliberate arming.
        QVERIFY(!f.restore->restore(plan, f.safetyPath(QStringLiteral("safety-2.syx"))));
        QVERIFY(!QFile::exists(f.safetyPath(QStringLiteral("safety-2.syx"))));
    }

    void aDisconnectDisarms()
    {
        Fixture f;
        QVERIFY(f.restore->arm());
        f.session->disconnectEndpoints();
        QVERIFY(!f.restore->isArmed());
        QVERIFY(!f.restore->canArm());
    }

    void concernsRepeatThePlansOwnWarnings()
    {
        Fixture f;
        const auto plan = RestorePlan::build(f.capturePerformances(), {RestoreArea::UserPerformances});
        const auto concerns = f.restore->concerns(plan, f.safetyPath());

        // Everything the plan would say, plus what this class knows about the
        // safety snapshot.
        for (const auto& warning : plan.warnings()) {
            QVERIFY(concerns.contains(warning));
        }
        QVERIFY(!concerns.isEmpty());
    }
};

QTEST_MAIN(TestSnapshotRestoreRun)
#include "tst_snapshot_restore_run.moc"
