// Writing Patches into the XP-60's permanent USER memory.
//
// This is the one destructive thing the application does, and the point of the
// whole librarian: a bank built in XP60Studio has to reach the keyboard. The
// tests are therefore written around the safety envelope rather than around the
// happy path, because the happy path is the easy half.
//
// The simulator holds a real XP-60 User bank (tests/fixtures/xp60/
// user-bank-amal.syx) at its real addresses `11 nn 00 00`, so a write here goes
// to the same place it would on the instrument, and a read-back reads the same
// bytes back.
//
// What each test is really asserting:
//
//   * nothing is written to a destination that has not first been read, so the
//     previous Patch can always be put back;
//   * a plan that is partly illegal writes nothing at all;
//   * an instrument that refuses the write (which is what User Memory Protect
//     looks like from here) is reported as a mismatch, never as success;
//   * a run stops between Patches, never inside one;
//   * arming is separate from the temporary-area transfer and is spent once.

#include "services/DeviceSession.h"
#include "services/PatchTransfer.h"
#include "services/UserMemoryWrite.h"
#include "support/FakeXp60.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QtTest>

#include <algorithm>
#include <chrono>
#include <memory>

using namespace xp60studio;
using namespace std::chrono_literals;
using services::UserMemoryWrite;
using testsupport::FakeXp60;
using xpmodel::ToneIndex;
using xpmodel::ToneParameter;
using xpmodel::Xp60Patch;
using xpmodel::Xp60PatchCodec;
using xpmodel::Xp60PatchLayout;
using State = services::UserMemoryWrite::State;

namespace {

Xp60Patch patchFrom(const xpmodel::MemoryImage& image, const roland::RolandAddress& base)
{
    const auto decoded = Xp60PatchCodec::decode(image, base);
    return *decoded.patch;
}

roland::RolandAddress userAddress(int userNumber)
{
    return *Xp60PatchLayout::userPatchAddress(userNumber);
}

struct Fixture
{
    midi::LoopbackMidiTransport* transport = nullptr;
    std::unique_ptr<services::DeviceSession> session;
    std::unique_ptr<UserMemoryWrite> writer;
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
        writer = std::make_unique<UserMemoryWrite>(*session);
        // The whole 128-Patch User bank, at its real addresses.
        device = std::make_unique<FakeXp60>(testsupport::fixtureImage());
        session->connectEndpoints("in-1", "out-1");
    }

    void pump(int rounds = 4000)
    {
        for (int i = 0; i < rounds; ++i) {
            QCoreApplication::processEvents();
            const auto replies = device->exchange(*transport);
            for (const auto& reply : replies) {
                const auto bytes = reply.encode();
                transport->injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
            }
            QCoreApplication::processEvents();
            if (replies.empty() && transport->sentMessages().empty() && session->pendingSendCount() == 0
                && !writer->isBusy()) {
                return;
            }
        }
    }

    [[nodiscard]] Xp60Patch inUserSlot(int userNumber) const
    {
        return patchFrom(device->memory(), userAddress(userNumber));
    }
};

} // namespace

class TestUserMemoryWrite : public QObject
{
    Q_OBJECT

private slots:
    void writesAPatchToAUserSlotAndVerifiesIt();
    void everyDestinationIsReadBeforeItIsWritten();
    void writesAWholeArrangementInOrder();
    void anInstrumentThatRefusesTheWriteIsReportedAsAMismatch();
    void restorePutsBackEveryPatchTheRunOverwrote();
    void retryContinuesFromTheDestinationThatFailed();
    void aPartlyIllegalPlanWritesNothingAtAll();
    void armingIsRequiredSeparateAndSpentByOneRun();
    void theWritePlanNamesWhatItWillOverwrite();
};

// ---------------------------------------------------------------------------

void TestUserMemoryWrite::writesAPatchToAUserSlotAndVerifiesIt()
{
    Fixture f;
    // Take a real Patch from one slot and write it to a different one, so the
    // destination provably changes.
    const auto source = f.inUserSlot(1);
    const auto before = f.inUserSlot(50);
    const auto before51 = f.inUserSlot(51);
    QVERIFY(!(source == before));

    QVERIFY(f.writer->arm());
    QVERIFY(f.writer->writeOne(source, 50));
    f.pump();

    QCOMPARE(f.writer->state(), State::Completed);
    QCOMPARE(f.writer->completed(), std::size_t{1});
    QVERIFY(f.inUserSlot(50) == source);
    // Only the destination moved: the Patch beside it is untouched, and the one
    // it was copied from is still there.
    QVERIFY(f.inUserSlot(1) == source);
    QVERIFY(f.inUserSlot(51) == before51);
}

// The safety rule that makes this feature usable before the write direction has
// been confirmed on hardware: the previous Patch is always in hand first.
void TestUserMemoryWrite::everyDestinationIsReadBeforeItIsWritten()
{
    Fixture f;
    const auto source = f.inUserSlot(1);
    const auto occupant = f.inUserSlot(77);

    QVERIFY(f.writer->arm());
    QVERIFY(f.writer->writeOne(source, 77));
    f.pump();

    QCOMPARE(f.writer->state(), State::Completed);
    QCOMPARE(f.writer->snapshots().size(), std::size_t{1});
    QCOMPARE(f.writer->snapshots().front().userNumber, 77);
    // The snapshot is what was there, not what was written.
    QVERIFY(f.writer->snapshots().front().patch == occupant);
    QVERIFY(f.writer->canRestore());
}

void TestUserMemoryWrite::writesAWholeArrangementInOrder()
{
    Fixture f;
    std::vector<UserMemoryWrite::Destination> plan;
    // A gappy arrangement, as a built bank is: destinations are chosen, not
    // consecutive.
    const std::vector<int> targets{5, 9, 128};
    for (std::size_t i = 0; i < targets.size(); ++i) {
        plan.push_back({targets[i], f.inUserSlot(static_cast<int>(i) + 1)});
    }
    const auto untouched = f.inUserSlot(6);

    QSignalSpy progress(f.writer.get(), &UserMemoryWrite::progressed);
    QSignalSpy finished(f.writer.get(), &UserMemoryWrite::finished);
    QVERIFY(f.writer->arm());
    QVERIFY(f.writer->write(plan));
    f.pump();

    QCOMPARE(f.writer->state(), State::Completed);
    QCOMPARE(f.writer->completed(), std::size_t{3});
    QCOMPARE(progress.count(), 3);
    QCOMPARE(finished.count(), 1);
    QVERIFY(finished.at(0).at(0).toBool());
    for (std::size_t i = 0; i < targets.size(); ++i) {
        QVERIFY(f.inUserSlot(targets[i]) == plan[i].patch);
    }
    // The gaps are gaps: a destination the plan says nothing about is untouched.
    QVERIFY(f.inUserSlot(6) == untouched);
    QCOMPARE(f.writer->snapshots().size(), std::size_t{3});
}

// An XP-60 with User Memory Protect ON refuses the write, and from here that
// looks exactly like a destination that reads back unchanged. It must never be
// reported as success, and the message must name the likely cause.
void TestUserMemoryWrite::anInstrumentThatRefusesTheWriteIsReportedAsAMismatch()
{
    Fixture f;
    const auto source = f.inUserSlot(1);
    const auto protectedSlot = f.inUserSlot(60);
    f.device->setAcceptWrites(false);

    QVERIFY(f.writer->arm());
    QVERIFY(f.writer->writeOne(source, 60));
    f.pump();

    QCOMPARE(f.writer->state(), State::Mismatch);
    QCOMPARE(f.writer->completed(), std::size_t{0});
    QVERIFY(f.writer->message().contains(QStringLiteral("User Memory Protect")));
    // And the instrument is exactly as it was.
    QVERIFY(f.inUserSlot(60) == protectedSlot);
}

void TestUserMemoryWrite::restorePutsBackEveryPatchTheRunOverwrote()
{
    Fixture f;
    const std::vector<int> targets{20, 21, 22};
    std::vector<Xp60Patch> before;
    std::vector<UserMemoryWrite::Destination> plan;
    for (std::size_t i = 0; i < targets.size(); ++i) {
        before.push_back(f.inUserSlot(targets[i]));
        plan.push_back({targets[i], f.inUserSlot(static_cast<int>(i) + 1)});
    }

    QVERIFY(f.writer->arm());
    QVERIFY(f.writer->write(plan));
    f.pump();
    QCOMPARE(f.writer->state(), State::Completed);
    for (std::size_t i = 0; i < targets.size(); ++i) {
        QVERIFY(f.inUserSlot(targets[i]) == plan[i].patch);
    }

    // Restoring needs no arming: putting back what was there is the undo, not a
    // new destructive act.
    QVERIFY(f.writer->canRestore());
    QVERIFY(f.writer->restore());
    f.pump();

    QCOMPARE(f.writer->state(), State::Completed);
    for (std::size_t i = 0; i < targets.size(); ++i) {
        QVERIFY2(f.inUserSlot(targets[i]) == before[i], "the Patch that was there is back");
    }
}

// A mismatch is most likely User Memory Protect being ON. The musician turns it
// off and presses Retry, which must continue from where the run stopped and must
// not lose the backup of what was already written.
void TestUserMemoryWrite::retryContinuesFromTheDestinationThatFailed()
{
    Fixture f;
    const std::vector<int> targets{80, 81, 82};
    std::vector<Xp60Patch> before;
    std::vector<UserMemoryWrite::Destination> plan;
    for (std::size_t i = 0; i < targets.size(); ++i) {
        before.push_back(f.inUserSlot(targets[i]));
        plan.push_back({targets[i], f.inUserSlot(static_cast<int>(i) + 1)});
    }

    QVERIFY(f.writer->arm());
    QVERIFY(f.writer->write(plan));
    // Let the first destination land, then have the instrument refuse.
    for (int i = 0; i < 4000 && f.writer->completed() < 1; ++i) {
        QCoreApplication::processEvents();
        const auto replies = f.device->exchange(*f.transport);
        for (const auto& reply : replies) {
            const auto bytes = reply.encode();
            f.transport->injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
        }
        QCoreApplication::processEvents();
    }
    QCOMPARE(f.writer->completed(), std::size_t{1});
    f.device->setAcceptWrites(false);
    f.pump();

    QCOMPARE(f.writer->state(), State::Mismatch);
    QVERIFY(f.inUserSlot(targets[0]) == plan[0].patch); // the one that landed
    QVERIFY(f.inUserSlot(targets[1]) == before[1]);     // the one that was refused
    QCOMPARE(f.writer->unwritten().size(), std::size_t{2});
    QVERIFY(f.writer->canRetry());

    // Retrying is a destructive write like any other, so it needs arming again.
    QVERIFY(!f.writer->retry());
    QVERIFY(f.writer->arm());

    // The instrument stops refusing — Protect turned off — and the rest lands.
    f.device->setAcceptWrites(true);
    QVERIFY(f.writer->retry());
    f.pump();

    QCOMPARE(f.writer->state(), State::Completed);
    for (std::size_t i = 0; i < targets.size(); ++i) {
        QVERIFY(f.inUserSlot(targets[i]) == plan[i].patch);
    }
    // And the backup still covers the whole run, including the destination
    // written before the failure — losing that on Retry would be the worst
    // possible moment to lose it. One snapshot per destination, and it is the
    // *first* read: a retry must not overwrite the original with whatever the
    // failed attempt left behind.
    QCOMPARE(f.writer->snapshots().size(), std::size_t{3});
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const auto snapshot = std::find_if(f.writer->snapshots().begin(), f.writer->snapshots().end(),
                                           [&](const UserMemoryWrite::Destination& d) {
                                               return d.userNumber == targets[i];
                                           });
        QVERIFY(snapshot != f.writer->snapshots().end());
        QVERIFY(snapshot->patch == before[i]);
    }
    QVERIFY(f.writer->restore());
    f.pump();
    for (std::size_t i = 0; i < targets.size(); ++i) {
        QVERIFY2(f.inUserSlot(targets[i]) == before[i], "every Patch the run overwrote is back");
    }
}

void TestUserMemoryWrite::aPartlyIllegalPlanWritesNothingAtAll()
{
    Fixture f;
    const auto source = f.inUserSlot(1);
    const auto occupant = f.inUserSlot(30);

    // One destination outside the bank.
    QVERIFY(f.writer->arm());
    QVERIFY(!f.writer->write({{30, source}, {129, source}}));
    QCOMPARE(f.writer->state(), State::Failed);
    f.pump();
    QVERIFY(f.inUserSlot(30) == occupant);
    QCOMPARE(f.writer->snapshots().size(), std::size_t{0});

    // The same destination twice: only the second would survive, which is a
    // silent loss on the instrument. No re-arming is needed, because the
    // refused plan above attempted nothing and so spent nothing.
    QVERIFY(f.writer->isArmed());
    QVERIFY(!f.writer->write({{30, source}, {30, source}}));
    QVERIFY(f.writer->message().contains(QStringLiteral("only the second")));
    f.pump();
    QVERIFY(f.inUserSlot(30) == occupant);

    // A refused plan does not spend the arming, because nothing was attempted.
    QVERIFY(f.writer->isArmed());
}

void TestUserMemoryWrite::armingIsRequiredSeparateAndSpentByOneRun()
{
    Fixture f;
    const auto source = f.inUserSlot(1);
    const auto occupant = f.inUserSlot(40);

    // Unarmed: refused, and nothing is sent.
    QVERIFY(!f.writer->writeOne(source, 40));
    f.pump();
    QVERIFY(f.inUserSlot(40) == occupant);

    // Arming the temporary-area transfer authorises nothing here. These are
    // separate services precisely so that one cannot stand in for the other.
    services::PatchTransfer transfer(*f.session);
    QVERIFY(!f.writer->isArmed());
    QVERIFY(!f.writer->writeOne(source, 40));
    f.pump();
    QVERIFY(f.inUserSlot(40) == occupant);

    QVERIFY(f.writer->arm());
    QVERIFY(f.writer->isArmed());
    QVERIFY(f.writer->writeOne(source, 40));
    // The arming is spent by the attempt, so a second bank write needs a second
    // deliberate decision.
    QVERIFY(!f.writer->isArmed());
    f.pump();
    QCOMPARE(f.writer->state(), State::Completed);

    // A second run without a second arming is refused, and slot 41 keeps what
    // it always had.
    const auto slot41 = f.inUserSlot(41);
    QVERIFY(!f.writer->writeOne(source, 41));
    f.pump();
    QVERIFY(f.inUserSlot(41) == slot41);
}

void TestUserMemoryWrite::theWritePlanNamesWhatItWillOverwrite()
{
    Fixture f;
    const auto source = f.inUserSlot(1);
    const auto plan = f.writer->writePlanDescription({{21, source}, {5, source}});
    // The panel identity leads, as it does everywhere else, with the linear
    // number beside it — and the range is stated in slot order, not call order.
    QVERIFY2(plan.contains(QStringLiteral("A15")), qPrintable(plan));
    QVERIFY2(plan.contains(QStringLiteral("USER:005")), qPrintable(plan));
    QVERIFY2(plan.contains(QStringLiteral("A35")), qPrintable(plan));
    QVERIFY2(plan.contains(QStringLiteral("USER:021")), qPrintable(plan));
    QVERIFY(plan.contains(QStringLiteral("permanent")));
    QVERIFY(plan.contains(QStringLiteral("User Memory Protect")));
    QVERIFY(f.writer->writePlanDescription({}).contains(QStringLiteral("Nothing")));
}

QTEST_MAIN(TestUserMemoryWrite)
#include "tst_user_memory_write.moc"
