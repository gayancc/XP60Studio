#pragma once

#include "protocol/RequestState.h"
#include "roland/RolandSysExMessage.h"

#include <chrono>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace xp60studio::protocol {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

struct RequestId
{
    std::uint64_t value = 0;

    [[nodiscard]] constexpr bool isValid() const noexcept { return value != 0; }
    friend constexpr auto operator<=>(const RequestId&, const RequestId&) noexcept = default;
};

// Observable record of one RQ1 and the DT1 data collected for it.
//
// A request for N bytes may be answered by any number of DT1 messages; the
// coverage bitmap records which bytes of the requested range have arrived so
// completion does not depend on assumptions about device chunking.
struct RequestOperation
{
    RequestId id;
    roland::RolandSysExMessage request; // always an RQ1
    RequestState state = RequestState::RequestSent;

    TimePoint createdAt{};
    std::optional<TimePoint> sentAt;
    std::optional<TimePoint> firstDataAt;
    std::optional<TimePoint> lastActivityAt;
    std::optional<TimePoint> finishedAt;

    std::uint32_t expectedBytes = 0;
    std::uint32_t receivedBytes = 0; // distinct bytes covered
    unsigned chunkCount = 0;
    std::vector<roland::Byte> data;  // expectedBytes long, filled as chunks arrive
    std::vector<bool> coverage;      // expectedBytes long

    std::string failureReason;       // for TimedOut / Cancelled / FailedValidation
    std::vector<std::string> notes;  // non-fatal observations (overlaps, out-of-order chunks)

    [[nodiscard]] bool isComplete() const noexcept { return receivedBytes == expectedBytes && expectedBytes != 0; }
    [[nodiscard]] double progress() const noexcept
    {
        return expectedBytes == 0 ? 0.0 : static_cast<double>(receivedBytes) / static_cast<double>(expectedBytes);
    }
};

} // namespace xp60studio::protocol
