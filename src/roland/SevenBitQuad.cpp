#include "roland/SevenBitQuad.h"

#include "roland/HexFormat.h"

#include <stdexcept>

namespace xp60studio::roland::detail {

namespace {

constexpr std::uint32_t pack(Byte b0, Byte b1, Byte b2, Byte b3) noexcept
{
    return (static_cast<std::uint32_t>(b0) << 21) | (static_cast<std::uint32_t>(b1) << 14)
        | (static_cast<std::uint32_t>(b2) << 7) | static_cast<std::uint32_t>(b3);
}

} // namespace

SevenBitQuad::SevenBitQuad(Byte b0, Byte b1, Byte b2, Byte b3)
{
    if (!isDataByte(b0) || !isDataByte(b1) || !isDataByte(b2) || !isDataByte(b3)) {
        throw std::invalid_argument("Roland 7-bit quad byte has bit 7 set");
    }
    m_value = pack(b0, b1, b2, b3);
}

std::optional<SevenBitQuad> SevenBitQuad::fromBytes(ByteSpan bytes) noexcept
{
    if (bytes.size() != kByteCount) {
        return std::nullopt;
    }
    for (const Byte b : bytes) {
        if (!isDataByte(b)) {
            return std::nullopt;
        }
    }
    return SevenBitQuad(pack(bytes[0], bytes[1], bytes[2], bytes[3]));
}

std::optional<SevenBitQuad> SevenBitQuad::fromValue(std::uint64_t linear) noexcept
{
    if (linear > kMaxValue) {
        return std::nullopt;
    }
    return SevenBitQuad(static_cast<std::uint32_t>(linear));
}

std::optional<SevenBitQuad> SevenBitQuad::parseHex(std::string_view text) noexcept
{
    const auto bytes = parseHexBytes(text);
    if (!bytes) {
        return std::nullopt;
    }
    return fromBytes(*bytes);
}

SevenBitQuad::Bytes SevenBitQuad::bytes() const noexcept
{
    return Bytes{
        static_cast<Byte>((m_value >> 21) & 0x7F),
        static_cast<Byte>((m_value >> 14) & 0x7F),
        static_cast<Byte>((m_value >> 7) & 0x7F),
        static_cast<Byte>(m_value & 0x7F),
    };
}

std::string SevenBitQuad::toHexString() const
{
    const auto b = bytes();
    return toHex(ByteSpan(b.data(), b.size()));
}

std::optional<SevenBitQuad> SevenBitQuad::plus(std::uint64_t offset) const noexcept
{
    const std::uint64_t sum = static_cast<std::uint64_t>(m_value) + offset;
    return fromValue(sum);
}

std::optional<SevenBitQuad> SevenBitQuad::minus(std::uint64_t offset) const noexcept
{
    if (offset > m_value) {
        return std::nullopt;
    }
    return SevenBitQuad(static_cast<std::uint32_t>(m_value - offset));
}

} // namespace xp60studio::roland::detail
