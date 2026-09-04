#include "midi/LibremidiTransport.h"

#include <libremidi/libremidi.hpp>

#include <algorithm>
#include <sstream>
#ifdef LIBREMIDI_WINUWP
#include <roapi.h>
#include <winrt/base.h>
#endif

namespace xp60studio::midi {

namespace {

struct RuntimeApartment {
#ifdef LIBREMIDI_WINUWP
    HRESULT result = RoInitialize(RO_INIT_MULTITHREADED);
    ~RuntimeApartment() { if (SUCCEEDED(result)) RoUninitialize(); }
#endif
};

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

        observers.push_back(std::make_unique<libremidi::observer>(conf, libremidi::midi1::observer_default_configuration()));
        api = observers.front()->get_current_api();
#ifdef LIBREMIDI_WINUWP
        if (api != libremidi::API::WINDOWS_UWP) {
            try {
                observers.push_back(std::make_unique<libremidi::observer>(conf, libremidi::API::WINDOWS_UWP));
            } catch (const std::exception& error) {
                startupWarning = "Windows Runtime MIDI unavailable: " + std::string(error.what());
            } catch (const winrt::hresult_error& error) {
                startupWarning = "Windows Runtime MIDI unavailable: " + winrt::to_string(error.message());
            }
        }
#endif
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

    void ensureInput(libremidi::API selectedApi)
    {
        if (input && input->get_current_api() == selectedApi) return;
        input.reset();
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
        input = std::make_unique<libremidi::midi_in>(conf, libremidi::midi_in_configuration_for(observerFor(selectedApi)));
    }

    void ensureOutput(libremidi::API selectedApi)
    {
        if (output && output->get_current_api() == selectedApi) return;
        output.reset();
        libremidi::output_configuration conf;
        conf.on_error = [this](std::string_view text, const libremidi::source_location&) {
            notifyError(TransportError::make(TransportErrorCode::Internal, "MIDI output: " + std::string(text)));
        };
        conf.on_warning = [](std::string_view, const libremidi::source_location&) {};
        output = std::make_unique<libremidi::midi_out>(conf, libremidi::midi_out_configuration_for(observerFor(selectedApi)));
    }

    libremidi::observer& observerFor(libremidi::API selectedApi)
    {
        for (auto& observer : observers) if (observer->get_current_api() == selectedApi) return *observer;
        throw std::runtime_error("The selected MIDI backend is unavailable");
    }

    std::vector<MidiEndpointInfo> enumerate(EndpointDirection direction)
    {
        std::vector<MidiEndpointInfo> result;
        std::lock_guard lock(portMutex);
        if (direction == EndpointDirection::Input) {
            inputPorts.clear();
            for (auto& observer : observers) {
                auto ports = observer->get_input_ports();
                inputPorts.insert(inputPorts.end(), ports.begin(), ports.end());
            }
            for (const auto& port : inputPorts) {
                result.push_back(MidiEndpointInfo{makeEndpointId(port, direction), makeDisplayName(port),
                                                  apiDisplayName(port.api), direction, isVirtualPort(port)});
            }
        } else {
            outputPorts.clear();
            for (auto& observer : observers) {
                auto ports = observer->get_output_ports();
                outputPorts.insert(outputPorts.end(), ports.begin(), ports.end());
            }
            for (const auto& port : outputPorts) {
                result.push_back(MidiEndpointInfo{makeEndpointId(port, direction), makeDisplayName(port),
                                                  apiDisplayName(port.api), direction, isVirtualPort(port)});
            }
        }
        return result;
    }

    RuntimeApartment apartment;
    libremidi::API api{};
    std::vector<std::unique_ptr<libremidi::observer>> observers;
    std::string startupWarning;
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
    setReceiveHandler({});
    setErrorHandler({});
    setEndpointsChangedHandler({});
    closeInput();
    closeOutput();
    m_impl->input.reset();
    m_impl->output.reset();
    // Observers may invoke callbacks during teardown; destroy them while the
    // callback state and its mutex still exist.
    m_impl->observers.clear();
}

std::string LibremidiTransport::libraryVersion()
{
    return std::string(libremidi::get_version());
}

std::string LibremidiTransport::backendName() const
{
    std::string result = "libremidi " + libraryVersion();
    for (const auto& observer : m_impl->observers) result += " / " + apiDisplayName(observer->get_current_api());
    if (!m_impl->startupWarning.empty()) result += " (" + m_impl->startupWarning + ")";
    return result;
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
try
{
    RuntimeApartment apartment;
    // Refresh so a freshly plugged device can be opened right away.
    const auto inputs = enumerateInputs();
    const auto it = std::find_if(inputs.begin(), inputs.end(), [&](const auto& e) { return e.id == endpointId; });
    if (it == inputs.end()) {
        return TransportError::make(TransportErrorCode::EndpointNotFound, "MIDI input '" + endpointId + "' is not available");
    }
    const auto index = static_cast<std::size_t>(std::distance(inputs.begin(), it));

    std::lock_guard lock(m_impl->portMutex);
    m_impl->ensureInput(m_impl->inputPorts[index].api);
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
#ifdef LIBREMIDI_WINUWP
catch (const winrt::hresult_error& error) {
    return TransportError::make(TransportErrorCode::OpenFailed, "MIDI input: " + winrt::to_string(error.message()));
}
#endif
catch (const std::exception& error) {
    return TransportError::make(TransportErrorCode::OpenFailed, error.what());
}

TransportError LibremidiTransport::openOutput(const std::string& endpointId)
try
{
    RuntimeApartment apartment;
    const auto outputs = enumerateOutputs();
    const auto it = std::find_if(outputs.begin(), outputs.end(), [&](const auto& e) { return e.id == endpointId; });
    if (it == outputs.end()) {
        return TransportError::make(TransportErrorCode::EndpointNotFound, "MIDI output '" + endpointId + "' is not available");
    }
    const auto index = static_cast<std::size_t>(std::distance(outputs.begin(), it));

    std::lock_guard lock(m_impl->portMutex);
    m_impl->ensureOutput(m_impl->outputPorts[index].api);
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
#ifdef LIBREMIDI_WINUWP
catch (const winrt::hresult_error& error) {
    return TransportError::make(TransportErrorCode::OpenFailed, "MIDI output: " + winrt::to_string(error.message()));
}
#endif
catch (const std::exception& error) {
    return TransportError::make(TransportErrorCode::OpenFailed, error.what());
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
try
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
#ifdef LIBREMIDI_WINUWP
catch (const winrt::hresult_error& error) {
    return TransportError::make(TransportErrorCode::SendFailed, "MIDI send: " + winrt::to_string(error.message()));
}
#endif
catch (const std::exception& error) {
    return TransportError::make(TransportErrorCode::SendFailed, error.what());
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
