#include "roland/RolandSize.h"

namespace xp60studio::roland {

RolandSize::RolandSize(Byte b0, Byte b1, Byte b2, Byte b3)
    : m_quad(b0, b1, b2, b3)
{
}

std::optional<RolandSize> RolandSize::fromBytes(ByteSpan bytes) noexcept
{
    if (const auto quad = detail::SevenBitQuad::fromBytes(bytes)) {
        return RolandSize(*quad);
    }
    return std::nullopt;
}

std::optional<RolandSize> RolandSize::fromValue(std::uint64_t byteCount) noexcept
{
    if (const auto quad = detail::SevenBitQuad::fromValue(byteCount)) {
        return RolandSize(*quad);
    }
    return std::nullopt;
}

std::optional<RolandSize> RolandSize::parseHex(std::string_view text) noexcept
{
    if (const auto quad = detail::SevenBitQuad::parseHex(text)) {
        return RolandSize(*quad);
    }
    return std::nullopt;
}

} // namespace xp60studio::roland
