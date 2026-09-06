// Reading the instrument's user memory into a snapshot.
//
// This is what takes the safety snapshot every restore requires, so it is
// tested around what it refuses and what it preserves rather than around the
// happy path. The simulator holds the golden fixture — a real XP-60's whole
// user memory — so a capture here reads the same addresses it would on the
// instrument and gets the same bytes back.

#include "services/DeviceSession.h"
#include "services/RestorePlan.h"
#include "services/SnapshotCapture.h"
#include "services/SnapshotStore.h"
#include "support/FakeXp60.h"
#include "xpmodel/Xp60PatchLayout.h"
#include "xpmodel/Xp60PerformanceLayout.h"

#include <QCoreApplication>
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
using services::SnapshotStore;
using testsupport::FakeXp60;
using State = services::SnapshotCapture::State;

namespace {

struct Fixture
{
    protocol::TimePoint now{std::chrono::duration_cast<protocol::Clock::duration>(1000ms)};
    midi::LoopbackMidiTransport* transport = nullptr;
    std::unique_ptr<services::DeviceSession> session;
    std::unique_ptr<SnapshotCapture> capture;
    std::unique_ptr<FakeXp60> device;

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
        device = std::make_unique<FakeXp60>(testsupport::fixtureImage());
        session->connectEndpoints("in-1", "out-1");
    }

    void pump(int rounds = 40000)
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
                && session->pendingSendCount() == 0 && !capture->isBusy()) {
                return;
            }
        }
    }
};

} // namespace

class TestSnapshotCapture : public QObject
{
    Q_OBJECT

private slots:
    // --- what it refuses ---------------------------------------------------

    void refusesAnAreaItCannotRead()
    {
        Fixture f;
        // Rhythm Setups and System are transcribed but have no block layout or
        // fetch plan, so they are refused by name. A snapshot silently missing
        // an area the user asked for is exactly the false safety net the
        // restore plan exists to prevent.
        QVERIFY(!SnapshotCapture::isCapturable(RestoreArea::UserRhythmSetups));
        QVERIFY(!SnapshotCapture::isCapturable(RestoreArea::System));
        QVERIFY(SnapshotCapture::isCapturable(RestoreArea::UserPatches));
        QVERIFY(SnapshotCapture::isCapturable(RestoreArea::UserPerformances));

        QVERIFY(!f.capture->capture({RestoreArea::System}));
        QCOMPARE(f.capture->state(), State::Failed);
        QVERIFY(f.capture->message().contains(QStringLiteral("System settings")));
        QVERIFY(f.capture->snapshot().isEmpty());
    }

    void refusesTheWholeRequestWhenOnePartIsUnreadable()
    {
        Fixture f;
        // Not "reads the Performances and skips the System": nothing at all.
        QVERIFY(!f.capture->capture({RestoreArea::UserPerformances, RestoreArea::System}));
        f.pump();
        QCOMPARE(f.capture->state(), State::Failed);
        QVERIFY(f.capture->snapshot().isEmpty());
        QCOMPARE(f.capture->totalSlots(), std::size_t(0));
    }

    void refusesWhenNotConnected()
    {
        Fixture f;
        f.session->disconnectEndpoints();
        QVERIFY(!f.capture->capture({RestoreArea::UserPerformances}));
        QCOMPARE(f.capture->state(), State::Failed);
    }

    void refusesAnEmptyRequest()
    {
        Fixture f;
        QVERIFY(!f.capture->capture({}));
        QVERIFY(f.capture->snapshot().isEmpty());
    }

    // --- what it captures --------------------------------------------------

    void capturesEveryPerformanceAsTheInstrumentSentIt()
    {
        Fixture f;
        QSignalSpy progress(f.capture.get(), &SnapshotCapture::progressed);
        QVERIFY(f.capture->capture({RestoreArea::UserPerformances}));
        QCOMPARE(f.capture->totalSlots(), std::size_t(32));
        f.pump();

        QCOMPARE(f.capture->state(), State::Completed);
        QCOMPARE(f.capture->completedSlots(), std::size_t(32));
        QCOMPARE(progress.count(), 32);

        // Message counts are not asserted: how many DT1s a reply arrives in is
        // the instrument's chunking decision, not a property of the data. What
        // must hold is coverage.
        const auto& snapshot = f.capture->snapshot();
        QVERIFY(!snapshot.isEmpty());
        QVERIFY(snapshot.isClean());

        // The bytes are the instrument's own: every Performance Common the
        // simulator holds is covered.
        for (int n = 1; n <= 32; ++n) {
            const auto base = *xpmodel::Xp60PerformanceLayout::userPerformanceAddress(n);
            QVERIFY2(snapshot.covers(base, 66), qPrintable(QStringLiteral("USER:%1").arg(n)));
        }
        // ...and nothing from the Patch bank came along.
        QVERIFY(!snapshot.covers(*xpmodel::Xp60PatchLayout::userPatchAddress(1), 73));
    }

    void aCapturedSnapshotPlansACompleteRestoreOfWhatItHolds()
    {
        Fixture f;
        QVERIFY(f.capture->capture({RestoreArea::UserPerformances}));
        f.pump();
        QCOMPARE(f.capture->state(), State::Completed);

        // The whole point of capturing: the result is a snapshot a restore can
        // act on, and it is complete for the area it covers.
        const auto plan = RestorePlan::build(f.capture->snapshot(), {RestoreArea::UserPerformances});
        QCOMPARE(plan.steps().size(), std::size_t(1));
        const auto& step = plan.steps().front();
        QCOMPARE(step.expectedEntries, 32);
        QCOMPARE(step.entriesWithData, 32);
        QCOMPARE(step.completeEntries, 32);
        QVERIFY(step.covered);
        QVERIFY(plan.isComplete());
        QVERIFY(plan.requiresSafetySnapshot());

        // Asking that same snapshot to restore the Patches gets nothing, and
        // says so.
        const auto patches = RestorePlan::build(f.capture->snapshot(), {RestoreArea::UserPatches});
        QVERIFY(!patches.writesAnything());
        QCOMPARE(patches.unavailableAreas().size(), std::size_t(1));
    }

    void capturesTwoAreasInTheOrderAskedFor()
    {
        Fixture f;
        QVERIFY(f.capture->capture({RestoreArea::UserPerformances, RestoreArea::UserPatches}));
        QCOMPARE(f.capture->totalSlots(), std::size_t(32 + 128));
        f.pump();

        QCOMPARE(f.capture->state(), State::Completed);
        QCOMPARE(f.capture->completedSlots(), std::size_t(32 + 128));
        const auto& snapshot = f.capture->snapshot();
        QVERIFY(snapshot.isClean());

        const auto plan = RestorePlan::buildForEverythingCovered(snapshot);
        QCOMPARE(plan.steps().size(), std::size_t(2));
        QVERIFY(plan.isComplete());
        // Rhythm Setups were never asked for and are not in the result, so the
        // plan does not offer to restore them.
        for (const auto& step : plan.steps()) {
            QVERIFY(step.area != RestoreArea::UserRhythmSetups);
        }
    }

    void carriesItsMetadataThrough()
    {
        Fixture f;
        library::SnapshotMetadata metadata;
        metadata.label = "Before the live set";
        metadata.deviceName = "XP-60";
        metadata.note = "safety snapshot";
        metadata.capturedAt
            = std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}};

        QVERIFY(f.capture->capture({RestoreArea::UserPerformances}, metadata));
        f.pump();
        QCOMPARE(f.capture->state(), State::Completed);
        QCOMPARE(f.capture->snapshot().metadata().label, std::string("Before the live set"));
        QCOMPARE(f.capture->snapshot().metadata().note, std::string("safety snapshot"));
        QCOMPARE(f.capture->snapshot().metadata().capturedAt, metadata.capturedAt);
    }

    void aCapturedSnapshotSavesAndReloadsUnchanged()
    {
        Fixture f;
        QVERIFY(f.capture->capture({RestoreArea::UserPerformances}));
        f.pump();
        QCOMPARE(f.capture->state(), State::Completed);

        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("safety.syx"));
        const auto saved = SnapshotStore::save(f.capture->snapshot(), path);
        QVERIFY2(saved.ok, qPrintable(saved.error));

        const auto loaded = SnapshotStore::load(path);
        QVERIFY(loaded.ok);
        QVERIFY(loaded.digestMatched);
        QVERIFY(loaded.warnings.isEmpty());
        QCOMPARE(loaded.snapshot->toSysEx(), f.capture->snapshot().toSysEx());
    }

    // --- what it does when things go wrong ---------------------------------

    void aFailedReadKeepsWhatArrivedAndSaysItIsIncomplete()
    {
        Fixture f;
        // Answer enough requests for a few whole Performances, then stop. A
        // Performance is seventeen blocks, so 17 * 3 answers three of them.
        f.device->answerOnlyNextRequests(17 * 3);

        QVERIFY(f.capture->capture({RestoreArea::UserPerformances}));
        f.pump(4000);
        f.now += std::chrono::duration_cast<protocol::Clock::duration>(5000ms);
        f.session->pollTimeouts();
        f.pump(4000);

        QVERIFY(f.capture->state() != State::Completed);
        QCOMPARE(f.capture->completedSlots(), std::size_t(3));
        // What arrived is kept: a partial capture is a true statement about the
        // slots it holds.
        QVERIFY(!f.capture->snapshot().isEmpty());
        for (int n = 1; n <= 3; ++n) {
            QVERIFY(f.capture->snapshot().covers(
                *xpmodel::Xp60PerformanceLayout::userPerformanceAddress(n), 66));
        }
        QVERIFY(!f.capture->snapshot().covers(
            *xpmodel::Xp60PerformanceLayout::userPerformanceAddress(4), 66));
        QVERIFY(f.capture->message().contains(QStringLiteral("not a complete backup")));

        // And the restore plan reports it as partial rather than complete.
        const auto plan = RestorePlan::build(f.capture->snapshot(), {RestoreArea::UserPerformances});
        QVERIFY(!plan.isComplete());
        QCOMPARE(plan.steps().front().entriesWithData, 3);
    }

    void aDisconnectMidCaptureKeepsWhatArrived()
    {
        Fixture f;
        QVERIFY(f.capture->capture({RestoreArea::UserPatches}));
        f.pump(200);
        const auto readSoFar = f.capture->completedSlots();
        f.session->disconnectEndpoints();

        QCOMPARE(f.capture->state(), State::Failed);
        QVERIFY(!f.capture->isBusy());
        QVERIFY(readSoFar > 0);
        QVERIFY(!f.capture->snapshot().isEmpty());
        for (std::size_t n = 1; n <= readSoFar; ++n) {
            QVERIFY(f.capture->snapshot().covers(
                *xpmodel::Xp60PatchLayout::userPatchAddress(static_cast<int>(n)), 73));
        }
        QVERIFY(f.capture->message().contains(QStringLiteral("not a complete backup")));
    }

    void nothingItDoesCanWriteToTheInstrument()
    {
        Fixture f;
        QVERIFY(f.capture->capture({RestoreArea::UserPerformances}));
        f.pump();
        QCOMPARE(f.capture->state(), State::Completed);
        // The capture path issues RQ1 only; RQ1 cannot modify device memory.
        QCOMPARE(f.device->dataSetsReceived(), std::size_t{0});
    }
};

QTEST_MAIN(TestSnapshotCapture)
#include "tst_snapshot_capture.moc"
