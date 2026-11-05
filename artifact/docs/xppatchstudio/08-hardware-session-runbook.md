# 08. Hardware session runbook (phase 9)

One session on a physical XP-60, driven by the items in `07-hardware-acceptance.md`. Nothing here has been run yet.

## Before you start

- XP-60 connected by USB or MIDI interface; note the model/firmware, OS, MIDI interface and cable in `docs/hardware-evidence/session.txt`.
- Turn **User Memory Protect OFF** for part 3 only; turn it ON for H-031, H-038, H-050b (protected case).
- Put a spare bank on the synth or accept that part 3 overwrites the User slots you choose (use spare slots, e.g. 120 to 128).
- Start the app (`cd crates/xppatch-app && cargo tauri dev`), choose the ports, press Connect. The app scans device IDs 17 to 32 itself.
- Read-only probe from a terminal: `cargo run -p xppatch-midi --example probe -- "<input name>" "<output name>"`.
- For each item: do the procedure, compare with Expected, save evidence (screenshot, log, MIDI monitor capture) as `docs/hardware-evidence/<ID>.txt`, then set the status in the ledger to PASS or FAIL with a one-line note. Stop at the first unexpected result in part 3.
- Only after an item passes may its row in `04-parity-matrix.md` change from IMPLEMENTED to DONE.

## 1. READ (nothing is written)

| ID | What to confirm | Procedure | Expected | Result |
|---|---|---|---|---|
| H-001 | Windows MIDI backend sees the XP-60 in and out ports; SysEx passes both ways | list ports; select in/out | both ports listed; a request gets a reply | |
| H-002 | Default device ID `0x10` (shown 17); model ID `0x6A` | RQ1 temp patch name (`03 00 00 00`, 12 bytes) | DT1 reply with model `6A`, device `10` | |
| H-003 | Wrong device ID gets no reply; device-ID scan finds the right one | request with another ID; run scan | silence then success at the right ID | |
| H-004 | Identity Request support (unknown) | send `F0 7E dev 06 01 F7` | record whether it answers | |
| H-010 | Temporary patch arrives as 5 DT1: common 73, tones 129 each, in one message each | fetch temp patch | 5 messages, sizes 73/129x4 | |
| H-011 | One outstanding RQ1 at a time; a second request while replies are streaming is dropped | issue overlapping RQ1s with the diagnostic tool | second is ignored (legacy-verified, re-confirm) | |
| H-012 | Reply spacing and first-reply latency (feeds our timeout defaults, 1,500 ms first response) | timestamped capture of several fetches | spacing about 37 to 70 ms, first reply well under 1.5 s | |
| H-013 | Sparse span reply: temp performance span `00 00 1F 19` (3,993) | request `01 00 00 00` size `00 00 1F 19` | 17 DT1, 466 data bytes | |
| H-014 | Partial block read (patch name, 12 bytes) returns exactly those bytes | RQ1 `03 00 00 00` size 12 | 12-byte DT1 | |
| H-015 | Reading all 128 User patches matches the golden bank format and decodes with zero range issues | bulk fetch user bank | 128 patches, 0 issues; compare with a known dump | |
| H-016 | Retry behaviour on a real dropped reply (unplug/replug or busy instrument) | interrupt during fetch | clear timeout report naming missing blocks; retry succeeds | |
| H-017 | Panel patch selection is announced as Bank Select + Program Change (stale-state detection) | select patches on the panel while monitoring | events seen; app marks its temp copy STALE | |
| H-018 | Soft Thru / loopback echo: with Soft Thru on, our own writes do not satisfy the read-back (the client listens about 0.4 ms per byte sent before reading) | write temp patch with Soft Thru on, then off; compare reports | with Soft Thru on, verified still reflects the instrument's reply; if an echo is accepted, lengthen the window | |
| H-019 | Absent or wrong port: a bank read with nothing answering gives up after 3 silent slots; ambiguous or empty port names are refused | run bank fetch with the cable unplugged; pass a name matching two ports | `aborted` after 3 slots (about 14 s); port error lists candidates | |
| H-048 | Reading all 128 User patches takes an acceptable time and every slot decodes with no range issues; cancel stops within one slot | Bank workspace, Read from synth; repeat with Cancel | 128 names shown; time noted (expect about 30 to 60 s) | |
| H-057 | Back up all memory reads the 128 patches, 32 performances and 2 rhythm sets without error and the performance and rhythm names match the panel | Bank workspace, Back up all memory | a file is offered; note the time (expect a few minutes) and compare a few names | |
| H-040 | Performance (common 66, parts 25) fetch and decode; tables beyond the bank fixture | fetch temp and user performance | decode with 0 range issues; compare with panel | |
| H-041 | Rhythm Setup (common 12, notes 58) fetch and decode | fetch user rhythm | 0 range issues | |
| H-042 | System common (96) and Scale Tune (12 x 17) fetch; the System tables have no bank backing | fetch system | 0 range issues; values match the panel | |
| H-043 | Parameter meaning and units (times, rates, EFX-specific) against the instrument | change on panel, re-fetch, compare | table notes updated | |

## 2. TEMP (temporary memory; harmless)

| ID | What to confirm | Procedure | Expected | Result |
|---|---|---|---|---|
| H-020 | Writing the patch just read back to the temporary area, split 128 + 1 with a 20 ms gap, then reading back | fetch temp, write temp, fetch temp | read-back equal, sound unchanged | |
| H-021 | Minimum safe gap between DT1 (documented 20 ms) | repeat H-020 at 20, 15, 10, 5 ms | find the smallest gap with 10 of 10 equal read-backs; keep 20 ms default | |
| H-022 | Live edit stream: debounced single-parameter DT1 to the temp patch is audible and read-back equal | move a control (tone level) | audible; read-back matches | |
| H-023 | Throttle options (chunk 32, 3,125 B/s) do not corrupt or slow unacceptably | enable byte-rate throttle on a large send | equal read-back | |
| H-024 | Hover audition sends to the temp area and restores the previous temp patch | hover a library patch, leave | previous sound restored | |
| H-025 | A single-parameter DT1 (one 7-bit byte) to the temporary patch changes only that parameter, for one parameter of each kind: Tone Level, LFO waveform, a signed value, Wave Number | change each on the app, watch the panel/sound, fetch and compare | only that value changes; read-back equals the app | |
| H-026 | A multi-byte (nibble) parameter, Patch Tempo, is accepted as one 2-byte DT1 | edit Patch Tempo with live edit on; fetch | tempo changes; read-back equal | |
| H-027 | Display meanings match what the instrument's own panel shows (footnote tables 3 to 13, pan L/R, signed ranges, stepped -100..+150 values, Wave Number 001..255) | pick about 20 parameters, set a raw value in the app, read the panel's display | every text matches; any mismatch is a bug in `display.rs` or the transcribed footnote | |
| H-028 | Fetch sound opens the instrument's temporary patch and Verify reports TEMP VERIFIED; a panel patch change then turns the chip STALE | connect; Fetch; Verify; select another patch on the panel | chip goes TEMP VERIFIED, then STALE within about a second | |
| H-029 | Live-edit pacing under a fast drag does not drop or corrupt writes (coalesced, 20 ms apart) | drag a knob quickly for 10 s, then fetch | the final value is on the instrument; no stuck intermediate values | |
| H-051c | Notes played from the Audition panel sound on the chosen channel, a held note is released on key-up, and Panic silences the channel | connect; play keys with mouse and computer keyboard; switch channel while holding | sound matches; no stuck notes | |
| H-044 | Temporary Performance (`01 00 00 00`) accepts single-parameter DT1 live edits in Performance mode: Part Level, MIDI Channel, Key Range | switch the XP-60 to Performance mode; edit in the app; listen; fetch and compare | audible change; read-back equals the app | |
| H-045 | The Rhythm Setup temporary area is Part 10's area at `02 09 00 00` (rhythm notes at `02 09 nn 00`, nn = key 35 to 98) | edit a rhythm note level while Part 10 plays a drum key; fetch `02 09 00 00` | audible change on that key; read-back equal; if wrong, correct `DocKind::temp_base` | |
| H-046 | A rhythm key's address byte is the MIDI note number (note 35 at `23 00` to note 98 at `62 00`) | fetch a User Rhythm Setup and compare with the key names on the panel | the note-to-address mapping matches the panel | |
| H-047 | Whole-document sends (17 blocks for a Performance, about 130 frames for a Rhythm Set) arrive complete at 20 ms spacing | send each; fetch; compare | read-back equal; time taken noted | |

## 3. PERMANENT (each one preceded by its backup; stop at the first surprise)

| ID | What to confirm | Procedure | Expected | Result |
|---|---|---|---|---|
| H-030 | Write ceremony: arm, choose slot, auto-backup, write, read-back compare | write one patch to a spare User slot | state SAVED; read-back equal; backup restorable | |
| H-031 | User Memory Protect ON refuses the write and the app reports it as a mismatch with the cause | enable protect on the instrument; repeat H-030 | MISMATCH/refused with explanation; nothing changed | |
| H-032 | Bank write of 128 patches with progress, cancel and verify | write a test bank to User | all equal; cancel stops cleanly | |
| H-033 | Restore from a backup made by the app | restore the H-030 backup | original patch back | |
| H-034 | The write into a User patch slot works as 9 DT1 messages (Common, then each Tone as 128 + 1 bytes) 20 ms apart, with a 50 ms settle before the read-back | do H-030 and capture the MIDI traffic | every message accepted; read-back equal; no dropped writes | |
| H-035 | The backup file written before a save (`<seconds>-user-NNN.syx`) is a valid User bank file the instrument can receive | send the backup file to the instrument with another tool, or restore it from the app | the old patch is back | |
| H-036 | Fetching a patch from a User slot after writing it returns exactly what was written (including the patch name) | write, fetch, compare in the app | equal | |
| H-037 | Library export (`.syx` of up to 128 patches) can be received by the instrument | export 10 patches; send them with a MIDI tool to spare slots | patches arrive intact | |
| H-038 | With Soft Thru on, a save to a protected User slot is reported as MISMATCH (our own writes echoed back must not satisfy the read-back) | turn User Memory Protect on and Soft Thru on; attempt a save | MISMATCH, nothing changed | |
| H-039 | The save refuses when the slot changed after it was backed up, and Undo refuses when the slot changed after the save | arm a save, change that patch from the panel, commit; then save, edit the slot on the panel, press Undo | both refused with the explanation; nothing overwritten | |
| H-049 | The whole-bank backup file (`<seconds>-user-bank.syx`, 128 patches) is received by the instrument and restores the bank | arm a bank write, then send the backup file back with a MIDI tool | all 128 patches equal to before | |
| H-050b | A bank write of a few changed slots writes only those slots, reads each back, and stops at the first failure (protect on) | change 3 slots; write; then repeat with protect on | 3 slots equal; with protect on it stops at the first slot and nothing else changes | |

