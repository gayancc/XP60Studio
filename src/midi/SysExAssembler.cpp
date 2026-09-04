#include "midi/SysExAssembler.h"

namespace xp60studio::midi {

SysExAssembler::SysExAssembler(std::size_t maxSysExBytes)
    : m_maxSysExBytes(maxSysExBytes)
{
}

void SysExAssembler::reset()
{
    m_buffer.clear();
    m_inSysEx = false;
    m_overflowing = false;
    m_expectedLength = 0;
}

void SysExAssembler::beginSysEx()
{
    m_buffer.clear();
    m_buffer.push_back(0xF0);
    m_inSysEx = true;
    m_overflowing = false;
    m_expectedLength = 0;
}

void SysExAssembler::abortSysEx()
{
    if (m_overflowing) {
        ++m_overflowSysEx;
    } else {
        ++m_abortedSysEx;
    }
    m_buffer.clear();
    m_inSysEx = false;
    m_overflowing = false;
}

void SysExAssembler::feed(MidiByteSpan chunk, const Emit& emit)
{
    for (const Byte b : chunk) {
        if (isRealtimeStatus(b)) {
            // Realtime bytes may legally interleave with anything.
            emit(MidiBytes{b});
            continue;
        }

        if (m_inSysEx) {
            if (b == 0xF7) {
                if (m_overflowing) {
                    ++m_overflowSysEx;
                    m_buffer.clear();
                } else {
                    m_buffer.push_back(b);
                    emit(std::move(m_buffer));
                    m_buffer = MidiBytes{};
                }
                m_inSysEx = false;
                m_overflowing = false;
                continue;
            }
            if (isStatusByte(b)) {
                // A new status interrupts the unfinished SysEx.
                abortSysEx();
                // fall through to normal handling of `b`
            } else {
                if (!m_overflowing) {
                    if (m_buffer.size() + 1 > m_maxSysExBytes) {
                        m_overflowing = true;
                        m_buffer.clear();
                    } else {
                        m_buffer.push_back(b);
                    }
                }
                continue;
            }
        }

        if (b == 0xF0) {
            if (!m_buffer.empty()) {
                // Unfinished channel message is discarded as stray bytes.
                m_strayBytes += m_buffer.size();
                m_buffer.clear();
            }
            beginSysEx();
            continue;
        }

        if (isStatusByte(b)) {
            if (!m_buffer.empty()) {
                m_strayBytes += m_buffer.size();
                m_buffer.clear();
            }
            m_expectedLength = expectedMessageLength(b);
            if (m_expectedLength == 0) {
                // F7 without F0, or undefined F4/F5: stray.
                ++m_strayBytes;
                continue;
            }
            m_buffer.push_back(b);
            if (m_expectedLength == 1) {
                emit(std::move(m_buffer));
                m_buffer = MidiBytes{};
                m_expectedLength = 0;
            }
            continue;
        }

        // Data byte outside SysEx.
        if (m_buffer.empty()) {
            ++m_strayBytes;
            continue;
        }
        m_buffer.push_back(b);
        if (m_buffer.size() == m_expectedLength) {
            emit(std::move(m_buffer));
            m_buffer = MidiBytes{};
            m_expectedLength = 0;
        }
    }
}

} // namespace xp60studio::midi
