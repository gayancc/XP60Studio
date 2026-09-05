#include "services/PatchSyncState.h"

namespace xp60studio::services {

std::string_view studioStateName(StudioState state) noexcept
{
    switch (state) {
    case StudioState::Untracked:
        return "untracked";
    case StudioState::Saved:
        return "saved";
    case StudioState::Edited:
        return "edited";
    }
    return "unknown";
}

std::string_view studioStateLabel(StudioState state) noexcept
{
    switch (state) {
    case StudioState::Untracked:
        // Not "unsaved": a Patch read from the instrument was never in the
        // library, so there is nothing it is a modified version of.
        return "NOT IN LIBRARY";
    case StudioState::Saved:
        return "SAVED";
    case StudioState::Edited:
        return "EDITED";
    }
    return "";
}

std::string_view deviceStateName(DeviceState state) noexcept
{
    switch (state) {
    case DeviceState::Offline:
        return "offline";
    case DeviceState::NotSent:
        return "not-sent";
    case DeviceState::Sending:
        return "sending";
    case DeviceState::Assumed:
        return "assumed";
    case DeviceState::InSync:
        return "in-sync";
    case DeviceState::Diverged:
        return "diverged";
    case DeviceState::Stale:
        return "stale";
    case DeviceState::Failed:
        return "failed";
    }
    return "unknown";
}

std::string_view deviceStateLabel(DeviceState state) noexcept
{
    switch (state) {
    case DeviceState::Offline:
        return "OFFLINE";
    case DeviceState::NotSent:
        return "NOT SENT";
    case DeviceState::Sending:
        return "SENDING";
    case DeviceState::Assumed:
        // Sent, not proved. Kept visibly different from XP TEMP so the one
        // state that claims verification stands alone.
        return "SENT";
    case DeviceState::InSync:
        return "XP TEMP";
    case DeviceState::Diverged:
        return "NOT SENT";
    case DeviceState::Stale:
        return "STALE";
    case DeviceState::Failed:
        return "FAILED";
    }
    return "";
}

std::string_view deviceStateTone(DeviceState state) noexcept
{
    switch (state) {
    case DeviceState::Offline:
    case DeviceState::NotSent:
        return "neutral";
    case DeviceState::Sending:
    case DeviceState::Assumed:
        return "info";
    case DeviceState::InSync:
        return "success";
    case DeviceState::Diverged:
    case DeviceState::Stale:
        return "warning";
    case DeviceState::Failed:
        return "error";
    }
    return "neutral";
}

bool assertsSynchronized(DeviceState state) noexcept
{
    return state == DeviceState::InSync;
}

} // namespace xp60studio::services
