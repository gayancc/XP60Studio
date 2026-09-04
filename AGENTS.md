# XP60Studio Agent Guide

This file is the root instruction map for Codex and other coding agents working in this repository.

## Mission

Build a modern, professional Roland XP-60 sound workstation: editor, librarian, bank-management system, sound-design environment, backup/restore utility, and live-performance tool.

Patch Base is a functional baseline for the expected capabilities of an XP-60 editor. Do not copy Patch Base source code, assets, proprietary implementation, or UI. XP60Studio must be an original product with stronger usability, transfer reliability, library intelligence, sound analysis, and bank engineering.

The initial hardware target is the Roland XP-60. Related XP/JV models are future targets only after XP-60 behavior is proven.

## Required reading before implementation

Read the documents relevant to the task before changing code:

- `docs/PRODUCT_VISION.md` — what the product should become
- `docs/FEATURE_BASELINE.md` — mature editor baseline and XP60Studio differentiators
- `docs/ARCHITECTURE.md` — authoritative technical layers and dependency boundaries
- `docs/ENGINEERING_PRINCIPLES.md` — correctness, testing, data safety, and implementation rules
- `docs/ROADMAP.md` — ordered development phases
- `docs/PHASE_1_PROTOCOL_FOUNDATION.md` — current first milestone
- `docs/design/UI_DESIGN_REFERENCE.md` — approved visual direction
- `docs/design/UI_IMPLEMENTATION_ARCHITECTURE.md` — authoritative Qt Quick/QML implementation contract
- `docs/design/COMPONENT_CATALOG.md` — reusable XP60Studio component system
- `docs/design/SCREEN_AND_FEATURE_MAP.md` — feature-to-screen mapping and mockup references

## Approved visual direction

The master visual reference is:

`docs/design/xp60studio-ui-master-mockup.jpg`

The four anchor screens in the master mockup are:

- Dashboard / Command Center
- Patch Editor / Four-Tone Mixer
- Wave Browser
- Bank Builder / Library Intelligence

When implementing any UI, use the master mockup together with `UI_DESIGN_REFERENCE.md`, `UI_IMPLEMENTATION_ARCHITECTURE.md`, `COMPONENT_CATALOG.md`, and `SCREEN_AND_FEATURE_MAP.md`.

The mockup is a design direction, not permission to invent unsupported XP-60 behavior. Hardware truth, accessibility, platform constraints, and validated usability take precedence while preserving the approved visual system.

## Authoritative technology direction

Production application:

- **C++20**
- **Qt 6.11.x** current project baseline
- **Qt Quick / QML** for application UI
- **Qt Quick Controls** and **Qt Quick Layouts** as UI foundations
- custom XP60Studio QML design/component library for the actual visual language
- **CMake** build system
- **libremidi 5.x** as the default MIDI implementation behind an internal `IMidiTransport` abstraction; pin an exact compatible release/commit during integration

Do **not** introduce JUCE as the application/UI framework. Do not introduce Qt Widgets, Electron/WebView, Dear ImGui, React, or another primary UI stack unless the architecture is explicitly revised.

Python may be used for research utilities, SysEx analysis, fixture generation, and bulk-data investigation, but it must not become the core production runtime.

Initial targets are Windows and macOS. Do not introduce platform assumptions unnecessarily.

## Non-negotiable architectural rule

The UI must never construct Roland SysEx directly.

Keep these concerns separated:

1. QML screens and XP60Studio UI components
2. C++ presentation/view-model layer
3. application services / orchestration
4. library intelligence and persistence
5. XP domain model and codecs
6. Roland SysEx protocol
7. MIDI transport abstraction
8. libremidi/platform backend

QML must not know Roland addresses, checksum bytes, or libremidi APIs. Hardware/protocol code must remain independent from visual controls.

## UI implementation rule

Do not build each screen as a one-off collection of styled rectangles and controls.

Reuse the component system defined in `docs/design/COMPONENT_CATALOG.md` and shared Theme/Typography/Metrics/Motion tokens.

Before creating a major new screen, consult `docs/design/SCREEN_AND_FEATURE_MAP.md`. Prefer existing screens, inspectors, drawers, and reusable components over inventing new navigation destinations.

The application must not degrade into a generic CRUD/admin UI merely because stock controls are easier to implement.

## Protocol-first rule

Do not build elaborate editor UI before the Roland SysEx foundation and XP-60 data model are trustworthy.

The required confidence loop is:

`XP-60 -> fetch -> decode -> model -> encode -> send -> fetch again -> compare`

Every unexplained difference must be investigated. Never hide a mismatch simply to make a test pass.

## Source-of-truth order for hardware behavior

Prefer, in order:

1. official Roland XP-60/XP-80 MIDI/SysEx documentation
2. official Roland manuals
3. verified hardware captures
4. known-good supplied SysEx files
5. experimentally verified round trips against a physical XP-60
6. carefully evaluated secondary references

Never guess addresses or parameter interpretations and then spread the guess through the codebase. Represent unknowns explicitly until verified.

## Testing rule

When a test fails, determine whether the implementation, fixture, assumption, model, or test is wrong before changing production logic.

Important deterministic test areas include:

- Roland checksum
- address arithmetic
- message serialization/parsing
- RQ1 / DT1 construction
- parameter conversions
- range validation
- patch encode/decode
- round-trip integrity
- corrupted/truncated SysEx handling
- duplicate fingerprints
- compatibility analysis
- presentation-model state transitions
- local vs hardware/verified/dirty UI states

## User-data safety

Local editing and experimentation should be non-destructive by default.

Writing to the XP-60 must always be explicit. The UI must distinguish clearly between:

- LOCAL
- ON XP-60
- MODIFIED / UNSAVED
- VERIFIED
- MISMATCH / FAILED

Never silently replace missing expansion waveforms, discard imported patches, or overwrite hardware state.

## Implementation behavior

For every phase:

- inspect existing implementation before changing it
- read relevant project/design docs first
- reuse sound abstractions and UI components instead of duplicating logic
- avoid speculative generic architecture
- keep protocol facts traceable
- build frequently
- run relevant tests
- investigate failures properly
- do not present placeholders as finished functionality
- keep long-running import/analysis/transfer work off the UI thread
- provide cancellation/progress for meaningful background operations
- keep QML primarily presentation/interaction focused; business/domain logic belongs in C++
- expose large datasets through Qt models rather than materializing thousands of QML objects

When physical hardware verification is required, state exactly what must be tested and what observation is needed. Never claim hardware verification without hardware evidence.

## Current priority

Start with **Phase 1 — Protocol Foundation** unless the user explicitly directs otherwise.

Phase 1 should establish the Qt/CMake application shell, C++/QML boundary, minimal diagnostics UI, MIDI abstraction/implementation, and Roland protocol foundation. It is not the phase for the polished Dashboard, Patch Editor, Bank Builder, Patch DNA, morphing, smart-bank generation, or setlists.

## Product standard

A musician should not need to understand hundreds of Roland parameters merely to answer basic questions such as:

- Which Tone is producing this part of the sound?
- Which waveform is missing?
- Which imported patches actually work with my installed expansions?
- Are these two patches really duplicates?
- Did all 128 patches actually reach the keyboard?
- Can I restore exactly what was on the XP-60 before this change?

The application must preserve expert depth while making these workflows straightforward.
