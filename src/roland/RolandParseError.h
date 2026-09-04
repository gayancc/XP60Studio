#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace xp60studio::roland {

// Distinguishable reasons why a byte sequence is not a valid Roland RQ1/DT1.
enum class RolandParseError {
    NotSysEx,            // does not start with F0
    Truncated,           // too short to hold the fixed fields
    MissingEnd,          // last byte is not F7
    NotRoland,           // manufacturer ID is not 41H
    InvalidDeviceId,     // device ID byte outside 10H..1FH
    UnsupportedModel,    // model ID does not match any known model
    UnsupportedCommand,  // command byte is neither RQ1 nor DT1
    InvalidAddressByte,  // an address byte has bit 7 set
    InvalidSizeByte,     // an RQ1 size byte has bit 7 set
    InvalidSize,         // RQ1 body is not exactly four size bytes, or size is zero
    InvalidDataByte,     // a DT1 data byte has bit 7 set
    EmptyData,           // DT1 carries no data bytes
    InvalidChecksum,     // checksum does not validate
};

struct RolandParseFailure
{
    RolandParseError error = RolandParseError::NotSysEx;
    std::size_t byteOffset = 0; // offset of the offending byte (best effort)
    std::string detail;         // human-readable explanation
};

[[nodiscard]] std::string_view parseErrorName(RolandParseError error) noexcept;
[[nodiscard]] std::string_view parseErrorDescription(RolandParseError error) noexcept;
[[nodiscard]] std::string describe(const RolandParseFailure& failure);

} // namespace xp60studio::roland
