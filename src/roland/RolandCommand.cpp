#include "roland/RolandCommand.h"

namespace xp60studio::roland {

std::optional<RolandCommand> commandFromByte(Byte value) noexcept
{
    switch (value) {
    case commandByte(RolandCommand::DataRequest1):
        return RolandCommand::DataRequest1;
    case commandByte(RolandCommand::DataSet1):
        return RolandCommand::DataSet1;
    default:
        return std::nullopt;
    }
}

std::string_view commandShortName(RolandCommand command) noexcept
{
    switch (command) {
    case RolandCommand::DataRequest1:
        return "RQ1";
    case RolandCommand::DataSet1:
        return "DT1";
    }
    return "?";
}

std::string_view commandLongName(RolandCommand command) noexcept
{
    switch (command) {
    case RolandCommand::DataRequest1:
        return "Data Request 1";
    case RolandCommand::DataSet1:
        return "Data Set 1";
    }
    return "Unknown command";
}

} // namespace xp60studio::roland
