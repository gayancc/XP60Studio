#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Fundamental byte-level vocabulary shared by the Roland protocol layer.
// This layer is deliberately free of Qt and MIDI-transport dependencies.
namespace xp60studio::roland {

using Byte = std::uint8_t;
using ByteVector = std::vector<Byte>;
using ByteSpan = std::span<const Byte>;

// MIDI System Exclusive framing bytes.
inline constexpr Byte kSysExStart = 0xF0;
inline constexpr Byte kSysExEnd = 0xF7;

// Roland Corporation manufacturer ID (single byte form).
inline constexpr Byte kRolandManufacturerId = 0x41;

// Every byte inside a SysEx body must have bit 7 clear.
[[nodiscard]] constexpr bool isDataByte(Byte value) noexcept
{
    return value < 0x80;
}

[[nodiscard]] constexpr bool isRealtimeStatus(Byte value) noexcept
{
    return value >= 0xF8;
}

} // namespace xp60studio::roland
