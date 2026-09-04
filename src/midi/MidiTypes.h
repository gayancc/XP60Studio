#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Transport-level vocabulary. Nothing here knows about Roland.
namespace xp60studio::midi {

using Byte = std::uint8_t;
using MidiBytes = std::vector<Byte>;
using MidiByteSpan = std::span<const Byte>;

enum class EndpointDirection {
    Input,
    Output,
};

[[nodiscard]] std::string_view endpointDirectionName(EndpointDirection direction) noexcept;

// Presentation-neutral description of a MIDI endpoint.
struct MidiEndpointInfo
{
    // Backend-provided identifier, as stable as the platform allows. Used to
    // re-open the same endpoint; not guaranteed to survive replugging.
    std::string id;
    // What the user sees.
    std::string displayName;
    // Backend name ("ALSA (sequencer)", "CoreMIDI", "WinMM", "Loopback", ...).
    std::string backendName;
    EndpointDirection direction = EndpointDirection::Input;
    bool isVirtual = false;

    friend bool operator==(const MidiEndpointInfo&, const MidiEndpointInfo&) = default;
};

// A complete MIDI message as delivered by a transport.
struct MidiEvent
{
    MidiBytes bytes;
    // Backend timestamp in nanoseconds, or 0 when unavailable.
    std::int64_t timestampNanoseconds = 0;

    [[nodiscard]] bool isSysEx() const noexcept { return !bytes.empty() && bytes.front() == 0xF0; }
    [[nodiscard]] bool isCompleteSysEx() const noexcept
    {
        return bytes.size() >= 2 && bytes.front() == 0xF0 && bytes.back() == 0xF7;
    }
};

enum class TransportErrorCode {
    None,
    BackendUnavailable,
    EndpointNotFound,
    OpenFailed,
    NotOpen,
    SendFailed,
    InvalidMessage,
    Internal,
};

[[nodiscard]] std::string_view transportErrorCodeName(TransportErrorCode code) noexcept;

struct TransportError
{
    TransportErrorCode code = TransportErrorCode::None;
    std::string message;

    [[nodiscard]] static TransportError none() noexcept { return {}; }
    [[nodiscard]] static TransportError make(TransportErrorCode code, std::string message)
    {
        return TransportError{code, std::move(message)};
    }
    [[nodiscard]] bool failed() const noexcept { return code != TransportErrorCode::None; }
    [[nodiscard]] explicit operator bool() const noexcept { return failed(); }
};

// Structural helpers shared by transports.
[[nodiscard]] bool isCompleteSysEx(MidiByteSpan bytes) noexcept;
[[nodiscard]] bool isStatusByte(Byte value) noexcept;
[[nodiscard]] bool isRealtimeStatus(Byte value) noexcept;
// Expected total length of a non-SysEx message given its status byte, or 0
// when the status is SysEx start / undefined.
[[nodiscard]] std::size_t expectedMessageLength(Byte status) noexcept;

} // namespace xp60studio::midi
