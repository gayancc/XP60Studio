#pragma once

#include "roland/RolandTypes.h"

#include <optional>
#include <string>
#include <string_view>

namespace xp60studio::roland {

// "F0 41 10 6A 12 ..." style formatting used by diagnostics and tests.
[[nodiscard]] std::string toHex(ByteSpan bytes, std::string_view separator = " ");
[[nodiscard]] std::string toHex(Byte value);

// Parses hex byte text. Accepts whitespace, '-', ',' and ':' separators,
// optional 0x prefixes, and unseparated even-length strings ("03000000").
// Returns nullopt on any malformed token.
[[nodiscard]] std::optional<ByteVector> parseHexBytes(std::string_view text) noexcept;

} // namespace xp60studio::roland
