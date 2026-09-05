#pragma once

// Helpers to assemble a Demo Mode transport + simulated device seed.

#include "midi/LoopbackMidiTransport.h"
#include "simulation/DemoFixture.h"
#include "simulation/SimulatedXp60.h"

#include <memory>

namespace xp60studio::simulation {

inline constexpr const char* kDemoModeSettingsKey = "app/demoMode";

struct DemoTransportBundle
{
    std::unique_ptr<midi::LoopbackMidiTransport> transport;
    midi::LoopbackMidiTransport* raw = nullptr; // non-owning; valid while transport lives in DeviceSession
};

// Creates loopback ports named for Demo Mode.
[[nodiscard]] DemoTransportBundle createDemoTransport();

// Seeds temporary-area memory from the embedded fixture (user patch 4 by default).
[[nodiscard]] SimulatedXp60 createSeededSimulatedXp60(int userPatchNumber = 4);

// True when the user preference, --demo, or XP60STUDIO_DEMO requests Demo Mode.
// Env XP60STUDIO_DEMO=0/false forces live mode even if the preference is on
// (useful for developers and screenshots).
[[nodiscard]] bool isDemoModeRequested(int argc, char* argv[]);

[[nodiscard]] bool demoModePreference();
void setDemoModePreference(bool enabled);

// Persists the preference and relaunches the same binary so transport wiring
// can switch. Returns false if the new process could not be started.
[[nodiscard]] bool relaunchForDemoMode(bool enabled);

} // namespace xp60studio::simulation
