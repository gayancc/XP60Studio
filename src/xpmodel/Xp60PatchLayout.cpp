#include "xpmodel/Xp60PatchLayout.h"

namespace xp60studio::xpmodel {

std::array<Xp60PatchLayout::Block, 5> Xp60PatchLayout::blocks() noexcept
{
    return {{
        Block{"Patch Common", std::nullopt, commonOffset(), patchCommonSize(), &patchCommonTable()},
        Block{"Tone 1", ToneIndex::tone1(), toneOffset(ToneIndex::tone1()), toneSize(), &patchToneTable()},
        Block{"Tone 2", ToneIndex::tone2(), toneOffset(ToneIndex::tone2()), toneSize(), &patchToneTable()},
        Block{"Tone 3", ToneIndex::tone3(), toneOffset(ToneIndex::tone3()), toneSize(), &patchToneTable()},
        Block{"Tone 4", ToneIndex::tone4(), toneOffset(ToneIndex::tone4()), toneSize(), &patchToneTable()},
    }};
}

roland::RolandAddress Xp60PatchLayout::temporaryPatchAddress() noexcept
{
    return roland::RolandAddress{0x03, 0x00, 0x00, 0x00};
}

std::optional<roland::RolandAddress> Xp60PatchLayout::userPatchAddress(int userNumber) noexcept
{
    if (userNumber < 1 || userNumber > kUserPatchCount) {
        return std::nullopt;
    }
    return roland::RolandAddress{0x11, 0x00, 0x00, 0x00}.plus(static_cast<std::uint64_t>(userNumber - 1) * kUserPatchStride);
}

std::vector<Xp60PatchLayout::ReadRequest> Xp60PatchLayout::fetchPlan(const roland::RolandAddress& patchBase)
{
    std::vector<ReadRequest> plan;
    for (const auto& block : blocks()) {
        const auto address = patchBase.plus(block.offset);
        const auto size = roland::RolandSize::fromValue(block.size);
        if (!address || !size) {
            return {};
        }
        plan.push_back(ReadRequest{block, *address, *size});
    }
    return plan;
}

std::optional<PatchName> Xp60PatchLayout::readPatchName(const MemoryImage& image, const roland::RolandAddress& patchBase)
{
    const auto bytes = image.read(patchBase, PatchName::kLength);
    if (!bytes) {
        return std::nullopt;
    }
    return PatchName::fromBytes(*bytes);
}

std::optional<PatchName> Xp60PatchLayout::readTemporaryPatchName(const MemoryImage& image)
{
    return readPatchName(image, temporaryPatchAddress());
}

std::vector<Xp60PatchLayout::UserPatchNameEntry> Xp60PatchLayout::readUserPatchNames(const MemoryImage& image)
{
    std::vector<UserPatchNameEntry> entries;
    entries.reserve(kUserPatchCount);
    for (int n = 1; n <= kUserPatchCount; ++n) {
        UserPatchNameEntry entry;
        entry.userNumber = n;
        entry.address = *userPatchAddress(n);
        entry.name = readPatchName(image, entry.address);
        entries.push_back(entry);
    }
    return entries;
}

} // namespace xp60studio::xpmodel
