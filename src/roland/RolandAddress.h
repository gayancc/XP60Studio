#pragma once

#include "roland/SevenBitQuad.h"

#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace xp60studio::roland {

// A Roland parameter address: four 7-bit bytes (28 significant bits).
//
// Callers never manipulate raw address bytes directly; offsets are applied
// through plus()/distanceTo() so carry behaviour lives in exactly one place.
class RolandAddress
{
public:
    static constexpr std::size_t kByteCount = detail::SevenBitQuad::kByteCount;
    using Bytes = detail::SevenBitQuad::Bytes;

    constexpr RolandAddress() noexcept = default;

    // Throws std::invalid_argument when any byte has bit 7 set.
    RolandAddress(Byte b0, Byte b1, Byte b2, Byte b3);

    [[nodiscard]] static std::optional<RolandAddress> fromBytes(ByteSpan bytes) noexcept;
    [[nodiscard]] static std::optional<RolandAddress> fromValue(std::uint64_t linear) noexcept;
    [[nodiscard]] static std::optional<RolandAddress> parseHex(std::string_view text) noexcept;

    // Linear 28-bit value: b0 * 2^21 + b1 * 2^14 + b2 * 2^7 + b3.
    [[nodiscard]] constexpr std::uint32_t value() const noexcept { return m_quad.value(); }
    [[nodiscard]] Bytes bytes() const noexcept { return m_quad.bytes(); }
    [[nodiscard]] std::string toHexString() const { return m_quad.toHexString(); }

    // Address arithmetic with 7-bit carry. Returns nullopt on overflow/underflow.
    [[nodiscard]] std::optional<RolandAddress> plus(std::uint64_t byteOffset) const noexcept;
    [[nodiscard]] std::optional<RolandAddress> minus(std::uint64_t byteOffset) const noexcept;

    // Number of bytes from this address up to `later` (later must not precede this).
    [[nodiscard]] std::optional<std::uint32_t> distanceTo(const RolandAddress& later) const noexcept;

    friend constexpr auto operator<=>(const RolandAddress&, const RolandAddress&) noexcept = default;

private:
    explicit constexpr RolandAddress(detail::SevenBitQuad quad) noexcept : m_quad(quad) {}

    detail::SevenBitQuad m_quad;
};

} // namespace xp60studio::roland
