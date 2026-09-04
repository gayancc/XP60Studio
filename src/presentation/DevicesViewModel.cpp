#include "presentation/DevicesViewModel.h"

#include "roland/RolandAddress.h"
#include "roland/RolandSize.h"
#include "xp60/Xp60Device.h"

#include <algorithm>

namespace xp60studio::presentation {

namespace {

QString toQString(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

// The expert RQ1 tool is meant for small confirmable reads.
constexpr std::uint32_t kMaxDiagnosticRequestBytes = 65535;

ConnectionState toPresentation(services::DeviceSession::ConnectionState state)
{
    switch (state) {
    case services::DeviceSession::ConnectionState::Disconnected:
        return ConnectionState::Disconnected;
    case services::DeviceSession::ConnectionState::Connecting:
        return ConnectionState::Connecting;
    case services::DeviceSession::ConnectionState::Connected:
        return ConnectionState::Connected;
    case services::DeviceSession::ConnectionState::Error:
        return ConnectionState::Error;
    }
    return ConnectionState::Error;
}

} // namespace

DevicesViewModel::DevicesViewModel(services::DeviceSession& session, services::PatchTransfer* transfer, QObject* parent)
    : QObject(parent)
    , m_session(session)
    , m_transfer(transfer)
{
    if (m_transfer) {
        connect(m_transfer, &services::PatchTransfer::changed, this, &DevicesViewModel::transferChanged);
        connect(m_transfer, &services::PatchTransfer::changed, this, &DevicesViewModel::connectionChanged);
        connect(m_transfer, &services::PatchTransfer::changed, this, &DevicesViewModel::canSendRequestChanged);
        connect(m_transfer, &services::PatchTransfer::changed, this, &DevicesViewModel::patchFetchChanged);
    }
    connect(&m_session, &services::DeviceSession::linkStateChanged, this, &DevicesViewModel::connectionChanged);
    connect(&m_session, &services::DeviceSession::endpointsChanged, this, &DevicesViewModel::syncEndpoints);
    connect(&m_session, &services::DeviceSession::connectionStateChanged, this, [this] {
        emit connectionChanged();
        emit canSendRequestChanged();
        emit patchFetchChanged();
        emit transferChanged();
    });
    connect(&m_session, &services::DeviceSession::deviceIdChanged, this, &DevicesViewModel::deviceIdChanged);
    connect(&m_session, &services::DeviceSession::logEntryAdded, this,
            [this](const diagnostics::ProtocolLogEntry& entry) { m_log.append(entry); });
    connect(&m_session, &services::DeviceSession::logCleared, this, [this] { m_log.clear(); });
    connect(&m_session, &services::DeviceSession::operationChanged, this, [this](quint64) {
        m_operations.refresh(m_session.tracker());
        emit operationsChanged();
        emit connectionChanged();
    });
    connect(&m_session, &services::DeviceSession::statisticsChanged, this, &DevicesViewModel::statisticsChanged);
    connect(&m_session, &services::DeviceSession::patchFetchChanged, this, [this] {
        const auto& fetch = m_session.patchFetch();
        if (fetch.state == services::DeviceSession::PatchFetchState::Completed && fetch.patch) {
            m_patchParameters.setPatch(*fetch.patch);
        } else if (fetch.state == services::DeviceSession::PatchFetchState::InProgress) {
            m_patchParameters.clear();
        }
        emit patchFetchChanged();
        emit transferChanged();
    });

    for (const auto& entry : m_session.log()) {
        m_log.append(entry);
    }
    syncEndpoints();
    applyReadPreset(0);
}

// ---------------------------------------------------------------------------
// Endpoints
// ---------------------------------------------------------------------------

void DevicesViewModel::syncEndpoints()
{
    m_inputs.setEndpoints(m_session.inputs());
    m_outputs.setEndpoints(m_session.outputs());
    restoreSelection();
    emit endpointsChanged();
    emit connectionChanged();
}

void DevicesViewModel::restoreSelection()
{
    const auto restore = [](const MidiEndpointListModel& model, std::optional<midi::MidiEndpointInfo>& preferred) {
        if (preferred) {
            const auto exact = model.indexOfId(QString::fromStdString(preferred->id));
            if (exact >= 0) return exact;
            // Replug may change an OS handle. Resolve a unique name/backend,
            // never a row number or an ambiguous pair of identically named ports.
            int found = -1;
            for (int i = 0; i < model.count(); ++i) {
                const auto& e = model.endpoints()[static_cast<std::size_t>(i)];
                if (e.displayName == preferred->displayName && e.backendName == preferred->backendName) {
                    if (found >= 0) return -1;
                    found = i;
                }
            }
            return found;
        }
        // A lone port is an unambiguous convenience; multiple choices require
        // the musician to explicitly choose which interface reaches the XP-60.
        if (model.count() == 1) { preferred = model.endpoints().front(); return 0; }
        return -1;
    };
    m_selectedInput = restore(m_inputs, m_preferredInput);
    m_selectedOutput = restore(m_outputs, m_preferredOutput);
    emit selectionChanged();
}

void DevicesViewModel::useConnectionSettings(QSettings* settings)
{
    m_settings = settings;
    if (!settings) return;
    const auto load = [&](const QString& direction) -> std::optional<midi::MidiEndpointInfo> {
        const auto prefix = QStringLiteral("midi/") + direction + QLatin1Char('/');
        const auto id = settings->value(prefix + QStringLiteral("id")).toString();
        if (id.isEmpty()) return {};
        return midi::MidiEndpointInfo{id.toStdString(), settings->value(prefix + QStringLiteral("name")).toString().toStdString(),
            settings->value(prefix + QStringLiteral("backend")).toString().toStdString(),
            direction == QStringLiteral("input") ? midi::EndpointDirection::Input : midi::EndpointDirection::Output, false};
    };
    m_preferredInput = load(QStringLiteral("input"));
    m_preferredOutput = load(QStringLiteral("output"));
    const int profile = settings->value(QStringLiteral("midi/pacingProfile"), 0).toInt();
    const int id = settings->value(QStringLiteral("midi/deviceId"), deviceId()).toInt();
    setPacingProfile(profile);
    setDeviceId(id);
    restoreSelection();
    emit connectionChanged();
}

void DevicesViewModel::saveConnectionSettings()
{
    if (!m_settings) return;
    const auto save = [&](const QString& direction, const std::optional<midi::MidiEndpointInfo>& port) {
        if (!port) { m_settings->remove(QStringLiteral("midi/") + direction); return; }
        const auto prefix = QStringLiteral("midi/") + direction + QLatin1Char('/');
        m_settings->setValue(prefix + QStringLiteral("id"), QString::fromStdString(port->id));
        m_settings->setValue(prefix + QStringLiteral("name"), QString::fromStdString(port->displayName));
        m_settings->setValue(prefix + QStringLiteral("backend"), QString::fromStdString(port->backendName));
    };
    save(QStringLiteral("input"), m_preferredInput);
    save(QStringLiteral("output"), m_preferredOutput);
    m_settings->setValue(QStringLiteral("midi/deviceId"), deviceId());
    m_settings->setValue(QStringLiteral("midi/pacingProfile"), m_pacingProfile);
}

QString DevicesViewModel::selectionMessage() const
{
    if (m_selectedInput < 0 && m_preferredInput) return tr("Preferred MIDI IN unavailable or ambiguous: %1. Reconnect it or choose another input.").arg(QString::fromStdString(m_preferredInput->displayName));
    if (m_selectedOutput < 0 && m_preferredOutput) return tr("Preferred MIDI OUT unavailable or ambiguous: %1. Reconnect it or choose another output.").arg(QString::fromStdString(m_preferredOutput->displayName));
    if (m_selectedInput < 0 || m_selectedOutput < 0) return tr("Choose both ports. MIDI OUT sends to the XP-60; MIDI IN receives its replies.");
    return tr("Both ports selected. Connect opens them; Test connection checks a read-only reply.");
}

bool DevicesViewModel::canTestConnection() const
{
    return m_session.connectionState() == services::DeviceSession::ConnectionState::Connected
        && !m_session.tracker().hasOutstanding() && !(m_transfer && (m_transfer->isBusy() || m_transfer->liveActive()));
}

void DevicesViewModel::setPacingProfile(int profile)
{
    if (profile < 0 || profile > 1 || m_session.tracker().hasOutstanding() || (m_transfer && (m_transfer->isBusy() || m_transfer->liveActive()))) return;
    auto pacing = m_session.pacing();
    const auto defaults = xp60::transferDefaults();
    pacing.interMessageDelay = profile == 1 ? std::chrono::milliseconds(60) : defaults.interMessageDelay;
    pacing.maxDataSetPayloadBytes = profile == 1 ? 64 : defaults.maxDataSetPayloadBytes;
    pacing.timeouts.firstResponse = profile == 1 ? std::chrono::milliseconds(5000) : defaults.firstResponseTimeout;
    pacing.timeouts.betweenChunks = profile == 1 ? std::chrono::milliseconds(3000) : defaults.betweenChunkTimeout;
    m_session.setPacing(pacing);
    m_pacingProfile = profile;
    saveConnectionSettings();
    emit connectionChanged();
}

void DevicesViewModel::setSelectedInputIndex(int index)
{
    if (canDisconnect() || connectionState() == ConnectionState::Connecting) return;
    const int clamped = std::clamp(index, -1, m_inputs.count() - 1);
    if (clamped == m_selectedInput) {
        return;
    }
    m_selectedInput = clamped;
    m_preferredInput = clamped >= 0 ? std::optional(m_inputs.endpoints()[static_cast<std::size_t>(clamped)]) : std::nullopt;
    saveConnectionSettings();
    emit selectionChanged();
    emit connectionChanged();
}

void DevicesViewModel::setSelectedOutputIndex(int index)
{
    if (canDisconnect() || connectionState() == ConnectionState::Connecting) return;
    const int clamped = std::clamp(index, -1, m_outputs.count() - 1);
    if (clamped == m_selectedOutput) {
        return;
    }
    m_selectedOutput = clamped;
    m_preferredOutput = clamped >= 0 ? std::optional(m_outputs.endpoints()[static_cast<std::size_t>(clamped)]) : std::nullopt;
    saveConnectionSettings();
    emit selectionChanged();
    emit connectionChanged();
}

bool DevicesViewModel::hasEndpoints() const
{
    return m_inputs.count() > 0 && m_outputs.count() > 0;
}

QString DevicesViewModel::backendName() const
{
    return QString::fromStdString(m_session.transport().backendName());
}

void DevicesViewModel::refreshEndpoints()
{
    m_session.refreshEndpoints();
}

// ---------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------

ConnectionState DevicesViewModel::connectionState() const
{
    return toPresentation(m_session.connectionState());
}

QString DevicesViewModel::connectionStateText() const
{
    switch (connectionState()) {
    case ConnectionState::Disconnected:
        return QStringLiteral("Disconnected");
    case ConnectionState::Connecting:
        return QStringLiteral("Connecting");
    case ConnectionState::Connected:
        return connectionVerified() ? tr("XP-60 responded") : tr("MIDI ports open");
    case ConnectionState::Error:
        return QStringLiteral("Connection error");
    }
    return {};
}

QString DevicesViewModel::connectionDetail() const
{
    switch (connectionState()) {
    case ConnectionState::Connected:
        return QStringLiteral("MIDI IN %1  ·  MIDI OUT %2").arg(connectedInputName(), connectedOutputName());
    case ConnectionState::Error:
        return lastError();
    case ConnectionState::Connecting:
        return lastError().isEmpty() ? tr("Opening MIDI ports… You can cancel while the driver is connecting.") : lastError();
    case ConnectionState::Disconnected:
        return hasEndpoints() ? QStringLiteral("Select MIDI IN and MIDI OUT, then connect")
                              : QStringLiteral("No MIDI endpoints found. Connect an interface and refresh.");
    }
    return {};
}

QString DevicesViewModel::connectedInputName() const
{
    const auto endpoint = m_session.connectedInput();
    return endpoint ? QString::fromStdString(endpoint->displayName) : QStringLiteral("-");
}

QString DevicesViewModel::connectedOutputName() const
{
    const auto endpoint = m_session.connectedOutput();
    return endpoint ? QString::fromStdString(endpoint->displayName) : QStringLiteral("-");
}

bool DevicesViewModel::canConnect() const
{
    const auto state = connectionState();
    return state != ConnectionState::Connecting && state != ConnectionState::Connected && m_selectedInput >= 0
        && m_selectedOutput >= 0;
}

bool DevicesViewModel::canDisconnect() const
{
    return connectionState() == ConnectionState::Connected || connectionState() == ConnectionState::Connecting;
}

QString DevicesViewModel::lastError() const
{
    return QString::fromStdString(m_session.lastError());
}

void DevicesViewModel::connectDevice()
{
    if (!canConnect()) {
        return;
    }
    m_preferredInput = m_inputs.endpoints()[static_cast<std::size_t>(m_selectedInput)];
    m_preferredOutput = m_outputs.endpoints()[static_cast<std::size_t>(m_selectedOutput)];
    saveConnectionSettings();
    m_session.connectEndpointsAsync(m_preferredInput->id, m_preferredOutput->id);
}

void DevicesViewModel::disconnectDevice()
{
    m_session.disconnectEndpoints();
}

// ---------------------------------------------------------------------------
// Device configuration
// ---------------------------------------------------------------------------

int DevicesViewModel::deviceId() const
{
    return m_session.deviceId().displayNumber();
}

void DevicesViewModel::setDeviceId(int displayNumber)
{
    if (const auto id = roland::RolandDeviceId::fromDisplayNumber(displayNumber)) {
        m_session.setDeviceId(*id);
        saveConnectionSettings();
    } else {
        // Reject silently but re-announce the current value so a bound control snaps back.
        emit deviceIdChanged();
    }
}

int DevicesViewModel::deviceIdMinimum() const
{
    return roland::RolandDeviceId::kMinDisplayNumber;
}

int DevicesViewModel::deviceIdMaximum() const
{
    return roland::RolandDeviceId::kMaxDisplayNumber;
}

QString DevicesViewModel::modelIdText() const
{
    return QString::fromStdString(m_session.modelId().toHexString());
}

QString DevicesViewModel::modelIdStatusText() const
{
    return toQString(xp60::verificationStatusLabel(xp60::modelIdStatus()));
}

// ---------------------------------------------------------------------------
// Expert RQ1 area
// ---------------------------------------------------------------------------

QStringList DevicesViewModel::readPresetNames() const
{
    QStringList names;
    for (const auto& preset : xp60::safeReadPresets()) {
        names << toQString(preset.name);
    }
    return names;
}

void DevicesViewModel::setSelectedReadPresetIndex(int index)
{
    applyReadPreset(index);
}

void DevicesViewModel::applyReadPreset(int index)
{
    const auto presets = xp60::safeReadPresets();
    if (index < 0 || static_cast<std::size_t>(index) >= presets.size()) {
        return;
    }
    const auto& preset = presets[static_cast<std::size_t>(index)];
    const bool presetChanged = index != m_selectedPreset;
    m_selectedPreset = index;
    m_requestAddress = QString::fromStdString(preset.address.toHexString());
    m_requestSize = QString::fromStdString(preset.size.toHexString());
    if (presetChanged) {
        emit readPresetChanged();
    }
    emit requestFieldsChanged();
    emit canSendRequestChanged();
}

QString DevicesViewModel::readPresetDescription() const
{
    const auto presets = xp60::safeReadPresets();
    if (m_selectedPreset < 0 || static_cast<std::size_t>(m_selectedPreset) >= presets.size()) {
        return {};
    }
    return toQString(presets[static_cast<std::size_t>(m_selectedPreset)].description);
}

QString DevicesViewModel::readPresetStatusText() const
{
    const auto presets = xp60::safeReadPresets();
    if (m_selectedPreset < 0 || static_cast<std::size_t>(m_selectedPreset) >= presets.size()) {
        return {};
    }
    return toQString(xp60::verificationStatusLabel(presets[static_cast<std::size_t>(m_selectedPreset)].status));
}

void DevicesViewModel::setRequestAddress(const QString& text)
{
    if (text == m_requestAddress) {
        return;
    }
    m_requestAddress = text;
    emit requestFieldsChanged();
    emit canSendRequestChanged();
}

void DevicesViewModel::setRequestSize(const QString& text)
{
    if (text == m_requestSize) {
        return;
    }
    m_requestSize = text;
    emit requestFieldsChanged();
    emit canSendRequestChanged();
}

bool DevicesViewModel::requestAddressValid() const
{
    return roland::RolandAddress::parseHex(m_requestAddress.toStdString()).has_value();
}

bool DevicesViewModel::requestSizeValid() const
{
    const auto size = roland::RolandSize::parseHex(m_requestSize.toStdString());
    return size && !size->isZero() && size->value() <= kMaxDiagnosticRequestBytes;
}

int DevicesViewModel::requestByteCount() const
{
    const auto size = roland::RolandSize::parseHex(m_requestSize.toStdString());
    return size ? static_cast<int>(size->value()) : 0;
}

QString DevicesViewModel::requestValidationMessage() const
{
    if (!requestAddressValid()) {
        return QStringLiteral("Address must be four hex bytes, each 00-7F (example: 03 00 00 00).");
    }
    const auto size = roland::RolandSize::parseHex(m_requestSize.toStdString());
    if (!size) {
        return QStringLiteral("Size must be four hex bytes, each 00-7F (example: 00 00 00 0C).");
    }
    if (size->isZero()) {
        return QStringLiteral("Size must request at least one byte.");
    }
    if (size->value() > kMaxDiagnosticRequestBytes) {
        return QStringLiteral("The diagnostics tool limits a single request to %1 bytes.").arg(kMaxDiagnosticRequestBytes);
    }
    if (connectionState() != ConnectionState::Connected) {
        return QStringLiteral("Connect to the XP-60 to send this request.");
    }
    return QStringLiteral("Read-only request for %1 byte(s). Nothing is written to the XP-60.").arg(size->value());
}

bool DevicesViewModel::canSendRequest() const
{
    return connectionState() == ConnectionState::Connected && requestAddressValid() && requestSizeValid()
        && !transferBusy();
}

bool DevicesViewModel::hasOutstandingRequests() const
{
    return m_session.tracker().hasOutstanding();
}

bool DevicesViewModel::sendRequest()
{
    if (!canSendRequest()) {
        return false;
    }
    const auto address = roland::RolandAddress::parseHex(m_requestAddress.toStdString());
    const auto size = roland::RolandSize::parseHex(m_requestSize.toStdString());
    const auto id = m_session.sendDataRequest(*address, *size);
    return id.isValid();
}

void DevicesViewModel::cancelAllRequests()
{
    m_session.cancelAllRequests();
}

void DevicesViewModel::clearLog()
{
    m_session.clearLog();
}

// ---------------------------------------------------------------------------
// Current Patch inspection
// ---------------------------------------------------------------------------

bool DevicesViewModel::canFetchPatch() const
{
    return connectionState() == ConnectionState::Connected && !patchFetchInProgress() && !transferBusy();
}

bool DevicesViewModel::patchFetchInProgress() const
{
    return m_session.patchFetch().state == services::DeviceSession::PatchFetchState::InProgress;
}

QString DevicesViewModel::patchFetchStateText() const
{
    switch (m_session.patchFetch().state) {
    case services::DeviceSession::PatchFetchState::Idle:
        return QStringLiteral("Not fetched");
    case services::DeviceSession::PatchFetchState::InProgress:
        return QStringLiteral("Reading");
    case services::DeviceSession::PatchFetchState::Completed:
        return QStringLiteral("Decoded");
    case services::DeviceSession::PatchFetchState::Failed:
        return QStringLiteral("Failed");
    }
    return {};
}

QString DevicesViewModel::patchFetchTone() const
{
    switch (m_session.patchFetch().state) {
    case services::DeviceSession::PatchFetchState::Idle:
        return QStringLiteral("neutral");
    case services::DeviceSession::PatchFetchState::InProgress:
        return QStringLiteral("warning");
    case services::DeviceSession::PatchFetchState::Completed:
        return QStringLiteral("success");
    case services::DeviceSession::PatchFetchState::Failed:
        return QStringLiteral("error");
    }
    return QStringLiteral("neutral");
}

QString DevicesViewModel::patchFetchMessage() const
{
    const auto& fetch = m_session.patchFetch();
    if (fetch.state == services::DeviceSession::PatchFetchState::Idle) {
        return connectionState() == ConnectionState::Connected
            ? QStringLiteral("Read the Patch-mode temporary Patch (5 blocks, 589 bytes) and decode it with the Parameter Address Map tables.")
            : QStringLiteral("Connect to the XP-60 to read its current Patch.");
    }
    return QString::fromStdString(fetch.message);
}

int DevicesViewModel::patchFetchCompletedBlocks() const
{
    return static_cast<int>(m_session.patchFetch().completedBlocks);
}

int DevicesViewModel::patchFetchTotalBlocks() const
{
    return static_cast<int>(m_session.patchFetch().totalBlocks);
}

bool DevicesViewModel::currentPatchAvailable() const
{
    return m_session.patchFetch().patch.has_value();
}

QString DevicesViewModel::currentPatchName() const
{
    const auto& patch = m_session.patchFetch().patch;
    return patch ? QString::fromStdString(patch->name().displayText()) : QString();
}

QString DevicesViewModel::currentPatchSummary() const
{
    const auto& patch = m_session.patchFetch().patch;
    return patch ? QString::fromStdString(patch->summary()) : QString();
}

QString DevicesViewModel::currentPatchDecodeReport() const
{
    return QString::fromStdString(m_session.patchFetch().decodeReport).trimmed();
}

void DevicesViewModel::fetchCurrentPatch()
{
    if (!canFetchPatch()) {
        return;
    }
    m_session.fetchTemporaryPatch();
}

void DevicesViewModel::cancelPatchFetch()
{
    m_session.cancelPatchFetch();
}

// ---------------------------------------------------------------------------
// Write and verify
// ---------------------------------------------------------------------------

using TransferState = services::PatchTransfer::State;

bool DevicesViewModel::canArmWrite() const
{
    return m_transfer && m_transfer->canArm() && currentPatchAvailable();
}

bool DevicesViewModel::writeArmed() const
{
    return m_transfer && m_transfer->isArmed();
}

bool DevicesViewModel::canWrite() const
{
    return writeArmed() && currentPatchAvailable() && !transferBusy();
}

bool DevicesViewModel::canRestoreSnapshot() const
{
    return m_transfer && m_transfer->safetySnapshot().has_value() && writeArmed() && !transferBusy();
}

bool DevicesViewModel::transferBusy() const
{
    return m_transfer && (m_transfer->isBusy() || m_transfer->liveActive());
}

QString DevicesViewModel::transferStateText() const
{
    return m_transfer ? QString::fromStdString(m_transfer->stateLabel()) : QString();
}

QString DevicesViewModel::transferTone() const
{
    if (!m_transfer) {
        return QStringLiteral("neutral");
    }
    switch (m_transfer->state()) {
    case TransferState::Verified:
        return QStringLiteral("success");
    case TransferState::Mismatch:
    case TransferState::Failed:
        return QStringLiteral("error");
    case TransferState::Cancelled:
        return QStringLiteral("warning");
    case TransferState::Idle:
        return QStringLiteral("neutral");
    default:
        return QStringLiteral("warning");
    }
}

QString DevicesViewModel::transferMessage() const
{
    if (!m_transfer) {
        return {};
    }
    if (m_transfer->state() == TransferState::Idle) {
        // The plan is already stated in the arming panel; do not repeat it.
        return {};
    }
    return QString::fromStdString(m_transfer->message());
}

QString DevicesViewModel::writePlanText() const
{
    return m_transfer ? QString::fromStdString(m_transfer->writePlanDescription()) : QString();
}

QString DevicesViewModel::armBlockedReason() const
{
    if (!m_transfer || m_transfer->isArmed() || (m_transfer->isBusy() || m_transfer->liveActive())) {
        return {};
    }
    if (connectionState() != ConnectionState::Connected) {
        return QStringLiteral("Connect to the XP-60 first.");
    }
    if (!m_transfer->readVerified()) {
        return QStringLiteral("Fetch the temporary Patch first. A successful read is what shows the address map is "
                              "right for this instrument, and only then is a write safe to attempt.");
    }
    if (!currentPatchAvailable()) {
        return QStringLiteral("Fetch a Patch first: the write sends back the Patch that was read.");
    }
    return {};
}

QString DevicesViewModel::mismatchReport() const
{
    if (!m_transfer || !m_transfer->diff() || m_transfer->diff()->identical()) {
        return {};
    }
    return QString::fromStdString(m_transfer->diff()->describe(12)).trimmed();
}

QString DevicesViewModel::safetySnapshotName() const
{
    if (!m_transfer || !m_transfer->safetySnapshot()) {
        return {};
    }
    return QString::fromStdString(m_transfer->safetySnapshot()->name().displayText());
}

void DevicesViewModel::armWrite()
{
    if (m_transfer) {
        m_transfer->arm();
    }
}

void DevicesViewModel::disarmWrite()
{
    if (m_transfer) {
        m_transfer->disarm();
    }
}

void DevicesViewModel::writeBackAndVerify()
{
    if (!canWrite()) {
        return;
    }
    const auto& patch = m_session.patchFetch().patch;
    if (patch) {
        m_transfer->writeAndVerifyTemporaryPatch(*patch);
    }
}

void DevicesViewModel::restoreSafetySnapshot()
{
    if (canRestoreSnapshot()) {
        m_transfer->restoreSafetySnapshot();
    }
}

void DevicesViewModel::cancelTransfer()
{
    if (m_transfer) {
        m_transfer->cancel();
    }
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

int DevicesViewModel::messagesIn() const
{
    return static_cast<int>(m_session.statistics().messagesIn);
}

int DevicesViewModel::messagesOut() const
{
    return static_cast<int>(m_session.statistics().messagesOut);
}

int DevicesViewModel::sysExIn() const
{
    return static_cast<int>(m_session.statistics().sysExIn);
}

int DevicesViewModel::sysExOut() const
{
    return static_cast<int>(m_session.statistics().sysExOut);
}

int DevicesViewModel::checksumFailures() const
{
    return static_cast<int>(m_session.statistics().checksumFailures);
}

int DevicesViewModel::parseFailures() const
{
    return static_cast<int>(m_session.statistics().parseFailures);
}

int DevicesViewModel::timeouts() const
{
    return static_cast<int>(m_session.statistics().requestsTimedOut);
}

int DevicesViewModel::requestsCompleted() const
{
    return static_cast<int>(m_session.statistics().requestsCompleted);
}

int DevicesViewModel::transportErrors() const
{
    return static_cast<int>(m_session.statistics().transportErrors);
}

QString DevicesViewModel::sysExHealthText() const
{
    const auto& stats = m_session.statistics();
    if (connectionState() != ConnectionState::Connected) {
        return QStringLiteral("Unknown");
    }
    if (stats.requestsCompleted == 0 && stats.requestsTimedOut == 0 && stats.checksumFailures == 0) {
        return QStringLiteral("Not tested");
    }
    if (stats.checksumFailures > 0) {
        return QStringLiteral("Checksum errors");
    }
    if (stats.requestsTimedOut > 0 && stats.requestsCompleted == 0) {
        return QStringLiteral("No response");
    }
    if (stats.requestsTimedOut > 0) {
        return QStringLiteral("Intermittent");
    }
    return QStringLiteral("Working");
}

QString DevicesViewModel::sysExHealthTone() const
{
    const QString text = sysExHealthText();
    if (text == QStringLiteral("Working")) {
        return QStringLiteral("success");
    }
    if (text == QStringLiteral("Checksum errors") || text == QStringLiteral("No response")) {
        return QStringLiteral("error");
    }
    if (text == QStringLiteral("Intermittent")) {
        return QStringLiteral("warning");
    }
    return QStringLiteral("neutral");
}

} // namespace xp60studio::presentation
