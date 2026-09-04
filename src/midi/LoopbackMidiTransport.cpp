#include "midi/LoopbackMidiTransport.h"

#include <algorithm>

namespace xp60studio::midi {

namespace {

std::optional<MidiEndpointInfo> findEndpoint(const std::vector<MidiEndpointInfo>& list, const std::string& id)
{
    const auto it = std::find_if(list.begin(), list.end(), [&](const MidiEndpointInfo& e) { return e.id == id; });
    if (it == list.end()) {
        return std::nullopt;
    }
    return *it;
}

} // namespace

LoopbackMidiTransport::LoopbackMidiTransport() = default;

void LoopbackMidiTransport::setInputs(std::vector<MidiEndpointInfo> inputs)
{
    m_inputs = std::move(inputs);
}

void LoopbackMidiTransport::setOutputs(std::vector<MidiEndpointInfo> outputs)
{
    m_outputs = std::move(outputs);
}

void LoopbackMidiTransport::addInput(const std::string& id, const std::string& displayName)
{
    m_inputs.push_back(MidiEndpointInfo{id, displayName, backendName(), EndpointDirection::Input, true});
}

void LoopbackMidiTransport::addOutput(const std::string& id, const std::string& displayName)
{
    m_outputs.push_back(MidiEndpointInfo{id, displayName, backendName(), EndpointDirection::Output, true});
}

void LoopbackMidiTransport::failNextOpen(TransportError error)
{
    m_nextOpenError = std::move(error);
}

void LoopbackMidiTransport::failNextSend(TransportError error)
{
    m_nextSendError = std::move(error);
}

void LoopbackMidiTransport::injectIncoming(MidiByteSpan bytes, std::int64_t timestampNanoseconds)
{
    if (m_receive) {
        m_receive(MidiEvent{MidiBytes(bytes.begin(), bytes.end()), timestampNanoseconds});
    }
}

void LoopbackMidiTransport::simulateEndpointsChanged()
{
    if (m_endpointsChanged) {
        m_endpointsChanged();
    }
}

void LoopbackMidiTransport::simulateError(const TransportError& error)
{
    if (m_error) {
        m_error(error);
    }
}

std::vector<MidiEndpointInfo> LoopbackMidiTransport::enumerateInputs()
{
    return m_inputs;
}

std::vector<MidiEndpointInfo> LoopbackMidiTransport::enumerateOutputs()
{
    return m_outputs;
}

TransportError LoopbackMidiTransport::openInput(const std::string& endpointId)
{
    if (m_nextOpenError) {
        auto error = *m_nextOpenError;
        m_nextOpenError.reset();
        return error;
    }
    const auto endpoint = findEndpoint(m_inputs, endpointId);
    if (!endpoint) {
        return TransportError::make(TransportErrorCode::EndpointNotFound, "No MIDI input with id '" + endpointId + "'");
    }
    m_openInput = endpoint;
    return TransportError::none();
}

TransportError LoopbackMidiTransport::openOutput(const std::string& endpointId)
{
    if (m_nextOpenError) {
        auto error = *m_nextOpenError;
        m_nextOpenError.reset();
        return error;
    }
    const auto endpoint = findEndpoint(m_outputs, endpointId);
    if (!endpoint) {
        return TransportError::make(TransportErrorCode::EndpointNotFound, "No MIDI output with id '" + endpointId + "'");
    }
    m_openOutput = endpoint;
    return TransportError::none();
}

void LoopbackMidiTransport::closeInput()
{
    m_openInput.reset();
}

void LoopbackMidiTransport::closeOutput()
{
    m_openOutput.reset();
}

TransportError LoopbackMidiTransport::send(MidiByteSpan message)
{
    if (!m_openOutput) {
        return TransportError::make(TransportErrorCode::NotOpen, "MIDI output is not open");
    }
    if (message.empty()) {
        return TransportError::make(TransportErrorCode::InvalidMessage, "Empty MIDI message");
    }
    if (m_nextSendError) {
        auto error = *m_nextSendError;
        m_nextSendError.reset();
        return error;
    }
    m_sent.emplace_back(message.begin(), message.end());
    return TransportError::none();
}

void LoopbackMidiTransport::setReceiveHandler(ReceiveHandler handler)
{
    m_receive = std::move(handler);
}

void LoopbackMidiTransport::setErrorHandler(ErrorHandler handler)
{
    m_error = std::move(handler);
}

void LoopbackMidiTransport::setEndpointsChangedHandler(EndpointsChangedHandler handler)
{
    m_endpointsChanged = std::move(handler);
}

} // namespace xp60studio::midi
