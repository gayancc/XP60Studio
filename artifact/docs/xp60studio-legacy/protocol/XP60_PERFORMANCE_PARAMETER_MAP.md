# XP-60 Performance Parameter Map

This file is the machine-checkable transcription source for the XP-60
**Performance Common** and **Performance Part** parameter tables used by Phase 8.
Every row implemented in code must have a row here, and every row here names its
Roland source.

Companion to [`XP60_PATCH_PARAMETER_MAP.md`](XP60_PATCH_PARAMETER_MAP.md), which
covers `1-3.Patch`. Status legend is the one in
[`ROLAND_XP60_PROTOCOL_FACTS.md`](ROLAND_XP60_PROTOCOL_FACTS.md).

## Source material and transcription policy

The source is the **Roland XP-60/XP-80 Owner's Manual, MIDI Implementation,
Parameter Address Map**, sections `1-2.Performance`, `1-2-1.Performance Common`
and `1-2-2.Performance Part`. In the copy held at
`docs/XP60-References/XP-60_80_OM.pdf` these fall on **PDF pages 224–225**.

Section numbers rather than page numbers are the citation of record below: the
Owner's Manual's printed page numbering runs two behind this PDF's (the Patch
tables the companion document cites as printed pp.223–225 are PDF pp.225–227),
and the section numbers are stable across both.

The appendix is a **scanned image**, so this transcription is OCR-assisted and
hand-corrected rather than copied from text. Every row was read back against the
page. Where the scan is genuinely unreadable this document says so instead of
supplying a plausible value.

**Two independent cross-checks** support the transcription, and both passed:

- **Total sizes.** Roland gives Performance Common as `00 00 00 42` (66 bytes)
  and Performance Part as `00 00 00 19` (25 bytes). The golden fixture
  `tests/fixtures/xp60/user-bank-amal.syx` contains exactly 32 blocks of 66
  bytes and 512 blocks of 25 bytes — 32 User Performances, each with Common and
  16 Parts. The tables account for every byte of both.
- **Block layout.** `1-2.Performance` places Common at `00 00` and Parts 1–16 at
  `10 00`…`1F 00`. The fixture's addresses match. `ROLAND_XP60_PROTOCOL_FACTS.md`
  previously recorded this layout as *observed in the fixture only*; it is now
  documentation-derived, and the two agree.

**No JV-1080, XP-30 or other JV/XP-family values were substituted.** Values in
angle brackets such as `<PCM>` and `<OUTPUT-2>` are preserved exactly as Roland
prints them rather than interpreted here.

All rows are **Documentation-derived**. Nothing here is hardware-verified; a
physical XP-60 capture is still required before any row may be promoted. See
`DEVICE_ACCEPTANCE.md`.

### Machine-readable conventions

The two parameter tables below are parsed by
`tools/generate_performance_tables.py`, so their shape is load-bearing:

- Seven columns, in the order shown, one row per parameter.
- A row's **Display** column must either read `same as raw`, give an integer
  range that scales linearly from the raw range, use Roland's pan or note
  notation, or list **exactly one label per raw value**. Enumerations are
  spelled out rather than abbreviated with a dash (`GROUP1, GROUP2, …` not
  `GROUP1..GROUP7`) so the count can be checked against the raw range — a
  mismatch fails the generator rather than shifting every later label.
- A row using `2 / nibble` occupies two offsets, and the next row's offset must
  account for that. The generator checks that the rows tile the declared total
  size exactly, with no gap and no overlap.

### Encoding notation

Identical to the Patch map:

- `1 / 7-bit (...)` — one SysEx data byte; the parenthesised bit pattern is
  Roland's exact field notation.
- `2 / nibble` — the logical value is carried in two consecutive low-nibble
  bytes (`0000 aaaa`, `0000 bbbb`), so the row occupies **two** offsets.
- `1 / ASCII` — a name byte as listed by Roland.
- `same as raw` — the map gives no separate parenthetical display mapping. It
  does **not** authorise a later layer to reinterpret the raw byte silently.

## Performance block layout — `1-2.Performance`

| Offset inside Performance | Block | Status | Source |
|---|---|---|---|
| `00 00` | Performance Common | Documentation-derived | Parameter Address Map §1-2 |
| `10 00` | Performance Part 1 | Documentation-derived | Parameter Address Map §1-2 |
| `11 00` | Performance Part 2 | Documentation-derived | Parameter Address Map §1-2 |
| … | Parts 3–15 continue at stride `01 00` | Documentation-derived | Parameter Address Map §1-2 |
| `1F 00` | Performance Part 16 | Documentation-derived | Parameter Address Map §1-2 |

Base addresses for the Performance regions themselves (Temporary `01 00 00 00`,
User bank `10 00 00 00` with stride `00 01 00 00`) are in
`ROLAND_XP60_PROTOCOL_FACTS.md` §3 and are not restated here.

## 1. Performance Common — `1-2-1`

Total size `00 00 00 42` (66 bytes, offsets `00 00`…`00 41`).

| Offset | Parameter | Bytes / encoding | Raw range | Display | Status | Source |
|---|---|---|---|---|---|---|
| `00 00` | Performance Name 1 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 01` | Performance Name 2 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 02` | Performance Name 3 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 03` | Performance Name 4 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 04` | Performance Name 5 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 05` | Performance Name 6 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 06` | Performance Name 7 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 07` | Performance Name 8 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 08` | Performance Name 9 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 09` | Performance Name 10 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 0A` | Performance Name 11 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 0B` | Performance Name 12 | 1 / ASCII (`0aaa aaaa`) | 32..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 0C` | EFX Source | 1 / 7-bit (`0000 aaaa`) | 0..15 | PERFORM, 1..9, 11..16 | Documentation-derived | Parameter Address Map §1-2-1 note *1 |
| `00 0D` | EFX Type | 1 / 7-bit (`00aa aaaa`) | 0..39 | 1..40 | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 0E` | EFX Parameter 1 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 0F` | EFX Parameter 2 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 10` | EFX Parameter 3 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 11` | EFX Parameter 4 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 12` | EFX Parameter 5 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 13` | EFX Parameter 6 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 14` | EFX Parameter 7 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 15` | EFX Parameter 8 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 16` | EFX Parameter 9 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 17` | EFX Parameter 10 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 18` | EFX Parameter 11 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 19` | EFX Parameter 12 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 1A` | EFX Output Assign | 1 / 7-bit (`0000 00aa`) | 0..2 | MIX, DIR, `<OUTPUT-2>` | Documentation-derived | Parameter Address Map §1-2-1 note *2 |
| `00 1B` | EFX Mix Out Send Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 1C` | EFX Chorus Send Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 1D` | EFX Reverb Send Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 1E` | EFX Control Source 1 | 1 / 7-bit (`0000 aaaa`) | 0..10 | OFF, SYS-CTRL1, SYS-CTRL2, MODULATION, BREATH, FOOT, VOLUME, PAN, EXPRESSION, PITCH BEND, AFTERTOUCH | Documentation-derived | Parameter Address Map §1-2-1 note *3 |
| `00 1F` | EFX Control Depth 1 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 20` | EFX Control Source 2 | 1 / 7-bit (`0000 aaaa`) | 0..10 | OFF, SYS-CTRL1, SYS-CTRL2, MODULATION, BREATH, FOOT, VOLUME, PAN, EXPRESSION, PITCH BEND, AFTERTOUCH | Documentation-derived | Parameter Address Map §1-2-1 note *3 |
| `00 21` | EFX Control Depth 2 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 22` | Chorus Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 23` | Chorus Rate | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 24` | Chorus Depth | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 25` | Chorus Pre-Delay | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 26` | Chorus Feedback | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 27` | Chorus Output | 1 / 7-bit (`0000 00aa`) | 0..2 | MIX, REVERB, MIX+REV | Documentation-derived | Parameter Address Map §1-2-1 note *4 |
| `00 28` | Reverb Type | 1 / 7-bit (`0000 0aaa`) | 0..7 | ROOM1, ROOM2, STAGE1, STAGE2, HALL1, HALL2, DELAY, PAN-DLY | Documentation-derived | Parameter Address Map §1-2-1 note *5 |
| `00 29` | Reverb Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 2A` | Reverb Time | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 2B` | Reverb HF Damp | 1 / 7-bit (`000a aaaa`) | 0..17 | 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, BYPASS | Documentation-derived | Parameter Address Map §1-2-1 note *6 |
| `00 2C` | Delay Feedback | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 2D` | Performance Tempo | 2 / nibble (`0000 aaaa`, `0000 bbbb`) | 20..250 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 2F` | Keyboard Range Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 30` | Voice Reserve 1 | 1 / 7-bit (`0aaa aaaa`) | 0..64 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 31` | Voice Reserve 2 | 1 / 7-bit (`0aaa aaaa`) | 0..64 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 32` | Voice Reserve 3 | 1 / 7-bit (`0aaa aaaa`) | 0..64 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 33` | Voice Reserve 4 | 1 / 7-bit (`0aaa aaaa`) | 0..64 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 34` | Voice Reserve 5 | 1 / 7-bit (`0aaa aaaa`) | 0..64 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 35` | Voice Reserve 6 | 1 / 7-bit (`0aaa aaaa`) | 0..64 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 36` | Voice Reserve 7 | 1 / 7-bit (`0aaa aaaa`) | 0..64 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 37` | Voice Reserve 8 | 1 / 7-bit (`0aaa aaaa`) | 0..64 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 38` | Voice Reserve 9 | 1 / 7-bit (`0aaa aaaa`) | 0..64 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 39` | Voice Reserve 10 | 1 / 7-bit (`0aaa aaaa`) | 0..64 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 3A` | Voice Reserve 11 | 1 / 7-bit (`0aaa aaaa`) | 0..64 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 3B` | Voice Reserve 12 | 1 / 7-bit (`0aaa aaaa`) | 0..64 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 3C` | Voice Reserve 13 | 1 / 7-bit (`0aaa aaaa`) | 0..64 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 3D` | Voice Reserve 14 | 1 / 7-bit (`0aaa aaaa`) | 0..64 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 3E` | Voice Reserve 15 | 1 / 7-bit (`0aaa aaaa`) | 0..64 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 3F` | Voice Reserve 16 | 1 / 7-bit (`0aaa aaaa`) | 0..64 | same as raw | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 40` | Keyboard Mode | 1 / 7-bit (`0000 000a`) | 0..1 | LAYER, SINGLE | Documentation-derived | Parameter Address Map §1-2-1 |
| `00 41` | Clock Source | 1 / 7-bit (`0000 000a`) | 0..1 | PERFORMANCE, SEQUENCER | Documentation-derived | Parameter Address Map §1-2-1 note *7 |

### Note on EFX Source (`00 0C`)

Roland's footnote *1 prints as `(PERFORM,1 - 9,11 - 16)`. Two OCR passes at
different resolutions disagreed on the one two-digit number (`11` at 200 dpi,
`14`/`17` at 400 dpi), so it was settled by counting rather than by reading:
`PERFORM` plus `1..9` plus `11..16` is **exactly 16 values**, matching the
declared raw range `0..15`, and no other reading of that digit gives 16.

The gap at Part 10 is consistent with the rest of the instrument — Part 10 is
the Rhythm part, which is why the Temporary Rhythm Setup sits at `02 09 00 00`
(`ROLAND_XP60_PROTOCOL_FACTS.md` §3) — so the EFX cannot be sourced from it.
Recorded here because it is a reconstruction from arithmetic, not a clean read.

## 2. Performance Part — `1-2-2`

Total size `00 00 00 19` (25 bytes, offsets `00 00`…`00 18`). This table is one
Part; the Performance holds sixteen of them at the addresses above.

| Offset | Parameter | Bytes / encoding | Raw range | Display | Status | Source |
|---|---|---|---|---|---|---|
| `00 00` | Receive Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 01` | MIDI Channel | 1 / 7-bit (`0000 aaaa`) | 0..15 | 1..16 | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 02` | Patch Group Type | 1 / 7-bit (`0000 00aa`) | 0..2 | USER&PRESET, `<PCM>`, EXP | Documentation-derived | Parameter Address Map §1-2-2 note *1 |
| `00 03` | Patch Group ID | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 04` | Patch Number | 2 / nibble (`0000 aaaa`, `0000 bbbb`) | 0..254 | 001..255 | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 06` | Part Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 07` | Part Pan | 1 / 7-bit (`0aaa aaaa`) | 0..127 | L64..63R | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 08` | Part Coarse Tune | 1 / 7-bit (`0aaa aaaa`) | 0..96 | -48..+48 | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 09` | Part Fine Tune | 1 / 7-bit (`0aaa aaaa`) | 0..100 | -50..+50 | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 0A` | Output Assign | 1 / 7-bit (`0000 0aaa`) | 0..4 | MIX, EFX, DIR, `<OUTPUT-2>`, PAT | Documentation-derived | Parameter Address Map §1-2-2 note *2 |
| `00 0B` | Mix/EFX Send Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 0C` | Chorus Send Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 0D` | Reverb Send Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 0E` | Receive Program Change Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 0F` | Receive Volume Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 10` | Receive Hold-1 Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 11` | Keyboard Range Lower | 1 / 7-bit (`0aaa aaaa`) | 0..127 | C-1..Upper | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 12` | Keyboard Range Upper | 1 / 7-bit (`0aaa aaaa`) | 0..127 | Lower..G9 | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 13` | Octave Shift | 1 / 7-bit (`0000 0aaa`) | 0..6 | -3..+3 | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 14` | Local Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 15` | Transmit Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | Parameter Address Map §1-2-2 |
| `00 16` | Transmit Bank Select Group | 1 / 7-bit (`0000 0aaa`) | 0..7 | PATCH, GROUP1, GROUP2, GROUP3, GROUP4, GROUP5, GROUP6, GROUP7 | Documentation-derived | Parameter Address Map §1-2-2 note *3 |
| `00 17` | Transmit Volume | 2 / nibble (`0000 aaaa`, `0000 bbbb`) | 0..128 | 0..127, OFF | Documentation-derived | Parameter Address Map §1-2-2 |

### Paired-range invariant

`Keyboard Range Lower` and `Keyboard Range Upper` are printed with each other as
a bound (`C-1..Upper`, `Lower..G9`). This is the same invariant the Patch Tone
key range carries, and the same rule applies: an edit that would cross the pair
is rejected rather than clamped, so no editor entry point can produce a Part
Roland's own map does not describe.

## What is deliberately not here

- **`1-1.System Common` and `1-1-2.Scale Tune`** (§1-1, PDF p.224). The
  System-data half of Phase 8, not started. Their total sizes read
  `00 00 00 60` (96 bytes) and `00 00 00 0C` (12 bytes).
- **`1-4.Rhythm Setup`, `1-4-1.Rhythm Common`, `1-4-2.Rhythm Note`** (§1-4,
  PDF p.227). The Rhythm-editor half of Phase 8, not started. Reading their
  addresses out of the fixture settles what the two block shapes
  `ROLAND_XP60_PROTOCOL_FACTS.md` §3 listed as unexplained: the 12-byte blocks
  sit at `10 40 00 00` and `10 41 00 00`, the two User Rhythm Setups, so they
  are **Rhythm Common**; the 128 blocks of 58 bytes sit at `10 4n 23 00`…
  `10 4n 62 00`, sixty-four per set, so they are **Rhythm Note** — and offsets
  `23`..`62` are MIDI notes 35..98, the instrument's rhythm key range. Sizes and
  positions only; no Rhythm parameter is transcribed.
- **Any EFX algorithm parameter meaning.** `EFX Parameter 1..12` are raw here
  exactly as they are for the Patch, and for the same reason: which control each
  slot carries is per-algorithm and unverified. See `DEVICE_ACCEPTANCE.md`
  area 8.
