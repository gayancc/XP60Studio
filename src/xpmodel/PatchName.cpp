#include "xpmodel/PatchName.h"

#include <algorithm>

namespace xp60studio::xpmodel {

PatchName::PatchName() noexcept
{
    m_bytes.fill(0x20);
}

std::optional<PatchName> PatchName::fromBytes(roland::ByteSpan bytes) noexcept
{
    if (bytes.size() != kLength) {
        return std::nullopt;
    }
    if (!std::all_of(bytes.begin(), bytes.end(), [](roland::Byte c) { return isAllowedChar(c); })) {
        return std::nullopt;
    }
    PatchName name;
    std::copy(bytes.begin(), bytes.end(), name.m_bytes.begin());
    return name;
}

std::optional<PatchName> PatchName::fromText(std::string_view text) noexcept
{
    if (text.size() > kLength) {
        return std::nullopt;
    }
    PatchName name;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<roland::Byte>(text[i]);
        if (!isAllowedChar(c)) {
            return std::nullopt;
        }
        name.m_bytes[i] = c;
    }
    return name;
}

std::string PatchName::text() const
{
    std::string out = paddedText();
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

std::string PatchName::displayText() const
{
    std::string out = text();
    for (auto& c : out) {
        if (static_cast<roland::Byte>(c) == 0x7F) {
            c = '?';
        }
    }
    return out;
}

std::string PatchName::paddedText() const
{
    return std::string(m_bytes.begin(), m_bytes.end());
}

bool PatchName::isBlank() const noexcept
{
    return std::all_of(m_bytes.begin(), m_bytes.end(), [](roland::Byte c) { return c == 0x20; });
}

} // namespace xp60studio::xpmodel
