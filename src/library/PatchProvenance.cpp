#include "library/PatchProvenance.h"

namespace xp60studio::library {

std::string_view patchOriginName(PatchOrigin origin) noexcept
{
    switch (origin) {
    case PatchOrigin::Unknown:
        return "unknown origin";
    case PatchOrigin::ImportedFile:
        return "imported file";
    case PatchOrigin::FetchedFromDevice:
        return "fetched from device";
    case PatchOrigin::CreatedLocally:
        return "created locally";
    }
    return "unknown origin";
}

std::string PatchProvenance::describe() const
{
    std::string out(patchOriginName(origin));
    if (!sourceName.empty()) {
        out += " '" + sourceName + "'";
    }
    if (userNumber) {
        out += " USER:";
        const std::string digits = std::to_string(*userNumber);
        out += std::string(digits.size() < 3 ? 3 - digits.size() : 0, '0') + digits;
    }
    out += " at " + address.toHexString();
    if (deviceId) {
        out += ", device " + std::to_string(deviceId->displayNumber());
    }
    return out;
}

} // namespace xp60studio::library
