#pragma once

#include "roland/RolandAddress.h"
#include "roland/RolandDeviceId.h"
#include "roland/RolandModelId.h"
#include "roland/RolandSysExMessage.h"
#include "xpmodel/BlockCodec.h"
#include "xpmodel/MemoryImage.h"
#include "xpmodel/Xp60Performance.h"
#include "xpmodel/Xp60PerformanceLayout.h"

#include <optional>
#include <string>
#include <vector>

namespace xp60studio::xpmodel {

// Result of decoding one Performance from a memory image.
struct Xp60PerformanceDecodeResult
{
    struct BlockIssueAt
    {
        std::string block; // "Performance Common", "Part 1", ...
        BlockIssue issue;
    };
    struct MissingBlock
    {
        std::string block;
        roland::RolandAddress address;
        MemoryImage::Coverage coverage;
    };

    std::optional<Xp60Performance> performance;
    std::vector<BlockIssueAt> issues;   // structural errors and range warnings
    std::vector<MissingBlock> missing;  // blocks the image does not fully cover

    [[nodiscard]] bool ok() const noexcept { return performance.has_value(); }
    [[nodiscard]] std::size_t errorCount() const noexcept;
    [[nodiscard]] bool hasWarnings() const noexcept;
    [[nodiscard]] std::string describe() const; // one line per issue / missing block
};

// Performance <-> bytes, with the same contract as Xp60PatchCodec: it never
// normalises. Bytes not described by the tables and out-of-range values travel
// through unchanged, so
//     encodeToImage(decode(image)) reproduces the seventeen blocks byte-for-byte.
struct Xp60PerformanceCodec
{
    static constexpr std::size_t kBlockCount = 1 + Xp60PerformanceLayout::kPartCount;

    [[nodiscard]] static Xp60PerformanceDecodeResult decode(const MemoryImage& image,
                                                            const roland::RolandAddress& base);

    // Writes the seventeen blocks into an image at `base` (nothing in between).
    [[nodiscard]] static MemoryImage encodeToImage(const Xp60Performance& performance,
                                                   const roland::RolandAddress& base);

    // DT1 messages transmitting the Performance to `base`, each carrying at
    // most maxPayloadBytes data bytes (the XP-60 requires <= 128). Empty on
    // address overflow.
    [[nodiscard]] static std::vector<roland::RolandSysExMessage> encodeToDataSets(
        const Xp60Performance& performance, roland::RolandDeviceId deviceId, const roland::RolandModelId& modelId,
        const roland::RolandAddress& base, std::size_t maxPayloadBytes = 128);

    // Raw block bytes in layout order (Common, Part 1..16).
    [[nodiscard]] static std::array<roland::ByteVector, kBlockCount> blockBytes(const Xp60Performance& performance);

    // Sends changed spans within each documented block. Preserves whole encoded
    // parameters at span/chunk boundaries (including multi-byte nibble values),
    // so a Part-level edit costs one short message rather than 3993 bytes.
    [[nodiscard]] static std::vector<roland::RolandSysExMessage> encodeChangesToDataSets(
        const Xp60Performance& before, const Xp60Performance& after, roland::RolandDeviceId deviceId,
        const roland::RolandModelId& modelId, const roland::RolandAddress& base,
        std::size_t maxPayloadBytes = 128);
};

} // namespace xp60studio::xpmodel
