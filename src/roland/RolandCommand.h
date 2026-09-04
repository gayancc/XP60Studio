#pragma once

#include "roland/RolandTypes.h"

#include <optional>
#include <string_view>

namespace xp60studio::roland {

// Roland exclusive command IDs used by the XP/JV family "one-way" transfer
// protocol. Only the two commands needed for Phase 1 are modelled; any other
// byte is reported as an unsupported command by the parser.
enum class RolandCommand : Byte {
    DataRequest1 = 0x11, // RQ1: ask the device to send data from an address range
    DataSet1 = 0x12,     // DT1: transmit data to / from an address
};

[[nodiscard]] std::optional<RolandCommand> commandFromByte(Byte value) noexcept;
[[nodiscard]] constexpr Byte commandByte(RolandCommand command) noexcept
{
    return static_cast<Byte>(command);
}

// Short name as used in Roland documentation ("RQ1", "DT1").
[[nodiscard]] std::string_view commandShortName(RolandCommand command) noexcept;
// Long name ("Data Request 1", "Data Set 1").
[[nodiscard]] std::string_view commandLongName(RolandCommand command) noexcept;

} // namespace xp60studio::roland
