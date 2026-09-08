# XP60Studio Screen and Feature Map

This document maps product functions to concrete UI surfaces so Codex does not invent unrelated screens or component patterns while implementing features.

Primary visual reference: [`xp60studio-ui-master-mockup.jpg`](xp60studio-ui-master-mockup.jpg)

Implementation contract: [`UI_IMPLEMENTATION_ARCHITECTURE.md`](UI_IMPLEMENTATION_ARCHITECTURE.md)

Reusable components: [`COMPONENT_CATALOG.md`](COMPONENT_CATALOG.md)

The master mockup contains four approved anchor screens:

- **M1 — Dashboard / Command Center** — top-left
- **M2 — Patch Editor / Four-Tone Mixer** — top-right
- **M3 — Wave Browser** — bottom-left
- **M4 — Bank Builder / Library Intelligence** — bottom-right

Screens not pictured in the master mockup must inherit the same shell, dark design system, density, component language, connection-state treatment, and musician-first hierarchy.

---

# Global application shell

| Function | UI surface | Mockup | Required components / behavior |
|---|---|---|---|
| Primary navigation | Persistent left navigation rail | M1–M4 | `AppNavigationRail`; Dashboard, Library, Editor, Banks, Performance, Compare, Devices, Settings |
| XP-60 connection state | Persistent global header | M1–M4 | `ConnectionStatusIndicator`; Connected/Disconnected/Reconnecting/Error; never color-only |
| Current device | Header / Devices context | M1–M4 | Device name, MIDI endpoints when expanded, device ID, health |
| Background operations | Global operation tray | Design-system extension | Transfers/imports/analysis continue visibly without blocking navigation |
| Notifications | Toast + operation details | Design-system extension | Human-readable success/warning/error; never replace detailed transfer result |
| Local vs hardware state | Patch/bank status badges | M2/M4 extension | LOCAL, ON XP-60, MODIFIED/UNSAVED, VERIFIED, MISMATCH |

---

# Dashboard / Command Center

Visual anchor: **M1, top-left of the master mockup**.

| Function | Placement | Interaction |
|---|---|---|
| Current Patch identity | Main hero card | Name, bank/slot, category/tags, hardware/local state |
| Four-Tone contribution | Main hero lower area | Tone 1–4 meters; click Tone to open Editor focused on it |
| Edit Patch | Quick action | Navigate to Patch Editor preserving selected patch |
| Compare | Quick action | Open Compare with current patch preselected as A |
| Save As | Quick action | Non-destructive local save/copy workflow |
| Add to Bank | Quick action | Choose bank/slot without leaving current context when possible |
| Library summary | Right card | Patch count, relevant status, open Library |
| Bank Builder summary | Right card | Current bank occupancy/warnings, open Banks |
| Device diagnostics summary | Right card | Connection/health summary, open Devices |
| Recent work | Later optional panel | Recently edited/imported patches; do not turn Dashboard into KPI clutter |

Dashboard should remain a command center, not an analytics dashboard.

---

# Patch Editor / Four-Tone Mixer

Visual anchor: **M2, top-right of the master mockup**.

## Patch-level functions

| Function | UI surface | Components |
|---|---|---|
| Patch identity | Editor header | `PatchHeaderCard`, bank/slot/name, dirty/hardware badges |
| Play / Design / Expert | Editor disclosure control | `XpSegmentedControl` or contextual tabs; same patch model underneath |
| A/B Original vs Current | Header action | `OriginalCurrentToggle`; instant non-destructive audition |
| Undo/Redo | Header action | Standard commands, disabled when unavailable |
| Write to XP-60 | Explicit primary hardware action | Must indicate target memory and confirmation semantics |
| Save local version | Header/menu | Does not imply write to hardware |

## Four-Tone functions

| Function | UI surface | Components |
|---|---|---|
| Tone enable | Each Tone card | `ToneEnableControl` |
| Wave selection | Each Tone card | `ToneWaveSelector`, opens Wave Browser contextual picker |
| Level | Each Tone card | `ToneLevelControl` / `XpFader` or knob |
| Pan | Each Tone card | `TonePanControl` |
| Octave/coarse tuning | Each Tone card | `ToneOctaveControl`, exact value entry available |
| Solo | Each Tone card | Temporary non-destructive audition |
| Mute | Each Tone card | Temporary non-destructive audition |
| Tone contribution | Card meter/mini visual | `ToneContributionMeter` |
| Tone mini-envelope | Card preview | Click to open relevant detailed envelope section |
| Tone copy/paste | Tone context menu / toolbar | Copy entire Tone without forcing full patch duplicate |

## Sound-design tabs/panels

| Function | UI surface | Notes |
|---|---|---|
| Wave | Sound section | Searchable source-aware picker, not raw IDs only |
| Pitch | Sound/Pitch panel | Coarse/fine, pitch envelope, key follow where supported |
| TVF | Filter tab | Filter type, cutoff, resonance, envelope, key/velocity behavior |
| TVA | Amp tab | Level, pan, envelope, velocity/key behavior |
| LFO | Motion tab | Shape/rate/delay/fade/mod targets with visual preview where useful |
| Controllers/modulation | Motion/Expert | Semantic assignments with exact XP values available |
| Structure | Signal-flow strip | Visual structure selection; only verified XP structures |
| MFX | Effects tab + signal flow | `EffectBlock`, parameters, routing |
| Chorus | Effects tab + signal flow | `EffectBlock` |
| Reverb | Effects tab + signal flow | `EffectBlock` |
| Key range | Tone detail panel | `KeyRangeSelector` on keyboard strip |
| Velocity range | Tone detail panel | `VelocityRangeSelector` |
| Portamento / bend / misc Tone settings | Context/detail panel | Compact grouped controls; not a giant property sheet |

## Envelope editing

Use `EnvelopeEditor` for Pitch, TVF and TVA envelopes.

Required behavior:

- draggable points
- keyboard-accessible adjustment where practical
- exact numeric value fields
- visible stage labels
- live local visual updates
- throttled/coalesced hardware writes
- reset/copy/paste where supported

---

# Wave Browser

Visual anchor: **M3, bottom-left of the master mockup**.

| Function | UI surface | Behavior |
|---|---|---|
| Search waveforms | Top search field | Fast incremental search |
| Filter by source | Filter chip row | Internal + installed/known SR-JV boards |
| Filter by category | Filter chip row | Strings, Piano, Brass, Winds, Synth, Vocal, FX, etc. |
| Availability | Result row badge | Available / Missing Expansion / Unknown Mapping |
| Wave source/slot | Result row | Human-readable board/source plus technical ID in details |
| Preview/select | Result row action | Context depends on hardware capability; selection does not hide compatibility warnings |
| Wave details | Right inspector | Source, number, category, loop/key metadata where verified |
| Compatibility | Right inspector | Current XP-60 expansion profile |
| Contextual picker mode | Opened from Tone | Selection returns wave to requesting Tone while preserving editor state |

Never silently substitute an unavailable expansion waveform.

---

# Library

The Library inherits visual language from M3/M4 but is a dedicated screen.

| Function | UI surface | Components / behavior |
|---|---|---|
| Import `.syx` | Toolbar/drop zone | `ImportDropZone`, import progress, no mutation of originals |
| Export `.syx` | Selection action | Clear scope: patch/bank/selected items |
| Search patches | Main search | Name/tags/source/metadata |
| Filter patches | Filter drawer/chips | Category, source, expansion, active Tones, compatibility, favourites, rating, duplicate state |
| Patch results | Virtualized list/grid | `PatchResultList` / card view option later |
| Favourite | Row/card action | Immediate local metadata update |
| Rating | Inspector/card | Local metadata only |
| Tags/categories | Inspector/editor | Multi-tag capable |
| Provenance | Inspector | Original file/bank/source preserved |
| Patch DNA | Inspector/summary | Clearly labeled derived metadata |
| Exact duplicate state | Badge/filter | Based on canonical parameter fingerprint |
| Near duplicate state | Badge/filter | Explainable similarity, open Compare |
| Unsupported/corrupt | Warning states | Preserve data; explain parsing/compatibility issue |
| Bulk analysis | Operation panel | Progress/cancel; totals unique/duplicate/unsupported |

---

# Bank Builder / User Bank Management

Visual anchor: **M4, bottom-right of the master mockup**.

Implemented as the Virtual XP-60 patch panel — SUBGROUP / BANK / NUMBER over
the linear 001-128 identity. See [BANK_BUILDER.md](BANK_BUILDER.md) for the
mental model, the drag rules, the component list and the screenshots.

| Function | UI surface | Behavior |
|---|---|---|
| Select User bank/work bank | Header selector | Distinguish local bank from keyboard User bank |
| 128-slot visualization | Main `BankGrid` | Slots numbered and readable, not plain spreadsheet rows |
| User-defined sections | Left category rail | Piano/Strings/etc. are examples, not hardcoded |
| Drag/reorder | Grid | Clear ghost/drop semantics; keyboard/menu alternative |
| Insert/replace | Grid/context actions | Explicit outcome before mutation |
| Multi-select | Grid | Batch move/tag/delete from local bank |
| Empty slots | Slot state | Clearly visible |
| Duplicate warnings | Slot + summary | Exact/near duplicate badges |
| Expansion warnings | Slot + warning panel | Missing board/wave clearly shown |
| Provenance | Inspector | Source file/bank/patch |
| Patch compare | Right inspector | Mini comparison with route to full Compare |
| Import bank/patches | Toolbar | Creates/updates local working bank first |
| Export bank | Toolbar | Export local working state |
| Fetch from XP-60 | Hardware action | Read operation with progress |
| Send to XP-60 | Hardware action | Explicit permanent-memory action |
| Transfer verification | Operation panel | Per-slot Sending/ReadBack/Compare/Verified/Mismatch |
| Retry mismatch | Per-item action | Retry only failed slot where safe |
| Keep keyboard version | Mismatch resolution | Pull read-back into local working state |
| Send again | Mismatch resolution | Explicit retry |
| Smart bank proposal | Later side panel/workflow | Proposal remains editable before replacing any working bank |

---

# Compare

| Function | UI surface | Behavior |
|---|---|---|
| Select Patch A/B | Compare header | Search/library/current patch sources |
| Similarity summary | Header | Explainable score/summary |
| Tone comparison | Four-Tone comparison strip | Consistent Tone colors |
| Parameter differences | Grouped difference list | Unchanged values hidden by default |
| Wave differences | Difference group | Include expansion/source meaning |
| Envelope differences | Mini curves + values | Allow focused inspection |
| Effects differences | Difference group | Structure/MFX/Chorus/Reverb |
| Copy component A->B/B->A | Explicit action | Tone/envelope/LFO/effects only when semantically safe |
| Open in Editor | Action | Preserve chosen target patch and comparison context |

---

# Expansion Manager

| Function | UI surface | Behavior |
|---|---|---|
| Configure EXP-A/B/C/D | Four `ExpansionSlotCard`s | Board picker/empty state |
| Installed profile | Summary | This is user/device profile metadata, not inferred from patch files |
| Compatibility scan | Action | Recompute library/bank compatibility |
| Missing expansion patch list | Results panel | Open affected patch/Tone |
| Resolve missing wave | Context workflow | Find Replacement / Disable Tone / Keep Anyway; never silent |

---

# Devices / Connection / Diagnostics

Phase 1 begins with a minimal version; later it inherits the full design system.

| Function | UI surface | Behavior |
|---|---|---|
| MIDI Input | Endpoint picker | Separate input/output support |
| MIDI Output | Endpoint picker | Separate input/output support |
| Connect/Disconnect | Device card | Obvious state transition |
| Device ID | Device settings | Exact value with validation |
| XP-60 handshake/read test | Health action | Reports actual result, not endpoint presence only |
| SysEx health | Health card | Working/failing/unknown |
| Round-trip/latency diagnostic | Diagnostic action | Measured result when implemented |
| Raw/decoded activity | Protocol log | Developer/expert panel |
| Manual safe RQ1 | Phase 1 expert test | Explicit address/size validation |
| Manual DT1 | Expert only | Disabled until safe target semantics are verified |
| Timeouts/checksum/retries | Metrics/details | Human summary + raw details |

---

# Transfer workflows

Transfers may appear from Editor, Banks, Snapshot/Restore, Performance, Rhythm, and Devices, but use one shared operation model and UI.

Required states:

```text
Queued
Sending
Waiting
Receiving
Read Back
Comparing
Verified
Mismatch
Failed
Cancelled
```

Use `TransferProgressPanel` and `GlobalOperationTray`; do not build unrelated progress dialogs for each feature.

---

# Performance Editor

Not shown directly in the master mockup; inherit M2's mixer/signal-flow language.

| Function | UI surface | Behavior |
|---|---|---|
| Performance identity | Header | Name/slot/local-hardware state |
| 16 Parts | `PartMixer` | Mixer/workstation layout |
| Patch per Part | Part strip | Search/select from valid sources |
| MIDI channel | Part strip/detail | Clear channel label |
| Level/Pan | Part strip | Fast mixer controls |
| Transpose | Part detail | Exact values |
| Key range | Part detail | Keyboard range selector |
| Velocity range | Part detail | Range selector |
| Effects routing | Mixer/routing panel | Visual relationships |
| Mute/Solo | Part strip | Audition behavior with verified semantics |
| Expert Part parameters | Inspector | Progressive disclosure |

---

# Rhythm Editor

Not shown directly; inherit M2/M3 control language.

| Function | UI surface | Behavior |
|---|---|---|
| Drum key map | `RhythmKeyboardMap` | Keyboard/drum-map primary view |
| Select drum key | Key click/keyboard | Drives one inspector |
| Wave | Inspector | Searchable Wave Browser integration |
| Pitch/level/pan | Inspector | Music controls + exact values |
| Filter/amp/effects | Inspector tabs | Reuse existing parameter controls where meaningful |
| Copy drum tone | Context action | Explicit source/target key |

---

# Snapshot / Backup / Restore

| Function | UI surface | Behavior |
|---|---|---|
| Create snapshot | Snapshot screen/action | Coverage summary before capture |
| Capture progress | Shared operation UI | Patches/Performances/Rhythm/System where verified |
| Snapshot contents | `SnapshotCard`/details | Distinguish captured raw data and decoded data |
| Restore | Restore plan | Show exactly what will be written |
| Safety snapshot | Required pre-restore option | Strongly recommended/default when feasible |
| Restore progress | Transfer UI | Per-region progress |
| Restore verification | Summary | Verified/mismatch/unsupported regions |

---

# Version History / A-B

| Function | UI surface | Behavior |
|---|---|---|
| Original vs current | Patch Editor | Instant A/B |
| Local versions | Editor history drawer | Timeline with meaningful change summaries |
| Restore local version | History action | Non-destructive until user confirms replacement of working state |
| Hardware written marker | Timeline/badge | Clearly distinguish local version from XP-written state |

---

# Intelligent Variation / Mutation / Morphing

Later-phase screens inherit Patch Editor visual language.

| Function | UI surface | Behavior |
|---|---|---|
| Variation amount | Sound Design panel | Bounded control |
| Lock waves/effects/etc. | `VariationLocks` | Explicit constraints |
| Generate candidates | Candidate grid | Non-destructive children |
| Audition candidate | Candidate card | Temporary audition |
| Adopt candidate | Action | Creates new working version |
| Mutation tree | `MutationTree` | Parent/children relationships |
| Morph A/B | `MorphControl` | Only semantically continuous parameters interpolate |
| Capture morph state | Action | Saves intermediate state as patch/version |

---

# Live Mode / Setlists

Not shown in master mockup; use the same brand but intentionally lower visual density.

| Function | UI surface | Behavior |
|---|---|---|
| Setlist | Setlist editor | Songs/sections ordering |
| Assign Patch/Performance | Song/section inspector | Library picker |
| Stage current sound | Large stage card | High readability |
| Next sound | Next preview | Always visible where practical |
| Navigate next/previous | Large actions + keyboard/MIDI | Safe under performance conditions |
| Notes | Optional stage info | Low-distraction |
| Connection status | Persistent | Never hide device disconnect during performance |
| Destructive editing | Hidden/disabled | Live Mode is performance-safe |

---

# Settings

Settings should be grouped by user intent, not implementation modules.

Expected areas:

- Appearance / UI scale / reduced motion
- MIDI & device defaults
- Transfer pacing and advanced reliability options
- Library storage paths/behavior
- Expansion profile shortcut
- Diagnostics/logging level
- Advanced/developer options

Do not expose raw internal implementation flags to normal users.

---

# Phase-to-UI rule

The design system can be established early, but each polished screen is implemented only when its backing domain/service layer is reliable enough.

- Phase 1: application shell + minimal Devices/Diagnostics UI
- Phase 2–3: domain/model/hardware validation; only diagnostic/model inspection UI as needed
- Phase 4: polished Patch Editor + Wave Browser + core design system
- Phase 5: Library
- Phase 6: Bank Builder
- Phase 7: Expansion Manager / compatibility surfaces
- Phase 8: Performance, Rhythm, Snapshot/Restore
- Phase 9: Library intelligence + Compare enhancements
- Phase 10: advanced sound-design surfaces
- Phase 11: Live Mode
- Phase 12: additional Roland XP/JV hardware

Do not implement a fake screen backed by placeholder logic and mark the feature finished.

Audio-preview/search screens are not planned — see ROADMAP.md "Rejected
scope — Audio Intelligence": a captured recording cannot represent the
XP-60's live, continuously-reacting synthesis engine, so a preview screen
built on recordings would misrepresent the instrument rather than merely be
unfinished.

---

# New-screen rule

Before creating any major new screen, Codex must answer from repository documentation:

1. Which user goal owns this function?
2. Can it fit naturally into an existing screen/inspector/drawer?
3. Which existing reusable components apply?
4. Which presentation model/service owns the state?
5. What are local, hardware, loading, empty, error, and disconnected states?
6. Which part of the master mockup or design system establishes its visual language?

If those questions are not clear, update this map/design documentation before inventing a disconnected UI pattern.
