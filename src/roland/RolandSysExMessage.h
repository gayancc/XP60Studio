#pragma once

#include "roland/RolandAddress.h"
#include "roland/RolandCommand.h"
#include "roland/RolandDeviceId.h"
#include "roland/RolandModelId.h"
#include "roland/RolandSize.h"
#include "roland/RolandTypes.h"

#include <optional>
#include <string>

namespace xp60studio::roland {

// A validated Roland exclusive message of the XP/JV "one-way" transfer family.
//
//   RQ1:  F0 41 dev <model...> 11 a0 a1 a2 a3 s0 s1 s2 s3 sum F7
//   DT1:  F0 41 dev <model...> 12 a0 a1 a2 a3 d0 ... dn     sum F7
//
// Instances are only constructible through the factory functions, so a
// RolandSysExMessage always encodes to a well-formed message with a correct
// checksum. Callers never append checksum bytes themselves.
class RolandSysExMessage
{
public:
    // Default: an RQ1 for the factory device ID with an empty model ID and a
    // zero address/size. Useful as a placeholder value; always replaced by a
    // factory-constructed message before use.
    RolandSysExMessage() = default;

    [[nodiscard]] static RolandSysExMessage dataRequest(RolandDeviceId deviceId, RolandModelId modelId,
                                                        RolandAddress address, RolandSize size);

    // Returns nullopt when data is empty or contains a byte with bit 7 set.
    [[nodiscard]] static std::optional<RolandSysExMessage> dataSet(RolandDeviceId deviceId, RolandModelId modelId,
                                                                   RolandAddress address, ByteVector data);

    [[nodiscard]] RolandCommand command() const noexcept { return m_command; }
    [[nodiscard]] bool isDataRequest() const noexcept { return m_command == RolandCommand::DataRequest1; }
    [[nodiscard]] bool isDataSet() const noexcept { return m_command == RolandCommand::DataSet1; }

    [[nodiscard]] RolandDeviceId deviceId() const noexcept { return m_deviceId; }
    [[nodiscard]] const RolandModelId& modelId() const noexcept { return m_modelId; }
    [[nodiscard]] RolandAddress address() const noexcept { return m_address; }

    // For RQ1: the requested size. For DT1: the number of data bytes.
    [[nodiscard]] RolandSize size() const noexcept { return m_size; }

    // DT1 payload (empty for RQ1).
    [[nodiscard]] ByteSpan data() const noexcept { return ByteSpan(m_data.data(), m_data.size()); }

    // First address after the range covered by this message.
    [[nodiscard]] std::optional<RolandAddress> endAddress() const noexcept;

    // Bytes covered by the checksum: address + (size | data).
    [[nodiscard]] ByteVector checksumBody() const;
    [[nodiscard]] Byte checksum() const;

    // Complete SysEx message from F0 through F7.
    [[nodiscard]] ByteVector encode() const;

    // One-line human readable description used by diagnostics.
    [[nodiscard]] std::string summary() const;

    friend bool operator==(const RolandSysExMessage& lhs, const RolandSysExMessage& rhs) noexcept;

private:
    RolandCommand m_command = RolandCommand::DataRequest1;
    RolandDeviceId m_deviceId = RolandDeviceId::factoryDefault();
    RolandModelId m_modelId;
    RolandAddress m_address;
    RolandSize m_size;
    ByteVector m_data;
};

} // namespace xp60studio::roland
