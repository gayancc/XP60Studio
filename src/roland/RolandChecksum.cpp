#include "roland/RolandChecksum.h"

namespace xp60studio::roland {

namespace {

unsigned sumLow7(ByteSpan bytes, unsigned running) noexcept
{
    for (const Byte b : bytes) {
        running = (running + (b & 0x7Fu)) & 0x7Fu;
    }
    return running;
}

} // namespace

Byte RolandChecksum::compute(ByteSpan addressAndBody) noexcept
{
    const unsigned sum = sumLow7(addressAndBody, 0u);
    return static_cast<Byte>((128u - sum) & 0x7Fu);
}

Byte RolandChecksum::compute(ByteSpan address, ByteSpan body) noexcept
{
    const unsigned sum = sumLow7(body, sumLow7(address, 0u));
    return static_cast<Byte>((128u - sum) & 0x7Fu);
}

bool RolandChecksum::verify(ByteSpan addressAndBody, Byte checksum) noexcept
{
    return compute(addressAndBody) == checksum;
}

bool RolandChecksum::verify(ByteSpan address, ByteSpan body, Byte checksum) noexcept
{
    return compute(address, body) == checksum;
}

} // namespace xp60studio::roland
