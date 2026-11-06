# 07. Hardware acceptance ledger (the single hardware phase)

**Rule (owner decision):** nothing is tested on the physical XP-60 phase by phase. Every item that needs the instrument is
recorded **here**, with its procedure and expected result, and all of them are run **together in one session at the end**,
when the application is fairly and properly implemented. Each phase appends new items or edits existing ones when new
requirements appear. No row in `04-parity-matrix.md` that depends on the instrument is marked DONE before its items here
are PASS and you have confirmed them.

Until then everything is built and tested against the virtual XP-60 (`xppatch-midi::testing`), the real-bank golden
fixture and the manual transcription.

## How to use this ledger

- **Status:** `NOT RUN` (default), `PASS`, `FAIL`, `BLOCKED`, `CHANGED` (procedure edited after a requirement change).
- **Risk:** `READ` (requests only, cannot change the instrument), `TEMP` (writes the temporary area, lost on the next
  sound change), `PERM` (writes User/System memory: destructive, needs backup first and your explicit go).
- **Order of the session:** all READ items, then TEMP, then PERM last, each PERM item preceded by a backup of what it will
  overwrite. Stop at the first unexpected result.
- **Evidence:** for each item keep the raw capture (hex of messages with timestamps) in `docs/hardware-evidence/<ID>.txt`
  and the result in this table. The test tool will produce these files.
- **Adding items:** when a phase introduces behaviour that depends on the instrument, add a row in the same commit.

## Items

### Connection and identity (READ)

| ID | Phase | What to confirm | Procedure | Expected | Status |
|---|---|---|---|---|---|
| H-001 | 2 | Windows MIDI backend sees the XP-60 in and out ports; SysEx passes both ways | list ports; select in/out | both ports listed; a request gets a reply | NOT RUN |
| H-002 | 2 | Default device ID `0x10` (shown 17); model ID `0x6A` | RQ1 temp patch name (`03 00 00 00`, 12 bytes) | DT1 reply with model `6A`, device `10` | NOT RUN |
| H-003 | 2 | Wrong device ID gets no reply; device-ID scan finds the right one | request with another ID; run scan | silence then success at the right ID | NOT RUN |
| H-004 | 2 | Identity Request support (unknown) | send `F0 7E dev 06 01 F7` | record whether it answers | NOT RUN |

### Reads and timing (READ)

| ID | Phase | What to confirm | Procedure | Expected | Status |
|---|---|---|---|---|---|
| H-010 | 2 | Temporary patch arrives as 5 DT1: common 73, tones 129 each, in one message each | fetch temp patch | 5 messages, sizes 73/129x4 | NOT RUN |
| H-011 | 2 | One outstanding RQ1 at a time; a second request while replies are streaming is dropped | issue overlapping RQ1s with the diagnostic tool | second is ignored (legacy-verified, re-confirm) | NOT RUN |
| H-012 | 2 | Reply spacing and first-reply latency (feeds our timeout defaults, 1,500 ms first response) | timestamped capture of several fetches | spacing about 37 to 70 ms, first reply well under 1.5 s | NOT RUN |
| H-013 | 2 | Sparse span reply: temp performance span `00 00 1F 19` (3,993) | request `01 00 00 00` size `00 00 1F 19` | 17 DT1, 466 data bytes | NOT RUN |
| H-014 | 2 | Partial block read (patch name, 12 bytes) returns exactly those bytes | RQ1 `03 00 00 00` size 12 | 12-byte DT1 | NOT RUN |
| H-015 | 2 | Reading all 128 User patches matches the golden bank format and decodes with zero range issues | bulk fetch user bank | 128 patches, 0 issues; compare with a known dump | NOT RUN |
| H-016 | 2 | Retry behaviour on a real dropped reply (unplug/replug or busy instrument) | interrupt during fetch | clear timeout report naming missing blocks; retry succeeds | NOT RUN |
| H-017 | 2 | Panel patch selection is announced as Bank Select + Program Change (stale-state detection) | select patches on the panel while monitoring | events seen; app marks its temp copy STALE | NOT RUN |
| H-018 | 2 | Soft Thru / loopback echo: with Soft Thru on, our own writes do not satisfy the read-back (the client listens about 0.4 ms per byte sent before reading) | write temp patch with Soft Thru on, then off; compare reports | with Soft Thru on, verified still reflects the instrument's reply; if an echo is accepted, lengthen the window | NOT RUN |
| H-019 | 2 | Absent or wrong port: a bank read with nothing answering gives up after 3 silent slots; ambiguous or empty port names are refused | run bank fetch with the cable unplugged; pass a name matching two ports | `aborted` after 3 slots (about 14 s); port error lists candidates | NOT RUN |

### Temporary writes (TEMP)

| ID | Phase | What to confirm | Procedure | Expected | Status |
|---|---|---|---|---|---|
| H-020 | 2 | Writing the patch just read back to the temporary area, split 128 + 1 with a 20 ms gap, then reading back | fetch temp, write temp, fetch temp | read-back equal, sound unchanged | NOT RUN |
| H-021 | 2 | Minimum safe gap between DT1 (documented 20 ms) | repeat H-020 at 20, 15, 10, 5 ms | find the smallest gap with 10 of 10 equal read-backs; keep 20 ms default | NOT RUN |
| H-022 | 4 | Live edit stream: debounced single-parameter DT1 to the temp patch is audible and read-back equal | move a control (tone level) | audible; read-back matches | NOT RUN |
| H-023 | 4 | Throttle options (chunk 32, 3,125 B/s) do not corrupt or slow unacceptably | enable byte-rate throttle on a large send | equal read-back | NOT RUN |
| H-024 | 6 | Hover audition sends to the temp area and restores the previous temp patch | hover a library patch, leave | previous sound restored | NOT RUN |
| H-025 | 4 | A single-parameter DT1 (one 7-bit byte) to the temporary patch changes only that parameter, for one parameter of each kind: Tone Level, LFO waveform, a signed value, Wave Number | change each on the app, watch the panel/sound, fetch and compare | only that value changes; read-back equals the app | NOT RUN |
| H-026 | 4 | A multi-byte (nibble) parameter, Patch Tempo, is accepted as one 2-byte DT1 | edit Patch Tempo with live edit on; fetch | tempo changes; read-back equal | NOT RUN |
| H-027 | 4 | Display meanings match what the instrument's own panel shows (footnote tables 3 to 13, pan L/R, signed ranges, stepped -100..+150 values, Wave Number 001..255) | pick about 20 parameters, set a raw value in the app, read the panel's display | every text matches; any mismatch is a bug in `display.rs` or the transcribed footnote | NOT RUN |
| H-028 | 4 | Fetch sound opens the instrument's temporary patch and Verify reports TEMP VERIFIED; a panel patch change then turns the chip STALE | connect; Fetch; Verify; select another patch on the panel | chip goes TEMP VERIFIED, then STALE within about a second | NOT RUN |
| H-029 | 4 | Live-edit pacing under a fast drag does not drop or corrupt writes (coalesced, 20 ms apart) | drag a knob quickly for 10 s, then fetch | the final value is on the instrument; no stuck intermediate values | NOT RUN |

### Permanent writes (PERM; backup first, owner present)

| ID | Phase | What to confirm | Procedure | Expected | Status |
|---|---|---|---|---|---|
| H-030 | 6 | Write ceremony: arm, choose slot, auto-backup, write, read-back compare | write one patch to a spare User slot | state SAVED; read-back equal; backup restorable | NOT RUN |
| H-031 | 6 | User Memory Protect ON refuses the write and the app reports it as a mismatch with the cause | enable protect on the instrument; repeat H-030 | MISMATCH/refused with explanation; nothing changed | NOT RUN |
| H-032 | 6 | Bank write of 128 patches with progress, cancel and verify | write a test bank to User | all equal; cancel stops cleanly | NOT RUN |
| H-033 | 6 | Restore from a backup made by the app | restore the H-030 backup | original patch back | NOT RUN |
| H-034 | 6 | The write into a User patch slot works as 9 DT1 messages (Common, then each Tone as 128 + 1 bytes) 20 ms apart, with a 50 ms settle before the read-back | do H-030 and capture the MIDI traffic | every message accepted; read-back equal; no dropped writes | NOT RUN |
| H-035 | 6 | The backup file written before a save (`<seconds>-user-NNN.syx`) is a valid User bank file the instrument can receive | send the backup file to the instrument with another tool, or restore it from the app | the old patch is back | NOT RUN |
| H-036 | 6 | Fetching a patch from a User slot after writing it returns exactly what was written (including the patch name) | write, fetch, compare in the app | equal | NOT RUN |
| H-037 | 6 | Library export (`.syx` of up to 128 patches) can be received by the instrument | export 10 patches; send them with a MIDI tool to spare slots | patches arrive intact | NOT RUN |
| H-038 | 6 | With Soft Thru on, a save to a protected User slot is reported as MISMATCH (our own writes echoed back must not satisfy the read-back) | turn User Memory Protect on and Soft Thru on; attempt a save | MISMATCH, nothing changed | NOT RUN |
| H-039 | 6 | The save refuses when the slot changed after it was backed up, and Undo refuses when the slot changed after the save | arm a save, change that patch from the panel, commit; then save, edit the slot on the panel, press Undo | both refused with the explanation; nothing overwritten | NOT RUN |
| H-048 | 6 | Reading all 128 User patches takes an acceptable time and every slot decodes with no range issues; cancel stops within one slot | Bank workspace, Read from synth; repeat with Cancel | 128 names shown; time noted (expect about 30 to 60 s) | NOT RUN |
| H-049 | 6 | The whole-bank backup file (`<seconds>-user-bank.syx`, 128 patches) is received by the instrument and restores the bank | arm a bank write, then send the backup file back with a MIDI tool | all 128 patches equal to before | NOT RUN |
| H-050b | 6 | A bank write of a few changed slots writes only those slots, reads each back, and stops at the first failure (protect on) | change 3 slots; write; then repeat with protect on | 3 slots equal; with protect on it stops at the first slot and nothing else changes | NOT RUN |
| H-051c | 4 | Notes played from the Audition panel sound on the chosen channel, a held note is released on key-up, and Panic silences the channel | connect; play keys with mouse and computer keyboard; switch channel while holding | sound matches; no stuck notes | NOT RUN |

### Other regions (READ first, then PERM if needed)

| ID | Phase | What to confirm | Procedure | Expected | Status |
|---|---|---|---|---|---|
| H-040 | 5 | Performance (common 66, parts 25) fetch and decode; tables beyond the bank fixture | fetch temp and user performance | decode with 0 range issues; compare with panel | NOT RUN |
| H-041 | 5 | Rhythm Setup (common 12, notes 58) fetch and decode | fetch user rhythm | 0 range issues | NOT RUN |
| H-042 | 5 | System common (96) and Scale Tune (12 x 17) fetch; the System tables have no bank backing | fetch system | 0 range issues; values match the panel | NOT RUN |
| H-043 | 5 | Parameter meaning and units (times, rates, EFX-specific) against the instrument | change on panel, re-fetch, compare | table notes updated | NOT RUN |
| H-044 | 5 | Temporary Performance (`01 00 00 00`) accepts single-parameter DT1 live edits in Performance mode: Part Level, MIDI Channel, Key Range | switch the XP-60 to Performance mode; edit in the app; listen; fetch and compare | audible change; read-back equals the app | NOT RUN |
| H-045 | 5 | The Rhythm Setup temporary area is Part 10's area at `02 09 00 00` (rhythm notes at `02 09 nn 00`, nn = key 35 to 98) | edit a rhythm note level while Part 10 plays a drum key; fetch `02 09 00 00` | audible change on that key; read-back equal; if wrong, correct `DocKind::temp_base` | NOT RUN |
| H-046 | 5 | A rhythm key's address byte is the MIDI note number (note 35 at `23 00` to note 98 at `62 00`) | fetch a User Rhythm Setup and compare with the key names on the panel | the note-to-address mapping matches the panel | NOT RUN |
| H-047 | 5 | Whole-document sends (17 blocks for a Performance, about 130 frames for a Rhythm Set) arrive complete at 20 ms spacing | send each; fetch; compare | read-back equal; time taken noted | NOT RUN |

### Decisions the hardware settles

| ID | Phase | Question | How | Status |
|---|---|---|---|---|
| H-050 | 2 | Unit and use of Patch Base's wait value 30 between messages | measure whether any gap above 20 ms helps | NOT RUN |
| H-051 | 2 | Is the 128-byte data limit also enforced for reads (RQ1) or only DT1 writes? | request larger spans | NOT RUN |
| H-052 | 7 | Does Init need device defaults? (the manual lists ranges, not defaults) | compare an initialised panel patch with our Init | NOT RUN |
| H-053 | 6 | A `.mid` exported by the Library or Bank plays back into the XP-60 intact (25 ms between messages is enough) | export a few patches as `.mid`, play the file from a sequencer to the XP-60, read the patches back and compare | NOT RUN |
| H-054 | 4 | A controller moved on the XP-60 (mod wheel, sliders, pedal, knobs) arrives as Control Change on MIDI OUT and drives the linked parameter; Bank Select and Program Change still mark the sound stale | link Cutoff to the wheel with Link to MIDI CC, sweep it, check the parameter and one undo step; then press a patch on the panel and check STALE | NOT RUN |
| H-055 | 6 | Hear on hover: a library patch sounds in the temporary patch and the instrument's own sound comes back exactly, with no change to User memory | with the toggle on, rest the pointer on a few rows, play keys, leave the list; read the temporary patch and compare with the one read before; check User patch 1 is unchanged | NOT RUN |
| H-056 | 2 | A whole sound sent in smaller chunks (for example 32 bytes) arrives identical to one sent in 128-byte chunks | send the same patch at 128 and at 32, read each back, compare; note the time each takes | NOT RUN |
| H-057 | 6 | Back up all memory reads every User patch, all 32 User Performances (`10 nn 00 00`) and both User Rhythm Sets (`10 40 00 00`, `10 41 00 00`) from the instrument, and the file matches what the instrument holds | Bank workspace, Back up all memory; load the file back into a sequencer or compare the performance and rhythm names with the panel | NOT RUN |
| H-058 | 4 | The wave named on a Tone card matches the wave the instrument plays: group type 0 with group ID 1 is INT-A and 2 is INT-B; group type 2 with group ID n is SR-JV80 board n | step a few Tones through INT-A, INT-B and an installed expansion board on the instrument's panel and compare the wave name it shows with the one the app shows; for expansion also check the board number against the card in the slot; note which Preset bank a Part's Patch Group ID and Number select (not yet known, so no Preset names are shown in the Part dropdowns) |
| H-059 | 4 | TO CONFIRM BEFORE ANY LABELS ARE SHOWN: a controller source or assignment value is the controller number: raw 7 is CC07, raw 96 is PITCH BEND, raw 97 is AFTERTOUCH, and (for pedals) raw 98 to 104 are PROG-UP, PROG-DOWN, START/STOP, PUNCH-I/O, TAP-TEMPO, OCT-UP, OCT-DOWN | set System Control Source 1, a Pedal Assign and Controller 1 Assign to a few of these values from the app and read the instrument's panel; try one number the manual does not offer (6 or 40) and note what the instrument does |
| H-070 | 6 | Pressing a patch on the Bank screen selects it on the instrument: Bank Select MSB, Bank Select LSB and Program Change as in the Owner's Manual p.181 (USER 80/0, PR-A 81/0, PR-B 81/1, PR-C 81/2, GM 81/3; program = patch number minus 1), on the channel chosen next to the toggle | System > MIDI: Receive Program Change and Receive Bank Select on, Patch receive channel known; in Bank, switch "Select on the instrument" on and press a USER patch, PR-A 1, PR-B 10, PR-C 128 and GM 1; watch the display on the XP-60 | the panel shows the same group, number and name each time; the sound is the one named in the app (a preset by its factory name) |
| H-071 | 6 | The same selection works with the switches off the other way round: with Receive Bank Select OFF only the Program Change is used (the manual says only the current group then changes) and a wrong channel selects nothing | turn Receive Bank Select off and press patches in another group; set the app's channel to a different one and press a patch | with Bank Select off the group does not change; with a wrong channel nothing changes; neither breaks the next press |
| H-072 | 6 | Expansion groups XP-A to XP-D (84 / 0, 2, 4, 6) select the first 128 patches of an installed board; the Bank screen greys these buttons out until confirmed | with a board in slot A, send 84 / 0 and program 1 from a MIDI tool and compare with the panel's XP-A 001; note what a missing board does | XP-A 001 appears when the board is installed; nothing changes or the display says the board is missing otherwise |

### Writing the System settings (PERM)

Risk: this replaces the instrument's System settings. Each item is preceded by the app's own backup of the settings (a `.syx` file in the backups folder); run them in this order and stop at the first unexpected result.
The System settings include MIDI channels and switches: use a harmless value first (Master Tune by one step), never a channel or receive switch, until H-061 to H-063 pass.

| ID | Phase | What to confirm | Procedure | Expected | Status |
|---|---|---|---|---|---|
| H-061 | 8 | Arming reads the System settings (`00 00 00 00` and the Scale Tune block at `00 00 10 00` to `00 00 1B 00`) and writes a backup that equals what was read | System workspace, Write to synth, step 1; open the backup file in the app (File > Open does not take System files: compare it by exporting the System settings from the app and checking both files hold the same messages) | the backup file exists, reads back as the same settings, nothing changed on the instrument | NOT RUN |
| H-062 | 8 | Writing changed System settings works and reads back equal | change Master Tune by one step, write (step 2) | the panel shows the new tune; the app reports "read back and match" | NOT RUN |
| H-063 | 8 | Undo puts the earlier settings back, and is refused when something changed since | undo the write; then change a System setting on the instrument panel and try an undo of a later write | the tune returns; the second undo is refused with the backup path | NOT RUN |
| H-064 | 8 | With User Memory Protect on, the write is reported as a mismatch and the backup stays | turn Memory Protect on, write | the app reports MISMATCH and offers undo; nothing is lost | NOT RUN |
| H-065 | 8 | The chunking (at most 128 data bytes, 20 ms apart) is accepted for the System addresses, and the Scale Tune block (12 x 17 bytes) is written whole | write after changing one Scale Tune value | read-back equal, the other 11 notes unchanged | NOT RUN |
| H-066 | 8 | Files exported by File > Export (a Performance or Rhythm Set in User slot 1; the System settings) are accepted by the instrument or another librarian when sent as they are | send an exported Performance file with a SysEx librarian to a spare User Performance slot (after a backup) | the Performance appears with the same name and values | NOT RUN |

## Changelog of this ledger

- 2026-11-03: created. Seeded from Phase 1 unverified items, the legacy hardware observations (20 ms gap, 128-byte write
  limit, one RQ1 at a time, sparse replies, 129-byte device messages) and the Phase 2 design.
- 2026-11-03 (phase 2): added H-018 (echo handling) and H-019 (absent device, ambiguous ports) from the Phase 2 reviews.
  Also to observe in H-012: whether a retry sent while the instrument is still streaming is dropped (the client now waits
  one inter-reply timeout before retrying).
- 2026-11-03 (phase 4): added H-025 to H-029 (single-parameter live edits, nibble parameter, display meanings, fetch/verify/stale
  in the UI, fast-drag pacing). Wave names are not in the project yet, so the Wave Number shows only its number (recorded
  as a gap for the librarian phase).
- 2026-11-03 (phase 5): added H-044 to H-047 for the Performance and Rhythm Set editors (temporary-area live edits, the
  Rhythm Setup address, key-to-address mapping, whole-document sends).
- 2026-11-03 (phase 6): added H-034 to H-037 for the save ceremony's wire format, backup files and library export. The
  ceremony itself (H-030 to H-033) is built and tested against the virtual instrument only. Also added in phase 5 review:
  the Rhythm Setup name is held back from live editing until H-045 confirms its temporary address.
- 2026-11-04 (phase 6 review): added H-038 (echo cannot fake verification) and H-039 (stale-slot refusals).
- 2026-11-04 (phase 6/7): added H-048, H-049, H-050b for the bank workspace (read, backup file, subset write). Phase 7
  tools (init, randomize, mutate, morph, compare, copy/paste) are application logic on top of live edits and need no
  separate item beyond H-052 (Init defaults) and H-029 (pacing of whole-patch sends after a tool).
- 2026-11-04 (parity D7): added H-053 for `.mid` playback. The file conversion itself (xppatch-core::smf) is tested in software, including a byte-exact round trip of the golden bank.
- 2026-11-04 (parity A6/A7): added H-054. The mapping, learn flow, grouped undo and the controller listener are tested against the virtual XP-60; which controllers the XP-60 really sends, and on which channel, is the open question.
- 2026-11-04 (parity D6): added H-055. The send/restore sequence, the dwell and leave timing and the failure handling (STALE when the original cannot be put back) are tested in software; how long a send takes on the real instrument is the open question.
- 2026-11-04 (parity A3): added H-056. Cutting frames (`resplit_dt1`) is tested in software to write the same bytes to the same addresses; whether the instrument takes small chunks as well as large ones is the open question.
- 2026-11-04 (parity C5): added H-057. Reading and writing the file is tested against the virtual XP-60 and the golden bank (which holds 32 performances and 2 rhythm sets); that the instrument answers requests at those User addresses is the open question.
- 2026-11-04 (factory lists): added H-058. Wave names (INT-A, INT-B, boards 1 and 2) come from Roland's lists and agree with the real bank in software (patches named for their contents resolve to the waves their names imply); the group ID reading and the Preset-bank mapping still need the instrument.
- 2026-11-04 (controller lists): added H-059. The values stay raw numbers in the app. The reading "value = controller number, 96 PITCH BEND, 97 AFTERTOUCH, pedals add seven functions" fits the manual's ranges and an Integra/XV research note, but no source states it for the XP-60, so it is a hypothesis to test, not a fact to display.

### H-060 EFX tempo-synced values (note lengths such as "1/2 trip")
Patch Base shows some EFX times (e.g. Triple Tap Delay "Delay-C") as note lengths when the Patch clock source is synced. The Owner's Manual gives no table mapping the raw
value to note lengths, so the app shows the raw number. On the XP-60: set Delay-C through its range with the clock synced and record the note lengths shown on the LCD; then add the table.
- 2026-11-05 (System write, export of Performances, Rhythm Sets and System settings): added H-061 to H-066. The ceremony (read, back up and verify the file, arm, write, read back, undo) is tested against the virtual XP-60 and in the interface; that the XP-60 accepts these writes at the System addresses is the open question.

### H-067 EFX display values not transcribed or only inferred
The Effects plug-in converts values only where the manual states the range. Open: (a) Peaking 1/2 Q (raw 0-4): the manual's value list was cut off in the scan, so the raw number is shown; (b) Cutoff Frequency (raw 0-16) uses the 200-8000 Hz list by count of entries, not by an explicit table; (c) Rate (0.05-10.0 Hz) and Pre Delay / Delay Time (0-100 ms, 0-500 ms) are non-linear: only the end points are known. On the XP-60: step each through its range and record the LCD readout; then add the tables.
- 2026-11-05 (Effects plug-in): added H-067.

### H-068 Output Assign raw values not in the manual
Tone Output Assign raw 3 and EFX Output Assign raw 2 are named `<OUTPUT-2>` in the parameter table but the Owner's Manual only lists MIX, EFX, DIR (Tone) and MIX, DIR (EFX). The app does not offer them. On the XP-60: write raw 3 / raw 2 with a SysEx tool to a temporary patch and record what the instrument does (accepts, ignores, or sounds from another output). Also check whether any factory or user patch holds them.
### H-069 Routing semantics the diagram assumes
The Routing diagram follows the manual's text. Confirm on the instrument: (a) a Tone on EFX also feeds chorus and reverb by its own sends; (b) the EFX's own chorus and reverb sends act only when EFX Output Assign is MIX; (c) Structure Type 2 to 10 ignores the first Tone's output settings; (d) chorus Output REVERB sends all chorus sound to reverb and none to MIX; (e) the [EFX]/[CHORUS]/[REVERB] switches silence the stage regardless of its settings.
- 2026-11-05 (Effects routing review): added H-068 and H-069.
- 2026-11-05 (routing verification): `08-effects-routing-verification.md` lists each routing rule with its manual page and status; rules 10-12 are the ones H-069 must confirm. The manual (p.8) names only two outputs, MIX OUTPUT and DIRECT OUTPUT, which supports H-068.
- 2026-11-05 (Bank screen, patch buttons): added H-070 to H-072. The messages (CC0, CC32, Program Change) and the table are from the XP-60/80 Owner's Manual p.181 and are checked in software against the virtual XP-60 (bytes sent and refused values); whether the instrument follows them, on which channel and with which switches, needs the instrument. The buttons only send when "Select on the instrument" is on and the instrument is connected.
