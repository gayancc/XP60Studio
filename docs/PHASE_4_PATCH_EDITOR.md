# Phase 4 — Patch Editor (mockup panel M2)

## Objective

Build the Patch Editor / Four-Tone Mixer screen from the master mockup so a
Patch read from the XP-60 can be shaped locally and written back through the
verified transfer path built in Phase 3.

`docs/design/xp60studio-ui-master-mockup.jpg`, panel 2 (top right) is the
authoritative visual target. A capture of the built screen is in
`docs/design/screenshots/phase4-editor-screen.png`, rendered headless from the
shipping QML against a real Patch out of the golden fixture.

## Milestones

| Milestone | State |
|---|---|
| **M2** Patch Editor / Four-Tone Mixer | **partial — not complete**; see `PHASE_4_REVALIDATION.md` for acceptance gaps and current validation |
| **M3** Wave Browser | **partial** — 448 documented internal names, search and inspector; assignment awaits bank-mapping evidence. See `PHASE_4_WAVE_BROWSER.md`. |

### What M2 covers, and what it does not

The 2026-09-04 revalidation supersedes historical completion claims below.
The screen defaults to local editing. Explicitly armed live audition now applies
Solo/Mute, A/B and coalesced parameter changes through verified transfers.
See `PHASE_4_LIVE_AUDITION.md`. Physical audio/latency checks, EFX-specific byte
interpretations are still open. Header/glyph refinement and keyboard Tone
controls are implemented; see the latest `PHASE_4_REVALIDATION.md` entry.
The fixed-chain routing gap is repaired; see `PHASE_4_EFFECT_ROUTING.md`.

Built: the Current Patch header, the visual four-Tone architecture and mixer,
Tone Solo/Mute, graphical envelope editors for the Sound (Pitch), Filter and
Amp (Level) sections, key and velocity range controls, signal-flow
visualisation, exact numeric entry beside every continuous control, the A/B
original/current foundation, and local / modified / on-instrument state
distinctions.

The 2026-09-04 continuation adds:

- Sound: Pitch, Pitch Envelope, Wave/FXM, Tone Delay and Structure groups.
- Filter: TVF type/cutoff/resonance and full TVF Envelope parameters.
- Amp: TVA, TVA Envelope and Pan parameters.
- Motion: both LFOs, including waveform, trigger, rate, delay, fade, sync and
  Pitch/Filter/Level/Pan modulation depths; controller assignments and switches.
- Effects: common EFX, Chorus and Reverb groups plus selected-Tone routing.
  EFX type-specific parameter meanings are **not** implemented: the twelve
  raw parameter slots remain explicitly labelled as such.
- Play / Design / Expert disclosure over the same working Patch. Play retains
  the mixer and common play settings; Design adds section controls and graphical
  envelopes/ranges; Expert provides searchable, grouped, virtualized controls
  for the selected Tone or Patch Common. All four Tone cards remain visible.
- Exact numeric entry for both key and velocity bounds, with ordered limits.
- Selected-Tone/Structure-pair connections and a branching effect-routing diagram.
  See `PHASE_4_EFFECT_ROUTING.md`; MIX/DIRECT and parallel sends follow the manual.
  Wrapped two-column layouts omit them to avoid crossing other Tone cards.

Still open:

- semantic, per-effect controls with verified EFX slot/unit mappings
- physical acceptance of live updates, Solo/Mute and A/B
- final M2 acceptance after verified EFX semantics and the physical checks
- Wave assignment and remaining M3 capabilities; catalog development is authorized before final physical validation

## Architecture

The UI never constructs Roland SysEx, and QML never sees an address, a
checksum or a libremidi type. Editing flows:

```
QML control  →  ToneViewModel / PatchEditorViewModel  →  Xp60Patch (typed model)
                                                            ↓  (explicit, armed)
                                        PatchTransfer  →  DeviceSession  →  DT1 + read-back verify
```

| Piece | Where | Notes |
|---|---|---|
| `PatchEditorViewModel` | `src/presentation/PatchEditorViewModel.*` | screen state: identity, sections, tones, signal flow, envelope, ranges, contextual settings, A/B, history, write |
| `ToneViewModel` | `src/presentation/ToneViewModel.*` | one Tone of the mixer; holds no data of its own beyond audition state |
| `EditorParameterModel` | `src/presentation/EditorParameterModel.*` | editable descriptor-backed section/Expert projections; no duplicated Patch state or protocol offsets in QML |
| `EditorParameterPanel` | `qml/XP60Studio/Controls/EditorParameterPanel.qml` | reusable virtualized parameter groups, enum menus, exact values and Expert search/scope |
| `EditorScreen.qml` | `qml/XP60Studio/Screens/` | the M2 composition |
| Synth controls | `qml/XP60Studio/Controls/` | `XpKnob`, `XpSegmentedControl`, `ParameterValueEditor`, `ToneCard`, `ToneMiniEnvelope`, `EnvelopeEditor`, `KeyboardStrip`, `XpRangeBar`, `SignalFlowNode`, `SignalFlowConnector` |

Three entries in `docs/design/COMPONENT_CATALOG.md` § 4 are satisfied by a
property or a pairing rather than a separate type, because splitting them
would have produced two components with the same code:

| Catalog entry | Built as |
|---|---|
| `XpBipolarKnob` | `XpKnob { bipolar: true }` — draws the arc from the centre and centres the double-click reset |
| `EnvelopePointHandle`, `EnvelopeStageReadout` | the draggable points inside `EnvelopeEditor`, and the `ParameterValueEditor` pair per stage |
| `KeyRangeSelector`, `VelocityRangeSelector` | `KeyboardStrip` (notes, with the range drawn on the keys) and `XpRangeBar` (velocity) |

Every continuous control has an exact numeric companion, as the catalog
requires: knobs pair with a readout, envelope points with their stage fields,
and both range controls with their numeric ends.

### Editing rules

- **Local editing is non-destructive.** The fetched Patch is kept as the A
  side; every edit applies to a working copy. Nothing reaches the instrument
  until the user arms and presses Write or explicitly starts live audition.
- **A refused value costs nothing.** A write outside the documented range is
  validated before history changes. Crossed key/velocity bounds are also
  rejected by the editor boundary, including the Expert path.
- **Undo is bounded** at 64 steps; a new edit clears the redo branch.
- **A/B freezes editing.** While the original is on screen, edits and renames
  are refused rather than silently applied to the hidden working copy.
- **Solo and mute are audition state**, never Patch data: toggling them leaves
  the Patch byte-identical and adds no undo step.
- **Re-fetching replaces the A side** and clears the history, because the
  instrument, not the editor, is then the source of truth. This applies only
  to an editing fetch; transfer safety/read-back fetches preserve editor history.
- **Parameter identity is independent of storage offsets.** Numeric and enum
  edits resolve a documented parameter ID and Tone, then use the typed model.
  Changing values sends `dataChanged`, preserving focused delegates. Section,
  scope and search changes rebuild the bounded, virtualized projection.
- **Unknown interpretation stays visible.** Numeric fields display formatted
  values when idle and edit raw XP values on focus; enum menus use transcribed
  labels. The Effects and Expert panels state missing semantic/unit information
  before the controls. No effect names, physical units or wave names are invented.

### Writing

Writing is unchanged from Phase 3 and stays explicit: a read must have
succeeded, the user must arm, and the write is followed by a read-back that
compares every parameter. The editor exposes `canArmWrite`, `writeArmed`,
`canWrite` and the transfer's own state text; it adds no path around them.

## The keybed

`docs/design/screenshots/phase4-key-range.png`

The key range control is a real keyboard, not a row of equal slices. Seven
white keys to the octave; the five black keys at 60 % of a white key's width,
each straddling the boundary between its neighbours the way a piano's do — the
middle black of a three-key group centred on the boundary, the outer ones
shifted outward. Keys carry a vertical gradient and a shaded front, black keys
a gloss line and a drop shadow onto the whites, and the octaves are marked
C2, C3, … so a key can be identified without counting.

It is **drawn, not photographed.** A bitmap of a keyboard cannot survive the
things this control has to do: stretch to any panel width without distorting
the key proportions, take the Tone's accent colour, stay crisp on a HiDPI
display, and work in either theme. It would also be a third-party asset with
its own licence in a repository that has none. Canvas drawing gives all of
that and repaints once per change.

**What it draws is the instrument's own keybed.** Roland's Key Range
parameters span the whole MIDI note range (0..127, C-1..G9), but the XP-60
itself has 61 keys, C2..C7. Drawing all 128 notes in a panel this wide leaves
about four pixels per key — a barcode, not a keyboard. So the strip draws the
XP-60's 61 keys, and widens to whole octaves whenever a Patch's range reaches
past them, saying plainly that the range is *"Past the XP-60's 61 keys — MIDI
only"*. Nothing is clamped: a range outside the keybed is legal, writable, and
playable from a sequencer, and the screen says so instead of hiding it. The
keybed span lives in `xp60::keybed()` beside the other device facts, marked
`DocumentationDerived` from the product specification, so QML never invents a
hardware fact.

## Motion

Animation is used to explain a change, never as decoration, and every duration
comes from the `Motion` singleton, so the reduced-motion switch turns all of it
off at once.

| Where | What moves | Why |
|---|---|---|
| Keybed range | the two edges, the front glow and the range bar ease to a new position (`durationFast`) | a drag, an undo, a revert or a re-fetch reads as the range moving rather than the panel redrawing |
| Keybed and velocity handles | the grabbed edge thickens and brightens | the pointer and the keyboard both show which end is being moved |
| Keybed hover | the key under the pointer lifts in brightness | says what a click would grab, without sending a note |
| Velocity window | the active part of the soft-to-hard ramp eases between values (`durationFast`) | the same reading as the keybed, on the other axis |
| Keybed emphasis | the range wash and glow fade with the velocity window (`durationNormal`) | a Tone that only answers a narrow band of velocity is drawn more faintly, so the two controls read as one setting |

Nothing loops while idle. Colour is never the only signal: the range also has
hard edges, a bar, and the two note names beneath it, and the out-of-keybed
state is stated in words as well as in warning colour.

## Deviations from the mockup

Each is allowed by `docs/design/UI_DESIGN_REFERENCE.md` § *Allowed deviations*,
rule 4 — the mockup shows illustrative data that verified device metadata does
not support. In every case the alternative would be to invent a Roland fact,
which `AGENTS.md` forbids.

| # | Mockup shows | Built screen shows | Why |
|---|---|---|---|
| 1 | Wave names — "Warm Strings", "Choir Aahs", "Analog Bell", "Warm Pad" | The wave identifier, e.g. `INT 005`, with group and gain beneath | The 448-name list is transcribed and searchable in M3. Mapping raw group IDs to INT-A/INT-B still requires XP-60 evidence; see `PHASE_4_WAVE_BROWSER.md`. |
| 2 | Tone level as `-2.3 dB` | The Roland raw value, e.g. `117` | The Parameter Address Map defines Tone Level as raw 0..127 and gives no conversion to decibels. |
| 3 | Envelope stage readouts as `0.40 s`, `2.20 s`, `-6.0 dB`, `3.20 s` | Raw times and levels 0..127, with a note on the card stating that the manual gives no conversion | Same reason. The note is shown rather than hidden so the screen never implies a precision it does not have. |
| 4 | Effect names — MFX "St.Parametric", Chorus "Chorus 1", Structure "4-TONE" | `HEXA-CHORUS`, `Level 127`, `1 / 1` (the structure pair) | All 40 EFX type names are now transcribed from the official Owner's Manual. Chorus has fixed controls and no documented type selector. Reverb is named (`STAGE2`) from the map. EFX-specific slot mappings/units remain open. |
| 5 | Envelope stage boxes in one row of four | Two rows of two | Rule 6 — at the mockup's column proportions four label-plus-field pairs crush the numeric fields to a few pixels, and every value must stay typeable. |
| 6 | Tone card octave box only | Octave box, plus the exact semitone count when the tuning is not a whole octave | Rule 1 — Coarse Tune is per-semitone on the XP-60, so an octave stepper alone cannot express every value a Patch may already hold. The line is blank when the tuning *is* a whole octave. |
| 7 | Keybed labelled `C-2` … `G8`, drawn across the full note range | `C-1` … `G9`, drawn as the XP-60's 61 keys and widened only when the range needs it | Rule 1 and rule 6. Roland's Parameter Address Map names the Key Range bounds `C-1..Upper`, so C-1..G9 is the instrument's own convention and the mockup's octave numbering is one octave off it. Drawing all 128 notes at this panel width gives about four pixels a key; see **The keybed** above. |

Nothing above changes the composition, colour system, hierarchy or interaction
model of the mockup; the four anchor screens are still the target.

## Tests

| Test | Covers |
|---|---|
| `tst_patch_editor` | existing editor/transfer/history/range regressions, plus disclosure preserving data, selected-Tone edits, stable model notifications, both LFOs and modulation targets, effect routing, nibble parameter indexing, Expert search and crossed-range rejection |
| `tst_qml` — `EditorScreen` | real fetched Patch and four Tone cards, grid/header reflow, envelopes and ranges, arming/A/B/history, plus disclosure, filter edits, Motion/Effects controls, Expert search/scope, exact bounds and real keyboard entry/enum activation |

Both run against `tests/fixtures/xp60/user-bank-amal.syx`, a real XP-60 user
bank, through a `FakeXp60` that answers RQ1 and applies DT1 — so the screen is
exercised against real Patch data and the real protocol path, not invented
bytes. The shared harness lives in `tests/cpp/support/FakeXp60.h`.

`tests/tools/screenshot_harness.cpp` renders any available screen headless
against the same fixture; it is a documentation aid and is never registered as
a test.

Latest Windows routing validation: **23/23 CTest suites pass**, including a new
routing suite and editor/QML routing regressions. Fresh Windows screen captures
are listed in `PHASE_4_REVALIDATION.md`; these supersede the historical capture
at the start of this document.

## Not verified on hardware

No part of Phase 4 has been exercised against a real XP-60. The editor writes
through the Phase 3 transfer path, whose hardware validation is still the
open procedure in `docs/HARDWARE_VALIDATION_XP60.md` (steps 1–8a). Nothing in
this phase promotes any parameter to `HardwareVerified`.

## Open items

1. **XP-60 bank mapping** — the 448-name list is transcribed for M3. Raw bank
   IDs still need XP-60 evidence before resolving deviation 1. Categories are unknown.
2. **EFX parameter slot/unit mappings** — the 40 EFX type names are now transcribed.
   The fixed Chorus has no documented type selector; its controls follow p.64.
3. **Envelope time/level units** — if Roland documents a conversion anywhere,
   deviations 2 and 3 can be resolved; otherwise raw values stay.
