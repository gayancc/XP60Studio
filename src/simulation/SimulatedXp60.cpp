#include "simulation/SimulatedXp60.h"

#include "protocol/TransferPacing.h"
#include "roland/RolandCodec.h"
#include "xp60/Xp60Device.h"

namespace xp60studio::simulation {

SimulatedXp60::SimulatedXp60(xpmodel::MemoryImage memory)
    : m_memory(std::move(memory))
{
}

void SimulatedXp60::corruptByte(const roland::RolandAddress& address, roland::Byte value)
{
    m_memory.write(address, roland::ByteVector{value});
}

void SimulatedXp60::corruptOnWrite(const roland::RolandAddress& address, roland::Byte value)
{
    m_corruptAddress = address;
    m_corruptValue = value;
    m_corruptPending = true;
}

void SimulatedXp60::setAcceptWrites(bool accept)
{
    m_acceptWrites = accept;
}

void SimulatedXp60::setAnswerRequests(bool answer)
{
    m_answerRequests = answer;
}

void SimulatedXp60::answerOnlyNextRequests(std::size_t n)
{
    m_maxAnswers = m_answered + n;
}

const xpmodel::MemoryImage& SimulatedXp60::memory() const noexcept
{
    return m_memory;
}

std::size_t SimulatedXp60::dataSetsReceived() const noexcept
{
    return m_dataSetsReceived;
}

std::vector<roland::RolandSysExMessage> SimulatedXp60::exchange(midi::LoopbackMidiTransport& transport)
{
    std::vector<roland::RolandSysExMessage> replies;
    const std::vector<roland::RolandModelId> models{xp60::modelId()};
    for (const auto& raw : transport.sentMessages()) {
        const auto decoded = roland::decodeRolandSysEx(roland::ByteSpan(raw.data(), raw.size()), models);
        if (!decoded.ok()) {
            continue;
        }
        const auto& message = *decoded.message;
        if (message.isDataSet()) {
            ++m_dataSetsReceived;
            if (m_acceptWrites) {
                m_memory.addDataSet(message);
                if (m_corruptPending && m_memory.contains(m_corruptAddress)) {
                    m_memory.write(m_corruptAddress, roland::ByteVector{m_corruptValue});
                }
            }
            continue;
        }
        if (!m_answerRequests || m_answered >= m_maxAnswers) {
            continue;
        }
        ++m_answered;
        const auto bytes = m_memory.read(message.address(), message.size().value());
        if (!bytes) {
            continue;
        }
        const auto whole = roland::RolandSysExMessage::dataSet(message.deviceId(), message.modelId(),
                                                               message.address(), *bytes);
        if (!whole) {
            continue;
        }
        for (auto& chunk : protocol::chunkDataSet(*whole, 128)) {
            replies.push_back(std::move(chunk));
        }
    }
    transport.clearSentMessages();
    return replies;
}

} // namespace xp60studio::simulation
