#pragma once

#include "midi/IMidiTransport.h"

#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace xp60studio::midi {

// In-memory IMidiTransport used by deterministic tests and by the application
// when no backend is available. Everything sent is recorded; incoming data is
// injected explicitly by the test.
class LoopbackMidiTransport final : public IMidiTransport
{
public:
    LoopbackMidiTransport();

    // Test configuration -----------------------------------------------------
    void setInputs(std::vector<MidiEndpointInfo> inputs);
    void setOutputs(std::vector<MidiEndpointInfo> outputs);
    void addInput(const std::string& id, const std::string& displayName);
    void addOutput(const std::string& id, const std::string& displayName);
    void failNextOpen(TransportError error);
    void failNextSend(TransportError error);

    // Simulates a backend delivering bytes (possibly a fragment of a message).
    void injectIncoming(MidiByteSpan bytes, std::int64_t timestampNanoseconds = 0);
    void simulateEndpointsChanged();
    void simulateError(const TransportError& error);

    [[nodiscard]] const std::vector<MidiBytes>& sentMessages() const noexcept { return m_sent; }
    void clearSentMessages() { m_sent.clear(); }

    // IMidiTransport ---------------------------------------------------------
    [[nodiscard]] std::string backendName() const override { return "Loopback"; }
    [[nodiscard]] std::vector<MidiEndpointInfo> enumerateInputs() override;
    [[nodiscard]] std::vector<MidiEndpointInfo> enumerateOutputs() override;
    [[nodiscard]] TransportError openInput(const std::string& endpointId) override;
    [[nodiscard]] TransportError openOutput(const std::string& endpointId) override;
    void closeInput() override;
    void closeOutput() override;
    [[nodiscard]] bool isInputOpen() const override { return m_openInput.has_value(); }
    [[nodiscard]] bool isOutputOpen() const override { return m_openOutput.has_value(); }
    [[nodiscard]] std::optional<MidiEndpointInfo> openInputEndpoint() const override { return m_openInput; }
    [[nodiscard]] std::optional<MidiEndpointInfo> openOutputEndpoint() const override { return m_openOutput; }
    [[nodiscard]] TransportError send(MidiByteSpan message) override;
    void setReceiveHandler(ReceiveHandler handler) override;
    void setErrorHandler(ErrorHandler handler) override;
    void setEndpointsChangedHandler(EndpointsChangedHandler handler) override;

private:
    std::vector<MidiEndpointInfo> m_inputs;
    std::vector<MidiEndpointInfo> m_outputs;
    std::optional<MidiEndpointInfo> m_openInput;
    std::optional<MidiEndpointInfo> m_openOutput;
    std::optional<TransportError> m_nextOpenError;
    std::optional<TransportError> m_nextSendError;
    std::vector<MidiBytes> m_sent;
    ReceiveHandler m_receive;
    ErrorHandler m_error;
    EndpointsChangedHandler m_endpointsChanged;
};

} // namespace xp60studio::midi
