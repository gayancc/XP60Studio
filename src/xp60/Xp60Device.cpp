#include "xp60/Xp60Device.h"

#include <array>

namespace xp60studio::xp60 {

using roland::RolandAddress;
using roland::RolandSize;

namespace {

// ---------------------------------------------------------------------------
// Source notes
//
// "MIDI Implementation" refers to the Roland XP-80/XP-60 owner's manual
// appendix, which the XP-60 shares with the XP-80. The address map below is
// the JV-1080-derived layout used by that family. Sizes of whole regions are
// deliberately left unknown until confirmed by a captured DT1 response.
// ---------------------------------------------------------------------------
constexpr std::string_view kMidiImplementationNote =
    "XP-80/XP-60 MIDI Implementation (owner's manual appendix); confirm against the printed "
    "address chart and a hardware capture before relying on offsets beyond the base address.";

const std::array<MemoryRegion, 6> kRegions{{
    MemoryRegion{
        "system",
        "System",
        "System common / control parameters.",
        RolandAddress{0x00, 0x00, 0x00, 0x00},
        std::nullopt,
        false,
        VerificationStatus::DocumentationDerived,
        kMidiImplementationNote,
    },
    MemoryRegion{
        "temporary-performance",
        "Temporary Performance",
        "Performance edit buffer.",
        RolandAddress{0x01, 0x00, 0x00, 0x00},
        std::nullopt,
        true,
        VerificationStatus::DocumentationDerived,
        kMidiImplementationNote,
    },
    MemoryRegion{
        "temporary-patch-performance-part-1",
        "Temporary Patch (Performance mode, Part 1)",
        "Patch edit buffer for Part 1 while in Performance mode. Parts 2-16 follow at 02 01 00 00 .. "
        "02 0F 00 00 (Part 10 is the temporary Rhythm Set).",
        RolandAddress{0x02, 0x00, 0x00, 0x00},
        std::nullopt,
        true,
        VerificationStatus::DocumentationDerived,
        kMidiImplementationNote,
    },
    MemoryRegion{
        "temporary-patch",
        "Temporary Patch (Patch mode)",
        "Patch edit buffer while in Patch mode. Patch Common starts at the base address and begins with "
        "the 12-character patch name.",
        RolandAddress{0x03, 0x00, 0x00, 0x00},
        std::nullopt,
        true,
        VerificationStatus::DocumentationDerived,
        kMidiImplementationNote,
    },
    MemoryRegion{
        "user-performance",
        "User Performance bank",
        "Permanent User Performances USER:01 .. USER:32, one per 00 01 00 00 stride.",
        RolandAddress{0x10, 0x00, 0x00, 0x00},
        std::nullopt,
        false,
        VerificationStatus::DocumentationDerived,
        kMidiImplementationNote,
    },
    MemoryRegion{
        "user-patch",
        "User Patch bank",
        "Permanent User Patches USER:001 .. USER:128, one per 00 01 00 00 stride.",
        RolandAddress{0x11, 0x00, 0x00, 0x00},
        std::nullopt,
        false,
        VerificationStatus::DocumentationDerived,
        kMidiImplementationNote,
    },
}};

const std::array<SafeReadPreset, 3> kSafeReadPresets{{
    SafeReadPreset{
        "temporary-patch-name",
        "Temporary Patch name (12 bytes)",
        "Reads the 12-character name at the start of the Patch-mode temporary Patch. Small, harmless, and "
        "easy to confirm on the XP-60 display.",
        RolandAddress{0x03, 0x00, 0x00, 0x00},
        RolandSize{0x00, 0x00, 0x00, 0x0C},
        VerificationStatus::DocumentationDerived,
    },
    SafeReadPreset{
        "user-patch-001-name",
        "User Patch USER:001 name (12 bytes)",
        "Reads the name of the first permanent User Patch. Read-only; nothing is written.",
        RolandAddress{0x11, 0x00, 0x00, 0x00},
        RolandSize{0x00, 0x00, 0x00, 0x0C},
        VerificationStatus::DocumentationDerived,
    },
    SafeReadPreset{
        "system-first-16",
        "System area, first 16 bytes",
        "Reads the first 16 bytes of the System area to confirm the device answers at address 00 00 00 00.",
        RolandAddress{0x00, 0x00, 0x00, 0x00},
        RolandSize{0x00, 0x00, 0x00, 0x10},
        VerificationStatus::DocumentationDerived,
    },
}};

const roland::RolandModelId kModelId{0x00, 0x6A};

} // namespace

std::string_view verificationStatusName(VerificationStatus status) noexcept
{
    switch (status) {
    case VerificationStatus::DocumentationDerived:
        return "DocumentationDerived";
    case VerificationStatus::HardwareVerified:
        return "HardwareVerified";
    case VerificationStatus::Unknown:
        return "Unknown";
    }
    return "Unknown";
}

std::string_view verificationStatusLabel(VerificationStatus status) noexcept
{
    switch (status) {
    case VerificationStatus::DocumentationDerived:
        return "Documentation-derived, not yet hardware verified";
    case VerificationStatus::HardwareVerified:
        return "Hardware verified";
    case VerificationStatus::Unknown:
        return "Unknown";
    }
    return "Unknown";
}

const roland::RolandModelId& modelId() noexcept
{
    return kModelId;
}

VerificationStatus modelIdStatus() noexcept
{
    return VerificationStatus::DocumentationDerived;
}

roland::RolandDeviceId factoryDefaultDeviceId() noexcept
{
    return roland::RolandDeviceId::factoryDefault();
}

std::span<const MemoryRegion> knownMemoryRegions() noexcept
{
    return std::span<const MemoryRegion>(kRegions.data(), kRegions.size());
}

const MemoryRegion* findMemoryRegion(std::string_view id) noexcept
{
    for (const auto& region : kRegions) {
        if (region.id == id) {
            return &region;
        }
    }
    return nullptr;
}

std::span<const SafeReadPreset> safeReadPresets() noexcept
{
    return std::span<const SafeReadPreset>(kSafeReadPresets.data(), kSafeReadPresets.size());
}

TransferDefaults transferDefaults() noexcept
{
    return TransferDefaults{};
}

} // namespace xp60studio::xp60
