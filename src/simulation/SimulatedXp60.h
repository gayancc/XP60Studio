#pragma once

// In-process XP-60 stand-in for Demo Mode and deterministic tests.
// Holds a memory image, answers RQ1 from it, and applies DT1 to it.

#include "midi/LoopbackMidiTransport.h"
#include "roland/RolandAddress.h"
#include "roland/RolandSysExMessage.h"
#include "roland/RolandTypes.h"
#include "xpmodel/MemoryImage.h"

#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace xp60studio::simulation {

class SimulatedXp60
{
public:
    explicit SimulatedXp60(xpmodel::MemoryImage memory);

    void corruptByte(const roland::RolandAddress& address, roland::Byte value);
    void corruptOnWrite(const roland::RolandAddress& address, roland::Byte value);
    void setAcceptWrites(bool accept);
    void setAnswerRequests(bool answer);
    void answerOnlyNextRequests(std::size_t n);

    [[nodiscard]] const xpmodel::MemoryImage& memory() const noexcept;
    [[nodiscard]] std::size_t dataSetsReceived() const noexcept;

    // Consumes everything the application sent and produces the replies.
    [[nodiscard]] std::vector<roland::RolandSysExMessage> exchange(midi::LoopbackMidiTransport& transport);

private:
    xpmodel::MemoryImage m_memory;
    bool m_acceptWrites = true;
    bool m_answerRequests = true;
    std::size_t m_dataSetsReceived = 0;
    std::size_t m_maxAnswers = std::numeric_limits<std::size_t>::max();
    std::size_t m_answered = 0;
    roland::RolandAddress m_corruptAddress;
    roland::Byte m_corruptValue = 0;
    bool m_corruptPending = false;
};

} // namespace xp60studio::simulation
