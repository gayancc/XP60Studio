#pragma once

#include "roland/RolandTypes.h"

#include <array>
#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace xp60studio::roland::detail {

// Four 7-bit bytes forming a 28-bit big-endian value.
//
// Roland addresses and sizes share this encoding: each byte carries seven
// significant bits, and arithmetic carries at 0x80 rather than 0x100.
// Representing the quantity as a linear 28-bit integer makes carry behaviour
// fall out of ordinary integer arithmetic; the byte view is derived on demand.
class SevenBitQuad
{
public:
    static constexpr std::size_t kByteCount = 4;
    static constexpr std::uint32_t kMaxValue = (1u << 28) - 1u;
    using Bytes = std::array<Byte, kByteCount>;

    constexpr SevenBitQuad() noexcept = default;

    // Throws std::invalid_argument when a byte has bit 7 set.
    SevenBitQuad(Byte b0, Byte b1, Byte b2, Byte b3);

    [[nodiscard]] static std::optional<SevenBitQuad> fromBytes(ByteSpan bytes) noexcept;
    [[nodiscard]] static std::optional<SevenBitQuad> fromValue(std::uint64_t linear) noexcept;

    // Accepts "03 00 00 00", "03000000", "03-00-00-00", "0x03 0x00 0x00 0x00".
    [[nodiscard]] static std::optional<SevenBitQuad> parseHex(std::string_view text) noexcept;

    [[nodiscard]] constexpr std::uint32_t value() const noexcept { return m_value; }
    [[nodiscard]] Bytes bytes() const noexcept;
    [[nodiscard]] std::string toHexString() const;

    [[nodiscard]] std::optional<SevenBitQuad> plus(std::uint64_t offset) const noexcept;
    [[nodiscard]] std::optional<SevenBitQuad> minus(std::uint64_t offset) const noexcept;

    friend constexpr auto operator<=>(const SevenBitQuad&, const SevenBitQuad&) noexcept = default;

private:
    explicit constexpr SevenBitQuad(std::uint32_t linear) noexcept : m_value(linear) {}

    std::uint32_t m_value = 0;
};

} // namespace xp60studio::roland::detail
