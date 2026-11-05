# XP-60 Rhythm Setup Parameter Map

This file is the machine-checkable transcription source for the XP-60 **Rhythm
Common** and **Rhythm Note** parameter tables used by Phase 8. Every row
implemented in code must have a row here, and every row here names its Roland
source.

Companion to [`XP60_PATCH_PARAMETER_MAP.md`](XP60_PATCH_PARAMETER_MAP.md) and
[`XP60_PERFORMANCE_PARAMETER_MAP.md`](XP60_PERFORMANCE_PARAMETER_MAP.md).
Status legend is the one in
[`ROLAND_XP60_PROTOCOL_FACTS.md`](ROLAND_XP60_PROTOCOL_FACTS.md).

## Source material and transcription policy

The source is the **Roland XP-60/XP-80 Owner's Manual, MIDI Implementation,
Parameter Address Map**, sections `1-4.Rhythm Setup`, `1-4-1.Rhythm Common` and
`1-4-2.Rhythm Note`. In the copy held at
`docs/XP60-References/XP-60_80_OM.pdf` these fall on **PDF page 227**.

The appendix is a scanned image, so this transcription is OCR-assisted and
hand-corrected. Conventions and cross-checks are the same as the Performance
map's, and both cross-checks passed:

- **Total sizes.** Roland gives Rhythm Common as `00 00 00 0C` (12 bytes) and
  Rhythm Note as `00 00 00 3A` (58 bytes). The golden fixture
  `tests/fixtures/xp60/user-bank-amal.syx` contains exactly 2 blocks of 12 bytes
  and 128 blocks of 58 — two User Rhythm Setups, each with a Common and 64
  Notes. The tables below tile both blocks exactly, no gap and no overlap.
- **Block layout.** `1-4.Rhythm Setup` places Common at `00 00` and the Notes at
  `23 00`…`62 00`, one per key. The fixture's addresses match, and `23`..`62` is
  MIDI notes 35..98 — the instrument's rhythm key range.

**No JV-1080, XP-30 or other JV/XP-family values were substituted.**

All rows are **Documentation-derived**. Nothing here is hardware-verified.

### Machine-readable conventions

Identical to the Performance map: seven columns per row, `Display` either
`same as raw`, a linearly scaling integer range, Roland's pan or note notation,
or exactly one label per raw value with enumerations spelled out.

### Two values recovered by arithmetic rather than read

The scan is poor in two places, and both were settled by counting rather than by
guessing at the glyphs. Recorded because they are reconstructions:

- **Wave Number** display OCRs as `002 - 255`. That would be 254 labels for 255
  raw values. `001..255` is the only reading that fits raw `0..254`, and it is
  what the identically-encoded Patch Tone Wave Number carries.
- **Mute Group** display OCRs as `(OFF,2 - 31)`. Raw `0..32` is 33 values, and
  `OFF` plus `1..32` is exactly 33.

## Rhythm Setup block layout — `1-4`

| Offset inside Rhythm Setup | Block | Status | Source |
|---|---|---|---|
| `00 00` | Rhythm Common | Documentation-derived | Parameter Address Map §1-4 |
| `23 00` | Rhythm Note for Key# 35 | Documentation-derived | Parameter Address Map §1-4 |
| … | one Note per key, stride `01 00` | Documentation-derived | Parameter Address Map §1-4 |
| `62 00` | Rhythm Note for Key# 98 | Documentation-derived | Parameter Address Map §1-4 |

Sixty-four Notes, keyed by MIDI note number rather than by index: the offset
*is* the key, so Key# 35 lives at `23 00` because 0x23 is 35.

Base addresses for the Rhythm regions themselves (Temporary Rhythm Setup at
`02 09 00 00` — Performance-mode Part 10 — and the two User Rhythm Setups at
`10 40 00 00` and `10 41 00 00`) are in `ROLAND_XP60_PROTOCOL_FACTS.md` §3.

## 1. Rhythm Common — `1-4-1`

Total size `00 00 00 0C` (12 bytes, offsets `00 00`…`00 0B`).

A Rhythm Setup's Common block is its name and nothing else. Everything that
shapes a sound lives in the per-key Notes.

| Offset | Parameter | Bytes / encoding | Raw range | Display | Status | Source |
|---|---|---|---|---|---|---|
| `00 00` | Rhythm Name 1 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-1 |
| `00 01` | Rhythm Name 2 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-1 |
| `00 02` | Rhythm Name 3 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-1 |
| `00 03` | Rhythm Name 4 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-1 |
| `00 04` | Rhythm Name 5 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-1 |
| `00 05` | Rhythm Name 6 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-1 |
| `00 06` | Rhythm Name 7 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-1 |
| `00 07` | Rhythm Name 8 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-1 |
| `00 08` | Rhythm Name 9 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-1 |
| `00 09` | Rhythm Name 10 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-1 |
| `00 0A` | Rhythm Name 11 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-1 |
| `00 0B` | Rhythm Name 12 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-1 |

## 2. Rhythm Note — `1-4-2`

Total size `00 00 00 3A` (58 bytes, offsets `00 00`…`00 39`).

One drum sound, on one key. The table is close to a Patch Tone — wave
reference, pitch/filter/level envelopes, pan and sends — but not identical, and
the differences are the point:

- **Source Key** (`00 0C`) sets the pitch the wave plays at, because a Note is
  bound to its own key rather than tracking the keyboard.
- **Mute Group** (`00 07`) is what makes a closed hi-hat cut off an open one.
- **Envelope Mode** (`00 08`) chooses whether the key-off is honoured at all.
- There is no LFO, no structure, no booster, and no keyboard or velocity range:
  a drum key has nothing to cross-fade with.

| Offset | Parameter | Bytes / encoding | Raw range | Display | Status | Source |
|---|---|---|---|---|---|---|
| `00 00` | Tone Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 01` | Wave Group Type | 1 / 7-bit (`0000 00aa`) | 0..2 | INT, `<PCM>`, EXP | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 02` | Wave Group ID | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 03` | Wave Number | 2 / nibble (`0000 aaaa`, `0000 bbbb`) | 0..254 | 001..255 | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 05` | Wave Gain | 1 / 7-bit (`0000 00aa`) | 0..3 | -6, 0, +6, +12 | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 06` | Bend Range | 1 / 7-bit (`0000 aaaa`) | 0..12 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 07` | Mute Group | 1 / 7-bit (`000a aaaa`) | 0..32 | OFF, 1..32 | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 08` | Envelope Mode | 1 / 7-bit (`0000 000a`) | 0..1 | NO-SUS, SUSTAIN | Documentation-derived | Parameter Address Map §1-4-2 note *1 |
| `00 09` | Volume Control Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 0A` | Hold-1 Control Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 0B` | Pan Control Switch | 1 / 7-bit (`0000 00aa`) | 0..2 | OFF, CONTINUOUS, KEY-ON | Documentation-derived | Parameter Address Map §1-4-2 note *2 |
| `00 0C` | Source Key | 1 / 7-bit (`0aaa aaaa`) | 0..127 | C-1..G9 | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 0D` | Fine Tune | 1 / 7-bit (`0aaa aaaa`) | 0..100 | -50..+50 | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 0E` | Random Pitch Depth | 1 / 7-bit (`000a aaaa`) | 0..30 | 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200 | Documentation-derived | Parameter Address Map §1-4-2 note *3 |
| `00 0F` | Pitch Envelope Depth | 1 / 7-bit (`000a aaaa`) | 0..24 | -12..+12 | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 10` | Pitch Envelope Velocity Sens | 1 / 7-bit (`0aaa aaaa`) | 0..125 | -100..+150 | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 11` | Pitch Envelope Velocity Time | 1 / 7-bit (`0000 aaaa`) | 0..14 | -100, -70, -50, -40, -30, -20, -10, 0, +10, +20, +30, +40, +50, +70, +100 | Documentation-derived | Parameter Address Map §1-4-2 note *4 |
| `00 12` | Pitch Envelope Time 1 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 13` | Pitch Envelope Time 2 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 14` | Pitch Envelope Time 3 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 15` | Pitch Envelope Time 4 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 16` | Pitch Envelope Level 1 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | Parameter Address Map §1-4-2 note *5 |
| `00 17` | Pitch Envelope Level 2 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | Parameter Address Map §1-4-2 note *5 |
| `00 18` | Pitch Envelope Level 3 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | Parameter Address Map §1-4-2 note *5 |
| `00 19` | Pitch Envelope Level 4 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | Parameter Address Map §1-4-2 note *5 |
| `00 1A` | Filter Type | 1 / 7-bit (`0000 0aaa`) | 0..4 | OFF, LPF, BPF, HPF, PKG | Documentation-derived | Parameter Address Map §1-4-2 note *6 |
| `00 1B` | Cutoff Frequency | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 1C` | Resonance | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 1D` | Resonance Velocity Sens | 1 / 7-bit (`0aaa aaaa`) | 0..125 | -100..+150 | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 1E` | Filter Envelope Depth | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | Parameter Address Map §1-4-2 note *5 |
| `00 1F` | Filter Envelope Velocity Sens | 1 / 7-bit (`0aaa aaaa`) | 0..125 | -100..+150 | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 20` | Filter Envelope Velocity Time | 1 / 7-bit (`0000 aaaa`) | 0..14 | -100, -70, -50, -40, -30, -20, -10, 0, +10, +20, +30, +40, +50, +70, +100 | Documentation-derived | Parameter Address Map §1-4-2 note *4 |
| `00 21` | Filter Envelope Time 1 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 22` | Filter Envelope Time 2 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 23` | Filter Envelope Time 3 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 24` | Filter Envelope Time 4 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 25` | Filter Envelope Level 1 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 26` | Filter Envelope Level 2 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 27` | Filter Envelope Level 3 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 28` | Filter Envelope Level 4 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 29` | Tone Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 2A` | Level Envelope Velocity Sens | 1 / 7-bit (`0aaa aaaa`) | 0..125 | -100..+150 | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 2B` | Level Envelope Velocity Time | 1 / 7-bit (`0000 aaaa`) | 0..14 | -100, -70, -50, -40, -30, -20, -10, 0, +10, +20, +30, +40, +50, +70, +100 | Documentation-derived | Parameter Address Map §1-4-2 note *4 |
| `00 2C` | Level Envelope Time 1 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 2D` | Level Envelope Time 2 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 2E` | Level Envelope Time 3 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 2F` | Level Envelope Time 4 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 30` | Level Envelope Level 1 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 31` | Level Envelope Level 2 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 32` | Level Envelope Level 3 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 33` | Tone Pan | 1 / 7-bit (`0aaa aaaa`) | 0..127 | L64..63R | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 34` | Random Pan Depth | 1 / 7-bit (`00aa aaaa`) | 0..63 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 35` | Alternate Pan Depth | 1 / 7-bit (`0aaa aaaa`) | 1..127 | L63..63R | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 36` | Output Assign | 1 / 7-bit (`0000 00aa`) | 0..3 | MIX, EFX, DIR, `<OUTPUT-2>` | Documentation-derived | Parameter Address Map §1-4-2 note *7 |
| `00 37` | Mix/EFX Send Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 38` | Chorus Send Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
| `00 39` | Reverb Send Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-4-2 |
