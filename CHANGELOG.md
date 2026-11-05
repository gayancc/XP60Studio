# Changelog

Process note (owner decision): hardware testing is one dedicated phase at the end. Items that need the instrument
are tracked in `docs/07-hardware-acceptance.md` and updated every phase; nothing here is run on hardware before then.

## Effects routing of a Patch (2026-11-05)

Reviewed against the Owner's Manual pp.60-64 and 75 ("Routing effects", the Output Assign cases and the routing examples). Gaps found and fixed:

- **No routing view.** Added a **Routing** tab (after Structure): the Output Assign and the three send levels of all four Tones in one place, and below them a **flow diagram** of the whole signal path (Tones, EFX, chorus, reverb, MIX OUTPUT and DIRECT OUT) drawn only where the current settings carry sound. The four Tones share one bus on the left with a single lane per destination carrying small dots in the colours of the Tones that feed it (the first version drew a line per Tone per destination and was too crowded); the stages send on to each other down the middle and to the outputs through a shared bus. A stage nothing feeds is dimmed, hovering names what feeds a lane, and clicking an effect stage opens the Effects tab. The per-Tone output settings moved here from the Zones tab (now just "Zones").
- **Sends with no effect were not shown as such.** With a Tone on DIR the chorus and reverb sends do nothing (manual p.61, p.75); those knobs are now dimmed and the hint bar says why. The first Tone of a pair whose Structure Type is 2 to 10 is dimmed (the instrument combines it into the second Tone, p.60).
- **A value the manual does not list was offered.** Output Assign offered `OUTPUT-2` and EFX Output Assign offered a third entry; the manual lists MIX, EFX, DIR (Tone) and MIX, DIR (EFX). They are no longer offered; a patch that already holds one still shows it, marked "(not in manual)" (ledger H-068).
- A stage that is fed but passes nothing on (for example an EFX whose Output Level, "Mix Out Send Level", is 0) now ends in a dashed stop mark and its hint says why, so a lit stage with no way out is not mistaken for a connection.
- The EFX level knob is named **EFX Output Level**, the manual's screen name (the address table calls it "EFX Mix Out Send Level"; kept in the hint).
- `artifact/docs/xppatchstudio/08-effects-routing-verification.md` checks every routing rule against the manual page that states it and marks what is only inferred.
- 14 new tests (routing model and panel).
- **Still open:** the same view for Performances (per-Part output, EFX source) and Rhythm Sets; the [EFX], [CHORUS] and [REVERB] panel switches (System settings) are not read by the diagram; the manual's routing examples are not offered as presets (they give "1-127" and "0" only, so a preset would invent the levels). Hardware ledger H-069.

## Drop-downs drawn the FL way (2026-11-05)

- **Changed (owner hard rule: FL Studio design everywhere):** no list in the app uses the system's native menu any more. Every drop-down is a flat recessed readout that opens FL's own menu, drawn from colours and sizes **measured from FL Studio 2025's VIEW menu** (2 px dark border, light body, 16 px heading bars, 19 px entries, blue-grey full-width hover; evidence and the list of what is still unmeasured in `artifact/docs/xppatchstudio/fl-menu-evidence.md`); the context menu and top menus use the same look; a plain list of choices uses FL's **combo-list** look instead (dark body, pale hover fill; also measured), grouped lists the menu look; the chosen entry is tinted, the row under the pointer or cursor is filled with the accent. The grouped EFX type list is one flat list with a **heading bar per family** and a **check mark** on the active effect, as in the owner's screenshot of FL's Windows menu (the cascade first built was an assumption and was removed). The bank picker in the top bar and the "What to copy" list use the same control. Keyboard: Enter/Space/Down open, Up/Down move, Enter picks, Esc closes; Up/Down on the closed readout steps the value.

## Effects as plug-ins (2026-11-05)

- **Changed:** the EFX unit is drawn as one effect plug-in. The effect is picked from a **drop-down grouped by family** (EQ, Drive, Dynamics, Filter, Chorus, Delay, Pitch, Reverb, Combos) with previous/next tiles; the header carries the family colour and a one-line description. A **sketch** of the effect (modulation wave, delay taps, EQ bars, reverb decay, drive curve, pitch bars, or the A to B chain of a combination) is drawn from its own settings. Parameters sit in labelled sections (Effect, Time and pitch, Modulation, Feedback, Tone, Output; a combination has one section per stage). Only the parameters the XP-60 gives the effect are drawn. The Effects tab still fits 1100 x 700 without a scroll bar. The Performance Effects tab uses the same plug-in.
- **Cross-checked against the Owner's Manual (pp.74-87, 199-205):** words for the discrete parameters (amp type, reverb and gate-reverb type, speed, filter type, post gain, ratio, feedback mode and phase, low and high frequency) are choices instead of knobs; values now read as pan L64-63R, coarse pitch -24 to +12 st, fine pitch in 2-cent steps, feedback -98 to +98 %, phase 0-180 degrees, spectrum width 1-5, peaking and cutoff frequency in Hz. Rates and times stay raw numbers (the manual's tables are not linear); the hint bar gives their real range.
- Unverified items are recorded in the hardware ledger (H-067). 7 new tests.

## Numeric choices are steppers, not drop-downs (2026-11-05)

- **Changed:** every parameter that is a fixed list of plain numbers is now a **minus / value / plus stepper** instead of a drop-down menu: key follow (pitch, cutoff, pan), velocity time 1 and 4, time key follow and bias level of the three envelopes, and random pitch depth, on every Tone and on Rhythm Set keys (18 parameters). The stepper walks the list in order (-100, -70, -50 and so on), so the readout is always the real value.
- **How it works (the FL Studio family's way):** a recessed value readout between two flat tiles with no border. Click a tile to step one entry; press and hold to keep stepping, faster the longer it is held, as one undo step; drag up or down on the value; turn the wheel over it; arrow keys; double-click puts it back to 0; right-click gives the value menu; the hint bar shows its name and value. A tile at the end of the list is disabled. The same stepper replaces the older bordered one used for MIDI channels, device ID and User slot.
- Lists of words (waveforms, controller destinations, patch names) stay drop-downs. 7 stepper tests; the control choice is covered in the model tests.

## README rewritten; envelope extras aligned (2026-11-05)

- **README** rewritten: features by editor, the seven Patch tabs, System settings and Scale tune, an **Interface conventions** section recording the owner's rules (desktop-first, every message bottom-left only, no box/ring/border around controls, tabs merged by signal path without scroll bars, no unexplained empty space), safety, install, the launcher, architecture, quality and roadmap. Screenshots of the Zones and output tab, Filter and amp, and the light theme were retaken from the repository's own golden bank.
- **Fixed:** the velocity time and time key follow drop-downs under an envelope were scattered, some beside the knobs and some on the next row. The small knobs now sit in one row and the drop-downs in a grid of equal columns directly under it, in both the filter and the amplifier frames.

## Patch tabs merged by what belongs together; empty spaces removed (2026-11-05)

- **What the blank box was:** the whole-Tone page I added (the "Tone" tab) laid its frames out in a grid; the Pitch frame beside Wave was taller, so everything below Wave had to wait under it, leaving an empty rectangle. It was never a feature. That tab scrolled in a small window, which defeats its purpose, so it is **removed**; the merged tabs below give the same reach without a scroll bar.
- **Thirteen Patch tabs became seven, merged along the signal path** (measured; none scrolls at the smallest window, 1100x700, and `npm run e2e` now fails if one does):
  Zones and output (where each Tone plays and where its sound goes) | Wave and pitch (the sound source) | Filter and amp (the two shaping stages, side by side) | LFO and modulation (both LFOs, then the controller routing) | Patch (patch, play, portamento and the patch's controllers) | Structure (how the Tones are joined) | Effects (effect, chorus and reverb).
  Patch-wide pages stay apart from the pages that follow the selected Tone, so a tab never mixes the two. The "Selected Tone / All 4 Tones" switch works on the three sound tabs.
- **Gaps removed:** the Patch page and the merged pages now use columns (a short group no longer leaves a hole beside a tall one); an envelope's extra controls sit beside its graph in a wide frame instead of leaving a blank area; the two Structure pictures sit side by side with the booster below each.
- Saved layouts that named a tab by position keep working (the position is clamped to the new, shorter list).

## No box, ring or border around knobs or any focused control (2026-11-05)

- **Fixed:** my earlier change drew a tinted square behind a knob that had keyboard focus (visible around the A knob of Scale tune). Knobs, sliders, switches, steppers, the Tone on/off dot, envelope points and key-range handles now show keyboard focus by **the control itself lighting up** (a brighter cap, thumb, dot or handle, and a brighter label); nothing is drawn around them. The LED switch no longer gets a box when hovered either; its label brightens.
- **Everywhere else:** the focus ring (`--focus`, a 1 px accent box-shadow) is gone from the whole app. Buttons, tabs, menu titles, drop-downs, list rows and the like show keyboard focus by a soft tint laid over the control (`--focus-fill`), which keeps a selected control's own colour. 29 files changed.
- `npm run e2e` now tabs through the Filter, Zones, Amp, LFO and Output tabs, Performance, Rhythm set and Scale tune, and fails if focusing any knob, slider, switch or spinner changes its shadow, outline, background or border (it compares each control focused and unfocused, so static artwork is not flagged). 8 checks, all passing; the focused knob looked at in the real window.

## Every message in the bottom-left corner (2026-11-05)

- **Owner rule:** every error, warning, piece of information, confirmation and validation message appears in the bottom-left corner of the window and nowhere else. New `messages` centre (`model/messages.svelte.ts`) and a `MessageStack` at the bottom left above the hint bar: each message has a kind tag (Error, Warning, Info, Done), wraps, can be dismissed with a button, and goes away by itself (info 5 s, done 4.5 s, warning 9 s, errors 20 s); the same text again renews the message instead of stacking; at most four show.
- **Everything is routed there.** The services (patch, performance, rhythm and system stores, bank, library, controller links, expansion boards, layouts, hear-on-hover, synth connection, the patch and System save flows) report whenever they set an error or notice, so the many places that already set them needed no change; `say()` reports too. Removed from the views: the top bar's notices and errors, the Bank, Library, Synth, Expansion, Layout and Storage message lines, the save panels' result and error lines, the controller-learning line, the structure note ("one of these Tones is off"), the Scale tune state lines, the shortcut-clash badge and the System page's permanent-memory banner (now a warning shown when the System page opens). While a save is reading or writing, the step button says so (disabled) instead of a status line. The advice about User Memory Protect follows a failed write as a warning.
- **What stays in place, on purpose:** the question a confirmation asks before something permanent, progress meters, live readouts (state chip, the readout in the top bar), the "needs the desktop app" pages, the find-and-replace preview summary, and static help text. `messageDiscipline.test.ts` fails if a new message line (role alert or status) appears anywhere else, and `npm run e2e` checks the corner position and that the top bar and the save panels show no message line.
- 13 new tests; 7 existing tests now check the corner messages instead of inline text. Checked in the real window.

## No folder paths in the top bar (2026-11-05)

- **Fixed:** after an export the top bar showed the whole file path, cut off at the window edge ("Saved C:/Users/.../AppData/Local/Te..."). It now says "Saved ALL RIGHT.syx", the file name only. `npm run e2e` checks that the notice after saving has no folder separator. The backup path in the save panels stays, because there it is information you need.

## Scale tune: no border, aligned, starts at equal temperament (2026-11-05)

- **No outline on knobs.** A knob that has keyboard focus or is being edited was ringed by an orange border; it is now shown by a soft tint behind it (every knob in the app, in line with the rule that selected things use a tint, never a border). On this page the key of the note being hovered, focused or edited is tinted too.
- **Alignment and spacing:** the keyboard on this page is drawn with seven equal white keys (112 px) and five black keys, so every natural note's knob is centred under its own white key and every sharp's knob is centred over its black key (measured in the real window: all 12 knob centres equal their key centres, naturals exactly 112 px apart). Each knob has its readout and its note name arranged the same way above and below the keys, with
  the name nearest the keys on both sides and the same 6 px to the keyboard; the old drawing placed knobs by the visible part of each key, which spaced them unevenly (128, 111, 111, 122, 140, 122 px).
- **Fixed:** a System page that had not been read from an instrument showed -64 cents on every note (all-minimum values), which would be written to the instrument if saved. The keyboard scale now starts at equal temperament (0 cents on every note), as an unedited new document; everything else on the System page still starts at its minimum. 3 new tests.

## Scale tune page redesigned (2026-11-05)

- System > Scale tune now has **big knobs with the value readable on every note** (+16, −8, 0 cents) over and under a keyboard drawing, replacing the 30 px knobs with no values. Above them: the **Scale tune on/off switch** with a line saying what off means
  (equal temperament; your values are kept, not used), an **Applies to** choice (Patch mode, or Performance parts with a 1 to 16 grid that marks Parts whose scale is not equal), and three **Start from** chips: Equal (reset), Pure with tonic C, and Arabian,
  whose values are the manual's own example table (Owner's Manual p.89, checked against the printed page). Below: *Copy to the other Parts…* and *Copy from Patch mode…*, each asking before it replaces anything. Every preset, reset or copy is one undo step; double-click a knob resets it.
- Built after a mockup with vertical bars was rejected; this keeps the knobs and the keyboard drawing. 6 model tests and 7 page tests; layout checked at 1280x800 and 1100x700.
- Not added (offered, not chosen): moving Master Tune and Master Key Shift here, and a "hear the note" button.

## Top menus no longer cut off (2026-11-05)

- **Fixed:** the File, Edit, Synth, Controllers, Settings, Layouts and View menus were clipped to one row by the top bar (the Edit menu showed only "Undo"). The same clipping that hid the save panels: the bar's rows hide anything that hangs below them. Menus and their panels are now placed against the window
  at the position of their title, clamped to the window, and scroll if the window is very short. Checked in the real window: all seven menus fully visible and inside the window. `npm run e2e` now fails if the File, Edit or View menu is cut off, and 3 menu tests were added.

## Structure diagrams with their text (2026-11-05)

- The structure picker's diagrams had no text: the small one in the Patch tab had its WG/TVF/TVA labels switched off and no Tone names. Every diagram now shows **WG, TVF, TVA, B and R on its boxes**, each lane is named (**TONE 1 / TONE 2**, or **TONE 3 / TONE 4**; the chooser shows
  "TONE 1 (3)" and "TONE 2 (4)" since it stands for both pairs), and under the panel's diagram there is a legend of the parts and what the type is for, including what the first Tone's amplifier does (balance, booster level, ring modulation depth), written from the Owner's
  Manual p.49 text. The chooser is three columns with the same text for the highlighted type, and the "one of these Tones is off" note now wraps instead of being cut off.
- **Wiring checked** type by type against the manual's descriptions and against the ten figures (kept as evidence only): types 1 to 10 match, including the booster before the first filter in type 4 and between the filters in type 3, the ring modulator in 5 to 10, and the second Tone's direct path in types 6, 8 and 10. No wiring change was needed.
- 2 new tests. Checked in the real window at 1280x800: the chooser fits without scrolling.

## Exports use the system Save dialog (2026-11-05)

- **Fixed:** exporting a second file raised the browser permission prompt "tauri.localhost wants to: Download multiple files" (Block / Allow). Every export (patch, Performance, Rhythm Set, System settings, bank, library, whole-memory backup)
  now goes through one `saveFile` helper and the app's `save_file` command, which asks where to save with the system *Save As* dialog (`tauri-plugin-dialog`) and writes the file from Rust; no browser download is used any more.
  The file name is checked (plain name, `.syx` or `.mid`, no folders). Cancelling the dialog writes nothing and says so.
- `npm run e2e` sets `XPPATCH_SAVE_DIR` so the app writes exports straight into a temporary folder without a dialog, and a new check fails if any export starts a page download. Confirmed in the real window that the *Save As* dialog opens.

## Writing the System settings, more exports, an end-to-end check (2026-11-05)

- **System settings can be written to the instrument.** System workspace > *Write to synth...* reads the settings, writes them to a backup `.syx` file and reads that file back to be sure it holds exactly what was read, asks for a second explicit
  confirmation, writes (at most 128 data bytes per message, paced, only inside the System region), reads back and compares, and offers *Undo the last write* (refused if the settings changed since). One arming allows one write; arming a patch or
  bank save drops it. Built as `plan_system_write` / `commit_system_write` / `restore_system_backup` in `xppatch-midi`, `arm/commit/restore_system_save` in the app, and `SystemSaveFlow` and `SystemSaveCeremony` in the interface. 4 client tests against the virtual XP-60, 6 application tests, a codec round-trip test,
  11 flow tests and 4 panel tests in the interface; hardware items H-061 to H-065 stay NOT RUN. Not marked done.
- **Export of a Performance, a Rhythm Set and the System settings** (File > Export ... as .syx / .mid), alongside the patch export; the new `Doc.toSyx`, `SyxImage::system_document` and `system_document_bytes`. Hardware item H-066.
- **Bug found while testing the new panel:** the save panels hung from the top bar, whose rows clip what hangs below them, so they could be cut off. They are now placed against the window and verified fully visible.
- **Layout bugs found by the new end-to-end check at 1100x700:** a Tab panel's content was cut off (it now scrolls), and the Library list used a fixed `100vh - 270px` height that overflowed with an import preview open (it now fills the space it is given).
- **`npm run e2e`** (ui/e2e/run.mjs): drives the built app over the WebView2 debugging port, 19 checks (bank, previous/next, edit and undo, export and re-open byte for byte, Performance and Rhythm Set files, Library preview, save panels, layout of every tab at 1100x700 and 1280x800, script errors). All pass.
- **`npm run coverage`**: interface line coverage measured at 56.6% (statements 51%), with most views at 0% in unit tests and covered only by the end-to-end check. Rust coverage was not measured (cargo-llvm-cov is not installed here).
- Docs: parity matrix gets section G (8 rows) and B2's System write, the roadmap table and the README are brought up to date, and ledger rows H-061 to H-066 are added.
- Found and recorded, not changed: the Library holds patches only, so Performances and Rhythm Sets can be exported and re-opened as files but not stored in it.

## Whole-memory files open in the Performance and Rhythm Set workspaces (2026-11-05)

- Found by loading a real whole-memory backup (`xp_50_Bulk_2.syx`: 128 patches, 32 Performances, 2 Rhythm Sets): the patches loaded and exported byte for byte, but the Performances
  and Rhythm Sets in the same file could not be opened. **File > Open** now works in those workspaces too (the codec already read them; new `docSyxNames` and `Doc.fromSyxSlot` bindings),
  with the header picker and previous/next buttons listing the file's Performances or Rhythm Sets. One file is open at a time, and a Performance file is not offered in the Patch workspace.
- Checked in the running app with that file: all 128 patches open one after another without errors, every tab of four of them renders without clipping, the four-Tone view shows four columns,
  patch export is byte-identical to the file's own slots (0, 3, 77, 127), the Library imports 128 (104 new, 24 already stored), the Bank workspace loads it, all 32 Performances and both
  Rhythm Sets open from the picker, and previous/next move one slot. One edit and one undo step back by one knob step as designed. 3 new UI tests, 1 new Rust test.

## Windows installer and release workflow (2026-11-05)

- `npm run installer` (in `ui/`) builds a standard NSIS setup, `XPPatchStudio_<version>_x64-setup.exe` (about 3 MB): welcome, install for all users or current user, **choose the install
  folder**, Start menu folder, finish; Start menu shortcut, an entry under Settings > Apps with uninstall; the WebView2 runtime is fetched when missing.
- `.github/workflows/release.yml`: pushing a `vX.Y.Z` tag (it must match the `Cargo.toml` version) runs the UI tests, builds the installer and publishes it on a GitHub release;
  it can also be run by hand to get the installer as a build artifact. The Tauri CLI is now a dev dependency of `ui/`.
- Not done: code signing (SmartScreen will warn on first run) and a macOS build, which needs an `.icns` icon, an Apple developer identity and notarisation.
- The installer was built and its NSIS script inspected (install-mode, folder and Start menu pages are in the flow), but the setup was not run end to end here.

## Export of the open patch (2026-11-04)

- Line-by-line check of the toolbar in the Patch Base screens against the app found one missing function, export of the patch being edited. **File > Export patch as .syx / .mid**
  now writes the open patch as a one-patch User bank (built by the same Rust code as bank and library export), named after the patch; it opens again with File > Open.
  4 tests. The Common and Global screens were checked control by control in the running app: every control shown there exists (the Tone switch on the Tone cards,
  Tap/Hold/Peak/Volume/Aftertouch sources and the Rcv group on the System page).

## Whole-Tone page; screenshot audit closed (2026-11-04)

- New **Tone** tab (Patch view): every part of the selected Tone on one page (Wave, Pitch, Filter, Amp, LFO 1 and 2, Output), as Patch Base's single Tone screen does; with
  "All 4 Tones" it shows all four. It scrolls inside itself; layout audit clean at 1100x700 and 1280x800.
- Screenshot-by-screenshot audit of the nine Patch Base screens (Tone Pitch, Filter, Amp, LFO, Pan/Out and whole-Tone pages, Common page with the structure chooser,
  Global page, bank list): every parameter shown exists in `data/xp60-parameters.json` and has a control; toolbar functions (init, randomize, copy/paste, fetch,
  send, export, previous/next, save to library) exist. Deliberately not built: Patch Base's "AI Patches" (Audio Intelligence was removed from the roadmap).
- EFX rate, delay and feedback stay raw numbers: the XP-60/80 manual gives no value table for tempo-synced note lengths ("1/2 trip"), so a conversion would be a guess (hardware item).

## Screenshot gap pass: structure, effects, all Tones, bank navigation (2026-11-04)

- Structure type 1 to 10 is chosen from a wiring diagram (our own drawing, from the Owner's Manual p.49 wiring), with the booster knob live only for types 3 and 4 and a note when a Tone is off.
- The EFX type is a named list and its parameters carry their names (all 40 types, from the manual's EFX appendix; names cross-checked against the XP-60/80 manual). Gains, balance and HF damp show real units; other values stay raw.
- "All 4 Tones" shows the four Tones of a section side by side.
- Controller destinations are spelled out (Pitch, Cutoff, Pitch LFO1 ...) from the manual pp.60-61.
- Bank navigation: previous/next buttons beside the Bank picker and a Bank tab listing every patch of the open bank. Controller number mapping is still raw (H-059).

## Patch Group table found; Part patch picker with names (2026-11-04, goal: verify, assume nothing)

- Roland's XP-30 and JV-1010 Owner's Manuals (text PDFs, downloaded to `artifact/references/roland-manuals`, git-ignored) print the table the XP-60/80 manual lacks:
  Patch Group ID 1 = USER, 3 to 6 = PR-A, PR-B, PR-C, PR-D (GM) (2 is the CARD bank those models do not have, and its absence does not shift the others),
  Group Type 2 with the SR-JV80 board number as ID for expansions (Session 9, Orchestral 2, Techno 11). That answers the open point and also confirms the
  board-number reading used for wave names. Agrees with the JV-2080 hardware capture. Recorded in `03-xp60-protocol.md`.
- The Performance Part rows now have a group dropdown (USER, PR-A to PR-D, and the expansion boards ticked in Settings) and a Patch dropdown that names Patches from
  Roland's list for the fixed banks, by number for User memory (the owner's own) and for expansions. A Part pointing somewhere the lists do not offer stays visible.
  Changing group sets type and ID together as one undo step. 7 new tests. Still to confirm on the XP-60 itself: H-058.

## Quality gates re-run (2026-11-04)

- `cargo clippy --workspace --all-targets -- -D warnings` clean; `cargo fmt --all --check` clean (the code added today was reformatted);
  `cargo test --workspace` (17 test binaries) and the 393 UI tests pass; `svelte-check` 0 errors; `npm audit --omit=dev` 0 vulnerabilities.
  (`cargo audit` is not installed here, so Rust dependencies were not scanned for advisories.)

## Parameter map read against the Owner's Manual: two errors fixed (2026-11-04, goal: verify, assume nothing)

- Pages 222 to 225 of the Owner's Manual (a scan) were rendered and every row of all eight blocks compared with `data/xp60-parameters.json`.
  All block sizes agree, including System Common 0x60 (96), the one size no open-source project states for the XP-60.
- **Fixed:** Arpeggio Beat Pattern is 0..60 (1..61), not 0..40; Rhythm Note Mute Group is 0..31 (OFF, 1..31), not 0..32, which a five-bit
  field cannot even hold. Both were wrong in the map and in the draft table.
- New fixture `tests/golden/manual-verified-map.json` and test `crates/xppatch-core/tests/manual_map.rs` (4 tests) keep the map and the
  manual reading from drifting apart.
- The footnote lists behind the Patch Tone enumerations (controller destinations, LFO waveform, offset, fade mode, external sync, random pitch,
  keyfollow, velocity time, filter type, output assign, bias direction) and the Patch Common lists were compared with the manual: all agree.
  Two System Common values that showed a bare number now show the manual's words: TAP Control Source (OFF, HOLD-1, SOSTENUTO, SOFT, HOLD-2)
  and Clock Source (INT, MIDI).
- Controller values (System Control Source 1/2, Controller 1/2 Assign, Pedal 1 to 4 Assign) stay **raw numbers**, as the manual gives them
  (1..97 and 1..104). A labelled version (controller number, then PITCH BEND 96 and AFTERTOUCH 97) was built and then removed: it fits the ranges and the
  Integra/XV research in sigma/integral, but it is an inference, and no source states the value-to-controller mapping for the XP-60. It returns only
  after ledger item H-059 confirms it on the instrument.
- Patch Group ID: written down exactly what is evidenced (a JV-2080 hardware capture) and what is not (whether XP IDs shift by one because
  there is no CARD bank), so nothing is inferred; the Part dropdowns stay number-only until H-058.

## Audit pass on the real window (2026-11-04, goal: UI/UX, glitches, bugs, gaps; checked, not assumed)

Method: the running desktop app was driven through the webview's debugging port at 1100x700 (the smallest window), 1280x800 and 1920x1080, over
every workspace and every tab, looking for overflow, clipping, off-screen elements, console errors and unnamed controls.

- **Bug, fixed:** at 1100 px with a bank open the header grew to 1196 px, pushing Close, Maximize and Minimize off the screen. The header can no
  longer grow past the window; the bank picker, the origin text and notices shrink first.
- **Bug, fixed:** the Library's details column overflowed the window at 700 px high after the Factory lists panel was added; the panel now takes
  the height that is left and its list scrolls.
- **Gap, fixed:** Send to synth did nothing, silently, with no instrument; it is now disabled and says why.
- **Gap, fixed:** the Part On switches (drawn without a label) had no accessible name; LED switches now fall back to their parameter name.
- **Glitch, fixed:** Tone card wave names were cut ("ST.Strin..."); the Tone tag moved under the name so the name has the whole row.
- Shortcuts re-verified in the app after the desktop changes: F5 opens the Library, Ctrl+R randomizes, Ctrl+K opens the palette.
- No console errors or off-screen controls at any of the three sizes; the remaining items the audit lists are the Library's scrolling rows.
- Protocol facts cross-checked against independent open-source projects (see `03-xp60-protocol.md`, "External cross-checks"): model ID, command
  codes, checksum, and the block sizes (Patch Common 73, Tone 129, Performance Common 66, Part 25, Rhythm Note 58) all agree. The System
  Common size (96) could not be independently checked and stays manual-only.
- 11 new tests.

## No browser behaviour in the desktop window (2026-11-04, owner request)

- The webview's own right-click menu (Back, Reload, Inspect...) is removed everywhere; text fields keep the ordinary Cut/Copy/Paste menu, and
  the app's own right-click menus on knobs and sliders are unaffected. Reload, print, save and view-source shortcuts (F5, Ctrl+R, Ctrl+P,
  Ctrl+U, Ctrl+S) and Ctrl+wheel page zoom are blocked too. `model/desktopChrome.ts`, installed only in the desktop app; 6 tests.

## Wave names for 17 expansion boards (2026-11-04, owner request and permission)

- With the owner's explicit permission (including accepting Roland's download licence on their behalf) the Waveform List of every
  SR-JV80 board that Roland publishes was downloaded by `scripts/fetch_sr_jv80_lists.py` into `artifact/references/roland-sr-jv80`
  (git-ignored: the licence forbids passing the PDFs on) and read by `scripts/sr_jv80_lists.py`. Each board is checked: wave numbers must
  run 1..N with no gap or repeat or the board is reported and left out. SR-JV80-03 comes out at 73 waves, the count Roland states; Pop and
  Orchestral agree with the earlier transcription (apostrophes aside).
- Wave names are now held for boards 1 to 5, 7 to 18 (17 of 23). None exists for 6 (Dance), 19 (House) and the promotional boards 96 to
  99: Roland publishes no list; they keep the "Board · number" title. Two entries Roland's SR-JV80-12 list cuts off (112 and 152) stay
  unnamed rather than guessed; one printing slip in SR-JV80-03 ("039 E. Grand 2", which must be 029) is repaired and stated in the script.
- `data/factory/xp60-factory.json` regenerated (names only). The licence question about shipping the names is the owner's to weigh.

## Wave title never blank (2026-11-04, owner feedback)

- A Tone whose wave has no held name is titled with where it lives and its number ("Piano · 012", "Internal B · 012"), never a bare
  code; the Overview does the same. Roland's names for boards 3 to 19 and 96 to 99 are still to be added (see the open item).

## Tone cards: dot switch, wave picker; installed expansions in Settings (2026-11-04, owner request)

- The Tone card's On control is gone: the coloured dot beside the Tone's name is the switch (hollow and the whole card greyed when the
  Tone is off). Level and Pan are two equal knobs side by side.
- New wave picker on each card (the Tone's wave is what a Tone holds; the Patch is chosen per Part in Performance): click the wave name
  to open a browser with the sources on the left (Internal A, Internal B, every installed expansion board) and a searchable numbered
  list on the right; arrow keys, Page Up/Down, Enter and Esc work; one undo step. A board whose wave names are not held is chosen by
  number; a wave the Tone already uses on a board not marked installed stays visible and is flagged.
- Settings has "Installed expansion boards": the 19 main SR-JV80 boards and the 4 promotional and regional ones, tick what is physically
  in the keyboard (up to four slots, EXP-A to EXP-D; a fifth is refused with the reason). Remembered between runs.
- 23 new tests.

## Fixed memory versus the owner's memory (2026-11-04, owner instruction)

- Preset patches (PR-A to PR-D) and expansion boards are treated as fixed ROM that is identical on every XP-60; the User bank is
  the owner's rewritable memory. Checked against the Owner's Manual (p.45, as the earlier project recorded) and Roland XP-30/XP-80
  documentation. The Factory lists panel now says which is which ("Fixed" or "Yours to change"), the User list is titled "User bank,
  as shipped", and the expansion note says boards are read-only and sound only while installed. The rule is in CLAUDE.md and README.
- The write path already agrees: only temporary, System and User addresses can be written; anything else (Preset, expansion) is
  classified Unknown and never written.

## Factory lists: wave names, expansion boards, factory patch names (2026-11-04, owner request)

- New `data/factory/xp60-factory.json`, generated by `scripts/build_factory_data.py` from Roland's published lists: the 640 factory
  patch names (USER, Preset A, B, C and D/GM), the 448 internal wave names (INT-A 255, INT-B 193), the 23 SR-JV80 expansion boards and
  the wave names of boards 1 (Pop, 154) and 2 (Orchestral, 174). Names only, no sound data. `--check` flags a stale file.
- Tone cards and the Overview now name each Tone's wave ("INT 026 · Synth Saw 1"). Checked against the real bank: "Clav 1 x4" resolves
  to Clav 2A/3A/3B/4A, "60s Organ x4" to 60's Organ 1 to 4, "Alto Sax Ex" to Alto Sax 1A and 1C (a regression test).
- Library has a "Factory lists" panel: bank choice, search, 128 numbered names.
- Not guessed: which Preset bank a Part's Patch Group ID and Number point at is not yet known, so the Part patch dropdowns still show
  numbers only; wave names for boards 3 to 19 and 96 to 99 need Roland's lists (recorded in the hardware ledger).
- 16 new tests.

## Status readout in the FL design language (2026-11-04, owner feedback)

- The blue dot-matrix display is replaced by `XpReadout`: a recessed graphite well in the toolbar style, with small-caps
  labels, the name in the text colour, and the accent colour reserved for live values (the control under the pointer, the
  slot tag, queued sends). The state shows as a coloured dot and word using the existing sync colours; the link light is green
  when the XP-60 is connected. Real text instead of canvas, so it follows the light theme and scales crisply. 7 tests.
- Captured and checked in the real desktop window (dark and light) through the webview's debugging port.

## Repository restructure (2026-11-04, owner request)

- The app is now the repository root (`crates/`, `ui/`, `data/`, `tests/`). The earlier Qt app moved to
  `artifact/archive/xp60studio-legacy/`; all plans, recon and investigations moved to `artifact/docs/` (`xppatchstudio/` for this
  project, `xp60studio-legacy/` for the old one). Removed regenerable leftovers (three old CMake build trees, `.venv-build`, the Qt
  installer log). `.gitignore` rewritten for this app. Doc references in CLAUDE.md, README and code comments updated.
- Verified from the new root: 350 UI tests, svelte-check, `cargo test --workspace` and the release build all pass.

## Smoother range dragging (2026-11-04, owner feedback)

- Dragging a range in the Performance key map and the velocity sliders was stepping because every note changed the whole
  edited sound and redrew the screen before the block moved. The block now follows the pointer immediately from its own state;
  the app is told once per frame (and with the final position on release), so a drag is still one undo step and no rate of
  mouse events can flood the sound or the live send.
- Cursor shows what a grab will do: move (middle of a block) or set an end.
- The display no longer reallocates its canvas on every redraw.
- 2 new tests; the key map tests now wait a frame as the app does.

## Smooth display type; own title bar (2026-11-04, owner request)

- The instrument display now uses smooth vector type with a soft glow instead of a blocky dot matrix (the dot font and its
  tests are removed). Same content and layout.
- The native window title bar is off (`decorations: false`). The header is the title bar: its empty parts drag the window and a
  double-click maximizes; minimize / maximize / close sit in a column at the top right (desktop app only). New
  `WindowControls`; the capability file allows only those window actions. Header tabs are a little tighter and the display 284 px
  wide so the buttons still fit on one line.

## Instrument display in the header (2026-11-04, owner request)

- The top right of the header now holds a blue backlit dot-matrix display in a dark bezel, in the manner of the XP-60's own
  screen (the legacy app's Bank Builder LCD was the look reference; no code or font copied: the 5x7 font in `kit/lcdFont.ts` is
  new). New `XpLcd` kit component, 300 x 52, drawn dot by dot on a canvas.
- It shows where you are (PATCH / PERFORM / ...), the sound's sync state (LOCAL, EDITED, SENT ...; flashes while sends are
  queued), the open sound's name large, the Bank slot as a small tag, and the instrument link (XP-60 ID / NO INSTRUMENT /
  PREVIEW, LIVE). While a control is under the pointer it shows that parameter's value large and its name below, as the
  hardware shows the parameter you touch.
- Header layout: tabs and buttons share the left 770 px, the display spans both rows on the right; the Bank slot picker moved to the
  first row. Header height unchanged (62 px), so no tab lost any room.
- 8 new tests (font coverage, display).

## Light theme: key map and sliders use theme tokens (2026-11-04, owner feedback)

- The key map lanes, octave seams, ruler bands, velocity groove/ticks and value chips had hard-coded dark colours, so the light
  theme showed dark lanes beside light panels. They now come from new `--lane-*`, `--seam*`, `--groove-*`, `--tick*`, `--chip-*`
  tokens, defined for both themes.

## Calmer colours, patch picker on the Part, FL-style range sliders (2026-11-04, owner feedback)

- Tone and Part colours desaturated (Tone 1-4: amber, steel blue, sage, mauve; Parts use a 38 % saturation ramp). The orange accent is unchanged.
- Performance: each Part has its patch dropdowns (group, number) in its own row (new `PartPatchPick`); no trip to another tab.
- Velocity ranges are now a recessed groove with a scale, a lit range and two raised grips with the values at the ends; key-range
  blocks in the key map get the same raised grips and a top highlight.
- Selected items use a tint, never an outline (see earlier entry).

## Zones tab on the key map; bigger Effects knobs (2026-11-04, owner feedback)

- Zones: the 480 px scrolling vertical roll (four full-height colour slabs, only four octaves visible) is replaced by the
  same `KeyMapH` as Performance: the whole keyboard with one lane per tone, then one wide velocity bar per tone with its
  cross-fade knob. Everything is visible at once, no scrolling or Lower/Higher buttons.
- Performance > Effects: knobs back to normal size (30); EFX/Chorus/Reverb columns rebalanced so it still fits without scrolling.

## Performance key map: planned space for a real keyboard (2026-11-04, owner feedback)

- The octave-ruler stopgap is replaced. The Performance tab now plans its space for the keys: a full-width piano keyboard
  (FL key geometry) with one lane per part directly beneath on the same scale, eight parts per page tabbed, part settings in
  tabs below. New `KeyMapH` kit component; gutter holds part number, On switch and patch.
- Dragging a range is one undo step; a one-note block can be moved (grab zone is at most a third of the block); pointer
  capture failures no longer break the drag. Same fix in the vertical piano roll.
- 10 new tests for `KeyMapH`. Full suite 345 passing, svelte-check clean. Not seen on the desktop exe yet.

## Performance part strips as octave rulers (2026-11-04, owner feedback)

- 128 piano keys in 200 px was noise. The part rows now draw the key range as an octave ruler: alternating octave bands with C
  labels (every octave when there is room, every second or fourth when there is not, never closer than a label is wide) and the
  range as a coloured block with its name written on it. The real keyboard is used where there is room for it (Key map, Zones,
  Audition, Rhythm set). `octaveMarks` and the ruler display have ten tests.
- System tab controls have explicit short labels (no more three switches called "Switch" or words broken across lines), and
  clicking no longer selects interface text.

## Keyboard and System layout fixes (2026-11-04, from the owner's screenshots)

- Selected or pressed keys: the outline is now the visible shape of the key (`keyOutline`): a white key goes round the black keys
  that cover it, a black key is a plain rectangle. Before, a plain rectangle was drawn over everything and cut across the black keys.
  Four tests (area of every white key, never outside its face, clipping).
- Performance parts: the key strip had its keys and range lane squeezed side by side; they are stacked now, the keys are taller
  and the range is written on the strip ("C2 - C6"). New Key map tab shows all 16 Parts as lanes on one keyboard, with dimmed lanes
  for Parts that are switched off, and edits ranges by dragging.
- System tabs: every tab has a declared layout (`systemLayout.ts`): Bank select, Pedals and Preview are tables whose columns line
  up (empty cells where a row has no such parameter), switches sit in aligned columns, the rest are titled groups. A test checks
  that every System parameter is shown exactly once and in the right tab.

## Bank to Library folder, parity C6 (2026-11-04)

- Bank > Add all to library files every patch in the grid in the Library folder "Synth/<date>" (a tag with a path, so folders
  nest). A patch the library already holds is filed in the folder without being stored twice; the batch stops at the first
  failure and says how far it got; an empty grid or empty folder name calls nothing. Three tests.
- With Read from synth and the Bank write this covers the bundle's group fetch, keep and send.

## Back up all memory, parity C5 (2026-11-04)

- `SyxImage::user_documents` and `user_document_bytes` read and write User Performances (`10 nn 00 00`) and User Rhythm Sets
  (`10 40 00 00`, `10 41 00 00`); the golden bank holds all 32 and both, and they survive being written and read in three
  different cuts. `Client::fetch_user_documents` reads them from the instrument (a slot that cannot be read ends the run,
  cancel is an error, never a short backup).
- `Synth::backup_user_memory` and the `midi_backup_all` command return patches, performances and rhythm sets as one `.syx`;
  Bank > Back up all memory offers it as a download. Read only. Virtual-device tests and three UI tests. Ledger H-057.

## Library folders, parity D2 (2026-11-04)

- A folder is a tag with a path ("Pads/Warm"). The library filter treats a tag as a folder: "Pads" finds "Pads", "Pads/Warm" and
  "Pads/Warm/Dark", but not "Padsy" or "Bass/Pads", and `%` and `_` in names match literally (two Rust tests).
- Library tabs: All, Favourites, the top-level folders; opening one shows a trail (Pads > Warm) and the folders inside it.
  Tags typed with stray spaces or doubled slashes are tidied. `folders.ts` (seven tests) and a component test of the folder
  navigation (five tests, with a small test harness).

## Chunk size of whole-sound sends, parity A3 (2026-11-04)

- `xppatch-sysex::chunk::resplit_dt1` cuts the DT1 frames of a whole-sound send into frames of at most N data bytes (held to the
  instrument's 128), keeping device, order and every byte at its address (four tests, including a 129-byte tone cut at 32).
  The Synth panel has a Chunk setting (16 to 128, remembered; 128 sends exactly what was sent before). It applies to the Send
  to synth button, bulk tool results and hear on hover. Five UI tests. Ledger H-056.

## Saved layouts, parity F1 (2026-11-04)

- `layouts.svelte.ts`: the open workspace and the tab of every workspace live in one arrangement that is written down on every
  change, so the window reopens as it was left (tabs used to reset when switching workspaces). Layouts menu: save the arrangement
  under a name (12 at most, built-in names reserved, same name replaces), apply, delete, reset; four layouts are built in.
  Stored data is read defensively. Twelve tests; checked in the preview across a reload.

## Overview, file preview, drop anywhere (2026-11-04)

- B12: an Overview side tab summarises the open patch (`overview.ts`, three tests).
- C4 and D9: `openFile.ts` is the one way a picked or dropped file gets in. A dropped `.syx` or `.mid` opens in Patch, is shown
  with its patch names in the Library before Import is pressed, or loads into the Bank grid; elsewhere the window says where
  to drop. The window no longer lets Tauri swallow file drops (`dragDropEnabled: false`). Twelve tests.
- A3: the write gap is also shown as frames per second.

## Copy as text, find and replace, backup browser; a name bug fixed (2026-11-04)

- B13: Edit > Copy as text puts the open sound on the clipboard as readable text (every block, every parameter as shown).
- E3: the Library has Find and replace for the shown names, with a preview; a result that is too long, empty or not plain
  text is skipped and named, the batch stops at the first failure and reports how far it got.
- D10: Bank > Backups lists the backup files written before saves (`backups_list`, `backup_read`; names only, no paths) and
  opens one in the bank grid.
- Bug fix: the characters of performance and rhythm-set names were not recognised as name characters, so Init, Randomize,
  Mutate and block copy could change them. They are protected now (`isNameCharacter`), with a regression test.
- Bug fix found by the new service test: a failed bulk rename lost its error message to the refresh that follows it.

## Completion notice, parity A8 (2026-11-04)

- After Fetch sound, Verify and reading System settings, the top bar says what happened for six seconds ("Fetched "name" from
  the instrument.", "Verified: ...", "Mismatch: ..."). Two tests. A failed read keeps showing its error instead.

## Hear on hover, parity D6 (2026-11-04)

- Library: a "Hear on hover" switch (off by default, desktop app only). Resting on a patch for 350 ms sends it to the
  instrument's temporary patch; leaving the list puts back the sound the instrument had (read once before the first
  preview, restored once, also when the switch goes off or the Library is left). Nothing is sent unless the current sound
  was read first; a failed restore marks the sound STALE and says so. User memory is never touched. Nine tests. Ledger H-055.

## Controllers drive parameters, parity A4, A6 and A7 (2026-11-04)

- NRPN and RPN (A4): `xppatch-midi::controls` decodes CC 99/98 and 101/100 with Data Entry (6 and 38) into one 14-bit value per parameter number
  (RPN 127,127 ends addressing; Data Entry with nothing selected stays an ordinary controller; channels are independent). Links can
  be made to an NRPN or RPN exactly as to a CC, and old maps are read as CC. Seven decoder tests, one client test, five UI tests.

- `xppatch-midi`: other Control Change messages are now reported (`MidiEvent::Controller`), collected per (channel,
  controller) with the latest value, capped, and kept apart from the Program/Bank events that mark the sound stale. The
  real input only passes controllers while someone asks (a link exists or one is being learned), so an idle connection to
  a busy keyboard cannot flood the queue. Virtual-device tests include a 5,000-message flood that must not hide a Program Change.
- `xppatch-app`: `midi_listen_controls`, `midi_poll_controls`.
- UI: right-click a knob, Link to MIDI CC, move a controller (Esc cancels). A controller sets the parameter across its
  range; a sweep is one undo step; Bank Select is never linked. Controllers menu lists and clears the links, which are
  remembered between runs. Pure map and service are tested (14 tests); checked in the desktop app up to the point where an
  instrument would answer. Ledger H-054.

## Storage location, parity D5 (2026-11-04)

- Settings menu: choose the folder for the library and backup files. A pointer file in the default folder selects it
  (`storage.rs`, six tests); the library is copied with SQLite `VACUUM INTO` and the backups are copied, an existing library
  in the chosen folder is kept, nothing is moved or deleted, a refused or failed choice changes nothing, and the new
  folder applies at the next start. Checked end to end in the desktop app (choose, restart, in use, reset).
- The UI test timeout is raised to 20 s: the many-seeds randomize test timed out while the machine was building.

## `.mid` import and export, parity D7 (2026-11-04)

- `xppatch-core::smf`: writes the SysEx messages of a `.syx` as a type 0 Standard MIDI File (one event per message, 25 ms
  apart) and reads any `.mid` back, taking only its SysEx events (split messages joined, notes and meta events skipped).
  Eight Rust tests, including a byte-exact round trip of the 128-patch golden bank and a garbage-input check.
- UI: Open, Library import and Bank load accept `.mid`; Library and Bank have Export .mid. Five UI tests.
- Ledger H-053 (playing the file back into an XP-60) stays NOT RUN. Corrected a stale UI test: a whole rhythm set sends
  64 note frames, its Common block being held back (H-045).

## UI rework: tabs, menus, FL-style piano (2026-11-04)

- No scroll bars: the shell is a fixed window (menu bar, workspace tabs, stage, hint bar); each workspace splits its
  contents into tab strips (Patch: Zones, Wave, Pitch, Filter, Amp, LFO, Output, Modulation, Patch, Effects, Controllers;
  side tabs Play, Tools, History; Performance: parts 1-8 / 9-16 and detail tabs; Rhythm set, System and Bank likewise).
- One-time setup moved into menus: File (New, Open), Edit (undo, tools, copy/paste), Synth (ports, device ID, connect),
  View (palette and shortcuts, theme).
- New kit: Tabs, TabPanel, MenuBar, PianoKeys, PianoRoll (+ `rollGeometry`, tested). One keyboard drawn from the measured FL
  piano-roll rules (docs in the GenosStyleStudio piano-roll study): natural faces 1.5/2/2/1.5/1.5/2/2 rows, one-row black
  keys centred on the natural boundaries at two thirds depth, naturals painted first, shaded C faces, separators only at C
  and E/F, right-aligned C labels, accidental-first hit test. Used vertically in Zones (flat range lanes, overview strip,
  wheel/Lower/Higher) and Rhythm set (a level bar per key), and horizontally in Performance part ranges, Audition and
  Scale tune. No note grid: only the keys follow FL.
- New and the starting documents (patch, performance, rhythm) now begin from the neutral sound, and the neutral sound opens
  the upper ends of key and velocity ranges (it used to leave every range at 0, which plays nothing). Not undoable, not an edit.
  System settings are untouched. Application defaults, not factory ones (ledger H-052).
- A failed connect now shows its error in the top bar (it used to appear only inside the Synth menu panel, which closes).
- Build note: a plain `cargo build --release` needs `--features custom-protocol` to embed the UI.

## Phase 7: Extras, and the Bank workspace (complete pending hardware)

- Sound-design tools (`ui/src/model/tools.ts`, one place for every kind of document): Init (neutral values, first
  Tone/Part/Key on), Randomize and Mutate (seeded and reproducible; a weaker run is a subset of a stronger one; names,
  routing and the ranges and switches that decide whether something sounds are never changed), Morph between the A and B
  versions, A/B switch, Compare (changes since opened, with meanings), Copy and Paste of a whole Tone/Part/Key or one
  section. Each tool is one undo step; a large batch goes out as one whole-document send.
- Command palette (Ctrl+K) searching commands and every parameter ("tone 3 tvf cutoff" jumps to the control), and an
  editable shortcut map (saved, conflicts shown, needs Ctrl or Alt or a function key).
- Bank workspace: read all 128 slots with progress and cancel, load a `.syx` bank, drag slots to swap them or drag library
  patches in, export, and write the changed slots through the guarded ceremony (all 128 saved to a verified backup file
  first, each slot read again and read back, stops at the first failure). Rust: `plan_user_bank`, `commit_user_bank`,
  `Synth::read_bank/arm_bank_write/commit_bank_write` with tests against the virtual XP-60.
- Audition panel: playable on-screen keyboard (velocity from where you press) and computer keys, channel and octave, every
  held note released on key-up, blur, panel close and Panic; `send_note`/`all_notes_off` in the client (no memory is
  written by notes). Ledger H-051c.
- Light theme (owner question 4 answered by providing both): a switch in the top bar, remembered.
- Write gap setting (20 to 250 ms, never below the documented 20 ms) applied in both the UI gate and the Rust client.
- Review hardening of the bank path: long operations are single-flight (a second one is refused, so Cancel always reaches the
  running one, and other instrument commands report busy instead of queueing); cancel is honoured between slots, never part
  way through one; a lost cable keeps the report of what was written; the whole bank call is rejected before the first write
  for a repeated or unbacked-up slot; arming records the exact slots the user confirmed and the write must match them; the
  bank is locked in the UI while armed and writes the patches frozen at arming; arming one kind of save drops the other;
  backup file names never collide; a partly unreadable bank is an error, not a short list.
- Review fixes in the tools and audition: percent scales on the Randomize/Mutate knobs, shortcuts that change the sound are
  disabled while typing and in the Library and Bank workspaces, mix levels are never randomised (a roll cannot silence a
  Tone), stale morph and A/B state dropped on undo and open, held notes tracked per key and channel and released on focus
  loss, palette closes on Escape from anywhere and returns focus.
- UI tests: 199. Rust: all pass.

## Phase 8: Parity audit (2026-11-04)

`docs/04-parity-matrix.md` now states a truthful status for every row: 11 DONE, 21 IMPLEMENTED pending hardware, 12 PARTIAL,
7 OPEN, 3 proposed WON'T-DO (needs owner approval). OPEN rows: NRPN/RPN option, hover audition (D6), incoming MIDI control and learn (A6, A7), group fetch/send (C6), storage location (D5), `.mid` (D7, needs owner answer),
toolbar customisation (E2). Nothing is marked DONE for an instrument-dependent row.

## Phase 6: Librarian and the save ceremony (complete pending hardware)

- `xppatch-library`: SQLite store (rusqlite, bundled). Import a User bank or a single patch dump, all or nothing;
  duplicate detection by content (the hash only finds candidates, bytes are always compared); search, tags, favourites,
  rename (refused if it would create a duplicate), delete, export as a bank `.syx` split to 128 bytes. 11 tests incl. a real bank.
- `xppatch-core`: `Patch::to_blocks`/`from_blocks` (one flat form used by the library, wasm and app), `SyxImage::temp_patch`.
- `xppatch-midi`: the guarded User write. `plan_user_write` reads the slot first (a `WritePlan` can be made no other way),
  `commit_user_write` writes only inside that slot's blocks, then reads back and compares (`Mismatch` names blocks, which
  is also what User Memory Protect looks like), `restore_user_backup` puts the old patch back. Live edits remain
  temporary-only.
- `xppatch-app`: `arm_user_save` writes the old patch to a new backup file (never overwriting) before arming exactly one
  save; `commit_user_save` disarms first, whatever happens; restore; Tauri commands for all of it and for the library.
- UI: Library workspace (search as you type, tag filter, favourites, import, export shown, add current patch, open in editor,
  rename, tags, two-step delete), the Save to synth panel (read and back up, confirm wording naming both patches, one
  write, verified result, undo), a service layer with tests (state machine for the ceremony; library refresh ordering).
- Phase 5 review fixes: one stale poll for all temporary copies (system excluded), shared pacing gate across the four
  stores, System never produces live frames, canonical block labels only, the Rhythm Setup name held back until H-045,
  UI state kept across workspace switches.
- Review hardening of the save path: the slot is read again at commit and must still hold what was backed up; the
  read-back listens through an echo window so Soft Thru cannot fake a verified write; the backup file is parsed back and
  compared before arming (a bad one is deleted); arming expires after 60 s; Undo is one-shot, kept apart from arming and
  refused if the slot changed since the save; the UI freezes the patch at arming, confirms Undo, cannot be closed while
  writing, disarms when its workspace goes away, and never shows an unverified write as success.
- Gaps recorded: bank grid view with slot occupancy and whole-bank writes (H-032), whole-synth backup to a dated folder,
  hover audition from the library (H-024), `.mid` import. Backups are written; browsing them is not built yet.
- Ledger: H-034 to H-037.

## Phase 5: Performance and Rhythm Set editors (complete pending hardware)

- `xppatch-core::doc`: `Document` for Performances (Common + 16 Parts) and Rhythm Sets (Common + 64 Notes, addressed by key
  number 35 to 98); split-and-reassemble round trips; shared block reassembly with `Patch`. Every parameter of every block
  now has a display (`display` test covers all blocks).
- `xppatch-midi`: `fetch_document`/`fetch_temp_document`; live-edit allow-list extended to the known blocks of the temporary
  Performance and Rhythm Setup (still nothing outside known blocks, no permanent memory); `VirtualXp60::set_document`.
- `xppatch-wasm`: generic `Doc` class and `docParamAddress`; catalogue covers all six patch/performance/rhythm blocks.
- `xppatch-app`: `midi_fetch_temp_doc` for patch, performance and rhythm.
- UI: store generalised over document kinds (one store per kind, each with its own history and queue); mode tabs; Performance
  view (16-part rack with enable, channel, patch, level, pan and key-range bars; part and common detail panels) and Rhythm
  view (64-key pad grid with on/off and level, full note editor with the three envelope graphs). Layout rules cover every
  parameter (tests). 103 UI tests.
- Gaps recorded: System and Scale Tune editors (Phase 5 leftover, no UI yet); patch names for performance parts; mixer/FX
  panel beyond the generic EFX parameter knobs.
- Ledger: H-044 to H-047.

## Phase 4: Patch editor, hint bar, undo, live edit (complete pending hardware, owner go-ahead given)

- `xppatch-core::display`: typed display for every patch parameter from the manual's text and footnote tables
  (signed, stepped, padded, pan, note, option lists). All 200 patch parameters resolve; tested at both range ends.
- `Patch`: `PatchBlock`, block access and `param_dt1` (the single DT1 for one parameter; refuses bad values).
- `xppatch-midi`: `Client::send_temp_frame` (live edit frames; temporary memory only, 128-byte cap, device-ID check).
- `xppatch-wasm`: the Rust codec for the UI (catalogue JSON, document get/set/format, frames, `.syx` slots).
- UI (`ui/src/model`, `ui/src/editor`): parameter catalogue and layout rules (test: no orphan parameter), control choice
  per parameter, patch store with grouped undo/redo and browsable history, paced and coalesced live-edit queue, sync-state
  transitions (EDITED, SENT, TEMP VERIFIED, STALE, MISMATCH), editor screens for Tones, Zones, History, Tone sections with
  envelope graphs, controller matrix and Patch (common) sections.
- `xppatch-app`: Tauri 2 shell with MIDI commands (ports, connect, send frame, fetch temp patch, stale poll) over the
  tested client; compiles, 6 tests against the virtual XP-60. `README.md` explains running it.
- Ledger: H-025 to H-029 added. Gaps recorded: wave names (number only), musical Init defaults (phase 7).
## Phase 3: Control kit, theme, storybook (complete, awaiting owner sign-off)
- Review fixes: wide arithmetic in display formatting and strict note/pan parsing; live edits limited to the Common and
  Tone blocks of the temporary patch; Tauri commands off the main thread and old ports released before reconnecting; raw
  device ID carried end to end; the queue retries failed frames and never reports SENT after a lost frame; SENT only when
  the instrument holds the whole patch (the first edit sends it all); STALE sticky; live-off and disconnect clear the
  queue; verify/fetch wait for the queue and ignore edits made meanwhile; zone ranges written in a safe order.

New `ui/` workspace: Vite + Svelte 5 + TypeScript (Tauri shell arrives with the app crate in phase 4). No hardware involved.

- Theme tokens (`ui/src/theme/tokens.css`): graphite neutrals, one warm accent, status and four Tone colours.
- Pure, tested logic: value maths (drag from the drag start, fine 0.28x, wheel notch 13.5 px, keyboard, NaN-safe), knob arc
  geometry, envelope layout (each stage owns a slot), key-range helpers, sync-state table.
- Controls: Knob (20/30/38/64, bipolar, disabled), Slider, Stepper, Dropdown, Segmented, LED toggle, Led, Meter,
  EnvelopeGraph, KeyRangeBar, GroupFrame (slanted tab), DockPanel (focus, collapse), HintBar with MIDI LEDs, queue
  meter and StateChip, shared right-click menu (type, copy, paste, reset).
- Gestures report lazy begin/end so a drag, a wheel notch or a key press is one undo step and a no-op opens none;
  removing a control mid-drag still closes its group.
- Storybook page shows every control in its states. Checked visually in the browser; 41 tests, `svelte-check` clean, build ok.
- Review fixes: multi-pointer and non-primary button guards, disabled-mid-drag, Shift+wheel on WebView2, one-note key range
  widening, key strip hit maths, context-menu focus and Tab handling, Segmented focus follow, Stepper undo bracketing.
- Not in this phase (recorded): full dock/tab/float layout engine (phase 4 with the rack), MIDI-learn link (menu item
  disabled), light theme (owner question 4). Full-range drag distance (160 px) is our choice; FL's absolute value is unknown.
- Dev note: TypeScript pinned to 6 (svelte-check does not support 7 yet); 2 low npm advisories in build tooling (`deepmerge`).
## Phase 2: MIDI transport (complete, awaiting owner sign-off)

New crate `xppatch-midi`. Verified only against the virtual XP-60; nothing has been run on hardware.

- `MidiTransport` trait that owns time (`now`, `sleep`), so timeouts and pacing run on a virtual clock in tests.
- `SysExAssembler`: reassembles fragments, drops real-time bytes, reports truncated frames, bounds memory, and surfaces
  Program Change / Bank Select events.
- `Client`: one RQ1 at a time, completion by expected block coverage (sparse replies work), timeouts and retries that name
  the missing blocks, device-ID scan, probe, patch/bank fetch with progress, cancellation, a stale-patch retry,
  and temporary-only writes (split to 128 bytes, 20 ms apart, optional read-back verify). No permanent-write API yet.
- `VirtualXp60` with fault injection (dropped requests, corrupt replies, noise, ignored writes, latency).
- Real `midir` transport and `probe` example behind the `hardware` feature (read-only).
- Review fixes: a request cannot be kept alive by repeated pieces and has a 10 s ceiling; bounded drain, event and
  receive queues; unambiguous port selection; oversized or empty expectations refused; pieces clipped to the block;
  retries wait for a streaming reply to finish; a bank read aborts after 3 silent slots; echo window before verify.
- Known limits (recorded, not fixed): `port_mut`/`into_port` expose the raw transport (used by tests); pacing is measured
  from the send call, not wire time; a scan can miss a very slow device. See ledger H-012, H-018, H-019.
- Tests: 19 integration + 11 unit in `xppatch-midi`; whole workspace passes, clippy `-D warnings` clean.

## Phase 1: Core + SysEx + checksum (complete, awaiting owner sign-off)

Added a Cargo workspace with two crates and the single-source parameter map.

- `data/xp60-parameters.json`: 8 block types, 464 parameters (offset, width, encoding, raw range, display), message
  constants, transfer limits and the memory-region table. Provenance is recorded inside the file.
- `xppatch-sysex`: base-128 `Address`/`Size` with carry, Roland checksum, strict RQ1/DT1 encode and parse with typed
  errors, stream splitting (interrupted and unterminated frames are reported, not dropped), DT1 chunking
  (128 data bytes, address advances with carry), memory map and the temporary/permanent write classification.
- `xppatch-core`: validating loader for the parameter map, `BlockBuf` codec (7-bit, ASCII and nibble encodings,
  range-checked, no cross-talk between parameters), `Patch` (Common + 4 Tones) to and from DT1 pieces in any order or
  chunking, `.syx` image parse/serialise and User Patch bank export.
- Tests: 76 passing (debug and release). Golden tests use a real XP-60 User Patch bank (128 patches, 1,314 messages): the file
  re-serialises byte for byte, 74,752 values are in range, every patch re-encodes to the original blocks, and a
  128 + 1 split export reads back identically. Every value of every parameter is round-tripped with a check that no
  other byte changes. Property tests cover address arithmetic and checksum (any single-bit error is detected).
- Lint: `cargo clippy --workspace --all-targets -- -D warnings` clean; `cargo fmt` applied; `unsafe` is forbidden.

### Review outcomes (independent Rust review and security review, both applied)

Fixed, each with a regression test:

- Conflicting or duplicate DT1 pieces were silently merged (`Patch::from_dt1`, so also `.syx` with a repeated slot).
  Now identical duplicates are accepted and any disagreement is `PatchError::Overlap`.
- `BlockBuf::get_param/set_param` could panic on a `ParamDef` from another block. They now reject foreign or
  out-of-bounds parameters with `CodecError::UnknownParam`.
- `ParameterMap::from_json` could overflow or panic on hostile numbers (huge offset, 8+ nibble bytes, huge block).
  Now validated with saturating arithmetic, nibble width capped at 4, blocks capped at 1,024 bytes, and the transfer
  limits checked (1 to 128 data bytes).
- A DT1/RQ1 whose span runs past the 28-bit address space, and an RQ1 of size 0, are now errors on encode and parse.
- Frames are bounded (512 bytes) and `.syx` files are bounded (16 MiB); frame iteration is lazy, so a file of
  1,000,000 `F0` bytes fails at the first frame instead of allocating a million errors. Empty files are an error.
- MIDI real-time bytes (`F8..FF`, for example Active Sensing) inside a frame are removed before parsing.
- `memory::classify_range(address, len)` classifies a write by where it ends as well as where it starts (a write that
  starts in temporary memory can carry into the next region); `classify` remains for single addresses.
- `Patch::to_dt1` and `user_patch_bank_bytes` refuse a patch that holds out-of-range or malformed values, so imported
  bad data cannot reach an export or send path. Loading a bad file for inspection still works.
- Patch names show non-printable bytes as `?`; `Address::parse` rejects a leading `+`; the checksum sum is 64-bit;
  `split_dt1` no longer overflows spuriously on a block ending on the last address; `split` above 128 bytes is refused
  for device-bound exports; error enums are `#[non_exhaustive]` and `SyxError` carries memory/chunk/encode errors
  directly; MSRV declared (`rust-version = "1.89"`).

Not changed, by decision:

- `Patch::from_dt1` takes owned `Vec`s and `SyxImage::user_patches` clones records. Fine at these sizes (112 KB bank);
  revisit if profiling says otherwise.
- `SyxImage` does not compare per-record device IDs; a file mixing device IDs is accepted and the IDs are kept on the
  records. A librarian decision for Phase 6.
- Three `expect` calls remain on the embedded map (`ParameterMap::embedded`, `patch::defs`); they are asserted by the
  test suite and cannot fail at runtime.
- `Cargo.lock` is present in the project folder; commit it with the project (it pins dependency versions).

### Unverified on real hardware (owner confirmation required before anything is called done)

- Everything above is checked against **a bank file and the manual transcription**, not against this project talking to
  your XP-60. No message has been sent or received by XPPatchStudio yet (Phase 2 adds MIDI).
- Legacy-verified behaviours carried into the design but not yet confirmed by this project: 20 ms minimum gap between
  DT1 messages, 128-byte limit on data sent to the instrument, one outstanding RQ1 at a time, sparse replies.
- Writes to User memory and the effect of User Memory Protect (the write ceremony is designed, not built).
- Performance, Rhythm and System tables: transcribed from the manual and range-checked only structurally; the bank
  contains no such data.
- Parameter meanings and units (the map gives raw ranges and Roland's display text only).
- The unit of the Patch Base wait value 30 (assumed milliseconds), and whether to use it at all.
