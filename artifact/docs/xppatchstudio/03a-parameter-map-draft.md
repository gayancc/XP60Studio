# 03a. XP-60 parameter map, draft (machine-checked)

Generated from the legacy XP60Studio transcription of the official XP-60/XP-80 manual (MIDI Implementation, Parameter
Address Map), re-formatted for this project. **Read-only source, no code reused.** Every value below was checked against a real
XP-60 user bank (128 patches): see `03-xp60-protocol.md` section 6. Offsets are within the block; two offset bytes use 7-bit
lanes (`hi lo` = hi x 128 + lo). Status of every row: **documentation-derived**; none is confirmed by *this* project on hardware.

## Patch: 1. Patch Common
Total: **464 parameters** in 8 blocks.


72 parameters, block span 73 bytes.

| Offset | Parameter | Bytes | Encoding | Raw range | Display |
|---|---|---|---|---|---|
| `00 00` | Patch Name 1 | 1 | ASCII | 32..127 | character |
| `00 01` | Patch Name 2 | 1 | ASCII | 32..127 | character |
| `00 02` | Patch Name 3 | 1 | ASCII | 32..127 | character |
| `00 03` | Patch Name 4 | 1 | ASCII | 32..127 | character |
| `00 04` | Patch Name 5 | 1 | ASCII | 32..127 | character |
| `00 05` | Patch Name 6 | 1 | ASCII | 32..127 | character |
| `00 06` | Patch Name 7 | 1 | ASCII | 32..127 | character |
| `00 07` | Patch Name 8 | 1 | ASCII | 32..127 | character |
| `00 08` | Patch Name 9 | 1 | ASCII | 32..127 | character |
| `00 09` | Patch Name 10 | 1 | ASCII | 32..127 | character |
| `00 0A` | Patch Name 11 | 1 | ASCII | 32..127 | character |
| `00 0B` | Patch Name 12 | 1 | ASCII | 32..127 | character |
| `00 0C` | EFX Type | 1 | 7-bit | 0..39 | 1..40 |
| `00 0D` | EFX Parameter 1 | 1 | 7-bit | 0..127 | effect-specific; map gives raw 0..127 |
| `00 0E` | EFX Parameter 2 | 1 | 7-bit | 0..127 | effect-specific; map gives raw 0..127 |
| `00 0F` | EFX Parameter 3 | 1 | 7-bit | 0..127 | effect-specific; map gives raw 0..127 |
| `00 10` | EFX Parameter 4 | 1 | 7-bit | 0..127 | effect-specific; map gives raw 0..127 |
| `00 11` | EFX Parameter 5 | 1 | 7-bit | 0..127 | effect-specific; map gives raw 0..127 |
| `00 12` | EFX Parameter 6 | 1 | 7-bit | 0..127 | effect-specific; map gives raw 0..127 |
| `00 13` | EFX Parameter 7 | 1 | 7-bit | 0..127 | effect-specific; map gives raw 0..127 |
| `00 14` | EFX Parameter 8 | 1 | 7-bit | 0..127 | effect-specific; map gives raw 0..127 |
| `00 15` | EFX Parameter 9 | 1 | 7-bit | 0..127 | effect-specific; map gives raw 0..127 |
| `00 16` | EFX Parameter 10 | 1 | 7-bit | 0..127 | effect-specific; map gives raw 0..127 |
| `00 17` | EFX Parameter 11 | 1 | 7-bit | 0..127 | effect-specific; map gives raw 0..127 |
| `00 18` | EFX Parameter 12 | 1 | 7-bit | 0..127 | effect-specific; map gives raw 0..127 |
| `00 19` | EFX Output Assign | 1 | 7-bit | 0..2 | MIX, DIR, `<OUTPUT-2>` |
| `00 1A` | EFX Mix Out Send Level | 1 | 7-bit | 0..127 | same as raw |
| `00 1B` | EFX Chorus Send Level | 1 | 7-bit | 0..127 | same as raw |
| `00 1C` | EFX Reverb Send Level | 1 | 7-bit | 0..127 | same as raw |
| `00 1D` | EFX Control Source 1 | 1 | 7-bit | 0..10 | OFF, SYS-CTRL1, SYS-CTRL2, MODULATION, BREATH, FOOT, VOLUME, PAN, EXPRESSION, PITCH BEND, AFTERTOUCH |
| `00 1E` | EFX Control Depth 1 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 1F` | EFX Control Source 2 | 1 | 7-bit | 0..10 | OFF, SYS-CTRL1, SYS-CTRL2, MODULATION, BREATH, FOOT, VOLUME, PAN, EXPRESSION, PITCH BEND, AFTERTOUCH |
| `00 20` | EFX Control Depth 2 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 21` | Chorus Level | 1 | 7-bit | 0..127 | same as raw |
| `00 22` | Chorus Rate | 1 | 7-bit | 0..127 | same as raw |
| `00 23` | Chorus Depth | 1 | 7-bit | 0..127 | same as raw |
| `00 24` | Chorus Pre-Delay | 1 | 7-bit | 0..127 | same as raw |
| `00 25` | Chorus Feedback | 1 | 7-bit | 0..127 | same as raw |
| `00 26` | Chorus Output | 1 | 7-bit | 0..2 | MIX, REVERB, MIX+REV |
| `00 27` | Reverb Type | 1 | 7-bit | 0..7 | ROOM1, ROOM2, STAGE1, STAGE2, HALL1, HALL2, DELAY, PAN-DLY |
| `00 28` | Reverb Level | 1 | 7-bit | 0..127 | same as raw |
| `00 29` | Reverb Time | 1 | 7-bit | 0..127 | same as raw |
| `00 2A` | Reverb HF Damp | 1 | 7-bit | 0..17 | 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, BYPASS |
| `00 2B` | Delay Feedback | 1 | 7-bit | 0..127 | same as raw |
| `00 2C` | Patch Tempo | 2 | nibble | 20..250 | 20..250 |
| `00 2E` | Patch Level | 1 | 7-bit | 0..127 | same as raw |
| `00 2F` | Patch Pan | 1 | 7-bit | 0..127 | L64..63R |
| `00 30` | Analog Feel | 1 | 7-bit | 0..127 | same as raw |
| `00 31` | Bend Range Up | 1 | 7-bit | 0..12 | 0..12 |
| `00 32` | Bend Range Down | 1 | 7-bit | 0..48 | 0..-48 |
| `00 33` | Key Assign Mode | 1 | 7-bit | 0..1 | POLY, SOLO |
| `00 34` | Solo Legato | 1 | 7-bit | 0..1 | OFF, ON |
| `00 35` | Portamento Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 36` | Portamento Mode | 1 | 7-bit | 0..1 | NORMAL, LEGATO |
| `00 37` | Portamento Type | 1 | 7-bit | 0..1 | RATE, TIME |
| `00 38` | Portamento Start | 1 | 7-bit | 0..1 | PITCH, NOTE |
| `00 39` | Portamento Time | 1 | 7-bit | 0..127 | same as raw |
| `00 3A` | Patch Control Source 2 | 1 | 7-bit | 0..15 | OFF, SYS-CTRL1, SYS-CTRL2, MODULATION, BREATH, FOOT, VOLUME, PAN, EXPRESSION, PITCH BEND, AFTERTOUCH, LFO1, LFO2, VELOCITY, KEYFOLLOW, PLAYMATE |
| `00 3B` | Patch Control Source 3 | 1 | 7-bit | 0..15 | OFF, SYS-CTRL1, SYS-CTRL2, MODULATION, BREATH, FOOT, VOLUME, PAN, EXPRESSION, PITCH BEND, AFTERTOUCH, LFO1, LFO2, VELOCITY, KEYFOLLOW, PLAYMATE |
| `00 3C` | EFX Control Hold/Peak | 1 | 7-bit | 0..2 | OFF, HOLD, PEAK |
| `00 3D` | Control 1 Hold/Peak | 1 | 7-bit | 0..2 | OFF, HOLD, PEAK |
| `00 3E` | Control 2 Hold/Peak | 1 | 7-bit | 0..2 | OFF, HOLD, PEAK |
| `00 3F` | Control 3 Hold/Peak | 1 | 7-bit | 0..2 | OFF, HOLD, PEAK |
| `00 40` | Velocity Range Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 41` | Octave Shift | 1 | 7-bit | 0..6 | -3..+3 |
| `00 42` | Stretch Tune Depth | 1 | 7-bit | 0..3 | OFF, 1..3 |
| `00 43` | Voice Priority | 1 | 7-bit | 0..1 | LAST, LOUDEST |
| `00 44` | Structure Type 1&2 | 1 | 7-bit | 0..9 | 1..10 |
| `00 45` | Booster 1&2 | 1 | 7-bit | 0..3 | 0, +6, +12, +18 |
| `00 46` | Structure Type 3&4 | 1 | 7-bit | 0..9 | 1..10 |
| `00 47` | Booster 3&4 | 1 | 7-bit | 0..3 | 0, +6, +12, +18 |
| `00 48` | Clock Source | 1 | 7-bit | 0..1 | PATCH, SEQUENCER |

## Patch: 2. Patch Tone

128 parameters, block span 129 bytes.

| Offset | Parameter | Bytes | Encoding | Raw range | Display |
|---|---|---|---|---|---|
| `00 00` | Tone Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 01` | Wave Group Type | 1 | 7-bit | 0..2 | INT, `<PCM>`, EXP |
| `00 02` | Wave Group ID | 1 | 7-bit | 0..127 | same as raw |
| `00 03` | Wave Number | 2 | nibble | 0..254 | 001..255 |
| `00 05` | Wave Gain | 1 | 7-bit | 0..3 | -6, 0, +6, +12 |
| `00 06` | FXM Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 07` | FXM Color | 1 | 7-bit | 0..3 | 1..4 |
| `00 08` | FXM Depth | 1 | 7-bit | 0..15 | 1..16 |
| `00 09` | Tone Delay Mode | 1 | 7-bit | 0..7 | NORMAL, HOLD, PLAYMATE, CLOCK-SYNC, `<TAP-SYNC>`, KEY-OFF-N, KEY-OFF-D, TEMPO-SYNC |
| `00 0A` | Tone Delay Time | 1 | 7-bit | 0..127 | same as raw |
| `00 0B` | Velocity Cross Fade | 1 | 7-bit | 0..127 | same as raw |
| `00 0C` | Velocity Range Lower | 1 | 7-bit | 1..127 | 1..Upper |
| `00 0D` | Velocity Range Upper | 1 | 7-bit | 1..127 | Lower..127 |
| `00 0E` | Keyboard Range Lower | 1 | 7-bit | 0..127 | C-1..Upper |
| `00 0F` | Keyboard Range Upper | 1 | 7-bit | 0..127 | Lower..G9 |
| `00 10` | Redamper Control Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 11` | Volume Control Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 12` | Hold-1 Control Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 13` | Bender Control Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 14` | Pan Control Switch | 1 | 7-bit | 0..2 | OFF, CONTINUOUS, KEY-ON |
| `00 15` | Controller 1 Destination 1 | 1 | 7-bit | 0..18 | see Tone footnote 3 |
| `00 16` | Controller 1 Depth 1 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 17` | Controller 1 Destination 2 | 1 | 7-bit | 0..18 | see Tone footnote 3 |
| `00 18` | Controller 1 Depth 2 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 19` | Controller 1 Destination 3 | 1 | 7-bit | 0..18 | see Tone footnote 3 |
| `00 1A` | Controller 1 Depth 3 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 1B` | Controller 1 Destination 4 | 1 | 7-bit | 0..18 | see Tone footnote 3 |
| `00 1C` | Controller 1 Depth 4 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 1D` | Controller 2 Destination 1 | 1 | 7-bit | 0..18 | see Tone footnote 3 |
| `00 1E` | Controller 2 Depth 1 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 1F` | Controller 2 Destination 2 | 1 | 7-bit | 0..18 | see Tone footnote 3 |
| `00 20` | Controller 2 Depth 2 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 21` | Controller 2 Destination 3 | 1 | 7-bit | 0..18 | see Tone footnote 3 |
| `00 22` | Controller 2 Depth 3 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 23` | Controller 2 Destination 4 | 1 | 7-bit | 0..18 | see Tone footnote 3 |
| `00 24` | Controller 2 Depth 4 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 25` | Controller 3 Destination 1 | 1 | 7-bit | 0..18 | see Tone footnote 3 |
| `00 26` | Controller 3 Depth 1 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 27` | Controller 3 Destination 2 | 1 | 7-bit | 0..18 | see Tone footnote 3 |
| `00 28` | Controller 3 Depth 2 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 29` | Controller 3 Destination 3 | 1 | 7-bit | 0..18 | see Tone footnote 3 |
| `00 2A` | Controller 3 Depth 3 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 2B` | Controller 3 Destination 4 | 1 | 7-bit | 0..18 | see Tone footnote 3 |
| `00 2C` | Controller 3 Depth 4 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 2D` | LFO1 Waveform | 1 | 7-bit | 0..7 | see Tone footnote 4 |
| `00 2E` | LFO1 Key Trigger | 1 | 7-bit | 0..1 | OFF, ON |
| `00 2F` | LFO1 Rate | 1 | 7-bit | 0..127 | same as raw |
| `00 30` | LFO1 Offset | 1 | 7-bit | 0..4 | see Tone footnote 5 |
| `00 31` | LFO1 Delay Time | 1 | 7-bit | 0..127 | same as raw |
| `00 32` | LFO1 Fade Mode | 1 | 7-bit | 0..3 | see Tone footnote 6 |
| `00 33` | LFO1 Fade Time | 1 | 7-bit | 0..127 | same as raw |
| `00 34` | LFO1 External Sync | 1 | 7-bit | 0..2 | see Tone footnote 7 |
| `00 35` | LFO2 Waveform | 1 | 7-bit | 0..7 | see Tone footnote 4 |
| `00 36` | LFO2 Key Trigger | 1 | 7-bit | 0..1 | OFF, ON |
| `00 37` | LFO2 Rate | 1 | 7-bit | 0..127 | same as raw |
| `00 38` | LFO2 Offset | 1 | 7-bit | 0..4 | see Tone footnote 5 |
| `00 39` | LFO2 Delay Time | 1 | 7-bit | 0..127 | same as raw |
| `00 3A` | LFO2 Fade Mode | 1 | 7-bit | 0..3 | see Tone footnote 6 |
| `00 3B` | LFO2 Fade Time | 1 | 7-bit | 0..127 | same as raw |
| `00 3C` | LFO2 External Sync | 1 | 7-bit | 0..2 | see Tone footnote 7 |
| `00 3D` | Coarse Tune | 1 | 7-bit | 0..96 | -48..+48 |
| `00 3E` | Fine Tune | 1 | 7-bit | 0..100 | -50..+50 |
| `00 3F` | Random Pitch Depth | 1 | 7-bit | 0..30 | see Tone footnote 8 |
| `00 40` | Pitch Keyfollow | 1 | 7-bit | 0..15 | see Tone footnote 9 |
| `00 41` | Pitch Envelope Depth | 1 | 7-bit | 0..24 | -12..+12 |
| `00 42` | Pitch Envelope Velocity Sens | 1 | 7-bit | 0..125 | -100..+150 |
| `00 43` | Pitch Envelope Velocity Time1 | 1 | 7-bit | 0..14 | see Tone footnote 10 |
| `00 44` | Pitch Envelope Velocity Time4 | 1 | 7-bit | 0..14 | see Tone footnote 10 |
| `00 45` | Pitch Envelope Time Keyfollow | 1 | 7-bit | 0..14 | see Tone footnote 10 |
| `00 46` | Pitch Envelope Time 1 | 1 | 7-bit | 0..127 | same as raw |
| `00 47` | Pitch Envelope Time 2 | 1 | 7-bit | 0..127 | same as raw |
| `00 48` | Pitch Envelope Time 3 | 1 | 7-bit | 0..127 | same as raw |
| `00 49` | Pitch Envelope Time 4 | 1 | 7-bit | 0..127 | same as raw |
| `00 4A` | Pitch Envelope Level 1 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 4B` | Pitch Envelope Level 2 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 4C` | Pitch Envelope Level 3 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 4D` | Pitch Envelope Level 4 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 4E` | Pitch LFO1 Depth | 1 | 7-bit | 0..126 | -63..+63 |
| `00 4F` | Pitch LFO2 Depth | 1 | 7-bit | 0..126 | -63..+63 |
| `00 50` | Filter Type | 1 | 7-bit | 0..4 | see Tone footnote 11 |
| `00 51` | Cutoff Frequency | 1 | 7-bit | 0..127 | same as raw |
| `00 52` | Cutoff Keyfollow | 1 | 7-bit | 0..15 | see Tone footnote 9 |
| `00 53` | Resonance | 1 | 7-bit | 0..127 | same as raw |
| `00 54` | Resonance Velocity Sens | 1 | 7-bit | 0..125 | -100..+150 |
| `00 55` | Filter Envelope Depth | 1 | 7-bit | 0..126 | -63..+63 |
| `00 56` | Filter Envelope Velocity Curve | 1 | 7-bit | 0..6 | 1..7 |
| `00 57` | Filter Envelope Velocity Sens | 1 | 7-bit | 0..125 | -100..+150 |
| `00 58` | Filter Envelope Velocity Time1 | 1 | 7-bit | 0..14 | see Tone footnote 10 |
| `00 59` | Filter Envelope Velocity Time4 | 1 | 7-bit | 0..14 | see Tone footnote 10 |
| `00 5A` | Filter Envelope Time Keyfollow | 1 | 7-bit | 0..14 | see Tone footnote 10 |
| `00 5B` | Filter Envelope Time 1 | 1 | 7-bit | 0..127 | same as raw |
| `00 5C` | Filter Envelope Time 2 | 1 | 7-bit | 0..127 | same as raw |
| `00 5D` | Filter Envelope Time 3 | 1 | 7-bit | 0..127 | same as raw |
| `00 5E` | Filter Envelope Time 4 | 1 | 7-bit | 0..127 | same as raw |
| `00 5F` | Filter Envelope Level 1 | 1 | 7-bit | 0..127 | same as raw |
| `00 60` | Filter Envelope Level 2 | 1 | 7-bit | 0..127 | same as raw |
| `00 61` | Filter Envelope Level 3 | 1 | 7-bit | 0..127 | same as raw |
| `00 62` | Filter Envelope Level 4 | 1 | 7-bit | 0..127 | same as raw |
| `00 63` | Filter LFO1 Depth | 1 | 7-bit | 0..126 | -63..+63 |
| `00 64` | Filter LFO2 Depth | 1 | 7-bit | 0..126 | -63..+63 |
| `00 65` | Tone Level | 1 | 7-bit | 0..127 | same as raw |
| `00 66` | Bias Direction | 1 | 7-bit | 0..3 | see Tone footnote 13 |
| `00 67` | Bias Position | 1 | 7-bit | 0..127 | C-1..G9 |
| `00 68` | Bias Level | 1 | 7-bit | 0..14 | see Tone footnote 10 |
| `00 69` | Level Envelope Velocity Curve | 1 | 7-bit | 0..6 | 1..7 |
| `00 6A` | Level Envelope Velocity Sens | 1 | 7-bit | 0..125 | -100..+150 |
| `00 6B` | Level Envelope Velocity Time1 | 1 | 7-bit | 0..14 | see Tone footnote 10 |
| `00 6C` | Level Envelope Velocity Time4 | 1 | 7-bit | 0..14 | see Tone footnote 10 |
| `00 6D` | Level Envelope Time Keyfollow | 1 | 7-bit | 0..14 | see Tone footnote 10 |
| `00 6E` | Level Envelope Time 1 | 1 | 7-bit | 0..127 | same as raw |
| `00 6F` | Level Envelope Time 2 | 1 | 7-bit | 0..127 | same as raw |
| `00 70` | Level Envelope Time 3 | 1 | 7-bit | 0..127 | same as raw |
| `00 71` | Level Envelope Time 4 | 1 | 7-bit | 0..127 | same as raw |
| `00 72` | Level Envelope Level 1 | 1 | 7-bit | 0..127 | same as raw |
| `00 73` | Level Envelope Level 2 | 1 | 7-bit | 0..127 | same as raw |
| `00 74` | Level Envelope Level 3 | 1 | 7-bit | 0..127 | same as raw |
| `00 75` | Level LFO1 Depth | 1 | 7-bit | 0..126 | -63..+63 |
| `00 76` | Level LFO2 Depth | 1 | 7-bit | 0..126 | -63..+63 |
| `00 77` | Tone Pan | 1 | 7-bit | 0..127 | L64..63R |
| `00 78` | Pan Keyfollow | 1 | 7-bit | 0..14 | see Tone footnote 10 |
| `00 79` | Random Pan Depth | 1 | 7-bit | 0..63 | same as raw |
| `00 7A` | Alternate Pan Depth | 1 | 7-bit | 1..127 | L63..63R |
| `00 7B` | Pan LFO1 Depth | 1 | 7-bit | 0..126 | -63..+63 |
| `00 7C` | Pan LFO2 Depth | 1 | 7-bit | 0..126 | -63..+63 |
| `00 7D` | Output Assign | 1 | 7-bit | 0..3 | see Tone footnote 12 |
| `00 7E` | Mix/EFX Send Level | 1 | 7-bit | 0..127 | same as raw |
| `00 7F` | Chorus Send Level | 1 | 7-bit | 0..127 | same as raw |
| `01 00` | Reverb Send Level | 1 | 7-bit | 0..127 | same as raw |

## Performance: 1. Performance Common - `1-2-1`

65 parameters, block span 66 bytes.

| Offset | Parameter | Bytes | Encoding | Raw range | Display |
|---|---|---|---|---|---|
| `00 00` | Performance Name 1 | 1 | ASCII | 32..127 | same as raw |
| `00 01` | Performance Name 2 | 1 | ASCII | 32..127 | same as raw |
| `00 02` | Performance Name 3 | 1 | ASCII | 32..127 | same as raw |
| `00 03` | Performance Name 4 | 1 | ASCII | 32..127 | same as raw |
| `00 04` | Performance Name 5 | 1 | ASCII | 32..127 | same as raw |
| `00 05` | Performance Name 6 | 1 | ASCII | 32..127 | same as raw |
| `00 06` | Performance Name 7 | 1 | ASCII | 32..127 | same as raw |
| `00 07` | Performance Name 8 | 1 | ASCII | 32..127 | same as raw |
| `00 08` | Performance Name 9 | 1 | ASCII | 32..127 | same as raw |
| `00 09` | Performance Name 10 | 1 | ASCII | 32..127 | same as raw |
| `00 0A` | Performance Name 11 | 1 | ASCII | 32..127 | same as raw |
| `00 0B` | Performance Name 12 | 1 | ASCII | 32..127 | same as raw |
| `00 0C` | EFX Source | 1 | 7-bit | 0..15 | PERFORM, 1..9, 11..16 |
| `00 0D` | EFX Type | 1 | 7-bit | 0..39 | 1..40 |
| `00 0E` | EFX Parameter 1 | 1 | 7-bit | 0..127 | same as raw |
| `00 0F` | EFX Parameter 2 | 1 | 7-bit | 0..127 | same as raw |
| `00 10` | EFX Parameter 3 | 1 | 7-bit | 0..127 | same as raw |
| `00 11` | EFX Parameter 4 | 1 | 7-bit | 0..127 | same as raw |
| `00 12` | EFX Parameter 5 | 1 | 7-bit | 0..127 | same as raw |
| `00 13` | EFX Parameter 6 | 1 | 7-bit | 0..127 | same as raw |
| `00 14` | EFX Parameter 7 | 1 | 7-bit | 0..127 | same as raw |
| `00 15` | EFX Parameter 8 | 1 | 7-bit | 0..127 | same as raw |
| `00 16` | EFX Parameter 9 | 1 | 7-bit | 0..127 | same as raw |
| `00 17` | EFX Parameter 10 | 1 | 7-bit | 0..127 | same as raw |
| `00 18` | EFX Parameter 11 | 1 | 7-bit | 0..127 | same as raw |
| `00 19` | EFX Parameter 12 | 1 | 7-bit | 0..127 | same as raw |
| `00 1A` | EFX Output Assign | 1 | 7-bit | 0..2 | MIX, DIR, `<OUTPUT-2>` |
| `00 1B` | EFX Mix Out Send Level | 1 | 7-bit | 0..127 | same as raw |
| `00 1C` | EFX Chorus Send Level | 1 | 7-bit | 0..127 | same as raw |
| `00 1D` | EFX Reverb Send Level | 1 | 7-bit | 0..127 | same as raw |
| `00 1E` | EFX Control Source 1 | 1 | 7-bit | 0..10 | OFF, SYS-CTRL1, SYS-CTRL2, MODULATION, BREATH, FOOT, VOLUME, PAN, EXPRESSION, PITCH BEND, AFTERTOUCH |
| `00 1F` | EFX Control Depth 1 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 20` | EFX Control Source 2 | 1 | 7-bit | 0..10 | OFF, SYS-CTRL1, SYS-CTRL2, MODULATION, BREATH, FOOT, VOLUME, PAN, EXPRESSION, PITCH BEND, AFTERTOUCH |
| `00 21` | EFX Control Depth 2 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 22` | Chorus Level | 1 | 7-bit | 0..127 | same as raw |
| `00 23` | Chorus Rate | 1 | 7-bit | 0..127 | same as raw |
| `00 24` | Chorus Depth | 1 | 7-bit | 0..127 | same as raw |
| `00 25` | Chorus Pre-Delay | 1 | 7-bit | 0..127 | same as raw |
| `00 26` | Chorus Feedback | 1 | 7-bit | 0..127 | same as raw |
| `00 27` | Chorus Output | 1 | 7-bit | 0..2 | MIX, REVERB, MIX+REV |
| `00 28` | Reverb Type | 1 | 7-bit | 0..7 | ROOM1, ROOM2, STAGE1, STAGE2, HALL1, HALL2, DELAY, PAN-DLY |
| `00 29` | Reverb Level | 1 | 7-bit | 0..127 | same as raw |
| `00 2A` | Reverb Time | 1 | 7-bit | 0..127 | same as raw |
| `00 2B` | Reverb HF Damp | 1 | 7-bit | 0..17 | 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, BYPASS |
| `00 2C` | Delay Feedback | 1 | 7-bit | 0..127 | same as raw |
| `00 2D` | Performance Tempo | 2 | nibble | 20..250 | same as raw |
| `00 2F` | Keyboard Range Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 30` | Voice Reserve 1 | 1 | 7-bit | 0..64 | same as raw |
| `00 31` | Voice Reserve 2 | 1 | 7-bit | 0..64 | same as raw |
| `00 32` | Voice Reserve 3 | 1 | 7-bit | 0..64 | same as raw |
| `00 33` | Voice Reserve 4 | 1 | 7-bit | 0..64 | same as raw |
| `00 34` | Voice Reserve 5 | 1 | 7-bit | 0..64 | same as raw |
| `00 35` | Voice Reserve 6 | 1 | 7-bit | 0..64 | same as raw |
| `00 36` | Voice Reserve 7 | 1 | 7-bit | 0..64 | same as raw |
| `00 37` | Voice Reserve 8 | 1 | 7-bit | 0..64 | same as raw |
| `00 38` | Voice Reserve 9 | 1 | 7-bit | 0..64 | same as raw |
| `00 39` | Voice Reserve 10 | 1 | 7-bit | 0..64 | same as raw |
| `00 3A` | Voice Reserve 11 | 1 | 7-bit | 0..64 | same as raw |
| `00 3B` | Voice Reserve 12 | 1 | 7-bit | 0..64 | same as raw |
| `00 3C` | Voice Reserve 13 | 1 | 7-bit | 0..64 | same as raw |
| `00 3D` | Voice Reserve 14 | 1 | 7-bit | 0..64 | same as raw |
| `00 3E` | Voice Reserve 15 | 1 | 7-bit | 0..64 | same as raw |
| `00 3F` | Voice Reserve 16 | 1 | 7-bit | 0..64 | same as raw |
| `00 40` | Keyboard Mode | 1 | 7-bit | 0..1 | LAYER, SINGLE |
| `00 41` | Clock Source | 1 | 7-bit | 0..1 | PERFORMANCE, SEQUENCER |

## Performance: 2. Performance Part - `1-2-2`

23 parameters, block span 25 bytes.

| Offset | Parameter | Bytes | Encoding | Raw range | Display |
|---|---|---|---|---|---|
| `00 00` | Receive Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 01` | MIDI Channel | 1 | 7-bit | 0..15 | 1..16 |
| `00 02` | Patch Group Type | 1 | 7-bit | 0..2 | USER&PRESET, `<PCM>`, EXP |
| `00 03` | Patch Group ID | 1 | 7-bit | 0..127 | same as raw |
| `00 04` | Patch Number | 2 | nibble | 0..254 | 001..255 |
| `00 06` | Part Level | 1 | 7-bit | 0..127 | same as raw |
| `00 07` | Part Pan | 1 | 7-bit | 0..127 | L64..63R |
| `00 08` | Part Coarse Tune | 1 | 7-bit | 0..96 | -48..+48 |
| `00 09` | Part Fine Tune | 1 | 7-bit | 0..100 | -50..+50 |
| `00 0A` | Output Assign | 1 | 7-bit | 0..4 | MIX, EFX, DIR, `<OUTPUT-2>`, PAT |
| `00 0B` | Mix/EFX Send Level | 1 | 7-bit | 0..127 | same as raw |
| `00 0C` | Chorus Send Level | 1 | 7-bit | 0..127 | same as raw |
| `00 0D` | Reverb Send Level | 1 | 7-bit | 0..127 | same as raw |
| `00 0E` | Receive Program Change Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 0F` | Receive Volume Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 10` | Receive Hold-1 Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 11` | Keyboard Range Lower | 1 | 7-bit | 0..127 | C-1..Upper |
| `00 12` | Keyboard Range Upper | 1 | 7-bit | 0..127 | Lower..G9 |
| `00 13` | Octave Shift | 1 | 7-bit | 0..6 | -3..+3 |
| `00 14` | Local Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 15` | Transmit Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 16` | Transmit Bank Select Group | 1 | 7-bit | 0..7 | PATCH, GROUP1, GROUP2, GROUP3, GROUP4, GROUP5, GROUP6, GROUP7 |
| `00 17` | Transmit Volume | 2 | nibble | 0..128 | 0..127, OFF |

## Rhythm: 1. Rhythm Common - `1-4-1`

12 parameters, block span 12 bytes.

| Offset | Parameter | Bytes | Encoding | Raw range | Display |
|---|---|---|---|---|---|
| `00 00` | Rhythm Name 1 | 1 | ASCII | 32..127 | same as raw |
| `00 01` | Rhythm Name 2 | 1 | ASCII | 32..127 | same as raw |
| `00 02` | Rhythm Name 3 | 1 | ASCII | 32..127 | same as raw |
| `00 03` | Rhythm Name 4 | 1 | ASCII | 32..127 | same as raw |
| `00 04` | Rhythm Name 5 | 1 | ASCII | 32..127 | same as raw |
| `00 05` | Rhythm Name 6 | 1 | ASCII | 32..127 | same as raw |
| `00 06` | Rhythm Name 7 | 1 | ASCII | 32..127 | same as raw |
| `00 07` | Rhythm Name 8 | 1 | ASCII | 32..127 | same as raw |
| `00 08` | Rhythm Name 9 | 1 | ASCII | 32..127 | same as raw |
| `00 09` | Rhythm Name 10 | 1 | ASCII | 32..127 | same as raw |
| `00 0A` | Rhythm Name 11 | 1 | ASCII | 32..127 | same as raw |
| `00 0B` | Rhythm Name 12 | 1 | ASCII | 32..127 | same as raw |

## Rhythm: 2. Rhythm Note - `1-4-2`

57 parameters, block span 58 bytes.

| Offset | Parameter | Bytes | Encoding | Raw range | Display |
|---|---|---|---|---|---|
| `00 00` | Tone Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 01` | Wave Group Type | 1 | 7-bit | 0..2 | INT, `<PCM>`, EXP |
| `00 02` | Wave Group ID | 1 | 7-bit | 0..127 | same as raw |
| `00 03` | Wave Number | 2 | nibble | 0..254 | 001..255 |
| `00 05` | Wave Gain | 1 | 7-bit | 0..3 | -6, 0, +6, +12 |
| `00 06` | Bend Range | 1 | 7-bit | 0..12 | same as raw |
| `00 07` | Mute Group | 1 | 7-bit | 0..31 | OFF, 1..31 |
| `00 08` | Envelope Mode | 1 | 7-bit | 0..1 | NO-SUS, SUSTAIN |
| `00 09` | Volume Control Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 0A` | Hold-1 Control Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 0B` | Pan Control Switch | 1 | 7-bit | 0..2 | OFF, CONTINUOUS, KEY-ON |
| `00 0C` | Source Key | 1 | 7-bit | 0..127 | C-1..G9 |
| `00 0D` | Fine Tune | 1 | 7-bit | 0..100 | -50..+50 |
| `00 0E` | Random Pitch Depth | 1 | 7-bit | 0..30 | 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200 |
| `00 0F` | Pitch Envelope Depth | 1 | 7-bit | 0..24 | -12..+12 |
| `00 10` | Pitch Envelope Velocity Sens | 1 | 7-bit | 0..125 | -100..+150 |
| `00 11` | Pitch Envelope Velocity Time | 1 | 7-bit | 0..14 | -100, -70, -50, -40, -30, -20, -10, 0, +10, +20, +30, +40, +50, +70, +100 |
| `00 12` | Pitch Envelope Time 1 | 1 | 7-bit | 0..127 | same as raw |
| `00 13` | Pitch Envelope Time 2 | 1 | 7-bit | 0..127 | same as raw |
| `00 14` | Pitch Envelope Time 3 | 1 | 7-bit | 0..127 | same as raw |
| `00 15` | Pitch Envelope Time 4 | 1 | 7-bit | 0..127 | same as raw |
| `00 16` | Pitch Envelope Level 1 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 17` | Pitch Envelope Level 2 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 18` | Pitch Envelope Level 3 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 19` | Pitch Envelope Level 4 | 1 | 7-bit | 0..126 | -63..+63 |
| `00 1A` | Filter Type | 1 | 7-bit | 0..4 | OFF, LPF, BPF, HPF, PKG |
| `00 1B` | Cutoff Frequency | 1 | 7-bit | 0..127 | same as raw |
| `00 1C` | Resonance | 1 | 7-bit | 0..127 | same as raw |
| `00 1D` | Resonance Velocity Sens | 1 | 7-bit | 0..125 | -100..+150 |
| `00 1E` | Filter Envelope Depth | 1 | 7-bit | 0..126 | -63..+63 |
| `00 1F` | Filter Envelope Velocity Sens | 1 | 7-bit | 0..125 | -100..+150 |
| `00 20` | Filter Envelope Velocity Time | 1 | 7-bit | 0..14 | -100, -70, -50, -40, -30, -20, -10, 0, +10, +20, +30, +40, +50, +70, +100 |
| `00 21` | Filter Envelope Time 1 | 1 | 7-bit | 0..127 | same as raw |
| `00 22` | Filter Envelope Time 2 | 1 | 7-bit | 0..127 | same as raw |
| `00 23` | Filter Envelope Time 3 | 1 | 7-bit | 0..127 | same as raw |
| `00 24` | Filter Envelope Time 4 | 1 | 7-bit | 0..127 | same as raw |
| `00 25` | Filter Envelope Level 1 | 1 | 7-bit | 0..127 | same as raw |
| `00 26` | Filter Envelope Level 2 | 1 | 7-bit | 0..127 | same as raw |
| `00 27` | Filter Envelope Level 3 | 1 | 7-bit | 0..127 | same as raw |
| `00 28` | Filter Envelope Level 4 | 1 | 7-bit | 0..127 | same as raw |
| `00 29` | Tone Level | 1 | 7-bit | 0..127 | same as raw |
| `00 2A` | Level Envelope Velocity Sens | 1 | 7-bit | 0..125 | -100..+150 |
| `00 2B` | Level Envelope Velocity Time | 1 | 7-bit | 0..14 | -100, -70, -50, -40, -30, -20, -10, 0, +10, +20, +30, +40, +50, +70, +100 |
| `00 2C` | Level Envelope Time 1 | 1 | 7-bit | 0..127 | same as raw |
| `00 2D` | Level Envelope Time 2 | 1 | 7-bit | 0..127 | same as raw |
| `00 2E` | Level Envelope Time 3 | 1 | 7-bit | 0..127 | same as raw |
| `00 2F` | Level Envelope Time 4 | 1 | 7-bit | 0..127 | same as raw |
| `00 30` | Level Envelope Level 1 | 1 | 7-bit | 0..127 | same as raw |
| `00 31` | Level Envelope Level 2 | 1 | 7-bit | 0..127 | same as raw |
| `00 32` | Level Envelope Level 3 | 1 | 7-bit | 0..127 | same as raw |
| `00 33` | Tone Pan | 1 | 7-bit | 0..127 | L64..63R |
| `00 34` | Random Pan Depth | 1 | 7-bit | 0..63 | same as raw |
| `00 35` | Alternate Pan Depth | 1 | 7-bit | 1..127 | L63..63R |
| `00 36` | Output Assign | 1 | 7-bit | 0..3 | MIX, EFX, DIR, `<OUTPUT-2>` |
| `00 37` | Mix/EFX Send Level | 1 | 7-bit | 0..127 | same as raw |
| `00 38` | Chorus Send Level | 1 | 7-bit | 0..127 | same as raw |
| `00 39` | Reverb Send Level | 1 | 7-bit | 0..127 | same as raw |

## System: 1. System Common - `1-1-1`

95 parameters, block span 96 bytes.

| Offset | Parameter | Bytes | Encoding | Raw range | Display |
|---|---|---|---|---|---|
| `00 00` | Sound Mode | 1 | 7-bit | 0..2 | PERFORMANCE, PATCH, GM |
| `00 01` | Performance Number | 1 | 7-bit | 0..127 | same as raw |
| `00 02` | Patch Group Type | 1 | 7-bit | 0..2 | USER&PRESET, `<PCM>`, EXP |
| `00 03` | Patch Group ID | 1 | 7-bit | 1..127 | same as raw |
| `00 04` | Patch Number | 2 | nibble | 0..254 | 001..255 |
| `00 06` | Master Tune | 1 | 7-bit | 0..126 | same as raw |
| `00 07` | Scale Tune Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 08` | EFX Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 09` | Chorus Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 0A` | Reverb Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 0B` | Patch Remain | 1 | 7-bit | 0..1 | OFF, ON |
| `00 0C` | Clock Source | 1 | 7-bit | 0..1 | same as raw |
| `00 0D` | TAP Control Source | 1 | 7-bit | 0..4 | same as raw |
| `00 0E` | Hold Control Source | 1 | 7-bit | 0..4 | OFF, HOLD-1, SOSTENUTO, SOFT, HOLD-2 |
| `00 0F` | Peak Control Source | 1 | 7-bit | 0..4 | OFF, HOLD-1, SOSTENUTO, SOFT, HOLD-2 |
| `00 10` | Volume Control Source | 1 | 7-bit | 0..1 | VOLUME, VOL&EXP |
| `00 11` | Aftertouch Source | 1 | 7-bit | 0..2 | CHANNEL, POLY, CH&POLY |
| `00 12` | System Control Source 1 | 1 | 7-bit | 1..97 | same as raw |
| `00 13` | System Control Source 2 | 1 | 7-bit | 1..97 | same as raw |
| `00 14` | Receive Program Change | 1 | 7-bit | 0..1 | OFF, ON |
| `00 15` | Receive Bank Select | 1 | 7-bit | 0..1 | OFF, ON |
| `00 16` | Receive Control Change | 1 | 7-bit | 0..1 | OFF, ON |
| `00 17` | Receive Modulation | 1 | 7-bit | 0..1 | OFF, ON |
| `00 18` | Receive Volume | 1 | 7-bit | 0..1 | OFF, ON |
| `00 19` | Receive Hold-1 | 1 | 7-bit | 0..1 | OFF, ON |
| `00 1A` | Receive Bender | 1 | 7-bit | 0..1 | OFF, ON |
| `00 1B` | Receive Aftertouch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 1C` | Control Channel | 1 | 7-bit | 0..16 | 1..16, OFF |
| `00 1D` | Patch Receive Channel | 1 | 7-bit | 0..15 | 1..16 |
| `00 1E` | Rhythm Edit Source | 1 | 7-bit | 0..1 | OFF, ON |
| `00 1F` | Preview Sound Mode | 1 | 7-bit | 0..1 | SINGLE, CHORD |
| `00 20` | Preview Note Set 1 | 1 | 7-bit | 0..127 | C-1..G9 |
| `00 21` | Preview Velocity Set 1 | 1 | 7-bit | 0..127 | same as raw |
| `00 22` | Preview Note Set 2 | 1 | 7-bit | 0..127 | C-1..G9 |
| `00 23` | Preview Velocity Set 2 | 1 | 7-bit | 0..127 | same as raw |
| `00 24` | Preview Note Set 3 | 1 | 7-bit | 0..127 | C-1..G9 |
| `00 25` | Preview Velocity Set 3 | 1 | 7-bit | 0..127 | same as raw |
| `00 26` | Preview Note Set 4 | 1 | 7-bit | 0..127 | C-1..G9 |
| `00 27` | Preview Velocity Set 4 | 1 | 7-bit | 0..127 | same as raw |
| `00 28` | Transmit Program Change | 1 | 7-bit | 0..1 | OFF, ON |
| `00 29` | Transmit Bank Select | 1 | 7-bit | 0..1 | OFF, ON |
| `00 2A` | Patch Transmit Channel | 1 | 7-bit | 0..17 | same as raw |
| `00 2B` | Transpose Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 2C` | Transpose Value | 1 | 7-bit | 0..11 | -5..+6 |
| `00 2D` | Octave Shift | 1 | 7-bit | 0..6 | -3..+3 |
| `00 2E` | Keyboard Velocity | 1 | 7-bit | 0..127 | same as raw |
| `00 2F` | Keyboard Sens | 1 | 7-bit | 0..2 | LIGHT, STANDARD, HEAVY |
| `00 30` | Aftertouch Sens | 1 | 7-bit | 0..100 | same as raw |
| `00 31` | Pedal1 Assign | 1 | 7-bit | 1..104 | same as raw |
| `00 32` | Pedal1 Output Mode | 1 | 7-bit | 0..3 | OFF, INT, MIDI, INT&MIDI |
| `00 33` | Pedal1 Polarity | 1 | 7-bit | 0..1 | STANDARD, REVERSE |
| `00 34` | Pedal2 Assign | 1 | 7-bit | 1..104 | same as raw |
| `00 35` | Pedal2 Output Mode | 1 | 7-bit | 0..3 | OFF, INT, MIDI, INT&MIDI |
| `00 36` | Pedal2 Polarity | 1 | 7-bit | 0..1 | STANDARD, REVERSE |
| `00 37` | C1 Assign | 1 | 7-bit | 1..97 | same as raw |
| `00 38` | C1 Output Mode | 1 | 7-bit | 0..3 | OFF, INT, MIDI, INT&MIDI |
| `00 39` | C2 Assign | 1 | 7-bit | 1..97 | same as raw |
| `00 3A` | C2 Output Mode | 1 | 7-bit | 0..3 | OFF, INT, MIDI, INT&MIDI |
| `00 3B` | Hold Pedal Output Mode | 1 | 7-bit | 0..3 | OFF, INT, MIDI, INT&MIDI |
| `00 3C` | Hold Pedal Polarity | 1 | 7-bit | 0..1 | STANDARD, REVERSE |
| `00 3D` | Bank Select Group1 Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 3E` | Bank Select Group1 MSB | 1 | 7-bit | 0..127 | same as raw |
| `00 3F` | Bank Select Group1 LSB | 1 | 7-bit | 0..127 | same as raw |
| `00 40` | Bank Select Group2 Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 41` | Bank Select Group2 MSB | 1 | 7-bit | 0..127 | same as raw |
| `00 42` | Bank Select Group2 LSB | 1 | 7-bit | 0..127 | same as raw |
| `00 43` | Bank Select Group3 Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 44` | Bank Select Group3 MSB | 1 | 7-bit | 0..127 | same as raw |
| `00 45` | Bank Select Group3 LSB | 1 | 7-bit | 0..127 | same as raw |
| `00 46` | Bank Select Group4 Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 47` | Bank Select Group4 MSB | 1 | 7-bit | 0..127 | same as raw |
| `00 48` | Bank Select Group4 LSB | 1 | 7-bit | 0..127 | same as raw |
| `00 49` | Bank Select Group5 Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 4A` | Bank Select Group5 MSB | 1 | 7-bit | 0..127 | same as raw |
| `00 4B` | Bank Select Group5 LSB | 1 | 7-bit | 0..127 | same as raw |
| `00 4C` | Bank Select Group6 Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 4D` | Bank Select Group6 MSB | 1 | 7-bit | 0..127 | same as raw |
| `00 4E` | Bank Select Group6 LSB | 1 | 7-bit | 0..127 | same as raw |
| `00 4F` | Bank Select Group7 Switch | 1 | 7-bit | 0..1 | OFF, ON |
| `00 50` | Bank Select Group7 MSB | 1 | 7-bit | 0..127 | same as raw |
| `00 51` | Bank Select Group7 LSB | 1 | 7-bit | 0..127 | same as raw |
| `00 52` | Pedal3 Assign | 1 | 7-bit | 1..104 | same as raw |
| `00 53` | Pedal3 Output Mode | 1 | 7-bit | 0..3 | OFF, INT, MIDI, INT&MIDI |
| `00 54` | Pedal3 Polarity | 1 | 7-bit | 0..1 | STANDARD, REVERSE |
| `00 55` | Pedal4 Assign | 1 | 7-bit | 1..104 | same as raw |
| `00 56` | Pedal4 Output Mode | 1 | 7-bit | 0..3 | OFF, INT, MIDI, INT&MIDI |
| `00 57` | Pedal4 Polarity | 1 | 7-bit | 0..1 | STANDARD, REVERSE |
| `00 58` | Arpeggio Style | 1 | 7-bit | 0..32 | 1..33 |
| `00 59` | Arpeggio Motif | 1 | 7-bit | 0..33 | 1..34 |
| `00 5A` | Arpeggio Beat Pattern | 1 | 7-bit | 0..60 | 1..61 |
| `00 5B` | Arpeggio Accent Rate | 1 | 7-bit | 0..100 | same as raw |
| `00 5C` | Arpeggio Shuffle Rate | 1 | 7-bit | 50..90 | same as raw |
| `00 5D` | Arpeggio Keyboard Velocity | 1 | 7-bit | 0..127 | same as raw |
| `00 5E` | Arpeggio Octave Range | 1 | 7-bit | 0..6 | -3..+3 |
| `00 5F` | Arpeggio Part Number | 1 | 7-bit | 0..15 | PART1, PART2, PART3, PART4, PART5, PART6, PART7, PART8, PART9, PART10, PART11, PART12, PART13, PART14, PART15, PART16 |

## System: 2. Scale Tune - `1-1-2`

12 parameters, block span 12 bytes.

| Offset | Parameter | Bytes | Encoding | Raw range | Display |
|---|---|---|---|---|---|
| `00 00` | Scale Tune for C | 1 | 7-bit | 0..127 | -64..+63 |
| `00 01` | Scale Tune for C# | 1 | 7-bit | 0..127 | -64..+63 |
| `00 02` | Scale Tune for D | 1 | 7-bit | 0..127 | -64..+63 |
| `00 03` | Scale Tune for D# | 1 | 7-bit | 0..127 | -64..+63 |
| `00 04` | Scale Tune for E | 1 | 7-bit | 0..127 | -64..+63 |
| `00 05` | Scale Tune for F | 1 | 7-bit | 0..127 | -64..+63 |
| `00 06` | Scale Tune for F# | 1 | 7-bit | 0..127 | -64..+63 |
| `00 07` | Scale Tune for G | 1 | 7-bit | 0..127 | -64..+63 |
| `00 08` | Scale Tune for G# | 1 | 7-bit | 0..127 | -64..+63 |
| `00 09` | Scale Tune for A | 1 | 7-bit | 0..127 | -64..+63 |
| `00 0A` | Scale Tune for A# | 1 | 7-bit | 0..127 | -64..+63 |
| `00 0B` | Scale Tune for B | 1 | 7-bit | 0..127 | -64..+63 |

