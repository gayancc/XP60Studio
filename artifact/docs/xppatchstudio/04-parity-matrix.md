# 04. Parity matrix

UI/UX flow for every row that has a UI: `06-ui-ux-flow.md`.

Status values: **DONE** (no instrument involved, or accepted on hardware), **IMPLEMENTED, needs hardware** (built and tested against the virtual XP-60 and fixtures, waiting for the ledger item named), **PARTIAL** (part built, the rest stated), **OPEN** (not built), **PROPOSED WON'T-DO** (needs owner approval).
Audited at the end of phase 7 (2026-11-04).
Behaviour specs recovered from the binaries are in `05-behaviour-spec.md` (rows A2, A3, B3, B4, B7 and B10 now have a written spec,
still NOT STARTED and unverified on hardware).

Instrument-dependent rows are accepted only through the hardware ledger (`07-hardware-acceptance.md`), run as one phase at the end.

Per the project rule no SysEx-dependent row becomes **DONE** until the owner confirms it on a physical XP-60.
Evidence codes refer to `02-feature-catalog.md` (CONFIRMED / INFERRED / NOT FOUND = owner-requested).

| Feature ID | Patch Base feature | Evidence | New app module | Phase | Status |
|---|---|---|---|---|---|
| A1 | MIDI in/out port selection | CONFIRMED | xppatch-midi (port discovery), UI top toolbar | 2, 4 | IMPLEMENTED, needs hardware (H-001) |
| A2 | Device ID setup | CONFIRMED | Sysex header builder, Midi settings | 1, 2 | IMPLEMENTED, needs hardware (H-002, H-003) |
| A3 | Throughput: speed, chunk, buffer | CONFIRMED | Sysex chunking/throttling + queue meter | 1, 2 | IMPLEMENTED (needs hardware: H-023, H-050, H-056): the three settings of the bundle are covered by the write gap (20 to 250 ms, shown as frames per second; the speed), the chunk size (16 to 128 data bytes for whole-sound sends; default and maximum 128, the instrument's limit) and the send queue with its meter (the buffer) |
| A4 | NRPN/RPN option | CONFIRMED | Midi transport options | 2 | IMPLEMENTED (needs hardware: H-054): incoming controller messages are decoded as plain CCs, NRPN or RPN (14-bit values from Data Entry MSB/LSB, RPN null honoured) and can be linked like any controller; there is no switch, because decoding is harmless for senders that use plain CCs |
| A5 | Notes from keyboard/interface to synth | CONFIRMED | Midi, UI keyboard | 4 | IMPLEMENTED, needs hardware (Audition panel; H-051c) |
| A6 | Incoming MIDI controls parameters | CONFIRMED | Midi + UI control binding | 4 | IMPLEMENTED (needs hardware: H-054): linked controllers set parameters over their range; one sweep is one undo step |
| A7 | MIDI map / learn | CONFIRMED | Core binding model + UI dialog | 4 | IMPLEMENTED (needs hardware: H-054): right-click a knob, Link to MIDI CC, move a controller; map listed and cleared in the Controllers menu; remembered between runs; Bank Select never linked |
| A8 | Fetch-complete notice | CONFIRMED | Midi request tracker + hint bar | 2 | IMPLEMENTED (needs hardware to see it on a real read): progress, state chip and a six-second notice in the top bar after a fetch ("Fetched ...") or a verify (match or mismatch) |
| A9 | On-screen keyboard | CONFIRMED | UI control kit (keyboard) | 3, 4 | IMPLEMENTED, needs hardware (playable on-screen and computer keyboard; H-051c) |
| B1 | Single-patch editor, rename | CONFIRMED | xppatch-core model + UI Patch view (tone rows, section windows) | 4 | IMPLEMENTED, needs hardware (H-025 to H-029) |
| B2 | Non-bank single-instance editor (system) | CONFIRMED | UI System editor | 5 | IMPLEMENTED, needs hardware for reading and local editing (H-042); writing System memory is OPEN |
| B3 | Fetch from synth | CONFIRMED | Midi request/response, Sysex RQ1 | 2 | IMPLEMENTED, needs hardware (H-010 to H-017) |
| B4 | Send to synth (temp) | CONFIRMED | Midi + Sysex DT1, queue | 2 | IMPLEMENTED, needs hardware (H-020, H-021) |
| B5 | Save to synth slot | CONFIRMED | Midi + Library backup-before-write, confirm dialog | 2, 6 | IMPLEMENTED, needs hardware for patches (H-030 to H-039) |
| B6 | Init | CONFIRMED | Core defaults from parameter map | 7 | IMPLEMENTED, needs hardware with the application's own defaults (H-052) |
| B7 | Randomize | CONFIRMED | Core randomizer (per-parameter rules in map) | 7 | IMPLEMENTED, needs hardware |
| B8 | Morph | INFERRED | Core morph (two-patch blend) | 7 | IMPLEMENTED, needs hardware |
| B9 | Mutate | NOT FOUND, owner-requested | Core mutate | 7 | IMPLEMENTED, needs hardware |
| B10 | Undo/redo + browsable history | CONFIRMED (+ owner extension) | Core command stack + UI history panel | 4 | DONE |
| B11 | Switch to other section of a synth | CONFIRMED | UI navigation | 4 | DONE (workspace tabs, palette jump) |
| B12 | Overview panel | CONFIRMED | UI (FL-style summary panel) | 4 | DONE (no instrument involved): the Overview side tab shows the patch at a glance (name, level, structure, effects, and per Tone: on or off, wave, level, key and velocity range), follows every edit and selects a Tone on click |
| B13 | Copy parameters / names to clipboard | CONFIRMED | UI + Core serialisation | 7 | DONE (no instrument involved): copy and paste inside the app, and Edit > Copy as text puts every parameter of the open sound on the clipboard as readable text |
| B14 | Copy/paste sections and tones | NOT FOUND, owner-requested | Core + UI | 7 | DONE |
| B15 | Compare edited vs original | NOT FOUND, owner-requested | Core diff + UI A/B | 7 | DONE (A/B and Changes since opened) |
| B16 | Arpeggiator parameters | INFERRED | Parameter map | 4 | IMPLEMENTED, needs hardware (System arpeggio group) |
| C1 | Bank editor | CONFIRMED | Library + UI bank/slot grid | 6 | IMPLEMENTED, needs hardware (Bank workspace slot grid; H-048) |
| C2 | Fetch bank | CONFIRMED | Midi bulk fetch with progress/cancel | 2, 6 | IMPLEMENTED, needs hardware (Bank workspace, progress and cancel; H-015, H-048) |
| C3 | Write bank | CONFIRMED | Midi bulk write, verify, backup first | 2, 6 | IMPLEMENTED, needs hardware (backup, arm, write, read-back; H-032, H-049, H-050b) |
| C4 | Load bank from file (preview) | CONFIRMED | Library import + preview | 6 | DONE (no instrument involved): a picked or dropped .syx or .mid is read and its patch names are shown first; the Library imports only after Import is pressed, the Bank grid shows the loaded bank unsent, Open takes its first patch and lists the rest |
| C5 | Whole-synth backup editor | CONFIRMED | Library backups | 6 | IMPLEMENTED (needs hardware: H-057): Back up all memory reads the 128 User patches, 32 User Performances and 2 User Rhythm Sets into one `.syx` (read only; a slot that cannot be read ends the run), every bank write saves all 128 first, and Bank > Backups browses the files; System settings are not included |
| C6 | Group "refs" fetch/send | CONFIRMED | Midi bulk + Library sets | 6 | IMPLEMENTED (needs hardware: H-048 to H-050b, H-057): the three things the bundle's group does are all there, as a bank: fetch all (Read from synth), keep the group (Add all to library files the 128 patches in a dated Library folder, repeats filed but not stored twice), and send all and save to synth (the Bank write, backup first); the bundle's separate object that holds references is not modelled, because folders do the same job |
| C7 | Multi-part browser | CONFIRMED | UI Performance part rack | 5 | IMPLEMENTED, needs hardware (H-040, H-044) |
| D1 | Patch browser tree | CONFIRMED | UI Browser panel | 6 | DONE (library list) |
| D2 | SysEx outline with folders | CONFIRMED | Library folders | 6 | DONE (no instrument involved): folders are tags with a path ("Pads/Warm"); the Library shows the top folders as tabs, a trail and the folders inside the open one, and filters by path prefix (a patch can sit in several folders); no separate folder store |
| D3 | Open/import `.syx` | CONFIRMED | Library import, Sysex parser | 6 | IMPLEMENTED, needs hardware |
| D4 | Browser search | CONFIRMED | Library SQLite index | 6 | DONE |
| D5 | Storage location setting | CONFIRMED | Library settings | 6 | DONE (software only, no instrument involved): Settings menu picks the folder; library and backups are copied in, nothing moved or deleted; applies at next start; checked end to end in the desktop app |
| D6 | Hover/preview send | INFERRED (+ owner extension) | Midi temp send + restore | 6 | IMPLEMENTED (needs hardware: H-055): Hear on hover in the Library, off by default, temporary patch only, the previous sound restored when the pointer leaves |
| D7 | `.mid` import/export | NOT FOUND, owner-requested | Library | 6 | IMPLEMENTED (needs hardware: H-053): standard MIDI file carrying the SysEx messages, read and written in the Library, the Bank and Open; default reading of owner question G2, easy to change |
| D8 | Tags, favourites | NOT FOUND, owner-requested | Library | 6 | DONE |
| D9 | Drag and drop | CONFIRMED | UI | 6 | DONE (no instrument involved): drag in the Bank workspace (slot to slot, library to slot), and drop a .syx or .mid anywhere in the window (Patch opens it, Library previews it for import, Bank loads it; other workspaces say where to drop) |
| D10 | Backup files | CONFIRMED | Library backups | 6 | DONE (no instrument involved): backup files are written before every save, and Bank > Backups lists them (newest first) and opens one in the bank grid; sending one back uses the normal write steps |
| E1 | Per-module editor manager | CONFIRMED | UI (XP-60 only, single module; WON'T-DO multi-module proposed) | n/a | PROPOSED WON'T-DO |
| E2 | Toolbar customisation | CONFIRMED | UI workspace layouts replace this | 3 | PROPOSED WON'T-DO: the tabbed workspaces and menus replace a customisable toolbar (needs owner approval) |
| E3 | Find/replace in lists | CONFIRMED | UI | 6 | DONE (no instrument involved): search, and find and replace in the shown patch names with a preview; results that are too long, empty or not plain text are skipped and named |
| E4 | Light/dark/system appearance | CONFIRMED | UI theme (dark graphite required; light optional) | 3 | DONE (dark graphite default, light theme switch) |
| E5 | Licensing, trial, updates | CONFIRMED | n/a | n/a | PROPOSED WON'T-DO |
| F1 | FL-style dockable/detachable panels + saved layouts | owner-requested | UI DockPanel | 3 | PARTIAL: tabbed, scroll-free workspaces and menus (docs/06 section 8.1), the window reopens as it was left, and named layouts can be saved, applied, deleted and reset (four built in); no free docking or floating windows, which would need a different panel system |
| F2 | Hint bar | owner-requested | UI HintBar | 3 | DONE |
| F3 | Editable shortcut map | owner-requested | UI | 4 | DONE |
| F4 | Right-click value menu, link to CC | owner-requested | UI control kit | 3 | IMPLEMENTED (needs hardware: H-054): type, copy, paste, reset and link to a MIDI controller on every knob; the link entry stays disabled in the browser preview, which has no instrument |
| F5 | Send-queue sync meter | owner-requested | Midi queue + UI | 2, 3 | DONE |
| F6 | Performance 16-part rack | owner-requested | Core/Sysex/UI | 5 | IMPLEMENTED, needs hardware (H-040, H-044, H-047) |
| F7 | Rhythm-set per-key view | owner-requested | Core/Sysex/UI | 5 | IMPLEMENTED, needs hardware (H-041, H-045, H-046) |

Every SysEx-dependent row (A2, A3, B1 to B7, B16, C1 to C7, D3, D6, F6, F7) now has a **draft address map**
(`03-xp60-protocol.md`, `03a-parameter-map-draft.md`) cross-checked against a real XP-60 bank. They still need the
owner's hardware confirmation.

Status count (2026-11-04, 54 rows): 19 DONE, 31 IMPLEMENTED needing hardware, 1 PARTIAL, 0 OPEN, 3 PROPOSED WON'T-DO.
