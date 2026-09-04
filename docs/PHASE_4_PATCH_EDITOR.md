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
| **M2** Patch Editor / Four-Tone Mixer | built, tested |
| **M3** Wave Browser | **blocked** — needs the XP-60 Waveform List (wave number → name, category), a manual appendix that has not been supplied |

### What M2 covers, and what it does not

Built: the Current Patch header, the visual four-Tone architecture and mixer,
Tone Solo/Mute, graphical envelope editors for the Sound (Pitch), Filter and
Amp (Level) sections, key and velocity range controls, signal-flow
visualisation, exact numeric entry beside every continuous control, the A/B
original/current foundation, and local / modified / on-instrument state
distinctions.

Not built yet, and honestly marked as such on the screen — the Motion and
Effects tabs show a "not built yet" state rather than an empty panel:

- LFO editor (Motion section)
- effects editor (Effects section)
- Play / Design / Expert progressive-disclosure model
- safe real-time parameter updates while dragging (throttling and coalescing);
  today edits are local and reach the instrument only on an explicit Write
- Wave Browser (M3, blocked above)

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
  until the user arms and presses Write.
- **A refused value costs nothing.** A write outside the documented range is
  rejected by `Xp60Patch::setRaw` and the undo step is rolled back, so the
  history never contains a state the model would not accept.
- **Undo is bounded** at 64 steps; a new edit clears the redo branch.
- **A/B freezes editing.** While the original is on screen, edits and renames
  are refused rather than silently applied to the hidden working copy.
- **Solo and mute are audition state**, never Patch data: toggling them leaves
  the Patch byte-identical and adds no undo step.
- **Re-fetching replaces the A side** and clears the history, because the
  instrument, not the editor, is then the source of truth.

### Writing

Writing is unchanged from Phase 3 and stays explicit: a read must have
succeeded, the user must arm, and the write is followed by a read-back that
compares every parameter. The editor exposes `canArmWrite`, `writeArmed`,
`canWrite` and the transfer's own state text; it adds no path around them.

## Deviations from the mockup

Each is allowed by `docs/design/UI_DESIGN_REFERENCE.md` § *Allowed deviations*,
rule 4 — the mockup shows illustrative data that verified device metadata does
not support. In every case the alternative would be to invent a Roland fact,
which `AGENTS.md` forbids.

| # | Mockup shows | Built screen shows | Why |
|---|---|---|---|
| 1 | Wave names — "Warm Strings", "Choir Aahs", "Analog Bell", "Warm Pad" | The wave identifier, e.g. `INT 005`, with group and gain beneath | The XP-60 Waveform List (wave number → name) is a separate manual appendix that has not been transcribed. The Parameter Address Map gives only Wave Group Type, Group ID and Wave Number. Resolved by M3 once the list is supplied. |
| 2 | Tone level as `-2.3 dB` | The Roland raw value, e.g. `117` | The Parameter Address Map defines Tone Level as raw 0..127 and gives no conversion to decibels. |
| 3 | Envelope stage readouts as `0.40 s`, `2.20 s`, `-6.0 dB`, `3.20 s` | Raw times and levels 0..127, with a note on the card stating that the manual gives no conversion | Same reason. The note is shown rather than hidden so the screen never implies a precision it does not have. |
| 4 | Effect names — MFX "St.Parametric", Chorus "Chorus 1", Structure "4-TONE" | `Type 11`, `Level 127`, `1 / 1` (the structure pair) | The EFX and Chorus type name lists are not transcribed; the map gives the raw index. Reverb *is* named (`STAGE2`) because its eight labels are in the map. |
| 5 | Envelope stage boxes in one row of four | Two rows of two | Rule 6 — at the mockup's column proportions four label-plus-field pairs crush the numeric fields to a few pixels, and every value must stay typeable. |
| 6 | Tone card octave box only | Octave box, plus the exact semitone count when the tuning is not a whole octave | Rule 1 — Coarse Tune is per-semitone on the XP-60, so an octave stepper alone cannot express every value a Patch may already hold. The line is blank when the tuning *is* a whole octave. |

Nothing above changes the composition, colour system, hierarchy or interaction
model of the mockup; the four anchor screens are still the target.

## Tests

| Test | Covers |
|---|---|
| `tst_patch_editor` (28 cases) | empty state, adopting a fetched Patch, tone card binding, refused values, octave stepping, pan/level formatting, solo/mute leaving data untouched, sections and envelopes, the three-level Level envelope, envelope dragging and exact entry, key/velocity ordering, contextual settings, undo/redo/revert/bounded history, A/B freezing, patch-name validation, arming and writing, mismatch reporting, re-fetch |
| `tst_qml` — `EditorScreen` (16 cases) | screen binds to a real fetched Patch, four colour-coded Tone cards, grid reflow, section tabs driving the envelope, a knob edit reaching the Patch, write gated behind arming, A/B, undo/redo/revert button state, raw-unit envelope readouts, key range and velocity following the selected Tone, and the new controls' keyboard, range and signal behaviour |

Both run against `tests/fixtures/xp60/user-bank-amal.syx`, a real XP-60 user
bank, through a `FakeXp60` that answers RQ1 and applies DT1 — so the screen is
exercised against real Patch data and the real protocol path, not invented
bytes. The shared harness lives in `tests/cpp/support/FakeXp60.h`.

`tests/tools/screenshot_harness.cpp` renders any available screen headless
against the same fixture; it is a documentation aid and is never registered as
a test.

## Not verified on hardware

No part of Phase 4 has been exercised against a real XP-60. The editor writes
through the Phase 3 transfer path, whose hardware validation is still the
open procedure in `docs/HARDWARE_VALIDATION_XP60.md` (steps 1–8a). Nothing in
this phase promotes any parameter to `HardwareVerified`.

## Open items

1. **XP-60 Waveform List** — required for M3 (Wave Browser) and to resolve
   deviation 1. Needed as wave number → name, with category if the manual
   gives one.
2. **EFX and Chorus type name lists** — would resolve deviation 4.
3. **Envelope time/level units** — if Roland documents a conversion anywhere,
   deviations 2 and 3 can be resolved; otherwise raw values stay.
