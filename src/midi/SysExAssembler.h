#pragma once

#include "midi/MidiTypes.h"

#include <cstddef>
#include <functional>

namespace xp60studio::midi {

// Reassembles a raw MIDI byte stream into complete messages.
//
// Backends normally deliver whole messages, but some platforms split long
// SysEx transfers across several callbacks. The assembler accepts arbitrary
// chunks and emits:
//   * complete SysEx messages (F0 .. F7), however they were fragmented;
//   * complete channel / system-common messages;
//   * single realtime bytes, even when they arrive inside a SysEx.
//
// Malformed input is counted rather than silently dropped: a status byte that
// interrupts an unfinished SysEx aborts it, data bytes without a status are
// stray bytes, and a SysEx exceeding the byte limit is discarded.
class SysExAssembler
{
public:
    using Emit = std::function<void(MidiBytes)>;

    static constexpr std::size_t kDefaultMaxSysExBytes = 256 * 1024;

    explicit SysExAssembler(std::size_t maxSysExBytes = kDefaultMaxSysExBytes);

    void feed(MidiByteSpan chunk, const Emit& emit);
    void reset();

    [[nodiscard]] bool sysExInProgress() const noexcept { return m_inSysEx; }
    [[nodiscard]] std::size_t pendingBytes() const noexcept { return m_buffer.size(); }

    [[nodiscard]] std::size_t abortedSysExCount() const noexcept { return m_abortedSysEx; }
    [[nodiscard]] std::size_t overflowSysExCount() const noexcept { return m_overflowSysEx; }
    [[nodiscard]] std::size_t strayByteCount() const noexcept { return m_strayBytes; }

private:
    void beginSysEx();
    void abortSysEx();

    std::size_t m_maxSysExBytes;
    MidiBytes m_buffer;
    bool m_inSysEx = false;
    bool m_overflowing = false;
    std::size_t m_expectedLength = 0; // for non-SysEx messages in progress
    std::size_t m_abortedSysEx = 0;
    std::size_t m_overflowSysEx = 0;
    std::size_t m_strayBytes = 0;
};

} // namespace xp60studio::midi
