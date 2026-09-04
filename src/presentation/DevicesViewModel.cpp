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

DevicesViewModel::DevicesViewModel(services::DeviceSession& session, QObject* parent)
    : QObject(parent)
    , m_session(session)
{
    connect(&m_session, &services::DeviceSession::endpointsChanged, this, &DevicesViewModel::syncEndpoints);
    connect(&m_session, &services::DeviceSession::connectionStateChanged, this, [this] {
        emit connectionChanged();
        emit canSendRequestChanged();
    });
    connect(&m_session, &services::DeviceSession::deviceIdChanged, this, &DevicesViewModel::deviceIdChanged);
    connect(&m_session, &services::DeviceSession::logEntryAdded, this,
            [this](const diagnostics::ProtocolLogEntry& entry) { m_log.append(entry); });
    connect(&m_session, &services::DeviceSession::logCleared, this, [this] { m_log.clear(); });
    connect(&m_session, &services::DeviceSession::operationChanged, this, [this](quint64) {
        m_operations.refresh(m_session.tracker());
        emit operationsChanged();
    });
    connect(&m_session, &services::DeviceSession::statisticsChanged, this, &DevicesViewModel::statisticsChanged);

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
    clampSelection();
    emit endpointsChanged();
    emit connectionChanged();
}

void DevicesViewModel::clampSelection()
{
    const int inputs = m_inputs.count();
    const int outputs = m_outputs.count();
    int input = m_selectedInput;
    int output = m_selectedOutput;
    if (input >= inputs) {
        input = inputs - 1;
    }
    if (input < 0 && inputs > 0) {
        input = 0;
    }
    if (output >= outputs) {
        output = outputs - 1;
    }
    if (output < 0 && outputs > 0) {
        output = 0;
    }
    if (input != m_selectedInput || output != m_selectedOutput) {
        m_selectedInput = input;
        m_selectedOutput = output;
        emit selectionChanged();
    }
}

void DevicesViewModel::setSelectedInputIndex(int index)
{
    const int clamped = std::clamp(index, -1, m_inputs.count() - 1);
    if (clamped == m_selectedInput) {
        return;
    }
    m_selectedInput = clamped;
    emit selectionChanged();
    emit connectionChanged();
}

void DevicesViewModel::setSelectedOutputIndex(int index)
{
    const int clamped = std::clamp(index, -1, m_outputs.count() - 1);
    if (clamped == m_selectedOutput) {
        return;
    }
    m_selectedOutput = clamped;
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
        return QStringLiteral("Connected");
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
        return QStringLiteral("Opening MIDI endpoints");
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
    return connectionState() == ConnectionState::Connected;
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
    m_session.connectEndpoints(m_inputs.endpointIdAt(m_selectedInput).toStdString(),
                               m_outputs.endpointIdAt(m_selectedOutput).toStdString());
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
    return connectionState() == ConnectionState::Connected && requestAddressValid() && requestSizeValid();
}

bool DevicesViewModel::hasOutstandingRequests() const
{
    return m_session.tracker().hasOutstanding();
}

QString DevicesViewModel::dataSetDisabledReason() const
{
    return QStringLiteral("Writing (DT1) stays disabled until the temporary-memory semantics are verified on a "
                          "physical XP-60. See docs/HARDWARE_VALIDATION_XP60.md.");
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
