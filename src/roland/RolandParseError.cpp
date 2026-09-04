#include "roland/RolandParseError.h"

namespace xp60studio::roland {

std::string_view parseErrorName(RolandParseError error) noexcept
{
    switch (error) {
    case RolandParseError::NotSysEx:
        return "NotSysEx";
    case RolandParseError::Truncated:
        return "Truncated";
    case RolandParseError::MissingEnd:
        return "MissingEnd";
    case RolandParseError::NotRoland:
        return "NotRoland";
    case RolandParseError::InvalidDeviceId:
        return "InvalidDeviceId";
    case RolandParseError::UnsupportedModel:
        return "UnsupportedModel";
    case RolandParseError::UnsupportedCommand:
        return "UnsupportedCommand";
    case RolandParseError::InvalidAddressByte:
        return "InvalidAddressByte";
    case RolandParseError::InvalidSizeByte:
        return "InvalidSizeByte";
    case RolandParseError::InvalidSize:
        return "InvalidSize";
    case RolandParseError::InvalidDataByte:
        return "InvalidDataByte";
    case RolandParseError::EmptyData:
        return "EmptyData";
    case RolandParseError::InvalidChecksum:
        return "InvalidChecksum";
    }
    return "Unknown";
}

std::string_view parseErrorDescription(RolandParseError error) noexcept
{
    switch (error) {
    case RolandParseError::NotSysEx:
        return "Message does not start with a System Exclusive status byte (F0)";
    case RolandParseError::Truncated:
        return "Message is too short to be a Roland RQ1/DT1 exclusive message";
    case RolandParseError::MissingEnd:
        return "Message does not end with End Of Exclusive (F7)";
    case RolandParseError::NotRoland:
        return "Manufacturer ID is not Roland (41H)";
    case RolandParseError::InvalidDeviceId:
        return "Device ID byte is outside the Roland range 10H..1FH";
    case RolandParseError::UnsupportedModel:
        return "Model ID does not match a supported device";
    case RolandParseError::UnsupportedCommand:
        return "Command is not Data Request 1 (11H) or Data Set 1 (12H)";
    case RolandParseError::InvalidAddressByte:
        return "An address byte has bit 7 set";
    case RolandParseError::InvalidSizeByte:
        return "A size byte has bit 7 set";
    case RolandParseError::InvalidSize:
        return "Data Request 1 must carry exactly four non-zero size bytes";
    case RolandParseError::InvalidDataByte:
        return "A data byte has bit 7 set";
    case RolandParseError::EmptyData:
        return "Data Set 1 carries no data bytes";
    case RolandParseError::InvalidChecksum:
        return "Checksum does not validate";
    }
    return "Unknown parse error";
}

std::string describe(const RolandParseFailure& failure)
{
    std::string out(parseErrorName(failure.error));
    out += " at byte ";
    out += std::to_string(failure.byteOffset);
    out += ": ";
    out += failure.detail.empty() ? std::string(parseErrorDescription(failure.error)) : failure.detail;
    return out;
}

} // namespace xp60studio::roland
