#pragma once

#include "roland/RolandTypes.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace xp60studio::xpmodel {

// A 12-character XP-60 Patch name (Patch Name 1..12 at Patch Common offset 0).
//
// Roland stores one printable ASCII character per byte and pads with spaces.
// The accepted set is 0x20..0x7E; whether the XP-60 further restricts it is
// recorded as unknown in docs/protocol/XP60_PATCH_PARAMETER_MAP.md.
class PatchName
{
public:
    static constexpr std::size_t kLength = 12;
    static constexpr roland::Byte kMinChar = 0x20;
    static constexpr roland::Byte kMaxChar = 0x7E;
    using Bytes = std::array<roland::Byte, kLength>;

    PatchName() noexcept; // twelve spaces

    [[nodiscard]] static bool isAllowedChar(roland::Byte c) noexcept { return c >= kMinChar && c <= kMaxChar; }
    // Exactly 12 bytes, each an allowed character.
    [[nodiscard]] static std::optional<PatchName> fromBytes(roland::ByteSpan bytes) noexcept;
    // Up to 12 allowed characters; shorter text is right-padded with spaces.
    [[nodiscard]] static std::optional<PatchName> fromText(std::string_view text) noexcept;

    [[nodiscard]] const Bytes& bytes() const noexcept { return m_bytes; }
    // Text without trailing padding.
    [[nodiscard]] std::string text() const;
    // Exactly 12 characters, padding included.
    [[nodiscard]] std::string paddedText() const;
    [[nodiscard]] bool isBlank() const noexcept;

    friend bool operator==(const PatchName&, const PatchName&) noexcept = default;

private:
    Bytes m_bytes{};
};

} // namespace xp60studio::xpmodel
