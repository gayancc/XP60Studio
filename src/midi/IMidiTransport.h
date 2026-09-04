#pragma once

#include "midi/MidiTypes.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace xp60studio::midi {

// Internal MIDI transport abstraction.
//
// Application code programs against this interface; libremidi (or any other
// backend) lives behind it. Inputs and outputs are opened independently
// because many interfaces expose them as separate endpoints.
//
// Threading contract: the handlers set through set*Handler may be invoked on a
// backend-owned thread. Implementations must not call handlers while holding
// internal locks, and callers must hand the data off to their own processing
// context quickly (see services::DeviceSession).
class IMidiTransport
{
public:
    using ReceiveHandler = std::function<void(const MidiEvent&)>;
    using ErrorHandler = std::function<void(const TransportError&)>;
    using EndpointsChangedHandler = std::function<void()>;

    virtual ~IMidiTransport() = default;

    [[nodiscard]] virtual std::string backendName() const = 0;

    // Enumeration
    [[nodiscard]] virtual std::vector<MidiEndpointInfo> enumerateInputs() = 0;
    [[nodiscard]] virtual std::vector<MidiEndpointInfo> enumerateOutputs() = 0;

    // Connection lifecycle
    [[nodiscard]] virtual TransportError openInput(const std::string& endpointId) = 0;
    [[nodiscard]] virtual TransportError openOutput(const std::string& endpointId) = 0;
    virtual void closeInput() = 0;
    virtual void closeOutput() = 0;
    void closeAll();

    [[nodiscard]] virtual bool isInputOpen() const = 0;
    [[nodiscard]] virtual bool isOutputOpen() const = 0;
    [[nodiscard]] virtual std::optional<MidiEndpointInfo> openInputEndpoint() const = 0;
    [[nodiscard]] virtual std::optional<MidiEndpointInfo> openOutputEndpoint() const = 0;

    // Sending. `send` transmits any complete MIDI message (channel, system
    // common or a complete SysEx). `sendSysEx` additionally validates the
    // F0 .. F7 framing before delegating to `send`.
    [[nodiscard]] virtual TransportError send(MidiByteSpan message) = 0;
    [[nodiscard]] TransportError sendSysEx(MidiByteSpan message);

    // Receiving / notifications
    virtual void setReceiveHandler(ReceiveHandler handler) = 0;
    virtual void setErrorHandler(ErrorHandler handler) = 0;
    virtual void setEndpointsChangedHandler(EndpointsChangedHandler handler) = 0;
};

} // namespace xp60studio::midi
