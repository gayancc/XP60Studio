#include "simulation/DemoFixture.h"

#include "xp60/Xp60Device.h"
#include "xpmodel/SysExStream.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QFile>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace xp60studio::simulation {

namespace {

roland::ByteVector readEmbeddedSyx()
{
    QFile file(QStringLiteral(":/demo/user-bank-amal.syx"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.readAll();
    return roland::ByteVector(bytes.begin(), bytes.end());
}

} // namespace

xpmodel::MemoryImage loadEmbeddedFixtureBank()
{
    const auto data = readEmbeddedSyx();
    if (data.empty()) {
        return {};
    }
    const std::vector<roland::RolandModelId> models{xp60::modelId()};
    return xpmodel::imageFromStream(xpmodel::parseSysExStream(data, models));
}

roland::RolandAddress temporaryPatchAddress()
{
    return xpmodel::Xp60PatchLayout::temporaryPatchAddress();
}

xpmodel::MemoryImage temporaryAreaFromFixture(int userPatchNumber)
{
    const auto bank = loadEmbeddedFixtureBank();
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

xpmodel::Xp60Patch patchFrom(const xpmodel::MemoryImage& image, const roland::RolandAddress& base)
{
    auto decoded = xpmodel::Xp60PatchCodec::decode(image, base);
    if (!decoded.patch.has_value()) {
        throw std::runtime_error(std::string("simulation::patchFrom: decode failed"));
    }
    return std::move(*decoded.patch);
}

} // namespace xp60studio::simulation
