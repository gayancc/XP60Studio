# Desktop and hardware acceptance continuation — 2026-11-07

Goal: finish the connected-instrument hardware ledger and verify all implemented desktop functions. Resumed from Claude session `d94dc808-810e-4d14-a2e0-7dda8da142f1`, whose last action launched `ui/e2e/hw-bank.mjs` before its usage limit.

## Evidence boundaries

Historical hardware evidence is preserved. No SysEx feature is promoted to DONE without owner confirmation. UI test assertions, instrument read-back, audible observations and panel observations are recorded separately.

The inherited Bank runner remained alive without output. It was stopped before a reported Bank write. Its old fixed patch-name assertions were removed, failed assertions now stop execution, and the intended slot is explicitly checked before writing. The debugger helper now reports evaluation errors and times out rather than waiting indefinitely.

## Fresh baseline

- Native desktop connection test: Connect, Fetch sound, Verify, Disconnect and Reconnect all passed; XP-60 ID 17, temporary patch “Jimmee Dee”.
- Read-only snapshot: `target/hardware-acceptance/2026-11-07-codex/1794068291-hardware-session-full-backup.syx`, 118,336 bytes, 1,844 messages; 128 patches, 32 performances, two rhythm sets and System settings. Saved again in the default application backup folder. Snapshot read took 75.35 seconds and the saved file parsed successfully.
- Temporary patch: `target/hardware-acceptance/2026-11-07-codex/temp-patch.blocks`, 589 bytes. This is the observed block length (older helper comments incorrectly say 590).
- Earlier backup paths recorded by Claude were absent at resumption; this is why a fresh baseline was taken. The old records are historical evidence, not proof that those files still exist.
- Standalone panel listener: zero events in a 60-second capture; owner subsequently reported selecting “steel drums”. Timing relative to the capture is unconfirmed. This does not pass H-017. Snapshot System bytes have Transmit Program Change=1 and Transmit Bank Select=1; Patch Transmit Channel raw=16 is recorded without interpreting it.
- UI type check: zero errors and zero warnings. UI unit suite passed. Rust workspace test rerun passed after the snapshot executable finished (the first attempt could not relink the running example).
- Desktop regression initially failed its obsolete Bank selector (`aria-label^="Slot "`). Current controls use patch numbers and panel coordinates. The assertion now checks the actual 64-slot subgroup; legacy tab labels in focus checks were also updated to the current labels and are now asserted to exist.

Machine-specific full logs and exports are retained under `target/hardware-acceptance/2026-11-07-codex/`. A log ending before a final success marker is not a pass.

## Function coverage inventory

This inventory does not imply acceptance. Each feature keeps its original parity status until the named checks and required owner observations are complete. Software unit coverage alone is not hardware verification.

| Feature | Implemented function | Current acceptance work |
|---|---|---|
| A1 | MIDI in/out port selection | Fresh desktop connect/disconnect/reconnect passed; H-001 historical evidence retained. |
| A2 | Device ID setup | Desktop connection at ID 17 passed; wrong-ID and scan evidence historical H-002/H-003. |
| A3 | Throughput: speed, chunk, buffer | H-056 chunk PASS; H-050 gap PASS; H-023 byte-rate 3125 B/s + chunk 32 PASS (H-023.txt). |
| A4 | NRPN/RPN option | Software-tested; hardware CC learn covered by H-054; NRPN/RPN still software-only (explicit remaining). |
| A5 | Notes from keyboard/interface to synth | H-051c PASS: computer-key and on-screen mouse owner-heard; mouse/velocity/octave/channel/blur MIDI trace PASS (`H-051c-mouse.txt`, `H-051c-mouse-listen.txt`). |
| A6 | Incoming MIDI controls parameters | H-054 CC learn PASS (mod wheel → Cutoff). |
| A7 | MIDI map / learn | H-054 PASS. |
| A8 | Fetch-complete notice | Fresh desktop fetch and verification notices observed. |
| A9 | On-screen keyboard | H-051c mouse CDP + computer keys on connected XP-60 (H-051c / H-051c-mouse). |
| B1 | Single-patch editor, rename | Native Patch tools + live edit hardware paths recorded (H-022 audible/read-back PASS; H-025/H-029; tools Init/Randomize/Mutate/Morph desktop). |
| B2 | Non-bank single-instance editor (system) | H-061..H-065 System editor hardware evidence recorded. |
| B3 | Fetch from synth | Desktop temp fetch/verify PASS; bank H-048 PASS. |
| B4 | Send to synth (temp) | H-020/H-021/H-025/H-029 and chunk H-056 PASS. |
| B5 | Save to synth slot | H-030/H-033/H-035/H-036/H-039 PASS (Protect OFF path); H-031 Protect PASS. |
| B6 | Init | Neutral Init changed bytes, sent/verified, Undo restored. H-052: separate XP-60 Init Patch action matches two panel DEFAULT captures exactly; native Send/read-back and Undo/restore PASS (`H-052-panel-init.txt`). |
| B7 | Randomize | Native Randomize sent/verified + Undo (09 narrative). |
| B8 | Morph | Native midpoint Morph sent/verified + cleanup (09 narrative). |
| B9 | Mutate | Native Mutate sent/verified + Undo (09 narrative). |
| B10 | Undo/redo + browsable history | Desktop regression/unit evidence to reconcile with explicit feature checks; current parity status retained; no new acceptance claim. |
| B11 | Switch to other section of a synth | Desktop regression/unit evidence to reconcile with explicit feature checks; current parity status retained; no new acceptance claim. |
| B12 | Overview panel | Desktop regression/unit evidence to reconcile with explicit feature checks; current parity status retained; no new acceptance claim. |
| B13 | Copy parameters / names to clipboard | Desktop regression/unit evidence to reconcile with explicit feature checks; current parity status retained; no new acceptance claim. |
| B14 | Copy/paste sections and tones | Desktop regression/unit evidence to reconcile with explicit feature checks; current parity status retained; no new acceptance claim. |
| B15 | Compare edited vs original | Desktop regression/unit evidence to reconcile with explicit feature checks; current parity status retained; no new acceptance claim. |
| B16 | Arpeggiator parameters | Desktop regression/unit evidence to reconcile with explicit feature checks; hardware or owner observation still required. |
| C1 | Bank editor | Native Bank read/write/restore + Select-on-instrument USER path (H-048/H-032/H-050b/H-070). |
| C2 | Fetch bank | H-048 PASS including Cancel. |
| C3 | Write bank | H-032 whole-bank PASS; H-050b subset PASS; H-049 restore PASS. |
| C4 | Load bank from file (preview) | Desktop regression/unit evidence; current parity status retained. |
| C5 | Whole-synth backup editor | Full snapshot + H-057 export decode PASS; owner panel name comparison remains. |
| C6 | Group "refs" fetch/send | Covered as bank fetch/write/library bulk (H-048..H-050b, H-037). |
| C7 | Multi-part browser | Performance/Rhythm native UI hardware (H-044..H-047 partial/PASS). |
| D1 | Patch browser tree | Desktop regression/unit evidence to reconcile with explicit feature checks; current parity status retained; no new acceptance claim. |
| D2 | SysEx outline with folders | Desktop regression/unit evidence to reconcile with explicit feature checks; current parity status retained; no new acceptance claim. |
| D3 | Open/import `.syx` | Native library import workflows PASS (09); damaged-bank load covered in software. |
| D4 | Browser search | Desktop library workflows PASS (09). |
| D5 | Storage location setting | Settings copied-library path PASS (09). |
| D6 | Hover/preview send | H-055 PASS with physical-key limitation; H-024 PASS. |
| D7 | `.mid` import/export | H-053 MIDI-tool receive PASS; Bank .mid export byte equality recorded. |
| D8 | Tags, favourites | Native library workflows PASS (09). |
| D9 | Drag and drop | Native library/bank drag workflows PASS (09). |
| D10 | Backup files | Bank/System/slot backups exercised in hardware (H-030/H-049/H-061). |
| E1 | Per-module editor manager | Desktop e2e retained; no new acceptance claim. |
| E2 | Toolbar customisation | Desktop e2e retained; no new acceptance claim. |
| E3 | Find/replace in lists | Desktop e2e retained; no new acceptance claim. |
| E4 | Light/dark/system appearance | Desktop e2e retained; no new acceptance claim. |
| E5 | Licensing, trial, updates | Desktop/product path retained; no hardware claim. |
| F1 | FL-style dockable/detachable panels + saved layouts | Desktop e2e retained; no new acceptance claim. |
| F2 | Hint bar | Desktop e2e / hw-hover retained. |
| F3 | Editable shortcut map | Desktop e2e retained; no new acceptance claim. |
| F4 | Right-click value menu, link to CC | H-054 CC learn PASS. |
| F5 | Send-queue sync meter | Desktop e2e retained; no new acceptance claim. |
| F6 | Performance 16-part rack | H-044/H-047 Performance editor hardware (Part Level/Channel/Key Range + whole send). |
| F7 | Rhythm-set per-key view | H-045/H-046/H-047 Rhythm hardware (address + live edit + whole send). |
| G1 | Structure type chooser | Desktop/unit retained; H-075 audible wiring still owner. |
| G2 | EFX type list and named parameters for all 40 types | Desktop/unit retained; H-073/H-074 panel still owner. |
| G3 | One Tone on one page, and all four Tones of a section side by side | Desktop e2e retained; no new acceptance claim. |
| G4 | Previous/next patch and a list of the patches of the open bank | Bank workspace hardware (H-048/H-070). |
| G5 | Export the open patch, Performance, Rhythm Set or the System settings as `.syx` or `.mid` | H-066 Perf/Rhythm receive PASS; System export/write H-061..H-065; Bank .mid H-053. |
| G6 | Open Performances and Rhythm Sets from a `.syx` | Snapshot/full-memory decode + H-057. |
| G7 | Windows installer with a choice of install folder, and a release workflow | Product packaging retained; no hardware claim. |
| G8 | Controller destination names | Desktop/unit retained; no new hardware claim. |
| G9 | Scale tune page | H-042 Scale Tune fetch + H-065 write PASS; panel labels still owner. |

## Confirmed owner-assisted panel check

H-017 passed: repeat listener received nine events (CC0/CC32/Program Change for User slots 80, 79 and 73 on channel 1). During the desktop run the owner selected another patch; the app changed from TEMP VERIFIED to STALE, then refetched “Strings”. Evidence: `hardware-evidence/H-017.txt`.

The first fresh Bank run reached arming and saved an 88,064-byte complete bank. Its inherited >100,000-byte heuristic rejected that valid file, so it stopped before writing. This assertion now decodes all 128 slots with the Rust/WASM codec; a deliberately truncated last slot is correctly rejected. Slot 128 currently reads “C-Z  Flute”, so historical fixed names are inappropriate acceptance criteria. The fresh snapshot is the baseline.

## Desktop Bank and System results

Changed User slot 128 was written and verified, then the saved backup was opened in Bank and the original slot restored and verified. Bank export: 88064-byte `.syx`, 90906-byte `.mid`; MIDI decoding equals SysEx byte for byte. Whole-memory export: 112206 bytes, 1314 frames, all 128/32/2 documents decode. The old >1500-frames heuristic was wrong and replaced by document parsing.

System: fetched before editing; exported; Master Tune changed one step; backup proved equal to original export; confirmed write read back equal; confirmed undo; refetch/export proved every System byte restored. No script errors. No owner observation of the Master Tune LCD was claimed. H-061 PASS; H-062 and H-063 partial.

Corrected desktop regression now passes. GATES.md retains bound automatic evidence for Rust tests, UI check, UI unit tests and native desktop end-to-end assertions. Remaining acceptance gates are intentionally unmet.

## Patch tools, audition and interrupted read

Native Init, Randomize and Mutate each changed patch bytes, sent and verified against the XP-60, and one Undo restored every original byte and was sent/verified again. Tone copy/mutate/paste restored the copied bytes. A/B preserved two distinct versions. Morph was exercised and sent/verified, but a midpoint assertion is being added before claiming interpolation acceptance. Cleanup reopened the retained original file and sent/verified it.

Computer-keyboard audition and Panic were confirmed audibly by the owner (“Heard notes; final note stopped”). Pointer/velocity and other keyboard controls remain open.

Owner unplugged/reconnected MIDI IN during a bank read. The app reported User patch 62 incomplete after three attempts, naming missing block 11 3D 12 00. Recovery exposed a product defect: recreated SynthBar stored input/output/device choices locally and reset them. Choices now live in SynthService by port name, preserving them across menu/workspace recreation and port-list reordering; an unavailable chosen port disables Connect rather than silently picking another port. Three regression tests pass; the first two failed before the fix. Native rebuild and recovery re-verification pending.

## Recovery fix and unexpected temporary patch

Rebuilt native app passed Connect/Fetch/Verify, switching Bank to Patch, Disconnect, retaining FCB1010 OUT, and reconnecting. The dropped-read error itself passed; physical cable recovery must be repeated against the fixed build before H-016 can fully pass.

Independent snapshot verification after Bank/System tests confirmed all 128 User patches, 32 performances, two rhythm sets and System settings equal the baseline; it restored and independently read back the original temporary Jimmee Dee.

After subsequent patch tools and the dropped-read/cable check, the temporary patch read Nice Piano. Tools had explicitly reopened and verified Jimmee Dee during cleanup. Owner says no panel change was made, and reports PR-A 004. Hardware writes stopped while investigating. A read-only transport guard logged only RQ1 messages and proved that reading User slot 1 leaves the current temporary patch unchanged. The desktop Bank read is now being traced for mutating IPC commands. Do not call final cleanup or complete acceptance while this unexplained selection remains open.

Diagnostic caution: the first IPC interception returned an empty trace because the Tauri invoke function is not replaceable; that trace is invalid evidence of absence. The replacement uses a debugger call breakpoint and requires positive controls for connect, fetch and bank-read commands. A first debugger attempt hit a non-main execution context; the helper now tracks the main frame execution context across startup navigation. The separate direct-MIDI RQ1-only diagnostic did pass and remains valid.

UI full suite after the selection fix: 71 files / 607 tests passed. Type check: zero errors and warnings. Final native rebuild is still required after the unavailable-port label polish.

Validated desktop trace now includes connect, fetch and bank-read positive controls (1 connect, 1 bank read, 2 fetches); zero mutating commands; temporary Nice Piano retained byte-identical. Edited-name probes pass for User slot, whole bank, System read, and MIDI transport close/reopen, each restoring the starting sound. Unexpected original selection remains unexplained. A second physical cable test on the fixed build uses DROP PROBE as a temporary marker, full original export, and command trace; owner has unplugged MIDI IN.

Latest regression gates: five met, three unmet (G2 hardware acceptance, G3 complete function coverage, G8 final cleanup). Runnable gates G4–G7 all passed against the latest built app: Rust workspace, UI type checking, 607 UI tests and native desktop end-to-end. The second physical interruption attempt missed the read and cleanup had to be recovered through the independent restore_temp example; DROP PROBE was restored to Nice Piano and independently verified. The next recovery test waits on explicit owner unplug/reconnect signals, avoiding the timing race.

Performance/Rhythm native UI tests passed in owner-confirmed Performance mode: fetched EasternSplit and PopDrumSet 2, one-byte level edits, live verification, whole-document sends, Undo, original file restoration. Independent reads match all 17 Performance blocks and 65 Rhythm blocks. Owner has been asked to return to Patch mode for MIDI Learn.

MIDI Learn passed: owner clarified the first gesture was pitch bend, then upward modulation arrived as CC 1. Cutoff sampled 0..127, final 0 from baseline 60; live patch read-back matched; one Undo restored 60 and verified; original temporary sound and exact controller-map storage restored. Native library tests are now running in a copied database selected through Settings; original library logical hash is checked and storage pointer restored in finally. First library assertion was corrected because the 128-slot fixture contains 118 distinct patch contents: Library intentionally deduplicates rows while the bulk retains every position.

Native copied-library workflows passed: Settings copied original logical DB exactly; restart used the copy; staged import retained all 128 bulk positions while deduplicating 118 distinct contents; patch rename/favourite/nested tag/search; filtered SysEx/MIDI byte equality; open in editor byte equality; bulk rename/copy/export/load; bank slot drag/swap; save over/version restore/reload; whole-bulk drag with replacement confirmation; delete only the copied test bulk. Original user library logical hash remained unchanged, and original storage pointer was restored (no custom pointer remains). Test copy retained at target/library-acceptance-7uXePs.

Owner confirmed both User Memory Protect options ON (Write Operation and System Exclusive Message). Native protected-save test prepared: local-only PROTECT TEST marker, slot 128 backup and explicit attempt; independent byte comparison required before accepting protection. No Soft Thru capability has been confirmed, so H-038 echo case remains open.

Protected-save desktop retest: the first UI run backed up slot 128 and clicked Replace but its 25-second notice wait expired before a result appeared; the native test process then closed. A separate hardware read compared every byte of slot 128 with that new backup and passed (`C-Z  Flute`). A second UI run timed out in the test harness waiting for the arming panel's backup text, so it made no write attempt. These retests do not add a UI-level H-031 pass; the earlier owner-confirmed `protect_probe` mismatch remains the valid hardware evidence. Both protections remain ON. The harness now waits longer and captures panel text for diagnosis; no further protected write is needed until the underlying timing is understood.

The revised Patch Tools run proved a genuine midpoint Morph: the exported blend differed from both A and B, it sent and verified on the XP-60, and the original temporary patch was sent and verified in cleanup. H-056 desktop test set Chunk to 128 and 32 in turn, sent the same fetched patch through the native window, and obtained Verified read-backs for both (send plus read about 942 ms and 928 ms). It restored the original Chunk setting. However, the fetched baseline was `PROTECT TEST`, revealing the protection harness had altered the temporary sound between its first and second attempts. A first restoration inadvertently used `protected-temp-original.syx`, which the second run had overwritten with `PROTECT TEST`; this was detected by the restoration's own before/after names. We then restored `Nice Piano` from the retained controller-session original (hash matching the earlier drop-session original) and independently read it back equal. The cause of the marker appearing in temporary memory is under investigation; User slot 128 remained byte-identical to its backup.

The cause of `PROTECT TEST` in temporary memory was a test harness selector error. The live switch has visible text `Live edit` but no `aria-label`; the protection script queried a nonexistent `aria-label` and silently failed to turn Live off. Its local name edit therefore legitimately sent to temporary memory. The corrected selector asserts `aria-checked=false` before any marker. A focused native probe fetched Nice Piano, set Live off, changed the editor name to `LIVE OFFCHK`, then Verify returned Mismatch, proving the keyboard retained its previous sound. No User memory was touched. This resolves the new marker issue, but does **not** explain the earlier unprompted Jimmee Dee to Nice Piano selection.

Corrected protected-save desktop attempt: Live was visibly OFF before local PROTECT TEST; app backed up C-Z  Flute in USER:128 and attempted Replace. App warned about Memory Protect and reported `read-back differs from what was sent: common, tone1, tone2, tone3, tone4`. Independent read proved every User128 byte equal to that specific backup; independent temporary read proved Nice Piano byte-equal to retained original. The Node harness exited 1 only because its regex missed the phrase `read-back differs`; regex corrected afterward, with no extra permanent attempt. `hardware-evidence/H-031-desktop.txt` records the exact limit.

Native Library hover audition: stored Brass bytes reached the XP-60 temporary patch exactly after pointer dwell; leaving the row restored all 589 prior Nice Piano bytes, and an independent User slot 1 read matched the full session snapshot. H-024 passed. H-055 remains partial until the owner plays and confirms the audible transition.

Owner then played the XP-60 keys during a held Brass preview and heard piano; display stayed PR-A 004 Nice Piano, audio was directly from the XP-60. In a separate held preview, MIDI notes sent by the app sounded like Brass, confirming the received-MIDI voice path uses the previewed temporary data while the instrument's own keys continued playing the panel-selected piano. To make the Library's Hear on hover feature audible without another control, the app now sends one 650 ms middle-C note after the preview patch arrives and releases it. Owner heard Brass automatically, but then reported a sustained note after the test ended. Emergency All Notes Off across all 16 channels stopped it. H-055 remains FAIL/INVESTIGATE until the release is proven. The preview-note helper now sends Note Off followed by All Notes Off on channel 1 in `finally`, with unit checks for the ordering and fallback; rebuilt hardware verification pending. The exact reason the first note remained sounding is not yet known.

Final rebuilt release: one hovered preview traced `midi_note` on, `midi_note` off, then `midi_all_notes_off` on channel 1; owner confirmed silence. A following repeated-hover check exposed a delayed Brass overwrite after a previous app exit, despite an in-window Nice Piano read. PreviewService now invalidates queued/in-flight hovers when leaving, switching rows or turning the feature off, with a regression test for a stored patch that finishes loading after pointer leave. The native acceptance harness now performs an independent temporary-patch read after the app exits. Four rapid leave/re-enter cycles on the fixed build passed in-window restore, after-exit byte equality and note command trace; owner confirmed silence, and independent User slot 1 read remained equal to snapshot. H-055 passes for automatic MIDI audition with the documented physical-key limitation. The exact cause of the initial hanging note and delayed overwrite was not directly instrumented, so only the final observed behavior is claimed.

With both User Memory Protect settings ON, the guarded native System Master Tune write was accepted and read back equal; Undo returned all settings to the exact backup, proven by a separate hardware read. The previous H-064 expectation that User Memory Protect would block System writes was wrong; the owner rechecked both protection settings still ON. The test harness exited 1 because of that obsolete expectation, and its assertion has been corrected; the hardware outcome is valid. H-065 then changed only Patch-mode C Scale Tune locally and in a guarded System write, proven by a one-address difference in local and post-write full System exports. Undo and independent read matched every original System byte. The first H-065 runner attempt failed before editing because its initial System fetch did not complete; strengthened diagnostics and the second run passed.

Latest regression gates after the preview fixes: Rust workspace tests passed; 72 UI files / 610 tests passed; type check has zero errors/warnings; native desktop end-to-end passed. The unlazy gate checker reverified G4–G7 with current evidence. G2 (remaining hardware), G3 (full function coverage) and G8 (final hardware cleanup) remain unmet, so its overall exit code 1 is expected. A native Patch Save-to-synth plus Undo script is staged; waiting for owner to set both User Memory Protect options OFF before permanent User memory writes.

Owner set both User Memory Protect options OFF. Native Patch Save-to-synth test backed up USER:128 C-Z  Flute, explicitly saved the distinct Nice Piano patch, obtained SAVED/read-back match, then used the app's Undo confirmation. Independent hardware read matched every byte of backup `1794076236-user-128.syx`. H-030 and the app-restore alternative of H-035 now pass. The direct SysEx librarian route in H-035 was not separately attempted.

## Cursor handoff after Codex (2026-11-07, continued)

Codex had already exported ten Library patches and written them with `library_export_roundtrip`, then aborted restore when `plan_user_bank` missed User patch 66. `recover_library_ten` restored slots 1-10. A later `verify_snapshot` showed User/Performance/Rhythm equal to the session backup and reported System differences.

Read-only System diffs (no write): versus the session full backup, live System differed only in `patch_group_id` (1→3), `patch_number` (127→3) and `chorus_switch` (1→0). Versus the H-065 System backup, only `chorus_switch` differed (live OFF). Master Tune and Scale Tune were not left dirty. Panel selection / Chorus Switch state was left as the owner has it; no System restore was performed.

Soft Thru investigation: not found as an XP-60 panel name in the uploaded parameter/protocol docs or Patch Base binary strings; client comments treat it as interface/loopback echo. H-018 and H-038 stay blocked until the owner names the Soft Thru source. Evidence: `hardware-evidence/soft-thru-investigation.txt`.

H-037 retry: hardened `library_export_roundtrip` to plan only the ten slots and restore with per-slot reads plus one independent retry (same flaky-read pattern seen on User 66/87/88). Roundtrip then passed end-to-end; full User bank equalled the prewrite backup. Evidence: `hardware-evidence/H-037.txt`.

Still open for G2/G3/G8: owner-assisted items (H-039 stale save, Soft Thru if they identify it, audible/panel observations), remaining PARTIAL rows, final cleanup snapshot after the owner is finished with panel state.

Hardware continuation with instrument connected (Cursor):
- Independent reads: temporary Nice Piano equals hover-session original (589 bytes); User 1 and User 128 equal session full backup.
- H-048 Cancel: mid-read Cancel at 8/128 stopped with peak 8. Misleading Cancelled text about temporary-patch residual on a read-only cancel was corrected to generic `cancelled`; rebuilt app rechecked.
- H-051 answered from existing H-010 evidence (Tone RQ1 replies are 129 data bytes), so the 128-byte limit is for DT1 writes, not reads.
- H-049 MIDI-tool half: after copying User 1 onto User 128, `restore_bank_file` restored from the session full-backup `.syx`; all 128 slots equalled the file.
- H-053: Bank `.mid` export received via `smf_to_syx` + ten-slot write/verify/restore (`mid_bank_receive`). External sequencer playback not claimed.
- H-042 Scale Tune separate RQ1s (17×12 bytes) equal full System Scale Tune blocks; panel comparison still owner.
- H-046 address half: temp and User Rhythm 1 DT1 note lanes are MIDI 35..98; panel key names still owner.
- H-023 remaining procedure recorded: no 3,125 B/s byte-rate throttle in the product; Chunk covered by H-056.

### Explicit remaining procedures (G2)

Every open ledger row has evidence or an explicit remaining procedure. The unresolved observations below remain for full hardware parity, while G2's evidence-or-procedure gate is met.

| Item | Remaining |
|---|---|
| H-018 / H-038 | XP-60 Sequencer Through source found; Through ON echoed DT1 exactly but SysEx reads timed out. H-038 stopped at pre-write backup. Through OFF plus XP-60 restart restored WIDI replies. See H-018-through-on.txt / H-038-through-protected.txt. |
| H-023 | PASS (H-023.txt). |
| H-022 / H-044 / H-045 / H-051c audible | H-022 PASS: owner heard Patch Level 120→0 become silent or much quieter, with read-back and Undo/restore verified (`H-022-live-audible.txt`). H-051c mouse listening PASS (`H-051c-mouse-listen.txt`). H-044/H-045 listening remains owner. |
| H-027 / H-042 panel / H-043 / H-046 panel / H-062 panel | Owner panel LCD observation. |
| H-059 panel | SysEx raw values PASS (H-059.txt); owner confirms LCD labels for 7/96/97/98–104 before fancy names. |
| H-052 | PASS: two panel DEFAULT Init captures were identical; native XP-60 Init Patch action, Send/read-back, Undo/restore matched (`H-052-panel-init.txt`). |
| H-058 panel / H-072 panel | H-058 Nice Piano Tone 1 LCD INT-A 004 (Ac Piano2 pA) confirmed against MIDI bytes and app; read-only User 97 verifies INT-B numeric mapping. INT-B/EXP LCD names and Part preset mapping remain. H-072 XP-A→St.Concert, type=2 id=9; owner board/card confirmation remains (`H-058-internal-wave-check.txt`, `H-072.txt`). |
| H-060 / H-067 | Owner LCD tables (tempo-synced note lengths; Peaking Q / Cutoff Hz / Rate / Delay). |
| H-068 routing | SysEx ACCEPTED raw 3 / raw 2 (H-068.txt); owner which output (if any) carries `<OUTPUT-2>`. |
| H-069 | Owner audible routing listen (08-effects-routing-verification.md). |
| H-070 LCD | Optional owner panel names (MIDI-tool + Bank UI already PASS). |
| H-039 / H-063 / H-071 panel wording | Optional owner confirmation of SysEx stand-in refuse paths. |
| H-073 / H-074 / H-075 | Owner EFX-type walk / slot-order probe / structure listen. SysEx type-only write does not load defaults (H-073-sysex-note.txt). |
| G8 final stamp | Owner authorized restoring initial session System and Jimmee Dee temporary patch. Latest full independent snapshot after H-076 equal; final temporary read after pitch-bend/modulation repeat equal. |

### Owner-assist checklist (remaining full hardware parity)

Agent-runnable SysEx halves are done where possible. Items below still need owner observations for their individual acceptance rows.

1. Through echo (H-018 / H-038): XP-60 Sequencer Through ON echoes DT1 but makes RQ1 read-back fail on this WIDI path. A protected save cannot advance past its pre-write backup while Through ON; do not claim a physical protected-write MISMATCH. The virtual echo regression passes.
2. H-059 panel: after a Control Source / Pedal Assign write of 7, 96, 97, 98–104 (or use the app System panel), say what the XP-60 LCD shows. Do not enable fancy controller labels in the product until this matches the hypothesis.
3. H-052 completed: the panel DEFAULT Init is captured twice and exposed as the separate XP-60 Init Patch action.
4. LCD glances still open: H-027, H-042 Scale Tune, H-043, H-046 key names, H-058 INT-B/expansion wave names (INT-A Tone 1 confirmed), H-060 note lengths, H-062, H-067 EFX displays, H-070 selection names, H-072 XP-A / missing board. The owner prefers MIDI-request comparisons over manual patch-by-patch searching.
5. Audible: H-022 and H-051c completed; H-044 / H-045 listen; H-068 `<OUTPUT-2>` routing; H-069 routing diagram; H-075 Structure wiring listen.
6. EFX: H-073 type-default walk on the panel (SysEx type write does not load defaults); H-074 one-parameter slot order on unusual types.
7. G8 cleanup: owner-approved initial System and Jimmee Dee temporary patch are restored; current snapshot and final temporary read passed. Repeat independent cleanup only after any future MIDI write test.

### Gate audit (2026-11-07, G2/G8 closed)

| Gate | Status | Note |
|---|---|---|
| G1 | met | Bank interrupt / restore already evidenced. |
| G2 | met | Every ledger item has PASS/PARTIAL/BLOCKED evidence or an explicit remaining procedure (07 + this file). H-052 is PASS. Owner Soft Thru / LCD / audible / H-059 panel / H-060 / H-067 / H-068 routing / H-069 / H-073–075 remain as recorded remaining procedures; SysEx rows stay not-DONE in 04 until owner confirms. |
| G3 | met | A1–G9 inventory reconciled in this file. |
| G4–G7 | met | Rust workspace tests, UI type check, 75 UI test files / 626 tests, and final custom-protocol desktop end-to-end run pass. The latest desktop run includes the repaired save confirmation checks. |
| G8 | met | Latest post-H-058 full independent snapshot: all 128 User patches, 32 Performances, two Rhythm Sets, System settings and Jimmee Dee temporary patch equal the original backup. The first read found only panel-selection System bytes; these were backed up and restored under the owner's earlier reset authorization before the passing second read (`H-058-internal-wave-check.txt`). |

### Owner-approved reset to the session snapshot

The owner explicitly authorized resetting the instrument to its initial backed-up state. The original session temporary patch is `Jimmee Dee` (589-byte `temp-patch.blocks`). `restore_system_file` saved the live System settings before writing the System document from the session full backup. `restore_temp` saved the live temporary patch (`C-Z  Flute`) before restoring `Jimmee Dee`. Separate `verify_snapshot` reads found zero differences across 128 User patches, 32 Performances, two Rhythm Sets, System settings, and the temporary patch. Evidence: `hardware-evidence/owner-approved-initial-restore.txt`. This is an interim reset; another final read is required after the remaining owner-assisted checks.

The XP-60/80 Owner's Manual p.117 identifies Sequencer Setup `Through`. With the owner setting Through ON, a 12-byte temporary-patch DT1 was echoed exactly over the WIDI/FCB1010 path (94.77 ms). The helper's restore verification then timed out, as did read-only probes after the owner turned Through OFF. The WIDI adapter was reseated; Windows subsequently listed its BLE MIDI endpoint but could not open the WinMM input. Windows Bluetooth reconnect is pending. No permanent memory write occurred in this echo probe. H-018 is partial, H-038 remains unrun, and G8 is reopened until the original Jimmee Dee temp and full snapshot are independently checked again. Evidence: `hardware-evidence/H-018-through-on.txt`.

The new secondary controller pass-through is recorded as A10 in `10-function-device-verification.md` and H-076 in the hardware ledger. Its unit tests cover message filtering and channel remapping; physical KL Essential 49 mk3 → XP-60 forwarding, SysEx hold, and no-hanging-note checks remain open until the Bluetooth MIDI port is restored.

After owner-confirmed Through ON and both Protect switches ON, `protect_probe` failed in the pre-write backup of User 128 (missing block `11 7F 12 00` after three RQ1 attempts). It sent no write, so H-038 is blocked at pre-write read rather than passing its expected MISMATCH. Owner returned all three settings OFF and restarted the XP-60; WIDI replies resumed. The session `Jimmee Dee` temporary patch was sent/read back equal, and another separate full snapshot read found all 128 User patches, 32 Performances, two Rhythm Sets, System and temporary patch equal. Evidence: `H-038-through-protected.txt`, `g8-after-through-recovery.txt`. G8 is met again for the work through H-038. Further hardware tests require another cleanup read.

H-076 secondary-controller native UI test: KL Essential 49 mk3 input forwarded Direct notes, pitch bend and modulation CC audibly to XP-60; channel 2 remap silenced the notes as expected for Patch Rx channel 1. When a full 128-patch read began, app status entered held=true and the owner's sustained note stopped; read completed with 128 patches, status resumed and notes sounded again without hangs. Stop closed thru. A separate full snapshot read again matched all memory and the temporary patch; a final temporary read after the focused pitch-bend/modulation repeat also matched saved Jimmee Dee. Default real-time filter status=true and unit tests pass, but no physical clock/Active Sense flood was injected. H-076 remains partial for that one observation (`H-076-controller-thru.txt`).
