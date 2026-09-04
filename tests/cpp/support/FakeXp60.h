#pragma once

// Shared test doubles for the XP-60 conversation.
//
// Requires XP60STUDIO_FIXTURE_DIR to point at tests/fixtures/xp60, because the
// fake device is loaded with real Patch data rather than invented bytes.

#include "midi/LoopbackMidiTransport.h"
#include "protocol/TransferPacing.h"
#include "roland/RolandCodec.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/MemoryImage.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60Patch.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <cstddef>
#include <fstream>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

namespace xp60studio::testsupport {

// A stand-in for the instrument: holds a memory image, answers RQ1 from it and
// applies DT1 to it. Deliberately simple — its job is to let the service layer
// run end to end, not to emulate an XP-60.
class FakeXp60
{
public:
    explicit FakeXp60(xpmodel::MemoryImage memory)
        : m_memory(std::move(memory))
    {
    }

    // Corrupts one byte of what the device will report, to simulate a write
    // that did not fully take.
    void corruptByte(const roland::RolandAddress& address, roland::Byte value)
    {
        m_memory.write(address, roland::ByteVector{value});
    }
    // Applies that corruption automatically once the write has been taken.
    void corruptOnWrite(const roland::RolandAddress& address, roland::Byte value)
    {
        m_corruptAddress = address;
        m_corruptValue = value;
        m_corruptPending = true;
    }
    // Silently ignores writes, as a device with Rx Exclusive off would.
    void setAcceptWrites(bool accept) { m_acceptWrites = accept; }
    void setAnswerRequests(bool answer) { m_answerRequests = answer; }
    // Answers `n` more requests, then goes quiet — the way a device that
    // stops responding part way through a transfer would.
    void answerOnlyNextRequests(std::size_t n) { m_maxAnswers = m_answered + n; }

    [[nodiscard]] const xpmodel::MemoryImage& memory() const noexcept { return m_memory; }
    [[nodiscard]] std::size_t dataSetsReceived() const noexcept { return m_dataSetsReceived; }

    // Consumes everything the application sent and produces the replies.
    [[nodiscard]] std::vector<roland::RolandSysExMessage> exchange(midi::LoopbackMidiTransport& transport)
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
                continue; // an unreadable range simply gets no answer, like a silent device
            }
            const auto whole = roland::RolandSysExMessage::dataSet(message.deviceId(), message.modelId(),
                                                                   message.address(), *bytes);
            for (auto& chunk : protocol::chunkDataSet(*whole, 128)) {
                replies.push_back(std::move(chunk));
            }
        }
        transport.clearSentMessages();
        return replies;
    }

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

inline xpmodel::MemoryImage fixtureImage()
{
    std::ifstream in(XP60STUDIO_FIXTURE_DIR "/user-bank-amal.syx", std::ios::binary);
    const roland::ByteVector data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::vector<roland::RolandModelId> models{xp60::modelId()};
    return xpmodel::imageFromStream(xpmodel::parseSysExStream(data, models));
}

inline roland::RolandAddress temporaryPatchAddress()
{
    return xpmodel::Xp60PatchLayout::temporaryPatchAddress();
}

// Puts a real patch from the fixture into the temporary area.
inline xpmodel::MemoryImage temporaryAreaWith(int userPatchNumber)
{
    const auto bank = fixtureImage();
    const auto source = *xpmodel::Xp60PatchLayout::userPatchAddress(userPatchNumber);
    const auto temp = temporaryPatchAddress();
    xpmodel::MemoryImage image;
    for (const auto& block : xpmodel::Xp60PatchLayout::blocks()) {
        image.write(*temp.plus(block.offset), *bank.read(*source.plus(block.offset), block.size));
    }
    return image;
}

inline xpmodel::Xp60Patch patchFrom(const xpmodel::MemoryImage& image, const roland::RolandAddress& base)
{
    return *xpmodel::Xp60PatchCodec::decode(image, base).patch;
}

} // namespace xp60studio::testsupport
