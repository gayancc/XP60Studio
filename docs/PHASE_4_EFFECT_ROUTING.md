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

To resolve this in the deferred manual verification task:

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
| Pending | Pending | Pending | Unknown | Pending | Pending | Pending | Unverified |

The header/glyph refinement is documented in `PHASE_4_REVALIDATION.md`.
Hardware acceptance and EFX semantic mappings keep M2 open.
M3 remains ordered after M2 acceptance.
