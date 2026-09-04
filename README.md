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

## Current status

Repository initialized. Development begins with Phase 1: Qt/CMake application shell, MIDI abstraction/transport, and Roland SysEx protocol foundation. Advanced product screens must wait for their backing domain phases, but when implemented they should be built directly against the approved mockup rather than as temporary generic UI.

The first implementation task is tracked as GitHub Issue #1: **Phase 1 — Roland SysEx protocol foundation**.
