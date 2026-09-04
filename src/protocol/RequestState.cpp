#include "protocol/RequestState.h"

namespace xp60studio::protocol {

std::string_view requestStateName(RequestState state) noexcept
{
    switch (state) {
    case RequestState::RequestSent:
        return "RequestSent";
    case RequestState::AwaitingData:
        return "AwaitingData";
    case RequestState::Receiving:
        return "Receiving";
    case RequestState::Completed:
        return "Completed";
    case RequestState::TimedOut:
        return "TimedOut";
    case RequestState::Cancelled:
        return "Cancelled";
    case RequestState::FailedValidation:
        return "FailedValidation";
    }
    return "Unknown";
}

std::string_view requestStateLabel(RequestState state) noexcept
{
    switch (state) {
    case RequestState::RequestSent:
        return "Request sent";
    case RequestState::AwaitingData:
        return "Awaiting data";
    case RequestState::Receiving:
        return "Receiving";
    case RequestState::Completed:
        return "Completed";
    case RequestState::TimedOut:
        return "Timed out";
    case RequestState::Cancelled:
        return "Cancelled";
    case RequestState::FailedValidation:
        return "Failed validation";
    }
    return "Unknown";
}

bool isTerminal(RequestState state) noexcept
{
    switch (state) {
    case RequestState::Completed:
    case RequestState::TimedOut:
    case RequestState::Cancelled:
    case RequestState::FailedValidation:
        return true;
    case RequestState::RequestSent:
    case RequestState::AwaitingData:
    case RequestState::Receiving:
        return false;
    }
    return true;
}

} // namespace xp60studio::protocol
