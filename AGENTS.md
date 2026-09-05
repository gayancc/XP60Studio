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
- `docs/design/UI_DESIGN_REFERENCE.md` — exact visual implementation target
- `docs/design/UI_IMPLEMENTATION_ARCHITECTURE.md` — authoritative Qt Quick/QML implementation contract
- `docs/design/COMPONENT_CATALOG.md` — reusable XP60Studio component system
- `docs/design/SCREEN_AND_FEATURE_MAP.md` — feature-to-screen mapping and mockup references
- `docs/design/UI_ACCEPTANCE_CRITERIA.md` — visual/interaction acceptance rules and screenshot comparison requirements

## Exact approved UI target

The master visual reference is:

`docs/design/xp60studio-ui-master-mockup.jpg`

The four anchor screens in the master mockup are:

- Dashboard / Command Center
- Patch Editor / Four-Tone Mixer
- Wave Browser
- Bank Builder / Library Intelligence

For these four anchor screens, the mockup is the **authoritative implementation target**, not loose inspiration.

When their roadmap phase begins, reproduce the mockup as closely as technically possible in:

- application shell and navigation
- major panel placement
- layout proportions
- spacing/density
- typography hierarchy
- card/control composition
- Tone 1/2/3/4 color semantics
- signal-flow placement
- envelope-editor placement
- summary/inspector panels
- filter/search arrangement
- bank-grid arrangement
- status/warning hierarchy
- overall premium music-software visual character

Do not replace the approved composition with a generic Qt screen, admin dashboard, property grid, or simplified stock-control interpretation merely because it is easier to implement.

A major deviation from the mockup is allowed only for a documented reason such as:

1. verified XP-60 behavior;
2. accessibility requirements;
3. Windows/macOS platform constraints;
4. responsive-layout necessity at smaller supported widths;
5. illustrative mockup data that is not supported by verified device metadata;
6. a demonstrated usability problem.

For material UI PRs, include implementation screenshots where the development environment permits and compare them against the master mockup and `UI_ACCEPTANCE_CRITERIA.md`.

Do not invent unsupported XP-60 behavior merely to reproduce a decorative detail in the mockup. Hardware truth governs behavior; the mockup governs the valid visual/product composition.

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

## Interaction-first and infographic-first UI rule

Visual fidelity to the master mockup is necessary but **not sufficient**. User-facing UI must also be designed as an interactive musical workspace that explains state visually and makes the next useful action obvious.

The primary surface must be designed from the musician's task and mental model, not from the underlying parameter schema, C++ object structure, MIDI protocol, or database-style field list.

### Required interaction hierarchy

For musical and device concepts that have a meaningful visual representation, prefer direct manipulation and information visualization before plain data-entry controls.

Examples include:

- four-Tone contribution, enable/mute/solo, level, pan and routing
- envelopes and envelope stages
- key and velocity ranges
- LFO shape and modulation behavior
- signal flow and effects routing
- waveform source/category/availability
- bank occupancy, slot state, duplicates and compatibility
- transfer progress, verification and mismatch state
- device connection and communication health

Use interactive controls such as Tone cards, knobs, faders, meters, draggable range handles, envelope points, keyboard strips, signal-flow nodes, filter chips, visual bank slots, timelines, progress states and contextual inspectors where they communicate the concept better than a form field.

### Plain fields are secondary tools

`TextField`, `ComboBox`, `SpinBox`, raw numeric entry and raw technical text must **not** become the default UX merely because they are easy to bind.

When a parameter can be understood and manipulated musically or visually:

1. provide the visual/direct-manipulation control first;
2. show the current value clearly in context;
3. preserve exact numeric/text entry as a secondary precision mechanism;
4. keep raw protocol/address/SysEx details inside Expert or Diagnostics surfaces unless the workflow itself is explicitly diagnostic.

A screen dominated by labels plus text fields, combo boxes, spin boxes or raw hexadecimal values is not considered complete for a polished user-facing phase unless the task is inherently textual/diagnostic and the design justifies it.

### Progressive disclosure is mandatory

Preserve expert depth without forcing every user to see it at once.

Use the product's Play / Design / Expert model where applicable:

- **Play** — immediate musical state, audition-oriented controls and safe high-value actions
- **Design** — visual sound-shaping controls, routing, envelopes, ranges and tone relationships
- **Expert** — exact values, uncommon parameters, raw/technical diagnostics and low-level detail

Do not expose low-level implementation concepts on the default musician-facing surface when a clearer semantic representation exists.

### Infographic-first communication

Do not use decorative charts or meaningless dashboard graphics. Visualization must answer a real question faster than text alone.

Prefer compact visual summaries for questions such as:

- which Tone is contributing most?
- where is the sound flowing?
- what part of the keyboard/velocity range is active?
- which wave is selected and is it available?
- what is modified locally versus verified on hardware?
- how far has a transfer progressed and where did it fail?
- which bank slots are empty, duplicated, incompatible or selected?

Use text to support the visualization, not to replace a visualization that would communicate the state more naturally.

### Interaction quality

Interactive surfaces must provide meaningful feedback for hover, focus, press, selection, drag/drop, disabled, warning, error, busy and completed states where applicable.

Use motion only when it helps explain change, focus, continuity, progress or state transitions. Do not add animation merely for decoration or allow animation to reduce responsiveness.

Visible objects that look interactive should behave interactively when the phase supports that behavior. Do not create decorative fake controls.

### Phase 4 quality gate

For the Visual Patch Editor and Wave Browser phase, a technically wired screen is **not acceptable** if its main interaction model is mostly forms, tables, labels and stock input controls.

Before Phase 4 UI is considered complete, the implementation must visibly use the intended musical interaction components such as:

- FourToneMixer / ToneCard
- XpKnob / XpFader or equivalent continuous musical controls
- EnvelopeEditor
- KeyRangeSelector / VelocityRangeSelector / KeyboardStrip
- signal-flow and effects-routing visualization
- visual LFO shape/preview controls
- WaveSearchBar, filter chips and virtualized WaveResultList
- WaveAvailabilityBadge / ExpansionRequirementBadge
- contextual wave/details/compatibility inspector

Exact entry remains available, but it must not replace the visual interaction model.

### UI review question

For every material user-facing UI change, review the result from the perspective of a musician who does not know the XP-60 MIDI implementation.

The screen should make it possible to understand the important state and the next useful action primarily by scanning the visual hierarchy and interacting with the controls, without having to read paragraphs of explanation, raw addresses, protocol codes or a long property list.

If that standard is not met, the UI requires another design pass even if the underlying functionality works.

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

**Transferring Patches and built banks to the XP-60 — including into its
permanent USER memory — is a core product capability, not a hazard to be
designed around.** A librarian that cannot put a bank on the keyboard is not a
librarian. Nothing in this file may be read as a prohibition on it.

What the rules govern is the *envelope* around a write, never whether it may
happen at all:

- **Explicit.** A write is a deliberate, armed, confirmed act, never a side
  effect of editing, selecting, arranging or connecting.
- **Addressed.** The destination is chosen by the user, never inherited or
  guessed.
- **Backed up.** Whatever a persistent write is about to overwrite is read
  first, so it can be put back.
- **Verified.** The destination is read back and compared. A completed
  transmission is not a verified state.
- **Distinguished.** Temporary memory (`03 00 00 00`, what the instrument
  sounds and discards on the next Patch change) and permanent USER memory
  (`11 nn 00 00`) are different operations with different words in the UI.
  Real-time audition uses the temporary area and must never write USER memory.

The UI must distinguish clearly between:

- LOCAL / NOT IN LIBRARY
- EDITED
- SENT (transmitted, not yet verified)
- XP TEMP (verified in the instrument's temporary area)
- STALE (the instrument's temporary area was replaced behind our back)
- VERIFIED
- MISMATCH / FAILED

See [`docs/PATCH_SYNCHRONIZATION.md`](docs/PATCH_SYNCHRONIZATION.md) for the
memory architecture these states come from and the evidence behind it.

Never silently replace missing expansion waveforms, discard imported patches, or
overwrite hardware state *without the user having asked for it*.

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

Phases 1-6 are implemented: the protocol foundation, the Patch model, the
hardware round trip, the Patch Editor and Wave Browser, the librarian and
Dashboard, and the Bank Builder including `.syx` bank import/export and the
write into the instrument's USER memory. Patch synchronization across the
Editor, Library, Bank Builder and a connected XP-60 is in
[`docs/PATCH_SYNCHRONIZATION.md`](docs/PATCH_SYNCHRONIZATION.md).

Work the next open item in [`docs/ROADMAP.md`](docs/ROADMAP.md) unless the user
directs otherwise. Physical validation for every phase is deferred to one
connected session indexed by
[`docs/DEVICE_ACCEPTANCE.md`](docs/DEVICE_ACCEPTANCE.md); local work continues
meanwhile and no hardware fact is promoted without hardware evidence.

## Product standard

A musician should not need to understand hundreds of Roland parameters merely to answer basic questions such as:

- Which Tone is producing this part of the sound?
- Which waveform is missing?
- Which imported patches actually work with my installed expansions?
- Are these two patches really duplicates?
- Did all 128 patches actually reach the keyboard?
- Can I restore exactly what was on the XP-60 before this change?

The application must preserve expert depth while making these workflows straightforward.

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

When the user types `/graphify`, use the installed graphify skill or instructions before doing anything else.

Rules:
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- Dirty graphify-out/ files are expected after hooks or incremental updates; dirty graph files are not a reason to skip graphify. Only skip graphify if the task is about stale or incorrect graph output, or the user explicitly says not to use it.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
