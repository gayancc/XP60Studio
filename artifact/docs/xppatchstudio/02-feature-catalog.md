# 02. Feature catalog (XP-60 / JV-family editor and librarian)

Phase 0 recon. Every row says **where the evidence comes from**:

- **CONFIRMED**: a label, window, menu item or resource in the bundle names it.
- **INFERRED**: implied by type names or structure; behaviour not read.
- **NOT FOUND**: no trace in the bundle. Rows from the owner's brief that are not in the bundle are listed so
  the matrix can mark them *owner-requested*.

Edge cases are our own analysis (design requirements), not claims about Patch Base.

## A. Connection and MIDI

| ID | Feature | Inputs → outputs | Edge cases to handle | Confidence |
|---|---|---|---|---|
| A1 | MIDI input and output port selection, per module | port list → chosen in/out pair stored per editor | port disappears while editing; two modules sharing a port; refresh list | CONFIRMED (Prefs, My Modules: "MIDI In/Out") |
| A2 | Device ID setup | integer → used in every SysEx header | wrong ID gives silence, not an error; default 0x10 is the observed sample value | CONFIRMED for existence (strings "device id"); default INFERRED from samples |
| A3 | SysEx throughput control: speed (bytes/sec), chunk size, buffer size, per interface | numbers → paced writes | too fast overruns the synth; too slow feels hung; per-interface values | CONFIRMED (Prefs labels) |
| A4 | NRPN/RPN-aware interface option | checkbox | only for interfaces that send NRPN/RPN | CONFIRMED |
| A5 | Send computer-keyboard/interface notes to the current synth | toggle | note stuck-off on port change | CONFIRMED (Prefs text, "Computer MIDI Keyboard" menu) |
| A6 | Use incoming MIDI to control editor parameters | toggle + map | feedback loops | CONFIRMED |
| A7 | MIDI Map / learn: bind a controller (channel, control, min, max, encoding, target path) to a parameter | learn mode → table rows | encodings differ (7-bit CC, NRPN); range clamp; duplicates | CONFIRMED (MidiMap, Learn/Toggle Learn) |
| A8 | Fetch-complete notification | event → notice | fetch cancelled or timed out | CONFIRMED |
| A9 | In-app on-screen keyboard to audition | toggle | polyphony, channel | CONFIRMED (toolbar, "Show Keyboard") |

## B. Editors

| ID | Feature | Inputs → outputs | Edge cases | Confidence |
|---|---|---|---|---|
| B1 | Single-patch editor: edit every parameter, rename | UI edits → patch model | name length/charset (XP names are fixed-width ASCII); range clamps | CONFIRMED |
| B2 | Editor for single-instance (non-bank) data such as global/system | same toolbar as B1 | settings that apply immediately vs on save | CONFIRMED (Nonbankable editor) |
| B3 | Fetch current data from the synth into the editor | request → response blocks | timeout, partial blocks, checksum failure, wrong device ID | CONFIRMED |
| B4 | Send current editor data to the synth (temporary area) | blocks → synth | throttling, order of blocks, verify by re-fetch | CONFIRMED ("Send") |
| B5 | Save to a chosen synth location (permanent slot) | slot picker → write | **destructive**: needs confirmation and prior backup | CONFIRMED (Basic Synth Save: name + destination) |
| B6 | Init/initialise the data to defaults | command → default values | undoable; per-section vs whole patch | CONFIRMED |
| B7 | Randomize | command → random values in range | respects per-parameter exclusions (e.g. output assign, names); undoable | CONFIRMED |
| B8 | Morph between two sets of values | two patches + amount → blended | integer rounding; non-numeric params | INFERRED (morph symbols in Roland/PBAPI; scope unknown) |
| B9 | Mutate (small random drift) | amount → nudged values | n/a | NOT FOUND (owner-requested) |
| B10 | Undo / redo of edits | stack | grouping drags into one step | CONFIRMED (undo/redo handlers) |
| B11 | Switch to the "other section" of a synth from an editor | toolbar item | n/a | CONFIRMED |
| B12 | Overview (a condensed whole-patch panel) | menu item | n/a | CONFIRMED ("Overview") |
| B13 | Copy parameters / copy patch names to the clipboard | commands | n/a | CONFIRMED |
| B14 | Copy/paste of sections or tones between patches | selection | tone-to-tone with different structure types | NOT FOUND as such (owner-requested) |
| B15 | Compare edited vs original | toggle | n/a | NOT FOUND (owner-requested) |
| B16 | Arpeggiator/keyboard-related parameter support | parameters | n/a | INFERRED (arpeggio symbols in PBAPI) |

## C. Banks, backups and sets

| ID | Feature | Inputs → outputs | Edge cases | Confidence |
|---|---|---|---|---|
| C1 | Bank editor: collection of patches, rename, reorder | UI → bank model | bank size per memory area; empty slots | CONFIRMED |
| C2 | Fetch a bank from the synth | request × N patches → bank | progress, cancel, partial failure | CONFIRMED ("Fetch Bank") |
| C3 | Write a bank to the synth | bank → N writes | **destructive**; verify; order | CONFIRMED ("Write Bank to Synth") |
| C4 | Load a bank from a file into the editor (preview first) | file → preview → bank | wrong model; truncated file | CONFIRMED ("Load Bank") |
| C5 | Whole-synth backup editor | fetch/send groups, preview and load | large transfers; resume | CONFIRMED (Backup editor, "Load Backup") |
| C6 | "Refs" group fetch/send (fetch all, send all, save to synth) | group of references | partial group failure | CONFIRMED (Full Ref editor) |
| C7 | Multi-part data browser ("Parts") for multitimbral data | part list | 16 parts | CONFIRMED (PatchPartBrowser) |

## D. Librarian (files)

| ID | Feature | Inputs → outputs | Edge cases | Confidence |
|---|---|---|---|---|
| D1 | Patch browser: banks, patches, directory tree | files → tree | duplicates; unknown files shown as "Unknown" | CONFIRMED |
| D2 | Stored SysEx outline with folders and create-folder | file system → outline | rename/move on disk | CONFIRMED |
| D3 | Open `.syx` files (registered file type); double-click opens/loads | file → patch/bank | multi-message files; mixed models | CONFIRMED |
| D4 | Search within the browser | text → filtered list | large libraries; index freshness | CONFIRMED (search strings, count 42 in PBCore) |
| D5 | Local file storage location setting ("File Storage: Local") | path | missing folder | CONFIRMED |
| D6 | Preview a patch by sending it to the synth | hover/select → temp send | state restore of the previous temp patch | INFERRED (preview strings) |
| D7 | Export/import `.mid` | n/a | n/a | NOT FOUND (owner-requested) |
| D8 | Tags, favourites | n/a | n/a | NOT FOUND (owner-requested) |
| D9 | Drag and drop patches (between browser and editor/bank) | drag | invalid targets | CONFIRMED (large count of drag handlers in PBCore) |
| D10 | Backups as files (create/browse/restore) | files | disk space | CONFIRMED (Backup editor) |

## E. Application shell

| ID | Feature | Notes | Confidence |
|---|---|---|---|
| E1 | Synth Editors manager: add an editor for a module, enable/disable, filter, per-module MIDI in/out | each module is its own editor window set | CONFIRMED |
| E2 | Toolbar customisation, sidebar and toolbar toggles, full screen | standard macOS features | CONFIRMED |
| E3 | Find / find-next / replace in lists | standard text-find | CONFIRMED (menu) |
| E4 | Appearance: light, dark or follow system | | CONFIRMED |
| E5 | Licensing, trial, purchase, update checks | **Out of scope for XPPatchStudio** | CONFIRMED |

## F. Owner-requested, not in the bundle (from the brief)

FL-style docking and saved workspace layouts; hint bar; editable shortcut map; right-click value menu with MIDI-CC
link; browsable undo history; send-queue "sync" meter; mutate; compare; tone/section copy-paste; `.mid`
import/export; tags and favourites; hover preview with restore; performance-mode 16-part rack; rhythm-set
per-key view. These are tracked as *owner-requested* in the parity matrix.

## G. Open questions for the owner

1. Is Patch Base **parity** the goal for rows marked NOT FOUND, or are those rows new features? (The matrix marks
   them owner-requested either way.)
2. `.mid` import/export: standard MIDI file containing SysEx events, or something else?
3. Which XP-60 areas must edit in real time (temporary area) versus write to memory?
