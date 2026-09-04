#include "roland/RolandDeviceId.h"

namespace xp60studio::roland {

std::optional<RolandDeviceId> RolandDeviceId::fromByte(Byte value) noexcept
{
    if (value < kMinByte || value > kMaxByte) {
        return std::nullopt;
    }
    return RolandDeviceId(value);
}

std::optional<RolandDeviceId> RolandDeviceId::fromDisplayNumber(int displayNumber) noexcept
{
    if (displayNumber < kMinDisplayNumber || displayNumber > kMaxDisplayNumber) {
        return std::nullopt;
    }
    return RolandDeviceId(static_cast<Byte>(displayNumber - 1));
}

RolandDeviceId RolandDeviceId::factoryDefault() noexcept
{
    return RolandDeviceId(kMinByte);
}

} // namespace xp60studio::roland
