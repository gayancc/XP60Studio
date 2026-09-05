#include "protocol/RolandRequestTracker.h"

#include <algorithm>

namespace xp60studio::protocol {

RolandRequestTracker::RolandRequestTracker(RequestTimeouts timeouts)
    : m_timeouts(timeouts)
{
}

void RolandRequestTracker::setHistoryLimit(std::size_t limit)
{
    m_historyLimit = std::max<std::size_t>(limit, 1);
    trimHistory();
}

RequestId RolandRequestTracker::enqueue(const roland::RolandSysExMessage& request, TimePoint now)
{
    if (!request.isDataRequest()) {
        return RequestId{};
    }
    RequestOperation op;
    op.id = RequestId{m_nextId++};
    op.request = request;
    op.state = RequestState::RequestSent;
    op.createdAt = now;
    op.lastActivityAt = now;
    op.expectedBytes = request.size().value();
    op.data.assign(op.expectedBytes, 0);
    op.coverage.assign(op.expectedBytes, false);
    m_operations.push_back(std::move(op));
    trimHistory();
    return m_operations.back().id;
}

bool RolandRequestTracker::markSent(RequestId id, TimePoint now)
{
    auto* op = findMutable(id);
    if (!op || op->state != RequestState::RequestSent) {
        return false;
    }
    op->state = RequestState::AwaitingData;
    op->sentAt = now;
    op->lastActivityAt = now;
    return true;
}

RolandRequestTracker::DataSetMatch RolandRequestTracker::onDataSet(const roland::RolandSysExMessage& dataSet,
                                                                   TimePoint now)
{
    DataSetMatch result;
    if (!dataSet.isDataSet()) {
        result.outcome = MatchOutcome::NoMatch;
        result.detail = "message is not a DT1";
        return result;
    }

    bool anyOutstanding = false;
    RequestOperation* match = nullptr;
    for (auto& op : m_operations) {
        if (isTerminal(op.state)) {
            continue;
        }
        anyOutstanding = true;
        if (op.state == RequestState::RequestSent) {
            // Not transmitted yet; the device cannot be answering it.
            continue;
        }
        if (op.request.deviceId() != dataSet.deviceId() || op.request.modelId() != dataSet.modelId()) {
            continue;
        }
        const auto start = op.request.address();
        const auto end = op.request.endAddress();
        if (!end) {
            continue;
        }
        const auto dataStart = dataSet.address();
        const auto dataEnd = dataSet.endAddress();
        if (!dataEnd) {
            continue;
        }
        if (dataStart < start || dataStart >= *end) {
            continue;
        }
        match = &op;
        break; // earliest outstanding request wins
    }

    if (!match) {
        result.outcome = anyOutstanding ? MatchOutcome::NoMatch : MatchOutcome::NoOutstandingRequest;
        result.detail = anyOutstanding ? "no outstanding request covers address " + dataSet.address().toHexString()
                                       : "no request outstanding";
        return result;
    }

    result.requestId = match->id;
    match->lastActivityAt = now;
    ++match->chunkCount;
    if (!match->firstDataAt) {
        match->firstDataAt = now;
    }

    const auto offset = match->request.address().distanceTo(dataSet.address());
    const auto data = dataSet.data();
    if (!offset || *offset + data.size() > match->expectedBytes) {
        const std::string reason = "DT1 at " + dataSet.address().toHexString() + " with " + std::to_string(data.size())
            + " bytes exceeds the requested range (" + std::to_string(match->expectedBytes) + " bytes from "
            + match->request.address().toHexString() + ")";
        finish(*match, RequestState::FailedValidation, now, reason);
        result.outcome = MatchOutcome::Rejected;
        result.detail = reason;
        return result;
    }

    // A chunk starting behind the high-water mark went backwards, which is a
    // real ordering anomaly. A forward jump is not: Roland block addresses are
    // padded, so the gap between two blocks holds no data and never arrives.
    if (*offset < match->coveredThroughBytes) {
        match->sawChunkOutOfOrder = true;
        match->notes.push_back("chunk " + std::to_string(match->chunkCount) + " arrived at offset "
                               + std::to_string(*offset) + ", behind offset "
                               + std::to_string(match->coveredThroughBytes) + " already received");
    }

    std::size_t overlapping = 0;
    for (std::size_t i = 0; i < data.size(); ++i) {
        const std::size_t index = *offset + i;
        if (match->coverage[index]) {
            ++overlapping;
        } else {
            match->coverage[index] = true;
            ++match->receivedBytes;
        }
        match->data[index] = data[i];
    }
    if (overlapping != 0) {
        match->notes.push_back("chunk " + std::to_string(match->chunkCount) + " overlapped "
                               + std::to_string(overlapping) + " already received byte(s)");
    }
    // Narrowing is safe only because the range check above already rejected
    // anything past expectedBytes; clamping keeps that explicit here so
    // reordering the two blocks cannot silently reintroduce a truncating cast.
    const auto chunkEnd = std::min<std::size_t>(*offset + data.size(), match->expectedBytes);
    match->coveredThroughBytes = std::max(match->coveredThroughBytes, static_cast<std::uint32_t>(chunkEnd));

    if (match->isComplete()) {
        finish(*match, RequestState::Completed, now, {});
        result.outcome = MatchOutcome::Completed;
        result.detail = "request complete: " + std::to_string(match->receivedBytes) + " bytes in "
            + std::to_string(match->chunkCount) + " chunk(s)";
    } else {
        match->state = RequestState::Receiving;
        result.outcome = MatchOutcome::Accepted;
        result.detail = std::to_string(match->receivedBytes) + " bytes received, covered through "
            + std::to_string(match->coveredThroughBytes) + " of " + std::to_string(match->expectedBytes)
            + " address units";
    }
    return result;
}

std::vector<RequestId> RolandRequestTracker::expire(TimePoint now)
{
    std::vector<RequestId> expired;
    for (auto& op : m_operations) {
        if (isTerminal(op.state) || op.state == RequestState::RequestSent) {
            continue;
        }
        const auto deadline = deadlineFor(op);
        if (deadline && now >= *deadline) {
            const bool hadData = op.state == RequestState::Receiving;
            const std::string reason = hadData
                ? "No further data after " + std::to_string(op.receivedBytes) + " bytes covering "
                    + std::to_string(op.coveredThroughBytes) + " of " + std::to_string(op.expectedBytes)
                    + " address units (" + std::to_string(m_timeouts.betweenChunks.count()) + " ms between chunks)"
                : "No response within " + std::to_string(m_timeouts.firstResponse.count()) + " ms";
            finish(op, RequestState::TimedOut, now, reason);
            expired.push_back(op.id);
        }
    }
    return expired;
}

bool RolandRequestTracker::cancel(RequestId id, TimePoint now, std::string reason)
{
    auto* op = findMutable(id);
    if (!op || isTerminal(op->state)) {
        return false;
    }
    finish(*op, RequestState::Cancelled, now, std::move(reason));
    return true;
}

std::size_t RolandRequestTracker::cancelAll(TimePoint now, std::string reason)
{
    std::size_t count = 0;
    for (auto& op : m_operations) {
        if (!isTerminal(op.state)) {
            finish(op, RequestState::Cancelled, now, reason);
            ++count;
        }
    }
    return count;
}

bool RolandRequestTracker::fail(RequestId id, TimePoint now, std::string reason)
{
    auto* op = findMutable(id);
    if (!op || isTerminal(op->state)) {
        return false;
    }
    finish(*op, RequestState::FailedValidation, now, std::move(reason));
    return true;
}

const RequestOperation* RolandRequestTracker::find(RequestId id) const noexcept
{
    for (const auto& op : m_operations) {
        if (op.id == id) {
            return &op;
        }
    }
    return nullptr;
}

RequestOperation* RolandRequestTracker::findMutable(RequestId id) noexcept
{
    for (auto& op : m_operations) {
        if (op.id == id) {
            return &op;
        }
    }
    return nullptr;
}

std::vector<RequestId> RolandRequestTracker::outstanding() const
{
    std::vector<RequestId> ids;
    for (const auto& op : m_operations) {
        if (!isTerminal(op.state)) {
            ids.push_back(op.id);
        }
    }
    return ids;
}

bool RolandRequestTracker::hasOutstanding() const noexcept
{
    return std::any_of(m_operations.begin(), m_operations.end(),
                       [](const RequestOperation& op) { return !isTerminal(op.state); });
}

std::optional<TimePoint> RolandRequestTracker::nextDeadline() const
{
    std::optional<TimePoint> next;
    for (const auto& op : m_operations) {
        if (isTerminal(op.state) || op.state == RequestState::RequestSent) {
            continue;
        }
        if (const auto deadline = deadlineFor(op)) {
            if (!next || *deadline < *next) {
                next = deadline;
            }
        }
    }
    return next;
}

std::optional<TimePoint> RolandRequestTracker::deadlineFor(const RequestOperation& op) const
{
    if (op.state == RequestState::AwaitingData && op.sentAt) {
        return *op.sentAt + m_timeouts.firstResponse;
    }
    if (op.state == RequestState::Receiving && op.lastActivityAt) {
        return *op.lastActivityAt + m_timeouts.betweenChunks;
    }
    return std::nullopt;
}

void RolandRequestTracker::finish(RequestOperation& op, RequestState state, TimePoint now, std::string reason)
{
    op.state = state;
    op.finishedAt = now;
    op.lastActivityAt = now;
    op.failureReason = std::move(reason);
}

void RolandRequestTracker::trimHistory()
{
    // Never drop an outstanding operation; drop the oldest finished ones first.
    while (m_operations.size() > m_historyLimit) {
        const auto it = std::find_if(m_operations.begin(), m_operations.end(),
                                     [](const RequestOperation& op) { return isTerminal(op.state); });
        if (it == m_operations.end()) {
            break;
        }
        m_operations.erase(it);
    }
}

std::string_view matchOutcomeName(RolandRequestTracker::MatchOutcome outcome) noexcept
{
    using MatchOutcome = RolandRequestTracker::MatchOutcome;
    switch (outcome) {
    case MatchOutcome::NoOutstandingRequest:
        return "NoOutstandingRequest";
    case MatchOutcome::NoMatch:
        return "NoMatch";
    case MatchOutcome::Accepted:
        return "Accepted";
    case MatchOutcome::Completed:
        return "Completed";
    case MatchOutcome::Rejected:
        return "Rejected";
    }
    return "Unknown";
}

} // namespace xp60studio::protocol
