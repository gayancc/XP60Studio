#include "midi/IMidiTransport.h"

namespace xp60studio::midi {

void IMidiTransport::closeAll()
{
    closeInput();
    closeOutput();
}

TransportError IMidiTransport::sendSysEx(MidiByteSpan message)
{
    if (!isCompleteSysEx(message)) {
        return TransportError::make(TransportErrorCode::InvalidMessage,
                                    "SysEx message must start with F0 and end with F7");
    }
    for (std::size_t i = 1; i + 1 < message.size(); ++i) {
        if (message[i] >= 0x80) {
            return TransportError::make(TransportErrorCode::InvalidMessage,
                                        "SysEx body contains a status byte at offset " + std::to_string(i));
        }
    }
    return send(message);
}

} // namespace xp60studio::midi
