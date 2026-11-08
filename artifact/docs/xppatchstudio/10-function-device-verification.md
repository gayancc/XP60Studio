# Function device verification — every intended operation on the XP-60

Goal: every intended XPPatchStudio operation is cross-checked and verified on the MIDI-connected physical XP-60, or has an explicit owner/product blocker. Soft Thru / LCD / audible claims stay owner. SysEx features are not marked DONE in `04-parity-matrix.md` without owner confirmation.

Current baseline: FCB1010 IN/OUT, temp **Jimmee Dee** from the original session backup, Patch mode; owner-confirmed Sequencer Through and both Protect options OFF. Earlier evidence used Nice Piano where named.

Status legend: **DEVICE** = instrument read-back or native-app MIDI evidence on this XP-60; **DESKTOP** = UI-only (no instrument required); **OWNER** = needs owner LCD/audible/panel (explicit blocker); **BLOCKED** = cannot run until owner names a prerequisite.

| ID | Intended operation | Status | Evidence / blocker |
|---|---|---|---|
| A1 | MIDI port select / connect | DEVICE | fn-verify-hw-connect.txt; fn-verify-hw-select.txt |
| A2 | Device ID 17 / model 6A | DEVICE | H-002/H-003 |
| A3 | Throughput chunk / gap / byte-rate | DEVICE | H-050/H-056/H-023; fn-verify-hw-chunk.txt (128/32); fn-verify-hw-edit.txt gap |
| A4 | NRPN/RPN decode + link | BLOCKED | CC learn DEVICE (H-054). NRPN/RPN unit-tested; no NRPN controller on this FCB1010 path. |
| A5 | Audition notes to synth | DEVICE PASS | H-051c computer-key and mouse owner-heard; mouse MIDI trace and release checks (`H-051c-mouse-listen.txt`) |
| A6 | Incoming CC drives parameters | DEVICE | H-054 |
| A7 | MIDI map / learn | DEVICE | H-054 |
| A8 | Fetch-complete notice | DEVICE | fetch notices on connect/edit/tools runs |
| A9 | On-screen keyboard | DEVICE PASS | H-051c native mouse clicks sounded and stopped between clicks by owner report |
| A10 | Secondary controller MIDI pass-through | DEVICE PARTIAL | H-076 native KL Essential 49 mk3 → XP-60: Direct notes, pitch bend and modulation CC audible; Ch 2 remap silent; 128-patch read held/silenced/resumed thru; no hangs and final snapshot equal. A physical real-time clock/Active Sense flood remains. |
| B1 | Patch editor live edit | DEVICE PASS | H-022: owner heard Patch Level 120→0 change; one-byte temporary read-back and Undo/restore passed |
| B2 | System editor write | DEVICE | H-061..H-065; fn-verify-hw-system.txt |
| B3 | Fetch from synth | DEVICE | H-010/H-048; connect/fetch runs |
| B4 | Send temp | DEVICE | H-020/H-021; hw-edit/hw-chunk |
| B5 | Save to User slot | DEVICE | H-030..H-039, H-031 Protect |
| B6 | Init (neutral sound) | DEVICE | fn-verify-hw-tools.txt; retains the existing neutral-sound behavior |
| B6b | XP-60 Init Patch (panel DEFAULT) | DEVICE PASS | H-052: two identical panel captures; native export, Send/read-back, Undo/restore all matched |
| B7 | Randomize | DEVICE | fn-verify-hw-tools.txt |
| B8 | Morph | DEVICE | fn-verify-hw-tools.txt |
| B9 | Mutate | DEVICE | fn-verify-hw-tools.txt |
| B10 | Undo/redo history | DEVICE | fn-verify-hw-tools.txt |
| B11 | Section navigation | DESKTOP | no instrument bytes |
| B12 | Overview panel | DESKTOP | no instrument bytes |
| B13 | Copy as text / clipboard | DESKTOP | no instrument bytes |
| B14 | Copy/paste tones/sections | DEVICE | fn-verify-hw-tools.txt |
| B15 | Compare A/B | DEVICE | fn-verify-hw-tools.txt |
| B16 | Arpeggiator parameters | DEVICE | B16-arpeggio.txt; audible OWNER |
| C1–C3 | Bank editor / fetch / write | DEVICE | H-048/H-032/H-050b |
| C4 | Load bank from file | DESKTOP | file preview; send covered by bank write |
| C5 | Whole-synth backup | DEVICE | H-057 |
| C6 | Group refs as bank | DEVICE | bank ops |
| C7 | Performance multi-part | DEVICE | fn-verify-hw-documents.txt |
| D1–D5 | Library browse/folders/search/settings | DESKTOP | library workflows |
| D3 | Import `.syx` then send | DEVICE | H-037 |
| D6 | Hover preview | DEVICE | fn-verify-hw-hover.txt |
| D7 | `.mid` import/export | DEVICE | H-053 |
| D8–D10 | Tags / DnD / backups | DESKTOP (+ DEVICE H-030 backups) | |
| E1–E5 | Module/toolbar/find/theme/license | DESKTOP / product | WON'T-DO where listed |
| F1–F3 | Dock / hint / shortcuts | DESKTOP | |
| F4 | Right-click + CC link | DEVICE | H-054 |
| F5 | Send queue / live send | DEVICE | fn-verify-hw-edit.txt |
| F6 | Performance rack | DEVICE | fn-verify-hw-documents.txt |
| F7 | Rhythm per-key | DEVICE | fn-verify-hw-documents.txt |
| G1 | Structure type chooser | DEVICE | G1-structure-sysex.txt; audible wiring OWNER H-075 |
| G2 | EFX type + named params | OWNER | H-073/H-074 panel; SysEx type-only does not load defaults |
| G3 | Tone page layout | DESKTOP | |
| G4 | Prev/next / bank select | DEVICE | fn-verify-hw-select.txt USER; H-070 MIDI-tool presets |
| G5 | Export open doc | DEVICE | H-066/H-053/H-061 |
| G6 | Open Perf/Rhythm from `.syx` | DEVICE | H-057 |
| G7 | Installer | DESKTOP/product | no MIDI |
| G8 | Controller destination names | DESKTOP | source number LCD labels OWNER H-059 |
| G9 | Scale tune page | DEVICE write | H-042/H-065; LCD OWNER |

## Explicit owner / path blockers (cannot fake without owner)

These remain open by design. Agent SysEx/desktop halves are recorded; product SysEx rows stay not-DONE until owner confirms.

1. **Sequencer Through (H-018 / H-038)** — PARTIAL: the XP-60/80 manual p.117 identifies this setting. With owner-confirmed Through ON, the WIDI path echoed a 12-byte DT1 exactly. Subsequent read-back timed out and the Windows MM Bluetooth MIDI input stopped opening after a WIDI reset. Restore the port, verify the original temporary patch, then decide whether the protected-save echo case can run safely (H-018-through-on.txt).
2. **H-052 panel Init** — PASS: two panel DEFAULT captures matched; the separate XP-60 Init Patch action matched the device and restored the original temporary patch after Undo.
3. **H-059 controller labels** — OWNER: LCD names for Control Source / Pedal Assign raw 7/96/97/98–104.
4. **H-073 / H-074 EFX** — OWNER: panel type-default walk and unusual slot-order probe.
5. **H-075 Structure listen** — OWNER: audible wiring for types 2–10.
6. **H-060 / H-067 LCD tables** — OWNER: tempo-synced note lengths; Peaking Q / Cutoff Hz / Rate / Delay displays.
7. **H-068 `<OUTPUT-2>` routing** — OWNER: which physical output (if any); SysEx ACCEPT recorded (H-068.txt).
8. **H-069 routing diagram** — OWNER: audible routing listen.
9. **Audible confirmation** — B1/H-022 and H-051c mouse are owner-confirmed. H-044/H-045 Performance/Rhythm listening remains owner.
10. **Panel LCD glances** — H-058 INT-A Tone 1 name matches the app; INT-B/expansion LCD names remain unverified because MIDI returns numeric IDs only. H-027 displays, H-042 Scale Tune, H-046 key names, H-062 Master Tune, H-070/H-072 selection names also remain. The owner prefers automated requests over patch-by-patch panel searching.
11. **A4 NRPN/RPN on-device** — BLOCKED: no NRPN source on this MIDI path.

## Completion audit (2026-11-07)

| Requirement | Result |
|---|---|
| Every MIDI-touching intended op cross-checked | Yes — matrix above; no GAP rows remain. |
| Device-verified where agent-runnable | Yes — native hw-* + SysEx probes with evidence files under `hardware-evidence/`. |
| Explicit blocker where owner/path required | Yes — section above. |
| Defects found this pass | Synth strip contrast redesigned (locked palette); no SysEx protocol inventing. |
| SysEx not marked DONE without owner | Yes — `04-parity-matrix.md` unchanged for DONE promotion. |
| Instrument cleanup | Owner-approved full session snapshot restored, then independently verified again after H-018/H-038 recovery and H-076. Final temporary read after the last voice test matched Jimmee Dee. |

**Verdict:** the operation inventory is reconciled, with explicit owner and path procedures. Hardware cleanup is verified; H-018/H-038 Through limitations and H-076 real-time flood remain open individually.

## Session log

- 2026-11-07 start: device connected; temp Nice Piano; matrix opened.
- B16 arpeggio SysEx PASS; G1 structure SysEx PARTIAL; hw-connect ALL PASSED; hw-tools PASSED.
- Synth strip high-contrast redesign; hw-edit / hover / documents PASSED.
- hw-select USER select PASS; hw-system Master Tune write/undo PASS; hw-chunk 128/32 PASS; hw-audition-mouse PASS.
- Owner/path blockers listed; no GAP left; goal audit closed.
