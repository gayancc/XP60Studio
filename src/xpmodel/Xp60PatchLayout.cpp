#include "xpmodel/Xp60PatchLayout.h"

namespace xp60studio::xpmodel {

namespace {

constexpr std::string_view kNameSource =
    "XP-60/XP-80 MIDI Implementation, Parameter Address Map: Patch Name 1..12 at Patch Common offset 00 00 "
    "(confirmed during PR #5 review).";

#define XP60_NAME_CHAR(n, off)                                                                                       \
    ParameterDescriptor{"common.name." #n, "Patch Name " #n, off, ParameterEncoding::Ascii, 1, PatchName::kMinChar, \
                        PatchName::kMaxChar, 0, {}, {}, "Name", xp60::VerificationStatus::DocumentationDerived,      \
                        kNameSource}

const std::array<ParameterDescriptor, PatchName::kLength> kPatchCommonKnown{{
    XP60_NAME_CHAR(1, 0), XP60_NAME_CHAR(2, 1), XP60_NAME_CHAR(3, 2), XP60_NAME_CHAR(4, 3),
    XP60_NAME_CHAR(5, 4), XP60_NAME_CHAR(6, 5), XP60_NAME_CHAR(7, 6), XP60_NAME_CHAR(8, 7),
    XP60_NAME_CHAR(9, 8), XP60_NAME_CHAR(10, 9), XP60_NAME_CHAR(11, 10), XP60_NAME_CHAR(12, 11),
}};

#undef XP60_NAME_CHAR

} // namespace

const ParameterTable& Xp60PatchLayout::patchCommonKnownPrefix() noexcept
{
    // The block size here is the size of the *known prefix*, not of Patch
    // Common; Partial completeness makes that explicit to every caller.
    // Function-local static: safe to use during other TUs' static init.
    static const ParameterTable kPatchCommonPrefixTable{
        "XP-60 Patch Common (known prefix)",
        PatchName::kLength,
        std::span<const ParameterDescriptor>(kPatchCommonKnown.data(), kPatchCommonKnown.size()),
        TableCompleteness::Partial,
        "Only Patch Name 1..12 transcribed; the remaining Patch Common parameters, the Patch Common size and the "
        "Tone offsets/sizes await the Parameter Address Map (docs/protocol/XP60_PATCH_PARAMETER_MAP.md).",
    };
    return kPatchCommonPrefixTable;
}

std::optional<std::uint32_t> Xp60PatchLayout::toneOffset(int toneIndex) noexcept
{
    (void)toneIndex;
    return std::nullopt;
}

std::string_view Xp60PatchLayout::missingInputs() noexcept
{
    return "Parameter Address Map pages for Patch Common (all parameters, offsets, ranges, total size) and Patch "
           "Tone (all parameters, offsets, ranges, total size, and the Tone 1-4 offsets within a Patch).";
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
