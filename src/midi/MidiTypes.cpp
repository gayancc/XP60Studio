#include "midi/MidiTypes.h"

namespace xp60studio::midi {

std::string_view endpointDirectionName(EndpointDirection direction) noexcept
{
    return direction == EndpointDirection::Input ? "Input" : "Output";
}

std::string_view transportErrorCodeName(TransportErrorCode code) noexcept
{
    switch (code) {
    case TransportErrorCode::None:
        return "None";
    case TransportErrorCode::BackendUnavailable:
        return "BackendUnavailable";
    case TransportErrorCode::EndpointNotFound:
        return "EndpointNotFound";
    case TransportErrorCode::OpenFailed:
        return "OpenFailed";
    case TransportErrorCode::NotOpen:
        return "NotOpen";
    case TransportErrorCode::SendFailed:
        return "SendFailed";
    case TransportErrorCode::InvalidMessage:
        return "InvalidMessage";
    case TransportErrorCode::Internal:
        return "Internal";
    }
    return "Unknown";
}

bool isCompleteSysEx(MidiByteSpan bytes) noexcept
{
    return bytes.size() >= 2 && bytes.front() == 0xF0 && bytes.back() == 0xF7;
}

bool isStatusByte(Byte value) noexcept
{
    return value >= 0x80;
}

bool isRealtimeStatus(Byte value) noexcept
{
    return value >= 0xF8;
}

std::size_t expectedMessageLength(Byte status) noexcept
{
    if (status < 0x80) {
        return 0;
    }
    if (status < 0xF0) {
        switch (status & 0xF0) {
        case 0xC0: // program change
        case 0xD0: // channel pressure
            return 2;
        default: // note off/on, poly pressure, control change, pitch bend
            return 3;
        }
    }
    switch (status) {
    case 0xF1: // MTC quarter frame
    case 0xF3: // song select
        return 2;
    case 0xF2: // song position
        return 3;
    case 0xF6: // tune request
        return 1;
    case 0xF0: // SysEx start: variable
    case 0xF7: // stray EOX
    case 0xF4:
    case 0xF5:
        return 0;
    default: // realtime
        return 1;
    }
}

} // namespace xp60studio::midi
