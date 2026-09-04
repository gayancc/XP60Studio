# XP60Studio Roadmap

The phases below are intentionally ordered. Do not skip foundational phases merely because later features are visually more interesting.

---

# Phase 1 — Protocol Foundation

Goal: establish reliable MIDI and Roland SysEx infrastructure.

Deliverables:

- JUCE application/project foundation
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
- raw diagnostic logging
- deterministic protocol tests

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

Success condition:

Known-good Patch fixtures round-trip without unexplained differences.

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

---

# Phase 4 — Visual Patch Editor

Goal: make Patch editing musically understandable.

Deliverables:

- Current Patch header
- Play / Design / Expert disclosure model
- visual four-Tone architecture
- four-Tone mixer
- Tone Solo/Mute
- waveform browser
- Pitch editor
- TVF editor
- TVA editor
- graphical envelope editors
- LFO editor
- effects editor
- exact-value Expert view
- A/B original/current audition
- safe real-time parameter updates

Do not send excessive MIDI while dragging controls; use throttling/coalescing where needed.

---

# Phase 5 — Librarian

Goal: make SysEx collections searchable and manageable.

Deliverables:

- `.syx` import
- `.syx` export
- individual Patch import/export
- local persistence
- original raw SysEx preservation
- provenance
- search
- tags
- categories
- favourites
- user ratings
- Patch source tracking

The library should remain responsive with thousands of patches.

---

# Phase 6 — Bank Management

Goal: make the 128-slot User Patch bank easy and safe to engineer.

Deliverables:

- 128-slot bank workspace
- drag/drop reorder
- multi-select
- replace/insert/copy/delete
- undo/redo
- duplicate warnings
- compatibility warnings
- source/provenance display
- bank import/export
- bank fetch/send
- transfer pacing controls
- read-back verification where supported
- mismatch inspection/retry

---

# Phase 7 — Expansion Intelligence

Goal: understand whether imported patches are playable on the user's physical XP-60 configuration.

Deliverables:

- EXP-A/B/C/D profile
- SR-JV board metadata
- waveform-to-board mapping
- patch compatibility analysis
- per-Tone compatibility
- missing-board warnings
- explicit replacement/disable/keep workflows

Never silently replace missing waves.

---

# Phase 8 — Complete XP-60 Editing

Goal: reach broad editor/librarian completeness.

Deliverables:

- Performance editor
- visual 16-Part mixer
- Rhythm editor
- supported System data
- snapshots
- restore workflows
- safety snapshot before destructive restore
- complete major XP-60 editor/librarian baseline

At the end of this phase XP60Studio should cover the major practical capabilities expected from a mature XP-60 editor.

---

# Phase 9 — Library Intelligence

Goal: make large libraries understandable.

Deliverables:

- exact parameter fingerprints
- exact duplicate detection
- near-duplicate similarity
- explainable patch diff
- automatic categorization assistance
- Patch DNA
- structural search
- compatibility filters
- source/bank cross-analysis

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

Live Mode must prevent accidental destructive editing.

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
12. retain complete expert access to the XP-60 underneath the simplified UX.
