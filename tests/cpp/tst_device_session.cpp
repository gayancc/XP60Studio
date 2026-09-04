#include "midi/LoopbackMidiTransport.h"
#include "roland/HexFormat.h"
#include "roland/RolandCodec.h"
#include "services/DeviceSession.h"
#include "xp60/Xp60Device.h"

#include <QSignalSpy>
#include <QtTest>

#include <chrono>
#include <memory>

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
