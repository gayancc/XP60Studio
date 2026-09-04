#include "protocol/TransferPacing.h"

#include <algorithm>

namespace xp60studio::protocol {

std::vector<roland::RolandSysExMessage> chunkDataSet(const roland::RolandSysExMessage& dataSet,
                                                     std::size_t maxPayloadBytes)
{
    std::vector<roland::RolandSysExMessage> chunks;
    if (!dataSet.isDataSet() || maxPayloadBytes == 0) {
        return chunks;
    }
    const auto data = dataSet.data();
    std::size_t offset = 0;
    while (offset < data.size()) {
        const std::size_t length = std::min(maxPayloadBytes, data.size() - offset);
        const auto address = dataSet.address().plus(offset);
        if (!address) {
            return {};
        }
        auto chunk = roland::RolandSysExMessage::dataSet(
            dataSet.deviceId(), dataSet.modelId(), *address,
            roland::ByteVector(data.begin() + static_cast<std::ptrdiff_t>(offset),
                               data.begin() + static_cast<std::ptrdiff_t>(offset + length)));
        if (!chunk) {
            return {};
        }
        chunks.push_back(std::move(*chunk));
        offset += length;
    }
    return chunks;
}

} // namespace xp60studio::protocol
