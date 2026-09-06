// Writing Performances into the XP-60's permanent USER memory.
//
// The Performance editor's Send writes the temporary Performance at
// `01 00 00 00` and nothing else. This is the other half: USER:01–32 at
// `10 nn 00 00`, with the same safety envelope the Patch writer has —
// read-before-write, verify-by-read-back, and snapshots as the undo.
//
// The simulator holds the golden fixture, which is a dump of a real XP-60's
// whole user memory and therefore already contains 32 real Performances at
// their real addresses. A write here goes where it would on the instrument, and
// a read-back reads the same bytes back.

#include "services/DeviceSession.h"
#include "services/UserMemoryWrite.h"
#include "services/UserPerformanceWrite.h"
#include "support/FakeXp60.h"
#include "xpmodel/Xp60PerformanceCodec.h"
#include "xpmodel/Xp60PerformanceLayout.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QtTest>

#include <chrono>
#include <memory>

using namespace xp60studio;
using namespace std::chrono_literals;
using services::UserPerformanceWrite;
using testsupport::FakeXp60;
using xpmodel::Xp60Performance;
using xpmodel::Xp60PerformanceCodec;
using xpmodel::Xp60PerformanceLayout;
using State = services::UserPerformanceWrite::State;

namespace {

roland::RolandAddress userAddress(int userNumber)
{
    return *Xp60PerformanceLayout::userPerformanceAddress(userNumber);
}

Xp60Performance performanceFrom(const xpmodel::MemoryImage& image, const roland::RolandAddress& base)
{
    return *Xp60PerformanceCodec::decode(image, base).performance;
}

struct Fixture
{
    midi::LoopbackMidiTransport* transport = nullptr;
    std::unique_ptr<services::DeviceSession> session;
    std::unique_ptr<UserPerformanceWrite> writer;
    std::unique_ptr<FakeXp60> device;

    Fixture()
    {
        auto loopback = std::make_unique<midi::LoopbackMidiTransport>();
        loopback->addInput("in-1", "XP-60 IN");
        loopback->addOutput("out-1", "XP-60 OUT");
        transport = loopback.get();
        session = std::make_unique<services::DeviceSession>(std::move(loopback));
        session->setAutomaticTimeoutPolling(false);
        auto pacing = session->pacing();
        pacing.interMessageDelay = 0ms;
        session->setPacing(pacing);
        writer = std::make_unique<UserPerformanceWrite>(*session);
        device = std::make_unique<FakeXp60>(testsupport::fixtureImage());
        session->connectEndpoints("in-1", "out-1");
    }

    void pump(int rounds = 8000)
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
                && session->pendingSendCount() == 0 && !writer->isBusy()) {
                return;
            }
        }
    }

    [[nodiscard]] Xp60Performance inUserSlot(int userNumber) const
    {
        return performanceFrom(device->memory(), userAddress(userNumber));
    }
};

} // namespace

class TestUserPerformanceWrite : public QObject
{
    Q_OBJECT

private slots:
    void writesAPerformanceToAUserSlotAndVerifiesIt()
    {
        Fixture f;
        const auto source = f.inUserSlot(1);
        const auto before = f.inUserSlot(20);
        const auto before21 = f.inUserSlot(21);
        QVERIFY(!(source == before));

        QVERIFY(f.writer->arm());
        QVERIFY(f.writer->writeOne(source, 20));
        f.pump();

        QCOMPARE(f.writer->state(), State::Completed);
        QCOMPARE(f.writer->completed(), std::size_t{1});
        QVERIFY(f.inUserSlot(20) == source);
        // Only the destination moved.
        QVERIFY(f.inUserSlot(1) == source);
        QVERIFY(f.inUserSlot(21) == before21);
    }

    void everyDestinationIsReadBeforeItIsWritten()
    {
        Fixture f;
        const auto source = f.inUserSlot(1);
        const auto occupant = f.inUserSlot(17);

        QVERIFY(f.writer->arm());
        QVERIFY(f.writer->writeOne(source, 17));
        f.pump();

        QCOMPARE(f.writer->state(), State::Completed);
        QCOMPARE(f.writer->snapshots().size(), std::size_t{1});
        QCOMPARE(f.writer->snapshots().front().userNumber, 17);
        // The snapshot is what was there, not what was written.
        QVERIFY(f.writer->snapshots().front().performance == occupant);
        QVERIFY(f.writer->canRestore());
    }

    void writesSeveralDestinationsInOrder()
    {
        Fixture f;
        std::vector<UserPerformanceWrite::Destination> plan;
        for (int i = 0; i < 3; ++i) {
            plan.push_back(
                UserPerformanceWrite::Destination{25 + i, f.inUserSlot(1 + i)});
        }
        const auto expected = plan;

        QVERIFY(f.writer->arm());
        QSignalSpy progress(f.writer.get(), &UserPerformanceWrite::progressed);
        QVERIFY(f.writer->write(plan));
        f.pump();

        QCOMPARE(f.writer->state(), State::Completed);
        QCOMPARE(f.writer->completed(), std::size_t{3});
        QCOMPARE(progress.count(), 3);
        for (const auto& destination : expected) {
            QVERIFY(f.inUserSlot(destination.userNumber) == destination.performance);
        }
    }

    void anInstrumentThatRefusesTheWriteIsReportedAsAMismatch()
    {
        Fixture f;
        const auto source = f.inUserSlot(1);
        const auto before = f.inUserSlot(30);
        // What User Memory Protect being ON looks like from here.
        f.device->setAcceptWrites(false);

        QVERIFY(f.writer->arm());
        QVERIFY(f.writer->writeOne(source, 30));
        f.pump();

        QCOMPARE(f.writer->state(), State::Mismatch);
        QCOMPARE(f.writer->completed(), std::size_t{0});
        QVERIFY(f.inUserSlot(30) == before);
        QVERIFY2(f.writer->message().contains(QStringLiteral("User Memory Protect")),
                 "the likely cause has to be named, not left for the user to guess");
    }

    void restorePutsBackEveryPerformanceTheRunOverwrote()
    {
        Fixture f;
        const auto beforeA = f.inUserSlot(10);
        const auto beforeB = f.inUserSlot(11);
        std::vector<UserPerformanceWrite::Destination> plan{
            UserPerformanceWrite::Destination{10, f.inUserSlot(1)},
            UserPerformanceWrite::Destination{11, f.inUserSlot(2)}};

        QVERIFY(f.writer->arm());
        QVERIFY(f.writer->write(plan));
        f.pump();
        QCOMPARE(f.writer->state(), State::Completed);
        QVERIFY(!(f.inUserSlot(10) == beforeA));

        // Restore needs no arming: putting back what was there is the undo, not
        // a new destructive act.
        QVERIFY(f.writer->canRestore());
        QVERIFY(f.writer->restore());
        f.pump();

        QCOMPARE(f.writer->state(), State::Completed);
        QVERIFY(f.inUserSlot(10) == beforeA);
        QVERIFY(f.inUserSlot(11) == beforeB);
    }

    void aPartlyIllegalPlanWritesNothingAtAll()
    {
        Fixture f;
        const auto before = f.inUserSlot(5);

        // 33 is outside the 32-slot Performance bank. The legal destination
        // that precedes it must not be written either.
        std::vector<UserPerformanceWrite::Destination> plan{
            UserPerformanceWrite::Destination{5, f.inUserSlot(1)},
            UserPerformanceWrite::Destination{33, f.inUserSlot(2)}};
        QVERIFY(f.writer->arm());
        QVERIFY(!f.writer->write(plan));
        f.pump();

        QCOMPARE(f.writer->state(), State::Failed);
        QVERIFY(f.inUserSlot(5) == before);
        QCOMPARE(f.device->dataSetsReceived(), std::size_t{0});

        // The same for a plan that addresses one slot twice: only the second
        // would survive, which is a silent loss on the instrument.
        Fixture g;
        std::vector<UserPerformanceWrite::Destination> duplicated{
            UserPerformanceWrite::Destination{7, g.inUserSlot(1)},
            UserPerformanceWrite::Destination{7, g.inUserSlot(2)}};
        QVERIFY(g.writer->arm());
        QVERIFY(!g.writer->write(duplicated));
        QCOMPARE(g.writer->state(), State::Failed);
        QCOMPARE(g.device->dataSetsReceived(), std::size_t{0});
    }

    void armingIsRequiredAndSpentByOneRun()
    {
        Fixture f;
        const auto source = f.inUserSlot(1);
        const auto before = f.inUserSlot(15);

        // Unarmed: refused, and nothing is sent.
        QVERIFY(!f.writer->writeOne(source, 15));
        f.pump();
        QCOMPARE(f.writer->state(), State::Failed);
        QVERIFY(f.inUserSlot(15) == before);
        QCOMPARE(f.device->dataSetsReceived(), std::size_t{0});

        QVERIFY(f.writer->arm());
        QVERIFY(f.writer->isArmed());
        QVERIFY(f.writer->writeOne(source, 15));
        f.pump();
        QCOMPARE(f.writer->state(), State::Completed);
        // One arming permits one run.
        QVERIFY(!f.writer->isArmed());
        QVERIFY(!f.writer->writeOne(source, 16));
        QVERIFY(!(f.inUserSlot(16) == source));
    }

    void armingIsNotSharedWithThePatchWriter()
    {
        Fixture f;
        services::UserMemoryWrite patchWriter(*f.session);

        QVERIFY(f.writer->arm());
        QVERIFY2(!patchWriter.isArmed(),
                 "arming one region must never authorise a write to the other");
        QVERIFY(patchWriter.canArm());
        patchWriter.disarm();
        QVERIFY(f.writer->isArmed());
    }

    void thePatchWriterIgnoresThisRunsReplies()
    {
        // Both writers listen to the one shared fetch slot on the session. A
        // Performance fetch leaves `patch` empty, so a Patch writer sees nothing
        // rather than the wrong thing — and vice versa.
        Fixture f;
        services::UserMemoryWrite patchWriter(*f.session);

        QVERIFY(f.writer->arm());
        QVERIFY(f.writer->writeOne(f.inUserSlot(1), 12));
        f.pump();

        QCOMPARE(f.writer->state(), State::Completed);
        QCOMPARE(patchWriter.state(), services::UserMemoryWrite::State::Idle);
        QVERIFY(patchWriter.snapshots().empty());
    }

    void theWritePlanSaysWhatItDoesNotWrite()
    {
        Fixture f;
        const std::vector<UserPerformanceWrite::Destination> plan{
            UserPerformanceWrite::Destination{3, f.inUserSlot(1)}};
        const auto text = f.writer->writePlanDescription(plan);

        QVERIFY(text.contains(QStringLiteral("USER:03")));
        QVERIFY(text.contains(QStringLiteral("permanent")));
        // The trap this warning exists for: a Performance names sixteen Patches
        // but does not carry them, so writing one does not bring its sounds.
        QVERIFY2(text.contains(QStringLiteral("does not carry them")),
                 "a Performance write that silently assumes the Patches are there is a trap");
        QVERIFY(text.contains(QStringLiteral("User Memory Protect")));
        QCOMPARE(f.writer->writePlanDescription({}), QStringLiteral("Nothing to write."));
    }

    void aDisconnectDisarmsAndFailsTheRun()
    {
        Fixture f;
        QVERIFY(f.writer->arm());
        f.session->disconnectEndpoints();
        QVERIFY2(!f.writer->isArmed(), "arming must never survive a disconnect");
        QVERIFY(!f.writer->canArm());
    }
};

QTEST_MAIN(TestUserPerformanceWrite)
#include "tst_user_performance_write.moc"
