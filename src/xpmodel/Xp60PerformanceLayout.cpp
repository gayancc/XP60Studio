#include "xpmodel/Xp60PerformanceLayout.h"

#include <string>

namespace xp60studio::xpmodel {
namespace {

// "Part 1" .. "Part 16", kept alive for the string_view in Block.
const std::string& partBlockName(int number)
{
    static const auto names = [] {
        std::array<std::string, PartIndex::kCount> made{};
        for (int n = 1; n <= PartIndex::kCount; ++n) {
            made[static_cast<std::size_t>(n - 1)] = "Part " + std::to_string(n);
        }
        return made;
    }();
    return names[static_cast<std::size_t>(number - 1)];
}

} // namespace

std::array<PartIndex, PartIndex::kCount> PartIndex::all() noexcept
{
    std::array<PartIndex, kCount> parts{
        PartIndex(1),  PartIndex(2),  PartIndex(3),  PartIndex(4),
        PartIndex(5),  PartIndex(6),  PartIndex(7),  PartIndex(8),
        PartIndex(9),  PartIndex(10), PartIndex(11), PartIndex(12),
        PartIndex(13), PartIndex(14), PartIndex(15), PartIndex(16),
    };
    return parts;
}

std::array<Xp60PerformanceLayout::Block, 1 + Xp60PerformanceLayout::kPartCount>
Xp60PerformanceLayout::blocks() noexcept
{
    std::array<Block, 1 + kPartCount> made{};
    made[0] = Block{"Performance Common", std::nullopt, commonOffset(), commonSize(), &commonTable()};
    for (const auto part : PartIndex::all()) {
        made[static_cast<std::size_t>(part.number())] =
            Block{partBlockName(part.number()), part, partOffset(part), partSize(), &partTable()};
    }
    return made;
}

roland::RolandAddress Xp60PerformanceLayout::temporaryPerformanceAddress() noexcept
{
    return roland::RolandAddress{0x01, 0x00, 0x00, 0x00};
}

std::optional<roland::RolandAddress> Xp60PerformanceLayout::userPerformanceAddress(int userNumber) noexcept
{
    if (userNumber < 1 || userNumber > kUserPerformanceCount) {
        return std::nullopt;
    }
    return roland::RolandAddress{0x10, 0x00, 0x00, 0x00}.plus(
        static_cast<std::uint64_t>(userNumber - 1) * kUserPerformanceStride);
}

std::vector<Xp60PerformanceLayout::ReadRequest>
Xp60PerformanceLayout::fetchPlan(const roland::RolandAddress& performanceBase)
{
    std::vector<ReadRequest> plan;
    plan.reserve(1 + kPartCount);
    for (const auto& block : blocks()) {
        const auto address = performanceBase.plus(block.offset);
        const auto size = roland::RolandSize::fromValue(block.size);
        if (!address || !size) {
            return {};
        }
        plan.push_back(ReadRequest{block, *address, *size});
    }
    return plan;
}

std::optional<PatchName> Xp60PerformanceLayout::readPerformanceName(const MemoryImage& image,
                                                                    const roland::RolandAddress& base)
{
    const auto bytes = image.read(base, PatchName::kLength);
    if (!bytes) {
        return std::nullopt;
    }
    return PatchName::fromBytes(*bytes);
}

std::optional<PatchName> Xp60PerformanceLayout::readTemporaryPerformanceName(const MemoryImage& image)
{
    return readPerformanceName(image, temporaryPerformanceAddress());
}

std::vector<Xp60PerformanceLayout::UserPerformanceNameEntry>
Xp60PerformanceLayout::readUserPerformanceNames(const MemoryImage& image)
{
    std::vector<UserPerformanceNameEntry> entries;
    entries.reserve(kUserPerformanceCount);
    for (int n = 1; n <= kUserPerformanceCount; ++n) {
        UserPerformanceNameEntry entry;
        entry.userNumber = n;
        entry.address = *userPerformanceAddress(n);
        entry.name = readPerformanceName(image, entry.address);
        entries.push_back(entry);
    }
    return entries;
}

} // namespace xp60studio::xpmodel
