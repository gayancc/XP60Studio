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
// A request may be answered by any number of DT1 messages; the coverage bitmap
// records which bytes of the requested range have arrived, so completion does
// not depend on assumptions about device chunking.
//
// The RQ1 size is an **address span**, not a count of payload bytes. Roland
// block addresses are padded — a Patch Tone block holds 129 bytes but the next
// Tone begins 0x200 address units later — so a request spanning several blocks
// is answered with only the populated blocks inside the span and the gaps
// between them never arrive. Requiring every byte of the span to be covered
// would therefore stall every multi-block read. Confirmed on hardware
// 2026-09-04; see ROLAND_XP60_PROTOCOL_FACTS.md §2.2.
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

    std::uint32_t expectedBytes = 0;        // the requested address span
    std::uint32_t receivedBytes = 0;        // distinct payload bytes covered
    std::uint32_t coveredThroughBytes = 0;  // one past the highest covered byte
    bool sawChunkOutOfOrder = false;        // a chunk started behind the high-water mark
    unsigned chunkCount = 0;
    std::vector<roland::Byte> data;  // expectedBytes long, filled as chunks arrive
    std::vector<bool> coverage;      // expectedBytes long

    std::string failureReason;       // for TimedOut / Cancelled / FailedValidation
    std::vector<std::string> notes;  // non-fatal observations (overlaps, out-of-order chunks)

    // Complete when every byte of the span arrived (a dense reply), or when a
    // padded reply has run from the start of the span to its end.
    //
    // A padded reply is only recognised as finished when it began at the
    // requested address, never went backwards, and reached the span end. Those
    // conditions are what separate "the gaps are padding" from "a block has not
    // arrived yet": the XP-60 answers from the request address in ascending
    // order (hardware-verified 2026-09-04), so a reply that starts late or
    // jumps backwards is not the padded pattern and must cover every byte.
    //
    // Limitation: a device that skipped forward past real data and then filled
    // the gap in a later message would be reported complete at the moment it
    // reached the span end. No XP-60 behaviour observed so far does this, and
    // distinguishing the two cases would require the block map, which lives in
    // the domain layer rather than here.
    [[nodiscard]] bool isComplete() const noexcept
    {
        if (expectedBytes == 0) {
            return false;
        }
        if (receivedBytes == expectedBytes) {
            return true;
        }
        const bool startedAtRequestAddress = !coverage.empty() && coverage.front();
        return startedAtRequestAddress && !sawChunkOutOfOrder && coveredThroughBytes == expectedBytes;
    }

    // How far through the requested span the device has got. For a dense reply
    // this equals receivedBytes / expectedBytes; for a padded one it advances
    // past the gaps instead of stalling at a fraction that never reaches 1.
    [[nodiscard]] double progress() const noexcept
    {
        return expectedBytes == 0 ? 0.0
                                  : static_cast<double>(coveredThroughBytes) / static_cast<double>(expectedBytes);
    }
};

} // namespace xp60studio::protocol
