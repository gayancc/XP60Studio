#pragma once

#include <string_view>

namespace xp60studio::protocol {

// Lifecycle of one Roland data request (RQ1) and its DT1 response(s).
enum class RequestState {
    RequestSent,      // queued for / handed to the transport
    AwaitingData,     // transmitted, no DT1 received yet
    Receiving,        // at least one matching DT1 received, range not complete
    Completed,        // requested range fully received and validated
    TimedOut,         // no (further) data within the configured timeout
    Cancelled,        // cancelled by the application or user
    FailedValidation, // data arrived but contradicted the request
};

[[nodiscard]] std::string_view requestStateName(RequestState state) noexcept;
[[nodiscard]] std::string_view requestStateLabel(RequestState state) noexcept; // user-facing
[[nodiscard]] bool isTerminal(RequestState state) noexcept;

} // namespace xp60studio::protocol
