#include "midi/LoopbackMidiTransport.h"
#include "roland/RolandCodec.h"
#include "services/PatchTransfer.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60PatchCodec.h"

#include <QSignalSpy>
#include <QtTest>

#include <chrono>
#include <limits>
#include <fstream>
#include <memory>

using namespace xp60studio;
using namespace xp60studio::roland;
using namespace xp60studio::xpmodel;
using namespace std::chrono_literals;

namespace {

// A stand-in for the instrument: holds a memory image, answers RQ1 from it and
// applies DT1 to it. Deliberately simple — its job is to let the transfer state
// machine run end to end, not to emulate an XP-60.
class FakeXp60
{
public:
    explicit FakeXp60(MemoryImage memory)
        : m_memory(std::move(memory))
    {
    }

    // Corrupts one byte of what the device will report, to simulate a write
    // that did not fully take.
    void corruptByte(const RolandAddress& address, Byte value) { m_memory.write(address, ByteVector{value}); }
    // Applies that corruption automatically once the write has been taken.
    void corruptOnWrite(const RolandAddress& address, Byte value)
    {
        m_corruptAddress = address;
        m_corruptValue = value;
        m_corruptPending = true;
    }
    // Silently ignores writes, as a device with Rx Exclusive off would.
    void setAcceptWrites(bool accept) { m_acceptWrites = accept; }
    void setAnswerRequests(bool answer) { m_answerRequests = answer; }
    // Answers `n` more requests, then goes quiet — the way a device that
    // stops responding part way through a transfer would.
    void answerOnlyNextRequests(std::size_t n) { m_maxAnswers = m_answered + n; }

    [[nodiscard]] const MemoryImage& memory() const noexcept { return m_memory; }
    [[nodiscard]] std::size_t dataSetsReceived() const noexcept { return m_dataSetsReceived; }

    // Consumes everything the application sent and produces the replies.
    [[nodiscard]] std::vector<RolandSysExMessage> exchange(midi::LoopbackMidiTransport& transport)
    {
        std::vector<RolandSysExMessage> replies;
        const std::vector<RolandModelId> models{xp60::modelId()};
        for (const auto& raw : transport.sentMessages()) {
            const auto decoded = roland::decodeRolandSysEx(ByteSpan(raw.data(), raw.size()), models);
            if (!decoded.ok()) {
                continue;
            }
            const auto& message = *decoded.message;
            if (message.isDataSet()) {
                ++m_dataSetsReceived;
                if (m_acceptWrites) {
                    m_memory.addDataSet(message);
                    if (m_corruptPending && m_memory.contains(m_corruptAddress)) {
                        m_memory.write(m_corruptAddress, ByteVector{m_corruptValue});
                    }
                }
                continue;
            }
            if (!m_answerRequests || m_answered >= m_maxAnswers) {
                continue;
            }
            ++m_answered;
            const auto bytes = m_memory.read(message.address(), message.size().value());
            if (!bytes) {
                continue; // an unreadable range simply gets no answer, like a silent device
            }
            const auto whole = RolandSysExMessage::dataSet(message.deviceId(), message.modelId(), message.address(), *bytes);
            for (auto& chunk : protocol::chunkDataSet(*whole, 128)) {
                replies.push_back(std::move(chunk));
            }
        }
        transport.clearSentMessages();
        return replies;
    }

private:
    MemoryImage m_memory;
    bool m_acceptWrites = true;
    bool m_answerRequests = true;
    std::size_t m_dataSetsReceived = 0;
    std::size_t m_maxAnswers = std::numeric_limits<std::size_t>::max();
    std::size_t m_answered = 0;
    RolandAddress m_corruptAddress;
    Byte m_corruptValue = 0;
    bool m_corruptPending = false;
};

MemoryImage fixtureImage()
{
    std::ifstream in(XP60STUDIO_FIXTURE_DIR "/user-bank-amal.syx", std::ios::binary);
    const ByteVector data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::vector<RolandModelId> models{xp60::modelId()};
    return imageFromStream(parseSysExStream(data, models));
}

const RolandAddress kTemp = Xp60PatchLayout::temporaryPatchAddress();

// Puts a real patch from the fixture into the temporary area.
MemoryImage temporaryAreaWith(int userPatchNumber)
{
    const auto bank = fixtureImage();
    const auto source = *Xp60PatchLayout::userPatchAddress(userPatchNumber);
    MemoryImage image;
    for (const auto& block : Xp60PatchLayout::blocks()) {
        image.write(*kTemp.plus(block.offset), *bank.read(*source.plus(block.offset), block.size));
    }
    return image;
}

Xp60Patch patchFrom(const MemoryImage& image, const RolandAddress& base)
{
    return *Xp60PatchCodec::decode(image, base).patch;
}

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

        Fixture g;
        g.establishReadVerified();
        QVERIFY(g.transfer->arm());
        QVERIFY(g.transfer->writeAndVerifyTemporaryPatch(patchFrom(fixtureImage(), *Xp60PatchLayout::userPatchAddress(34))));
        g.session->disconnectEndpoints();
        QCOMPARE(g.transfer->state(), State::Failed);
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
