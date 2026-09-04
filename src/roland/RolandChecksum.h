#pragma once

#include "roland/RolandTypes.h"

namespace xp60studio::roland {

// Roland SysEx checksum.
//
// The checksum covers every byte between the command byte and the checksum
// itself: the four address bytes followed by either the four size bytes (RQ1)
// or the data bytes (DT1). The value is chosen so that
//     (sum(address..data) + checksum) mod 128 == 0
// i.e. checksum = (128 - (sum mod 128)) mod 128.
struct RolandChecksum
{
    [[nodiscard]] static Byte compute(ByteSpan addressAndBody) noexcept;
    [[nodiscard]] static Byte compute(ByteSpan address, ByteSpan body) noexcept;
    [[nodiscard]] static bool verify(ByteSpan addressAndBody, Byte checksum) noexcept;
    [[nodiscard]] static bool verify(ByteSpan address, ByteSpan body, Byte checksum) noexcept;
};

} // namespace xp60studio::roland
