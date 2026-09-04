#include "roland/HexFormat.h"

#include <cctype>

namespace xp60studio::roland {

namespace {

constexpr char kDigits[] = "0123456789ABCDEF";

std::optional<unsigned> nibble(char c) noexcept
{
    if (c >= '0' && c <= '9') {
        return static_cast<unsigned>(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return static_cast<unsigned>(c - 'a' + 10);
    }
    if (c >= 'A' && c <= 'F') {
        return static_cast<unsigned>(c - 'A' + 10);
    }
    return std::nullopt;
}

bool isSeparator(char c) noexcept
{
    return std::isspace(static_cast<unsigned char>(c)) != 0 || c == '-' || c == ',' || c == ':';
}

} // namespace

std::string toHex(ByteSpan bytes, std::string_view separator)
{
    std::string out;
    out.reserve(bytes.size() * (2 + separator.size()));
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i != 0) {
            out += separator;
        }
        out += kDigits[bytes[i] >> 4];
        out += kDigits[bytes[i] & 0x0F];
    }
    return out;
}

std::string toHex(Byte value)
{
    return toHex(ByteSpan(&value, 1));
}

std::optional<ByteVector> parseHexBytes(std::string_view text) noexcept
{
    ByteVector out;
    std::size_t i = 0;
    while (i < text.size()) {
        if (isSeparator(text[i])) {
            ++i;
            continue;
        }
        // Optional 0x / 0X prefix.
        if (text[i] == '0' && i + 1 < text.size() && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
            i += 2;
        }
        // Collect a run of hex digits.
        const std::size_t start = i;
        while (i < text.size() && nibble(text[i])) {
            ++i;
        }
        const std::size_t length = i - start;
        if (length == 0 || (length % 2) != 0) {
            return std::nullopt;
        }
        if (i < text.size() && !isSeparator(text[i])) {
            return std::nullopt;
        }
        for (std::size_t j = start; j < i; j += 2) {
            const auto hi = nibble(text[j]);
            const auto lo = nibble(text[j + 1]);
            out.push_back(static_cast<Byte>((*hi << 4) | *lo));
        }
    }
    if (out.empty()) {
        return std::nullopt;
    }
    return out;
}

} // namespace xp60studio::roland
