#pragma once

#include "roland/RolandAddress.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/MemoryImage.h"
#include "xpmodel/ParameterTable.h"
#include "xpmodel/PatchName.h"

#include <array>
#include <optional>
#include <string_view>
#include <vector>

namespace xp60studio::xpmodel {

// XP-60 Patch layout facts.
//
// Only entries confirmed from the Roland Parameter Address Map are present.
// Everything else is exposed as an explicit "not yet transcribed" optional so
// callers cannot mistake a placeholder for a fact. The Patch model proper
// (Common / Tone 1-4 as strong types) is added once the map is transcribed;
// see docs/protocol/XP60_PATCH_PARAMETER_MAP.md for the required inputs.
struct Xp60PatchLayout
{
    static constexpr int kToneCount = 4;
    static constexpr int kUserPatchCount = 128;

    // Patch Common, confirmed prefix: Patch Name 1..12 at offset 00 00.
    [[nodiscard]] static const ParameterTable& patchCommonKnownPrefix() noexcept;

    // Sizes and offsets still to be taken from the Parameter Address Map.
    [[nodiscard]] static std::optional<std::uint32_t> patchCommonSize() noexcept { return std::nullopt; }
    [[nodiscard]] static std::optional<std::uint32_t> toneSize() noexcept { return std::nullopt; }
    [[nodiscard]] static std::optional<std::uint32_t> toneOffset(int toneIndex) noexcept; // 1..4
    [[nodiscard]] static std::optional<std::uint32_t> patchSize() noexcept { return std::nullopt; }
    [[nodiscard]] static bool isComplete() noexcept { return false; }
    [[nodiscard]] static std::string_view missingInputs() noexcept;

    // Documented bases (see docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md).
    [[nodiscard]] static roland::RolandAddress temporaryPatchAddress() noexcept;
    [[nodiscard]] static std::optional<roland::RolandAddress> userPatchAddress(int userNumber) noexcept; // 1..128
    static constexpr std::uint32_t kUserPatchStride = 0x01u << 14;                                       // 00 01 00 00

    // Name helpers working on an assembled memory image.
    [[nodiscard]] static std::optional<PatchName> readPatchName(const MemoryImage& image, const roland::RolandAddress& patchBase);
    [[nodiscard]] static std::optional<PatchName> readTemporaryPatchName(const MemoryImage& image);

    struct UserPatchNameEntry
    {
        int userNumber = 0;               // 1..128
        roland::RolandAddress address;    // patch base
        std::optional<PatchName> name;    // nullopt when the image lacks the 12 bytes
    };
    // One entry per User Patch; names present where the image covers them.
    [[nodiscard]] static std::vector<UserPatchNameEntry> readUserPatchNames(const MemoryImage& image);
};

} // namespace xp60studio::xpmodel
