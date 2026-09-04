# XP-60 Patch Parameter Map (transcription in progress)

This file is the machine-checkable source for the XP-60 **Patch Common** and
**Patch Tone** parameter tables in `src/xpmodel/Xp60PatchLayout.cpp`. Every row
in the code must have a row here, and every row here names where it came from.

Status legend is the one in `ROLAND_XP60_PROTOCOL_FACTS.md`. As of Phase 2
start only the entries in section 1 are transcribed; the tables in code declare
themselves **Partial** until this file is complete.

## Inputs still required

To finish Phase 2 the following pages of the Roland XP-60/XP-80 MIDI
Implementation, *Parameter Address Map*, are needed (photo, scan or typed):

1. **Patch Common** — every parameter with its offset, size in bytes / nibble
   encoding, raw range, display range, and the block's total size.
2. **Patch Tone** — the same for one Tone, plus the offsets of Tone 1, 2, 3, 4
   inside a Patch (JV-1080 uses `10 00`, `12 00`, `14 00`, `16 00`; the XP-60
   values must be read from its own map, not assumed).
3. Optionally a **DT1 dump of the Temporary Patch** captured from the XP-60
   (any patch), which becomes the first golden fixture.

Transcribe rows in the table format below; the C++ tables are then generated
by hand from this file one block at a time, with the block tests asserting the
total size and that `encode(decode(bytes)) == bytes` on the fixture.

## Row format

| Offset (hex, within block) | Parameter (Roland name) | Bytes / encoding | Raw range | Display | Status | Source |
|---|---|---|---|---|---|---|

- *Bytes / encoding*: `1 / 7-bit`, `2 / nibble` (0000 aaaa 0000 bbbb → 0..255),
  `4 / nibble`, or `1 / ASCII`.
- *Display*: how the front panel shows the raw value, e.g. `raw - 64` or an
  enumeration list.

## 1. Patch Common — transcribed rows

| Offset | Parameter | Bytes / encoding | Raw range | Display | Status | Source |
|---|---|---|---|---|---|---|
| `00 00` | Patch Name 1 | 1 / ASCII | 32..126 | character | Documentation-derived | MIDI Implementation, confirmed in PR #5 review |
| `00 01` | Patch Name 2 | 1 / ASCII | 32..126 | character | Documentation-derived | same |
| `00 02` | Patch Name 3 | 1 / ASCII | 32..126 | character | Documentation-derived | same |
| `00 03` | Patch Name 4 | 1 / ASCII | 32..126 | character | Documentation-derived | same |
| `00 04` | Patch Name 5 | 1 / ASCII | 32..126 | character | Documentation-derived | same |
| `00 05` | Patch Name 6 | 1 / ASCII | 32..126 | character | Documentation-derived | same |
| `00 06` | Patch Name 7 | 1 / ASCII | 32..126 | character | Documentation-derived | same |
| `00 07` | Patch Name 8 | 1 / ASCII | 32..126 | character | Documentation-derived | same |
| `00 08` | Patch Name 9 | 1 / ASCII | 32..126 | character | Documentation-derived | same |
| `00 09` | Patch Name 10 | 1 / ASCII | 32..126 | character | Documentation-derived | same |
| `00 0A` | Patch Name 11 | 1 / ASCII | 32..126 | character | Documentation-derived | same |
| `00 0B` | Patch Name 12 | 1 / ASCII | 32..126 | character | Documentation-derived | same |
| `00 0C` … | *remaining Patch Common parameters* | | | | **Unknown** | awaiting the map |

Open question: the exact character set the XP-60 allows in names. The code
accepts printable ASCII `20H..7EH`; if the manual lists a narrower set, tighten
`PatchName::isAllowedChar` and record it here.

Patch Common total size: **Unknown** (awaiting the map).

## 2. Patch Tone — transcribed rows

| Offset | Parameter | Bytes / encoding | Raw range | Display | Status | Source |
|---|---|---|---|---|---|---|
| — | *no rows transcribed yet* | | | | **Unknown** | awaiting the map |

Tone size: **Unknown**. Tone 1–4 offsets within a Patch: **Unknown**.

## 3. Facts already established (from ROLAND_XP60_PROTOCOL_FACTS.md)

| Fact | Value | Status |
|---|---|---|
| Temporary Patch (Patch mode) base | `03 00 00 00` | Documentation-derived |
| User Patch USER:001 base / stride | `11 00 00 00` / `00 01 00 00` | Documentation-derived |
| DT1 packet limit | 128 data bytes, ≥ 20 ms apart | Documentation-derived |
