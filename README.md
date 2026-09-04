# XP60Studio

XP60Studio is an XP-60-first sound workstation for the Roland XP/JV family.

The goal is not merely to expose Roland parameters. The application should make the XP-60 substantially easier to understand, edit, organize, preserve, transfer, compare, and perform with while retaining access to the full depth of the synthesizer.

## Approved UI implementation target

![XP60Studio approved UI master mockup](docs/design/xp60studio-ui-master-mockup.jpg)

The master mockup is the authoritative visual target for the four anchor screens: Dashboard, Patch Editor / Four-Tone Mixer, Wave Browser, and Bank Builder / Library Intelligence.

Codex should match these screens as closely as technically possible in layout, hierarchy, component composition, spacing/density, Tone color semantics, panel placement, and overall visual character. Major deviations require a real reason such as verified XP-60 behavior, accessibility, platform constraints, or proven usability issues.

Read:

- [`docs/design/UI_DESIGN_REFERENCE.md`](docs/design/UI_DESIGN_REFERENCE.md)
- [`docs/design/UI_IMPLEMENTATION_ARCHITECTURE.md`](docs/design/UI_IMPLEMENTATION_ARCHITECTURE.md)
- [`docs/design/COMPONENT_CATALOG.md`](docs/design/COMPONENT_CATALOG.md)
- [`docs/design/SCREEN_AND_FEATURE_MAP.md`](docs/design/SCREEN_AND_FEATURE_MAP.md)
- [`docs/design/UI_ACCEPTANCE_CRITERIA.md`](docs/design/UI_ACCEPTANCE_CRITERIA.md)

## Product direction

XP60Studio starts from the functional baseline expected from a mature XP-60 editor/librarian and goes further with:

- complete Patch / Tone editing
- visual four-Tone sound architecture
- reliable Roland SysEx communication with read-back verification
- searchable waveform and expansion-board awareness
- intelligent SysEx library management
- exact and near-duplicate detection based on real parameters
- patch compatibility analysis against installed SR-JV boards
- visual patch comparison and component copy
- non-destructive A/B and version history
- intelligent variation, mutation, and later morphing
- 128-slot bank engineering and smart bank construction
- Performance and Rhythm editing
- safe snapshots and restore
- live setlist / stage workflows
- optional later audio-preview and sound-aware search features

## Technology direction

Primary production stack:

- C++20
- Qt 6.11.x
- Qt Quick / QML
- Qt Quick Controls / Layouts
- custom XP60Studio QML component/design system
- CMake
- libremidi 5.x behind `IMidiTransport` for MIDI I/O

Initial desktop targets:

- Windows
- macOS

Do not introduce JUCE as the application/UI framework unless the architecture is explicitly revised.

The architecture should remain open to future related Roland XP/JV models without compromising XP-60 correctness.

## Repository guide

Start with [`AGENTS.md`](AGENTS.md). Codex and human contributors should use it as the project map.

Detailed project documentation lives in [`docs/`](docs/):

- `PRODUCT_VISION.md` — product goals, UX model, and advanced feature direction
- `FEATURE_BASELINE.md` — mature XP-60 editor baseline and XP60Studio differentiators
- `ARCHITECTURE.md` — technical boundaries and core layers
- `ENGINEERING_PRINCIPLES.md` — correctness, testing, safety, and workflow rules
- `ROADMAP.md` — phased development plan
- `PHASE_1_PROTOCOL_FOUNDATION.md` — first implementation milestone and execution brief
- `design/` — exact UI target, architecture, component catalog, screen map, and acceptance criteria

## Building

Requirements: a C++20 compiler, CMake 3.22+, Ninja (recommended), Qt 6 with the
Quick, Quick Controls 2, Quick Test and Test modules (project baseline 6.11.x;
6.4 is the compile floor), and network access on the first configure so CMake
can fetch the pinned libremidi release (`cmake/Dependencies.cmake`). On Linux
also install the ALSA development headers (`libasound2-dev`).

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/src/app/XP60Studio
```

Headless render of the shell (used for documentation and review):

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
XP60STUDIO_SCREENSHOT=devices.png ./build/src/app/XP60Studio
```

Source layout (lower layers never depend on higher ones):

```text
src/roland        Roland SysEx types and codec           (no Qt)
src/xp60          XP-60 protocol facts + verification status (no Qt)
src/midi          IMidiTransport, SysEx assembler, loopback, libremidi backend
src/protocol      request/response correlation, pacing, timeouts (no Qt)
src/diagnostics   structured protocol log entries (no Qt)
src/xpmodel       XP domain model & codecs: parameter tables (generated from
                  docs/protocol), block codec, Xp60Patch + codec, memory image,
                  .syx stream parsing (no Qt)
src/services      DeviceSession orchestration and PatchTransfer
                  (write -> read back -> compare -> verify) (Qt Core)
src/presentation  view models and Qt item models for QML
src/app           Qt Quick executable
qml/XP60Studio    design tokens, reusable controls, shell, screens
tests/cpp         Qt Test suites          tests/qml  Qt Quick Test suites
tests/fixtures    real Roland SysEx used as golden fixtures
tools/            research utilities (Python; not part of the runtime)
```

## Current status

**Phase 1 — Protocol Foundation + Application Shell** is implemented and
covered by deterministic tests; it awaits validation against a physical XP-60.
See [`docs/PHASE_1_EXIT_REPORT.md`](docs/PHASE_1_EXIT_REPORT.md) for what
exists, what is tested, what is still unknown, and
[`docs/HARDWARE_VALIDATION_XP60.md`](docs/HARDWARE_VALIDATION_XP60.md) for the
hardware procedure. Protocol facts and their verification status are tracked in
[`docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md`](docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md).

**Phase 2 — XP-60 Patch Model** is complete. The Patch Common and Tone tables
are generated from the transcribed Roland Parameter Address Map, the typed
`Xp60Patch` model and codec round-trip byte-for-byte, and the Devices screen
can fetch and decode the XP-60's current Patch. The model is validated against
a real XP-60 user bank (`tests/fixtures/xp60/`): all 128 patches decode with
zero out-of-range values and round-trip byte-exact. See
[`docs/PHASE_2_PATCH_MODEL.md`](docs/PHASE_2_PATCH_MODEL.md) and
[`docs/protocol/XP60_PATCH_PARAMETER_MAP.md`](docs/protocol/XP60_PATCH_PARAMETER_MAP.md).

**Phase 3 — Hardware Round-Trip Validation** has its engine in place: the
Devices screen can write a Patch back to the XP-60's temporary area and verify
it by reading it back and comparing every parameter. Writing is armed
explicitly, targets only the edit buffer, and captures a safety snapshot first.
The hardware session itself is documented in
[`docs/HARDWARE_VALIDATION_XP60.md`](docs/HARDWARE_VALIDATION_XP60.md).

Advanced product screens wait for their backing domain phases, and when
implemented they are built directly against the approved mockup rather than as
temporary generic UI.

Phase 1 was tracked as GitHub Issue #1: **Phase 1 — Roland SysEx protocol foundation**.
