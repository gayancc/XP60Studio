#include "xp60/Xp60Device.h"

#include <array>

namespace xp60studio::xp60 {

using roland::RolandAddress;
using roland::RolandSize;

namespace {

// ---------------------------------------------------------------------------
// Source notes
//
// "MIDI Implementation" refers to the Roland XP-60/XP-80 owner's manual
// appendix, which the XP-60 shares with the XP-80. The address map below was
// cross-checked against that appendix during the PR #5 review. Sizes of whole
// regions are deliberately left unknown until confirmed by a captured DT1
// response, except where Roland publishes the size in a worked example.
// ---------------------------------------------------------------------------
constexpr std::string_view kMidiImplementationNote =
    "XP-60/XP-80 MIDI Implementation (owner's manual appendix); confirm with a hardware capture "
    "before relying on offsets beyond the base address.";

const std::array<MemoryRegion, 8> kRegions{{
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
        "02 0F 00 00; Part 10 (02 09 00 00) holds the Temporary Rhythm Setup instead of a Patch.",
        RolandAddress{0x02, 0x00, 0x00, 0x00},
        std::nullopt,
        true,
        VerificationStatus::DocumentationDerived,
        kMidiImplementationNote,
    },
    MemoryRegion{
        "temporary-rhythm-setup",
        "Temporary Rhythm Setup",
        "Rhythm Setup edit buffer (Performance mode Part 10).",
        RolandAddress{0x02, 0x09, 0x00, 0x00},
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
        "user-rhythm-setup",
        "User Rhythm Setup",
        "Permanent User Rhythm Setups USER:1 (10 40 00 00) and USER:2 (10 41 00 00).",
        RolandAddress{0x10, 0x40, 0x00, 0x00},
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

const std::array<SafeReadPreset, 4> kSafeReadPresets{{
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
        "temporary-performance-roland-example",
        "Temporary Performance (Roland RQ1 example, 3993 bytes)",
        "Roland's published RQ1 example: address 01 00 00 00, size 00 00 1F 19. The reply must arrive as "
        "several DT1 packets of at most 128 bytes, at least 20 ms apart, covering the whole range. Read-only.",
        RolandAddress{0x01, 0x00, 0x00, 0x00},
        RolandSize{0x00, 0x00, 0x1F, 0x19},
        VerificationStatus::DocumentationDerived,
    },
    SafeReadPreset{
        "system-first-16",
        "System area, first 16 bytes (project-defined)",
        "Reads the first 16 bytes of the System area to confirm the device answers at address 00 00 00 00. "
        "The base address is documented; the 16-byte partial size is a project choice, not a Roland block.",
        RolandAddress{0x00, 0x00, 0x00, 0x00},
        RolandSize{0x00, 0x00, 0x00, 0x10},
        VerificationStatus::ProjectDefined,
    },
}};

// Roland XP-60/XP-80 MIDI Implementation: Model ID = 6AH. The published RQ1
// example is F0 41 10 6A 11 01 00 00 00 00 00 1F 19 47 F7 and the published
// DT1 example is F0 41 10 6A 12 01 00 00 28 06 51 F7; both are test fixtures.
const roland::RolandModelId kModelId{0x6A};

} // namespace

std::string_view verificationStatusName(VerificationStatus status) noexcept
{
    switch (status) {
    case VerificationStatus::DocumentationDerived:
        return "DocumentationDerived";
    case VerificationStatus::ProjectDefined:
        return "ProjectDefined";
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
    case VerificationStatus::ProjectDefined:
        return "Project-defined diagnostic read, not a Roland transfer block";
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
