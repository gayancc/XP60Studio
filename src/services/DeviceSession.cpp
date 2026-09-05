#include "services/DeviceSession.h"

#include "roland/HexFormat.h"
#include "roland/RolandCodec.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/MemoryImage.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QMetaObject>
#include <QtConcurrent/QtConcurrentRun>

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
        const auto epoch = m_connectionEpoch.load();
        std::vector<midi::MidiBytes> complete;
        {
            std::lock_guard lock(m_assemblerMutex);
            if (epoch != m_connectionEpoch.load()) return;
            m_assembler.feed(midi::MidiByteSpan(event.bytes.data(), event.bytes.size()),
                             [&complete](midi::MidiBytes message) { complete.push_back(std::move(message)); });
        }
        for (auto& message : complete) {
            QMetaObject::invokeMethod(
                this, [this, epoch, bytes = std::move(message)]() mutable {
                    if (epoch == m_connectionEpoch.load() && m_state == ConnectionState::Connected)
                        handleIncomingMessage(std::move(bytes));
                },
                Qt::QueuedConnection);
        }
    });
    m_transport->setErrorHandler([this](const midi::TransportError& error) {
        const auto epoch = m_connectionEpoch.load();
        QMetaObject::invokeMethod(
            this, [this, epoch, error]() { if (epoch == m_connectionEpoch.load()) handleTransportError(error); }, Qt::QueuedConnection);
    });
    m_transport->setEndpointsChangedHandler([this]() {
        QMetaObject::invokeMethod(this, [this]() { handleEndpointsChanged(); }, Qt::QueuedConnection);
    });

    connect(this, &DeviceSession::operationChanged, this, [this](quint64) { updatePatchFetch(); });

    connect(this, &DeviceSession::operationChanged, this, [this](quint64) { updateConnectionTest(); });
    connect(&m_connectionOpen, &QFutureWatcher<midi::TransportError>::finished, this, [this] {
        const auto error = m_connectionOpen.result();
        m_openPending = false;
        if (m_cancelOpen) {
            endConnection(ConnectionState::Disconnected, "Connection cancelled");
        } else {
            finishConnection(error);
        }
        refreshEndpoints();
    });
    refreshEndpoints();
    logSystem(LogKind::Transport, LogSeverity::Info, "MIDI backend: " + m_transport->backendName());
}

DeviceSession::~DeviceSession()
{
    m_connectionOpen.waitForFinished();
    // Detach callbacks before the transport dies so a late backend callback
    // cannot touch a destroyed object.
    m_transport->setReceiveHandler({});
    m_transport->setErrorHandler({});
    m_transport->setEndpointsChangedHandler({});
    m_transport->closeAll();
    // Join/destroy backend resources while all callback state still exists.
    m_transport.reset();
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void DeviceSession::setDeviceId(roland::RolandDeviceId id)
{
    if (id == m_deviceId) {
        return;
    }
    cancelAllRequests();
    ++m_connectionEpoch;
    { std::lock_guard lock(m_assemblerMutex); m_assembler.reset(); }
    setLinkState(LinkState::Unchecked, "Device ID changed. Test the connection again.");
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
    if (m_openPending) return; // Opening a wireless port must not block UI enumeration.
    try {
        m_inputs = m_transport->enumerateInputs();
        m_outputs = m_transport->enumerateOutputs();
        checkOpenEndpoints();
        emit endpointsChanged();
    } catch (const std::exception& error) {
        handleTransportError(midi::TransportError::make(midi::TransportErrorCode::Internal,
                                                       "MIDI discovery failed: " + std::string(error.what())));
    }
}

void DeviceSession::handleEndpointsChanged()
{
    refreshEndpoints();
}

void DeviceSession::checkOpenEndpoints()
{
    if (m_state != ConnectionState::Connected) return;
    const auto in = m_transport->openInputEndpoint();
    const auto out = m_transport->openOutputEndpoint();
    const bool inputGone = !in || std::none_of(m_inputs.begin(), m_inputs.end(), [&](const auto& e) { return e.id == in->id; });
    const bool outputGone = !out || std::none_of(m_outputs.begin(), m_outputs.end(), [&](const auto& e) { return e.id == out->id; });
    if (inputGone || outputGone) {
        endConnection(ConnectionState::Error, "MIDI connection lost. Check the interface or wireless link, then reconnect the selected ports.");
    }
}

// ---------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------

std::optional<midi::MidiEndpointInfo> DeviceSession::connectedInput() const
{
    return m_state == ConnectionState::Connected ? m_transport->openInputEndpoint() : std::nullopt;
}

std::optional<midi::MidiEndpointInfo> DeviceSession::connectedOutput() const
{
    return m_state == ConnectionState::Connected ? m_transport->openOutputEndpoint() : std::nullopt;
}

midi::TransportError DeviceSession::openEndpoints(const std::string& inputId, const std::string& outputId)
{
    try {
        if (auto error = m_transport->openInput(inputId)) { m_transport->closeAll(); return error; }
        if (auto error = m_transport->openOutput(outputId)) { m_transport->closeAll(); return error; }
        return {};
    } catch (const std::exception& error) {
        m_transport->closeAll();
        return midi::TransportError::make(midi::TransportErrorCode::OpenFailed, error.what());
    }
}

bool DeviceSession::finishConnection(const midi::TransportError& error)
{
    if (error) {
        ++m_statistics.transportErrors;
        endConnection(ConnectionState::Error, error.message);
        logSystem(LogKind::Transport, LogSeverity::Error, error.message);
        emit statisticsChanged();
        return false;
    }
    { std::lock_guard lock(m_assemblerMutex); m_assembler.reset(); }
    setLinkState(LinkState::Unchecked, "MIDI ports are open. Test the connection to check that the XP-60 responds.");
    setState(ConnectionState::Connected);
    logSystem(LogKind::Transport, LogSeverity::Info, "Ports open. MIDI IN: " + inputName() + "  MIDI OUT: " + outputName());
    return true;
}

bool DeviceSession::connectEndpoints(const std::string& inputId, const std::string& outputId)
{
    if (m_openPending || m_endingConnection || m_state == ConnectionState::Connecting) return false;
    endConnection(ConnectionState::Disconnected, "Changing MIDI connection");
    setState(ConnectionState::Connecting);
    return finishConnection(openEndpoints(inputId, outputId));
}

void DeviceSession::connectEndpointsAsync(const std::string& inputId, const std::string& outputId)
{
    if (m_openPending || m_endingConnection || m_state == ConnectionState::Connecting) return;
    endConnection(ConnectionState::Disconnected, "Changing MIDI connection");
    m_openPending = true;
    m_cancelOpen = false;
    setState(ConnectionState::Connecting);
    m_connectionOpen.setFuture(QtConcurrent::run([this, inputId, outputId] { return openEndpoints(inputId, outputId); }));
}

void DeviceSession::disconnectEndpoints()
{
    if (m_openPending) {
        m_cancelOpen = true;
        setState(ConnectionState::Connecting, "Cancelling; waiting for the MIDI driver to finish opening the port.");
        return;
    }
    endConnection(ConnectionState::Disconnected, "Disconnected");
}

void DeviceSession::endConnection(ConnectionState state, const std::string& reason)
{
    if (m_endingConnection) return;
    m_endingConnection = true;
    ++m_connectionEpoch;
    m_sendTimer.stop();
    m_timeoutTimer.stop();
    m_sendQueue.clear();
    m_lastSendAt.reset();
    m_connectionTest = {};
    // Block any reentrant requests/writes before notifying consumers.
    m_state = state;
    m_lastError = state == ConnectionState::Error ? reason : std::string{};
    m_transport->closeAll();
    { std::lock_guard lock(m_assemblerMutex); m_assembler.reset(); }
    const auto outstanding = m_tracker.outstanding();
    m_statistics.requestsCancelled += m_tracker.cancelAll(m_steadyClock(), reason);
    setLinkState(LinkState::Unchecked, "Open both MIDI ports, then test the connection.");
    emit connectionStateChanged();
    for (const auto id : outstanding) emit operationChanged(id.value);
    while (!m_dataSetBatches.empty()) {
        finishDataSetBatch(DataSetBatchId{m_dataSetBatches.begin()->first}, false, reason);
    }
    emit statisticsChanged();
    m_endingConnection = false;
}

void DeviceSession::setLinkState(LinkState state, std::string message)
{
    m_linkState = state;
    m_linkMessage = std::move(message);
    emit linkStateChanged();
}

bool DeviceSession::testConnection()
{
    if (m_state != ConnectionState::Connected || m_tracker.hasOutstanding() || !m_dataSetBatches.empty()) return false;
    setLinkState(LinkState::Checking, "Reading the temporary Patch name. Put the XP-60 in Patch mode.");
    const auto& preset = xp60::safeReadPresets().front();
    m_connectionTest = sendDataRequest(preset.address, preset.size);
    updateConnectionTest();
    return m_connectionTest.isValid();
}

void DeviceSession::updateConnectionTest()
{
    if (!m_connectionTest.isValid()) return;
    const auto* op = m_tracker.find(m_connectionTest);
    if (!op || !protocol::isTerminal(op->state)) return;
    m_connectionTest = {};
    if (op->state != protocol::RequestState::Completed) {
        setLinkState(LinkState::Failed, "No complete valid reply. Check both port selections, MIDI IN/OUT cables, wireless pairing, Patch mode and Device ID. " + op->failureReason);
    }
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
    m_sendQueue.push_back(Outgoing{request.encode(), id, request, DataSetBatchId{}});
    logSystem(LogKind::Operation, LogSeverity::Info,
              "Request #" + std::to_string(id.value) + " queued: " + request.summary(), {}, id.value);
    emit operationChanged(id.value);
    scheduleSend();
    return id;
}

DeviceSession::DataSetBatchId DeviceSession::sendDataSets(const std::vector<roland::RolandSysExMessage>& messages)
{
    if (m_state != ConnectionState::Connected) {
        logSystem(LogKind::Operation, LogSeverity::Warning, "Cannot send data: not connected");
        return DataSetBatchId{};
    }
    if (messages.empty()) {
        return DataSetBatchId{};
    }
    for (const auto& message : messages) {
        if (!message.isDataSet()) {
            logSystem(LogKind::Operation, LogSeverity::Error, "sendDataSets refused: not every message is a DT1");
            return DataSetBatchId{};
        }
    }

    const DataSetBatchId id{m_nextBatchId++};
    m_dataSetBatches[id.value] = DataSetBatch{messages.size(), false, {}};
    for (const auto& message : messages) {
        m_sendQueue.push_back(Outgoing{message.encode(), protocol::RequestId{}, message, id});
    }
    logSystem(LogKind::Operation, LogSeverity::Info,
              "Queued " + std::to_string(messages.size()) + " DT1 message(s) to "
                  + messages.front().address().toHexString());
    // Start sending on the next turn of the event loop, never inside this
    // call: with a short pacing interval the whole batch would otherwise
    // finish — and dataSetBatchFinished fire — before the caller had received
    // the batch id it needs to recognise its own batch.
    QMetaObject::invokeMethod(this, [this] { scheduleSend(); }, Qt::QueuedConnection);
    return id;
}

bool DeviceSession::cancelDataSetBatch(DataSetBatchId id)
{
    if (!m_dataSetBatches.contains(id.value)) {
        return false;
    }
    finishDataSetBatch(id, false, "Cancelled");
    return true;
}

void DeviceSession::finishDataSetBatch(DataSetBatchId id, bool ok, std::string error)
{
    const auto it = m_dataSetBatches.find(id.value);
    if (it == m_dataSetBatches.end()) {
        return;
    }
    m_dataSetBatches.erase(it);
    // Drop any of this batch's messages still queued behind a failure.
    if (!ok) {
        for (auto queued = m_sendQueue.begin(); queued != m_sendQueue.end();) {
            queued = queued->batchId == id ? m_sendQueue.erase(queued) : std::next(queued);
        }
    }
    emit dataSetBatchFinished(id.value, ok, QString::fromStdString(error));
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
        if (outgoing.batchId.isValid()) {
            finishDataSetBatch(outgoing.batchId, false, error.message);
        }
        endConnection(ConnectionState::Error, "MIDI send failed: " + error.message);
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
    if (outgoing.batchId.isValid()) {
        const auto batch = m_dataSetBatches.find(outgoing.batchId.value);
        if (batch != m_dataSetBatches.end() && --batch->second.remaining == 0) {
            finishDataSetBatch(outgoing.batchId, true, {});
        }
    }
    emit statisticsChanged();
    updateTimeoutTimer();
    scheduleSend();
}

// ---------------------------------------------------------------------------
// Patch fetch
// ---------------------------------------------------------------------------

bool DeviceSession::fetchTemporaryPatch(PatchFetchPurpose purpose)
{
    return fetchPatch(xpmodel::Xp60PatchLayout::temporaryPatchAddress(), purpose);
}

bool DeviceSession::fetchPatch(const roland::RolandAddress& patchBase, PatchFetchPurpose purpose)
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
    m_patchFetch.purpose = purpose;
    m_patchFetch.state = PatchFetchState::InProgress;
    m_patchFetch.base = patchBase;
    m_patchFetch.totalBlocks = plan.size();
    m_patchFetch.message = "Reading " + std::to_string(plan.size()) + " blocks from " + patchBase.toHexString();
    m_patchFetchPlan = plan;

    // Block reads are issued one at a time, each sent only after the previous
    // block's reply has completed.
    //
    // The XP-60 cannot be pipelined. A 129-byte Tone block reply is 140 bytes
    // on the wire, which at the MIDI DIN rate of 31250 baud takes about 45 ms
    // to transmit (measured: 53 ms from request to complete reply). Queueing
    // all five RQ1s and spacing them only by interMessageDelay asks the
    // instrument to receive further requests while it is still transmitting,
    // and it drops them: with the former 20 ms default the last block read
    // timed out every time (hardware, 2026-09-04).
    //
    // Waiting for each reply is used rather than a larger delay because the
    // safe delay is a property of the link, not of the instrument -- the
    // measured cliff sat between 30 and 33 ms on a USB-MIDI cable and would
    // differ again over Bluetooth. Serialising is correct on any link.
    if (!requestNextPatchBlock()) {
        return false;
    }
    logSystem(LogKind::Operation, LogSeverity::Info, "Patch fetch started: " + m_patchFetch.message);
    emit patchFetchChanged();
    return true;
}

// Issues the next outstanding block read. Returns false when the send failed,
// having already marked the fetch failed.
bool DeviceSession::requestNextPatchBlock()
{
    if (m_patchFetch.requests.size() >= m_patchFetchPlan.size()) {
        return true;
    }
    const auto& request = m_patchFetchPlan[m_patchFetch.requests.size()];
    // sendDataRequest() emits operationChanged, which re-enters
    // updatePatchFetch(); the guard keeps that from issuing a second block.
    m_patchFetchAdvancing = true;
    const auto id = sendDataRequest(request.address, request.size);
    m_patchFetchAdvancing = false;
    if (!id.isValid()) {
        m_patchFetch.state = PatchFetchState::Failed;
        m_patchFetch.message = m_lastError.empty()
            ? "Could not queue the request for " + std::string(request.block.name)
            : m_lastError;
        for (const auto other : m_patchFetch.requests) {
            const auto* op = m_tracker.find(other);
            if (op && !protocol::isTerminal(op->state)) {
                cancelRequest(other);
            }
        }
        emit patchFetchChanged();
        return false;
    }
    m_patchFetch.requests.push_back(id);

    // A transport failure marks the request terminal from inside
    // sendDataRequest(), while the re-entry guard above was suppressing
    // updatePatchFetch(). Nothing else would notice it, so check here.
    const auto* op = m_tracker.find(id);
    if (op && protocol::isTerminal(op->state) && op->state != protocol::RequestState::Completed) {
        m_patchFetch.state = PatchFetchState::Failed;
        m_patchFetch.message = "Block read at " + request.address.toHexString() + " "
            + std::string(protocol::requestStateLabel(op->state))
            + (op->failureReason.empty() ? std::string() : ": " + op->failureReason);
        logSystem(LogKind::Operation, LogSeverity::Error, "Patch fetch failed: " + m_patchFetch.message);
        emit patchFetchChanged();
        return false;
    }
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
    // Re-entered by the operationChanged emitted from within
    // requestNextPatchBlock(); that block has not been answered yet.
    if (m_patchFetchAdvancing) {
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
    // Every block issued so far is complete; ask for the next one.
    if (m_patchFetch.requests.size() < m_patchFetchPlan.size()) {
        if (requestNextPatchBlock()) {
            emit patchFetchChanged();
        }
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

// Bank Select and Program Change arriving on MIDI IN mean a Patch was selected
// somewhere upstream — most often on the XP-60's own front panel, which
// transmits both unless the Tx Program Change / Tx Bank Select switches are OFF
// (Owner's Manual p.218-219).
//
// That matters far beyond diagnostics. Selecting a Patch on the instrument
// *replaces the temporary area* (Owner's Manual p.45), so every belief the
// application holds about what the XP-60 is currently sounding becomes worthless
// at that instant. Watching for these two messages is the only way to learn it
// without polling, and it costs no traffic at all.
//
// This reports the observation as a fact and stops there. Whether it invalidates
// anything is the workspace's decision, not the transport's.
void DeviceSession::noticePatchSelection(midi::MidiByteSpan bytes)
{
    if (bytes.size() < 2) {
        return;
    }
    const auto status = static_cast<unsigned>(bytes[0]);
    const int channel = static_cast<int>(status & 0x0FU) + 1;

    if ((status & 0xF0U) == 0xC0U) { // Program Change
        emit patchSelectionObserved(channel, static_cast<int>(bytes[1]) + 1);
        return;
    }
    if ((status & 0xF0U) == 0xB0U && bytes.size() >= 3) { // Control Change
        const auto controller = static_cast<unsigned>(bytes[1]);
        // 0 = Bank Select MSB, 32 = LSB. A bank select alone does not change the
        // sound — the instrument acts on it at the following Program Change —
        // but it is part of the same selection and is reported the same way.
        if (controller == 0U || controller == 32U) {
            emit patchSelectionObserved(channel, -1);
        }
    }
}

void DeviceSession::handleIncomingMessage(midi::MidiBytes bytes)
{
    ++m_statistics.messagesIn;
    const midi::MidiByteSpan span(bytes.data(), bytes.size());
    const roland::ByteSpan rspan(bytes.data(), bytes.size());
    const auto now = m_steadyClock();
    const auto wall = m_wallClock();

    if (!midi::isCompleteSysEx(span)) {
        appendLog(diagnostics::logRawMidi(LogDirection::In, span, inputName(), wall));
        noticePatchSelection(span);
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
            setLinkState(LinkState::Responding, "A complete Roland reply matched the request, Device ID and checksum. The XP-60 MIDI path responded.");
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
    if (m_state == ConnectionState::Connected && !m_endingConnection) endConnection(ConnectionState::Error, error.message);
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
        setLinkState(LinkState::Failed, "The XP-60 did not reply in time. Check the MIDI ports, wireless link and Device ID; retry with conservative pacing if needed.");
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
