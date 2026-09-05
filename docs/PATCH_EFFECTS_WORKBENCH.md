# Patch Effects Workbench

The Patch Editor's Design → Effects surface now has General, EFX, Control,
Chorus and Reverb views. The existing four-Tone mixer, signal-flow graph,
Original/Current comparison, undo/redo and explicit hardware workflows remain
the owning editor. Expert retains the generic exact parameter infrastructure.

## Interaction

- General: three constrained output sockets and a send mixer. Drag a socket
  onto one of its destinations, or select the destination by mouse/keyboard.
  The preview snaps to the destination and describes the pending assignment.
  Escape or releasing outside a destination cancels without editing.
- The source controls follow the actual Structure output owner. Structures
  2–10 use Tone 2/4 settings, including when Tone 1/3 is selected.
- Click EFX, Chorus or Reverb in the signal graph to select that processor.
  Click a route's value to open its send control. Paths respond to send amount;
  dashed means zero send or inactive upstream. An omitted path is not the same
  as a configured zero send. DIRECT suppresses the documented sends without
  deleting their stored values.
- Knobs snap to integer XP values. Keyboard arrows and Shift fine steps are
  available. Click the displayed value to expose exact numeric entry. Ghost
  ticks show the fetched Original A position.
- Chorus has stereo modulation lanes and Reverb has decay/echo diagrams;
  PAN-DLY shows alternating left/right events. The point controls Rate/Depth
  or Time/Level. Knobs and graph positions share presentation state. These are
  normalized parameter diagrams, not audio analysis or physical timing models.
- Dragging a bound point or knob forms one undo command. Local state updates
  immediately and uses the existing live-audition coalescing/verification path
  only when the musician has explicitly enabled live audition.
- Control has two source/depth lanes and Hold/Peak states. Negative depth has
  a reverse-direction indicator. Documented eligible targets can be inspected
  in the algorithm rack, but target-slot/lane bindings are not fabricated.

## Binding inventory

“Bound” below means implemented using existing documentation-derived parameter
descriptors, not newly verified against a physical XP-60.

| Surface | Bound to the existing Patch model |
|---|---|
| Tone/Structure output | Output Assign (MIX/EFX/DIRECT), Mix/EFX Send, Chorus Send, Reverb Send, using the Structure output owner |
| EFX rack | Type 01–40, EFX Output Assign (MIX/DIRECT), EFX output level, EFX Chorus/Reverb sends |
| Control | Both sources, both signed depths, EFX Hold/Peak |
| Independent Chorus | Level, Rate, Depth, Pre Delay, Feedback, Output (MIX/REVERB/MIX+REV) |
| Independent Reverb | All eight types, Level, Time, HF Damp; Delay Feedback only displayed for DELAY/PAN-DLY |

Intentionally unavailable in Design:

- Every algorithm-specific semantic EFX control: the twelve raw slots still
  lack verified algorithm-specific slot assignments and conversion mappings.
  Their diagrams are explicitly dashed **schematics**, with no claim to show
  the current Patch's response. Controls show **UNBOUND**, not invented values.
- Controller target-to-slot/lane mappings. The manual's `#`-marked eligible
  target names are shown for inspection, not serialized as new assignments.
- Undocumented `<OUTPUT-2>` destinations. Imported values are preserved and
  shown as unknown by routing; Design cannot create them.

`editEffect` rejects raw EFX slots, unrelated fields, invalid ranges, unknown
destinations and all edits while comparing A. Raw EFX slots remain available
through the existing Expert workflow; this slice does not reinterpret them.

## Catalog and visual families

All 40 existing EFX identities are retained. Algorithms 01–25 have a single
algorithm stage; 26–37 explicitly show A → B **SERIES**; 38–40 explicitly show
A / B **PARALLEL**. Combined algorithms group controls by stage and retain the
algorithm output Level separately from the general EFX output send.

Read-only semantic families cover EQ/Spectrum, drive, filter/modulation,
enhancer, rotary, compressor/limiter, chorus/flanger, delay/taps, pitch lanes,
reverb and gated reverb. They must receive verified semantic bindings before
their controls or graph points are enabled. No response curve is derived from
unverified raw slots.

Reusable components:

- `EffectsWorkbench`: five-view composition and rack palette.
- `EffectParameterControl`: existing XpKnob plus exact entry, original marker,
  integer edits and grouped gesture lifecycle.
- `EffectChoiceControl`: keyboard-accessible semantic choice chips.
- `EffectRouteSelector`: constrained socket/cable gesture and destination buttons.
- `EffectParameterGraph`: parameter visualization and bound point interaction.
- `EffectRoutingView` / `SignalFlowConnector`: unit selection, send-level
  emphasis, accessible route readouts and contextual editing.

`PatchEffectsPresentation.cpp` extends the existing editor presentation model.
All validation, catalog terminology and Structure ownership stay in C++.
No protocol addresses, conversions, codecs or transport behavior were changed.

## Evidence and remaining hardware work

Source: official [Roland XP-60/XP-80 Owner's Manual](https://cdn.roland.com/assets/media/pdf/XP-60_80_OM.pdf),
printed pp.60–65 (routing and independent processors), 74–88 (topologies and
`#` controller eligibility), 199–203 (algorithm terminology), and the existing
MIDI Implementation pp.223–225 descriptors. The repository's local scanned
manual was visually reviewed; the page references are printed page numbers.

Software checks exercise all algorithm identities/topologies, invalid writes,
Structure ownership, knob/graph synchronization, A/B, grouped undo/redo,
destination clicks/drags, Escape cancellation and page navigation. The existing
protocol, codec, transfer, live-audition and QML regression suites also apply.

Physical follow-up: fetch a known temporary Patch; change each bound routing
and processor control; play sustained notes; send/read back and compare bytes;
verify DIRECT behavior, Chorus destinations, delay feedback, A/B and live
audition interruption. For each EFX type, capture one front-panel parameter
change at a time to establish slot identity, discrete resolution, units and
controller-lane correspondence before enabling semantic controls. No hardware
verification is claimed by this slice.

## Visual acceptance

The workbench extends the approved Patch Editor composition. It preserves
the shell, four Tone identities, signal graph and shared dark surfaces. The
new lower processor workspace replaces only the Design Effects grid. Tone
play-setting fields are omitted in Effects because they are unrelated to this
workflow and remain in the other Design sections. Secondary exact entry is
disclosed from each value; Expert remains intact.

Screenshots are produced by the real QML screenshot harness using the golden
Patch through FakeXp60, not a connected instrument. The shell's LIVE/Verified
indicator in these captures belongs to that simulated fixture connection.
