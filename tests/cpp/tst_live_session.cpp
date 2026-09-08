// Phase 11 — driving a running order on stage.
//
// Two things are being pinned. That navigation never touches the instrument:
// a musician looking ahead at the next song must not change what is currently
// playing. And that switching is honest about what it can and cannot do —
// every refusal is a named reason available *before* the button is pressed,
// because on stage a failure that only shows up afterwards is a silent bar.

#include "library/SyxImport.h"
#include "services/LiveSession.h"
#include "support/FakeXp60.h"

#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QtTest>

#include <chrono>
#include <memory>

using namespace xp60studio;
using namespace std::chrono_literals;
using library::LibraryDatabase;
using library::LiveTarget;
using library::LiveTargetKind;
using library::Setlist;
using library::SetlistSection;
using library::SetlistSong;
using services::LiveBlock;
using services::LiveSession;
using testsupport::FakeXp60;

namespace {

roland::ByteVector readFixture()
{
    QFile file(QStringLiteral(XP60STUDIO_FIXTURE_DIR "/user-bank-amal.syx"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.readAll();
    return roland::ByteVector(reinterpret_cast<const roland::Byte*>(bytes.constData()),
                              reinterpret_cast<const roland::Byte*>(bytes.constData()) + bytes.size());
}

LiveTarget userPatch(int number, std::string name = {})
{
    return LiveTarget{LiveTargetKind::UserPatchSlot, 0, number, std::move(name)};
}

LiveTarget performance(int number)
{
    return LiveTarget{LiveTargetKind::UserPerformanceSlot, 0, number, {}};
}

SetlistSection section(std::string name, LiveTarget target)
{
    return SetlistSection{std::move(name), std::move(target), {}};
}

// The fixture's user memory *and* something in the temporary area. Both are
// needed here: arming requires a verified temporary read, and the cues point at
// USER slots. `temporaryAreaWith` alone holds no user memory, and
// `fixtureImage` alone holds no temporary Patch.
xpmodel::MemoryImage bankWithTemporaryPatch(int userPatchNumber)
{
    auto image = testsupport::fixtureImage();
    const auto source = *xpmodel::Xp60PatchLayout::userPatchAddress(userPatchNumber);
    const auto temporary = xpmodel::Xp60PatchLayout::temporaryPatchAddress();
    for (const auto& block : xpmodel::Xp60PatchLayout::blocks()) {
        const auto bytes = image.read(*source.plus(block.offset), block.size);
        image.write(*temporary.plus(block.offset), *bytes);
    }
    return image;
}

struct Fixture
{
    protocol::TimePoint now{std::chrono::duration_cast<protocol::Clock::duration>(1000ms)};
    midi::LoopbackMidiTransport* transport = nullptr;
    std::unique_ptr<services::DeviceSession> session;
    std::unique_ptr<services::PatchTransfer> transfer;
    std::unique_ptr<LiveSession> live;
    std::unique_ptr<FakeXp60> device;
    LibraryDatabase db;
    std::vector<std::int64_t> libraryIds;

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
        transfer = std::make_unique<services::PatchTransfer>(*session);
        live = std::make_unique<LiveSession>(*session, *transfer);
        device = std::make_unique<FakeXp60>(bankWithTemporaryPatch(64));
        if (connect) {
            session->connectEndpoints("in-1", "out-1");
        }
    }

    void openLibrary(const roland::ByteVector& fixture)
    {
        QVERIFY2(db.open(QString::fromLatin1(LibraryDatabase::kInMemoryPath)), qPrintable(db.lastError()));
        library::SyxImportOptions options;
        options.sourceName = "user-bank-amal.syx";
        const auto entries = library::importSyxStream(fixture, options).entries;
        const auto ids = db.insertAll(entries);
        QVERIFY(ids.has_value());
        libraryIds = *ids;
        live->setLibrary(&db);
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
            const bool idle = replies.empty() && transport->sentMessages().empty()
                && session->pendingSendCount() == 0 && !transfer->isBusy()
                && session->patchFetch().state
                    != services::DeviceSession::PatchFetchState::InProgress;
            if (idle && !live->switching()) {
                return;
            }
            if (idle) {
                // A queued live update waits on PatchTransfer's 120 ms throttle,
                // which is real time on a real timer. Spinning processEvents
                // never reaches it.
                QTest::qWait(10);
            }
        }
    }

    // Arming needs a verified temporary read first, which is the rule
    // PatchTransfer enforces and Live Mode inherits.
    void armForLive()
    {
        QVERIFY(session->fetchTemporaryPatch());
        pump();
        QVERIFY2(transfer->canArm(), transfer->message().c_str());
        QVERIFY(transfer->arm());
    }
};

Setlist threeCues()
{
    Setlist list;
    list.name = "Friday";
    list.songs = {
        SetlistSong{"Opener", {}, {section("Intro", userPatch(1, "one")), section("Verse", {})}},
        SetlistSong{"Second", {}, {section("Whole", userPatch(9, "nine"))}},
    };
    return list;
}

} // namespace

class TestLiveSession : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_fixture = readFixture();
        QVERIFY2(!m_fixture.empty(), "golden fixture missing");
        QVERIFY2(QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")),
                 "this Qt build has no QSQLITE driver");
    }

    // --- navigation, which must never transmit ------------------------------

    void nothingIsLoadedAndNothingIsPlayableBeforeASetlistArrives()
    {
        Fixture f;
        QVERIFY(!f.live->hasSetlist());
        QCOMPARE(f.live->cueCount(), 0);
        QCOMPARE(f.live->position(), -1);
        QVERIFY(!f.live->currentCue());
        QVERIFY(!f.live->nextCue());
        QVERIFY(!f.live->canGoNext());
        QVERIFY(!f.live->goToCurrent());
        QCOMPARE(f.live->planForCurrent().block, LiveBlock::NoSetlist);
    }

    void loadingASetlistLeavesTheCursorBeforeTheFirstCue()
    {
        Fixture f;
        QSignalSpy moved(f.live.get(), &LiveSession::positionChanged);
        f.live->load(threeCues());

        QCOMPARE(f.live->cueCount(), 3);
        // Loading a setlist is not the downbeat. The instrument keeps whatever
        // it is holding until somebody says go.
        QCOMPARE(f.live->position(), -1);
        QVERIFY(!f.live->currentCue());
        QCOMPARE(f.live->nextCue()->index, 0);
        QCOMPARE(f.live->soundingIndex(), -1);
        QVERIFY(moved.size() >= 1);
        QVERIFY(f.transport->sentMessages().empty());
    }

    void scrollingAheadMovesTheCursorAndSendsNothing()
    {
        Fixture f;
        f.live->load(threeCues());
        QVERIFY(f.live->goNext());
        QVERIFY(f.live->goNext());
        QCOMPARE(f.live->position(), 1);
        QCOMPARE(f.live->currentCue()->sectionName, std::string("Verse"));
        QCOMPARE(f.live->nextCue()->sectionName, std::string("Whole"));
        QVERIFY(f.live->goPrevious());
        QCOMPARE(f.live->position(), 0);

        // The whole point: looking ahead at the next song must not change what
        // is currently playing.
        QVERIFY(f.transport->sentMessages().empty());
        QCOMPARE(f.live->soundingIndex(), -1);
    }

    void theCursorStopsAtBothEndsRatherThanWrapping()
    {
        Fixture f;
        f.live->load(threeCues());
        QVERIFY(!f.live->goPrevious()); // before the first cue
        while (f.live->canGoNext()) {
            QVERIFY(f.live->goNext());
        }
        QCOMPARE(f.live->position(), 2);
        QVERIFY(!f.live->goNext());
        QVERIFY(!f.live->nextCue());
        f.live->rewind();
        QCOMPARE(f.live->position(), -1);
    }

    // --- what a switch would involve, before it is attempted -----------------

    void thePlanSaysWhatACueCostsBeforeAnythingHappens()
    {
        Fixture f;
        f.openLibrary(m_fixture);
        f.armForLive();

        Setlist list;
        list.name = "Friday";
        list.songs = {SetlistSong{
            "Opener", {}, {section("A", LiveTarget{LiveTargetKind::LibraryPatch, f.libraryIds[0], 0, "one"}),
                           section("B", userPatch(9, "nine"))}}};
        f.live->load(std::move(list));

        // A library cue: the bytes are already held, so nothing is read.
        QVERIFY(f.live->goToIndex(0));
        auto plan = f.live->planForCurrent();
        QVERIFY2(plan.possible(), qPrintable(plan.reason));
        QCOMPARE(plan.blocksToRead, 0);
        QCOMPARE(plan.blocksToWrite, 5);
        QVERIFY(plan.headline.contains(QStringLiteral("one")));

        // A USER slot cue: read the slot first, then write it. Worth knowing
        // before a downbeat.
        QVERIFY(f.live->goToIndex(1));
        plan = f.live->planForCurrent();
        QVERIFY(plan.possible());
        QCOMPARE(plan.blocksToRead, 5);
        QCOMPARE(plan.blocksToWrite, 5);
    }

    void aCarryPreviousCueIsSatisfiedByDoingNothingAndSaysSo()
    {
        Fixture f;
        f.armForLive();
        f.live->load(threeCues());
        QVERIFY(f.live->goToIndex(1)); // "Verse", carries the previous sound

        const auto plan = f.live->planForCurrent();
        // Not a failure — the section is written to keep the sound.
        QCOMPARE(plan.block, LiveBlock::NothingToDo);
        QVERIFY(plan.reason.contains(QStringLiteral("keeps the previous sound")));
        QVERIFY(!f.live->goToCurrent());
        // And the display now points at this cue rather than the earlier one.
        QCOMPARE(f.live->soundingIndex(), 1);
        QVERIFY(f.transport->sentMessages().empty());
    }

    void aPerformanceCueIsNavigableButRefusedWithItsOwnReason()
    {
        Fixture f;
        f.armForLive();
        Setlist list;
        list.name = "Friday";
        list.songs = {SetlistSong{"Opener", {}, {section("A", performance(3))}}};
        f.live->load(std::move(list));
        QVERIFY(f.live->goToIndex(0));

        QCOMPARE(f.live->planForCurrent().block, LiveBlock::PerformanceNotSupported);
        QVERIFY(!f.live->goToCurrent());
        QVERIFY(f.live->lastMessage().contains(QStringLiteral("Performance")));
        QVERIFY(f.transport->sentMessages().empty());
    }

    void beingOfflineAndBeingUnarmedAreDifferentAnswers()
    {
        Fixture offline(/*connect=*/false);
        offline.live->load(threeCues());
        QVERIFY(offline.live->goToIndex(0));
        QCOMPARE(offline.live->planForCurrent().block, LiveBlock::NotConnected);
        QVERIFY(!offline.live->goToCurrent());

        Fixture connected;
        connected.live->load(threeCues());
        QVERIFY(connected.live->goToIndex(0));
        const auto plan = connected.live->planForCurrent();
        QCOMPARE(plan.block, LiveBlock::NotArmed);
        QVERIFY(plan.reason.contains(QStringLiteral("Arm once")));
        QVERIFY(!connected.live->goToCurrent());
        QVERIFY(connected.transport->sentMessages().empty());
    }

    void aCueWhosePatchWasDeletedIsRefusedByName()
    {
        Fixture f;
        f.openLibrary(m_fixture);
        f.armForLive();
        Setlist list;
        list.name = "Friday";
        list.songs = {SetlistSong{
            "Opener", {}, {section("A", LiveTarget{LiveTargetKind::LibraryPatch, 0, 0, "GrandPiano"})}}};
        f.live->load(std::move(list));
        QVERIFY(f.live->goToIndex(0));

        const auto plan = f.live->planForCurrent();
        QCOMPARE(plan.block, LiveBlock::PatchMissing);
        QVERIFY(plan.reason.contains(QStringLiteral("GrandPiano")));
        QVERIFY(!f.live->goToCurrent());
    }

    // --- actually switching --------------------------------------------------

    void goingToALibraryCuePutsThosebytesInTheTemporaryArea()
    {
        Fixture f;
        f.openLibrary(m_fixture);
        f.armForLive();

        Setlist list;
        list.name = "Friday";
        list.songs = {SetlistSong{
            "Opener", {},
            {section("A", LiveTarget{LiveTargetKind::LibraryPatch, f.libraryIds[3], 0, "four"})}}};
        f.live->load(std::move(list));
        QVERIFY(f.live->goToIndex(0));

        QSignalSpy finished(f.live.get(), &LiveSession::switchFinished);
        QVERIFY(f.live->goToCurrent());
        f.pump();

        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.at(0).at(0).toInt(), 0);
        QCOMPARE(finished.at(0).at(1).toBool(), true);
        QCOMPARE(f.live->soundingIndex(), 0);
        QVERIFY(!f.live->switching());

        const auto wanted = f.db.loadEntry(f.libraryIds[3]);
        QVERIFY(wanted.has_value());
        const auto onDevice = xpmodel::Xp60PatchCodec::decode(
            f.device->memory(), xpmodel::Xp60PatchLayout::temporaryPatchAddress());
        QVERIFY(onDevice.patch.has_value());
        QVERIFY(*onDevice.patch == wanted->patch());
    }

    void goingToAUserSlotReadsItThenWritesItToTheTemporaryArea()
    {
        Fixture f;
        f.armForLive();
        Setlist list;
        list.name = "Friday";
        list.songs = {SetlistSong{"Opener", {}, {section("A", userPatch(9, "nine"))}}};
        f.live->load(std::move(list));
        QVERIFY(f.live->goToIndex(0));

        QSignalSpy finished(f.live.get(), &LiveSession::switchFinished);
        QVERIFY(f.live->goToCurrent());
        f.pump();

        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.at(0).at(1).toBool(), true);
        QCOMPARE(f.live->soundingIndex(), 0);

        const auto slot = xpmodel::Xp60PatchCodec::decode(
            f.device->memory(), *xpmodel::Xp60PatchLayout::userPatchAddress(9));
        const auto temporary = xpmodel::Xp60PatchCodec::decode(
            f.device->memory(), xpmodel::Xp60PatchLayout::temporaryPatchAddress());
        QVERIFY(slot.patch.has_value());
        QVERIFY(temporary.patch.has_value());
        QVERIFY(*temporary.patch == *slot.patch);
    }

    void advancingRepeatedlyWorksOnOneArmingRatherThanOnePerCue()
    {
        Fixture f;
        f.armForLive();
        Setlist list;
        list.name = "Friday";
        list.songs = {SetlistSong{"Opener", {}, {section("A", userPatch(1, "one")),
                                                 section("B", userPatch(9, "nine")),
                                                 section("C", userPatch(17, "seventeen"))}}};
        f.live->load(std::move(list));

        // Three cues, one arming. Re-arming between every bar is not an armed
        // write, it is an unguarded one with an extra tap.
        for (int expected = 0; expected < 3; ++expected) {
            QVERIFY2(f.live->advance(), qPrintable(f.live->lastMessage()));
            f.pump();
            QCOMPARE(f.live->soundingIndex(), expected);
        }
        QVERIFY(!f.live->advance()); // end of the list

        const auto slot = xpmodel::Xp60PatchCodec::decode(
            f.device->memory(), *xpmodel::Xp60PatchLayout::userPatchAddress(17));
        const auto temporary = xpmodel::Xp60PatchCodec::decode(
            f.device->memory(), xpmodel::Xp60PatchLayout::temporaryPatchAddress());
        QVERIFY(*temporary.patch == *slot.patch);

        // Leaving the session verifies rather than ending on an unproved send.
        f.live->endLiveWrites();
        for (int i = 0; i < 200 && f.transfer->liveActive(); ++i) {
            f.pump();
            QTest::qWait(10);
        }
        QVERIFY(!f.transfer->liveActive());
        QCOMPARE(f.transfer->state(), services::PatchTransfer::State::Verified);
    }

    void aSecondSwitchIsRefusedWhileTheFirstIsStillGoing()
    {
        Fixture f;
        f.armForLive();
        Setlist list;
        list.name = "Friday";
        list.songs = {SetlistSong{"Opener", {}, {section("A", userPatch(1)), section("B", userPatch(9))}}};
        f.live->load(std::move(list));

        QVERIFY(f.live->goToIndex(0));
        QVERIFY(f.live->goToCurrent());
        QVERIFY(f.live->switching());
        // Mid-read. The cursor can still move — that is navigation — but the
        // plan says why nothing else can be sent yet.
        QVERIFY(f.live->goToIndex(1));
        QCOMPARE(f.live->planForCurrent().block, LiveBlock::Busy);
        QVERIFY(!f.live->goToCurrent());
        f.pump();
    }

private:
    roland::ByteVector m_fixture;
};

QTEST_MAIN(TestLiveSession)
#include "tst_live_session.moc"
