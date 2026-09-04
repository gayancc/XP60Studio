#include "roland/RolandSysExMessage.h"

#include "roland/RolandChecksum.h"

#include <algorithm>

namespace xp60studio::roland {

RolandSysExMessage RolandSysExMessage::dataRequest(RolandDeviceId deviceId, RolandModelId modelId,
                                                   RolandAddress address, RolandSize size)
{
    RolandSysExMessage message;
    message.m_command = RolandCommand::DataRequest1;
    message.m_deviceId = deviceId;
    message.m_modelId = modelId;
    message.m_address = address;
    message.m_size = size;
    return message;
}

std::optional<RolandSysExMessage> RolandSysExMessage::dataSet(RolandDeviceId deviceId, RolandModelId modelId,
                                                              RolandAddress address, ByteVector data)
{
    if (data.empty()) {
        return std::nullopt;
    }
    if (!std::all_of(data.begin(), data.end(), [](Byte b) { return isDataByte(b); })) {
        return std::nullopt;
    }
    const auto size = RolandSize::fromValue(data.size());
    if (!size) {
        return std::nullopt;
    }
    RolandSysExMessage message;
    message.m_command = RolandCommand::DataSet1;
    message.m_deviceId = deviceId;
    message.m_modelId = modelId;
    message.m_address = address;
    message.m_size = *size;
    message.m_data = std::move(data);
    return message;
}

std::optional<RolandAddress> RolandSysExMessage::endAddress() const noexcept
{
    return m_address.plus(m_size.value());
}

ByteVector RolandSysExMessage::checksumBody() const
{
    ByteVector body;
    const auto address = m_address.bytes();
    body.insert(body.end(), address.begin(), address.end());
    if (isDataRequest()) {
        const auto size = m_size.bytes();
        body.insert(body.end(), size.begin(), size.end());
    } else {
        body.insert(body.end(), m_data.begin(), m_data.end());
    }
    return body;
}

Byte RolandSysExMessage::checksum() const
{
    const auto body = checksumBody();
    return RolandChecksum::compute(ByteSpan(body.data(), body.size()));
}

ByteVector RolandSysExMessage::encode() const
{
    const auto body = checksumBody();
    ByteVector out;
    out.reserve(4 + m_modelId.size() + body.size() + 2);
    out.push_back(kSysExStart);
    out.push_back(kRolandManufacturerId);
    out.push_back(m_deviceId.byte());
    const auto model = m_modelId.bytes();
    out.insert(out.end(), model.begin(), model.end());
    out.push_back(commandByte(m_command));
    out.insert(out.end(), body.begin(), body.end());
    out.push_back(RolandChecksum::compute(ByteSpan(body.data(), body.size())));
    out.push_back(kSysExEnd);
    return out;
}

std::string RolandSysExMessage::summary() const
{
    std::string out = "Roland ";
    out += commandShortName(m_command);
    out += " device=" + std::to_string(m_deviceId.displayNumber());
    out += " model=" + m_modelId.toHexString();
    out += " address=" + m_address.toHexString();
    if (isDataRequest()) {
        out += " size=" + m_size.toHexString() + " (" + std::to_string(m_size.value()) + " bytes)";
    } else {
        out += " bytes=" + std::to_string(m_data.size());
    }
    return out;
}

bool operator==(const RolandSysExMessage& lhs, const RolandSysExMessage& rhs) noexcept
{
    return lhs.m_command == rhs.m_command && lhs.m_deviceId == rhs.m_deviceId && lhs.m_modelId == rhs.m_modelId
        && lhs.m_address == rhs.m_address && lhs.m_size == rhs.m_size && lhs.m_data == rhs.m_data;
}

} // namespace xp60studio::roland
