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

#include <QObject>
#include <QTimer>

#include <chrono>
#include <deque>
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
    void disconnectEndpoints();

    // Requests ----------------------------------------------------------------
    // Builds an RQ1 for the configured device/model and queues it. Returns an
    // invalid id when not connected.
    protocol::RequestId sendDataRequest(const roland::RolandAddress& address, const roland::RolandSize& size);
    bool cancelRequest(protocol::RequestId id);
    std::size_t cancelAllRequests();
    [[nodiscard]] const protocol::RolandRequestTracker& tracker() const noexcept { return m_tracker; }

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
    void deviceIdChanged();
    void pacingChanged();
    void logEntryAdded(const xp60studio::diagnostics::ProtocolLogEntry& entry);
    void logCleared();
    void operationChanged(quint64 requestId);
    void statisticsChanged();

private:
    struct Outgoing
    {
        midi::MidiBytes bytes;
        protocol::RequestId requestId;
        std::optional<roland::RolandSysExMessage> roland;
    };

    void handleIncomingMessage(midi::MidiBytes bytes);
    void handleTransportError(midi::TransportError error);
    void handleEndpointsChanged();
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

    std::mutex m_assemblerMutex;
    midi::SysExAssembler m_assembler;

    std::deque<Outgoing> m_sendQueue;
    std::optional<protocol::TimePoint> m_lastSendAt;
    QTimer m_sendTimer;
    QTimer m_timeoutTimer;
    bool m_automaticTimeoutPolling = true;

    Statistics m_statistics;
    std::deque<diagnostics::ProtocolLogEntry> m_log;
    std::size_t m_logLimit = 2000;
};

[[nodiscard]] std::string_view connectionStateName(DeviceSession::ConnectionState state) noexcept;

} // namespace xp60studio::services
