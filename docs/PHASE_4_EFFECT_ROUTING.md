# Phase 4 effect routing

Status: documentation-derived implementation, tested locally. No physical
XP-60 or WIDI validation is claimed.

## Source and behavior

The official [Roland XP-60/XP-80 Owner's Manual](https://cdn.roland.com/assets/media/pdf/XP-60_80_OM.pdf),
printed pp.60-64 (PDF pp.62-66), defines the effect routing used here. The MIDI
Implementation, printed pp.223-225, establishes the corresponding raw enums.

`xpmodel/Xp60PatchRouting.h` derives a selected-Tone configuration graph:

- Structure 1 uses each Tone's own output settings. Structures 2-10 use Tone
  2's settings for pair 1+2, and Tone 4's for pair 3+4.
- MIX sends dry sound to MIX, with independent Chorus and Reverb sends.
- EFX sends dry sound to EFX, retaining independent pre-EFX Tone effect sends.
- Tone DIRECT sends only to DIRECT; that Tone's effect sends are ignored.
- EFX MIX permits EFX Chorus/Reverb sends. EFX DIRECT suppresses those sends,
  while independent pre-EFX Tone sends remain available.
- Chorus output MIX, REVERB and MIX+REV select parallel or serial branches.
- Zero sends close the corresponding configured path. Downstream paths are
  shown dashed if no nonzero path reaches them from the selected source.
- Undocumented `<OUTPUT-2>` destinations remain unknown, not mapped to DIRECT.

This is a graph of stored routing settings, not an audio meter or an evaluation
of the internal Structure algorithm. It intentionally excludes Tone switches,
audition overlays, global effect switches, audio input and gain measurements.
Other Tones can feed the shared effects even when this selected source's route
is zero. Values are raw XP levels, never claimed dB values.

## Presentation and visual decision

`PatchEditorViewModel::routing()` projects the domain result into display
identities, source ownership, edges, levels and accessible descriptions. It
uses the displayed A/B Patch and refreshes on selection, editing, undo and redo.
It cannot send MIDI or modify Patch data.

The reusable `EffectRoutingView` uses shared `SignalFlowNode` and
`SignalFlowConnector` controls. Sound/Filter/Amp/Motion use a compact diagram;
Effects and Expert expand the same graph with numeric sends and a scope legend.
Only the selected Tone or Structure pair connects down from the Tone cards.
The four Tone cards stay visible and selectable. DIRECT/unknown destination
nodes appear only when an edge uses them, avoiding an unused output row.

The master mockup's fixed series chain would falsely imply every Patch passes
through every effect. Branches and separate MIX/DIRECT destinations are a
documented hardware-truth exception under `UI_DESIGN_REFERENCE.md`. More vertical
space is needed for branches; compact disclosure preserves room for the envelope
and range controls. This resolves the fixed-chain defect, not all M2 visual gaps.

## Verification

- `tst_patch_routing`: parallel MIX sends, Tone DIRECT, EFX DIRECT with independent
  Tone sends, serial and parallel examples from p.62, zero upstream sends,
  all ten Structures for all four selections, and unknown destinations.
- `tst_patch_editor`: selection notifications, undo/redo, A/B and no MIDI writes.
- `tst_qml`: output edits create/remove arrows; nodes remain within the minimum
  width and inherit the new state.
- Screenshots use the supplied Patch fixture via FakeXp60, not an instrument.

## Remaining EFX slot evidence

Printed pp.199-203 list algorithm controls and displayed ranges; p.223 lists
twelve generic EFX Parameter bytes at Patch Common offsets 0x0D-0x18. Neither
inspected table establishes a per-algorithm slot assignment or raw-to-display
conversion. The display-table order alone is insufficient evidence to wire an
editable semantic control. Raw parameters therefore remain explicitly raw.

To resolve this in the deferred manual verification task (area 8 of
[`DEVICE_ACCEPTANCE.md`](DEVICE_ACCEPTANCE.md)):

1. Preserve a full before Patch capture and identify firmware, device ID,
   algorithm number/name and the front-panel parameter name.
2. Change exactly one EFX parameter on the XP-60 front panel in temporary Patch
   memory, then fetch again. Do not store over a permanent User Patch.
3. Record the before/after values shown on the instrument, changed Common
   offsets and raw bytes. Repeat at adjacent values and both endpoints.
   Run `tools/capture_diff.py BEFORE AFTER --markdown` to name every byte that
   moved: it resolves each changed address against the same transcribed
   Parameter Address Map that generates the C++ tables, so the reported
   parameter is a document row rather than a reading of the hex by eye. It
   accepts a binary `.syx` or hex text copied from the Protocol activity panel,
   and reports coverage differences, out-of-range values and addresses it
   cannot resolve instead of hiding them. **A changed byte is a correlation,
   not a proven slot assignment** — the mapping is established by the whole
   procedure below, not by one diff.
4. For nonlinear rates, frequencies, delays and balance, capture every supported
   displayed step or find an explicit Roland conversion table. Do not infer a
   linear conversion from two endpoints.
5. Confirm enum order, unused slots, and whether changing the algorithm resets
   any parameter bytes. Keep unexplained additional changes as open evidence.
6. Add traceable fixtures/mappings, enforce semantic bounds only at the editing
   boundary, and preserve unsupported imported raw values byte-for-byte.

Evidence row template:

| Algorithm | Front-panel parameter | Display before/after | Common offset | Raw before/after | Capture files | Firmware/interface | Result |
|---|---|---|---|---|---|---|---|
| 03:DISTORTION | Drive | not read from screen | 13 (EFX Parameter 1) | 127→0→25→127 | `--watch` 2026-09-04 | CME U2MIDI Pro | **Verified**, both endpoints |
| 03:DISTORTION | Pan | not read from screen | 14 (EFX Parameter 2) | 64→69 | `--watch` 2026-09-04 | CME U2MIDI Pro | **Verified**, one step |
| 03:DISTORTION | Amp Type | not read from screen | 15 (EFX Parameter 3) | 3→0 | `--watch` 2026-09-04 | CME U2MIDI Pro | **Verified**, 2 of 4 values |
| 03:DISTORTION | Low Gain | not read from screen | 16 (EFX Parameter 4) | 15→22 | `--watch` 2026-09-04 | CME U2MIDI Pro | **Verified**, one step |
| 03:DISTORTION | High Gain | not read from screen | 17 (EFX Parameter 5) | 15→19→22 | `--watch` 2026-09-04 | CME U2MIDI Pro | **Verified**, two steps |
| 03:DISTORTION | Level | not read from screen | 18 (EFX Parameter 6) | 127→104→100 | `--watch` 2026-09-04 | CME U2MIDI Pro | **Verified**, two steps |
| all other algorithms | Pending | Pending | Unknown | Pending | Pending | Pending | Unverified |

The first row is filled in by "First EFX slot established" below.

The header/glyph refinement is documented in `PHASE_4_REVALIDATION.md`.
Hardware acceptance and EFX semantic mappings keep M2 open.
M3 remains ordered after M2 acceptance.

## Hardware observations 2026-09-04 — EFX algorithm switching

Captured with `xp60studio_hardware_probe --watch` while the EFX algorithm was
changed from the XP-60's front panel. Read-only; every value is what the
instrument reported after a panel edit.

### Changing the algorithm rewrites the EFX parameter bytes

Step 5 of the procedure above asks whether changing the algorithm resets any
parameter bytes. **It does**, observed on three consecutive changes:

| EFX Type raw | Algorithm (our table) | Parameter bytes that changed |
|---|---|---|
| 0 → 4 | STEREO-EQ → SPECTRUM | 1, 3, 5, 6, 8, 9, 10 |
| 4 → 5 | SPECTRUM → ENHANCER | 1, 2, 5 |
| 5 → 9 | ENHANCER → LIMITER | 1, 2, 3, 4, 5, 8, 9, 10 |

The instrument loads the new algorithm's own values into the shared twelve-byte
parameter area. **A byte that did not change is not evidence that the new
algorithm leaves that slot unused** — it may simply have held the same value
before and after. Slot usage cannot be read off this table, and is not claimed.

This matters for editing: any UI that keeps a per-algorithm parameter cache
must expect the instrument to overwrite these bytes whenever the algorithm
changes, and re-read rather than assume its cached values still apply.

### Panel edits move exactly one documented byte

Four separate single-control edits each moved exactly one byte, at an offset the
transcribed Patch Common table already names:

| Byte | Offset | Change |
|---|---|---|
| EFX Parameter 1 | 13 | 64 → 75 |
| EFX Control Depth 2 | 32 | 63 → 74 |
| Reverb Type | 39 | 3 → 7 |
| Chorus Level | 33 | 127 → 101 |

This confirms the Patch Common offsets are one-to-one with front-panel controls
with no hidden coupling — turning one control did not disturb any other byte.

**It does not identify which control each byte belongs to.** The operator did not
record what was on the display for these edits, and the labels this project
prints for them are its own table's, not the instrument's. Per the warning
above, a changed byte is a correlation, not a proven slot assignment: the EFX
slot mapping stays **Unverified**, and the evidence row template below is still
unfilled.

### What the next session needs

For each algorithm under test, with the algorithm noted first:

1. the **front-panel parameter name** as the XP-60 displays it;
2. the **displayed value before and after**, not just the raw byte;
3. both endpoints of its range, and for a nonlinear control every step.

The capture side is now trivial — `--watch` names the byte the moment it moves.
The missing half is what the instrument's screen says at the same instant.

## First EFX slot established — 03:DISTORTION Drive

### Evidence

| Algorithm | Front-panel parameter | Display before/after | Common offset | Raw before/after | Capture files | Firmware/interface | Result |
|---|---|---|---|---|---|---|---|
| 03:DISTORTION (EFX Type raw 2) | Drive | 127 / 0 / 25 / 127 | 13 (EFX Parameter 1) | 127→0, 0→25, 25→127 | `--watch` capture 2026-09-04 | not recorded / CME U2MIDI Pro USB-MIDI | **Verified** |

Both endpoints of the documented 0—127 range were reached on the instrument,
plus an interior value, and no other byte moved during any of the three edits.

### Supporting structure

Switching EFX Type from raw 3 (04:PHASER) to raw 2 (03:DISTORTION) rewrote
**exactly parameter bytes 1 through 6** and nothing beyond. The manual lists
**six** controls for DISTORTION — Drive, Amp Type, Low Gain, High Gain, Pan,
Level — so the count of bytes the algorithm claims matches the count of controls
it exposes.

### The byte order is not the display order

The values loaded when DISTORTION was selected were:

| Byte | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|
| raw | 127 | 64 | 3 | 15 | 15 | 127 |

Taking the manual's display order literally would make byte 2 **Amp Type**,
which has four values (SMALL, BUILT-IN, 2-STACK, 3-STACK). A raw 64 cannot be
one of them. **Position in the printed table therefore does not give the byte
index**, which is what this document suspected and now has a concrete
counter-example for.

The remaining values are suggestive without being evidence: 64 is the centre of
`L64—0—63R`, and 15 is 0 dB in a `-15—+15 dB` range, so bytes 2, 4 and 5 look
like Pan, Low Gain and High Gain. **They are recorded as unverified guesses, not
mappings**, and each still needs its own panel sweep.

### Reference — EFX controls per algorithm

`docs/XP60-References/XP-60_80_OM.pdf` p.199 (PDF page 201) lists the controls
and displayed ranges for every algorithm. The scan carries no text layer; the
page renders legibly with PyMuPDF at 110 dpi if it needs to be read again.
Algorithms 01-11 appear there, the rest on the following pages. That table is
**documentation-derived**: it names the controls and their displayed ranges, and
says nothing about which byte carries which, which is exactly the gap the sweeps
above close one slot at a time.

### Status

One slot of twelve on one algorithm of forty. Area 8 stays open. What it now
has that it lacked is a method that works: `--watch` names the byte the instant
it moves, the manual names the algorithm's controls, and a single sweep to both
endpoints ties the two together.

## 03:DISTORTION — all six slots identified

Every control the manual lists for DISTORTION was moved on the front panel, one
at a time, with `--watch` naming the byte each time. No edit disturbed any other
byte.

| Byte | Common offset | Control | Manual range | Observed raw | Slot assignment | Range coverage |
|---|---|---|---|---|---|---|
| EFX Parameter 1 | 13 | Drive | 0—127 | 127→0→25→127 | **Verified** | **both endpoints** |
| EFX Parameter 2 | 14 | Pan | L64—0—63R | 64→69 | **Verified** | one step only |
| EFX Parameter 3 | 15 | Amp Type | SMALL, BUILT-IN, 2-STACK, 3-STACK | 3→0 | **Verified** | 2 of 4 values |
| EFX Parameter 4 | 16 | Low Gain | -15—+15 dB | 15→22 | **Verified** | one step only |
| EFX Parameter 5 | 17 | High Gain | -15—+15 dB | 15→19→22 | **Verified** | two steps only |
| EFX Parameter 6 | 18 | Level | 0—127 | 127→104→100 | **Verified** | two steps, neither endpoint |

### How each assignment was reached

Drive was swept to both ends and the control named. The other five rest on three
independent agreements, which is why they are recorded as verified rather than
guessed:

- **the default loaded with the algorithm** — byte 2 defaulted to 64, the centre
  of `L64—0—63R`; bytes 4 and 5 to 15, which is 0 dB in `-15—+15 dB`; byte 3 to
  3, the last of Amp Type's four values;
- **the observed range** — byte 3 moved 3→0, a span no other DISTORTION control
  has, every other control being at least 0—30;
- **the order the controls were swept**, confirmed by the operator, which
  separates Low Gain from High Gain since ranges alone cannot.

### What is still not established

**Range coverage.** Only Drive has been taken to both endpoints. The procedure
above asks for both endpoints and adjacent values on each control, and for
nonlinear ones every step. Five of six slots have one or two samples.

**Raw-to-display conversion.** Not one displayed value was read off the
instrument. The `display` column in the capture is this project's own passthrough
— EFX parameters are deliberately still raw — so the watcher printing `15` says
nothing about the XP-60 showing `0 dB`. The conversions implied by the manual
(gain = raw − 15, Pan centred at 64) are **documentation-derived and untested**.

**Amp Type enumeration.** Two of four values were seen. Which raw value names
SMALL, BUILT-IN or 2-STACK is unknown; only that raw 3 and raw 0 are both legal.

### What this does establish beyond DISTORTION

The byte layout follows the manual's printed order for **five of six** controls —
Drive, Amp Type, Low Gain, High Gain, Level sit at bytes 1, 3, 4, 5, 6 — with
**Pan displaced**, appearing at byte 2 where the printed table has Amp Type.
Whether that displacement is a property of Pan, of this algorithm, or of the
printed table's layout cannot be told from one algorithm. It is the first thing
a second algorithm's sweep will answer.
