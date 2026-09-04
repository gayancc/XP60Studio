#pragma once

#include "roland/RolandTypes.h"

#include <compare>
#include <optional>

namespace xp60studio::roland {

// Roland exclusive device ID.
//
// The XP/JV family transmits the device ID as a byte in the range 10H..1FH.
// Roland front panels display it as 17..32 (byte value + 1). The factory
// default is byte 10H, displayed as "17".
class RolandDeviceId
{
public:
    static constexpr Byte kMinByte = 0x10;
    static constexpr Byte kMaxByte = 0x1F;
    static constexpr int kMinDisplayNumber = kMinByte + 1; // 17
    static constexpr int kMaxDisplayNumber = kMaxByte + 1; // 32

    [[nodiscard]] static std::optional<RolandDeviceId> fromByte(Byte value) noexcept;
    [[nodiscard]] static std::optional<RolandDeviceId> fromDisplayNumber(int displayNumber) noexcept;
    [[nodiscard]] static RolandDeviceId factoryDefault() noexcept;

    [[nodiscard]] constexpr Byte byte() const noexcept { return m_byte; }
    [[nodiscard]] constexpr int displayNumber() const noexcept { return static_cast<int>(m_byte) + 1; }

    friend constexpr auto operator<=>(const RolandDeviceId&, const RolandDeviceId&) noexcept = default;

private:
    explicit constexpr RolandDeviceId(Byte value) noexcept : m_byte(value) {}

    Byte m_byte = kMinByte;
};

} // namespace xp60studio::roland
