# XP60Studio UI Compliance Report

**Date:** 2026-09-08 (pass 3)  
**Guidelines:** [`SYNTHESIZER_UI_AGENT_GUIDELINES.md`](SYNTHESIZER_UI_AGENT_GUIDELINES.md)  
(adapted from Synthesizer UI Agent Guidelines v2.4.1)  
**Evidence shots:** `shots/ui-audit/` (regenerated this pass)  
**Tests:** full `ctest` suite — 71/71 passed

---

## Verdict

Sound-shaping surfaces in the **Editor** and **Performance** inspector now follow
Directive Alpha (knobs / graphs / ranges / chips first; exact entry secondary).
Librarian, Devices, Expansion, and Bank Builder correctly keep text controls for
textual/device tasks. Every deviation from the master mockup that survives is
listed under *Deliberate deviations* with the hardware-truth or roadmap reason
for it.

Overall status: **PASS**. Remaining items are deferred by hardware truth or by
roadmap phase, not by an unmet directive.

---

## Guideline adaptations (project-scoped)

| PDF directive | XP60Studio mapping |
|---|---|
| Zero text-entry for sound shaping | `MusicalParamKnob`, envelope graph, `KeyboardStrip`, `XpRangeBar`, chips, `TonePlaySettingsStrip` |
| 30 fps DSP subscription | Immediate visual feedback + live temp audition (no local DSP) |
| Wavetable 3D morph | Wave Browser + Tone wave identity / availability (PCM/EXP truth) |
| Modulation rings / DnD matrix | Deferred — use LFO selector + Effects workbench; no invented destinations |
| MIDI CC learn on knobs | Future; not faked |
| React/HTML forms | Forbidden — Qt Quick / QML only |

Agent rule: `.cursor/rules/synthesizer-ui.mdc`  
Pointer in `AGENTS.md` Required reading + interaction-first section.

---

## Screen-by-screen

### Dashboard — PASS
- Hero / summary cards / Tone contribution; no form-first sound shaping.
- Shot: `shots/ui-audit/dashboard.png`

### Devices — PASS (allowed forms)
- Port pickers, device ID, diagnostics — textual by nature.
- Shot: `shots/ui-audit/devices.png`

### Editor · Design · Sound — PASS (core target)
| Area | Status | Notes |
|---|---|---|
| Four Tone cards | Pass | Level / Pan / Octave knobs (octave is a snapped bipolar dial, −4…+4) |
| Pitch envelope graph | Pass | Interactive nodes |
| Envelope Time/Level | Pass | Compact knobs (replaced steppers) |
| Key range | Pass | Keyboard primary; Exact notes secondary |
| Velocity | Pass | Range bar primary; Exact velocity secondary |
| Tone Sound (Pitch) | Pass | Knobs + discrete knobs for long enums |
| Tone Settings | Pass | Bend wheel + portamento glide + POLY/SOLO, third module in the Design row |
| Signal path | Pass | Effects canvas |
- Shots: `editor-sound.png`, `editor-filter.png`, `editor-amp.png`, `editor-motion.png`, `editor-effects.png`, `editor-play.png`, `editor-expert.png`, `editor-min.png`

### Wave Browser — PASS
- Overlay from Editor; search + result list + availability (catalog task).
- Shot: `shots/ui-audit/waves.png`

### Library — PASS (allowed forms)
- Search / filters / transfer cards; details may use text for metadata.
- Shot: `shots/ui-audit/library.png`

### Expansion — PASS (allowed forms)
- Board declaration / slot identity; spin fields for IDs are appropriate.
- Shot: `shots/ui-audit/expansion.png`

### Bank Builder — PASS
- Visual bank grid / panel display / drag; name fields for save dialogs OK.
- Shot: `shots/ui-audit/banks.png`

### Performance — PASS
| Area | Status | Notes |
|---|---|---|
| 16-part mixer | Pass | Faders + receive switches |
| Part inspector sends | Pass | MIDI Ch / Chorus / Reverb / Voices / Octave knobs |
| Part key range | Pass | `KeyboardStrip` primary |
| Strip cho/rev/vcs rows | Pass | Mini level meters + value; voice reserve 0 flags in `Theme.warning` |
| Part output assign | Pass | Five documented destinations as chips (was a read-only label) |
- Shots: `shots/ui-audit/performance.png`, `performance-min.png`

---

## Directive checklist

| Directive | Status |
|---|---|
| Alpha — no primary text for continuous sound params (Design) | Met |
| Alpha — exact entry secondary + testable `objectName`s | Met |
| Beta — visual feedback on edit | Met |
| Beta — no fake DSP meters / wavetable morph | Met |
| Envelopes as graphs + compact stage controls | Met |
| High-contrast dark theme + Tone colours | Met |
| Forgiving knob hitboxes (~20%) | Met (`MusicalParamKnob`) |
| Play / Design / Expert progressive disclosure | Met |
| Hardware truth (no invented envelope curves / CC learn) | Met |

---

## Pass-3 — density and module hierarchy

Reference for this pass: a Serum 2 screenshot supplied by the user, read for its
*interaction language* only. Nothing about the XP-60's parameter set changed —
no wavetable, no macros, no modulation matrix. What was worth taking is how a
dense instrument organises a screen.

| Serum idiom | Applied here |
|---|---|
| Every module opens with a tinted header band carrying a lamp, a name and its own inline controls | New `XpModuleHeader`; replaced every `XpPanelHeader` on the sound-shaping surfaces |
| Caption silk-screened under the control, not floating above it | `MusicalParamKnob` reads dial → name → value |
| Modules sit abreast in rows, not stacked down a scroll | Editor's Design work is one row of three — Envelope, Key Range + Velocity, Tone Settings — with the parameter grid beneath |
| Values live in the module header, not repeated under the graphic | Key Range, Velocity and Part key range show their pair in the band |
| Panels are flush; whitespace is not a separator | Card padding on parameter panels down from 16 to 8; standing prose removed from Design |

Measured: the SOUND section went from ~1500 px of content to ~1150 px, with a
bigger envelope graph than before, not a smaller one.

Layout rules established while doing it, both worth keeping:

1. **A layout nested in a layout is expanding by default.** The collapsed
   exact-entry rows in the Key Range column were absorbing the column's spare
   height and opening a hole in the middle of it. Only the graphic in a column
   may grow; everything else is pinned with `Layout.fillHeight: false`, and the
   slack collects in a trailing spacer.
2. **Spare height goes to a graphic or to the bottom — never between modules.**
   The envelope graph takes its column's slack, because a taller envelope is a
   better control. The keybed does not: 128 keys in a third of the window is
   already a 2.5 px white key, and stretching it only produced a moiré.

### Regrouping that closed the last of the empty space

The Design row's three columns are naturally different heights, and Key Range
ran out of content ~235 px before the row ended. A spacer would have hidden
that, not fixed it. The grouping was wrong: **Key Assign (POLY/SOLO) belongs
with Key Range and Velocity**, because all three answer what the Tone does when
keys go down, while bend and portamento are what the player does afterwards.
Moved to a new `ToneKeyAssign`; the columns now end flush.

That also made it obvious that Key Range and Velocity were hidden on MOTION for
no reason — they are Tone parameters, true whichever section is being edited.
MOTION went from one full-width module to a two-column row.

Breakpoints are measured on the Design row's own width rather than the window's
(the rail and margins are already out of it): three columns at ≥ 1060, two at
≥ 680, one below. At two columns Tone Settings spans both, so it lays its bend
and portamento modules side by side instead of sitting in a half-empty row.

Also this pass: the pitch-bend wheel and both its knobs follow the selected
Tone's colour instead of a fixed orange/blue pair the instrument never implied;
the Design parameter grid never opens more columns than it has parameters, so
four knobs no longer sit in the left two thirds of a six-column row.

## Pass-2 changes

| Was | Now | Directive |
|---|---|---|
| Knob captions elided to knob width ("Coarse T…", "Cutoff Fr…", "Random…") | `MusicalParamKnob.labelWidth`; captions wrap to two lines and use their cell | Alpha — a control the musician cannot name is not a control |
| Design tone parameters in a ~300 px right rail, 1 column, scrolling | Full-width row under Envelope + Key Range; 4–6 knob columns, no scroll | Alpha / E |
| Design group chosen from a `ComboBox` | Chip row (`parameterGroupChips`); the ComboBox is Expert-only | Alpha |
| Tone card Octave: bordered text box with wheel/arrow keys | Snapped bipolar `XpKnob`, −4…+4, semitone remainder in the readout | Alpha |
| TVA envelope: stage 4 sank half a row (no Level knob) | Stage delegates align top; Time reads as one row | Layout defect |
| Key Range card ended early, ~200 px of dead space beneath | Card fills the row; the keyboard grows to 150 px | E — forgiving hitboxes |
| Motion: two hand-rolled LFO blocks, Rate/Delay only, no depths, no preview | `LfoLane` ×2: drawn shape, chips, Rate/Delay and the four documented depths, coloured by destination | D |
| Performance strip sends: `cho 127 / rev 127 / vcs 0` text rows | Mini meters + value; reserve 0 in warning colour | Alpha |
| Performance inspector: "Output" and "EFX" as two orphaned labels | Destination chips wired to `setPartOutputAssign` | Alpha / layout defect |

Supporting change in C++: `EditorParameterModel::valueTextForId` now falls back
to the Tone table, so a panel may show a documented parameter the current group
or search does not list (the LFO depth knobs need this).

New tests: `test_tone_octave_turns_like_a_knob_and_moves_coarse_tune`,
`test_design_selects_a_parameter_group_with_chips_not_a_dropdown`,
`test_motion_shows_the_documented_lfo_depths_as_knobs`,
`test_part_strip_shows_sends_as_meters_and_flags_no_voice_reserve`,
`test_musical_knob_caption_survives_a_long_documented_name`.

## Deliberate deviations (not defects)

1. **Dashboard Tone contribution is a proportion bar, not the mockup's dB
   meter.** The XP-60 stores Tone Level as 0-127 and the manual gives no dB
   conversion; inventing one would print a number the instrument never said.
   Same reason the envelope stages show raw 0-127 rather than "0.40 s".
2. **Wave Browser shows no waveform art.** The mockup draws a sample
   thumbnail; the XP-60 does not return sample data, and a decorative
   stand-in would be exactly the "fake DSP" Directive Beta forbids.
3. **Expert stays a form/grid.** That is its job — the raw XP surface.
4. **MIDI CC learn and a drag-to-knob modulation matrix are absent.** Both are
   future interaction models; the guidelines forbid faking them before the
   transport/CC layer exists.
5. **Compare and Settings are unavailable screens.** No compliance claim.

## Verification commands

```powershell
# Build + the whole suite
.	oolsuild_windows.ps1

# Just the UI tests
ctest --test-dir build-windows -R "tst_qml|tst_patch_editor" --output-on-failure

# Screenshots
$env:QT_QPA_PLATFORM='offscreen'; $env:QT_QUICK_BACKEND='software'
.\build-windows\xp60studio_screenshot.exe shots\ui-audit\editor.png editor 1440 1400
```

---

## Change summary

**Pass 1** — guidelines + Cursor rule + AGENTS/catalog pointers; envelope stages
and the Design parameter panel to knobs/chips; `TonePlaySettingsStrip`;
key/velocity exact entry behind Exact toggles; Performance inspector to knobs and
a keyboard range.

**Pass 2** — every pass-1 follow-up closed, plus the caption, layout and
LFO-depth gaps found by re-reading the captures against the mockup.

**Pass 3** — knob feel (the dial's own drag mapped the whole range onto the
control's height, and a `QVariantList` Repeater model destroyed the knob
mid-drag), then the density and module-hierarchy pass above.
