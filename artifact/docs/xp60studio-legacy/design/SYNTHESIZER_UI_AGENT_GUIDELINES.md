# XP60Studio Synthesizer UI Agent Guidelines

Adapted from *Synthesizer UI Agent Guidelines* v2.4.1 for this repository.
Hardware truth and the approved mockup still govern product behavior; these
rules govern **interaction language** for sound-shaping surfaces.

Canonical visual target: [`xp60studio-ui-master-mockup.jpg`](xp60studio-ui-master-mockup.jpg)  
Also read: [`UI_IMPLEMENTATION_ARCHITECTURE.md`](UI_IMPLEMENTATION_ARCHITECTURE.md),
[`COMPONENT_CATALOG.md`](COMPONENT_CATALOG.md), [`UI_ACCEPTANCE_CRITERIA.md`](UI_ACCEPTANCE_CRITERIA.md),
and the interaction-first section of [`../AGENTS.md`](../../AGENTS.md).

---

## Role

You are designing and reviewing UI for a **Roland XP-60 sound workstation**
(editor, librarian, bank builder), not a generic CRUD app and not a web form.

Stack: **C++20 + Qt Quick / QML**. Do not introduce React, HTML forms, Widgets,
or Electron. QML must never construct Roland SysEx.

---

## Directive Alpha — Zero text-entry fallback (sound shaping)

Never default to `TextField`, `SpinBox`, `ParameterValueEditor`, or `XpSpinField`
as the **primary** control for continuous or musically meaningful parameters.

| Prefer first | Exact entry only as secondary |
|---|---|
| `XpKnob` / `MusicalParamKnob` | Double-click / readout toggle → `ParameterValueEditor` |
| Envelope graph nodes (`EnvelopeEditor`) | Stage knobs under the graph |
| `KeyboardStrip`, `XpRangeBar` | Soft/Hard / Low/High exact fields behind disclosure |
| Choice chips / `LfoShapeSelector` / POLY·SOLO cards | ComboBox in Expert or Exact |
| Portamento glide viz (`TonePlaySettingsStrip`) | Raw Portamento Time in Expert |

Allowed primary text/spin usage (not sound shaping):

- search boxes, patch name (12 ASCII), file paths
- device IDs, MIDI port pickers, SysEx address diagnostics
- Expert/Diagnostics surfaces whose job is raw XP values
- librarian metadata that is inherently textual

**Reject** Design-mode panels that are mostly labels + steppers/dropdowns.

---

## Directive Beta — Sensory feedback (XP-60 adapted)

There is no local DSP clock. Feedback means:

1. **Immediate visual response** when a parameter changes (knob arc, envelope
   polyline, range highlight, status pills, transfer progress).
2. **Live audition** to the instrument temporary area when armed — never silent
   USER writes as a side effect of editing.
3. Motion only when it explains change, focus, or progress (`Motion` tokens).
4. Do not invent fake audio meters or wavetable morphing the XP-60 cannot feed.

---

## Component patterns (project mapping)

### A. Rotary encoder → `XpKnob` / `MusicalParamKnob`

- Vertical drag; Shift for fine steps; keyboard arrows; double-click / Home resets.
- Arc ring shows normalised value; bipolar knobs centre at musical zero.
- Hit target: enlarge interactive area ~15–20% beyond the drawn knob when space allows.
- MIDI CC learn overlays are **future** — do not fake hardware-map menus until
  the transport/CC layer exists.

### B. Multi-stage envelopes → `EnvelopeEditor` + stage knobs

- Graph is primary; Time/Level knobs are compact companions.
- Do **not** invent inter-node curvature the XP-60 envelope model does not expose.
- Pitch envelope levels are bipolar; TVF/TVA follow documented raw ranges.

### C. Timbre visibility → Wave Browser + Tone cards

- XP-60 uses PCM/expansion waves, not morphing wavetables.
- Show wave identity, source (INT/EXP), availability, and expansion requirements —
  never a decorative static “waveform art” that pretends to be the sample.

### D. Modulation / routing

- Prefer existing LFO shape selector, depth knobs, and Effects canvas/workbench.
- Drag-source → drop-on-knob modulation matrix is a **future** interaction model;
  do not invent undocumented LFO destinations. When adding modulation UI, colour
  arcs by Tone/source semantics already in `Theme`.

### E. Live performance constraints

- Dark surfaces (`Theme`), high-luminance accents, Tone 1–4 colour identity.
- Forgiving hitboxes on knobs, envelope nodes, keyboard range handles.
- Group by Tone / Part / bank slot; never dump hundreds of raw parameters on Play.

---

## Play · Design · Expert

| Mode | Surface |
|---|---|
| **Play** | Audition, four Tone mix, safe high-value actions |
| **Design** | Knobs, envelopes, ranges, LFO visuals, portamento glide, effects nodes |
| **Expert** | Exact XP values, uncommon parameters, protocol-adjacent detail |

Exact entry is always available; it must not replace the Design interaction model.

---

## Screens in scope

Dashboard, Devices, Editor, Wave Browser, Library, Expansion, Bank Builder,
Performance, Unavailable. Librarian/device screens may use forms for textual
tasks; musical controls still win wherever the concept is continuous or spatial.

---

## Agent output protocol (this repo)

When designing or redesigning a control:

1. Name the musician task (e.g. “set portamento glide”, not “edit offset 57”).
2. Prefer an existing catalog component; extend rather than one-off rectangles.
3. Keep exact entry secondary with stable `objectName`s for QML tests.
4. Capture screenshots via `xp60studio_screenshot` and compare to the mockup /
   these directives.
5. Never claim hardware verification without device evidence.

Do **not** emit React/HTML form specs as the implementation path.

---

## Hard refusals

- Stock Qt look for primary musical controls
- Invented XP-60 parameters, curves, or SysEx to decorate the UI
- Silent USER-memory writes from editing
- QML constructing Roland addresses/checksums
