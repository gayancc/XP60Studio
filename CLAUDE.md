# XPPatchStudio: project rules

A brand-new, standalone Roland XP-60 patch editor/librarian. Functional parity with the Patch Base app bundle
(documented behaviour only), with an original UI that follows the FL Studio design language and interaction model.

## Hard rules (non-negotiable)

0. **This is a desktop application, not a web application. Permanent owner instruction.** Everything is designed, planned,
   implemented, tested and documented as a native Windows/macOS desktop app (Tauri shell, Rust core, direct MIDI access, native
   window and title bar, files, drag and drop, installer). Web technology is only how the window is drawn. Do not design for
   browsers, URLs, responsive/mobile layouts, hosting, web accessibility-of-a-site concerns or "preview in a browser" as a
   product; do not describe the app as a web app in any doc or README. Dev-only tooling that happens to run the UI in a browser
   is a developer convenience, never a deliverable or a target.
1. **The earlier XP60Studio (Qt) app is archived** in `artifact/archive/xp60studio-legacy/` and its notes are in
   `artifact/docs/xp60studio-legacy/`. Treat everything under `artifact/archive/` as read-only reference: do not edit,
   revive or build it, and do not move anything back out of it.
2. **The app is this repository root** (one Cargo workspace; `ui/` is the Svelte front end).
3. **Do not reference, link or import any project, crate or assembly from the archived app.**
4. **Clean-room.** Study Patch Base to document *behaviour* (features, workflows, data formats, SysEx handling).
   Do **not** copy its code, scripts, images, icons, fonts or text into the new app. Re-implement everything from our
   own spec.
5. **FL Studio is a design-language reference only.** No Image-Line assets, logos, names, icons or skins. Recreate
   the look and feel with original vector-drawn controls.
6. **Do not follow Patch Base's layout, navigation or visual structure anywhere.**
7. **Live desktop verification — tests green is not enough.** For any behavioural UI or MIDI/hardware-facing change,
   verify on the **real running desktop app** by clicking the **same controls a person uses** (pads, menus, tabs,
   knobs). Unit/integration tests must still pass, but they alone do not prove the fix. Do **not** treat underground
   API calls (`invoke`, store methods, IPC-only scripts) as acceptance. Report what you clicked and observed; if the
   live window could not be exercised, say so explicitly and do not claim the behaviour verified. (Also:
   `.cursor/rules/live-ui-verification.mdc`.)

## Hardware truth

- The official Roland XP-60/XP-80 owner's manual (the owner's XP60MD repo, per-chapter markdown) outranks the
  bundle. Where they disagree the manual wins and the conflict is flagged.
- **Never mark a SysEx feature DONE until the owner has confirmed it against a physical XP-60.**
- Unknown addresses or meanings are recorded as unknown, never guessed.

## Stack (Rust-first; UI choice to be confirmed at Phase 0 approval)

- **Rust** for everything that is not drawing: domain model, SysEx, MIDI, library, tests.
- **UI (recommended): Tauri 2 shell + TypeScript/Svelte 5**, with a custom control kit drawn in SVG/Canvas.
  Why: FL-faithful vector controls, gradients, condensed type, detachable/dockable panels and multi-window layouts
  are far cheaper to get pixel-right on the web stack, while all MIDI and SysEx stays in Rust (no Web MIDI).
- **UI (pure-Rust alternative): egui + egui_dock.** Single language, custom painting is easy; docking, typography
  and detachable windows are rougher. Revisit only if the owner prefers zero JavaScript.
- MIDI: `midir` behind a `MidiTransport` trait (mockable). Storage: SQLite via `rusqlite` plus raw `.syx` files.
- Tests: `cargo test`, golden-file fixtures for SysEx encode/decode and checksums, property tests (`proptest`) for
  round trips; UI tests with Vitest and a storybook-style control gallery.

```
XPPatchStudio/
  Cargo.toml                 workspace
  data/xp60-parameters.json  the single source of truth (parameter map)
  crates/
    xppatch-core             domain model, parameter definitions (generated from data), no UI, no I/O
    xppatch-sysex            encode/decode, checksum, address map, chunking/throttling
    xppatch-midi             MidiTransport, port discovery, request/response with timeouts
    xppatch-library          SQLite DB, import/export, tags, search, backups
    xppatch-app              Tauri 2 shell: commands exposing the crates to the UI
  ui/                        TypeScript (Svelte 5): control kit, storybook route, views
  tests/golden/              SysEx fixtures
```

The parameter map is defined **once as data** (`data/xp60-parameters.json`). Editor controls (UI types) and SysEx
addressing (Rust) are generated from that single source; nothing is hand-copied between them.

## Phases (stop and wait for owner approval where stated)

0. Recon docs only: `artifact/docs/xppatchstudio/01-bundle-inventory.md`, `02-feature-catalog.md`, `03-xp60-protocol.md`,
   `04-parity-matrix.md`. **No app code. Stop and wait for approval.**
1. Core + SysEx + checksum, golden tests (round-trip every parameter)
2. MIDI transport: ports, device ID, fetch single/bulk, send, timeouts, retries
3. Control kit + theme + storybook window
4. Patch editor (all tones and sections) + hint bar + undo
5. Performance + Rhythm Set editors
6. Librarian (browser, DB, import/export .syx/.mid, tags, search, backups)
7. Extras at parity: compare, init, randomize/mutate/morph, copy/paste sections/tones
8. Parity audit until every matrix row is DONE or WON'T-DO (with approval)
9. **Hardware acceptance**, one single session on the physical XP-60, running every item in
   `artifact/docs/xppatchstudio/07-hardware-acceptance.md` (owner decision: hardware testing is **not** done phase by phase)

End of every phase: build passes, tests green, a short CHANGELOG entry, and **new or changed items appended to
`artifact/docs/xppatchstudio/07-hardware-acceptance.md`** for anything that needs the instrument. Before phase 9 everything is built and tested
against the virtual XP-60 (`xppatch-midi::testing`), the golden bank fixture and the manual transcription. Keep the ledger
current whenever a requirement changes. Order in the hardware session: READ items, then TEMP, then PERM (each PERM item
preceded by a backup); stop at the first unexpected result.

## FL design language (reinterpreted, not skin-copied)

Dark graphite theme, subtle gradients, one warm-orange accent plus status colours, compact density, small-caps
labels. Hint bar shows name/value/description of the control under the pointer. Dockable/detachable floating panels
with savable layouts. Controls: vertical drag, Ctrl+drag fine, wheel step, double-click or Alt+click reset,
right-click menu (type value, copy/paste value, link to MIDI CC, reset). Every edit goes to a browsable undo
history. Debounced, throttled real-time send with visible queue status. Editable shortcut map.

Measured FL reference material lives outside this folder at
`D:\Codes\GenosStyleStudio\docs\fl-ui-evidence` (read-only reference). It shows the native FL knob construction
(track ring with value arc, gradient cap, position dot, condensed caps label) and the measured gesture ratios
(Ctrl-drag about 0.28x, wheel notch about 13.5 px of drag). The control kit here is written from scratch.

## Memory facts (owner instruction, verified against the Owner's Manual p.45 and Roland documentation)

- **Preset memory (Patch PR-A, PR-B, PR-C, PR-D/GM) is ROM: the same in every XP-60, read-only, never written by this app.** Lists of its
  contents describe every instrument.
- **Wave Expansion boards (SR-JV80, EXP-A to EXP-D) are fixed too:** their waves, patches and rhythm sets are read-only and only sound
  while the board is installed.
- **User memory (128 Patches, 32 Performances, 2 Rhythm Sets) and System settings are rewritable and the owner's own.** The "User bank as
  shipped" list is only the factory default, never a statement about what a particular instrument holds; always label it so, and
  never present it as the user's current data.
- Presets and expansion patches are edited by loading them into the editor (temporary memory) and saving the copy into User memory.

## Repository layout

This folder is the app. `artifact/docs/xppatchstudio/` holds this project's plans, recon and hardware ledger (the files that older
text calls `docs/`); `artifact/docs/xp60studio-legacy/` holds the investigations and plans of the earlier Qt app;
`artifact/archive/xp60studio-legacy/` holds that earlier app itself (source, build files, tooling), kept for reference only and
never built from here. `artifact/Patch Base.app` is the read-only reference bundle.
