#pragma once

#include "protocol/RequestOperation.h"

#include <chrono>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace xp60studio::protocol {

struct RequestTimeouts
{
    std::chrono::milliseconds firstResponse{1500};
    std::chrono::milliseconds betweenChunks{1000};
};

// Correlates incoming DT1 messages with outstanding RQ1 operations.
//
// The tracker is deterministic: every time-dependent decision receives the
// current time as a parameter, so tests drive it with synthetic clocks. It
// owns no transport and never performs I/O.
class RolandRequestTracker
{
public:
    enum class MatchOutcome {
        NoOutstandingRequest, // nothing is waiting; data is unsolicited
        NoMatch,              // outstanding requests exist but none covers this data
        Accepted,             // data stored, request still incomplete
        Completed,            // data stored, request fully satisfied
        Rejected,             // data contradicted the request; request failed validation
    };

    struct DataSetMatch
    {
        MatchOutcome outcome = MatchOutcome::NoOutstandingRequest;
        std::optional<RequestId> requestId;
        std::string detail;
    };

    explicit RolandRequestTracker(RequestTimeouts timeouts = {});

    void setTimeouts(RequestTimeouts timeouts) noexcept { m_timeouts = timeouts; }
    [[nodiscard]] RequestTimeouts timeouts() const noexcept { return m_timeouts; }
    void setHistoryLimit(std::size_t limit);

    // Registers a new RQ1. Returns an invalid id when the message is not an RQ1.
    [[nodiscard]] RequestId enqueue(const roland::RolandSysExMessage& request, TimePoint now);

    // RequestSent -> AwaitingData once the bytes left the transport.
    bool markSent(RequestId id, TimePoint now);

    // Feed a decoded DT1. Returns how it was correlated.
    DataSetMatch onDataSet(const roland::RolandSysExMessage& dataSet, TimePoint now);

    // Applies timeouts; returns the ids that transitioned to TimedOut.
    std::vector<RequestId> expire(TimePoint now);

    bool cancel(RequestId id, TimePoint now, std::string reason = "Cancelled");
    std::size_t cancelAll(TimePoint now, std::string reason = "Cancelled");

    // Fails an outstanding request explicitly (e.g. transport send failure).
    bool fail(RequestId id, TimePoint now, std::string reason);

    [[nodiscard]] const RequestOperation* find(RequestId id) const noexcept;
    [[nodiscard]] std::vector<RequestId> outstanding() const;
    [[nodiscard]] bool hasOutstanding() const noexcept;
    [[nodiscard]] const std::deque<RequestOperation>& operations() const noexcept { return m_operations; }

    // Earliest moment at which expire() could change something.
    [[nodiscard]] std::optional<TimePoint> nextDeadline() const;

private:
    RequestOperation* findMutable(RequestId id) noexcept;
    void finish(RequestOperation& op, RequestState state, TimePoint now, std::string reason);
    void trimHistory();
    [[nodiscard]] std::optional<TimePoint> deadlineFor(const RequestOperation& op) const;

    RequestTimeouts m_timeouts;
    std::uint64_t m_nextId = 1;
    std::size_t m_historyLimit = 256;
    std::deque<RequestOperation> m_operations;
};

[[nodiscard]] std::string_view matchOutcomeName(RolandRequestTracker::MatchOutcome outcome) noexcept;

} // namespace xp60studio::protocol
