#pragma once

#include "roland/SevenBitQuad.h"

#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace xp60studio::roland {

// A Roland data size as used by RQ1: four 7-bit bytes giving a byte count.
// Example: 00 00 01 00 means 128 bytes, 00 00 00 0C means 12 bytes.
class RolandSize
{
public:
    static constexpr std::size_t kByteCount = detail::SevenBitQuad::kByteCount;
    static constexpr std::uint32_t kMaxValue = detail::SevenBitQuad::kMaxValue;
    using Bytes = detail::SevenBitQuad::Bytes;

    constexpr RolandSize() noexcept = default;

    // Throws std::invalid_argument when any byte has bit 7 set.
    RolandSize(Byte b0, Byte b1, Byte b2, Byte b3);

    [[nodiscard]] static std::optional<RolandSize> fromBytes(ByteSpan bytes) noexcept;
    [[nodiscard]] static std::optional<RolandSize> fromValue(std::uint64_t byteCount) noexcept;
    [[nodiscard]] static std::optional<RolandSize> parseHex(std::string_view text) noexcept;

    // Number of data bytes described by this size.
    [[nodiscard]] constexpr std::uint32_t value() const noexcept { return m_quad.value(); }
    [[nodiscard]] constexpr bool isZero() const noexcept { return m_quad.value() == 0; }
    [[nodiscard]] Bytes bytes() const noexcept { return m_quad.bytes(); }
    [[nodiscard]] std::string toHexString() const { return m_quad.toHexString(); }

    friend constexpr auto operator<=>(const RolandSize&, const RolandSize&) noexcept = default;

private:
    explicit constexpr RolandSize(detail::SevenBitQuad quad) noexcept : m_quad(quad) {}

    detail::SevenBitQuad m_quad;
};

} // namespace xp60studio::roland
