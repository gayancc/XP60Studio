#include "xpmodel/ParameterDescriptor.h"

namespace xp60studio::xpmodel {

std::string_view parameterEncodingName(ParameterEncoding encoding) noexcept
{
    switch (encoding) {
    case ParameterEncoding::SevenBit:
        return "SevenBit";
    case ParameterEncoding::Nibble:
        return "Nibble";
    case ParameterEncoding::Ascii:
        return "Ascii";
    }
    return "Unknown";
}

int ParameterDescriptor::encodingMaximum() const noexcept
{
    switch (encoding) {
    case ParameterEncoding::SevenBit:
    case ParameterEncoding::Ascii:
        return 0x7F;
    case ParameterEncoding::Nibble:
        return byteCount >= 8 ? 0x7FFFFFFF : (1 << (4 * byteCount)) - 1;
    }
    return 0x7F;
}

std::optional<std::string_view> ParameterDescriptor::label(int raw) const noexcept
{
    if (enumLabels.empty() || raw < rawMin) {
        return std::nullopt;
    }
    const auto index = static_cast<std::size_t>(raw - rawMin);
    if (index >= enumLabels.size()) {
        return std::nullopt;
    }
    return enumLabels[index];
}

std::optional<std::string> ParameterDescriptor::selfCheck() const
{
    if (id.empty()) {
        return "descriptor has an empty id";
    }
    if (byteCount == 0) {
        return std::string(id) + ": byteCount is zero";
    }
    if ((encoding == ParameterEncoding::SevenBit || encoding == ParameterEncoding::Ascii) && byteCount != 1) {
        return std::string(id) + ": " + std::string(parameterEncodingName(encoding)) + " parameters occupy one byte";
    }
    if (encoding == ParameterEncoding::Nibble && byteCount != 2 && byteCount != 4) {
        return std::string(id) + ": Nibble parameters occupy 2 or 4 bytes";
    }
    if (rawMin > rawMax) {
        return std::string(id) + ": rawMin exceeds rawMax";
    }
    if (rawMin < 0) {
        return std::string(id) + ": raw values are unsigned in Roland blocks";
    }
    if (rawMax > encodingMaximum()) {
        return std::string(id) + ": rawMax exceeds what the encoding can carry";
    }
    if (isEnumeration() && enumLabels.size() != static_cast<std::size_t>(rawMax - rawMin + 1)) {
        return std::string(id) + ": enum label count does not match the raw range";
    }
    return std::nullopt;
}

} // namespace xp60studio::xpmodel
