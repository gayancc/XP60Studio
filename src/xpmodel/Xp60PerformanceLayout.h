#pragma once

#include "roland/RolandAddress.h"
#include "roland/RolandSize.h"
#include "xpmodel/MemoryImage.h"
#include "xpmodel/ParameterTable.h"
#include "xpmodel/PatchName.h"
#include "xpmodel/generated/Xp60PerformanceTables.h"

#include <array>
#include <optional>
#include <string_view>
#include <vector>

namespace xp60studio::xpmodel {

// One-based Part number 1..16 with compile-time bounds checking, the
// Performance counterpart of ToneIndex.
class PartIndex
{
public:
    static constexpr int kCount = xp60performance::kPartCount;

    [[nodiscard]] static std::optional<PartIndex> fromNumber(int number) noexcept
    {
        return number >= 1 && number <= kCount ? std::optional<PartIndex>(PartIndex(number)) : std::nullopt;
    }
    [[nodiscard]] static constexpr PartIndex part1() noexcept { return PartIndex(1); }
    // Part 10 is the Rhythm part on this instrument — the reason Performance
    // Common's EFX Source enumerates 1..9 and 11..16 but not 10.
    [[nodiscard]] static constexpr PartIndex rhythmPart() noexcept { return PartIndex(10); }
    [[nodiscard]] static std::array<PartIndex, kCount> all() noexcept;

    [[nodiscard]] constexpr int number() const noexcept { return m_number; }  // 1..16
    [[nodiscard]] constexpr std::size_t index() const noexcept { return static_cast<std::size_t>(m_number - 1); }
    [[nodiscard]] constexpr bool isRhythmPart() const noexcept { return m_number == 10; }

    friend constexpr auto operator<=>(const PartIndex&, const PartIndex&) noexcept = default;

private:
    explicit constexpr PartIndex(int number) noexcept : m_number(number) {}
    int m_number;
};

// XP-60 Performance layout: block tables, sizes, offsets and documented
// addresses.
//
// All tables come from docs/protocol/XP60_PERFORMANCE_PARAMETER_MAP.md via
// tools/generate_performance_tables.py and are Complete. Every fact is
// DocumentationDerived until a hardware capture promotes it.
struct Xp60PerformanceLayout
{
    static constexpr int kPartCount = PartIndex::kCount;
    static constexpr int kUserPerformanceCount = 32;

    [[nodiscard]] static const ParameterTable& commonTable() noexcept
    {
        return xp60performance::performanceCommonTable();
    }
    [[nodiscard]] static const ParameterTable& partTable() noexcept
    {
        return xp60performance::performancePartTable();
    }

    [[nodiscard]] static constexpr std::uint32_t commonSize() noexcept
    {
        return xp60performance::kPerformanceCommonSize;
    }
    [[nodiscard]] static constexpr std::uint32_t partSize() noexcept
    {
        return xp60performance::kPerformancePartSize;
    }
    [[nodiscard]] static constexpr std::uint32_t commonOffset() noexcept { return 0; }
    [[nodiscard]] static constexpr std::uint32_t partOffset(PartIndex part) noexcept
    {
        return xp60performance::kPartOffsets[part.index()];
    }
    // Distance from the Performance base to the byte after Part 16. The blocks
    // are not contiguous: Roland leaves address space between them, which is
    // why this is larger than 66 + 16 * 25.
    //
    // It comes to 3993, which is exactly the size of Roland's own published RQ1
    // example for the Temporary Performance (`00 00 1F 19`) — an independent
    // check that the offsets and sizes transcribed here are right.
    [[nodiscard]] static constexpr std::uint32_t performanceSpan() noexcept
    {
        return xp60performance::kPerformanceSpan;
    }
    [[nodiscard]] static constexpr bool isComplete() noexcept { return true; }

    // The seventeen data blocks of a Performance with their offsets from the
    // Performance base.
    struct Block
    {
        std::string_view name;             // "Performance Common", "Part 1", ...
        std::optional<PartIndex> part;     // nullopt for Common
        std::uint32_t offset = 0;
        std::uint32_t size = 0;
        const ParameterTable* table = nullptr;
    };
    [[nodiscard]] static std::array<Block, 1 + kPartCount> blocks() noexcept;

    // Documented bases (see docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md §3).
    [[nodiscard]] static roland::RolandAddress temporaryPerformanceAddress() noexcept;
    // 1..32.
    [[nodiscard]] static std::optional<roland::RolandAddress> userPerformanceAddress(int userNumber) noexcept;
    static constexpr std::uint32_t kUserPerformanceStride = 0x01u << 14;  // 00 01 00 00

    // RQ1 requests (address, size) that read one whole Performance block by
    // block, exactly as the Parameter Address Map defines the blocks.
    //
    // Block by block rather than one 3993-byte request: §2.2 established that
    // an RQ1 size is an address span, so a single request is answered with the
    // populated blocks anyway, and asking per block keeps each reply's identity
    // unambiguous.
    struct ReadRequest
    {
        Block block;
        roland::RolandAddress address;
        roland::RolandSize size;
    };
    [[nodiscard]] static std::vector<ReadRequest> fetchPlan(const roland::RolandAddress& performanceBase);

    // Name helpers working on an assembled memory image.
    [[nodiscard]] static std::optional<PatchName> readPerformanceName(const MemoryImage& image,
                                                                      const roland::RolandAddress& base);
    [[nodiscard]] static std::optional<PatchName> readTemporaryPerformanceName(const MemoryImage& image);

    struct UserPerformanceNameEntry
    {
        int userNumber = 0;               // 1..32
        roland::RolandAddress address;    // performance base
        std::optional<PatchName> name;    // nullopt when the image lacks the 12 bytes
    };
    [[nodiscard]] static std::vector<UserPerformanceNameEntry> readUserPerformanceNames(const MemoryImage& image);
};

} // namespace xp60studio::xpmodel
