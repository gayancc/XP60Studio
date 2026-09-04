#pragma once

#include "roland/RolandAddress.h"
#include "roland/RolandSize.h"
#include "xpmodel/MemoryImage.h"
#include "xpmodel/ParameterTable.h"
#include "xpmodel/PatchName.h"
#include "xpmodel/generated/Xp60PatchTables.h"

#include <array>
#include <optional>
#include <string_view>
#include <vector>

namespace xp60studio::xpmodel {

// One-based Tone number 1..4 with compile-time bounds checking helpers.
class ToneIndex
{
public:
    static constexpr int kCount = 4;

    [[nodiscard]] static std::optional<ToneIndex> fromNumber(int number) noexcept
    {
        return number >= 1 && number <= kCount ? std::optional<ToneIndex>(ToneIndex(number)) : std::nullopt;
    }
    [[nodiscard]] static constexpr ToneIndex tone1() noexcept { return ToneIndex(1); }
    [[nodiscard]] static constexpr ToneIndex tone2() noexcept { return ToneIndex(2); }
    [[nodiscard]] static constexpr ToneIndex tone3() noexcept { return ToneIndex(3); }
    [[nodiscard]] static constexpr ToneIndex tone4() noexcept { return ToneIndex(4); }
    [[nodiscard]] static constexpr std::array<ToneIndex, kCount> all() noexcept
    {
        return {tone1(), tone2(), tone3(), tone4()};
    }

    [[nodiscard]] constexpr int number() const noexcept { return m_number; }        // 1..4
    [[nodiscard]] constexpr std::size_t index() const noexcept { return static_cast<std::size_t>(m_number - 1); }

    friend constexpr auto operator<=>(const ToneIndex&, const ToneIndex&) noexcept = default;

private:
    explicit constexpr ToneIndex(int number) noexcept : m_number(number) {}
    int m_number;
};

// XP-60 Patch layout: block tables, sizes, offsets and documented addresses.
//
// All tables come from docs/protocol/XP60_PATCH_PARAMETER_MAP.md via
// tools/generate_patch_tables.py and are Complete. Every fact is still
// DocumentationDerived until a hardware capture promotes it.
struct Xp60PatchLayout
{
    static constexpr int kToneCount = ToneIndex::kCount;
    static constexpr int kUserPatchCount = 128;

    [[nodiscard]] static const ParameterTable& patchCommonTable() noexcept { return xp60tables::patchCommonTable(); }
    [[nodiscard]] static const ParameterTable& patchToneTable() noexcept { return xp60tables::patchToneTable(); }

    [[nodiscard]] static constexpr std::uint32_t patchCommonSize() noexcept { return xp60tables::kPatchCommonSize; }
    [[nodiscard]] static constexpr std::uint32_t toneSize() noexcept { return xp60tables::kPatchToneSize; }
    [[nodiscard]] static constexpr std::uint32_t commonOffset() noexcept { return 0; }
    [[nodiscard]] static constexpr std::uint32_t toneOffset(ToneIndex tone) noexcept
    {
        return xp60tables::kToneOffsets[tone.index()];
    }
    // Distance from the Patch base to the byte after Tone 4 (the blocks are
    // not contiguous: Roland leaves address space between them).
    [[nodiscard]] static constexpr std::uint32_t patchSpan() noexcept { return xp60tables::kPatchSpan; }
    [[nodiscard]] static constexpr bool isComplete() noexcept { return true; }

    // The five data blocks of a Patch with their offsets from the Patch base.
    struct Block
    {
        std::string_view name;             // "Patch Common", "Tone 1", ...
        std::optional<ToneIndex> tone;     // nullopt for Common
        std::uint32_t offset = 0;
        std::uint32_t size = 0;
        const ParameterTable* table = nullptr;
    };
    [[nodiscard]] static std::array<Block, 5> blocks() noexcept;

    // Documented bases (see docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md).
    [[nodiscard]] static roland::RolandAddress temporaryPatchAddress() noexcept;
    [[nodiscard]] static std::optional<roland::RolandAddress> userPatchAddress(int userNumber) noexcept; // 1..128
    static constexpr std::uint32_t kUserPatchStride = 0x01u << 14;                                       // 00 01 00 00

    // RQ1 requests (address, size) that read one whole Patch block by block,
    // exactly as the Parameter Address Map defines the blocks.
    struct ReadRequest
    {
        Block block;
        roland::RolandAddress address;
        roland::RolandSize size;
    };
    [[nodiscard]] static std::vector<ReadRequest> fetchPlan(const roland::RolandAddress& patchBase);

    // Name helpers working on an assembled memory image.
    [[nodiscard]] static std::optional<PatchName> readPatchName(const MemoryImage& image, const roland::RolandAddress& patchBase);
    [[nodiscard]] static std::optional<PatchName> readTemporaryPatchName(const MemoryImage& image);

    struct UserPatchNameEntry
    {
        int userNumber = 0;               // 1..128
        roland::RolandAddress address;    // patch base
        std::optional<PatchName> name;    // nullopt when the image lacks the 12 bytes
    };
    [[nodiscard]] static std::vector<UserPatchNameEntry> readUserPatchNames(const MemoryImage& image);
};

} // namespace xp60studio::xpmodel
