#include "services/DeviceSession.h"

#include "roland/HexFormat.h"
#include "roland/RolandCodec.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/MemoryImage.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QMetaObject>

#include <algorithm>

namespace xp60studio::services {

using diagnostics::LogDirection;
using diagnostics::LogKind;
using diagnostics::LogSeverity;

namespace {

constexpr int kTimeoutPollIntervalMs = 50;

} // namespace

std::string_view connectionStateName(DeviceSession::ConnectionState state) noexcept
{
    switch (state) {
    case DeviceSession::ConnectionState::Disconnected:
        return "Disconnected";
    case DeviceSession::ConnectionState::Connecting:
        return "Connecting";
    case DeviceSession::ConnectionState::Connected:
        return "Connected";
    case DeviceSession::ConnectionState::Error:
        return "Error";
    }
    return "Unknown";
}

DeviceSession::DeviceSession(std::unique_ptr<midi::IMidiTransport> transport, QObject* parent)
    : QObject(parent)
    , m_transport(std::move(transport))
    , m_deviceId(xp60::factoryDefaultDeviceId())
    , m_modelId(xp60::modelId())
    , m_knownModelIds{xp60::modelId()}
    , m_steadyClock([] { return protocol::Clock::now(); })
    , m_wallClock([] { return std::chrono::system_clock::now(); })
{
    const auto defaults = xp60::transferDefaults();
    m_pacing.interMessageDelay = defaults.interMessageDelay;
    m_pacing.maxDataSetPayloadBytes = defaults.maxDataSetPayloadBytes;
    m_pacing.timeouts.firstResponse = defaults.firstResponseTimeout;
    m_pacing.timeouts.betweenChunks = defaults.betweenChunkTimeout;
    m_tracker.setTimeouts(m_pacing.timeouts);

    m_sendTimer.setSingleShot(true);
    connect(&m_sendTimer, &QTimer::timeout, this, &DeviceSession::pumpSendQueue);
    m_timeoutTimer.setInterval(kTimeoutPollIntervalMs);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &DeviceSession::pollTimeouts);

    // Backend-thread callbacks: minimal work, then hop to this object's thread.
    m_transport->setReceiveHandler([this](const midi::MidiEvent& event) {
        std::vector<midi::MidiBytes> complete;
        {
            std::lock_guard lock(m_assemblerMutex);
            m_assembler.feed(midi::MidiByteSpan(event.bytes.data(), event.bytes.size()),
                             [&complete](midi::MidiBytes message) { complete.push_back(std::move(message)); });
        }
        for (auto& message : complete) {
            QMetaObject::invokeMethod(
                this, [this, bytes = std::move(message)]() mutable { handleIncomingMessage(std::move(bytes)); },
                Qt::QueuedConnection);
        }
    });
    m_transport->setErrorHandler([this](const midi::TransportError& error) {
        QMetaObject::invokeMethod(
            this, [this, error]() { handleTransportError(error); }, Qt::QueuedConnection);
    });
    m_transport->setEndpointsChangedHandler([this]() {
        QMetaObject::invokeMethod(this, [this]() { handleEndpointsChanged(); }, Qt::QueuedConnection);
    });

    connect(this, &DeviceSession::operationChanged, this, [this](quint64) { updatePatchFetch(); });

    refreshEndpoints();
    logSystem(LogKind::Transport, LogSeverity::Info, "MIDI backend: " + m_transport->backendName());
}

DeviceSession::~DeviceSession()
{
    // Detach callbacks before the transport dies so a late backend callback
    // cannot touch a destroyed object.
    m_transport->setReceiveHandler({});
    m_transport->setErrorHandler({});
    m_transport->setEndpointsChangedHandler({});
    m_transport->closeAll();
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void DeviceSession::setDeviceId(roland::RolandDeviceId id)
{
    if (id == m_deviceId) {
        return;
    }
    m_deviceId = id;
    logSystem(LogKind::Info, LogSeverity::Info, "Device ID set to " + std::to_string(id.displayNumber()));
    emit deviceIdChanged();
}

void DeviceSession::setPacing(const protocol::TransferPacing& pacing)
{
    if (!pacing.isValid()) {
        return;
    }
    m_pacing = pacing;
    m_tracker.setTimeouts(pacing.timeouts);
    emit pacingChanged();
}

void DeviceSession::setClocks(SteadyClock steady, WallClock wall)
{
    if (steady) {
        m_steadyClock = std::move(steady);
    }
    if (wall) {
        m_wallClock = std::move(wall);
    }
}

void DeviceSession::setAutomaticTimeoutPolling(bool enabled)
{
    m_automaticTimeoutPolling = enabled;
    updateTimeoutTimer();
}

// ---------------------------------------------------------------------------
// Endpoints
// ---------------------------------------------------------------------------

void DeviceSession::refreshEndpoints()
{
    m_inputs = m_transport->enumerateInputs();
    m_outputs = m_transport->enumerateOutputs();
    emit endpointsChanged();
}

void DeviceSession::handleEndpointsChanged()
{
    refreshEndpoints();
    logSystem(LogKind::Transport, LogSeverity::Info,
              "MIDI endpoints changed: " + std::to_string(m_inputs.size()) + " input(s), "
                  + std::to_string(m_outputs.size()) + " output(s)");

    // Detect loss of an open endpoint.
    if (m_state == ConnectionState::Connected) {
        const auto in = m_transport->openInputEndpoint();
        const auto out = m_transport->openOutputEndpoint();
        const bool inputGone = in && std::none_of(m_inputs.begin(), m_inputs.end(),
                                                  [&](const auto& e) { return e.id == in->id; });
        const bool outputGone = out && std::none_of(m_outputs.begin(), m_outputs.end(),
                                                    [&](const auto& e) { return e.id == out->id; });
        if (inputGone || outputGone) {
            m_transport->closeAll();
            const auto outstanding = m_tracker.outstanding();
            const std::size_t cancelled = m_tracker.cancelAll(m_steadyClock(), "MIDI endpoint disappeared");
            m_statistics.requestsCancelled += cancelled;
            m_sendQueue.clear();
            setState(ConnectionState::Error, "The connected MIDI device was removed");
            for (const auto id : outstanding) {
                emit operationChanged(id.value);
            }
            emit statisticsChanged();
        }
    }
}

// ---------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------

std::optional<midi::MidiEndpointInfo> DeviceSession::connectedInput() const
{
    return m_transport->openInputEndpoint();
}

std::optional<midi::MidiEndpointInfo> DeviceSession::connectedOutput() const
{
    return m_transport->openOutputEndpoint();
}

bool DeviceSession::connectEndpoints(const std::string& inputId, const std::string& outputId)
{
    if (m_state == ConnectionState::Connected) {
        disconnectEndpoints();
    }
    setState(ConnectionState::Connecting);

    if (const auto error = m_transport->openInput(inputId)) {
        ++m_statistics.transportErrors;
        setState(ConnectionState::Error, error.message);
        logSystem(LogKind::Transport, LogSeverity::Error, error.message);
        emit statisticsChanged();
        return false;
    }
    if (const auto error = m_transport->openOutput(outputId)) {
        ++m_statistics.transportErrors;
        m_transport->closeInput();
        setState(ConnectionState::Error, error.message);
        logSystem(LogKind::Transport, LogSeverity::Error, error.message);
        emit statisticsChanged();
        return false;
    }

    {
        std::lock_guard lock(m_assemblerMutex);
        m_assembler.reset();
    }
    setState(ConnectionState::Connected);
    logSystem(LogKind::Transport, LogSeverity::Info, "Connected. MIDI IN: " + inputName() + "  MIDI OUT: " + outputName());
    return true;
}

void DeviceSession::disconnectEndpoints()
{
    const std::size_t cancelled = m_tracker.cancelAll(m_steadyClock(), "Disconnected");
    m_statistics.requestsCancelled += cancelled;
    for (const auto& op : m_tracker.operations()) {
        if (op.state == protocol::RequestState::Cancelled && op.finishedAt && op.failureReason == "Disconnected") {
            emit operationChanged(op.id.value);
        }
    }
    m_sendQueue.clear();
    m_sendTimer.stop();
    m_transport->closeAll();
    if (m_state != ConnectionState::Disconnected) {
        setState(ConnectionState::Disconnected);
        logSystem(LogKind::Transport, LogSeverity::Info, "Disconnected");
    }
    if (cancelled != 0) {
        emit statisticsChanged();
    }
    updateTimeoutTimer();
}

void DeviceSession::setState(ConnectionState state, std::string error)
{
    m_lastError = std::move(error);
    if (m_state == state) {
        emit connectionStateChanged();
        return;
    }
    m_state = state;
    emit connectionStateChanged();
}

std::string DeviceSession::inputName() const
{
    const auto in = m_transport->openInputEndpoint();
    return in ? in->displayName : std::string("-");
}

std::string DeviceSession::outputName() const
{
    const auto out = m_transport->openOutputEndpoint();
    return out ? out->displayName : std::string("-");
}

// ---------------------------------------------------------------------------
// Sending
// ---------------------------------------------------------------------------

protocol::RequestId DeviceSession::sendDataRequest(const roland::RolandAddress& address, const roland::RolandSize& size)
{
    if (m_state != ConnectionState::Connected) {
        logSystem(LogKind::Operation, LogSeverity::Warning, "Cannot send RQ1: not connected");
        return protocol::RequestId{};
    }
    if (size.isZero()) {
        logSystem(LogKind::Operation, LogSeverity::Warning, "Cannot send RQ1: size is zero");
        return protocol::RequestId{};
    }
    const auto request = roland::RolandSysExMessage::dataRequest(m_deviceId, m_modelId, address, size);
    const auto id = m_tracker.enqueue(request, m_steadyClock());
    m_sendQueue.push_back(Outgoing{request.encode(), id, request});
    logSystem(LogKind::Operation, LogSeverity::Info,
              "Request #" + std::to_string(id.value) + " queued: " + request.summary(), {}, id.value);
    emit operationChanged(id.value);
    scheduleSend();
    return id;
}

bool DeviceSession::cancelRequest(protocol::RequestId id)
{
    // Drop it from the send queue if it has not left yet.
    const auto it = std::find_if(m_sendQueue.begin(), m_sendQueue.end(),
                                 [&](const Outgoing& o) { return o.requestId == id; });
    if (it != m_sendQueue.end()) {
        m_sendQueue.erase(it);
    }
    if (!m_tracker.cancel(id, m_steadyClock())) {
        return false;
    }
    ++m_statistics.requestsCancelled;
    logSystem(LogKind::Operation, LogSeverity::Info, "Request #" + std::to_string(id.value) + " cancelled", {}, id.value);
    emit operationChanged(id.value);
    emit statisticsChanged();
    updateTimeoutTimer();
    return true;
}

std::size_t DeviceSession::cancelAllRequests()
{
    const auto ids = m_tracker.outstanding();
    std::size_t count = 0;
    for (const auto id : ids) {
        if (cancelRequest(id)) {
            ++count;
        }
    }
    return count;
}

void DeviceSession::scheduleSend()
{
    if (m_sendQueue.empty()) {
        return;
    }
    const auto now = m_steadyClock();
    if (!m_lastSendAt) {
        pumpSendQueue();
        return;
    }
    const auto elapsed = now - *m_lastSendAt;
    if (elapsed >= m_pacing.interMessageDelay) {
        pumpSendQueue();
        return;
    }
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(m_pacing.interMessageDelay - elapsed);
    m_sendTimer.start(static_cast<int>(std::max<long long>(1, remaining.count())));
}

void DeviceSession::pumpSendQueue()
{
    if (m_sendQueue.empty()) {
        return;
    }
    if (m_state != ConnectionState::Connected) {
        m_sendQueue.clear();
        return;
    }
    const auto now = m_steadyClock();
    if (m_lastSendAt && (now - *m_lastSendAt) < m_pacing.interMessageDelay) {
        scheduleSend();
        return;
    }

    Outgoing outgoing = std::move(m_sendQueue.front());
    m_sendQueue.pop_front();

    const midi::MidiByteSpan bytes(outgoing.bytes.data(), outgoing.bytes.size());
    const auto error = midi::isCompleteSysEx(bytes) ? m_transport->sendSysEx(bytes) : m_transport->send(bytes);
    m_lastSendAt = now;

    if (error) {
        ++m_statistics.transportErrors;
        if (outgoing.requestId.isValid()) {
            m_tracker.fail(outgoing.requestId, now, "Send failed: " + error.message);
            ++m_statistics.requestsFailed;
            emit operationChanged(outgoing.requestId.value);
        }
        logSystem(LogKind::Transport, LogSeverity::Error, "Send failed: " + error.message,
                  roland::toHex(roland::ByteSpan(bytes.data(), bytes.size())));
        emit statisticsChanged();
        scheduleSend();
        return;
    }

    ++m_statistics.messagesOut;
    if (midi::isCompleteSysEx(bytes)) {
        ++m_statistics.sysExOut;
    }
    if (outgoing.roland) {
        std::optional<std::uint64_t> requestId;
        if (outgoing.requestId.isValid()) {
            requestId = outgoing.requestId.value;
            m_tracker.markSent(outgoing.requestId, now);
            ++m_statistics.requestsSent;
            emit operationChanged(outgoing.requestId.value);
        }
        appendLog(diagnostics::logRolandMessage(LogDirection::Out, *outgoing.roland, outputName(), requestId, m_wallClock()));
    } else {
        appendLog(diagnostics::logRawMidi(LogDirection::Out, bytes, outputName(), m_wallClock()));
    }
    emit statisticsChanged();
    updateTimeoutTimer();
    scheduleSend();
}

// ---------------------------------------------------------------------------
// Patch fetch
// ---------------------------------------------------------------------------

bool DeviceSession::fetchTemporaryPatch()
{
    return fetchPatch(xpmodel::Xp60PatchLayout::temporaryPatchAddress());
}

bool DeviceSession::fetchPatch(const roland::RolandAddress& patchBase)
{
    if (m_state != ConnectionState::Connected) {
        logSystem(LogKind::Operation, LogSeverity::Warning, "Cannot fetch patch: not connected");
        return false;
    }
    if (m_patchFetch.state == PatchFetchState::InProgress) {
        logSystem(LogKind::Operation, LogSeverity::Warning, "A patch fetch is already in progress");
        return false;
    }
    const auto plan = xpmodel::Xp60PatchLayout::fetchPlan(patchBase);
    if (plan.empty()) {
        logSystem(LogKind::Operation, LogSeverity::Error, "Cannot fetch patch: address overflow at " + patchBase.toHexString());
        return false;
    }

    m_patchFetch = PatchFetchStatus{};
    m_patchFetch.state = PatchFetchState::InProgress;
    m_patchFetch.base = patchBase;
    m_patchFetch.totalBlocks = plan.size();
    m_patchFetch.message = "Reading " + std::to_string(plan.size()) + " blocks from " + patchBase.toHexString();
    for (const auto& request : plan) {
        const auto id = sendDataRequest(request.address, request.size);
        if (!id.isValid()) {
            m_patchFetch.state = PatchFetchState::Failed;
            m_patchFetch.message = "Could not queue the request for " + std::string(request.block.name);
            emit patchFetchChanged();
            return false;
        }
        m_patchFetch.requests.push_back(id);
    }
    logSystem(LogKind::Operation, LogSeverity::Info, "Patch fetch started: " + m_patchFetch.message);
    emit patchFetchChanged();
    // A send failure during queueing marks its request terminal while
    // updatePatchFetch() is still short-circuited by the "all queued" guard;
    // evaluate once now that every block has been requested.
    updatePatchFetch();
    return true;
}

void DeviceSession::cancelPatchFetch()
{
    if (m_patchFetch.state != PatchFetchState::InProgress) {
        return;
    }
    for (const auto id : m_patchFetch.requests) {
        cancelRequest(id);
    }
    m_patchFetch.state = PatchFetchState::Failed;
    m_patchFetch.message = "Patch fetch cancelled";
    emit patchFetchChanged();
}

void DeviceSession::updatePatchFetch()
{
    if (m_patchFetch.state != PatchFetchState::InProgress) {
        return;
    }
    // sendDataRequest() emits operationChanged while fetchPatch() is still
    // queueing; completion is only meaningful once every block is requested.
    if (m_patchFetch.requests.size() < m_patchFetch.totalBlocks) {
        return;
    }
    std::size_t completed = 0;
    for (const auto id : m_patchFetch.requests) {
        const auto* op = m_tracker.find(id);
        if (!op) {
            m_patchFetch.state = PatchFetchState::Failed;
            m_patchFetch.message = "Request #" + std::to_string(id.value) + " disappeared from the tracker";
            emit patchFetchChanged();
            return;
        }
        if (op->state == protocol::RequestState::Completed) {
            ++completed;
        } else if (protocol::isTerminal(op->state)) {
            m_patchFetch.state = PatchFetchState::Failed;
            m_patchFetch.message = "Block read at " + op->request.address().toHexString() + " "
                + std::string(protocol::requestStateLabel(op->state))
                + (op->failureReason.empty() ? std::string() : ": " + op->failureReason);
            logSystem(LogKind::Operation, LogSeverity::Error, "Patch fetch failed: " + m_patchFetch.message);
            // The remaining block reads no longer serve a purpose.
            for (const auto other : m_patchFetch.requests) {
                const auto* otherOp = m_tracker.find(other);
                if (otherOp && !protocol::isTerminal(otherOp->state)) {
                    cancelRequest(other);
                }
            }
            emit patchFetchChanged();
            return;
        }
    }
    if (completed != m_patchFetch.completedBlocks) {
        m_patchFetch.completedBlocks = completed;
        m_patchFetch.message = std::to_string(completed) + " / " + std::to_string(m_patchFetch.totalBlocks) + " blocks received";
        emit patchFetchChanged();
    }
    if (completed != m_patchFetch.requests.size()) {
        return;
    }

    xpmodel::MemoryImage image;
    for (const auto id : m_patchFetch.requests) {
        const auto* op = m_tracker.find(id);
        image.write(op->request.address(), op->data);
    }
    auto decoded = xpmodel::Xp60PatchCodec::decode(image, m_patchFetch.base);
    m_patchFetch.decodeReport = decoded.describe();
    if (!decoded.ok()) {
        m_patchFetch.state = PatchFetchState::Failed;
        m_patchFetch.message = "All blocks received but the patch did not decode ("
            + std::to_string(decoded.errorCount()) + " error(s))";
        logSystem(LogKind::Operation, LogSeverity::Error, "Patch fetch failed: " + m_patchFetch.message, m_patchFetch.decodeReport);
    } else {
        m_patchFetch.state = PatchFetchState::Completed;
        m_patchFetch.patch = std::move(decoded.patch);
        m_patchFetch.message = "Patch decoded: " + m_patchFetch.patch->summary();
        if (decoded.hasWarnings()) {
            m_patchFetch.message += " (" + std::to_string(decoded.issues.size()) + " out-of-range value(s), kept verbatim)";
        }
        logSystem(LogKind::Operation, decoded.hasWarnings() ? LogSeverity::Warning : LogSeverity::Info,
                  "Patch fetch complete: " + m_patchFetch.message, m_patchFetch.decodeReport);
    }
    emit patchFetchChanged();
}

// ---------------------------------------------------------------------------
// Receiving
// ---------------------------------------------------------------------------

void DeviceSession::handleIncomingMessage(midi::MidiBytes bytes)
{
    ++m_statistics.messagesIn;
    const midi::MidiByteSpan span(bytes.data(), bytes.size());
    const roland::ByteSpan rspan(bytes.data(), bytes.size());
    const auto now = m_steadyClock();
    const auto wall = m_wallClock();

    if (!midi::isCompleteSysEx(span)) {
        appendLog(diagnostics::logRawMidi(LogDirection::In, span, inputName(), wall));
        emit statisticsChanged();
        return;
    }

    ++m_statistics.sysExIn;
    const auto decoded = roland::decodeRolandSysEx(rspan, std::span<const roland::RolandModelId>(m_knownModelIds));
    if (!decoded.ok()) {
        if (roland::isRolandSysEx(rspan)) {
            ++m_statistics.parseFailures;
            if (decoded.failure->error == roland::RolandParseError::InvalidChecksum) {
                ++m_statistics.checksumFailures;
            }
        }
        appendLog(diagnostics::logParseFailure(LogDirection::In, span, *decoded.failure, inputName(), wall));
        emit statisticsChanged();
        return;
    }

    ++m_statistics.rolandMessagesIn;
    const auto& message = *decoded.message;
    std::optional<std::uint64_t> requestId;
    std::string detail;

    if (message.isDataSet()) {
        const auto match = m_tracker.onDataSet(message, now);
        if (match.requestId) {
            requestId = match.requestId->value;
        }
        detail = std::string(protocol::matchOutcomeName(match.outcome)) + ": " + match.detail;
        switch (match.outcome) {
        case protocol::RolandRequestTracker::MatchOutcome::Completed:
            ++m_statistics.requestsCompleted;
            break;
        case protocol::RolandRequestTracker::MatchOutcome::Rejected:
            ++m_statistics.requestsFailed;
            break;
        case protocol::RolandRequestTracker::MatchOutcome::NoOutstandingRequest:
        case protocol::RolandRequestTracker::MatchOutcome::NoMatch:
            ++m_statistics.unsolicitedDataSets;
            break;
        case protocol::RolandRequestTracker::MatchOutcome::Accepted:
            break;
        }
        auto entry = diagnostics::logRolandMessage(LogDirection::In, message, inputName(), requestId, wall);
        entry.detail = detail;
        appendLog(std::move(entry));
        if (match.requestId) {
            const auto* op = m_tracker.find(*match.requestId);
            if (op && protocol::isTerminal(op->state)) {
                const std::string summary = "Request #" + std::to_string(op->id.value) + " "
                    + std::string(protocol::requestStateLabel(op->state)) + ": " + match.detail;
                logSystem(LogKind::Operation,
                          op->state == protocol::RequestState::Completed ? LogSeverity::Info : LogSeverity::Error,
                          summary, op->failureReason, op->id.value);
            }
            emit operationChanged(match.requestId->value);
        } else {
            logSystem(LogKind::Operation, LogSeverity::Warning, "Unsolicited DT1: " + match.detail);
        }
    } else {
        // An RQ1 arriving at the computer is unusual (another controller?) but
        // still worth showing.
        appendLog(diagnostics::logRolandMessage(LogDirection::In, message, inputName(), std::nullopt, wall));
    }
    emit statisticsChanged();
    updateTimeoutTimer();
}

void DeviceSession::handleTransportError(midi::TransportError error)
{
    ++m_statistics.transportErrors;
    logSystem(LogKind::Transport, LogSeverity::Error, error.message, std::string(midi::transportErrorCodeName(error.code)));
    emit statisticsChanged();
}

// ---------------------------------------------------------------------------
// Timeouts
// ---------------------------------------------------------------------------

void DeviceSession::pollTimeouts()
{
    const auto expired = m_tracker.expire(m_steadyClock());
    for (const auto id : expired) {
        ++m_statistics.requestsTimedOut;
        const auto* op = m_tracker.find(id);
        const std::string reason = op ? op->failureReason : std::string("Timed out");
        logSystem(LogKind::Operation, LogSeverity::Error,
                  "Request #" + std::to_string(id.value) + " timed out: " + reason,
                  "MIDI IN: " + inputName() + "  MIDI OUT: " + outputName(), id.value);
        emit operationChanged(id.value);
    }
    if (!expired.empty()) {
        emit statisticsChanged();
    }
    updateTimeoutTimer();
}

void DeviceSession::updateTimeoutTimer()
{
    if (!m_automaticTimeoutPolling) {
        m_timeoutTimer.stop();
        return;
    }
    if (m_tracker.hasOutstanding()) {
        if (!m_timeoutTimer.isActive()) {
            m_timeoutTimer.start();
        }
    } else {
        m_timeoutTimer.stop();
    }
}

// ---------------------------------------------------------------------------
// Log
// ---------------------------------------------------------------------------

void DeviceSession::setLogLimit(std::size_t limit)
{
    m_logLimit = std::max<std::size_t>(limit, 1);
    while (m_log.size() > m_logLimit) {
        m_log.pop_front();
    }
}

void DeviceSession::clearLog()
{
    m_log.clear();
    emit logCleared();
}

void DeviceSession::appendLog(diagnostics::ProtocolLogEntry entry)
{
    m_log.push_back(std::move(entry));
    while (m_log.size() > m_logLimit) {
        m_log.pop_front();
    }
    emit logEntryAdded(m_log.back());
}

void DeviceSession::logSystem(LogKind kind, LogSeverity severity, std::string summary, std::string detail,
                              std::optional<std::uint64_t> requestId)
{
    appendLog(diagnostics::logSystem(kind, severity, std::move(summary), m_wallClock(), std::move(detail), requestId));
}

} // namespace xp60studio::services
