# SR-JV80 Wave Expansion Board references

Roland's own per-board documents for the SR-JV80 series. These are the
authoritative source for expansion **waveform names and numbers**, and they are
what `docs/data/sr_jv80_expansion_waves.csv` is transcribed from.

| File | Board | Contents |
|---|---|---|
| `SR-JV80-01_Pop_Waveform_List.pdf` | SR-JV80-01 *Pop* | 154 waveforms, numbered 001–154 |
| `SR-JV80-01_Pop_Patch_List.pdf` | SR-JV80-01 *Pop* | Preset Patch and Rhythm Set names |
| `SR-JV80-02_Orchestral_Waveform_List.pdf` | SR-JV80-02 *Orchestral* | 174 waveforms, numbered 001–174 |
| `SR-JV80-02_Orchestral_Patch_List.pdf` | SR-JV80-02 *Orchestral* | Preset Patch and Rhythm Set names |

Each is stamped "© 1996 Roland Corporation U.S." and "For use with
JV-80/880/90/1000/1080, JD-990, and XP-50/80 synthesizers".

## Why these matter

Two separate things were unknown before these arrived, and they close one of
them outright.

**Which board a Wave Group ID denotes.** XP60Studio reads the ID as the SR-JV80
catalogue number. These documents let that be checked against Roland's own data
rather than against a third-party editor: every one of the 76 references to
group 1 in the golden fixture resolves to a real wave on the *Pop* board, and
the Patches named for their contents use exactly the waves those names imply —
`Clav 1 x4` uses Clav 2A, 3A, 3B and 4A; `60s Organ x4` uses 60's Organ 1, 2, 3
and 4. See `../../protocol/ROLAND_XP60_PROTOCOL_FACTS.md` §7.

**What each wave is called.** Previously nothing: a Tone pointing at an
expansion wave could only be shown as a number. Now, for the boards represented
here, XP60Studio names the wave Roland names it.

## Numbering

Roland prints wave numbers from **1**. A Tone's raw wave byte is **one less**,
which is the same relationship hardware-verified for internal waves
(`../../DEVICE_ACCEPTANCE.md` area 9). The generated catalogue stores Roland's
printed numbering and the conversion happens at the edge.

## Adding another board

1. Drop the Waveform List PDF here, named `SR-JV80-nn_<Title>_Waveform_List.pdf`.
2. Add its rows to `docs/data/sr_jv80_expansion_waves.csv` (`board,number,name,source`).
   Numbering must start at 1 and be contiguous — the generator refuses a gap,
   because a gap silently shifts every later wave.
3. Run `tools/generate_expansion_waves.py`. A CTest (`tst_expansion_waves_current`)
   fails if the generated header is stale.

Nothing else needs changing: `library::hasSrJv80WaveList` starts returning true
for that board, and every screen that names waves picks it up.

## Boards without a list here

Not an error, and not the same as a board with no waves. `hasSrJv80WaveList`
returns false, wave names are omitted rather than invented, and the UI shows the
number with the board name — "wave 3 on SR-JV80-14 Asia". Compatibility analysis
does not depend on wave names at all, so it is unaffected.

## Related

- <https://www.scribd.com/document/188644639/Roland-SR-JV80-Expansion-Board-Patches>
  — a community compilation of SR-JV80 patch lists. Recorded because it was part
  of the trail that led here; not used as a source, since Roland's own documents
  above supersede it and are not blocked from this environment.
