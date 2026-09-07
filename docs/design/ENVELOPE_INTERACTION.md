# Dragging envelope points, and why it feels smooth

A musician drags the corner of an envelope and expects the handle to sit under
the pointer and the sound to follow. The failure mode is familiar: the handle
jumps in visible steps, sticks under the cursor during slow movement, and
stutters when the hand rests on a boundary. That is what "glitchy and stuck"
means, and it is worth being precise about its cause.

## It is not a frame-rate problem

An XP-60 envelope Time is an integer 0..127 and a Level is 0..127 (0..126 for
the Pitch envelope). A plot is a few hundred pixels wide. If the pointer is
mapped onto the integer and the handle is then drawn *from that integer*, the
handle can only ever be in 128 places. Every symptom follows from that one
decision, and no amount of render smoothing fixes it, because the information
was thrown away before the renderer saw it.

## The four mechanisms

`interaction::EnvelopeDrag` keeps the resolution instead of recovering it.

**1. A continuous shadow.** The grabbed point's position is held as a double
from the moment of the grab. The view draws that. The stored integer is derived
from it, never the reverse. The handle therefore tracks the pointer at display
resolution while the parameter underneath moves in its own steps.
*Test: `theHandleTracksThePointerExactlyUntilItIsPinned` — 1000 samples, the
handle within 1e-9 of the pointer throughout the free part of the drag.*

**2. Hysteresis at the step boundary.** The integer only follows once the shadow
has crossed the midpoint by a fraction of a step. A hand resting on a boundary
produces one change, not a stream of them — which matters twice over, because
each spurious change is also a MIDI message.
*Test: `aHandHoveringOnAStepBoundaryDoesNotFlicker` — 200 jitter samples astride
a boundary produce **zero** value changes.*

**3. Exact accumulation.** The shadow is recomputed from the grab origin on
every sample rather than integrated per sample. Drift is therefore impossible
rather than merely small, and a slow careful drag loses no motion.
*Tests: `aSlowDragNeverSticks` — 500 sub-step samples, the handle strictly
advances every time; `manySmallMovesLandWhereOneBigMoveWould` — 2000 small moves
and one large move reach the identical integer.*

**4. Coalescing.** `takeEdits()` yields at most one entry per parameter, holding
its latest value. A drag producing hundreds of samples a second yields one
message per parameter per drain, which is what makes it safe to feed a
31250-baud link (`ROADMAP.md` Phase 4: "do not send excessive MIDI while
dragging controls").
*Test: `hundredsOfSamplesCoalesceIntoTwoEdits` — 800 samples, at most 2 edits.*

## What it deliberately does not do

**No smoothing, filtering, prediction or easing of the pointer.** Those make a
control feel *different*, not more accurate, and on an editor whose whole
purpose is exact values they put the handle somewhere the user did not put it.
The smoothness here comes from not discarding information in the first place,
which is a stronger guarantee than any filter: it is exact, and it is testable.

**No invented time axis.** Roland documents envelope Time as a value 0..127 and
nowhere says what any of those values is in milliseconds. The relationship is
certainly not linear, but this project has not measured it. An axis labelled
"seconds" drawn from an unmeasured curve would be the most convincing kind of
wrong — authoritative-looking and misleading every judgement made from it. So
the horizontal axis is **proportional to the stored Time values**: a faithful
picture of the data, honest about being that rather than a picture of elapsed
time, and exactly what a musician grabs when what they are editing *is* the
stored value. If the times are ever measured on hardware, a real time axis
becomes a second, clearly-labelled mode.

**No fighting back.** The plot's horizontal axis is normalized by the total of
the four Time values, so widening one stage narrows the others. The pixels-per-
step mapping is fixed at the grab for exactly this reason: recomputed per
sample, the handle would accelerate away from the pointer as the drag
proceeded.

## What it says

A control that stops moving without saying why reads as broken, so a frame
pinned against a range end carries the reason: "Time 2 is at its maximum."
`cancel()` returns the edits that put every parameter back where the grab found
it — an escape-key undo that does not depend on the editor's history.

## Corrections this work forced

The Filter envelope's *Levels* are 0..127, not bipolar. Filter Envelope **Depth**
is bipolar (-63..+63) and it is easy to carry that across to the envelope's own
levels; the tables say otherwise. Only the **Pitch** envelope's levels are
bipolar, so only its plot draws zero in the middle. Measured from the generated
tables, not assumed.

## The same engine, generalised

`interaction::ParameterDrag` is the four mechanisms factored out for every
ordinary knob, slider and field, where the same stickiness shows up and is if
anything more obvious. Wave Gain has **four** legal values: drawn from the
quantised value a knob snaps between four positions and feels broken; drawn from
the shadow it turns continuously and lands on the value the hand chose. It takes
a `ParameterDescriptor` — the same one that drives decoding, display and
validation — so the range and step count are the instrument's, never a caller's
guess. `sensitivity` gives a fine-drag modifier for free, because precision here
is a scale factor on an exact mechanism rather than a separate code path.

`interaction::LfoGeometry` draws the LFO, on the same terms as the envelope:

- **No frequency axis.** Roland documents LFO Rate as 0..127 and publishes no Hz
  mapping. The plot shows one width, and the number of cycles across it is
  proportional to Rate. A faster Rate draws more cycles; nothing claims a
  frequency.
- **Delay and Fade** shade the start of the plot in proportion to their stored
  values, and the curve rests on the centre line until the delay has elapsed
  rather than pretending the LFO starts at note-on. Each takes at most half the
  width, so a long delay cannot push the waveform off the plot.
- **The waveform is an identity**, one of Roland's eight, drawn as itself and
  switched — never blended. `lfoWaveformIsAperiodic()` marks Sample & Hold,
  Random and Chaos, which have no fixed shape: a view draws a representative one
  and can say that it is representative. That shape is seeded from the stored
  Rate, so the same Patch always draws the same picture — a plot that reshuffled
  itself on every repaint would look like a fault.

## A bug this work found

The generator assigns categories by matching rules **in order**, and the `Wave`
rule sat above the `LFO1` / `LFO2` rules. "LFO1 Waveform" contains the word
"Wave", so both LFO waveforms were filed under the Wave category. The
consequences had already shipped: `PatchComponentCopy` made "copy the wave"
silently change a Tone's LFO shapes, and made "copy LFO 1" leave its own shape
behind. The parameter's block position settles it — offset `00 2D` sits between
the Controller rows and LFO1 Key Trigger — so the rules are reordered, the tables
regenerated (Wave 9 → 7 parameters, LFO1 and LFO2 11 → 12 each), and
`tst_generated_tables` now pins every `tone.lfo*` parameter to its own LFO
category so it cannot drift back.

## What is not built yet

The QML surface. This container has Qt 6.4 and the screens need 6.5+, so a view
written here could be neither run nor looked at (`ROADMAP.md` Phase 9 records
the same constraint for the other screens). The engine above is where the
difficulty lives and it is fully tested without a window; the view is a
`Canvas`/`Shape` that draws `DragFrame::points` and forwards pointer samples,
and it should be written where it can be seen.
