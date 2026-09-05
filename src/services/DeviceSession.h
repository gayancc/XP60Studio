#pragma once

#include "diagnostics/ProtocolLogEntry.h"
#include "midi/IMidiTransport.h"
#include "midi/SysExAssembler.h"
#include "protocol/RolandRequestTracker.h"
#include "protocol/TransferPacing.h"
#include "roland/RolandAddress.h"
#include "roland/RolandDeviceId.h"
#include "roland/RolandModelId.h"
#include "roland/RolandSize.h"
#include "roland/RolandSysExMessage.h"
#include "xpmodel/Xp60PatchCodec.h"

#include <QObject>
#include <QTimer>
#include <QFutureWatcher>
#include <atomic>

#include <chrono>
#include <deque>
#include <map>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace xp60studio::services {

// Orchestrates one XP-60 connection: endpoint selection, the send queue with
// pacing, the receive pipeline (assembly -> decode -> correlation -> log) and
// request timeouts.
//
// Threading: transport callbacks may arrive on a backend thread. They are
// reduced to complete MIDI messages inside the callback (SysExAssembler) and
// then queued to the thread owning this QObject. Everything else, including
// all signals, happens on that thread.
class DeviceSession : public QObject
{
    Q_OBJECT

public:
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Connected,
        Error,
    };
    enum class LinkState { Unchecked, Checking, Responding, Failed };
    LinkState linkState() const { return m_linkState; }
    std::string linkMessage() const { return m_linkMessage; }
    bool testConnection();

    struct Statistics
    {
        std::uint64_t messagesIn = 0;
        std::uint64_t messagesOut = 0;
        std::uint64_t sysExIn = 0;
        std::uint64_t sysExOut = 0;
        std::uint64_t rolandMessagesIn = 0;
        std::uint64_t checksumFailures = 0;
        std::uint64_t parseFailures = 0;
        std::uint64_t unsolicitedDataSets = 0;
        std::uint64_t requestsSent = 0;
        std::uint64_t requestsCompleted = 0;
        std::uint64_t requestsTimedOut = 0;
        std::uint64_t requestsCancelled = 0;
        std::uint64_t requestsFailed = 0;
        std::uint64_t transportErrors = 0;
    };

    // A batch of DT1 messages queued together. DT1 has no reply, so the batch
    // finishes when the last message has left the transport.
    struct DataSetBatchId
    {
        std::uint64_t value = 0;
        [[nodiscard]] constexpr bool isValid() const noexcept { return value != 0; }
        friend constexpr auto operator<=>(const DataSetBatchId&, const DataSetBatchId&) noexcept = default;
    };

    // Phase 2 inspection aid: reading one whole Patch block by block.
    enum class PatchFetchState {
        Idle,
        InProgress,
        Completed,
        Failed,
    };

    enum class PatchFetchPurpose { Editing, Transfer };

    struct PatchFetchStatus
    {
        PatchFetchState state = PatchFetchState::Idle;
        PatchFetchPurpose purpose = PatchFetchPurpose::Editing;
        roland::RolandAddress base;
        std::vector<protocol::RequestId> requests;  // one per block, layout order
        std::size_t completedBlocks = 0;
        std::size_t totalBlocks = 0;
        std::string message;                        // human readable outcome
        std::optional<xpmodel::Xp60Patch> patch;    // present when Completed
        std::string decodeReport;                   // warnings / errors from the codec
        // The exact DT1 messages the instrument sent for this Patch, in arrival
        // order. Preserved so a Patch read from the device can be stored with
        // the bytes that actually arrived rather than a re-encoding of them
        // (ARCHITECTURE.md §12, "original raw SysEx preservation") -- which is
        // what makes a device read a real backup.
        roland::ByteVector originalSysEx;
    };

    using SteadyClock = std::function<protocol::TimePoint()>;
    using WallClock = std::function<std::chrono::system_clock::time_point()>;

    explicit DeviceSession(std::unique_ptr<midi::IMidiTransport> transport, QObject* parent = nullptr);
    ~DeviceSession() override;

    // Configuration -----------------------------------------------------------
    [[nodiscard]] midi::IMidiTransport& transport() noexcept { return *m_transport; }
    [[nodiscard]] const midi::IMidiTransport& transport() const noexcept { return *m_transport; }

    [[nodiscard]] roland::RolandDeviceId deviceId() const noexcept { return m_deviceId; }
    void setDeviceId(roland::RolandDeviceId id);

    [[nodiscard]] const roland::RolandModelId& modelId() const noexcept { return m_modelId; }

    [[nodiscard]] protocol::TransferPacing pacing() const noexcept { return m_pacing; }
    void setPacing(const protocol::TransferPacing& pacing);

    // Time sources are injectable so tests can drive timeouts deterministically.
    void setClocks(SteadyClock steady, WallClock wall);
    // When false, the session never starts its own timeout timer; call
    // pollTimeouts() explicitly (tests).
    void setAutomaticTimeoutPolling(bool enabled);

    // Endpoints ---------------------------------------------------------------
    [[nodiscard]] std::vector<midi::MidiEndpointInfo> inputs() const { return m_inputs; }
    [[nodiscard]] std::vector<midi::MidiEndpointInfo> outputs() const { return m_outputs; }
    void refreshEndpoints();

    // Connection --------------------------------------------------------------
    [[nodiscard]] ConnectionState connectionState() const noexcept { return m_state; }
    [[nodiscard]] std::string lastError() const { return m_lastError; }
    [[nodiscard]] std::optional<midi::MidiEndpointInfo> connectedInput() const;
    [[nodiscard]] std::optional<midi::MidiEndpointInfo> connectedOutput() const;
    bool connectEndpoints(const std::string& inputId, const std::string& outputId);
    // Production UI uses the asynchronous path; synchronous entry remains for
    // deterministic fixtures and non-UI callers. Both share the same cleanup.
    void connectEndpointsAsync(const std::string& inputId, const std::string& outputId);
    void disconnectEndpoints();

    // Requests ----------------------------------------------------------------
    // Builds an RQ1 for the configured device/model and queues it. Returns an
    // invalid id when not connected.
    protocol::RequestId sendDataRequest(const roland::RolandAddress& address, const roland::RolandSize& size);
    bool cancelRequest(protocol::RequestId id);
    std::size_t cancelAllRequests();
    [[nodiscard]] const protocol::RolandRequestTracker& tracker() const noexcept { return m_tracker; }

    // Patch fetch ---------------------------------------------------------------
    // Issues the RQ1s of Xp60PatchLayout::fetchPlan(base). False when not
    // connected or a fetch is already running.
    // Queues DT1 messages for transmission with the configured pacing.
    // Returns an invalid id when not connected or the list is empty.
    // dataSetBatchFinished() reports the outcome.
    DataSetBatchId sendDataSets(const std::vector<roland::RolandSysExMessage>& messages);
    bool cancelDataSetBatch(DataSetBatchId id);
    [[nodiscard]] std::size_t pendingDataSetBatches() const noexcept { return m_dataSetBatches.size(); }

    bool fetchPatch(const roland::RolandAddress& patchBase, PatchFetchPurpose purpose = PatchFetchPurpose::Editing);
    bool fetchTemporaryPatch(PatchFetchPurpose purpose = PatchFetchPurpose::Editing);
    void cancelPatchFetch();
    [[nodiscard]] const PatchFetchStatus& patchFetch() const noexcept { return m_patchFetch; }

    // Diagnostics -------------------------------------------------------------
    [[nodiscard]] const Statistics& statistics() const noexcept { return m_statistics; }
    [[nodiscard]] const std::deque<diagnostics::ProtocolLogEntry>& log() const noexcept { return m_log; }
    void setLogLimit(std::size_t limit);
    void clearLog();
    [[nodiscard]] std::size_t pendingSendCount() const noexcept { return m_sendQueue.size(); }

public slots:
    // Applies request timeouts using the injected steady clock.
    void pollTimeouts();
    // Sends the next queued message if pacing allows.
    void pumpSendQueue();

signals:
    void endpointsChanged();
    void connectionStateChanged();
    void linkStateChanged();
    void deviceIdChanged();
    void pacingChanged();
    void logEntryAdded(const xp60studio::diagnostics::ProtocolLogEntry& entry);
    void logCleared();
    void operationChanged(quint64 requestId);
    void statisticsChanged();
    void patchFetchChanged();
    void dataSetBatchFinished(quint64 batchId, bool ok, const QString& error);
    // A Bank Select or Program Change arrived on MIDI IN, so a Patch was
    // selected upstream — typically on the XP-60's own front panel, which
    // transmits both (Owner's Manual p.218-219). `program` is 1-128, or -1 for a
    // Bank Select with no Program Change yet.
    //
    // Selecting a Patch replaces the instrument's temporary area (Owner's Manual
    // p.45), so a listener that believed it knew what the XP-60 held no longer
    // does. Reported as an observation; acting on it is the listener's business.
    void patchSelectionObserved(int channel, int program);

private:
    struct Outgoing
    {
        midi::MidiBytes bytes;
        protocol::RequestId requestId;
        std::optional<roland::RolandSysExMessage> roland;
        DataSetBatchId batchId;
    };

    struct DataSetBatch
    {
        std::size_t remaining = 0;
        bool failed = false;
        std::string error;
    };

    void finishDataSetBatch(DataSetBatchId id, bool ok, std::string error);

    void handleIncomingMessage(midi::MidiBytes bytes);
    void noticePatchSelection(midi::MidiByteSpan bytes);
    void updatePatchFetch();
    bool requestNextPatchBlock();
    void handleTransportError(midi::TransportError error);
    void handleEndpointsChanged();
    void checkOpenEndpoints();
    void endConnection(ConnectionState state, const std::string& reason);
    midi::TransportError openEndpoints(const std::string& inputId, const std::string& outputId);
    bool finishConnection(const midi::TransportError& error);
    void updateConnectionTest();
    void setLinkState(LinkState state, std::string message);
    void setState(ConnectionState state, std::string error = {});
    void appendLog(diagnostics::ProtocolLogEntry entry);
    void logSystem(diagnostics::LogKind kind, diagnostics::LogSeverity severity, std::string summary,
                   std::string detail = {}, std::optional<std::uint64_t> requestId = std::nullopt);
    void scheduleSend();
    void updateTimeoutTimer();
    [[nodiscard]] std::string inputName() const;
    [[nodiscard]] std::string outputName() const;

    std::unique_ptr<midi::IMidiTransport> m_transport;
    roland::RolandDeviceId m_deviceId;
    roland::RolandModelId m_modelId;
    std::vector<roland::RolandModelId> m_knownModelIds;
    protocol::TransferPacing m_pacing;
    protocol::RolandRequestTracker m_tracker;

    SteadyClock m_steadyClock;
    WallClock m_wallClock;

    std::vector<midi::MidiEndpointInfo> m_inputs;
    std::vector<midi::MidiEndpointInfo> m_outputs;
    ConnectionState m_state = ConnectionState::Disconnected;
    std::string m_lastError;
    std::atomic<std::uint64_t> m_connectionEpoch{0};
    QFutureWatcher<midi::TransportError> m_connectionOpen;
    bool m_openPending = false;
    bool m_cancelOpen = false;
    bool m_endingConnection = false;
    LinkState m_linkState = LinkState::Unchecked;
    std::string m_linkMessage = "Open both MIDI ports, then test the connection.";
    protocol::RequestId m_connectionTest;

    std::mutex m_assemblerMutex;
    midi::SysExAssembler m_assembler;

    std::deque<Outgoing> m_sendQueue;
    std::map<std::uint64_t, DataSetBatch> m_dataSetBatches;
    std::uint64_t m_nextBatchId = 1;
    std::optional<protocol::TimePoint> m_lastSendAt;
    QTimer m_sendTimer;
    QTimer m_timeoutTimer;
    bool m_automaticTimeoutPolling = true;

    PatchFetchStatus m_patchFetch;
    // Remaining blocks of the fetch in progress. Block reads are issued one at
    // a time: see fetchPatch() for why the XP-60 cannot be pipelined.
    std::vector<xpmodel::Xp60PatchLayout::ReadRequest> m_patchFetchPlan;
    bool m_patchFetchAdvancing = false;

    Statistics m_statistics;
    std::deque<diagnostics::ProtocolLogEntry> m_log;
    std::size_t m_logLimit = 2000;
};

[[nodiscard]] std::string_view connectionStateName(DeviceSession::ConnectionState state) noexcept;

} // namespace xp60studio::services
