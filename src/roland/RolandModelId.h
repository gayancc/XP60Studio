#pragma once

#include "roland/RolandTypes.h"

#include <array>
#include <initializer_list>
#include <optional>
#include <string>

namespace xp60studio::roland {

// Roland model ID.
//
// Roland model IDs are one or more 7-bit bytes; older devices use a single
// byte (e.g. JV-1080 = 6AH) while later devices prefix with 00H
// (e.g. XP-80/XP-60 = 00H 6AH). The length is part of the identity, so a
// parser must know which model IDs it expects before it can locate the
// command byte. See docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md.
class RolandModelId
{
public:
    static constexpr std::size_t kMaxBytes = 4;

    RolandModelId() noexcept = default;

    // Throws std::invalid_argument for an empty list, more than kMaxBytes
    // bytes, or any byte with bit 7 set.
    RolandModelId(std::initializer_list<Byte> bytes);

    [[nodiscard]] static std::optional<RolandModelId> fromBytes(ByteSpan bytes) noexcept;

    [[nodiscard]] ByteSpan bytes() const noexcept { return ByteSpan(m_bytes.data(), m_length); }
    [[nodiscard]] std::size_t size() const noexcept { return m_length; }
    [[nodiscard]] bool isEmpty() const noexcept { return m_length == 0; }
    [[nodiscard]] std::string toHexString() const;

    friend bool operator==(const RolandModelId& lhs, const RolandModelId& rhs) noexcept;

private:
    std::array<Byte, kMaxBytes> m_bytes{};
    std::size_t m_length = 0;
};

} // namespace xp60studio::roland
