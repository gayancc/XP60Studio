#pragma once

#include "protocol/RolandRequestTracker.h"
#include "roland/RolandSysExMessage.h"

#include <chrono>
#include <cstddef>
#include <vector>

namespace xp60studio::protocol {

// Pacing configuration for exclusive transfers. USB, DIN and wireless
// interfaces tolerate different rates, so nothing here is hard-coded into the
// transport; the application applies the values at send time.
struct TransferPacing
{
    std::chrono::milliseconds interMessageDelay{20};
    std::size_t maxDataSetPayloadBytes{128};
    RequestTimeouts timeouts{};

    [[nodiscard]] bool isValid() const noexcept
    {
        return maxDataSetPayloadBytes > 0 && interMessageDelay.count() >= 0 && timeouts.firstResponse.count() > 0
            && timeouts.betweenChunks.count() > 0;
    }
};

// Splits a DT1 into consecutive DT1 messages of at most `maxPayloadBytes`
// data bytes each, advancing the address with Roland 7-bit carry.
// Returns an empty vector when the input is not a DT1 or the range overflows.
[[nodiscard]] std::vector<roland::RolandSysExMessage> chunkDataSet(const roland::RolandSysExMessage& dataSet,
                                                                   std::size_t maxPayloadBytes);

} // namespace xp60studio::protocol
