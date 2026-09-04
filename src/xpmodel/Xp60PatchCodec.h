#pragma once

#include "roland/RolandAddress.h"
#include "roland/RolandDeviceId.h"
#include "roland/RolandModelId.h"
#include "roland/RolandSysExMessage.h"
#include "xpmodel/BlockCodec.h"
#include "xpmodel/MemoryImage.h"
#include "xpmodel/Xp60Patch.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <optional>
#include <string>
#include <vector>

namespace xp60studio::xpmodel {

// Result of decoding one Patch from a memory image.
struct Xp60PatchDecodeResult
{
    struct BlockIssueAt
    {
        std::string block; // "Patch Common", "Tone 1", ...
        BlockIssue issue;
    };
    struct MissingBlock
    {
        std::string block;
        roland::RolandAddress address;
        MemoryImage::Coverage coverage;
    };

    std::optional<Xp60Patch> patch;
    std::vector<BlockIssueAt> issues;   // structural errors and range warnings
    std::vector<MissingBlock> missing;  // blocks the image does not fully cover

    [[nodiscard]] bool ok() const noexcept { return patch.has_value(); }
    [[nodiscard]] std::size_t errorCount() const noexcept;
    [[nodiscard]] bool hasWarnings() const noexcept;
    [[nodiscard]] std::string describe() const; // one line per issue / missing block
};

// Patch <-> bytes. Never normalises: bytes not described by the tables and
// out-of-range values travel through unchanged, so
//     encodeToImage(decode(image)) reproduces the five blocks byte-for-byte.
struct Xp60PatchCodec
{
    [[nodiscard]] static Xp60PatchDecodeResult decode(const MemoryImage& image, const roland::RolandAddress& patchBase);

    // Writes the five blocks into an image at patchBase (nothing in between).
    [[nodiscard]] static MemoryImage encodeToImage(const Xp60Patch& patch, const roland::RolandAddress& patchBase);

    // DT1 messages that transmit the Patch to patchBase, each carrying at most
    // maxPayloadBytes data bytes (the XP-60 requires <= 128). Empty on
    // address overflow.
    [[nodiscard]] static std::vector<roland::RolandSysExMessage> encodeToDataSets(const Xp60Patch& patch,
                                                                                  roland::RolandDeviceId deviceId,
                                                                                  const roland::RolandModelId& modelId,
                                                                                  const roland::RolandAddress& patchBase,
                                                                                  std::size_t maxPayloadBytes = 128);

    // Raw block bytes of a Patch in layout order (Common, Tone 1..4).
    [[nodiscard]] static std::array<roland::ByteVector, 5> blockBytes(const Xp60Patch& patch);

    // Sends changed spans within each documented block. Preserves whole encoded
    // parameters at span/chunk boundaries (including multi-byte nibble values).
    [[nodiscard]] static std::vector<roland::RolandSysExMessage> encodeChangesToDataSets(
        const Xp60Patch& before, const Xp60Patch& after, roland::RolandDeviceId deviceId,
        const roland::RolandModelId& modelId, const roland::RolandAddress& patchBase,
        std::size_t maxPayloadBytes = 128);
};

} // namespace xp60studio::xpmodel
