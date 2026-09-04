#include "midi/LoopbackMidiTransport.h"
#include "roland/HexFormat.h"
#include "roland/RolandCodec.h"
#include "services/DeviceSession.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/BlockCodec.h"
#include "xpmodel/Xp60PatchCodec.h"

#include <QSignalSpy>
#include <QtTest>

#include <chrono>
#include <memory>
#include <random>

using namespace xp60studio;
using namespace xp60studio::roland;
using namespace std::chrono_literals;

namespace {

struct FakeClock
{
    protocol::TimePoint now{std::chrono::duration_cast<protocol::Clock::duration>(1000ms)};
    void advance(std::chrono::milliseconds ms) { now += std::chrono::duration_cast<protocol::Clock::duration>(ms); }
};

struct Fixture
{
    midi::LoopbackMidiTransport* transport = nullptr;
    std::unique_ptr<services::DeviceSession> session;
    FakeClock clock;

    Fixture()
    {
        auto loopback = std::make_unique<midi::LoopbackMidiTransport>();
        loopback->addInput("in-1", "XP-60 IN");
        loopback->addOutput("out-1", "XP-60 OUT");
        loopback->addInput("in-2", "Other IN");
        transport = loopback.get();
        session = std::make_unique<services::DeviceSession>(std::move(loopback));
        session->setAutomaticTimeoutPolling(false);
        session->setClocks([this] { return clock.now; }, [] { return std::chrono::system_clock::time_point(std::chrono::seconds(1'700'000'000)); });
        protocol::TransferPacing pacing = session->pacing();
        pacing.interMessageDelay = 0ms; // deterministic: send immediately
        session->setPacing(pacing);
    }

    bool connectDefault() { return session->connectEndpoints("in-1", "out-1"); }

    // Feeds a complete DT1 as the device would answer, then drains the queued
    // cross-thread handoff.
    void deviceReplies(const RolandSysExMessage& message)
    {
        const auto bytes = message.encode();
        transport->injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
        QCoreApplication::processEvents();
    }

    void deviceSendsRaw(const ByteVector& bytes)
    {
        transport->injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
        QCoreApplication::processEvents();
    }
};

const RolandAddress kTempPatch(0x03, 0, 0, 0);

} // namespace

class DeviceSessionTest : public QObject
{
    Q_OBJECT

private slots:
    void cancellingAnAsyncOpenClosesBothPortsWithoutSending()
    {
        Fixture f;
        f.session->connectEndpointsAsync("in-1", "out-1");
        QCOMPARE(f.session->connectionState(), services::DeviceSession::ConnectionState::Connecting);
        f.session->disconnectEndpoints();
        QTRY_COMPARE(f.session->connectionState(), services::DeviceSession::ConnectionState::Disconnected);
        QVERIFY(!f.transport->isInputOpen());
        QVERIFY(!f.transport->isOutputOpen());
        QVERIFY(f.transport->sentMessages().empty());
    }

    void failureOfOutputOpenClosesTheInput()
    {
        Fixture f;
        f.session->connectEndpointsAsync("in-1", "missing");
        QTRY_COMPARE(f.session->connectionState(), services::DeviceSession::ConnectionState::Error);
        QVERIFY(!f.transport->isInputOpen());
        QVERIFY(!f.transport->isOutputOpen());
    }

    void staleQueuedReplyCannotValidateAReconnectedSession()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        QVERIFY(f.session->testConnection());
        const auto bytes = RolandSysExMessage::dataSet(RolandDeviceId::factoryDefault(), xp60::modelId(),
            kTempPatch, ByteVector(12, 'A'))->encode();
        f.transport->injectIncoming(bytes); // delivery queued for the old connection
        f.session->disconnectEndpoints();
        QVERIFY(f.connectDefault());
        QVERIFY(f.session->testConnection());
        QCoreApplication::processEvents();
        QCOMPARE(f.session->linkState(), services::DeviceSession::LinkState::Checking);
        QVERIFY(f.session->tracker().hasOutstanding());
        f.deviceSendsRaw(bytes);
        QCOMPARE(f.session->linkState(), services::DeviceSession::LinkState::Responding);
    }

    void wrongDeviceCannotPassConnectionTestAndTimeoutIsActionable()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        QVERIFY(f.session->testConnection());
        f.deviceReplies(RolandSysExMessage::dataSet(*RolandDeviceId::fromDisplayNumber(18), xp60::modelId(),
            kTempPatch, ByteVector(12, 'A')).value());
        QCOMPARE(f.session->linkState(), services::DeviceSession::LinkState::Checking);
        f.clock.advance(2000ms);
        f.session->pollTimeouts();
        QCOMPARE(f.session->linkState(), services::DeviceSession::LinkState::Failed);
        QVERIFY(f.session->linkMessage().find("Device ID") != std::string::npos);
    }

    void refreshLossCancelsQueuedWritesAndReconnectDoesNotReplay()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        auto pacing = f.session->pacing();
        pacing.interMessageDelay = 100ms;
        f.session->setPacing(pacing);
        const auto dt1 = RolandSysExMessage::dataSet(RolandDeviceId::factoryDefault(), xp60::modelId(),
            kTempPatch, ByteVector(12, 'A')).value();
        QSignalSpy batches(f.session.get(), &services::DeviceSession::dataSetBatchFinished);
        f.session->sendDataSets({dt1, dt1, dt1});
        QCoreApplication::processEvents();
        QCOMPARE(f.transport->sentMessages().size(), std::size_t(1));
        f.transport->setOutputs({});
        f.session->refreshEndpoints(); // manual discovery also detects removal
        QCOMPARE(f.session->connectionState(), services::DeviceSession::ConnectionState::Error);
        QCOMPARE(batches.count(), 1);
        QCOMPARE(batches.front()[1].toBool(), false);
        QVERIFY(!f.transport->isInputOpen());
        f.transport->addOutput("out-1", "XP-60 OUT");
        f.session->refreshEndpoints();
        QVERIFY(f.connectDefault());
        f.clock.advance(1000ms);
        QTest::qWait(150);
        QCOMPARE(f.transport->sentMessages().size(), std::size_t(1));
    }

    void backendErrorEndsConnectionAndCancelsRequests()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        QVERIFY(f.session->testConnection());
        f.transport->simulateError(midi::TransportError::make(midi::TransportErrorCode::Internal, "Wireless link lost"));
        QCoreApplication::processEvents();
        QCOMPARE(f.session->connectionState(), services::DeviceSession::ConnectionState::Error);
        QVERIFY(!f.session->tracker().hasOutstanding());
        QVERIFY(!f.transport->isInputOpen());
        QVERIFY(!f.transport->isOutputOpen());
        QCOMPARE(f.session->linkState(), services::DeviceSession::LinkState::Unchecked);
    }

    void enumeratesEndpointsFromTransport()
    {
        Fixture f;
        QCOMPARE(f.session->inputs().size(), std::size_t(2));
        QCOMPARE(f.session->outputs().size(), std::size_t(1));
        QCOMPARE(f.session->connectionState(), services::DeviceSession::ConnectionState::Disconnected);
        QCOMPARE(f.session->deviceId().displayNumber(), 17);
        QCOMPARE(f.session->modelId(), xp60::modelId());
    }

    void connectAndDisconnect()
    {
        Fixture f;
        QSignalSpy stateSpy(f.session.get(), &services::DeviceSession::connectionStateChanged);
        QVERIFY(f.connectDefault());
        QCOMPARE(f.session->connectionState(), services::DeviceSession::ConnectionState::Connected);
        QVERIFY(f.transport->isInputOpen());
        QVERIFY(f.transport->isOutputOpen());
        QCOMPARE(QString::fromStdString(f.session->connectedInput()->displayName), QStringLiteral("XP-60 IN"));
        QVERIFY(stateSpy.count() >= 2); // Connecting -> Connected

        f.session->disconnectEndpoints();
        QCOMPARE(f.session->connectionState(), services::DeviceSession::ConnectionState::Disconnected);
        QVERIFY(!f.transport->isInputOpen());
        QVERIFY(!f.transport->isOutputOpen());
    }

    void connectFailureIsReportedNotThrown()
    {
        Fixture f;
        QVERIFY(!f.session->connectEndpoints("missing", "out-1"));
        QCOMPARE(f.session->connectionState(), services::DeviceSession::ConnectionState::Error);
        QVERIFY(!f.session->lastError().empty());
        QCOMPARE(f.session->statistics().transportErrors, std::uint64_t(1));

        f.transport->failNextOpen(midi::TransportError::make(midi::TransportErrorCode::OpenFailed, "busy"));
        QVERIFY(!f.session->connectEndpoints("in-1", "out-1"));
        QCOMPARE(f.session->lastError(), std::string("busy"));
        QVERIFY(!f.transport->isInputOpen());
    }

    void requestIsEncodedSentAndCorrelated()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        QSignalSpy opSpy(f.session.get(), &services::DeviceSession::operationChanged);

        const auto id = f.session->sendDataRequest(kTempPatch, RolandSize(0, 0, 0, 0x0C));
        QVERIFY(id.isValid());
        QCOMPARE(f.transport->sentMessages().size(), std::size_t(1));
        QCOMPARE(QString::fromStdString(toHex(f.transport->sentMessages()[0])),
                 QStringLiteral("F0 41 10 6A 11 03 00 00 00 00 00 00 0C 71 F7"));
        QCOMPARE(f.session->tracker().find(id)->state, protocol::RequestState::AwaitingData);
        QCOMPARE(f.session->statistics().requestsSent, std::uint64_t(1));
        QCOMPARE(f.session->statistics().sysExOut, std::uint64_t(1));

        // The device answers with the 12-byte patch name.
        const ByteVector name{'W', 'a', 'r', 'm', ' ', 'O', 'r', 'c', 'h', 'e', 's', 't'};
        f.deviceReplies(RolandSysExMessage::dataSet(RolandDeviceId::factoryDefault(), xp60::modelId(), kTempPatch, name).value());

        const auto* op = f.session->tracker().find(id);
        QCOMPARE(op->state, protocol::RequestState::Completed);
        QCOMPARE(op->data, name);
        QCOMPARE(f.session->statistics().requestsCompleted, std::uint64_t(1));
        QCOMPARE(f.session->statistics().sysExIn, std::uint64_t(1));
        QCOMPARE(f.session->statistics().rolandMessagesIn, std::uint64_t(1));
        QVERIFY(opSpy.count() >= 3); // queued, sent, completed

        // Log contains the OUT RQ1 and the IN DT1 lines.
        bool sawOut = false;
        bool sawIn = false;
        for (const auto& entry : f.session->log()) {
            if (entry.direction == diagnostics::LogDirection::Out && entry.kind == diagnostics::LogKind::RolandDataRequest) {
                sawOut = true;
                QCOMPARE(entry.requestId.value(), id.value);
            }
            if (entry.direction == diagnostics::LogDirection::In && entry.kind == diagnostics::LogKind::RolandDataSet) {
                sawIn = true;
                QCOMPARE(entry.requestId.value(), id.value);
                QCOMPARE(entry.checksum, diagnostics::ChecksumStatus::Valid);
            }
        }
        QVERIFY(sawOut);
        QVERIFY(sawIn);
    }

    void chunkedResponseArrivingInFragmentsCompletes()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        const RolandAddress user(0x11, 0, 0, 0);
        const auto id = f.session->sendDataRequest(user, RolandSize::fromValue(300).value());

        // The XP-60 splits data into packets of at most 128 bytes: 300 = 128 + 128 + 44.
        const auto chunk1 = RolandSysExMessage::dataSet(RolandDeviceId::factoryDefault(), xp60::modelId(), user, ByteVector(128, 0x11)).value().encode();
        const auto chunk2 = RolandSysExMessage::dataSet(RolandDeviceId::factoryDefault(), xp60::modelId(), *user.plus(128), ByteVector(128, 0x11)).value().encode();
        const auto chunk3 = RolandSysExMessage::dataSet(RolandDeviceId::factoryDefault(), xp60::modelId(), *user.plus(256), ByteVector(44, 0x22)).value().encode();
        // Deliver the first chunk in three fragments, as a backend might.
        f.deviceSendsRaw(ByteVector(chunk1.begin(), chunk1.begin() + 10));
        QCOMPARE(f.session->tracker().find(id)->state, protocol::RequestState::AwaitingData);
        f.deviceSendsRaw(ByteVector(chunk1.begin() + 10, chunk1.begin() + 100));
        f.deviceSendsRaw(ByteVector(chunk1.begin() + 100, chunk1.end()));
        QCOMPARE(f.session->tracker().find(id)->state, protocol::RequestState::Receiving);
        QCOMPARE(f.session->tracker().find(id)->receivedBytes, 128u);
        f.deviceSendsRaw(chunk2);
        QCOMPARE(f.session->tracker().find(id)->receivedBytes, 256u);
        f.deviceSendsRaw(chunk3);
        QCOMPARE(f.session->tracker().find(id)->state, protocol::RequestState::Completed);
        QCOMPARE(f.session->tracker().find(id)->data[299], Byte(0x22));
    }

    void checksumFailureIsCountedAndLogged()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        const auto id = f.session->sendDataRequest(kTempPatch, RolandSize(0, 0, 0, 4));
        auto bytes = RolandSysExMessage::dataSet(RolandDeviceId::factoryDefault(), xp60::modelId(), kTempPatch, ByteVector{1, 2, 3, 4}).value().encode();
        bytes[bytes.size() - 2] ^= 0x01; // corrupt checksum
        f.deviceSendsRaw(bytes);
        QCOMPARE(f.session->statistics().checksumFailures, std::uint64_t(1));
        QCOMPARE(f.session->statistics().parseFailures, std::uint64_t(1));
        QCOMPARE(f.session->tracker().find(id)->state, protocol::RequestState::AwaitingData); // still waiting
        const auto& last = f.session->log().back();
        QCOMPARE(last.kind, diagnostics::LogKind::RolandInvalid);
        QCOMPARE(last.checksum, diagnostics::ChecksumStatus::Invalid);
        QCOMPARE(last.severity, diagnostics::LogSeverity::Error);
    }

    void timeoutViaInjectedClock()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        const auto id = f.session->sendDataRequest(kTempPatch, RolandSize(0, 0, 0, 0x0C));
        f.clock.advance(1499ms);
        f.session->pollTimeouts();
        QCOMPARE(f.session->tracker().find(id)->state, protocol::RequestState::AwaitingData);
        f.clock.advance(2ms);
        f.session->pollTimeouts();
        QCOMPARE(f.session->tracker().find(id)->state, protocol::RequestState::TimedOut);
        QCOMPARE(f.session->statistics().requestsTimedOut, std::uint64_t(1));
        const auto& last = f.session->log().back();
        QCOMPARE(last.severity, diagnostics::LogSeverity::Error);
        QVERIFY(last.summary.find("timed out") != std::string::npos);
        QVERIFY(last.detail.find("XP-60 IN") != std::string::npos); // actionable: names the endpoints
    }

    void cancellation()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        const auto id = f.session->sendDataRequest(kTempPatch, RolandSize(0, 0, 0, 0x0C));
        QVERIFY(f.session->cancelRequest(id));
        QCOMPARE(f.session->tracker().find(id)->state, protocol::RequestState::Cancelled);
        QCOMPARE(f.session->statistics().requestsCancelled, std::uint64_t(1));
        // Data arriving afterwards is unsolicited, not matched.
        f.deviceReplies(RolandSysExMessage::dataSet(RolandDeviceId::factoryDefault(), xp60::modelId(), kTempPatch, ByteVector(12, 0)).value());
        QCOMPARE(f.session->statistics().unsolicitedDataSets, std::uint64_t(1));
        QCOMPARE(f.session->tracker().find(id)->state, protocol::RequestState::Cancelled);
    }

    void disconnectCancelsOutstanding()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        const auto id = f.session->sendDataRequest(kTempPatch, RolandSize(0, 0, 0, 0x0C));
        f.session->disconnectEndpoints();
        QCOMPARE(f.session->tracker().find(id)->state, protocol::RequestState::Cancelled);
        QVERIFY(!f.session->tracker().hasOutstanding());
    }

    void requestsAreRefusedWhenDisconnected()
    {
        Fixture f;
        const auto id = f.session->sendDataRequest(kTempPatch, RolandSize(0, 0, 0, 0x0C));
        QVERIFY(!id.isValid());
        QVERIFY(f.transport->sentMessages().empty());
        QCOMPARE(f.session->log().back().severity, diagnostics::LogSeverity::Warning);
    }

    void pacingDelaysSecondMessage()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        auto pacing = f.session->pacing();
        pacing.interMessageDelay = 20ms;
        f.session->setPacing(pacing);

        f.session->sendDataRequest(kTempPatch, RolandSize(0, 0, 0, 1));
        f.session->sendDataRequest(kTempPatch, RolandSize(0, 0, 0, 2));
        QCOMPARE(f.transport->sentMessages().size(), std::size_t(1));
        QCOMPARE(f.session->pendingSendCount(), std::size_t(1));
        f.clock.advance(20ms);
        f.session->pumpSendQueue();
        QCOMPARE(f.transport->sentMessages().size(), std::size_t(2));
        QCOMPARE(f.session->pendingSendCount(), std::size_t(0));
    }

    void sendFailureFailsTheRequest()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        f.transport->failNextSend(midi::TransportError::make(midi::TransportErrorCode::SendFailed, "cable"));
        const auto id = f.session->sendDataRequest(kTempPatch, RolandSize(0, 0, 0, 1));
        QCOMPARE(f.session->tracker().find(id)->state, protocol::RequestState::FailedValidation);
        QVERIFY(f.session->tracker().find(id)->failureReason.find("cable") != std::string::npos);
        QCOMPARE(f.session->statistics().requestsFailed, std::uint64_t(1));
    }

    void ordinaryMidiAndForeignSysExAreLoggedNotDropped()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        f.deviceSendsRaw(ByteVector{0x90, 0x3C, 0x64});
        QCOMPARE(f.session->log().back().kind, diagnostics::LogKind::ChannelMessage);
        f.deviceSendsRaw(ByteVector{0xF0, 0x43, 0x10, 0x4C, 0xF7});
        QCOMPARE(f.session->log().back().kind, diagnostics::LogKind::OtherSysEx);
        QCOMPARE(f.session->statistics().messagesIn, std::uint64_t(2));
        QCOMPARE(f.session->statistics().parseFailures, std::uint64_t(0)); // foreign SysEx is not a Roland failure
    }

    void endpointRemovalWhileConnectedBecomesError()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        f.session->sendDataRequest(kTempPatch, RolandSize(0, 0, 0, 1));
        f.transport->setInputs({});
        f.transport->simulateEndpointsChanged();
        QCoreApplication::processEvents();
        QCOMPARE(f.session->connectionState(), services::DeviceSession::ConnectionState::Error);
        QVERIFY(!f.session->tracker().hasOutstanding());
        QVERIFY(f.session->inputs().empty());
    }

    void deviceIdIsUsedInRequests()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        f.session->setDeviceId(RolandDeviceId::fromDisplayNumber(20).value());
        f.session->sendDataRequest(kTempPatch, RolandSize(0, 0, 0, 0x0C));
        QCOMPARE(f.transport->sentMessages()[0][2], Byte(0x13));
    }

    void patchFetchReadsFiveBlocksAndDecodes()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        QSignalSpy fetchSpy(f.session.get(), &services::DeviceSession::patchFetchChanged);
        QVERIFY(f.session->fetchTemporaryPatch());
        QCOMPARE(f.session->patchFetch().state, services::DeviceSession::PatchFetchState::InProgress);
        QCOMPARE(f.session->patchFetch().totalBlocks, std::size_t(5));
        QCOMPARE(f.transport->sentMessages().size(), std::size_t(5)); // pacing 0 ms -> all sent
        QVERIFY(!f.session->fetchTemporaryPatch()); // one at a time

        // The RQ1s follow the Parameter Address Map blocks.
        QCOMPARE(QString::fromStdString(toHex(f.transport->sentMessages()[0])),
                 QStringLiteral("F0 41 10 6A 11 03 00 00 00 00 00 00 49 34 F7"));
        QCOMPARE(QString::fromStdString(toHex(f.transport->sentMessages()[1])),
                 QStringLiteral("F0 41 10 6A 11 03 00 10 00 00 00 01 01 6B F7"));

        // Build a synthetic patch and answer block by block, tones in 128 + 1 packets.
        const auto base = xpmodel::Xp60PatchLayout::temporaryPatchAddress();
        std::mt19937 rng(11);
        xpmodel::MemoryImage image;
        for (const auto& block : xpmodel::Xp60PatchLayout::blocks()) {
            ByteVector bytes(block.size, 0);
            for (const auto& p : block.table->parameters()) {
                std::uniform_int_distribution<int> dist(p.rawMin, p.rawMax);
                xpmodel::BlockCodec::writeRaw(p, dist(rng), bytes);
            }
            if (!block.tone) {
                const auto name = xpmodel::PatchName::fromText("Warm Orchest")->bytes();
                std::copy(name.begin(), name.end(), bytes.begin());
            }
            image.write(*base.plus(block.offset), bytes);
        }
        const auto patch = *xpmodel::Xp60PatchCodec::decode(image, base).patch;
        const auto replies = xpmodel::Xp60PatchCodec::encodeToDataSets(patch, RolandDeviceId::factoryDefault(), xp60::modelId(), base, 128);
        QCOMPARE(replies.size(), std::size_t(9));
        for (std::size_t i = 0; i < replies.size(); ++i) {
            f.deviceReplies(replies[i]);
            if (i + 1 < replies.size()) {
                QCOMPARE(f.session->patchFetch().state, services::DeviceSession::PatchFetchState::InProgress);
            }
        }
        const auto& fetch = f.session->patchFetch();
        QCOMPARE(fetch.state, services::DeviceSession::PatchFetchState::Completed);
        QCOMPARE(fetch.completedBlocks, std::size_t(5));
        QVERIFY(fetch.patch.has_value());
        QCOMPARE(fetch.patch->name().text(), std::string("Warm Orchest"));
        QVERIFY(*fetch.patch == patch); // byte-exact through the whole pipeline
        QVERIFY(fetch.message.find("Patch decoded") != std::string::npos);
        QVERIFY(fetchSpy.count() >= 3);
        QCOMPARE(f.session->statistics().requestsCompleted, std::uint64_t(5));

        // A second fetch is allowed once the first finished.
        QVERIFY(f.session->fetchTemporaryPatch());
        QCOMPARE(f.session->patchFetch().state, services::DeviceSession::PatchFetchState::InProgress);
    }

    void patchFetchFailsWhenABlockTimesOut()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        QVERIFY(f.session->fetchTemporaryPatch());
        f.clock.advance(1600ms);
        f.session->pollTimeouts();
        QCOMPARE(f.session->patchFetch().state, services::DeviceSession::PatchFetchState::Failed);
        QVERIFY(f.session->patchFetch().message.find("Timed out") != std::string::npos);
        QVERIFY(!f.session->patchFetch().patch.has_value());
        QVERIFY(!f.session->tracker().hasOutstanding()); // remaining blocks timed out too
    }

    void patchFetchFailsImmediatelyWhenABlockCannotBeSent()
    {
        Fixture f;
        QVERIFY(f.connectDefault());
        // The first block read fails at the transport, closing the connection.
        f.session->sendDataRequest(kTempPatch, RolandSize(0, 0, 0, 1));
        f.transport->clearSentMessages();
        f.session->cancelAllRequests();
        f.transport->failNextSend(midi::TransportError::make(midi::TransportErrorCode::SendFailed, "cable"));
        QVERIFY(!f.session->fetchTemporaryPatch());
        // The remaining blocks cannot be queued after a transport failure.
        QCOMPARE(f.session->patchFetch().state, services::DeviceSession::PatchFetchState::Failed);
        QVERIFY(f.session->patchFetch().message.find("cable") != std::string::npos);
        QVERIFY(!f.session->tracker().hasOutstanding()); // the other blocks were cancelled
    }

    void patchFetchRequiresConnection()
    {
        Fixture f;
        QVERIFY(!f.session->fetchTemporaryPatch());
        QCOMPARE(f.session->patchFetch().state, services::DeviceSession::PatchFetchState::Idle);
        QVERIFY(f.connectDefault());
        QVERIFY(f.session->fetchTemporaryPatch());
        f.session->cancelPatchFetch();
        QCOMPARE(f.session->patchFetch().state, services::DeviceSession::PatchFetchState::Failed);
        QVERIFY(!f.session->tracker().hasOutstanding());
        f.session->disconnectEndpoints();
    }

    void logIsBounded()
    {
        Fixture f;
        f.session->setLogLimit(5);
        QVERIFY(f.connectDefault());
        for (int i = 0; i < 10; ++i) {
            f.deviceSendsRaw(ByteVector{0xF8});
        }
        QCOMPARE(f.session->log().size(), std::size_t(5));
        QSignalSpy cleared(f.session.get(), &services::DeviceSession::logCleared);
        f.session->clearLog();
        QVERIFY(f.session->log().empty());
        QCOMPARE(cleared.count(), 1);
    }
};

QTEST_GUILESS_MAIN(DeviceSessionTest)
#include "tst_device_session.moc"
