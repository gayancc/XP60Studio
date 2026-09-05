#pragma once

// Seed data helpers for Demo Mode. Prefer the embedded Qt resource so the
// shipping app does not need XP60STUDIO_FIXTURE_DIR.

#include "roland/RolandAddress.h"
#include "xpmodel/MemoryImage.h"
#include "xpmodel/Xp60Patch.h"

namespace xp60studio::simulation {

inline constexpr const char* kDemoInputId = "demo-in";
inline constexpr const char* kDemoOutputId = "demo-out";
inline constexpr const char* kDemoInputName = "XP-60 IN (Demo)";
inline constexpr const char* kDemoOutputName = "XP-60 OUT (Demo)";

// Loads the embedded user-bank fixture. Returns an empty image on failure.
[[nodiscard]] xpmodel::MemoryImage loadEmbeddedFixtureBank();

// Places user patch `userPatchNumber` into the temporary Patch area.
[[nodiscard]] xpmodel::MemoryImage temporaryAreaFromFixture(int userPatchNumber);

[[nodiscard]] roland::RolandAddress temporaryPatchAddress();

[[nodiscard]] xpmodel::Xp60Patch patchFrom(const xpmodel::MemoryImage& image, const roland::RolandAddress& base);

} // namespace xp60studio::simulation
