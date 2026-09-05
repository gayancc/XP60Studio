#include "support/FakeXp60.h"

#include "services/PatchTransfer.h"

#include <QSignalSpy>
#include <QtTest>

#include <chrono>
#include <memory>

using namespace xp60studio;
using namespace xp60studio::roland;
using namespace xp60studio::xpmodel;
using namespace xp60studio::testsupport;
using namespace std::chrono_literals;

namespace {

const RolandAddress kTemp = temporaryPatchAddress();

struct Fixture
{
    midi::LoopbackMidiTransport* transport = nullptr;
    std::unique_ptr<services::DeviceSession> session;
    std::unique_ptr<services::PatchTransfer> transfer;
    std::unique_ptr<FakeXp60> device;
    protocol::TimePoint now{std::chrono::duration_cast<protocol::Clock::duration>(1000ms)};

    explicit Fixture(int patchInTemporaryArea = 4)
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
        device = std::make_unique<FakeXp60>(temporaryAreaWith(patchInTemporaryArea));
        session->connectEndpoints("in-1", "out-1");
    }

    // Runs the conversation until nothing is left in flight.
    void pump(int rounds = 40)
    {
        for (int i = 0; i < rounds; ++i) {
            QCoreApplication::processEvents();
            const auto replies = device->exchange(*transport);
            for (const auto& reply : replies) {
                const auto bytes = reply.encode();
                transport->injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
            }
            QCoreApplication::processEvents();
            if (replies.empty() && transport->sentMessages().empty() && session->pendingSendCount() == 0) {
                return;
            }
        }
    }

    // Runs until the transfer reaches `target`, or gives up.
    void pumpUntil(services::PatchTransfer::State target, int rounds = 40)
    {
        for (int i = 0; i < rounds && transfer->state() != target; ++i) {
            QCoreApplication::processEvents();
            const auto replies = device->exchange(*transport);
            for (const auto& reply : replies) {
                const auto bytes = reply.encode();
                transport->injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
            }
            QCoreApplication::processEvents();
        }
    }

    // Reads the temporary patch once so the transfer may be armed.
    void establishReadVerified()
    {
        QVERIFY(session->fetchTemporaryPatch());
        pump();
        QCOMPARE(session->patchFetch().state, services::DeviceSession::PatchFetchState::Completed);
        QVERIFY(transfer->readVerified());
    }
};

using State = services::PatchTransfer::State;

} // namespace

class PatchTransferTest : public QObject
{
    Q_OBJECT

private slots:
    void changedSpanCodecPreservesMultiByteParametersAndAllPatchBytes()
    {
        const auto before = patchFrom(temporaryAreaWith(4), kTemp);
        auto after = before;
        QVERIFY(after.setRaw(CommonParameter::PatchTempo, after.raw(CommonParameter::PatchTempo) == 120 ? 121 : 120));
        auto messages = Xp60PatchCodec::encodeChangesToDataSets(before, after, xp60::factoryDefaultDeviceId(), xp60::modelId(), kTemp);
        QCOMPARE(messages.size(), std::size_t(1));
        const auto& tempo = xp60tables::descriptor(CommonParameter::PatchTempo);
        QCOMPARE(messages.front().address(), *kTemp.plus(tempo.offset));
        QCOMPARE(messages.front().data().size(), std::size_t(tempo.byteCount));
        const auto target = patchFrom(temporaryAreaWith(7), kTemp);
        messages = Xp60PatchCodec::encodeChangesToDataSets(before, target, xp60::factoryDefaultDeviceId(), xp60::modelId(), kTemp, 64);
        QVERIFY(!messages.empty());
        auto image = Xp60PatchCodec::encodeToImage(before, kTemp);
        for (const auto& message : messages) {
            QVERIFY(message.data().size() <= 64);
            image.addDataSet(message);
        }
        QVERIFY(patchFrom(image, kTemp) == target);
        QVERIFY(Xp60PatchCodec::encodeChangesToDataSets(before, before, xp60::factoryDefaultDeviceId(), xp60::modelId(), kTemp).empty());
    }

    void livePreviewRequiresArmingCoalescesAndKeepsItsOriginalSnapshot()
    {
        Fixture f;
        f.establishReadVerified();
        const auto before = *f.session->patchFetch().patch;
        QVERIFY(!f.transfer->startLivePreview(before));
        QVERIFY(f.transfer->arm());
        QVERIFY(f.transfer->startLivePreview(before));
        f.pump();
        QCOMPARE(f.transfer->state(), State::Verified);
        QVERIFY(f.transfer->liveActive());
        QVERIFY(!f.transfer->canArm());
        QVERIFY(!f.transfer->writeAndVerifyTemporaryPatch(before));
        const auto sent = f.device->dataSetsReceived();
        auto desired = before;
        for (int i = 0; i < 90; ++i) {
            desired.setRaw(ToneIndex::tone1(), ToneParameter::ToneLevel, i);
            f.transfer->queueLivePreview(desired);
        }
        QCOMPARE(f.device->dataSetsReceived(), sent);
        QTest::qWait(150);
        f.pump();
        QCOMPARE(f.transfer->state(), State::Verified);
        QCOMPARE(f.device->dataSetsReceived(), sent + 1);
        QVERIFY(*f.transfer->readBack() == desired);
        QVERIFY(*f.transfer->safetySnapshot() == before);
        f.transfer->stopLivePreview(before);
        QTest::qWait(150);
        f.pump();
        QTest::qWait(150);
        QVERIFY(!f.transfer->liveActive());
        QVERIFY(patchFrom(f.device->memory(), kTemp) == before);
        QCOMPARE(f.transfer->state(), State::Verified);
    }

    // Deferred verification: the latency fix, and the promise that goes with it.
    //
    // A full read-back costs about 265 ms on the instrument, so paying it
    // between every two values of a knob drag is what made live editing feel
    // disconnected. WhenSettled sends without reading back while the musician is
    // still moving. What must never slip is the honesty of the state: an
    // unverified update reports Sent, never Verified.
    void deferredVerificationSendsWithoutReadingBackUntilEditingStops()
    {
        Fixture f;
        f.establishReadVerified();
        const auto before = *f.session->patchFetch().patch;
        QVERIFY(f.transfer->arm());
        f.transfer->setVerification(services::PatchTransfer::Verification::WhenSettled);
        f.transfer->setSettleDelay(400ms);
        QVERIFY(f.transfer->startLivePreview(before));
        f.pump();

        auto desired = before;
        desired.setRaw(ToneIndex::tone1(), ToneParameter::ToneLevel, 40);
        f.transfer->queueLivePreview(desired);
        QTest::qWait(150);
        f.pump();

        // The bytes are on the instrument, and the transfer says so without
        // claiming to have proved it.
        QCOMPARE(f.transfer->state(), State::Sent);
        QVERIFY(patchFrom(f.device->memory(), kTemp) == desired);

        // A second update goes out immediately: nothing is waiting on a
        // read-back, which is the entire point.
        const auto afterFirst = f.device->dataSetsReceived();
        desired.setRaw(ToneIndex::tone1(), ToneParameter::ToneLevel, 41);
        f.transfer->queueLivePreview(desired);
        QTest::qWait(150);
        f.pump();
        QVERIFY(f.device->dataSetsReceived() > afterFirst);
        QCOMPARE(f.transfer->state(), State::Sent);

        // The gesture stops. The settle timer comes round and proves it.
        QTest::qWait(500);
        f.pump();
        QCOMPARE(f.transfer->state(), State::Verified);
        QVERIFY(*f.transfer->readBack() == desired);
        QVERIFY(patchFrom(f.device->memory(), kTemp) == desired);

        // Leaving live mode restores immediate verification, so a one-shot
        // armed write is never governed by a policy meant for a knob drag.
        f.transfer->stopLivePreview(desired);
        QTest::qWait(150);
        f.pump();
        QTest::qWait(150);
        f.pump();
        QVERIFY(!f.transfer->liveActive());
        QCOMPARE(f.transfer->verification(), services::PatchTransfer::Verification::EveryUpdate);
        QCOMPARE(f.transfer->state(), State::Verified);
    }

    // Stopping is the moment the promise is kept: audition never ends on an
    // unverified state, whatever the policy was during it.
    void stoppingAnAuditionAlwaysVerifiesEvenWhenDeferring()
    {
        Fixture f;
        f.establishReadVerified();
        const auto before = *f.session->patchFetch().patch;
        QVERIFY(f.transfer->arm());
        f.transfer->setVerification(services::PatchTransfer::Verification::WhenSettled);
        QVERIFY(f.transfer->startLivePreview(before));
        f.pump();

        auto desired = before;
        desired.setRaw(ToneIndex::tone1(), ToneParameter::ToneLevel, 55);
        f.transfer->queueLivePreview(desired);
        QTest::qWait(150);
        f.pump();
        QCOMPARE(f.transfer->state(), State::Sent);

        // Stop before the settle timer would have fired.
        f.transfer->stopLivePreview(desired);
        QTest::qWait(150);
        f.pump();
        QTest::qWait(150);
        f.pump();
        QCOMPARE(f.transfer->state(), State::Verified);
        QVERIFY(*f.transfer->readBack() == desired);
    }

    void livePreviewRetainsLatestEditDuringReadBackAndStopsOnMismatch()
    {
        Fixture f;
        f.establishReadVerified();
        auto desired = *f.session->patchFetch().patch;
        QVERIFY(f.transfer->arm());
        QVERIFY(f.transfer->startLivePreview(desired));
        desired.setRaw(ToneIndex::tone1(), ToneParameter::ToneLevel, 17);
        f.transfer->queueLivePreview(desired);
        desired.setRaw(ToneIndex::tone1(), ToneParameter::ToneLevel, 19);
        f.transfer->queueLivePreview(desired);
        f.pump();
        QTest::qWait(150);
        f.pump();
        QVERIFY(*f.transfer->readBack() == desired);
        f.device->setAcceptWrites(false);
        desired.setRaw(ToneIndex::tone1(), ToneParameter::ToneLevel, 23);
        f.transfer->queueLivePreview(desired);
        QTest::qWait(150);
        f.pump();
        QCOMPARE(f.transfer->state(), State::Mismatch);
        QVERIFY(!f.transfer->liveActive());
        const auto sent = f.device->dataSetsReceived();
        f.transfer->queueLivePreview(desired);
        QTest::qWait(150);
        f.pump();
        QCOMPARE(f.device->dataSetsReceived(), sent);
    }

    void livePreviewCancellationAndDisconnectDiscardPendingChanges()
    {
        for (bool disconnect : {false, true}) {
            Fixture f;
            f.establishReadVerified();
            auto desired = *f.session->patchFetch().patch;
            QVERIFY(f.transfer->arm());
            QVERIFY(f.transfer->startLivePreview(desired));
            f.pump();
            const auto sent = f.device->dataSetsReceived();
            desired.setRaw(ToneIndex::tone1(), ToneParameter::ToneLevel, 8);
            f.transfer->queueLivePreview(desired);
            if (disconnect) f.session->disconnectEndpoints();
            else f.transfer->cancel();
            QVERIFY(!f.transfer->liveActive());
            QTest::qWait(150);
            f.pump();
            QCOMPARE(f.device->dataSetsReceived(), sent);
            QVERIFY(!f.transfer->isArmed());
        }
    }

    void armingIsRefusedUntilAReadHasSucceeded()
    {
        Fixture f;
        QVERIFY(!f.transfer->readVerified());
        QVERIFY(!f.transfer->canArm());
        QVERIFY(!f.transfer->arm());
        QVERIFY(!f.transfer->isArmed());

        // Writing without arming is refused and writes nothing.
        const auto patch = patchFrom(f.device->memory(), kTemp);
        QVERIFY(!f.transfer->writeAndVerifyTemporaryPatch(patch));
        QCOMPARE(f.transfer->state(), State::Failed);
        QCOMPARE(f.device->dataSetsReceived(), std::size_t(0));

        f.establishReadVerified();
        QVERIFY(f.transfer->canArm());
        QVERIFY(f.transfer->arm());
        QVERIFY(f.transfer->isArmed());
        QVERIFY(!f.transfer->arm()); // already armed
    }

    void writePlanNamesTheTemporaryAreaAndTheByteCount()
    {
        Fixture f;
        const auto plan = f.transfer->writePlanDescription();
        QVERIFY(plan.find("03 00 00 00") != std::string::npos);
        QVERIFY(plan.find("589") != std::string::npos); // 73 + 4 * 129
        QVERIFY(plan.find("permanent User patches are not touched") != std::string::npos);
    }

    void happyPathCapturesSnapshotSendsReadsBackAndVerifies()
    {
        Fixture f(4); // "Jimmee Dee" sits in the temporary area
        f.establishReadVerified();
        const auto intended = patchFrom(fixtureImage(), *Xp60PatchLayout::userPatchAddress(34)); // "Singil Piper"
        QCOMPARE(intended.name().text(), std::string("Singil Piper"));

        QVERIFY(f.transfer->arm());
        QSignalSpy spy(f.transfer.get(), &services::PatchTransfer::changed);
        QVERIFY(f.transfer->writeAndVerifyTemporaryPatch(intended));
        QCOMPARE(f.transfer->state(), State::CapturingSafetySnapshot);
        f.pump();

        QCOMPARE(f.transfer->state(), State::Verified);
        QVERIFY(f.transfer->message().find("Verified") != std::string::npos);
        QVERIFY(f.transfer->diff()->identical());
        QCOMPARE(f.transfer->messagesSent(), std::size_t(9)); // Common + 4 Tones split at 128
        QCOMPARE(f.device->dataSetsReceived(), std::size_t(9));
        QVERIFY(spy.count() >= 4); // snapshot, sending, reading back, comparing, verified

        // The snapshot holds what was there before, the read-back what is there now.
        QCOMPARE(f.transfer->safetySnapshot()->name().text(), std::string("Jimmee Dee"));
        QCOMPARE(f.transfer->readBack()->name().text(), std::string("Singil Piper"));
        QVERIFY(*f.transfer->readBack() == intended);
        // The device really holds the new patch.
        QCOMPARE(patchFrom(f.device->memory(), kTemp).name().text(), std::string("Singil Piper"));
        // Arming was spent.
        QVERIFY(!f.transfer->isArmed());
    }

    void aSingleWrongByteIsReportedAsAMismatchNotASuccess()
    {
        Fixture f;
        f.establishReadVerified();
        const auto intended = patchFrom(fixtureImage(), *Xp60PatchLayout::userPatchAddress(34));
        QVERIFY(f.transfer->arm());
        QVERIFY(f.transfer->writeAndVerifyTemporaryPatch(intended));

        // The device takes the write, but one byte of Tone 2's cutoff does not stick.
        const auto cutoff = *kTemp.plus(Xp60PatchLayout::toneOffset(ToneIndex::tone2()) + 0x51);
        const int sent = intended.raw(ToneIndex::tone2(), ToneParameter::CutoffFrequency);
        f.device->corruptOnWrite(cutoff, static_cast<Byte>(sent == 0 ? 1 : sent - 1));
        f.pump();

        QCOMPARE(f.transfer->state(), State::Mismatch);
        QVERIFY(!f.transfer->diff()->identical());
        QCOMPARE(f.transfer->diff()->count(), std::size_t(1));
        const auto& d = f.transfer->diff()->differences()[0];
        QCOMPARE(d.block, std::string("Tone 2"));
        QCOMPARE(d.parameterName, std::string("Cutoff Frequency"));
        QCOMPARE(d.leftRaw, sent);
        QVERIFY(d.rightRaw != sent);
        QVERIFY(f.transfer->message().find("nothing has been normalised") != std::string::npos);
    }

    void aDeviceThatIgnoresWritesIsNotReportedAsVerified()
    {
        Fixture f(4);
        f.establishReadVerified();
        f.device->setAcceptWrites(false); // e.g. Rx Exclusive off
        const auto intended = patchFrom(fixtureImage(), *Xp60PatchLayout::userPatchAddress(34));
        QVERIFY(f.transfer->arm());
        QVERIFY(f.transfer->writeAndVerifyTemporaryPatch(intended));
        f.pump();

        // Bytes were transmitted, but the instrument still holds the old patch.
        QCOMPARE(f.device->dataSetsReceived(), std::size_t(9));
        QCOMPARE(f.transfer->state(), State::Mismatch);
        QCOMPARE(f.transfer->readBack()->name().text(), std::string("Jimmee Dee"));
        QVERIFY(f.transfer->diff()->count() > 1);
    }

    void aFailedSafetySnapshotWritesNothing()
    {
        Fixture f;
        f.establishReadVerified();
        f.device->setAnswerRequests(false);
        QVERIFY(f.transfer->arm());
        QVERIFY(f.transfer->writeAndVerifyTemporaryPatch(patchFrom(fixtureImage(), *Xp60PatchLayout::userPatchAddress(34))));
        f.pump();
        f.now += std::chrono::duration_cast<protocol::Clock::duration>(2000ms);
        f.session->pollTimeouts();

        QCOMPARE(f.transfer->state(), State::Failed);
        QVERIFY(f.transfer->message().find("nothing was written") != std::string::npos);
        QCOMPARE(f.device->dataSetsReceived(), std::size_t(0));
        QVERIFY(!f.transfer->isArmed());
    }

    void aFailedReadBackLeavesTheWriteUnverified()
    {
        Fixture f;
        f.establishReadVerified();
        const auto intended = patchFrom(fixtureImage(), *Xp60PatchLayout::userPatchAddress(34));
        // The device answers the safety snapshot (one request per block, five
        // in all), takes the write, then goes quiet before the read-back.
        f.device->answerOnlyNextRequests(Xp60PatchLayout::blocks().size());
        QVERIFY(f.transfer->arm());
        QVERIFY(f.transfer->writeAndVerifyTemporaryPatch(intended));
        f.pump();
        QCOMPARE(f.transfer->state(), State::ReadingBack);
        f.now += std::chrono::duration_cast<protocol::Clock::duration>(2000ms);
        f.session->pollTimeouts();

        QCOMPARE(f.transfer->state(), State::Failed);
        QVERIFY(f.transfer->message().find("unverified") != std::string::npos);
        QCOMPARE(f.device->dataSetsReceived(), std::size_t(9)); // the data did go out
    }

    void restoringTheSafetySnapshotPutsTheOriginalBack()
    {
        Fixture f(4);
        f.establishReadVerified();
        const auto intended = patchFrom(fixtureImage(), *Xp60PatchLayout::userPatchAddress(34));
        QVERIFY(f.transfer->arm());
        QVERIFY(f.transfer->writeAndVerifyTemporaryPatch(intended));
        f.pump();
        QCOMPARE(f.transfer->state(), State::Verified);
        QCOMPARE(patchFrom(f.device->memory(), kTemp).name().text(), std::string("Singil Piper"));

        // Restore needs its own arming, like any other write.
        QVERIFY(!f.transfer->restoreSafetySnapshot());
        QVERIFY(f.transfer->arm());
        QVERIFY(f.transfer->restoreSafetySnapshot());
        f.pump();
        QCOMPARE(f.transfer->state(), State::Verified);
        QCOMPARE(patchFrom(f.device->memory(), kTemp).name().text(), std::string("Jimmee Dee"));
        QVERIFY(f.transfer->message().find("safety snapshot") != std::string::npos);
    }

    void cancellingMidTransferSaysTheStateIsUncertain()
    {
        Fixture f;
        f.establishReadVerified();
        QVERIFY(f.transfer->arm());
        QVERIFY(f.transfer->writeAndVerifyTemporaryPatch(patchFrom(fixtureImage(), *Xp60PatchLayout::userPatchAddress(34))));
        f.transfer->cancel();
        QCOMPARE(f.transfer->state(), State::Cancelled);
        QVERIFY(f.transfer->message().find("power-cycle") != std::string::npos);
        QVERIFY(!f.transfer->isArmed());
    }

    void disconnectingDisarmsAndFailsAnyTransfer()
    {
        Fixture f;
        f.establishReadVerified();
        QVERIFY(f.transfer->arm());
        f.session->disconnectEndpoints();
        QVERIFY(!f.transfer->isArmed());
        QVERIFY(!f.transfer->canArm());
        QVERIFY(!f.transfer->readVerified());
        QVERIFY(f.session->connectEndpoints("in-1", "out-1"));
        QVERIFY(!f.transfer->canArm());

        Fixture g;
        g.establishReadVerified();
        QVERIFY(g.transfer->arm());
        QVERIFY(g.transfer->writeAndVerifyTemporaryPatch(patchFrom(fixtureImage(), *Xp60PatchLayout::userPatchAddress(34))));
        g.session->disconnectEndpoints();
        QCOMPARE(g.transfer->state(), State::Failed);
    }

    void cancellationStopsQueuedDataSets()
    {
        Fixture f;
        f.establishReadVerified();
        QVERIFY(f.transfer->arm());
        // Queue cancellation before sendDataSets queues its send callback.
        // No packet may leave after cancellation, even on zero-delay pacing.
        QObject::connect(f.transfer.get(), &services::PatchTransfer::changed, f.transfer.get(), [&] {
            if (f.transfer->state() == State::Sending) {
                QMetaObject::invokeMethod(f.transfer.get(), [&] { f.transfer->cancel(); }, Qt::QueuedConnection);
            }
        });
        QVERIFY(f.transfer->writeAndVerifyTemporaryPatch(patchFrom(fixtureImage(), *Xp60PatchLayout::userPatchAddress(34))));
        f.pump();
        QCOMPARE(f.transfer->state(), State::Cancelled);
        QCOMPARE(f.session->pendingDataSetBatches(), std::size_t{0});
        QCOMPARE(f.session->pendingSendCount(), std::size_t{0});
        QCOMPARE(f.device->dataSetsReceived(), std::size_t{0});
    }

    void changingDeviceIdRequiresAnotherSuccessfulRead()
    {
        Fixture f;
        f.establishReadVerified();
        QVERIFY(f.transfer->arm());
        f.session->setDeviceId(*RolandDeviceId::fromDisplayNumber(18));
        QVERIFY(!f.transfer->isArmed());
        QVERIFY(!f.transfer->readVerified());
        QVERIFY(!f.transfer->canArm());
    }

    void everyMessageStaysWithinThePacketLimitAndIsPaced()
    {
        Fixture f;
        f.establishReadVerified();
        auto pacing = f.session->pacing();
        pacing.interMessageDelay = 20ms; // the documented minimum gap
        f.session->setPacing(pacing);

        QVERIFY(f.transfer->arm());
        QVERIFY(f.transfer->writeAndVerifyTemporaryPatch(patchFrom(fixtureImage(), *Xp60PatchLayout::userPatchAddress(34))));

        // With a 20 ms gap the batch cannot drain in one go: the queue holds
        // messages back until the clock advances.
        bool sawQueueHeldBack = false;
        for (int i = 0; i < 60 && f.transfer->state() != State::Verified; ++i) {
            QCoreApplication::processEvents();
            if (f.transfer->state() == State::Sending && f.session->pendingSendCount() > 0) {
                sawQueueHeldBack = true;
            }
            const auto replies = f.device->exchange(*f.transport);
            for (const auto& reply : replies) {
                const auto bytes = reply.encode();
                f.transport->injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
            }
            QCoreApplication::processEvents();
            f.now += std::chrono::duration_cast<protocol::Clock::duration>(20ms);
            f.session->pumpSendQueue();
        }
        QVERIFY(sawQueueHeldBack);
        QCOMPARE(f.transfer->state(), State::Verified);
        QCOMPARE(f.device->dataSetsReceived(), std::size_t(9));
    }

    void stateNamesAndLabels()
    {
        Fixture f;
        QCOMPARE(QString::fromUtf8(f.transfer->stateName().data()), QStringLiteral("Idle"));
        QCOMPARE(f.transfer->stateLabel(), std::string("Idle"));
        QVERIFY(!f.transfer->isBusy());
        QVERIFY(!f.transfer->diff().has_value());
        QVERIFY(!f.transfer->safetySnapshot().has_value());
    }
};

QTEST_GUILESS_MAIN(PatchTransferTest)
#include "tst_patch_transfer.moc"
