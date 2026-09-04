#pragma once

#include "roland/RolandTypes.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace xp60studio::xpmodel {

// A 12-character XP-60 Patch name (Patch Name 1..12 at Patch Common offset 0).
//
// Roland documents each byte as raw 32..127 (Parameter Address Map, Patch
// Name 1..12). That exact range is accepted here so a name captured from the
// instrument always round-trips; how 7FH is rendered is a presentation choice
// (see displayText()).
class PatchName
{
public:
    static constexpr std::size_t kLength = 12;
    static constexpr roland::Byte kMinChar = 0x20;
    static constexpr roland::Byte kMaxChar = 0x7F;
    using Bytes = std::array<roland::Byte, kLength>;

    PatchName() noexcept; // twelve spaces

    [[nodiscard]] static bool isAllowedChar(roland::Byte c) noexcept { return c >= kMinChar && c <= kMaxChar; }
    // Exactly 12 bytes, each an allowed character.
    [[nodiscard]] static std::optional<PatchName> fromBytes(roland::ByteSpan bytes) noexcept;
    // Up to 12 allowed characters; shorter text is right-padded with spaces.
    [[nodiscard]] static std::optional<PatchName> fromText(std::string_view text) noexcept;

    [[nodiscard]] const Bytes& bytes() const noexcept { return m_bytes; }
    // Text without trailing padding; bytes are returned verbatim.
    [[nodiscard]] std::string text() const;
    // Like text() but with the one non-printable value in the range (7FH)
    // shown as '?', for UI use only.
    [[nodiscard]] std::string displayText() const;
    // Exactly 12 characters, padding included.
    [[nodiscard]] std::string paddedText() const;
    [[nodiscard]] bool isBlank() const noexcept;

    friend bool operator==(const PatchName&, const PatchName&) noexcept = default;

private:
    Bytes m_bytes{};
};

} // namespace xp60studio::xpmodel
