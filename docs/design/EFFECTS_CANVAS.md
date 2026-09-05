# The Living Effects Canvas

The effects surface of the Patch Editor. Replaces the routing diagram that
preceded it, which was accurate but read as engineering documentation: seven
cables of near-equal weight, bare numbers floating beside them, five
same-shaped boxes, and no indication that any of it could be touched.

Screenshots: [`screenshots/effects-canvas/`](screenshots/effects-canvas/).

## What the redesign changes

### A spine, not a graph

`SOURCE → MFX/EFX → MIX OUT` runs dead straight along one centreline at full
weight. Chorus and Reverb are a **send tier above it**; the dry bypass and any
alternate output run **below it**. Where a stage sits states its role before a
word is read, and the spine gives the eye somewhere to start.

Every rail class owns a horizontal channel, and no channel crosses a node body:
returns along the top, the send tier, two send channels, the spine, then dry and
direct. Source sends and EFX sends have **separate** channels and separate entry
points, so two sends into the same processor never share a line or a chip
position. That is what removed the crossings; it is not a matter of styling.

### Four rail classes, deliberately unequal

| Class | Weight | Carries |
|---|---|---|
| `main` | heaviest, opaque | the spine |
| `return` | medium | Chorus/Reverb back to Mix, Chorus into Reverb, dry bypass |
| `send` | light, scaled by level | Tone and EFX sends |
| `direct` | light | alternate or unmapped output |

A send's weight follows how much is actually being sent, **within its class**:
turning a send to 127 can never make it outrank the spine. A path carrying
nothing stays visible as a faint dashed trace, so the topology never appears to
change shape — it just stops competing for attention.

### Values that say what they are

Bare numbers are gone. Each addressable route carries a chip — `CHORUS SEND
127`, `EFX OUT 100` — that names the value and **is** the control for it:

- **drag vertically** to adjust, with a value bubble showing the parameter's
  own label and display text;
- **click** to isolate the route;
- **double-click** for exact numeric entry;
- **arrow keys** to nudge when focused.

A drag is one undo entry however many values it passes through, because the
gesture brackets `beginEffectGesture`/`endEffectGesture`. Values are written raw
through `editEffect`, inside the range the model published, so XP resolution is
exact and nothing is rescaled to suit the gesture.

### Focus and isolation

**Focus** — clicking a stage makes it the hero: it lifts slightly rather than
moving, unrelated stages and routes recede, and its own controls appear beneath
the canvas. Every stage stays on screen, so the topology is never lost.

**Isolation** — clicking a route lights that path alone, keeps its two endpoints
legible, and explains the value rather than leaving it bare.

Both clear by clicking empty canvas or **Back to overview**.

### Direct routing manipulation

A focused stage that has a destination choice grows a handle. Dragging it lights
**only** the destinations the XP-60 actually offers, states the consequence in
the instrument's own words before the drop (`EFX Output Assign → MIX`), and
writes the documented parameter on release as a single undoable edit.

The permitted destinations come from the model's own enumerations — `MIX`/`DIR`
for EFX, `MIX`/`REVERB`/`MIX+REV` for Chorus, `MIX`/`EFX`/`DIR` for a Tone. Two
deliberate refusals:

- the undocumented `<OUTPUT-2>` value is dropped by the presentation layer
  before it reaches QML and can never be selected by a drop;
- `MIX+REV` names two stages at once, so it is **not** a drop target. Guessing
  which one a pointer meant would be inventing behaviour; it stays with the
  enumerated control in the inspector.

This is not modular cabling. The XP-60 parameter model remains authoritative and
the canvas only ever writes parameters it publishes.

### Micro-visualisation

Each stage draws what it does: six voices across the stereo field for a
hexa-chorus, taps on a timeline for a delay, a transfer curve for a compressor,
rotors for a rotary, a decay shape for a room. Changing algorithm or reverb type
crossfades the drawing, which is how the eye notices the change.

**These are topology and parameter pictures, never audio analysis.** Nothing is
sampled from the instrument.

One honesty constraint shapes them. Which byte carries which EFX control is
established for **3 of the XP-60's 40 algorithms**
([`DEVICE_ACCEPTANCE.md`](../DEVICE_ACCEPTANCE.md) area 8), so EFX drawings are
keyed to the algorithm's *identity*, which is verified, and never to
`common.efx_parameter_*`, which is not. Chorus and Reverb drawings do read real
values, because those parameters are supported and carried by the model. A
focused EFX says plainly that its parameter mapping is unverified and points at
Expert for the raw bytes, rather than labelling bytes with control names the
project cannot justify.

### Motion

Restrained and only ever explanatory: a single small dot travels **configured,
open** paths while live audition is running; rail weight animates when a send
changes; the micro-drawing crossfades when an algorithm changes; focus
transitions preserve position. All of it is off under `Motion.reducedMotion`.
There is no constant decorative animation.

## Compact mode

In the Effects section and in Expert the canvas is the workspace. In the other
sections it is a strip — same topology, same lanes, shorter tiers, no chips —
so the mixer and envelope below keep their room. The previous view behaved this
way and the layout depends on it.

## Architecture

QML positions, weights and animates. It decides nothing about Roland.

| Owned by C++ | Owned by the canvas |
|---|---|
| topology (`editor.routing.edges`) | tiers, lanes, chip placement |
| levels, ranges, display text (`editor.effectValues`) | rail class and weight |
| which destinations are legal (`choices`) | which node a choice denotes |
| undo, A/B, live audition, gestures | focus and isolation state |

Components: `EffectsCanvas`, `EffectProcessorNode`, `EffectRouteChip`,
`EffectSignalRail`, `EffectMicroViz`. Parameter editing reuses the existing
`EffectParameterControl` and `EffectChoiceControl`, so exact/raw precision in
Expert is unchanged.

## Verification

`tests/qml/tst_EffectsCanvas.qml` covers the claims rather than the pixels: the
spine outranks every send, the three spine stages share one centreline, every
route value carries a name, a chip edits at full raw resolution, focus keeps the
topology visible, isolation lights one path and explains it, and routing offers
only destinations the XP-60 allows — including that `<OUTPUT-2>` and `MIX+REV`
are never drop targets.
