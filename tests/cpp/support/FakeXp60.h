#pragma once

// Shared test doubles for the XP-60 conversation.
//
// Thin wrapper around simulation::SimulatedXp60. Fixture helpers still prefer
// XP60STUDIO_FIXTURE_DIR so unit tests do not depend on Qt resources.

#include "midi/LoopbackMidiTransport.h"
#include "simulation/SimulatedXp60.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/MemoryImage.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60Patch.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace xp60studio::testsupport {

class FakeXp60
{
public:
    explicit FakeXp60(xpmodel::MemoryImage memory)
        : m_device(std::move(memory))
    {
    }

    void corruptByte(const roland::RolandAddress& address, roland::Byte value)
    {
        m_device.corruptByte(address, value);
    }
    void corruptOnWrite(const roland::RolandAddress& address, roland::Byte value)
    {
        m_device.corruptOnWrite(address, value);
    }
    void setAcceptWrites(bool accept) { m_device.setAcceptWrites(accept); }
    void setAnswerRequests(bool answer) { m_device.setAnswerRequests(answer); }
    void answerOnlyNextRequests(std::size_t n) { m_device.answerOnlyNextRequests(n); }

    [[nodiscard]] const xpmodel::MemoryImage& memory() const noexcept { return m_device.memory(); }
    [[nodiscard]] std::size_t dataSetsReceived() const noexcept { return m_device.dataSetsReceived(); }

    [[nodiscard]] std::vector<roland::RolandSysExMessage> exchange(midi::LoopbackMidiTransport& transport)
    {
        return m_device.exchange(transport);
    }

private:
    simulation::SimulatedXp60 m_device;
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

inline xpmodel::MemoryImage temporaryAreaWith(int userPatchNumber)
{
    const auto bank = fixtureImage();
    const auto sourceOpt = xpmodel::Xp60PatchLayout::userPatchAddress(userPatchNumber);
    if (!sourceOpt || bank.isEmpty()) {
        return {};
    }
    const auto source = *sourceOpt;
    const auto temp = temporaryPatchAddress();
    xpmodel::MemoryImage image;
    for (const auto& block : xpmodel::Xp60PatchLayout::blocks()) {
        const auto dest = temp.plus(block.offset);
        const auto srcAddr = source.plus(block.offset);
        if (!dest || !srcAddr) {
            return {};
        }
        const auto bytes = bank.read(*srcAddr, block.size);
        if (!bytes) {
            return {};
        }
        image.write(*dest, *bytes);
    }
    return image;
}

inline xpmodel::Xp60Patch patchFrom(const xpmodel::MemoryImage& image, const roland::RolandAddress& base)
{
    auto decoded = xpmodel::Xp60PatchCodec::decode(image, base);
    if (!decoded.patch) {
        throw std::runtime_error("patchFrom: decode failed");
    }
    return *decoded.patch;
}

} // namespace xp60studio::testsupport
