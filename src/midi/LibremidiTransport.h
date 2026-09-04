#pragma once

#include "midi/IMidiTransport.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace xp60studio::midi {

// IMidiTransport implementation backed by libremidi 5.x.
//
// Observers enumerate native MIDI ports and, when built, Windows Runtime
// ports (including OS-paired Bluetooth MIDI). Input and output use the API of
// their selected endpoint; duplicate APIs remain explicitly labelled. libremidi
// recombines SysEx internally for MIDI 1 backends, but the received bytes are
// still passed through the caller's SysExAssembler so platform differences in
// fragmentation never reach the protocol layer.
//
// Only this translation unit includes libremidi headers.
class LibremidiTransport final : public IMidiTransport
{
public:
    LibremidiTransport();
    ~LibremidiTransport() override;

    LibremidiTransport(const LibremidiTransport&) = delete;
    LibremidiTransport& operator=(const LibremidiTransport&) = delete;

    // "libremidi 5.4.3 / ALSA (sequencer)" etc.
    [[nodiscard]] std::string backendName() const override;
    [[nodiscard]] static std::string libraryVersion();

    [[nodiscard]] std::vector<MidiEndpointInfo> enumerateInputs() override;
    [[nodiscard]] std::vector<MidiEndpointInfo> enumerateOutputs() override;
    [[nodiscard]] TransportError openInput(const std::string& endpointId) override;
    [[nodiscard]] TransportError openOutput(const std::string& endpointId) override;
    void closeInput() override;
    void closeOutput() override;
    [[nodiscard]] bool isInputOpen() const override;
    [[nodiscard]] bool isOutputOpen() const override;
    [[nodiscard]] std::optional<MidiEndpointInfo> openInputEndpoint() const override;
    [[nodiscard]] std::optional<MidiEndpointInfo> openOutputEndpoint() const override;
    [[nodiscard]] TransportError send(MidiByteSpan message) override;
    void setReceiveHandler(ReceiveHandler handler) override;
    void setErrorHandler(ErrorHandler handler) override;
    void setEndpointsChangedHandler(EndpointsChangedHandler handler) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xp60studio::midi
