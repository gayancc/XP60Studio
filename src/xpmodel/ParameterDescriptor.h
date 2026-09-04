#pragma once

#include "roland/RolandTypes.h"
#include "xp60/Xp60Device.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace xp60studio::xpmodel {

// How a parameter is stored inside a Roland data block.
enum class ParameterEncoding {
    // One byte, bits 0-6 (0..127).
    SevenBit,
    // N bytes, each carrying 4 bits (0000 aaaa), most significant first.
    // Roland uses 2-byte nibbles for 0..255 values (e.g. wave numbers) and
    // 4-byte nibbles for 16-bit values.
    Nibble,
    // One byte holding a printable ASCII character (names).
    Ascii,
};

[[nodiscard]] std::string_view parameterEncodingName(ParameterEncoding encoding) noexcept;

// How the display value is rendered for the user.
enum class DisplayStyle {
    Number,   // plain number (with optional unit), e.g. -63..+63
    Pan,      // Roland pan notation: L64 .. 0 .. 63R
    NoteName, // MIDI note name: raw 0 = C-1 .. 127 = G9
};

[[nodiscard]] std::string_view displayStyleName(DisplayStyle style) noexcept;

// Static description of one parameter: where it lives in the block, how it is
// encoded, what raw values are legal and how they are displayed.
//
// Descriptors are data, not behaviour: the same descriptor drives decoding,
// encoding, validation, UI display, diffing and fingerprinting. Every entry
// carries the verification status of the fact it encodes.
struct ParameterDescriptor
{
    std::string_view id;          // stable identifier, e.g. "common.name.1"
    std::string_view name;        // Roland's name, e.g. "Patch Name 1"
    std::uint32_t offset = 0;     // byte offset within the block
    ParameterEncoding encoding = ParameterEncoding::SevenBit;
    std::uint8_t byteCount = 1;   // 1 for SevenBit/Ascii; 2 or 4 for Nibble
    int rawMin = 0;               // inclusive
    int rawMax = 127;             // inclusive
    int displayOffset = 0;        // display value = raw * displayScale + displayOffset
    int displayScale = 1;         // e.g. 2 for Roland's -100..+150 over raw 0..125, -1 for 0..-48
    DisplayStyle displayStyle = DisplayStyle::Number;
    std::string_view unit;        // "", "cent", "dB", ...
    std::span<const std::string_view> enumLabels; // indexed by raw - rawMin when non-empty
    std::string_view category;    // "Name", "Effects", "Pitch", "TVF", ...
    xp60::VerificationStatus status = xp60::VerificationStatus::Unknown;
    std::string_view sourceNote;

    [[nodiscard]] constexpr std::uint32_t endOffset() const noexcept { return offset + byteCount; }
    [[nodiscard]] constexpr bool isEnumeration() const noexcept { return !enumLabels.empty(); }
    [[nodiscard]] constexpr bool isText() const noexcept { return encoding == ParameterEncoding::Ascii; }

    // Largest value the encoding itself can carry (independent of rawMax).
    [[nodiscard]] int encodingMaximum() const noexcept;
    [[nodiscard]] bool isRawInRange(int raw) const noexcept { return raw >= rawMin && raw <= rawMax; }
    [[nodiscard]] int toDisplay(int raw) const noexcept { return raw * displayScale + displayOffset; }
    // Inverse of toDisplay; nullopt when `display` is not exactly representable.
    [[nodiscard]] std::optional<int> fromDisplay(int display) const noexcept;
    [[nodiscard]] std::optional<std::string_view> label(int raw) const noexcept;
    // User-facing text for a raw value: enum label, pan/note notation, or number with unit.
    [[nodiscard]] std::string formatDisplay(int raw) const;

    // Structural sanity of the descriptor itself (encoding/byteCount/range).
    [[nodiscard]] std::optional<std::string> selfCheck() const;
};

} // namespace xp60studio::xpmodel
