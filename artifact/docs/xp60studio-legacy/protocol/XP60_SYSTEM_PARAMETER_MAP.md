# XP-60 System Parameter Map

This file is the machine-checkable transcription source for the XP-60 **System
Common** and **Scale Tune** parameter tables. Every row implemented in code must
have a row here, and every row here names its Roland source.

The last of the four Parameter Address Map regions to be transcribed; the others
are [`XP60_PATCH_PARAMETER_MAP.md`](XP60_PATCH_PARAMETER_MAP.md),
[`XP60_PERFORMANCE_PARAMETER_MAP.md`](XP60_PERFORMANCE_PARAMETER_MAP.md) and
[`XP60_RHYTHM_PARAMETER_MAP.md`](XP60_RHYTHM_PARAMETER_MAP.md). Status legend is
the one in [`ROLAND_XP60_PROTOCOL_FACTS.md`](ROLAND_XP60_PROTOCOL_FACTS.md).

## Source material and transcription policy

The source is the **Roland XP-60/XP-80 Owner's Manual, MIDI Implementation,
Parameter Address Map**, sections `1-1.System`, `1-1-1.System Common` and
`1-1-2.Scale Tune`, on **PDF page 224** of
`docs/XP60-References/XP-60_80_OM.pdf`.

OCR-assisted and hand-corrected, with the same conventions as the Performance
and Rhythm maps. The tables tile both blocks exactly, no gap and no overlap.

**This region is not in the golden fixture.** The fixture is a User bank dump;
it carries Patches, Performances and Rhythm Setups but no System data, so the
size-and-contents cross-check that validated the other three transcriptions is
not available here. That makes these tables the least corroborated of the four,
and `DEVICE_ACCEPTANCE.md` area 19 is what would settle them.

**No JV-1080, XP-30 or other JV/XP-family values were substituted.**

All rows are **Documentation-derived**. Nothing here is hardware-verified.

### Enumerations Roland prints but does not index

Several rows carry a printed list that is *not* one label per raw value — it
describes which controllers may be chosen, not an indexed table. Encoding those
as labels would silently mis-name every value, so those rows read `same as raw`
and Roland's list is reproduced verbatim below instead.

- **\*2** Performance Number (0..127) — `(USER:01 - USER:32, <CARD:01 -
  CARD:32>, PR-A:01 - PR-A:32, PR-B:01 - PR-B:32)`. Four banks of 32 does come
  to 128, but the printed form is four ranges rather than 128 labels.
- **\*4** Master Tune (0..126) — `(427.4 - 452.6)`, in Hz. A tuning frequency,
  not an enumeration.
- **\*5** Clock Source (0..1) — `(<INT,MIDI>)`.
- **\*6** TAP Control Source (0..4) — `(<OFF,HOLD-1,SOSTENUTO,SOFT,HOLD-2>)`.
  The whole list is bracketed, unlike \*7's identical list which is not, so the
  two are kept apart rather than merged.
- **\*10** System Control Source 1/2 and C1/C2 Assign (1..97) — `(CC01 - CC05,
  CC07 - CC31, CC64 - CC95, PITCH BEND, AFTERTOUCH)`. That is 64 selectable
  controllers across a 97-value field.
- **\*11** Preview Velocity Set (0..127) — `(<OFF,1 - 127>)`.
- **\*12** Patch Transmit Channel (0..17) — `(1 - 16, Rx-Ch, OFF)`.
- **\*13** Keyboard Velocity and Arpeggio Keyboard Velocity (0..127) —
  `(REAL,1 - 127)`.
- **\*15** Pedal1..4 Assign (1..104) — `(CC01 - CC05, CC07 - CC31, CC64 - CC95,
  PITCH BEND, AFTERTOUCH, PROG-UP, PROG-DOWN, START/STOP, PUNCH-I/O, TAP-TEMPO,
  OCT-UP, OCT-DOWN)`. 71 named entries across a 104-value field.

Named footnotes that *do* index one-to-one are encoded as labels in the table:
\*1 Sound Mode, \*3 Patch Group Type, \*7 Hold/Peak Control Source, \*8 Volume
Control Source, \*9 Aftertouch Source, \*14 Keyboard Sens, \*16 pedal Output
Mode, \*17 pedal Polarity.

### Values recovered by arithmetic rather than read

- **Arpeggio Beat Pattern** display OCRs as `(1 - 61)` against raw `0..40`.
  `1..41` is the only reading that fits, and it matches the neighbouring
  Arpeggio Style (`0..32` → `1..33`) and Motif (`0..33` → `1..34`).
- **Preview Sound Mode** OCRs its range as `0 - 2` while printing two labels.
  Roland's own bit pattern `0000 000a` is one bit, so `0..1` is right.

## System block layout — `1-1`

| Offset inside System | Block | Status | Source |
|---|---|---|---|
| `00 00` | System Common (§1-1-1) | Documentation-derived | Parameter Address Map §1-1 |
| `10 00` | Part 1 Scale Tune (§1-1-2) | Documentation-derived | Parameter Address Map §1-1 |
| `11 00` | Part 2 Scale Tune | Documentation-derived | Parameter Address Map §1-1 |
| … | Parts 3–15 continue at stride `01 00` | Documentation-derived | Parameter Address Map §1-1 |
| `1F 00` | Part 16 Scale Tune | Documentation-derived | Parameter Address Map §1-1 |
| `20 00` | Patch Mode Scale Tune (§1-1-2) | Documentation-derived | Parameter Address Map §1-1 |

**Seventeen Scale Tune blocks, not one.** Each Performance Part carries its own,
and Patch mode has a seventeenth of its own — which follows from the instrument
having sixteen Parts that can be tuned independently plus a single-Patch mode
that is not one of them. All seventeen use the same §1-1-2 table below.

The System region's own base address (`00 00 00 00`) is in
`ROLAND_XP60_PROTOCOL_FACTS.md` §3.

## 1. System Common — `1-1-1`

Total size `00 00 00 60` (96 bytes, offsets `00 00`…`00 5F`).

| Offset | Parameter | Bytes / encoding | Raw range | Display | Status | Source |
|---|---|---|---|---|---|---|
| `00 00` | Sound Mode | 1 / 7-bit (`0000 00aa`) | 0..2 | PERFORMANCE, PATCH, GM | Documentation-derived | Parameter Address Map §1-1-1 note *1 |
| `00 01` | Performance Number | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *2 |
| `00 02` | Patch Group Type | 1 / 7-bit (`0000 00aa`) | 0..2 | USER&PRESET, `<PCM>`, EXP | Documentation-derived | Parameter Address Map §1-1-1 note *3 |
| `00 03` | Patch Group ID | 1 / 7-bit (`0aaa aaaa`) | 1..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 04` | Patch Number | 2 / nibble (`0000 aaaa`, `0000 bbbb`) | 0..254 | 001..255 | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 06` | Master Tune | 1 / 7-bit (`0aaa aaaa`) | 0..126 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *4 |
| `00 07` | Scale Tune Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 08` | EFX Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 09` | Chorus Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 0A` | Reverb Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 0B` | Patch Remain | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 0C` | Clock Source | 1 / 7-bit (`0000 000a`) | 0..1 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *5 |
| `00 0D` | TAP Control Source | 1 / 7-bit (`0000 0aaa`) | 0..4 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *6 |
| `00 0E` | Hold Control Source | 1 / 7-bit (`0000 0aaa`) | 0..4 | OFF, HOLD-1, SOSTENUTO, SOFT, HOLD-2 | Documentation-derived | Parameter Address Map §1-1-1 note *7 |
| `00 0F` | Peak Control Source | 1 / 7-bit (`0000 0aaa`) | 0..4 | OFF, HOLD-1, SOSTENUTO, SOFT, HOLD-2 | Documentation-derived | Parameter Address Map §1-1-1 note *7 |
| `00 10` | Volume Control Source | 1 / 7-bit (`0000 000a`) | 0..1 | VOLUME, VOL&EXP | Documentation-derived | Parameter Address Map §1-1-1 note *8 |
| `00 11` | Aftertouch Source | 1 / 7-bit (`0000 00aa`) | 0..2 | CHANNEL, POLY, CH&POLY | Documentation-derived | Parameter Address Map §1-1-1 note *9 |
| `00 12` | System Control Source 1 | 1 / 7-bit (`0aaa aaaa`) | 1..97 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *10 |
| `00 13` | System Control Source 2 | 1 / 7-bit (`0aaa aaaa`) | 1..97 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *10 |
| `00 14` | Receive Program Change | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 15` | Receive Bank Select | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 16` | Receive Control Change | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 17` | Receive Modulation | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 18` | Receive Volume | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 19` | Receive Hold-1 | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 1A` | Receive Bender | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 1B` | Receive Aftertouch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 1C` | Control Channel | 1 / 7-bit (`000a aaaa`) | 0..16 | 1..16, OFF | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 1D` | Patch Receive Channel | 1 / 7-bit (`0000 aaaa`) | 0..15 | 1..16 | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 1E` | Rhythm Edit Source | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 1F` | Preview Sound Mode | 1 / 7-bit (`0000 000a`) | 0..1 | SINGLE, CHORD | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 20` | Preview Note Set 1 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | C-1..G9 | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 21` | Preview Velocity Set 1 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *11 |
| `00 22` | Preview Note Set 2 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | C-1..G9 | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 23` | Preview Velocity Set 2 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *11 |
| `00 24` | Preview Note Set 3 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | C-1..G9 | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 25` | Preview Velocity Set 3 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *11 |
| `00 26` | Preview Note Set 4 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | C-1..G9 | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 27` | Preview Velocity Set 4 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *11 |
| `00 28` | Transmit Program Change | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 29` | Transmit Bank Select | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 2A` | Patch Transmit Channel | 1 / 7-bit (`000a aaaa`) | 0..17 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *12 |
| `00 2B` | Transpose Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 2C` | Transpose Value | 1 / 7-bit (`0000 aaaa`) | 0..11 | -5..+6 | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 2D` | Octave Shift | 1 / 7-bit (`0000 0aaa`) | 0..6 | -3..+3 | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 2E` | Keyboard Velocity | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *13 |
| `00 2F` | Keyboard Sens | 1 / 7-bit (`0000 00aa`) | 0..2 | LIGHT, STANDARD, HEAVY | Documentation-derived | Parameter Address Map §1-1-1 note *14 |
| `00 30` | Aftertouch Sens | 1 / 7-bit (`0aaa aaaa`) | 0..100 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 31` | Pedal1 Assign | 1 / 7-bit (`0aaa aaaa`) | 1..104 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *15 |
| `00 32` | Pedal1 Output Mode | 1 / 7-bit (`0000 00aa`) | 0..3 | OFF, INT, MIDI, INT&MIDI | Documentation-derived | Parameter Address Map §1-1-1 note *16 |
| `00 33` | Pedal1 Polarity | 1 / 7-bit (`0000 000a`) | 0..1 | STANDARD, REVERSE | Documentation-derived | Parameter Address Map §1-1-1 note *17 |
| `00 34` | Pedal2 Assign | 1 / 7-bit (`0aaa aaaa`) | 1..104 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *15 |
| `00 35` | Pedal2 Output Mode | 1 / 7-bit (`0000 00aa`) | 0..3 | OFF, INT, MIDI, INT&MIDI | Documentation-derived | Parameter Address Map §1-1-1 note *16 |
| `00 36` | Pedal2 Polarity | 1 / 7-bit (`0000 000a`) | 0..1 | STANDARD, REVERSE | Documentation-derived | Parameter Address Map §1-1-1 note *17 |
| `00 37` | C1 Assign | 1 / 7-bit (`0aaa aaaa`) | 1..97 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *10 |
| `00 38` | C1 Output Mode | 1 / 7-bit (`0000 00aa`) | 0..3 | OFF, INT, MIDI, INT&MIDI | Documentation-derived | Parameter Address Map §1-1-1 note *16 |
| `00 39` | C2 Assign | 1 / 7-bit (`0aaa aaaa`) | 1..97 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *10 |
| `00 3A` | C2 Output Mode | 1 / 7-bit (`0000 00aa`) | 0..3 | OFF, INT, MIDI, INT&MIDI | Documentation-derived | Parameter Address Map §1-1-1 note *16 |
| `00 3B` | Hold Pedal Output Mode | 1 / 7-bit (`0000 00aa`) | 0..3 | OFF, INT, MIDI, INT&MIDI | Documentation-derived | Parameter Address Map §1-1-1 note *16 |
| `00 3C` | Hold Pedal Polarity | 1 / 7-bit (`0000 000a`) | 0..1 | STANDARD, REVERSE | Documentation-derived | Parameter Address Map §1-1-1 note *17 |
| `00 3D` | Bank Select Group1 Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 3E` | Bank Select Group1 MSB | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 3F` | Bank Select Group1 LSB | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 40` | Bank Select Group2 Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 41` | Bank Select Group2 MSB | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 42` | Bank Select Group2 LSB | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 43` | Bank Select Group3 Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 44` | Bank Select Group3 MSB | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 45` | Bank Select Group3 LSB | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 46` | Bank Select Group4 Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 47` | Bank Select Group4 MSB | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 48` | Bank Select Group4 LSB | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 49` | Bank Select Group5 Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 4A` | Bank Select Group5 MSB | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 4B` | Bank Select Group5 LSB | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 4C` | Bank Select Group6 Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 4D` | Bank Select Group6 MSB | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 4E` | Bank Select Group6 LSB | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 4F` | Bank Select Group7 Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 50` | Bank Select Group7 MSB | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 51` | Bank Select Group7 LSB | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 52` | Pedal3 Assign | 1 / 7-bit (`0aaa aaaa`) | 1..104 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *15 |
| `00 53` | Pedal3 Output Mode | 1 / 7-bit (`0000 00aa`) | 0..3 | OFF, INT, MIDI, INT&MIDI | Documentation-derived | Parameter Address Map §1-1-1 note *16 |
| `00 54` | Pedal3 Polarity | 1 / 7-bit (`0000 000a`) | 0..1 | STANDARD, REVERSE | Documentation-derived | Parameter Address Map §1-1-1 note *17 |
| `00 55` | Pedal4 Assign | 1 / 7-bit (`0aaa aaaa`) | 1..104 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *15 |
| `00 56` | Pedal4 Output Mode | 1 / 7-bit (`0000 00aa`) | 0..3 | OFF, INT, MIDI, INT&MIDI | Documentation-derived | Parameter Address Map §1-1-1 note *16 |
| `00 57` | Pedal4 Polarity | 1 / 7-bit (`0000 000a`) | 0..1 | STANDARD, REVERSE | Documentation-derived | Parameter Address Map §1-1-1 note *17 |
| `00 58` | Arpeggio Style | 1 / 7-bit (`00aa aaaa`) | 0..32 | 1..33 | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 59` | Arpeggio Motif | 1 / 7-bit (`00aa aaaa`) | 0..33 | 1..34 | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 5A` | Arpeggio Beat Pattern | 1 / 7-bit (`00aa aaaa`) | 0..40 | 1..41 | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 5B` | Arpeggio Accent Rate | 1 / 7-bit (`0aaa aaaa`) | 0..100 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 5C` | Arpeggio Shuffle Rate | 1 / 7-bit (`0aaa aaaa`) | 50..90 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 5D` | Arpeggio Keyboard Velocity | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-1-1 note *13 |
| `00 5E` | Arpeggio Octave Range | 1 / 7-bit (`0000 0aaa`) | 0..6 | -3..+3 | Documentation-derived | Parameter Address Map §1-1-1 |
| `00 5F` | Arpeggio Part Number | 1 / 7-bit (`0000 aaaa`) | 0..15 | PART1, PART2, PART3, PART4, PART5, PART6, PART7, PART8, PART9, PART10, PART11, PART12, PART13, PART14, PART15, PART16 | Documentation-derived | Parameter Address Map §1-1-1 |

## 2. Scale Tune — `1-1-2`

Total size `00 00 00 0C` (12 bytes, offsets `00 00`…`00 0B`).

One offset per pitch class, applied to every octave, and the same table for all
seventeen blocks above. `Scale Tune Switch` in System Common (`00 07`) is what
turns it on.

| Offset | Parameter | Bytes / encoding | Raw range | Display | Status | Source |
|---|---|---|---|---|---|---|
| `00 00` | Scale Tune for C | 1 / 7-bit (`0aaa aaaa`) | 0..127 | -64..+63 | Documentation-derived | Parameter Address Map §1-1-2 |
| `00 01` | Scale Tune for C# | 1 / 7-bit (`0aaa aaaa`) | 0..127 | -64..+63 | Documentation-derived | Parameter Address Map §1-1-2 |
| `00 02` | Scale Tune for D | 1 / 7-bit (`0aaa aaaa`) | 0..127 | -64..+63 | Documentation-derived | Parameter Address Map §1-1-2 |
| `00 03` | Scale Tune for D# | 1 / 7-bit (`0aaa aaaa`) | 0..127 | -64..+63 | Documentation-derived | Parameter Address Map §1-1-2 |
| `00 04` | Scale Tune for E | 1 / 7-bit (`0aaa aaaa`) | 0..127 | -64..+63 | Documentation-derived | Parameter Address Map §1-1-2 |
| `00 05` | Scale Tune for F | 1 / 7-bit (`0aaa aaaa`) | 0..127 | -64..+63 | Documentation-derived | Parameter Address Map §1-1-2 |
| `00 06` | Scale Tune for F# | 1 / 7-bit (`0aaa aaaa`) | 0..127 | -64..+63 | Documentation-derived | Parameter Address Map §1-1-2 |
| `00 07` | Scale Tune for G | 1 / 7-bit (`0aaa aaaa`) | 0..127 | -64..+63 | Documentation-derived | Parameter Address Map §1-1-2 |
| `00 08` | Scale Tune for G# | 1 / 7-bit (`0aaa aaaa`) | 0..127 | -64..+63 | Documentation-derived | Parameter Address Map §1-1-2 |
| `00 09` | Scale Tune for A | 1 / 7-bit (`0aaa aaaa`) | 0..127 | -64..+63 | Documentation-derived | Parameter Address Map §1-1-2 |
| `00 0A` | Scale Tune for A# | 1 / 7-bit (`0aaa aaaa`) | 0..127 | -64..+63 | Documentation-derived | Parameter Address Map §1-1-2 |
| `00 0B` | Scale Tune for B | 1 / 7-bit (`0aaa aaaa`) | 0..127 | -64..+63 | Documentation-derived | Parameter Address Map §1-1-2 |
