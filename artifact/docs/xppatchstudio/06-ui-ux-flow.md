# 06. UI/UX design: an FL Studio-style workflow for the XP-60

Status: design proposal for approval. Nothing is built. Everything here is **original**: it follows FL Studio's design
language and interaction model (docked/floating panels, rack, browser, hint bar, native-plugin controls) and
re-expresses it for a ROMpler workstation. It does **not** follow Patch Base's layout, navigation or visuals (section 11).
FL measurements referenced here live in `D:\Codes\GenosStyleStudio\docs\fl-ui-evidence` (read-only reference).

## 0. The idea in one paragraph

An XP-60 is played and shaped, not filled in. So the app is a **sound workstation**, not a parameter form: you open a
sound, hear it, and move things while it plays. Everything is a small, focused panel you can dock or float; every
control reports its name and value in one always-visible hint bar; every edit is instantly audible (throttled live send),
undoable from a browsable history, and visibly queued. Writing to the synth's permanent memory is a deliberate, guarded
ceremony, never a side effect.

## 1. Workspace anatomy

```
+-----------------------------------------------------------------------------------------------+
| FILE EDIT VIEW SYNTH LIBRARY TOOLS HELP      [XP-60 ● IN ● OUT] [Port v] [Dev ID 17] [sync 0/3]|  menu + device + queue meter
+-----------------------------------------------------------------------------------------------+
| [Hint bar: two lines: context | "Cutoff Frequency: 64"            Ctrl-drag fine, dbl-click reset]|  hint bar
| [PAT|PERF|RHY|SYS] [Patch ▸ U:047 Warm Pad] [A|B] [♪ audition] [Undo ◂ ▸] [Arm write]  [layouts v] |  mode + transport row
+--------------+-------------------------------------------------------+------------------------+
| BROWSER      |  WORK AREA (tiled panels, drag to rearrange, float)   |  RIGHT DOCK            |
| Banks        |  +--- Patch rack ---------------+ +--- Zones -------+ |  History   (undo list) |
|  User  128   |  | 1 ● M S [Lvl][Pan] Wave name | | keyboard strip  | |  Send queue + monitor  |
|  Card        |  | 2 ● M S ...                  | | + 4 tone zones  | |  Inspector (selection) |
|  Preset      |  | 3 ...  4 ...                 | +-----------------+ |                        |
| Folders      |  +------------------------------+ +--- FX / output --+|                        |
| Tags         |  +--- Tone 1: Wave | Pitch | TVF | TVA | LFO | Ctrl --+ |                        |
| Favourites   |  |  knob panels + draggable envelope graphs          | |                        |
| Imported     |  +---------------------------------------------------+ |                        |
+--------------+-------------------------------------------------------+------------------------+
| status strip: MIDI activity · last response · state chip [TEMP ✓ verified] · memory area       |  33 px
+-----------------------------------------------------------------------------------------------+
```

Geometry follows measured FL values: two-row top dock (about 41 px per row), caption bars 26 px, system buttons 21 px, side
docks 32 px, 8 px splitters, bottom status strip 33 px, left browser about 205 px.

## 2. Panels, mapped from XP-60 concepts to FL metaphors

| XP-60 concept | FL metaphor | Panel contents |
|---|---|---|
| Patch (4 tones) | **Channel rack** | 4 rows: on LED, mute, solo, level and pan mini-knobs, waveform-name button, mini envelope/zone glyph. Click a row to open that tone's window. |
| Tone sections (Wave, FXM, Tone Delay, Pitch, TVF, TVA, LFO, Controllers, Velocity/Key ranges) | **Plugin window** | Tabs along the top; each tab is a panel of native-style knobs and sliders; envelopes are draggable graphs (time/level points). |
| Patch Common (EFX, Chorus, Reverb, tempo, bend, portamento, key assign, structure) | **Plugin window** | Same tabbed layout; EFX type picker with its 12 generic parameters; structure type diagrams. |
| EFX, Chorus, Reverb, outputs | **Mixer** | Insert-style slots with send amounts, output assign, level meters for parts. |
| Key and velocity ranges (tones, parts) | **Piano roll** | Keyboard on the left; drag coloured rectangles for each tone/part; velocity cross-fade handles. Rhythm set: one row per key with the 58-byte note editor. |
| Performance (16 parts) | **Channel rack (performance mode)** | 16 rows: MIDI channel, patch assignment chip, level/pan, mini key-range glyph, receive switches. |
| Banks User / Card / Preset, slots | **Pattern/playlist selector** | Slot grid (USER 001 to 128 etc.) with occupancy, dirty marks, drag and drop. |
| Patch librarian | **Browser** | Tree of Banks, Folders, Tags, Favourites, Imported `.syx`; search as you type; detail strip. |
| Undo | **History panel** | Browsable list; click a row to return to that state. |
| System, Scale Tune | **Settings plugin window** | Grouped knobs and drop-downs. |
| Hint | **Hint bar** | See section 4. |

Every panel can be **docked, tabbed, floated, detached to another monitor**, and a workspace layout is savable and nameable.
Presets shipped: *Sound design* (rack + tone window + zones), *Performance* (16-part rack + mixer), *Librarian* (browser large).

## 3. The control kit (built first, shown in a storybook window)

Knob, MiniKnob, Slider, Led, StepButton, EnvelopeGraph, KeyRangeBar, Meter, HintBar, DockPanel, plus Segmented toggle,
Dropdown tile, Stepper, Group frame, Caption bar, Menu.

Measured FL native construction (re-drawn from scratch as vectors):

- **Knob**: full dark track ring that carries the coloured value arc, flat cap lighter at the top rim and darker towards
  the bottom, dark outline, a small dot for the position, a notch above the ring for bipolar origin, condensed upper-case
  label beneath. Diameters 20, 30, 36/38, 64 px. No numeric readout box on the control: the value appears in the hint bar.
- **Group**: outlined frame with a slanted-edge tab title; section titles mixed-case.
- **LED toggle** with a label; segmented text toggle for two or three modes; stepper for small integers; drop-down tile.

Behaviour (FL-measured ratios, XP-defined absolutes):

| Gesture | Result |
|---|---|
| Vertical drag | value change; up increases |
| Ctrl+drag (or Shift) | fine, about 0.28x of a drag |
| Mouse wheel | one notch is about 13.5 px of dragging; Ctrl for fine |
| Double-click or Alt+click | reset to default |
| Right-click | menu: Type value, Copy value, Paste value, Link to MIDI CC, Reset, Add to favourites |
| Hover | name and value in the hint bar |
| Esc | close the focused floating window |

Because XP-60 values are small integers with units (for example 0 to 127, 0 to 15, notes), the hint bar shows both the
**raw value** and the **meaning** ("Cutoff Frequency: 64 (raw 64)", "Octave: +1"). Exact entry is always one right-click away.

## 4. The hint bar and feedback system

- Two lines: dim context ("Tone 2 > TVF"), then `Name: value` of the hovered control, right-aligned shortcut.
- **MIDI activity LEDs** (in and out) with a tooltip-free click that opens the monitor.
- **Send queue meter**: shows pending messages and the current pace; turns amber when behind, red on timeout.
- **State chip** for the open sound, always visible:

| State | Meaning |
|---|---|
| LOCAL | exists only in the app or library, not on the synth |
| EDITED | changed since last fetch/send/save |
| SENT | transmitted to the synth's temporary area, not yet read back |
| TEMP VERIFIED | temporary area read back and equal |
| STALE | the synth's temporary area changed on the panel behind our back (patch selected on the keyboard) |
| SAVED | written to a User slot **and** read back equal |
| MISMATCH / FAILED | read-back differs, a block timed out, or the synth refused (for example User Memory Protect) |

## 5. Core flows

1. **First run and connect.** Welcome panel: choose MIDI in/out, device ID (default 17), then *Connect*. The app requests one
   small block (patch name) as a handshake, shows the answer in the hint bar, and only then enables fetch/send. Wrong
   device ID shows a clear "no reply" with the one-click fix (try IDs 17 to 32). No modal walls.
2. **Fetch and live-edit.** *Synth > Fetch current sound* reads the temporary patch (request one block at a time, sparse
   replies handled), shows progress in the queue meter, fills the rack. Moving any control sends a **debounced, throttled
   live edit** to the temporary area; you hear it immediately. Undo/redo groups a whole drag as one step.
3. **Audition.** On-screen keyboard and computer keyboard play notes on the synth; hovering a library patch plays a preview
   by sending it to the temporary area, then **restores** the previous temporary patch when the pointer leaves (with the
   stale-state guard from section 4).
4. **Compare.** A/B toggle between the original and the edited sound; a diff list shows only changed parameters.
5. **Init, Randomize, Mutate, Morph.** Toolbar tools with a strength knob; randomize draws uniformly inside each
   parameter's range or option list (as the reference behaves) and respects a "protect" list (name, output assign).
   Mutate nudges; Morph blends A and B by a slider. All undoable and previewed live.
6. **Copy and paste.** Copy a tone, a section (for example only TVF), or one parameter; paste to another tone or patch.
7. **Librarian.** Import `.syx` (and standard MIDI files with SysEx), auto-detect patch, performance, rhythm or bank;
   tag, favourite, search; drag a patch onto a slot or a rack. Duplicate detection by content hash.
8. **Safe write to User memory** (the one ceremony):
   *Arm* (explicit toggle, auto-disarms after use) > pick the destination slot (shows the current occupant's name) >
   automatic **backup** of that slot > write with 128-byte splitting and 20 ms gaps > **read back and compare** > state
   chip becomes SAVED, or MISMATCH with the likely cause. Wording and colour differ from temporary sends everywhere.
9. **Banks and backups.** Bank grid view (slot occupancy, dirty marks), fetch/write whole banks with progress and cancel,
   whole-synth backup to a dated folder, restore with preview, all through the same write ceremony.
10. **Performance and rhythm.** 16-part rack (patch chips, channel, level/pan, key range); rhythm set per-key rows with
    the note editor; both have the same live-edit and undo model.
11. **Recovery.** Timeouts show which block is missing and offer *Retry block* or *Skip*; stale state offers *Refetch*;
    a refused write explains User Memory Protect. Nothing is lost silently; unsent work stays in the history.

## 6. Interaction model

- **Selection and focus**: one focused panel; focused panel caption lights up in the accent colour; keyboard shortcuts act
  on the focused panel.
- **Shortcuts**: fully keyboard drivable, with an **editable shortcut map** (searchable table). Defaults: F-keys toggle panels
  (browser, rack, tone window, zones, mixer, history), Ctrl+Z/Y undo/redo, Space audition, Ctrl+R randomize, Ctrl+D init,
  Ctrl+B/Ctrl+N A/B, Ctrl+K command palette.
- **Command palette**: fuzzy search over every command and every parameter ("tvf cutoff"), opens its panel and focuses it.
- **MIDI learn**: right-click > Link to MIDI CC; the map is a table with channel, control, range and encoding; NRPN aware.
- **Context menus** everywhere, FL-style, with the same items as the toolbar so nothing is mouse-only.

## 7. Visual language (original, FL-measured)

- Dark graphite neutrals sampled from FL native plugins (body `#363C40`, caption `#434C51`, graph well `#46565A`, hint
  `#3E464B`, text `#D3D7DE`) with **one warm-orange accent** for active toggles and value arcs, plus status colours
  (green LED `#97DB3A`, amber, red). Tone identity colours (4) are used on tone rows, zone rectangles and knob arcs.
- Flat panels, 1 px edges, 2 to 4 px radii, subtle vertical gradients only on caps and key faces; compact density; small-caps
  condensed labels (a system condensed face such as Bahnschrift SemiCondensed).
- Everything drawn as vectors, crisp at 100% to 200% scale. No Image-Line assets, logos or names.

## 8. Layouts and workspaces

Dock, tab, float, split and detach; layouts savable and nameable; per-monitor memory; a *Reset layout* command; first run
opens *Sound design*. Minimum window 1,100 x 700 with graceful docking collapse; floating windows remember size/position.

### 8.1 As built (owner direction, 2026-11-04)

The owner asked for **no scroll bars** and for functions grouped logically, with one-time set-up in menus. The layout is
therefore a fixed window (menu bar, workspace tabs, stage, hint bar) in which every workspace is split into **tab strips**
instead of dock panels that stack and scroll:

- Menu bar: File (New, Open .syx), Edit (undo/redo, Init, Randomize, Mutate, A/B, copy/paste), Synth (ports, device ID,
  connect, rescan), View (command palette and shortcuts, theme). Connection errors also show in the top bar.
- Patch: Tones always visible; main tabs Zones, Wave, Pitch, Filter, Amp, LFO, Output, Modulation, Patch, Effects,
  Chorus/Rev, Controllers; side tabs Play, Tools, History. Performance: Parts 1-8 / 9-16 plus detail tabs. Rhythm set:
  keyboard plus key tabs. System: one tab per section plus Scale tune. Bank: four 32-slot tabs. Library: groups as tabs.
- Saved layouts are built: the open workspace and the tab shown in each workspace are remembered between runs, and the Layouts menu
  saves them under a name (up to 12), applies, deletes and resets them; four layouts are built in (Sound design, Key zones,
  Performance rack, Librarian). Stored layouts are read defensively and a tab index is clamped to tabs that exist.
- Free docking and floating windows remain unbuilt: parity row F1 stays PARTIAL.
- Piano keys follow the measured FL piano-roll geometry (natural faces 1.5/2/2/1.5/1.5/2/2 rows, one-row accidentals at
  two thirds depth, shaded C faces, separators only at C and E/F, accidental-first hit test); no note grid is drawn.

## 9. Performance and trust of the UI

Live edits are throttled and coalesced (never one message per pixel); the queue meter always reflects reality; large
operations are cancellable; no UI thread blocking on MIDI; unsent state is never discarded.

## 10. Accessibility

Every control is keyboard reachable with visible focus, exposes name, value and range to assistive tech, and respects
system text scale; colour is never the only signal (the state chip always carries words); hover-only actions have a keyboard
equivalent.

## 11. What we deliberately do differently from Patch Base

Patch Base is a per-module editor with toolbar-driven windows. XPPatchStudio instead: has a **single sound-centric
workspace** with dockable panels; a **persistent hint bar** and send-queue meter; **hover audition** in the browser; a
**browsable undo history**; a **command palette**; and a guarded **write ceremony** with automatic backup and read-back.
Layout, navigation and visuals are our own; no Patch Base text, icons or screens are reused.

## 12. Build order for the UI (maps to delivery phases)

| Phase | UI deliverable |
|---|---|
| 3 | Theme tokens, full control kit, storybook window showing every control in every state, docking panel host, hint bar |
| 4 | Patch rack, tone and common windows, envelope graphs, zones editor, history panel, live-edit pipeline |
| 5 | Performance rack, rhythm set grid, system window, mixer/FX panel |
| 6 | Browser/librarian, slot grid, backups, write ceremony |
| 7 | Compare, init, randomize, mutate, morph, copy/paste, command palette, shortcut editor |

## 13. Questions for the owner

1. Default accent: warm orange as in the brief, or follow FL's exact orange more closely?
2. Is the XP-60 only (single module) the right scope, or should the architecture allow other JV/XP models later?
3. Should hover-audition be on by default (it changes the synth's temporary patch) or opt-in?
4. Light theme: needed, or dark only?
