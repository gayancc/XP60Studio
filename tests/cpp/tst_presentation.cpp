#include "midi/LoopbackMidiTransport.h"
#include "presentation/AppShellViewModel.h"
#include "presentation/DevicesViewModel.h"
#include "presentation/MidiEndpointListModel.h"
#include "presentation/ProtocolLogModel.h"
#include "roland/RolandSysExMessage.h"
#include "services/DeviceSession.h"
#include "services/PatchTransfer.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/BlockCodec.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QtTest>

#include <chrono>
#include <memory>

using namespace xp60studio;
using namespace xp60studio::presentation;
using namespace std::chrono_literals;

namespace {

struct Fixture
{
    midi::LoopbackMidiTransport* transport = nullptr;
    std::unique_ptr<services::DeviceSession> session;
    std::unique_ptr<services::PatchTransfer> transfer;
    std::unique_ptr<DevicesViewModel> devices;
    std::unique_ptr<AppShellViewModel> shell;
    protocol::TimePoint now{std::chrono::duration_cast<protocol::Clock::duration>(1000ms)};

    explicit Fixture(bool withEndpoints = true)
    {
        auto loopback = std::make_unique<midi::LoopbackMidiTransport>();
        if (withEndpoints) {
            loopback->addInput("in-1", "XP-60 IN");
            loopback->addOutput("out-1", "XP-60 OUT");
        }
        transport = loopback.get();
        session = std::make_unique<services::DeviceSession>(std::move(loopback));
        session->setAutomaticTimeoutPolling(false);
        session->setClocks([this] { return now; }, {});
        auto pacing = session->pacing();
        pacing.interMessageDelay = 0ms;
        session->setPacing(pacing);
        transfer = std::make_unique<services::PatchTransfer>(*session);
        devices = std::make_unique<DevicesViewModel>(*session, transfer.get());
        shell = std::make_unique<AppShellViewModel>(devices.get());
    }

    void deviceReplies(const roland::RolandSysExMessage& message)
    {
        const auto bytes = message.encode();
        transport->injectIncoming(midi::MidiByteSpan(bytes.data(), bytes.size()));
        QCoreApplication::processEvents();
    }
};

} // namespace

class PresentationTest : public QObject
{
    Q_OBJECT

private slots:
    void portSelectionSurvivesReorderAndRefusesMissingOrAmbiguousPorts()
    {
        Fixture f;
        const auto input = f.session->inputs().front();
        const auto output = f.session->outputs().front();
        auto otherInput = input; otherInput.id = "other-in"; otherInput.displayName = "Other";
        auto otherOutput = output; otherOutput.id = "other-out"; otherOutput.displayName = "Other";
        f.transport->setInputs({otherInput, input});
        f.transport->setOutputs({otherOutput, output});
        f.devices->refreshEndpoints();
        QCOMPARE(f.devices->selectedInputIndex(), 1);
        QCOMPARE(f.devices->selectedOutputIndex(), 1);

        f.transport->setInputs({otherInput});
        f.devices->refreshEndpoints();
        QCOMPARE(f.devices->selectedInputIndex(), -1);
        QVERIFY(!f.devices->canConnect());
        QVERIFY(f.devices->selectionMessage().contains("unavailable"));

        auto replug = input; replug.id = "new-handle";
        f.transport->setInputs({otherInput, replug});
        f.devices->refreshEndpoints();
        QCOMPARE(f.devices->selectedInputIndex(), 1);
        auto duplicate = replug; duplicate.id = "duplicate";
        f.transport->setInputs({replug, duplicate});
        f.devices->refreshEndpoints();
        QCOMPARE(f.devices->selectedInputIndex(), -1);
        QVERIFY(!f.devices->canConnect());
        QVERIFY(f.transport->sentMessages().empty());
    }

    void multiplePortsRequireAnExplicitSelection()
    {
        Fixture f(false);
        f.transport->addInput("a", "A");
        f.transport->addInput("b", "B");
        f.transport->addOutput("c", "C");
        f.transport->addOutput("d", "D");
        f.devices->refreshEndpoints();
        QCOMPARE(f.devices->selectedInputIndex(), -1);
        QCOMPARE(f.devices->selectedOutputIndex(), -1);
        QVERIFY(!f.devices->canConnect());
        f.devices->setSelectedInputIndex(1);
        f.devices->setSelectedOutputIndex(0);
        QVERIFY(f.devices->canConnect());
    }

    void connectionPreferencesPersistWithoutOpeningPorts()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("connections.ini");
        {
            QSettings settings(path, QSettings::IniFormat);
            Fixture f;
            f.devices->useConnectionSettings(&settings);
            f.transport->addInput("saved-in", "Wireless input");
            f.transport->addOutput("saved-out", "Wireless output");
            f.devices->refreshEndpoints();
            f.devices->setSelectedInputIndex(1);
            f.devices->setSelectedOutputIndex(1);
            f.devices->setDeviceId(23);
            f.devices->setPacingProfile(1);
            settings.sync();
        }
        QSettings settings(path, QSettings::IniFormat);
        Fixture f;
        f.transport->addInput("saved-in", "Wireless input");
        f.transport->addOutput("saved-out", "Wireless output");
        f.devices->refreshEndpoints();
        f.devices->useConnectionSettings(&settings);
        QCOMPARE(f.devices->deviceId(), 23);
        QCOMPARE(f.devices->pacingProfile(), 1);
        QCOMPARE(f.session->pacing().maxDataSetPayloadBytes, std::size_t(64));
        QCOMPARE(f.session->pacing().timeouts.firstResponse, 5000ms);
        QCOMPARE(f.devices->selectedInputIndex(), 1);
        QCOMPARE(f.devices->selectedOutputIndex(), 1);
        QCOMPARE(f.devices->connectionState(), ConnectionState::Disconnected);
        QVERIFY(!f.transport->isInputOpen());
        QVERIFY(f.transport->sentMessages().empty());
    }

    void readOnlyConnectionTestUpdatesShellOnlyAfterValidReply()
    {
        Fixture f;
        f.devices->connectDevice();
        QTRY_COMPARE(f.devices->connectionState(), ConnectionState::Connected);
        QVERIFY(!f.devices->connectionVerified());
        QVERIFY(f.devices->canTestConnection());
        f.devices->testConnection();
        QVERIFY(!f.devices->canTestConnection());
        QVERIFY(!f.devices->connectionVerified());
        f.deviceReplies(roland::RolandSysExMessage::dataSet(roland::RolandDeviceId::factoryDefault(), xp60::modelId(),
            roland::RolandAddress(3, 0, 0, 0), roland::ByteVector(12, 'A')).value());
        QVERIFY(f.devices->connectionVerified());
        QCOMPARE(f.shell->connectionLabel(), QStringLiteral("XP-60 LIVE"));
        QCOMPARE(f.transport->sentMessages().size(), std::size_t(1));
        QCOMPARE(f.transport->sentMessages().front()[4], std::uint8_t(0x11)); // RQ1 only
        f.devices->setDeviceId(18);
        QVERIFY(!f.devices->connectionVerified());
    }

    void endpointModelsAreValidAndSelectionDefaults()
    {
        Fixture f;
        QAbstractItemModelTester inputsTester(f.devices->inputs(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        QAbstractItemModelTester outputsTester(f.devices->outputs(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(f.devices->inputs()->rowCount(), 1);
        QCOMPARE(f.devices->outputs()->rowCount(), 1);
        QCOMPARE(f.devices->selectedInputIndex(), 0);
        QCOMPARE(f.devices->selectedOutputIndex(), 0);
        QVERIFY(f.devices->hasEndpoints());
        QCOMPARE(f.devices->inputs()->data(f.devices->inputs()->index(0, 0), MidiEndpointListModel::DisplayNameRole).toString(),
                 QStringLiteral("XP-60 IN"));
        QCOMPARE(qobject_cast<MidiEndpointListModel*>(f.devices->inputs())->indexOfId(QStringLiteral("in-1")), 0);
        QCOMPARE(qobject_cast<MidiEndpointListModel*>(f.devices->inputs())->indexOfId(QStringLiteral("nope")), -1);
    }

    void connectEnablementFollowsSelectionAndState()
    {
        Fixture f;
        QVERIFY(f.devices->canConnect());
        QVERIFY(!f.devices->canDisconnect());
        QCOMPARE(f.devices->connectionState(), ConnectionState::Disconnected);
        QCOMPARE(f.shell->connectionLabel(), QStringLiteral("Offline"));

        f.devices->setSelectedInputIndex(-1);
        QVERIFY(!f.devices->canConnect());
        f.devices->setSelectedInputIndex(0);

        QSignalSpy connectionSpy(f.devices.get(), &DevicesViewModel::connectionChanged);
        QSignalSpy shellSpy(f.shell.get(), &AppShellViewModel::connectionChanged);
        f.devices->connectDevice();
        QTRY_COMPARE(f.devices->connectionState(), ConnectionState::Connected);
        QCOMPARE(f.devices->connectionState(), ConnectionState::Connected);
        QCOMPARE(f.devices->connectionStateText(), QStringLiteral("Connected"));
        QVERIFY(f.devices->connectionDetail().contains(QStringLiteral("XP-60 IN")));
        QVERIFY(!f.devices->canConnect());
        QVERIFY(f.devices->canDisconnect());
        QVERIFY(connectionSpy.count() >= 1);
        QVERIFY(shellSpy.count() >= 1);
        QCOMPARE(f.shell->connectionLabel(), QStringLiteral("Connected"));
        QCOMPARE(f.shell->connectionState(), ConnectionState::Connected);

        f.devices->disconnectDevice();
        QCOMPARE(f.devices->connectionState(), ConnectionState::Disconnected);
        QVERIFY(f.devices->canConnect());
    }

    void noEndpointsMeansNoConnect()
    {
        Fixture f(false);
        QVERIFY(!f.devices->hasEndpoints());
        QVERIFY(!f.devices->canConnect());
        QCOMPARE(f.devices->selectedInputIndex(), -1);
        QVERIFY(f.devices->connectionDetail().contains(QStringLiteral("No MIDI devices found")));

        // Hot-plug: endpoints appear, selection defaults, connect becomes possible.
        f.transport->addInput("in-1", "XP-60 IN");
        f.transport->addOutput("out-1", "XP-60 OUT");
        f.transport->simulateEndpointsChanged();
        QCoreApplication::processEvents();
        QVERIFY(f.devices->hasEndpoints());
        QCOMPARE(f.devices->selectedInputIndex(), 0);
        QVERIFY(f.devices->canConnect());
    }

    void connectionErrorIsSurfaced()
    {
        Fixture f;
        f.transport->failNextOpen(midi::TransportError::make(midi::TransportErrorCode::OpenFailed, "port busy"));
        f.devices->connectDevice();
        QTRY_COMPARE(f.devices->connectionState(), ConnectionState::Error);
        QCOMPARE(f.devices->lastError(), QStringLiteral("port busy"));
        QCOMPARE(f.devices->connectionDetail(), QStringLiteral("port busy"));
        QCOMPARE(f.shell->connectionLabel(), QStringLiteral("Connection error"));
        QVERIFY(f.devices->canConnect()); // user may retry
    }

    void deviceIdValidation()
    {
        Fixture f;
        QCOMPARE(f.devices->deviceId(), 17);
        QCOMPARE(f.devices->deviceIdMinimum(), 17);
        QCOMPARE(f.devices->deviceIdMaximum(), 32);
        QSignalSpy spy(f.devices.get(), &DevicesViewModel::deviceIdChanged);
        f.devices->setDeviceId(20);
        QCOMPARE(f.devices->deviceId(), 20);
        QCOMPARE(spy.count(), 1);
        f.devices->setDeviceId(99); // rejected, value unchanged, change re-announced
        QCOMPARE(f.devices->deviceId(), 20);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(f.devices->modelIdText(), QStringLiteral("6A"));
        QVERIFY(!f.devices->modelIdStatusText().isEmpty());
    }

    void requestFieldValidationAndPresets()
    {
        Fixture f;
        QVERIFY(!f.devices->readPresetNames().isEmpty());
        QCOMPARE(f.devices->selectedReadPresetIndex(), 0);
        QCOMPARE(f.devices->requestAddress(), QStringLiteral("03 00 00 00"));
        QCOMPARE(f.devices->requestSize(), QStringLiteral("00 00 00 0C"));
        QCOMPARE(f.devices->requestByteCount(), 12);
        QVERIFY(f.devices->requestAddressValid());
        QVERIFY(f.devices->requestSizeValid());
        QVERIFY(!f.devices->canSendRequest()); // not connected
        QVERIFY(f.devices->requestValidationMessage().contains(QStringLiteral("Connect")));

        QSignalSpy fields(f.devices.get(), &DevicesViewModel::requestFieldsChanged);
        f.devices->setRequestAddress(QStringLiteral("03 00 00 8C"));
        QVERIFY(!f.devices->requestAddressValid());
        QVERIFY(f.devices->requestValidationMessage().contains(QStringLiteral("Address")));
        QCOMPARE(fields.count(), 1);
        f.devices->setRequestAddress(QStringLiteral("03 00 00 00"));

        f.devices->setRequestSize(QStringLiteral("00 00 00 00"));
        QVERIFY(!f.devices->requestSizeValid());
        QVERIFY(f.devices->requestValidationMessage().contains(QStringLiteral("at least one byte")));
        f.devices->setRequestSize(QStringLiteral("7F 7F 7F 7F"));
        QVERIFY(!f.devices->requestSizeValid());
        f.devices->setRequestSize(QStringLiteral("bad"));
        QVERIFY(!f.devices->requestSizeValid());
        QCOMPARE(f.devices->requestByteCount(), 0);

        f.devices->applyReadPreset(1);
        QCOMPARE(f.devices->selectedReadPresetIndex(), 1);
        QCOMPARE(f.devices->requestAddress(), QStringLiteral("11 00 00 00"));
        QVERIFY(f.devices->requestSizeValid());
        QVERIFY(!f.devices->readPresetDescription().isEmpty());
        f.devices->applyReadPreset(99); // ignored
        QCOMPARE(f.devices->selectedReadPresetIndex(), 1);
    }

    void sendRequestFlowsThroughToModels()
    {
        Fixture f;
        QAbstractItemModelTester logTester(f.devices->log(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        QAbstractItemModelTester opsTester(f.devices->operations(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        f.devices->connectDevice();
        QTRY_COMPARE(f.devices->connectionState(), ConnectionState::Connected);
        QVERIFY(f.devices->canSendRequest());
        QSignalSpy canSend(f.devices.get(), &DevicesViewModel::canSendRequestChanged);
        QVERIFY(f.devices->sendRequest());
        QVERIFY(f.devices->hasOutstandingRequests());
        QCOMPARE(f.devices->operations()->rowCount(), 1);
        QCOMPARE(f.devices->operations()->data(f.devices->operations()->index(0, 0), RequestOperationModel::StateNameRole).toString(),
                 QStringLiteral("AwaitingData"));
        QCOMPARE(f.transport->sentMessages().size(), std::size_t(1));
        QCOMPARE(f.devices->messagesOut(), 1);
        QCOMPARE(f.devices->sysExHealthText(), QStringLiteral("Not tested yet"));

        const roland::ByteVector name{'W', 'a', 'r', 'm', ' ', 'O', 'r', 'c', 'h', 'e', 's', 't'};
        f.deviceReplies(roland::RolandSysExMessage::dataSet(roland::RolandDeviceId::factoryDefault(), xp60::modelId(),
                                                            roland::RolandAddress(0x03, 0, 0, 0), name).value());
        QVERIFY(!f.devices->hasOutstandingRequests());
        const auto index = f.devices->operations()->index(0, 0);
        QCOMPARE(f.devices->operations()->data(index, RequestOperationModel::StateNameRole).toString(), QStringLiteral("Completed"));
        QCOMPARE(f.devices->operations()->data(index, RequestOperationModel::IsSuccessRole).toBool(), true);
        QCOMPARE(f.devices->operations()->data(index, RequestOperationModel::ReceivedBytesRole).toInt(), 12);
        QCOMPARE(f.devices->operations()->data(index, RequestOperationModel::DataTextRole).toString(), QStringLiteral("Warm Orchest"));
        QCOMPARE(f.devices->operations()->data(index, RequestOperationModel::ProgressRole).toDouble(), 1.0);
        QCOMPARE(f.devices->requestsCompleted(), 1);
        QCOMPARE(f.devices->messagesIn(), 1);
        QCOMPARE(f.devices->sysExHealthText(), QStringLiteral("All good ✓"));
        QCOMPARE(f.devices->sysExHealthTone(), QStringLiteral("success"));

        // Log model has OUT and IN Roland lines.
        auto* log = f.devices->log();
        bool sawIn = false;
        for (int row = 0; row < log->rowCount(); ++row) {
            const auto idx = log->index(row, 0);
            if (log->data(idx, ProtocolLogModel::DirectionRole).toString() == QStringLiteral("IN")
                && log->data(idx, ProtocolLogModel::KindRole).toString() == QStringLiteral("RolandDataSet")) {
                sawIn = true;
                QCOMPARE(log->data(idx, ProtocolLogModel::ChecksumRole).toString(), QStringLiteral("OK"));
                QVERIFY(log->data(idx, ProtocolLogModel::LineRole).toString().contains(QStringLiteral("IN  Roland DT1")));
                QVERIFY(log->data(idx, ProtocolLogModel::IsRolandRole).toBool());
            }
        }
        QVERIFY(sawIn);

        f.devices->clearLog();
        QCOMPARE(log->rowCount(), 0);
    }

    void timeoutAndCancelReachTheViewModel()
    {
        Fixture f;
        f.devices->connectDevice();
        QTRY_COMPARE(f.devices->connectionState(), ConnectionState::Connected);
        QVERIFY(f.devices->sendRequest());
        f.now += std::chrono::duration_cast<protocol::Clock::duration>(2000ms);
        f.session->pollTimeouts();
        QCOMPARE(f.devices->timeouts(), 1);
        QCOMPARE(f.devices->sysExHealthText(), QStringLiteral("No reply from XP-60"));
        QCOMPARE(f.devices->sysExHealthTone(), QStringLiteral("error"));
        QVERIFY(!f.devices->hasOutstandingRequests());

        QVERIFY(f.devices->sendRequest());
        QVERIFY(f.devices->hasOutstandingRequests());
        f.devices->cancelAllRequests();
        QVERIFY(!f.devices->hasOutstandingRequests());
        QCOMPARE(f.devices->operations()->data(f.devices->operations()->index(0, 0), RequestOperationModel::StateNameRole).toString(),
                 QStringLiteral("Cancelled"));
    }

    void shellNavigationOnlyAllowsAvailableScreens()
    {
        Fixture f;
        QCOMPARE(f.shell->currentScreen(), QStringLiteral("devices"));
        QCOMPARE(f.shell->currentScreenTitle(), QStringLiteral("Devices"));
        QCOMPARE(f.shell->navigationItems().size(), 9);
        QVERIFY(f.shell->isScreenAvailable(QStringLiteral("devices")));
        QVERIFY(f.shell->isScreenAvailable(QStringLiteral("editor")));  // built in Phase 4
        QVERIFY(f.shell->isScreenAvailable(QStringLiteral("library"))); // built in Phase 5
        QVERIFY(f.shell->isScreenAvailable(QStringLiteral("banks")));   // Bank Builder
        QVERIFY(f.shell->isScreenAvailable(QStringLiteral("expansion"))); // Expansion Manager
        // Still unbuilt, and therefore still not navigable.
        QVERIFY(!f.shell->isScreenAvailable(QStringLiteral("performance")));
        QSignalSpy spy(f.shell.get(), &AppShellViewModel::currentScreenChanged);
        QVERIFY(!f.shell->navigate(QStringLiteral("performance")));
        QVERIFY(!f.shell->navigate(QStringLiteral("nonsense")));
        QCOMPARE(f.shell->currentScreen(), QStringLiteral("devices"));
        QCOMPARE(spy.count(), 0);
        QVERIFY(f.shell->navigate(QStringLiteral("editor")));
        QCOMPARE(f.shell->currentScreen(), QStringLiteral("editor"));
        QCOMPARE(spy.count(), 1);
        QVERIFY(f.shell->navigate(QStringLiteral("devices")));
        QCOMPARE(spy.count(), 2);
        QVERIFY(f.shell->navigate(QStringLiteral("devices")));
        QCOMPARE(spy.count(), 2); // already there
        QCOMPARE(f.shell->appName(), QStringLiteral("XP60Studio"));
        QVERIFY(!f.shell->buildInfo().isEmpty());

        const auto items = f.shell->navigationItems();
        int enabled = 0;
        for (const auto& item : items) {
            if (item.toMap().value(QStringLiteral("enabled")).toBool()) {
                ++enabled;
            }
        }
        QCOMPARE(enabled, 6); // Dashboard, Library, Editor, Banks, Expansion and Devices
    }

    void patchFetchFlowsIntoTheViewModel()
    {
        Fixture f;
        QAbstractItemModelTester tester(f.devices->patchParameters(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        QVERIFY(!f.devices->canFetchPatch());
        QCOMPARE(f.devices->patchFetchStateText(), QStringLiteral("Not read yet"));
        f.devices->connectDevice();
        QTRY_COMPARE(f.devices->connectionState(), ConnectionState::Connected);
        QVERIFY(f.devices->canFetchPatch());
        QSignalSpy spy(f.devices.get(), &DevicesViewModel::patchFetchChanged);
        f.devices->fetchCurrentPatch();
        QVERIFY(f.devices->patchFetchInProgress());
        QVERIFY(!f.devices->canFetchPatch());
        QCOMPARE(f.devices->patchFetchTotalBlocks(), 5);
        QCOMPARE(f.devices->patchFetchTone(), QStringLiteral("warning"));

        const auto base = xpmodel::Xp60PatchLayout::temporaryPatchAddress();
        xpmodel::MemoryImage image;
        for (const auto& block : xpmodel::Xp60PatchLayout::blocks()) {
            roland::ByteVector bytes(block.size, 0);
            for (const auto& p : block.table->parameters()) {
                xpmodel::BlockCodec::writeRaw(p, p.rawMin, bytes);
            }
            if (!block.tone) {
                const auto name = xpmodel::PatchName::fromText("Piano 1")->bytes();
                std::copy(name.begin(), name.end(), bytes.begin());
            } else {
                xpmodel::BlockCodec::writeRaw(xpmodel::xp60tables::descriptor(xpmodel::ToneParameter::ToneSwitch),
                                              block.tone->number() <= 2 ? 1 : 0, bytes);
            }
            image.write(*base.plus(block.offset), bytes);
        }
        const auto patch = *xpmodel::Xp60PatchCodec::decode(image, base).patch;
        for (const auto& reply : xpmodel::Xp60PatchCodec::encodeToDataSets(patch, roland::RolandDeviceId::factoryDefault(), xp60::modelId(), base)) {
            f.deviceReplies(reply);
        }
        QVERIFY(!f.devices->patchFetchInProgress());
        QCOMPARE(f.devices->patchFetchStateText(), QStringLiteral("Ready"));
        QCOMPARE(f.devices->patchFetchTone(), QStringLiteral("success"));
        QVERIFY(f.devices->currentPatchAvailable());
        QCOMPARE(f.devices->currentPatchName(), QStringLiteral("Piano 1"));
        QVERIFY(f.devices->currentPatchSummary().contains(QStringLiteral("tones 1,2")));
        QCOMPARE(f.devices->patchParameters()->rowCount(), 72 + 4 * 128);
        const auto idx0 = f.devices->patchParameters()->index(0, 0);
        QCOMPARE(f.devices->patchParameters()->data(idx0, PatchParameterModel::BlockRole).toString(), QStringLiteral("Common"));
        QCOMPARE(f.devices->patchParameters()->data(idx0, PatchParameterModel::NameRole).toString(), QStringLiteral("Patch Name 1"));
        QCOMPARE(f.devices->patchParameters()->data(idx0, PatchParameterModel::ValueTextRole).toString(), QStringLiteral("P"));
        const auto idxTone = f.devices->patchParameters()->index(72, 0);
        QCOMPARE(f.devices->patchParameters()->data(idxTone, PatchParameterModel::BlockRole).toString(), QStringLiteral("Tone 1"));
        QCOMPARE(f.devices->patchParameters()->data(idxTone, PatchParameterModel::ValueTextRole).toString(), QStringLiteral("ON"));
        QCOMPARE(f.devices->patchParameters()->data(idxTone, PatchParameterModel::ToneNumberRole).toInt(), 1);
        QVERIFY(spy.count() >= 2);
        QVERIFY(f.devices->canFetchPatch());

        f.devices->disconnectDevice();
        QVERIFY(!f.devices->canFetchPatch());
    }

    void writingRequiresAReadThenArming()
    {
        Fixture f;
        QVERIFY(f.devices->writeSupported());
        QVERIFY(!f.devices->canArmWrite());
        QVERIFY(!f.devices->writeArmed());
        QVERIFY(!f.devices->canWrite());
        QVERIFY(f.devices->armBlockedReason().contains(QStringLiteral("Connect")));
        QVERIFY(f.devices->writePlanText().contains(QStringLiteral("03 00 00 00")));
        QVERIFY(f.devices->writePlanText().contains(QStringLiteral("permanent User patches are not touched")));

        f.devices->connectDevice();
        QTRY_COMPARE(f.devices->connectionState(), ConnectionState::Connected);
        // Connected but nothing read yet: arming is still refused, and says why.
        QVERIFY(!f.devices->canArmWrite());
        QVERIFY(f.devices->armBlockedReason().contains(QStringLiteral("Read the current sound first")));
        f.devices->armWrite();
        QVERIFY(!f.devices->writeArmed());
        f.devices->writeBackAndVerify();
        QCOMPARE(f.devices->transferStateText(), QStringLiteral("Idle")); // nothing happened

        // A successful read unlocks arming.
        f.devices->fetchCurrentPatch();
        const auto base = xpmodel::Xp60PatchLayout::temporaryPatchAddress();
        xpmodel::MemoryImage image;
        for (const auto& block : xpmodel::Xp60PatchLayout::blocks()) {
            roland::ByteVector bytes(block.size, 0);
            for (const auto& p : block.table->parameters()) {
                xpmodel::BlockCodec::writeRaw(p, p.rawMin, bytes);
            }
            if (!block.tone) {
                const auto name = xpmodel::PatchName::fromText("Piano 1")->bytes();
                std::copy(name.begin(), name.end(), bytes.begin());
            }
            image.write(*base.plus(block.offset), bytes);
        }
        const auto patch = *xpmodel::Xp60PatchCodec::decode(image, base).patch;
        for (const auto& reply : xpmodel::Xp60PatchCodec::encodeToDataSets(
                 patch, roland::RolandDeviceId::factoryDefault(), xp60::modelId(), base)) {
            f.deviceReplies(reply);
        }
        QVERIFY(f.devices->currentPatchAvailable());
        QVERIFY(f.devices->canArmWrite());
        QVERIFY(f.devices->armBlockedReason().isEmpty());

        QSignalSpy spy(f.devices.get(), &DevicesViewModel::transferChanged);
        f.devices->armWrite();
        QVERIFY(f.devices->writeArmed());
        QVERIFY(f.devices->canWrite());
        QVERIFY(spy.count() >= 1);

        // Disarming is always available.
        f.devices->disarmWrite();
        QVERIFY(!f.devices->writeArmed());
        QVERIFY(!f.devices->canWrite());

        // Disconnecting clears arming.
        f.devices->armWrite();
        QVERIFY(f.devices->writeArmed());
        f.devices->disconnectDevice();
        QVERIFY(!f.devices->writeArmed());
        QVERIFY(!f.devices->canArmWrite());
    }

    void protocolLogModelBounded()
    {
        ProtocolLogModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setLimit(3);
        for (int i = 0; i < 5; ++i) {
            model.append(diagnostics::logSystem(diagnostics::LogKind::Info, diagnostics::LogSeverity::Info,
                                                "entry " + std::to_string(i), std::chrono::system_clock::time_point{}));
        }
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.data(model.index(0, 0), ProtocolLogModel::SummaryRole).toString(), QStringLiteral("entry 2"));
        QVERIFY(model.lineAt(2).endsWith(QStringLiteral("entry 4")));
        QVERIFY(model.lineAt(7).isEmpty());
        model.clear();
        QCOMPARE(model.rowCount(), 0);
    }
};

QTEST_GUILESS_MAIN(PresentationTest)
#include "tst_presentation.moc"
