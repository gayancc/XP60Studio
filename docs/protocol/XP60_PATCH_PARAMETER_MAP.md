# XP-60 Patch Parameter Map

This file is the machine-checkable transcription source for the XP-60 **Patch Common** and **Patch Tone** parameter tables used by Phase 2. Every row implemented in `src/xpmodel/Xp60PatchLayout.cpp` must have a row here, and every row here names the Roland source page.

## Source material and transcription policy

The source is the **Roland XP-60/XP-80 Owner's Manual, MIDI Implementation, Parameter Address Map**, specifically:

- p.223 — `1-3.Patch`, Tone offsets, and the first part of `1-3-1.Patch Common`
- p.224 — remainder of `1-3-1.Patch Common` and most of `1-3-2.Patch Tone`
- p.225 — end of `1-3-2.Patch Tone` and its footnotes

Source mirror used for this transcription because the conversation-uploaded PDF ends before the MIDI Implementation appendix:

- https://roland.manymanuals.de/musical-instruments/xp-60/owners-manual-16751/223
- https://roland.manymanuals.de/musical-instruments/xp-60/owners-manual-16751/224
- https://roland.manymanuals.de/musical-instruments/xp-60/owners-manual-16751/225

**No JV-1080, XP-30, or other JV/XP-family parameter values were substituted.** Values in angle brackets such as `<PCM>`, `<TAP>`, `<TAP-SYNC>`, and `<OUTPUT-2>` are preserved exactly as Roland prints them rather than interpreted here.

Status legend is the one in `ROLAND_XP60_PROTOCOL_FACTS.md`. All rows below are **Documentation-derived**, not Hardware-verified. A physical XP-60 DT1 capture is still required before any row may be promoted to Hardware-verified.

### Encoding notation

- `1 / 7-bit (...)` means one SysEx data byte; the parenthesized bit pattern is Roland's exact field notation.
- `2 / nibble` means the logical value is encoded in two consecutive low-nibble bytes (`0000 aaaa`, `0000 bbbb`).
- `1 / ASCII` is the Patch-name byte as listed by Roland.
- `same as raw` means the Parameter Address Map gives no separate parenthetical display mapping. It does **not** authorize a later layer to normalize or reinterpret the raw byte silently.

## Patch block layout

| Offset inside Patch | Block | Status | Source |
|---|---|---|---|
| `00 00` | Patch Common | Documentation-derived | MIDI Implementation p.223 |
| `10 00` | Patch Tone 1 | Documentation-derived | MIDI Implementation p.223 |
| `12 00` | Patch Tone 2 | Documentation-derived | MIDI Implementation p.223 |
| `14 00` | Patch Tone 3 | Documentation-derived | MIDI Implementation p.223 |
| `16 00` | Patch Tone 4 | Documentation-derived | MIDI Implementation p.223 |

## Row format

| Offset (hex, within block) | Parameter (Roland name) | Bytes / encoding | Raw range | Display | Status | Source |
|---|---|---|---|---|---|---|

## 1. Patch Common

| Offset | Parameter | Bytes / encoding | Raw range | Display | Status | Source |
|---|---|---|---|---|---|---|
| `00 00` | Patch Name 1 | 1 / ASCII (`0aaa aaaa`) | 32..127 | character | Documentation-derived | MIDI Implementation p.223 |
| `00 01` | Patch Name 2 | 1 / ASCII (`0aaa aaaa`) | 32..127 | character | Documentation-derived | MIDI Implementation p.223 |
| `00 02` | Patch Name 3 | 1 / ASCII (`0aaa aaaa`) | 32..127 | character | Documentation-derived | MIDI Implementation p.223 |
| `00 03` | Patch Name 4 | 1 / ASCII (`0aaa aaaa`) | 32..127 | character | Documentation-derived | MIDI Implementation p.223 |
| `00 04` | Patch Name 5 | 1 / ASCII (`0aaa aaaa`) | 32..127 | character | Documentation-derived | MIDI Implementation p.223 |
| `00 05` | Patch Name 6 | 1 / ASCII (`0aaa aaaa`) | 32..127 | character | Documentation-derived | MIDI Implementation p.223 |
| `00 06` | Patch Name 7 | 1 / ASCII (`0aaa aaaa`) | 32..127 | character | Documentation-derived | MIDI Implementation p.223 |
| `00 07` | Patch Name 8 | 1 / ASCII (`0aaa aaaa`) | 32..127 | character | Documentation-derived | MIDI Implementation p.223 |
| `00 08` | Patch Name 9 | 1 / ASCII (`0aaa aaaa`) | 32..127 | character | Documentation-derived | MIDI Implementation p.223 |
| `00 09` | Patch Name 10 | 1 / ASCII (`0aaa aaaa`) | 32..127 | character | Documentation-derived | MIDI Implementation p.223 |
| `00 0A` | Patch Name 11 | 1 / ASCII (`0aaa aaaa`) | 32..127 | character | Documentation-derived | MIDI Implementation p.223 |
| `00 0B` | Patch Name 12 | 1 / ASCII (`0aaa aaaa`) | 32..127 | character | Documentation-derived | MIDI Implementation p.223 |
| `00 0C` | EFX Type | 1 / 7-bit (`00aa aaaa`) | 0..39 | 1..40 | Documentation-derived | MIDI Implementation p.223 |
| `00 0D` | EFX Parameter 1 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | effect-specific; map gives raw 0..127 | Documentation-derived | MIDI Implementation p.223 |
| `00 0E` | EFX Parameter 2 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | effect-specific; map gives raw 0..127 | Documentation-derived | MIDI Implementation p.223 |
| `00 0F` | EFX Parameter 3 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | effect-specific; map gives raw 0..127 | Documentation-derived | MIDI Implementation p.223 |
| `00 10` | EFX Parameter 4 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | effect-specific; map gives raw 0..127 | Documentation-derived | MIDI Implementation p.223 |
| `00 11` | EFX Parameter 5 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | effect-specific; map gives raw 0..127 | Documentation-derived | MIDI Implementation p.223 |
| `00 12` | EFX Parameter 6 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | effect-specific; map gives raw 0..127 | Documentation-derived | MIDI Implementation p.223 |
| `00 13` | EFX Parameter 7 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | effect-specific; map gives raw 0..127 | Documentation-derived | MIDI Implementation p.223 |
| `00 14` | EFX Parameter 8 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | effect-specific; map gives raw 0..127 | Documentation-derived | MIDI Implementation p.223 |
| `00 15` | EFX Parameter 9 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | effect-specific; map gives raw 0..127 | Documentation-derived | MIDI Implementation p.223 |
| `00 16` | EFX Parameter 10 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | effect-specific; map gives raw 0..127 | Documentation-derived | MIDI Implementation p.223 |
| `00 17` | EFX Parameter 11 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | effect-specific; map gives raw 0..127 | Documentation-derived | MIDI Implementation p.223 |
| `00 18` | EFX Parameter 12 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | effect-specific; map gives raw 0..127 | Documentation-derived | MIDI Implementation p.223 |
| `00 19` | EFX Output Assign | 1 / 7-bit (`0000 00aa`) | 0..2 | MIX, DIR, `<OUTPUT-2>` | Documentation-derived | MIDI Implementation p.223 |
| `00 1A` | EFX Mix Out Send Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.223 |
| `00 1B` | EFX Chorus Send Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.223 |
| `00 1C` | EFX Reverb Send Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.223 |
| `00 1D` | EFX Control Source 1 | 1 / 7-bit (`0000 aaaa`) | 0..10 | OFF, SYS-CTRL1, SYS-CTRL2, MODULATION, BREATH, FOOT, VOLUME, PAN, EXPRESSION, PITCH BEND, AFTERTOUCH | Documentation-derived | MIDI Implementation p.223 |
| `00 1E` | EFX Control Depth 1 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.223 |
| `00 1F` | EFX Control Source 2 | 1 / 7-bit (`0000 aaaa`) | 0..10 | OFF, SYS-CTRL1, SYS-CTRL2, MODULATION, BREATH, FOOT, VOLUME, PAN, EXPRESSION, PITCH BEND, AFTERTOUCH | Documentation-derived | MIDI Implementation p.223 |
| `00 20` | EFX Control Depth 2 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.223 |
| `00 21` | Chorus Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.223 |
| `00 22` | Chorus Rate | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.223 |
| `00 23` | Chorus Depth | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.223 |
| `00 24` | Chorus Pre-Delay | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.223 |
| `00 25` | Chorus Feedback | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.223 |
| `00 26` | Chorus Output | 1 / 7-bit (`0000 00aa`) | 0..2 | MIX, REVERB, MIX+REV | Documentation-derived | MIDI Implementation p.223 |
| `00 27` | Reverb Type | 1 / 7-bit (`0000 0aaa`) | 0..7 | ROOM1, ROOM2, STAGE1, STAGE2, HALL1, HALL2, DELAY, PAN-DLY | Documentation-derived | MIDI Implementation p.223 |
| `00 28` | Reverb Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.223 |
| `00 29` | Reverb Time | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.223 |
| `00 2A` | Reverb HF Damp | 1 / 7-bit (`000a aaaa`) | 0..17 | 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, BYPASS | Documentation-derived | MIDI Implementation p.223 |
| `00 2B` | Delay Feedback | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.223 |
| `00 2C` | Patch Tempo | 2 / nibble (`0000 aaaa`, `0000 bbbb`) | 20..250 | 20..250 | Documentation-derived | MIDI Implementation p.223 |
| `00 2E` | Patch Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.223 |
| `00 2F` | Patch Pan | 1 / 7-bit (`0aaa aaaa`) | 0..127 | L64..63R | Documentation-derived | MIDI Implementation p.223 |
| `00 30` | Analog Feel | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.223 |
| `00 31` | Bend Range Up | 1 / 7-bit (`0000 aaaa`) | 0..12 | 0..12 | Documentation-derived | MIDI Implementation p.223 |
| `00 32` | Bend Range Down | 1 / 7-bit (`00aa aaaa`) | 0..48 | 0..-48 | Documentation-derived | MIDI Implementation p.223 |
| `00 33` | Key Assign Mode | 1 / 7-bit (`0000 000a`) | 0..1 | POLY, SOLO | Documentation-derived | MIDI Implementation p.223 |
| `00 34` | Solo Legato | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | MIDI Implementation p.223 |
| `00 35` | Portamento Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | MIDI Implementation p.223 |
| `00 36` | Portamento Mode | 1 / 7-bit (`0000 000a`) | 0..1 | NORMAL, LEGATO | Documentation-derived | MIDI Implementation pp.223-224 |
| `00 37` | Portamento Type | 1 / 7-bit (`0000 000a`) | 0..1 | RATE, TIME | Documentation-derived | MIDI Implementation p.224 |
| `00 38` | Portamento Start | 1 / 7-bit (`0000 000a`) | 0..1 | PITCH, NOTE | Documentation-derived | MIDI Implementation p.224 |
| `00 39` | Portamento Time | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 3A` | Patch Control Source 2 | 1 / 7-bit (`0000 aaaa`) | 0..15 | OFF, SYS-CTRL1, SYS-CTRL2, MODULATION, BREATH, FOOT, VOLUME, PAN, EXPRESSION, PITCH BEND, AFTERTOUCH, LFO1, LFO2, VELOCITY, KEYFOLLOW, PLAYMATE | Documentation-derived | MIDI Implementation p.224 |
| `00 3B` | Patch Control Source 3 | 1 / 7-bit (`0000 aaaa`) | 0..15 | OFF, SYS-CTRL1, SYS-CTRL2, MODULATION, BREATH, FOOT, VOLUME, PAN, EXPRESSION, PITCH BEND, AFTERTOUCH, LFO1, LFO2, VELOCITY, KEYFOLLOW, PLAYMATE | Documentation-derived | MIDI Implementation p.224 |
| `00 3C` | EFX Control Hold/Peak | 1 / 7-bit (`0000 00aa`) | 0..2 | OFF, HOLD, PEAK | Documentation-derived | MIDI Implementation p.224 |
| `00 3D` | Control 1 Hold/Peak | 1 / 7-bit (`0000 00aa`) | 0..2 | OFF, HOLD, PEAK | Documentation-derived | MIDI Implementation p.224 |
| `00 3E` | Control 2 Hold/Peak | 1 / 7-bit (`0000 00aa`) | 0..2 | OFF, HOLD, PEAK | Documentation-derived | MIDI Implementation p.224 |
| `00 3F` | Control 3 Hold/Peak | 1 / 7-bit (`0000 00aa`) | 0..2 | OFF, HOLD, PEAK | Documentation-derived | MIDI Implementation p.224 |
| `00 40` | Velocity Range Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | MIDI Implementation p.224 |
| `00 41` | Octave Shift | 1 / 7-bit (`0000 0aaa`) | 0..6 | -3..+3 | Documentation-derived | MIDI Implementation p.224 |
| `00 42` | Stretch Tune Depth | 1 / 7-bit (`0000 00aa`) | 0..3 | OFF, 1..3 | Documentation-derived | MIDI Implementation p.224 |
| `00 43` | Voice Priority | 1 / 7-bit (`0000 000a`) | 0..1 | LAST, LOUDEST | Documentation-derived | MIDI Implementation p.224 |
| `00 44` | Structure Type 1&2 | 1 / 7-bit (`0000 aaaa`) | 0..9 | 1..10 | Documentation-derived | MIDI Implementation p.224 |
| `00 45` | Booster 1&2 | 1 / 7-bit (`0000 00aa`) | 0..3 | 0, +6, +12, +18 | Documentation-derived | MIDI Implementation p.224 |
| `00 46` | Structure Type 3&4 | 1 / 7-bit (`0000 aaaa`) | 0..9 | 1..10 | Documentation-derived | MIDI Implementation p.224 |
| `00 47` | Booster 3&4 | 1 / 7-bit (`0000 00aa`) | 0..3 | 0, +6, +12, +18 | Documentation-derived | MIDI Implementation p.224 |
| `00 48` | Clock Source | 1 / 7-bit (`0000 000a`) | 0..1 | PATCH, SEQUENCER | Documentation-derived | MIDI Implementation p.224 |

**Patch Common total size:** `00 00 00 49` = **73 bytes** (Roland base-128 address/size notation), Documentation-derived, MIDI Implementation p.224.

### Source correction from the earlier partial transcription

Roland prints Patch Name 1-12 as raw **32..127**. The previous partial document said `32..126`; that narrower value was not source-faithful. The protocol map above records Roland's `32..127` exactly. Any application-level decision about printable/displayable characters must remain separate from the protocol range and must not rewrite unknown/raw bytes during a round trip.

## 2. Patch Tone

| Offset | Parameter | Bytes / encoding | Raw range | Display | Status | Source |
|---|---|---|---|---|---|---|
| `00 00` | Tone Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | MIDI Implementation p.224 |
| `00 01` | Wave Group Type | 1 / 7-bit (`0000 00aa`) | 0..2 | INT, `<PCM>`, EXP | Documentation-derived | MIDI Implementation p.224 |
| `00 02` | Wave Group ID | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 03` | Wave Number | 2 / nibble (`0000 aaaa`, `0000 bbbb`) | 0..254 | 001..255 | Documentation-derived | MIDI Implementation p.224 |
| `00 05` | Wave Gain | 1 / 7-bit (`0000 00aa`) | 0..3 | -6, 0, +6, +12 | Documentation-derived | MIDI Implementation p.224 |
| `00 06` | FXM Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | MIDI Implementation p.224 |
| `00 07` | FXM Color | 1 / 7-bit (`0000 00aa`) | 0..3 | 1..4 | Documentation-derived | MIDI Implementation p.224 |
| `00 08` | FXM Depth | 1 / 7-bit (`0000 aaaa`) | 0..15 | 1..16 | Documentation-derived | MIDI Implementation p.224 |
| `00 09` | Tone Delay Mode | 1 / 7-bit (`0000 0aaa`) | 0..7 | NORMAL, HOLD, PLAYMATE, CLOCK-SYNC, `<TAP-SYNC>`, KEY-OFF-N, KEY-OFF-D, TEMPO-SYNC | Documentation-derived | MIDI Implementation p.224 |
| `00 0A` | Tone Delay Time | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 0B` | Velocity Cross Fade | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 0C` | Velocity Range Lower | 1 / 7-bit (`0aaa aaaa`) | 1..127 | 1..Upper | Documentation-derived | MIDI Implementation p.224 |
| `00 0D` | Velocity Range Upper | 1 / 7-bit (`0aaa aaaa`) | 1..127 | Lower..127 | Documentation-derived | MIDI Implementation p.224 |
| `00 0E` | Keyboard Range Lower | 1 / 7-bit (`0aaa aaaa`) | 0..127 | C-1..Upper | Documentation-derived | MIDI Implementation p.224 |
| `00 0F` | Keyboard Range Upper | 1 / 7-bit (`0aaa aaaa`) | 0..127 | Lower..G9 | Documentation-derived | MIDI Implementation p.224 |
| `00 10` | Redamper Control Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | MIDI Implementation p.224 |
| `00 11` | Volume Control Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | MIDI Implementation p.224 |
| `00 12` | Hold-1 Control Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | MIDI Implementation p.224 |
| `00 13` | Bender Control Switch | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | MIDI Implementation p.224 |
| `00 14` | Pan Control Switch | 1 / 7-bit (`0000 00aa`) | 0..2 | OFF, CONTINUOUS, KEY-ON | Documentation-derived | MIDI Implementation p.224 |
| `00 15` | Controller 1 Destination 1 | 1 / 7-bit (`000a aaaa`) | 0..18 | see Tone footnote 3 | Documentation-derived | MIDI Implementation p.224 |
| `00 16` | Controller 1 Depth 1 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 17` | Controller 1 Destination 2 | 1 / 7-bit (`000a aaaa`) | 0..18 | see Tone footnote 3 | Documentation-derived | MIDI Implementation p.224 |
| `00 18` | Controller 1 Depth 2 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 19` | Controller 1 Destination 3 | 1 / 7-bit (`000a aaaa`) | 0..18 | see Tone footnote 3 | Documentation-derived | MIDI Implementation p.224 |
| `00 1A` | Controller 1 Depth 3 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 1B` | Controller 1 Destination 4 | 1 / 7-bit (`000a aaaa`) | 0..18 | see Tone footnote 3 | Documentation-derived | MIDI Implementation p.224 |
| `00 1C` | Controller 1 Depth 4 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 1D` | Controller 2 Destination 1 | 1 / 7-bit (`000a aaaa`) | 0..18 | see Tone footnote 3 | Documentation-derived | MIDI Implementation p.224 |
| `00 1E` | Controller 2 Depth 1 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 1F` | Controller 2 Destination 2 | 1 / 7-bit (`000a aaaa`) | 0..18 | see Tone footnote 3 | Documentation-derived | MIDI Implementation p.224 |
| `00 20` | Controller 2 Depth 2 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 21` | Controller 2 Destination 3 | 1 / 7-bit (`000a aaaa`) | 0..18 | see Tone footnote 3 | Documentation-derived | MIDI Implementation p.224 |
| `00 22` | Controller 2 Depth 3 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 23` | Controller 2 Destination 4 | 1 / 7-bit (`000a aaaa`) | 0..18 | see Tone footnote 3 | Documentation-derived | MIDI Implementation p.224 |
| `00 24` | Controller 2 Depth 4 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 25` | Controller 3 Destination 1 | 1 / 7-bit (`000a aaaa`) | 0..18 | see Tone footnote 3 | Documentation-derived | MIDI Implementation p.224 |
| `00 26` | Controller 3 Depth 1 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 27` | Controller 3 Destination 2 | 1 / 7-bit (`000a aaaa`) | 0..18 | see Tone footnote 3 | Documentation-derived | MIDI Implementation p.224 |
| `00 28` | Controller 3 Depth 2 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 29` | Controller 3 Destination 3 | 1 / 7-bit (`000a aaaa`) | 0..18 | see Tone footnote 3 | Documentation-derived | MIDI Implementation p.224 |
| `00 2A` | Controller 3 Depth 3 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 2B` | Controller 3 Destination 4 | 1 / 7-bit (`000a aaaa`) | 0..18 | see Tone footnote 3 | Documentation-derived | MIDI Implementation p.224 |
| `00 2C` | Controller 3 Depth 4 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 2D` | LFO1 Waveform | 1 / 7-bit (`0000 0aaa`) | 0..7 | see Tone footnote 4 | Documentation-derived | MIDI Implementation p.224 |
| `00 2E` | LFO1 Key Trigger | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | MIDI Implementation p.224 |
| `00 2F` | LFO1 Rate | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 30` | LFO1 Offset | 1 / 7-bit (`0000 0aaa`) | 0..4 | see Tone footnote 5 | Documentation-derived | MIDI Implementation p.224 |
| `00 31` | LFO1 Delay Time | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 32` | LFO1 Fade Mode | 1 / 7-bit (`0000 00aa`) | 0..3 | see Tone footnote 6 | Documentation-derived | MIDI Implementation p.224 |
| `00 33` | LFO1 Fade Time | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 34` | LFO1 External Sync | 1 / 7-bit (`0000 00aa`) | 0..2 | see Tone footnote 7 | Documentation-derived | MIDI Implementation p.224 |
| `00 35` | LFO2 Waveform | 1 / 7-bit (`0000 0aaa`) | 0..7 | see Tone footnote 4 | Documentation-derived | MIDI Implementation p.224 |
| `00 36` | LFO2 Key Trigger | 1 / 7-bit (`0000 000a`) | 0..1 | OFF, ON | Documentation-derived | MIDI Implementation p.224 |
| `00 37` | LFO2 Rate | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 38` | LFO2 Offset | 1 / 7-bit (`0000 0aaa`) | 0..4 | see Tone footnote 5 | Documentation-derived | MIDI Implementation p.224 |
| `00 39` | LFO2 Delay Time | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 3A` | LFO2 Fade Mode | 1 / 7-bit (`0000 00aa`) | 0..3 | see Tone footnote 6 | Documentation-derived | MIDI Implementation p.224 |
| `00 3B` | LFO2 Fade Time | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 3C` | LFO2 External Sync | 1 / 7-bit (`0000 00aa`) | 0..2 | see Tone footnote 7 | Documentation-derived | MIDI Implementation p.224 |
| `00 3D` | Coarse Tune | 1 / 7-bit (`0aaa aaaa`) | 0..96 | -48..+48 | Documentation-derived | MIDI Implementation p.224 |
| `00 3E` | Fine Tune | 1 / 7-bit (`0aaa aaaa`) | 0..100 | -50..+50 | Documentation-derived | MIDI Implementation p.224 |
| `00 3F` | Random Pitch Depth | 1 / 7-bit (`000a aaaa`) | 0..30 | see Tone footnote 8 | Documentation-derived | MIDI Implementation p.224 |
| `00 40` | Pitch Keyfollow | 1 / 7-bit (`0000 aaaa`) | 0..15 | see Tone footnote 9 | Documentation-derived | MIDI Implementation p.224 |
| `00 41` | Pitch Envelope Depth | 1 / 7-bit (`000a aaaa`) | 0..24 | -12..+12 | Documentation-derived | MIDI Implementation p.224 |
| `00 42` | Pitch Envelope Velocity Sens | 1 / 7-bit (`0aaa aaaa`) | 0..125 | -100..+150 | Documentation-derived | MIDI Implementation p.224 |
| `00 43` | Pitch Envelope Velocity Time1 | 1 / 7-bit (`0000 aaaa`) | 0..14 | see Tone footnote 10 | Documentation-derived | MIDI Implementation p.224 |
| `00 44` | Pitch Envelope Velocity Time4 | 1 / 7-bit (`0000 aaaa`) | 0..14 | see Tone footnote 10 | Documentation-derived | MIDI Implementation p.224 |
| `00 45` | Pitch Envelope Time Keyfollow | 1 / 7-bit (`0000 aaaa`) | 0..14 | see Tone footnote 10 | Documentation-derived | MIDI Implementation p.224 |
| `00 46` | Pitch Envelope Time 1 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 47` | Pitch Envelope Time 2 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 48` | Pitch Envelope Time 3 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 49` | Pitch Envelope Time 4 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 4A` | Pitch Envelope Level 1 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 4B` | Pitch Envelope Level 2 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 4C` | Pitch Envelope Level 3 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 4D` | Pitch Envelope Level 4 | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 4E` | Pitch LFO1 Depth | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 4F` | Pitch LFO2 Depth | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 50` | Filter Type | 1 / 7-bit (`0000 0aaa`) | 0..4 | see Tone footnote 11 | Documentation-derived | MIDI Implementation p.224 |
| `00 51` | Cutoff Frequency | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 52` | Cutoff Keyfollow | 1 / 7-bit (`0000 aaaa`) | 0..15 | see Tone footnote 9 | Documentation-derived | MIDI Implementation p.224 |
| `00 53` | Resonance | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 54` | Resonance Velocity Sens | 1 / 7-bit (`0aaa aaaa`) | 0..125 | -100..+150 | Documentation-derived | MIDI Implementation p.224 |
| `00 55` | Filter Envelope Depth | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 56` | Filter Envelope Velocity Curve | 1 / 7-bit (`0000 0aaa`) | 0..6 | 1..7 | Documentation-derived | MIDI Implementation p.224 |
| `00 57` | Filter Envelope Velocity Sens | 1 / 7-bit (`0aaa aaaa`) | 0..125 | -100..+150 | Documentation-derived | MIDI Implementation p.224 |
| `00 58` | Filter Envelope Velocity Time1 | 1 / 7-bit (`0000 aaaa`) | 0..14 | see Tone footnote 10 | Documentation-derived | MIDI Implementation p.224 |
| `00 59` | Filter Envelope Velocity Time4 | 1 / 7-bit (`0000 aaaa`) | 0..14 | see Tone footnote 10 | Documentation-derived | MIDI Implementation p.224 |
| `00 5A` | Filter Envelope Time Keyfollow | 1 / 7-bit (`0000 aaaa`) | 0..14 | see Tone footnote 10 | Documentation-derived | MIDI Implementation p.224 |
| `00 5B` | Filter Envelope Time 1 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 5C` | Filter Envelope Time 2 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 5D` | Filter Envelope Time 3 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 5E` | Filter Envelope Time 4 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 5F` | Filter Envelope Level 1 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 60` | Filter Envelope Level 2 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 61` | Filter Envelope Level 3 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 62` | Filter Envelope Level 4 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 63` | Filter LFO1 Depth | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 64` | Filter LFO2 Depth | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 65` | Tone Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 66` | Bias Direction | 1 / 7-bit (`0000 00aa`) | 0..3 | see Tone footnote 13 | Documentation-derived | MIDI Implementation p.224 |
| `00 67` | Bias Position | 1 / 7-bit (`0aaa aaaa`) | 0..127 | C-1..G9 | Documentation-derived | MIDI Implementation p.224 |
| `00 68` | Bias Level | 1 / 7-bit (`0000 aaaa`) | 0..14 | see Tone footnote 10 | Documentation-derived | MIDI Implementation p.224 |
| `00 69` | Level Envelope Velocity Curve | 1 / 7-bit (`0000 0aaa`) | 0..6 | 1..7 | Documentation-derived | MIDI Implementation p.224 |
| `00 6A` | Level Envelope Velocity Sens | 1 / 7-bit (`0aaa aaaa`) | 0..125 | -100..+150 | Documentation-derived | MIDI Implementation p.224 |
| `00 6B` | Level Envelope Velocity Time1 | 1 / 7-bit (`0000 aaaa`) | 0..14 | see Tone footnote 10 | Documentation-derived | MIDI Implementation p.224 |
| `00 6C` | Level Envelope Velocity Time4 | 1 / 7-bit (`0000 aaaa`) | 0..14 | see Tone footnote 10 | Documentation-derived | MIDI Implementation p.224 |
| `00 6D` | Level Envelope Time Keyfollow | 1 / 7-bit (`0000 aaaa`) | 0..14 | see Tone footnote 10 | Documentation-derived | MIDI Implementation p.224 |
| `00 6E` | Level Envelope Time 1 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 6F` | Level Envelope Time 2 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 70` | Level Envelope Time 3 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 71` | Level Envelope Time 4 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 72` | Level Envelope Level 1 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 73` | Level Envelope Level 2 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 74` | Level Envelope Level 3 | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 75` | Level LFO1 Depth | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 76` | Level LFO2 Depth | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.224 |
| `00 77` | Tone Pan | 1 / 7-bit (`0aaa aaaa`) | 0..127 | L64..63R | Documentation-derived | MIDI Implementation p.224 |
| `00 78` | Pan Keyfollow | 1 / 7-bit (`0000 aaaa`) | 0..14 | see Tone footnote 10 | Documentation-derived | MIDI Implementation p.224 |
| `00 79` | Random Pan Depth | 1 / 7-bit (`00aa aaaa`) | 0..63 | same as raw | Documentation-derived | MIDI Implementation p.224 |
| `00 7A` | Alternate Pan Depth | 1 / 7-bit (`0aaa aaaa`) | 1..127 | L63..63R | Documentation-derived | MIDI Implementation p.224 |
| `00 7B` | Pan LFO1 Depth | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.225 |
| `00 7C` | Pan LFO2 Depth | 1 / 7-bit (`0aaa aaaa`) | 0..126 | -63..+63 | Documentation-derived | MIDI Implementation p.225 |
| `00 7D` | Output Assign | 1 / 7-bit (`0000 00aa`) | 0..3 | see Tone footnote 12 | Documentation-derived | MIDI Implementation p.225 |
| `00 7E` | Mix/EFX Send Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.225 |
| `00 7F` | Chorus Send Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.225 |
| `01 00` | Reverb Send Level | 1 / 7-bit (`0aaa aaaa`) | 0..127 | same as raw | Documentation-derived | MIDI Implementation p.225 |

**Patch Tone total size:** `00 00 01 01` = **129 bytes** in Roland base-128 address/size notation, Documentation-derived, MIDI Implementation p.225.

### Patch Tone footnote enumerations (Roland)

The row Display values above expand or reference Roland's numbered footnotes:

1. Tone Delay Mode: `NORMAL, HOLD, PLAYMATE, CLOCK-SYNC, <TAP-SYNC>, KEY-OFF-N, KEY-OFF-D, TEMPO-SYNC`
2. Pan Control Switch: `OFF, CONTINUOUS, KEY-ON`
3. Controller Destination: `OFF, PCH, CUT, RES, LEV, PAN, MIX, CHO, REV, PL1, PL2, FL1, FL2, AL1, AL2, pL1, pL2, L1R, L2R`
4. LFO Waveform: `TRI, SIN, SAW, SQR, TRP, S&H, RND, CHS`
5. LFO Offset: `-100, -50, 0, +50, +100`
6. LFO Fade Mode: `ON-IN, ON-OUT, OFF-IN, OFF-OUT`
7. LFO External Sync: `OFF, CLOCK, <TAP>`
8. Random Pitch Depth: `0,1,2,3,4,5,6,7,8,9,10,20,30,40,50,60,70,80,90,100,200,300,400,500,600,700,800,900,1000,1100,1200`
9. Pitch/Cutoff Keyfollow: `-100,-70,-50,-30,-10,0,+10,+20,+30,+40,+50,+70,+100,+120,+150,+200`
10. Envelope velocity-time/keyfollow, Bias Level, Pan Keyfollow: `-100,-70,-50,-40,-30,-20,-10,0,+10,+20,+30,+40,+50,+70,+100`
11. Filter Type: `OFF, LPF, BPF, HPF, PKG`
12. Output Assign: `MIX, EFX, DIR, <OUTPUT-2>`
13. Bias Direction: `LOWER, UPPER, LOW&UP, ALL`

## 3. Established Patch addressing facts

| Fact | Value | Status | Source |
|---|---|---|---|
| Temporary Patch (Patch mode) base | `03 00 00 00` | Documentation-derived | `ROLAND_XP60_PROTOCOL_FACTS.md` / MIDI Implementation |
| User Patch USER:001 base / stride | `11 00 00 00` / `00 01 00 00` | Documentation-derived | `ROLAND_XP60_PROTOCOL_FACTS.md` / MIDI Implementation |
| Patch Common offset | `00 00` | Documentation-derived | MIDI Implementation p.223 |
| Tone 1 offset | `10 00` | Documentation-derived | MIDI Implementation p.223 |
| Tone 2 offset | `12 00` | Documentation-derived | MIDI Implementation p.223 |
| Tone 3 offset | `14 00` | Documentation-derived | MIDI Implementation p.223 |
| Tone 4 offset | `16 00` | Documentation-derived | MIDI Implementation p.223 |
| DT1 packet limit | 128 data bytes, >=20 ms apart | Documentation-derived | MIDI Implementation |

## 4. Golden fixture status

No physical Temporary Patch DT1 dump was supplied with this transcription. Therefore:

- the Patch Common and Patch Tone parameter tables may now be implemented as **Documentation-derived**;
- no Patch field is Hardware-verified yet;
- the first captured Temporary Patch DT1 stream should be committed as the first golden fixture;
- Phase 2 round-trip acceptance remains `encode(decode(captured_patch_bytes)) == captured_patch_bytes` byte-for-byte;
- unexplained differences must be investigated, never normalized away.
