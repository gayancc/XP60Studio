#include "midi/LibremidiTransport.h"

#include <libremidi/libremidi.hpp>

#include <algorithm>
#include <sstream>

namespace xp60studio::midi {

namespace {

std::string apiDisplayName(libremidi::API api)
{
    return std::string(libremidi::get_api_display_name(api));
}

// Best-effort stable identity for a port: backend + port name + backend handle.
std::string makeEndpointId(const libremidi::port_information& port, EndpointDirection direction)
{
    std::ostringstream out;
    out << (direction == EndpointDirection::Input ? "in" : "out") << '|' << libremidi::get_api_name(port.api)
        << '|' << port.port_name << '|' << port.port;
    return out.str();
}

std::string makeDisplayName(const libremidi::port_information& port)
{
    if (!port.display_name.empty()) {
        return port.display_name;
    }
    if (!port.device_name.empty() && !port.port_name.empty() && port.device_name != port.port_name) {
        return port.device_name + " - " + port.port_name;
    }
    if (!port.port_name.empty()) {
        return port.port_name;
    }
    return port.device_name.empty() ? "MIDI port" : port.device_name;
}

bool isVirtualPort(const libremidi::port_information& port)
{
    return (port.type & libremidi::transport_type::software) != 0
        || (port.type & libremidi::transport_type::loopback) != 0;
}

TransportError fromLibremidi(TransportErrorCode code, std::string_view context, const stdx::error& error)
{
    std::string message(context);
    const auto text = error.message();
    if (!text.empty()) {
        message += ": ";
        message.append(text.data(), text.size());
    }
    return TransportError::make(code, std::move(message));
}

} // namespace

struct LibremidiTransport::Impl
{
    Impl()
    {
        libremidi::observer_configuration conf;
        conf.track_hardware = true;
        conf.track_virtual = true;
        conf.track_any = true;
        conf.notify_in_constructor = false;
        conf.input_added = [this](const libremidi::input_port&) { notifyEndpointsChanged(); };
        conf.input_removed = [this](const libremidi::input_port&) { notifyEndpointsChanged(); };
        conf.output_added = [this](const libremidi::output_port&) { notifyEndpointsChanged(); };
        conf.output_removed = [this](const libremidi::output_port&) { notifyEndpointsChanged(); };
        conf.on_error = [this](std::string_view text, const libremidi::source_location&) {
            notifyError(TransportError::make(TransportErrorCode::Internal, "MIDI observer: " + std::string(text)));
        };
        conf.on_warning = [](std::string_view, const libremidi::source_location&) {};

        observer = std::make_unique<libremidi::observer>(conf, libremidi::midi1::observer_default_configuration());
        api = observer->get_current_api();
    }

    void notifyEndpointsChanged()
    {
        EndpointsChangedHandler handler;
        {
            std::lock_guard lock(handlerMutex);
            handler = endpointsChanged;
        }
        if (handler) {
            handler();
        }
    }

    void notifyError(const TransportError& error)
    {
        ErrorHandler handler;
        {
            std::lock_guard lock(handlerMutex);
            handler = onError;
        }
        if (handler) {
            handler(error);
        }
    }

    void notifyReceive(const MidiEvent& event)
    {
        ReceiveHandler handler;
        {
            std::lock_guard lock(handlerMutex);
            handler = onReceive;
        }
        if (handler) {
            handler(event);
        }
    }

    void ensureInput()
    {
        if (input) {
            return;
        }
        libremidi::input_configuration conf;
        conf.on_message = [this](libremidi::message&& message) {
            MidiEvent event;
            event.bytes.assign(message.bytes.begin(), message.bytes.end());
            event.timestampNanoseconds = message.timestamp;
            notifyReceive(event);
        };
        conf.on_error = [this](std::string_view text, const libremidi::source_location&) {
            notifyError(TransportError::make(TransportErrorCode::Internal, "MIDI input: " + std::string(text)));
        };
        conf.on_warning = [](std::string_view, const libremidi::source_location&) {};
        conf.ignore_sysex = false;
        conf.ignore_timing = true;
        conf.ignore_sensing = true;
        conf.timestamps = libremidi::timestamp_mode::SystemMonotonic;
        input = std::make_unique<libremidi::midi_in>(conf, libremidi::midi_in_configuration_for(*observer));
    }

    void ensureOutput()
    {
        if (output) {
            return;
        }
        libremidi::output_configuration conf;
        conf.on_error = [this](std::string_view text, const libremidi::source_location&) {
            notifyError(TransportError::make(TransportErrorCode::Internal, "MIDI output: " + std::string(text)));
        };
        conf.on_warning = [](std::string_view, const libremidi::source_location&) {};
        output = std::make_unique<libremidi::midi_out>(conf, libremidi::midi_out_configuration_for(*observer));
    }

    std::vector<MidiEndpointInfo> enumerate(EndpointDirection direction)
    {
        std::vector<MidiEndpointInfo> result;
        std::lock_guard lock(portMutex);
        if (direction == EndpointDirection::Input) {
            inputPorts = observer->get_input_ports();
            for (const auto& port : inputPorts) {
                result.push_back(MidiEndpointInfo{makeEndpointId(port, direction), makeDisplayName(port),
                                                  apiDisplayName(port.api), direction, isVirtualPort(port)});
            }
        } else {
            outputPorts = observer->get_output_ports();
            for (const auto& port : outputPorts) {
                result.push_back(MidiEndpointInfo{makeEndpointId(port, direction), makeDisplayName(port),
                                                  apiDisplayName(port.api), direction, isVirtualPort(port)});
            }
        }
        return result;
    }

    libremidi::API api{};
    std::unique_ptr<libremidi::observer> observer;
    std::unique_ptr<libremidi::midi_in> input;
    std::unique_ptr<libremidi::midi_out> output;

    std::mutex portMutex;
    std::vector<libremidi::input_port> inputPorts;
    std::vector<libremidi::output_port> outputPorts;
    std::optional<MidiEndpointInfo> openInput;
    std::optional<MidiEndpointInfo> openOutput;

    std::mutex handlerMutex;
    ReceiveHandler onReceive;
    ErrorHandler onError;
    EndpointsChangedHandler endpointsChanged;
};

LibremidiTransport::LibremidiTransport()
    : m_impl(std::make_unique<Impl>())
{
}

LibremidiTransport::~LibremidiTransport()
{
    closeInput();
    closeOutput();
}

std::string LibremidiTransport::libraryVersion()
{
    return std::string(libremidi::get_version());
}

std::string LibremidiTransport::backendName() const
{
    return "libremidi " + libraryVersion() + " / " + apiDisplayName(m_impl->api);
}

std::vector<MidiEndpointInfo> LibremidiTransport::enumerateInputs()
{
    return m_impl->enumerate(EndpointDirection::Input);
}

std::vector<MidiEndpointInfo> LibremidiTransport::enumerateOutputs()
{
    return m_impl->enumerate(EndpointDirection::Output);
}

TransportError LibremidiTransport::openInput(const std::string& endpointId)
{
    // Refresh so a freshly plugged device can be opened right away.
    const auto inputs = enumerateInputs();
    const auto it = std::find_if(inputs.begin(), inputs.end(), [&](const auto& e) { return e.id == endpointId; });
    if (it == inputs.end()) {
        return TransportError::make(TransportErrorCode::EndpointNotFound, "MIDI input '" + endpointId + "' is not available");
    }
    const auto index = static_cast<std::size_t>(std::distance(inputs.begin(), it));

    std::lock_guard lock(m_impl->portMutex);
    m_impl->ensureInput();
    if (m_impl->input->is_port_open()) {
        m_impl->input->close_port();
    }
    if (const auto err = m_impl->input->open_port(m_impl->inputPorts[index], "XP60Studio In"); err != stdx::error{}) {
        m_impl->openInput.reset();
        return fromLibremidi(TransportErrorCode::OpenFailed, "Could not open MIDI input '" + it->displayName + "'", err);
    }
    m_impl->openInput = *it;
    return TransportError::none();
}

TransportError LibremidiTransport::openOutput(const std::string& endpointId)
{
    const auto outputs = enumerateOutputs();
    const auto it = std::find_if(outputs.begin(), outputs.end(), [&](const auto& e) { return e.id == endpointId; });
    if (it == outputs.end()) {
        return TransportError::make(TransportErrorCode::EndpointNotFound, "MIDI output '" + endpointId + "' is not available");
    }
    const auto index = static_cast<std::size_t>(std::distance(outputs.begin(), it));

    std::lock_guard lock(m_impl->portMutex);
    m_impl->ensureOutput();
    if (m_impl->output->is_port_open()) {
        m_impl->output->close_port();
    }
    if (const auto err = m_impl->output->open_port(m_impl->outputPorts[index], "XP60Studio Out"); err != stdx::error{}) {
        m_impl->openOutput.reset();
        return fromLibremidi(TransportErrorCode::OpenFailed, "Could not open MIDI output '" + it->displayName + "'", err);
    }
    m_impl->openOutput = *it;
    return TransportError::none();
}

void LibremidiTransport::closeInput()
{
    std::lock_guard lock(m_impl->portMutex);
    if (m_impl->input && m_impl->input->is_port_open()) {
        m_impl->input->close_port();
    }
    m_impl->openInput.reset();
}

void LibremidiTransport::closeOutput()
{
    std::lock_guard lock(m_impl->portMutex);
    if (m_impl->output && m_impl->output->is_port_open()) {
        m_impl->output->close_port();
    }
    m_impl->openOutput.reset();
}

bool LibremidiTransport::isInputOpen() const
{
    std::lock_guard lock(m_impl->portMutex);
    return m_impl->openInput.has_value();
}

bool LibremidiTransport::isOutputOpen() const
{
    std::lock_guard lock(m_impl->portMutex);
    return m_impl->openOutput.has_value();
}

std::optional<MidiEndpointInfo> LibremidiTransport::openInputEndpoint() const
{
    std::lock_guard lock(m_impl->portMutex);
    return m_impl->openInput;
}

std::optional<MidiEndpointInfo> LibremidiTransport::openOutputEndpoint() const
{
    std::lock_guard lock(m_impl->portMutex);
    return m_impl->openOutput;
}

TransportError LibremidiTransport::send(MidiByteSpan message)
{
    if (message.empty()) {
        return TransportError::make(TransportErrorCode::InvalidMessage, "Empty MIDI message");
    }
    std::lock_guard lock(m_impl->portMutex);
    if (!m_impl->output || !m_impl->openOutput) {
        return TransportError::make(TransportErrorCode::NotOpen, "MIDI output is not open");
    }
    if (const auto err = m_impl->output->send_message(message.data(), message.size()); err != stdx::error{}) {
        return fromLibremidi(TransportErrorCode::SendFailed, "MIDI send failed", err);
    }
    return TransportError::none();
}

void LibremidiTransport::setReceiveHandler(ReceiveHandler handler)
{
    std::lock_guard lock(m_impl->handlerMutex);
    m_impl->onReceive = std::move(handler);
}

void LibremidiTransport::setErrorHandler(ErrorHandler handler)
{
    std::lock_guard lock(m_impl->handlerMutex);
    m_impl->onError = std::move(handler);
}

void LibremidiTransport::setEndpointsChangedHandler(EndpointsChangedHandler handler)
{
    std::lock_guard lock(m_impl->handlerMutex);
    m_impl->endpointsChanged = std::move(handler);
}

} // namespace xp60studio::midi
