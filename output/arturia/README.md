# XP-60 Patch template for KeyLab Essential Mk3

`XP-60 Patch.keylabessential3` is derived from the supplied Arturia export. It
keeps the original KeyLab Essential Mk3 1.2.1 structure and changes only MIDI
CC assignments and the pad transmit channel.

## Control map

All continuous controls transmit on MIDI channel 1 with range 0-127.

| Control | MIDI message | XP-60 Patch-mode use |
|---|---:|---|
| Encoder 1 | CC74 | Filter cutoff offset |
| Encoder 2 | CC71 | Filter resonance offset |
| Encoder 3 | CC73 | Envelope attack offset |
| Encoder 4 | CC72 | Envelope release offset |
| Encoder 5 | CC5 | Portamento time |
| Encoder 6 | CC10 | Pan |
| Encoder 7 | CC1 | Modulation |
| Encoder 8 | CC11 | Expression |
| Encoder 9 | CC7 | Volume |
| Fader 1 | CC80 | Tone 1 level offset |
| Fader 2 | CC81 | Tone 2 level offset |
| Fader 3 | CC82 | Tone 3 level offset |
| Fader 4 | CC83 | Tone 4 level offset |
| Fader 5 | CC74 | Filter cutoff offset |
| Fader 6 | CC71 | Filter resonance offset |
| Fader 7 | CC73 | Envelope attack offset |
| Fader 8 | CC72 | Envelope release offset |
| Fader 9 | CC7 | Volume |
| Pads 1-16 | Notes 36-51, channel 1 | Chromatic Patch audition |

The pitch wheel, modulation wheel, sustain/expression pedal behavior, and
transport/control buttons are preserved from the source template.

## Important XP-60 behavior

CC71-74 and CC80-83 are relative sound offsets on the XP-60. A value of 64 is
neutral; values below 64 reduce and values above 64 increase the corresponding
parameter. The faders are absolute hardware controls, so put Faders 1-8 near
the middle before first moving them if you want to begin near the Patch's saved
sound. CC7 volume is absolute.

CC80-83 alter the four Tone level offset parameters; they do not toggle Tone
Switch. A Tone that is switched off in the Patch remains off.

## Load and play

1. Open Arturia MIDI Control Center and select KeyLab Essential Mk3.
2. Import `XP-60 Patch.keylabessential3` as a local template.
3. Drag it to a KeyLab User memory (this overwrites only that selected KeyLab
   User memory).
4. In Device Settings, set the KeyLab Main Channel to the XP-60 Patch receive
   channel. This template assumes channel 1 for knobs, faders, and pads.
5. Connect KeyLab MIDI OUT to XP-60 MIDI IN, select the new User program, put
   the XP-60 in Patch mode, and play.

If the XP-60 is configured to receive Patch data on another channel, either set
it to channel 1 or change every template control to the matching channel in MIDI
Control Center.

This template uses documented XP-60 MIDI reception behavior. It has not been
physically verified with this particular KeyLab/XP-60 pair yet.
