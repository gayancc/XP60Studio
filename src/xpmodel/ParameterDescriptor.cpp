#include "xpmodel/ParameterDescriptor.h"

#include <algorithm>

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

std::string_view displayStyleName(DisplayStyle style) noexcept
{
    switch (style) {
    case DisplayStyle::Number:
        return "Number";
    case DisplayStyle::Pan:
        return "Pan";
    case DisplayStyle::NoteName:
        return "NoteName";
    }
    return "Unknown";
}

std::optional<int> ParameterDescriptor::fromDisplay(int display) const noexcept
{
    if (displayScale == 0) {
        return std::nullopt;
    }
    const int numerator = display - displayOffset;
    if (numerator % displayScale != 0) {
        return std::nullopt;
    }
    return numerator / displayScale;
}

std::string ParameterDescriptor::formatDisplay(int raw) const
{
    if (const auto text = label(raw)) {
        return std::string(*text);
    }
    if (encoding == ParameterEncoding::Ascii) {
        return std::string(1, static_cast<char>(raw & 0x7F));
    }
    const int display = toDisplay(raw);
    switch (displayStyle) {
    case DisplayStyle::Pan:
        if (display < 0) {
            return "L" + std::to_string(-display);
        }
        if (display > 0) {
            return std::to_string(display) + "R";
        }
        return "0";
    case DisplayStyle::NoteName: {
        static constexpr const char* kNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        const int note = std::max(0, display);
        return std::string(kNames[note % 12]) + std::to_string(note / 12 - 1);
    }
    case DisplayStyle::Number:
        break;
    }
    std::string out = display > 0 && displayOffset < 0 ? "+" + std::to_string(display) : std::to_string(display);
    if (!unit.empty()) {
        out += " ";
        out += unit;
    }
    return out;
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
    if (displayScale == 0) {
        return std::string(id) + ": displayScale must not be zero";
    }
    if (isEnumeration() && enumLabels.size() != static_cast<std::size_t>(rawMax - rawMin + 1)) {
        return std::string(id) + ": enum label count does not match the raw range";
    }
    return std::nullopt;
}

} // namespace xp60studio::xpmodel
