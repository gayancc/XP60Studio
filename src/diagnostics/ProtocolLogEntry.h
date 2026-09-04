#pragma once

#include "midi/MidiTypes.h"
#include "roland/RolandParseError.h"
#include "roland/RolandSysExMessage.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace xp60studio::diagnostics {

enum class LogDirection {
    In,
    Out,
    System, // application / transport events, not MIDI traffic
};

enum class LogKind {
    RolandDataRequest,
    RolandDataSet,
    RolandInvalid,   // Roland SysEx that failed validation
    OtherSysEx,      // non-Roland exclusive
    ChannelMessage,
    SystemMessage,   // system common / realtime
    Transport,       // open/close/error events
    Operation,       // request lifecycle events
    Info,
};

enum class LogSeverity {
    Info,
    Warning,
    Error,
};

enum class ChecksumStatus {
    NotApplicable,
    Valid,
    Invalid,
};

// One structured diagnostics record. Presentation models render these; the
// formatter below produces the human-readable one-line form.
struct ProtocolLogEntry
{
    std::chrono::system_clock::time_point wallTime{};
    LogDirection direction = LogDirection::System;
    LogKind kind = LogKind::Info;
    LogSeverity severity = LogSeverity::Info;
    std::string endpoint;            // endpoint display name when relevant
    std::string summary;             // human readable, without timestamp/direction

    // Roland interpretation (empty / nullopt when not applicable)
    std::optional<int> deviceIdDisplay;
    std::string modelIdHex;
    std::string commandName;         // "RQ1" / "DT1"
    std::string addressHex;
    std::string sizeHex;             // RQ1 requested size
    std::size_t payloadLength = 0;   // DT1 data bytes or raw message length
    ChecksumStatus checksum = ChecksumStatus::NotApplicable;
    std::optional<std::uint64_t> requestId;

    std::string rawHex;              // full raw bytes (advanced view)
    std::string detail;              // extra technical detail (parse failure, notes)
};

[[nodiscard]] std::string_view logDirectionName(LogDirection direction) noexcept; // "IN", "OUT", "SYS"
[[nodiscard]] std::string_view logKindName(LogKind kind) noexcept;
[[nodiscard]] std::string_view logSeverityName(LogSeverity severity) noexcept;
[[nodiscard]] std::string_view checksumStatusName(ChecksumStatus status) noexcept;

// "20:14:03.115" local wall-clock time.
[[nodiscard]] std::string formatTimeOfDay(std::chrono::system_clock::time_point time);

// "20:14:03.115 OUT Roland RQ1 device=17 address=11 00 00 00 size=00 00 0C 00"
[[nodiscard]] std::string formatLogLine(const ProtocolLogEntry& entry);

// Builders --------------------------------------------------------------------
[[nodiscard]] ProtocolLogEntry logRolandMessage(LogDirection direction, const roland::RolandSysExMessage& message,
                                                std::string endpoint, std::optional<std::uint64_t> requestId,
                                                std::chrono::system_clock::time_point wallTime);

[[nodiscard]] ProtocolLogEntry logParseFailure(LogDirection direction, midi::MidiByteSpan raw,
                                               const roland::RolandParseFailure& failure, std::string endpoint,
                                               std::chrono::system_clock::time_point wallTime);

[[nodiscard]] ProtocolLogEntry logRawMidi(LogDirection direction, midi::MidiByteSpan raw, std::string endpoint,
                                          std::chrono::system_clock::time_point wallTime);

[[nodiscard]] ProtocolLogEntry logSystem(LogKind kind, LogSeverity severity, std::string summary,
                                         std::chrono::system_clock::time_point wallTime, std::string detail = {},
                                         std::optional<std::uint64_t> requestId = std::nullopt);

} // namespace xp60studio::diagnostics
