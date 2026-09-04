#pragma once

#include "roland/RolandAddress.h"
#include "roland/RolandDeviceId.h"
#include "roland/RolandModelId.h"
#include "roland/RolandSize.h"

#include <chrono>
#include <optional>
#include <span>
#include <string_view>

// XP-60 specific protocol facts layered on top of the generic Roland codec.
//
// Every fact carries a VerificationStatus. Nothing in this file may be
// promoted to HardwareVerified without an actual capture from a physical
// XP-60; see docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md and
// docs/HARDWARE_VALIDATION_XP60.md.
namespace xp60studio::xp60 {

enum class VerificationStatus {
    // Taken from the official Roland XP-60/XP-80 MIDI Implementation; still to
    // be confirmed by a hardware capture.
    DocumentationDerived,
    // A project choice (for example a partial diagnostic read) that Roland does
    // not document as such. Safe to use, but never presented as a Roland fact.
    ProjectDefined,
    // Observed on a physical XP-60 and recorded in the hardware log.
    HardwareVerified,
    // Not yet established; recorded so the gap is visible instead of guessed.
    Unknown,
};

[[nodiscard]] std::string_view verificationStatusName(VerificationStatus status) noexcept;
[[nodiscard]] std::string_view verificationStatusLabel(VerificationStatus status) noexcept;

// Model ID used by the XP-60/XP-80 exclusive implementation: 6AH (one byte,
// shared with the JV-1080 family). See ROLAND_XP60_PROTOCOL_FACTS.md.
[[nodiscard]] const roland::RolandModelId& modelId() noexcept;
[[nodiscard]] VerificationStatus modelIdStatus() noexcept;

// Factory-default device ID (displayed as 17, transmitted as 10H).
[[nodiscard]] roland::RolandDeviceId factoryDefaultDeviceId() noexcept;

// A named region of the XP-60 memory map.
struct MemoryRegion
{
    std::string_view id;
    std::string_view name;
    std::string_view description;
    roland::RolandAddress baseAddress;
    // Total size when known; nullopt when it still needs to be established.
    std::optional<roland::RolandSize> size;
    bool temporaryMemory; // true: edit buffer (lost on power-off), false: permanent User memory
    VerificationStatus status;
    std::string_view sourceNote;
};

[[nodiscard]] std::span<const MemoryRegion> knownMemoryRegions() noexcept;
[[nodiscard]] const MemoryRegion* findMemoryRegion(std::string_view id) noexcept;

// A read request that is safe to issue against an XP-60 because RQ1 never
// modifies device memory. Used by the Devices/Diagnostics screen.
struct SafeReadPreset
{
    std::string_view id;
    std::string_view name;
    std::string_view description;
    roland::RolandAddress address;
    roland::RolandSize size;
    VerificationStatus status;
};

[[nodiscard]] std::span<const SafeReadPreset> safeReadPresets() noexcept;

// Transfer pacing defaults for the XP-60.
struct TransferDefaults
{
    // Minimum gap between consecutive exclusive messages sent to the device.
    std::chrono::milliseconds interMessageDelay{20};
    // Largest DT1 data payload sent in one message. The MIDI Implementation
    // requires data longer than 128 bytes to be split into packets of 128
    // bytes or less.
    std::size_t maxDataSetPayloadBytes{128};
    // How long to wait for the first DT1 after an RQ1.
    std::chrono::milliseconds firstResponseTimeout{1500};
    // How long to wait between DT1 chunks of one response.
    std::chrono::milliseconds betweenChunkTimeout{1000};
    VerificationStatus status{VerificationStatus::DocumentationDerived};
};

[[nodiscard]] TransferDefaults transferDefaults() noexcept;

} // namespace xp60studio::xp60
