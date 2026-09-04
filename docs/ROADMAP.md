# XP60Studio Roadmap

The phases below are intentionally ordered. Do not skip foundational phases merely because later features are visually more interesting.

For every phase containing UI work, read and follow:

- `design/xp60studio-ui-master-mockup.jpg`
- `design/UI_DESIGN_REFERENCE.md`
- `design/UI_IMPLEMENTATION_ARCHITECTURE.md`
- `design/COMPONENT_CATALOG.md`
- `design/SCREEN_AND_FEATURE_MAP.md`
- `design/UI_ACCEPTANCE_CRITERIA.md`

The four anchor screens shown in the master mockup must be implemented to closely match that mockup when their roadmap phase begins.

---

# Phase 1 — Protocol Foundation + Application Shell

Goal: establish a trustworthy C++/Qt application foundation, MIDI transport, and Roland SysEx infrastructure.

Deliverables:

- C++20 + CMake project foundation
- Qt 6.11.x application shell
- minimal Qt Quick/QML shell and design tokens sufficient for diagnostics
- explicit C++ presentation/QML boundary
- `IMidiTransport` abstraction
- libremidi 5.x integration behind that abstraction
- MIDI input/output discovery
- open/close MIDI endpoints
- SysEx send/receive
- long SysEx handling
- Roland checksum
- Roland address/size types
- DT1 support
- RQ1 support
- message parsing/serialization
- request/response correlation foundation
- pacing/timeouts/cancellation foundation
- raw/decoded diagnostic logging
- deterministic protocol tests
- minimal Devices/Diagnostics screen only

Do not build the polished Dashboard/Patch Editor/Wave Browser/Bank Builder in Phase 1.

Success condition:

```text
Detect MIDI ports
        ↓
Open MIDI IN/OUT
        ↓
Construct valid Roland SysEx
        ↓
Calculate / verify checksum
        ↓
Send SysEx
        ↓
Receive SysEx
        ↓
Parse messages
        ↓
Correlate expected response
        ↓
Report useful diagnostics
```

See `PHASE_1_PROTOCOL_FOUNDATION.md`.

---

# Phase 2 — XP-60 Patch Model

Goal: decode and encode XP-60 Patch data accurately.

Deliverables:

- Patch Common model
- Tone 1–4 models
- Wave references
- Pitch
- Pitch Envelope
- TVF
- TVF Envelope
- TVA
- TVA Envelope
- LFO
- controller/modulation behavior
- structure settings
- effects
- parameter metadata
- Patch decoder
- Patch encoder
- golden fixture tests
- presentation-model scaffolding only where useful for inspection/testing

Success condition: known-good Patch fixtures round-trip without unexplained differences.

---

# Phase 3 — Physical Hardware Round-Trip Validation

Goal: prove the model and transfer logic against an actual XP-60.

Required flow:

```text
FETCH
  ↓
DECODE
  ↓
ENCODE
  ↓
SEND
  ↓
FETCH AGAIN
  ↓
COMPARE
```

Resolve all unexplained differences before calling the Patch codec mature.

This phase requires clear hardware test scripts when Codex cannot access the physical XP-60 directly.

UI remains diagnostic/inspection focused.

---

# Phase 4 — Visual Patch Editor + Wave Browser

Goal: implement the first polished product surfaces and make Patch editing musically understandable.

Visual targets:

- **M2** — top-right Patch Editor / Four-Tone Mixer in `design/xp60studio-ui-master-mockup.jpg`
- **M3** — bottom-left Wave Browser in the same mockup

Deliverables:

- production XP60Studio design-system foundation
- reusable components from `design/COMPONENT_CATALOG.md`
- Current Patch header
- Play / Design / Expert disclosure model
- visual four-Tone architecture
- four-Tone mixer matching M2
- Tone Solo/Mute
- waveform browser matching M3
- Pitch editor
- TVF editor
- TVA editor
- graphical envelope editors
- LFO editor
- effects editor
- key/velocity range controls
- signal-flow visualization
- exact-value Expert view
- A/B original/current audition foundation
- safe real-time parameter updates
- local/dirty/hardware state visual distinctions
- screenshot review against the approved mockup

Do not send excessive MIDI while dragging controls; use throttling/coalescing.

Acceptance: follow `design/UI_ACCEPTANCE_CRITERIA.md`.

---

# Phase 5 — Librarian + Dashboard

Goal: make SysEx collections searchable/manageable and introduce the polished command center once meaningful data exists.

Visual target:

- **M1** — top-left Dashboard / Command Center in the master mockup
- Library screen inherits M3/M4 design language

Deliverables:

- `.syx` import/export
- individual Patch import/export
- local persistence
- original raw SysEx preservation
- provenance
- search
- tags/categories
- favourites
- user ratings
- Patch source tracking
- virtualized Library result models
- import progress/cancellation
- initial exact-duplicate foundation if appropriate to import workflow
- polished Dashboard matching M1
- current patch hero
- four-Tone contribution
- Library / Bank / Device summary cards
- working quick actions appropriate to implemented features

The library must remain responsive with thousands of patches.

Do not add fake Dashboard actions for future phases.

---

# Phase 6 — Bank Management

Goal: make the 128-slot User Patch bank easy and safe to engineer.

Visual target:

- **M4** — bottom-right Bank Builder / Library Intelligence in the master mockup

Deliverables:

- 128-slot visual bank workspace matching M4
- user-defined section/category rail
- drag/drop reorder
- keyboard/menu alternatives to drag/drop
- multi-select
- replace/insert/copy/delete
- undo/redo
- duplicate warnings where data exists
- compatibility warning placeholders only if backed by verified metadata
- source/provenance display
- mini comparison inspector
- bank import/export
- bank fetch/send
- transfer pacing controls
- read-back verification where supported
- mismatch inspection/retry
- screenshot review against the approved mockup

Do not reduce the primary bank experience to a plain table.

---

# Phase 7 — Expansion Intelligence

Goal: understand whether imported patches are playable on the user's physical XP-60 configuration.

Deliverables:

- EXP-A/B/C/D profile
- Expansion Manager UI using the shared design system
- SR-JV board metadata
- waveform-to-board mapping
- patch compatibility analysis
- per-Tone compatibility
- missing-board warnings
- explicit Find Replacement / Disable Tone / Keep Anyway workflows
- Wave Browser compatibility integration
- Library/Bank compatibility filters and warnings

Never silently replace missing waves.

---

# Phase 8 — Complete XP-60 Editing

Goal: reach broad editor/librarian completeness.

Deliverables:

- Performance editor using the mixer/workstation visual language from `SCREEN_AND_FEATURE_MAP.md`
- visual 16-Part mixer
- Rhythm editor using keyboard/drum-map interaction
- supported System data
- snapshots
- restore workflows
- safety snapshot before destructive restore
- shared transfer/verification UI
- complete major XP-60 editor/librarian baseline

At the end of this phase XP60Studio should cover the major practical capabilities expected from a mature XP-60 editor.

---

# Phase 9 — Library Intelligence + Full Compare

Goal: make large libraries understandable.

Deliverables:

- exact parameter fingerprints
- exact duplicate detection
- near-duplicate similarity
- explainable patch diff
- full Compare screen
- automatic categorization assistance
- Patch DNA
- structural search
- compatibility filters
- source/bank cross-analysis
- Bank Builder analysis summaries
- Dashboard intelligence summaries only where useful

Derived metadata must remain separate from Roland hardware data.

---

# Phase 10 — Advanced Sound Design

Goal: help users create new patches without losing expert control.

Deliverables:

- component copy between patches
- intelligent constrained variation
- mutation/evolution workflow
- morphing where parameter semantics permit
- advanced A/B
- richer version history
- musical transformations such as warmer/wider/brighter where deterministic parameter mappings are justified
- dedicated sound-design surfaces that inherit the Patch Editor design language

Do not interpolate discrete IDs blindly.

---

# Phase 11 — Live Mode

Goal: support stage use.

Deliverables:

- setlists
- songs/sections
- Patch and Performance assignment
- large stage display
- next-sound preview
- safe program switching
- optional MIDI-triggered navigation
- persistent connection status
- destructive editing hidden/disabled in Live Mode

Live Mode must inherit the XP60Studio visual identity while using lower density for stage readability.

---

# Phase 12 — Audio Intelligence

Goal: make library auditioning and search richer without emulating the XP-60 synthesizer.

Potential deliverables:

- capture XP-60 audio output
- associate previews with patches
- standardized preview notes/velocities
- local preview playback
- audio-derived metadata research
- sound-aware search
- similarity based on actual recordings

Do not implement fake audio understanding without real data.

---

# Phase 13 — Additional Roland XP/JV Hardware

Goal: reuse verified abstractions for related devices.

Potential targets:

- XP-50
- XP-80
- XP-30
- JV-1080
- JV-2080

Extract common abstractions based on verified shared behavior.

XP-60 correctness must not be weakened for premature generic support.

---

# Long-Term Success Standard

A mature XP60Studio should let a musician:

1. connect an XP-60 reliably;
2. inspect and edit any major Patch component;
3. understand which Tone contributes what;
4. safely import years of old SysEx banks;
5. detect exact and near duplicates by real parameter content;
6. identify unsupported expansion dependencies;
7. compare and copy patch components visually;
8. create clean custom 128-patch banks;
9. transfer those banks with verifiable results;
10. back up and restore the keyboard confidently;
11. organize live-performance sounds;
12. retain complete expert access to the XP-60 underneath the simplified UX;
13. present the four approved anchor screens with the same composition and design system as the master mockup, except for documented justified deviations.
