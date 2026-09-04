#include "roland/RolandAddress.h"

namespace xp60studio::roland {

RolandAddress::RolandAddress(Byte b0, Byte b1, Byte b2, Byte b3)
    : m_quad(b0, b1, b2, b3)
{
}

std::optional<RolandAddress> RolandAddress::fromBytes(ByteSpan bytes) noexcept
{
    if (const auto quad = detail::SevenBitQuad::fromBytes(bytes)) {
        return RolandAddress(*quad);
    }
    return std::nullopt;
}

std::optional<RolandAddress> RolandAddress::fromValue(std::uint64_t linear) noexcept
{
    if (const auto quad = detail::SevenBitQuad::fromValue(linear)) {
        return RolandAddress(*quad);
    }
    return std::nullopt;
}

std::optional<RolandAddress> RolandAddress::parseHex(std::string_view text) noexcept
{
    if (const auto quad = detail::SevenBitQuad::parseHex(text)) {
        return RolandAddress(*quad);
    }
    return std::nullopt;
}

std::optional<RolandAddress> RolandAddress::plus(std::uint64_t byteOffset) const noexcept
{
    if (const auto quad = m_quad.plus(byteOffset)) {
        return RolandAddress(*quad);
    }
    return std::nullopt;
}

std::optional<RolandAddress> RolandAddress::minus(std::uint64_t byteOffset) const noexcept
{
    if (const auto quad = m_quad.minus(byteOffset)) {
        return RolandAddress(*quad);
    }
    return std::nullopt;
}

std::optional<std::uint32_t> RolandAddress::distanceTo(const RolandAddress& later) const noexcept
{
    if (later.value() < value()) {
        return std::nullopt;
    }
    return later.value() - value();
}

} // namespace xp60studio::roland
