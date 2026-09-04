#include "diagnostics/ProtocolLogEntry.h"

#include "roland/HexFormat.h"
#include "roland/RolandCodec.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace xp60studio::diagnostics {

std::string_view logDirectionName(LogDirection direction) noexcept
{
    switch (direction) {
    case LogDirection::In:
        return "IN";
    case LogDirection::Out:
        return "OUT";
    case LogDirection::System:
        return "SYS";
    }
    return "?";
}

std::string_view logKindName(LogKind kind) noexcept
{
    switch (kind) {
    case LogKind::RolandDataRequest:
        return "RolandDataRequest";
    case LogKind::RolandDataSet:
        return "RolandDataSet";
    case LogKind::RolandInvalid:
        return "RolandInvalid";
    case LogKind::OtherSysEx:
        return "OtherSysEx";
    case LogKind::ChannelMessage:
        return "ChannelMessage";
    case LogKind::SystemMessage:
        return "SystemMessage";
    case LogKind::Transport:
        return "Transport";
    case LogKind::Operation:
        return "Operation";
    case LogKind::Info:
        return "Info";
    }
    return "Unknown";
}

std::string_view logSeverityName(LogSeverity severity) noexcept
{
    switch (severity) {
    case LogSeverity::Info:
        return "Info";
    case LogSeverity::Warning:
        return "Warning";
    case LogSeverity::Error:
        return "Error";
    }
    return "Unknown";
}

std::string_view checksumStatusName(ChecksumStatus status) noexcept
{
    switch (status) {
    case ChecksumStatus::NotApplicable:
        return "-";
    case ChecksumStatus::Valid:
        return "OK";
    case ChecksumStatus::Invalid:
        return "INVALID";
    }
    return "?";
}

std::string formatTimeOfDay(std::chrono::system_clock::time_point time)
{
    using namespace std::chrono;
    const auto sinceEpoch = time.time_since_epoch();
    const auto seconds = duration_cast<std::chrono::seconds>(sinceEpoch);
    const auto millis = duration_cast<milliseconds>(sinceEpoch - seconds).count();
    const std::time_t tt = static_cast<std::time_t>(seconds.count());
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &tt);
#else
    localtime_r(&tt, &local);
#endif
    std::ostringstream out;
    out << std::put_time(&local, "%H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << millis;
    return out.str();
}

std::string formatLogLine(const ProtocolLogEntry& entry)
{
    std::string line = formatTimeOfDay(entry.wallTime);
    line += ' ';
    std::string direction(logDirectionName(entry.direction));
    while (direction.size() < 3) {
        direction += ' ';
    }
    line += direction;
    line += ' ';
    line += entry.summary;
    return line;
}

namespace {

roland::ByteSpan asRoland(midi::MidiByteSpan raw) noexcept
{
    return roland::ByteSpan(raw.data(), raw.size());
}

std::string channelMessageSummary(midi::MidiByteSpan raw)
{
    if (raw.empty()) {
        return "empty message";
    }
    const midi::Byte status = raw[0];
    const int channel = (status & 0x0F) + 1;
    std::string out;
    switch (status & 0xF0) {
    case 0x80:
        out = "Note Off";
        break;
    case 0x90:
        out = "Note On";
        break;
    case 0xA0:
        out = "Poly Pressure";
        break;
    case 0xB0:
        out = "Control Change";
        break;
    case 0xC0:
        out = "Program Change";
        break;
    case 0xD0:
        out = "Channel Pressure";
        break;
    case 0xE0:
        out = "Pitch Bend";
        break;
    default:
        return "MIDI " + roland::toHex(asRoland(raw));
    }
    out += " ch=" + std::to_string(channel);
    if (raw.size() > 1) {
        out += " data=" + roland::toHex(roland::ByteSpan(raw.data() + 1, raw.size() - 1));
    }
    return out;
}

std::string systemMessageSummary(midi::MidiByteSpan raw)
{
    switch (raw.empty() ? 0 : raw[0]) {
    case 0xF1:
        return "MTC Quarter Frame";
    case 0xF2:
        return "Song Position";
    case 0xF3:
        return "Song Select";
    case 0xF6:
        return "Tune Request";
    case 0xF8:
        return "Timing Clock";
    case 0xFA:
        return "Start";
    case 0xFB:
        return "Continue";
    case 0xFC:
        return "Stop";
    case 0xFE:
        return "Active Sensing";
    case 0xFF:
        return "System Reset";
    default:
        return "System message " + roland::toHex(asRoland(raw));
    }
}

} // namespace

ProtocolLogEntry logRolandMessage(LogDirection direction, const roland::RolandSysExMessage& message,
                                  std::string endpoint, std::optional<std::uint64_t> requestId,
                                  std::chrono::system_clock::time_point wallTime)
{
    ProtocolLogEntry entry;
    entry.wallTime = wallTime;
    entry.direction = direction;
    entry.kind = message.isDataRequest() ? LogKind::RolandDataRequest : LogKind::RolandDataSet;
    entry.severity = LogSeverity::Info;
    entry.endpoint = std::move(endpoint);
    entry.deviceIdDisplay = message.deviceId().displayNumber();
    entry.modelIdHex = message.modelId().toHexString();
    entry.commandName = std::string(roland::commandShortName(message.command()));
    entry.addressHex = message.address().toHexString();
    entry.requestId = requestId;
    // A RolandSysExMessage carries its checksum by construction and the codec
    // rejects invalid checksums before a message object exists.
    entry.checksum = ChecksumStatus::Valid;

    const auto raw = message.encode();
    entry.rawHex = roland::toHex(roland::ByteSpan(raw.data(), raw.size()));

    std::string summary = "Roland ";
    summary += entry.commandName;
    summary += " device=" + std::to_string(*entry.deviceIdDisplay);
    summary += " address=" + entry.addressHex;
    if (message.isDataRequest()) {
        entry.sizeHex = message.size().toHexString();
        entry.payloadLength = message.size().value();
        summary += " size=" + entry.sizeHex;
    } else {
        entry.payloadLength = message.data().size();
        summary += " bytes=" + std::to_string(entry.payloadLength);
        summary += " checksum=OK";
    }
    if (requestId) {
        summary += " req=#" + std::to_string(*requestId);
    }
    entry.summary = std::move(summary);
    return entry;
}

ProtocolLogEntry logParseFailure(LogDirection direction, midi::MidiByteSpan raw, const roland::RolandParseFailure& failure,
                                 std::string endpoint, std::chrono::system_clock::time_point wallTime)
{
    ProtocolLogEntry entry;
    entry.wallTime = wallTime;
    entry.direction = direction;
    entry.endpoint = std::move(endpoint);
    entry.payloadLength = raw.size();
    entry.rawHex = roland::toHex(asRoland(raw));
    entry.detail = roland::describe(failure);

    if (!roland::isRolandSysEx(asRoland(raw))) {
        entry.kind = LogKind::OtherSysEx;
        entry.severity = LogSeverity::Info;
        entry.summary = "SysEx (non-Roland) " + std::to_string(raw.size()) + " bytes";
        if (raw.size() >= 2) {
            entry.summary += " manufacturer=" + roland::toHex(raw[1]);
        }
        return entry;
    }

    entry.kind = LogKind::RolandInvalid;
    entry.severity = failure.error == roland::RolandParseError::UnsupportedModel ? LogSeverity::Warning : LogSeverity::Error;
    entry.checksum = failure.error == roland::RolandParseError::InvalidChecksum ? ChecksumStatus::Invalid
                                                                                : ChecksumStatus::NotApplicable;
    if (raw.size() > 2) {
        if (const auto deviceId = roland::RolandDeviceId::fromByte(raw[2])) {
            entry.deviceIdDisplay = deviceId->displayNumber();
        }
    }
    entry.summary = "Roland SysEx rejected: " + std::string(roland::parseErrorName(failure.error)) + " ("
        + std::to_string(raw.size()) + " bytes)";
    if (failure.error == roland::RolandParseError::InvalidChecksum) {
        entry.summary += " checksum=INVALID";
    }
    return entry;
}

ProtocolLogEntry logRawMidi(LogDirection direction, midi::MidiByteSpan raw, std::string endpoint,
                            std::chrono::system_clock::time_point wallTime)
{
    ProtocolLogEntry entry;
    entry.wallTime = wallTime;
    entry.direction = direction;
    entry.endpoint = std::move(endpoint);
    entry.payloadLength = raw.size();
    entry.rawHex = roland::toHex(asRoland(raw));
    if (!raw.empty() && raw[0] >= 0xF0) {
        entry.kind = LogKind::SystemMessage;
        entry.summary = systemMessageSummary(raw);
    } else {
        entry.kind = LogKind::ChannelMessage;
        entry.summary = channelMessageSummary(raw);
    }
    return entry;
}

ProtocolLogEntry logSystem(LogKind kind, LogSeverity severity, std::string summary,
                           std::chrono::system_clock::time_point wallTime, std::string detail,
                           std::optional<std::uint64_t> requestId)
{
    ProtocolLogEntry entry;
    entry.wallTime = wallTime;
    entry.direction = LogDirection::System;
    entry.kind = kind;
    entry.severity = severity;
    entry.summary = std::move(summary);
    entry.detail = std::move(detail);
    entry.requestId = requestId;
    return entry;
}

} // namespace xp60studio::diagnostics
