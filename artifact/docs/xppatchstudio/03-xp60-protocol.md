# 03. XP-60 protocol (consolidated, Phase 0)

Status: the address map is now **available as a draft** (see `03a-parameter-map-draft.md`) and cross-checked from four
independent sources. Nothing here is confirmed by *this* project on a physical XP-60 yet; that gate stays with the owner.

## 0. Sources and how they rank

| Rank | Source | What it gives | Used as |
|---|---|---|---|
| 1 | Official XP-60/XP-80 owner's manual, MIDI Implementation (pp. 223 to 225 and the System, Performance and Rhythm sections) | addresses, sizes, encodings, ranges | transcribed in the legacy XP60Studio docs (`docs\protocol\*`), read-only |
| 2 | Legacy hardware log, physical XP-60, 2026-09-04 (`XP60Studio\docs\HARDWARE_VALIDATION_XP60.md`) | real transfer behaviour | **legacy-verified**, to be re-confirmed by this project |
| 3 | Real XP-60 user bank (golden fixture, 1,314 messages) | ground truth for blocks and value ranges | validation |
| 4 | Patch Base bundle: sample files and reverse-engineered code | independent confirmation, behaviours | cross-check (`05-behaviour-spec.md`) |

Status words: **manual** (printed by Roland), **legacy-verified** (observed on hardware by the old project),
**bundle** (seen in Patch Base), **project** (our own choice), **unknown**. No row is "verified" for XPPatchStudio until
you confirm it.

## 1. Message framing

```
RQ1: F0 41 dd 6A 11  a0 a1 a2 a3  s0 s1 s2 s3  cc F7
DT1: F0 41 dd 6A 12  a0 a1 a2 a3  d0 ... dn    cc F7
```

| Field | Value | Agreement |
|---|---|---|
| Manufacturer | `0x41` | manual, fixture, bundle code and samples |
| Device ID | `0x10` to `0x1F`, shown to the user as 17 to 32; factory default `0x10` | manual, fixture, bundle code (`0x10` default, configurable) |
| Model ID | `0x6A`, **one byte** | manual, fixture (all 1,314 messages), bundle samples, legacy hardware |
| Commands | RQ1 `0x11`, DT1 `0x12` | manual published example, fixture, bundle (DT1 samples; request shape in code) |
| Address / size width | 4 bytes each, 7 bits per byte | manual, bundle code |
| Wrong device ID | message ignored (shows as a timeout) | manual |

Published example (manual): `F0 41 10 6A 11 01 00 00 00 00 00 1F 19 47 F7` requests the whole temporary performance
(size `00 00 1F 19` = 3,993 address units).

## 2. Checksum

`checksum = (128 - (sum of address bytes + size or data bytes) mod 128) mod 128`; the message is valid when the sum
including the checksum is `0 mod 128`. Agreement: manual, bundle code, bundle samples (7 of 7), fixture (1,314 of 1,314).

## 3. Address arithmetic

Base-128: compact = `b3<<21 | b2<<14 | b1<<7 | b0`; add, subtract and divide on compact values; expand back to four
7-bit bytes. Agreement: manual (7-bit carry), bundle code (`05-behaviour-spec.md` B-03). Block-to-block offsets follow
this arithmetic, including padded strides (below).

## 4. Memory map (base addresses)

| Region | Base | Temporary? | Notes |
|---|---|---|---|
| System | `00 00 00 00` | no | System Common at `00 00`, Scale Tune blocks at `10 00` to `1F 00` (parts) and `20 00` (patch mode) |
| Temporary Performance | `01 00 00 00` | yes | Common at `00 00`, Parts 1 to 16 at `10 00` to `1F 00` |
| Temporary Patch, Part n | `02 nn 00 00` (nn = 00 to 0F) | yes | Part 10 doubles as the Rhythm Setup at `02 09 00 00` |
| Temporary Patch, Patch mode | `03 00 00 00` | yes | name at `00 00` (12 ASCII chars); tones at `10 00`, `12 00`, `14 00`, `16 00` |
| User Performance | `10 00 00 00` | no | USER:01 to 32, stride `00 01 00 00` |
| User Rhythm Setup | `10 40 00 00` | no | USER:1 at `10 40 00 00`, USER:2 at `10 41 00 00` |
| User Patch | `11 00 00 00` | no | USER:001 to 128, stride `00 01 00 00` |

Reading `11 nn 00 00` is **legacy-verified** (all 128 patches read from the XP-60 and re-encoded byte-exactly).
Writing there is a destructive operation; see section 8.

## 5. Block layouts and sizes

| Block | Data bytes | Params | Layout | Agreement |
|---|---|---|---|---|
| Patch Common | 73 | 72 | 12-char name, EFX, chorus, reverb, tempo, ... | manual; fixture (128 blocks of 73); bundle XP-80 sample (73) |
| Patch Tone (x4) | 129 | 128 | one block per tone; tones 0x200 apart (padded address space) | manual; fixture (512 blocks of 129); bundle XP-80 sample (129) |
| Performance Common | 66 | 65 | at `10 nn 00 00` | manual |
| Performance Part (x16) | 25 | 23 | at `10 nn 10 00` to `1F 00` (stride `0x80`) | manual |
| Rhythm Common | 12 | 12 | at `10 4n 00 00` | manual |
| Rhythm Note (x64) | 58 | 57 | at `10 4n 23 00` to `62 00` | manual |
| System Common | 96 | 95 | at `00 00 00 00` | manual; bundle XP-80 global sample is also 96 |
| Scale Tune (x17) | 12 | 12 | 16 parts plus patch mode | manual |

Total **464 parameters** in 8 block types. Encodings: 421 single 7-bit bytes, 36 ASCII bytes (names), 7 two-nibble values
(for example Patch Tempo 20 to 250, Wave Number 0 to 254). Full rows: `03a-parameter-map-draft.md`.

## 6. Cross-checks performed

| Check | Result |
|---|---|
| Spans derived from the parsed tables vs known sizes | Patch 73/129, Performance 66/25, Rhythm 12/58, System 96/12 all consistent |
| Golden bank (real XP-60, 128 patches): message checksums | **0 failures of 1,314** |
| Golden bank: block shapes | 128 common x 73 bytes, 512 tones x 129 bytes, 674 other-region messages (User Performance etc.) |
| Golden bank: every Patch Common/Tone value vs transcribed range | **74,752 values checked, 0 out of range** |
| Patch Base XP-80 samples (`xp80-voice`, `xp80-global`) | framing and checksum identical; patch 73 + 4 x 129; global 96 |
| Patch Base code vs manual | checksum, device ID default, 7-bit carry, header shape all agree |

Limits of the check: in-range values prove ranges are not *too narrow*, not that they are not too wide, and they say
nothing about what each parameter *means*. Performance, rhythm and system tables are not covered by the bank
(it contains no System data), so they rest on the manual transcription alone.

## 7. Transfer behaviour to implement

Combines the legacy hardware observations (2026-09-04) with the Patch Base findings.

| Behaviour | Value | Source |
|---|---|---|
| The XP-60 transmits whole blocks in one message | up to 129 data bytes (Patch Tone) | legacy-verified; fixture |
| Data sent **to** the XP-60 | at most 128 data bytes per DT1 (a 129-byte tone is split 128 + 1) | manual; legacy-verified (read-back equal 4 of 4) |
| Gap between successive DT1 to the XP-60 | at least 20 ms | manual; legacy-verified |
| RQ1 pipelining | **not allowed**: one request at a time, wait for the reply | legacy-verified |
| RQ1 size | an **address span**, not a payload count; padded blocks mean the reply is sparse (3,993 span returned 466 data bytes in 17 messages) | legacy-verified |
| Reply order | ascending address | legacy-verified |
| First-response timeout | 1,500 ms default (project choice, tunable) | project; both apps |
| Wire pacing option | chunk size 32 B, 3125 B/s (10.24 ms per chunk), throttle off by default | bundle code |
| Inter-message wait | 30 (ms assumed) in Patch Base for patch and bank sends | bundle code |

Consistency note: Patch Base's 10.24 ms is *wire-level chunk pacing*; the 20 ms rule is a *message gap* for the XP-60.
They are different layers. XPPatchStudio exposes both: a message gap (default 20 ms, the XP-60 minimum) and an optional
byte-rate throttle.

## 8. Memory safety (drives the UI)

- The synth sounds the **temporary area**; editing on the panel changes only that area. Power-off and selecting another
  patch, performance or rhythm set both discard it. (manual)
- A DT1 to `03 00 00 00` reaches temporary memory and cannot damage a stored sound. A DT1 to `11 nn 00 00` writes
  permanent User memory. The destructive boundary is one address byte. (manual)
- User Memory Protect on the instrument refuses writes (manual p.164: "Write Operation" for the panel, "System Exclusive Message" for incoming SysEx; a refused SysEx
  shows "User Memory Write Protected", p.189). **Hardware result 2026-11-07 (H-031):** with both settings ON the XP-60 refuses a DT1 write to User memory and the app sees
  a read-back mismatch on every block; the slot is unchanged. Two earlier attempts, made before both settings were confirmed ON, were accepted, so a mismatch is the sign of
  Protect (or a dropped write): the message should name the System Exclusive Message and Write Operation settings. Which of the two does the blocking was not separated.
- Every write to permanent memory in XPPatchStudio: explicit, addressed by the user, backed up (read first), verified
  (read back and compared), and worded differently from temporary sends.
- A selection on the panel is announced as Bank Select + Program Change on MIDI OUT unless the Tx switches are off, which
  lets the app notice that its copy of the temporary area is stale.

## 9. Open items

1. **Hardware confirmation by the owner** of everything above, in particular User-memory writes and User Memory Protect.
2. Meaning (not just range) of each parameter, units for times and rates, and EFX type-specific parameters: from the manual.
3. Wait value 30: unit and effect on the XP-60 (the 20 ms message gap is the documented floor).
4. System, Performance and Rhythm tables are not backed by a bank fixture; capture real dumps to validate them.
5. Identity Request support is unknown (candidate for a hardware session).

Binary-level findings: `05-behaviour-spec.md`. The tables are not stored as readable data in Patch Base, so the tables
here come from the manual transcription, cross-checked as above.

## External cross-checks (2026-11-04)

Independent open-source projects and Roland PDFs were compared with the project's own map. Nothing here replaces the hardware ledger; it
only shows that these facts were not mis-transcribed.

| Fact in this project | Independent source | Result |
|---|---|---|
| Manufacturer 41, Model ID 6A, RQ1 command 11, DT1 command 12, default device 10, `F0 41 dev 6A 12 aa bb cc dd data sum F7` | [sigma/integral `docs/research/jv-patch-format.md`](https://github.com/sigma/integral/blob/main/docs/research/jv-patch-format.md) (cites the JV-1080 Owner's Manual p.149); [audiocontrol-org/audiocontrol](https://github.com/audiocontrol-org/audiocontrol) `roland-jv-1080.ts` (`MODEL_ID = 0x6A`, `CMD_DT1 = 0x12`) | agrees |
| Checksum `(128 - sum mod 128) mod 128` over address and data | same JV-1080 documentation; manual example `F0 41 10 6A 11 01 00 00 00 00 00 1F 19 47 F7` is a unit test here | agrees |
| Patch Common is 73 bytes (`0x49`) for the XP-50/60/80; the JV-1080 has `0x48`, the JV-2080/XP-30 `0x4A` | sigma/integral (cites the XP-60/XP-80 Owner's Manual pp.221–224); [christofmuc/KnobKraft-orm](https://github.com/christofmuc/KnobKraft-orm) `Roland_JV1080.py` (`0x48`, "JV-2080 has 2 more bytes") | agrees (73 here) |
| Patch Tone is 129 bytes (`0x81`); Performance Common 66 (`0x42`) and Part 25 (`0x19`) for the XP-60/80; Rhythm Note 58 (`0x3A`) | sigma/integral tables P3, P4 | agrees |
| Wave Group Type 0 is the instrument's own waves, 2 is an SR-JV80 board | sigma/integral `docs/research/jv-wave-map.md`; JSynthLib `RolandJV80WaveBank` | agrees on the grouping; the board-number reading of Group ID stays in ledger H-058 |
| System Common is 96 bytes (`0x60`) | the Owner's Manual itself, p.222 (PDF p.224), page image read directly: "Total size 00 00 00 60" | **confirmed** (the open-source projects give other models: `0x28` JV-1080, `0x62` JV-2080, `0x66` JV-1010/XP-30) |

The same sources note that a reader should tell the families apart by Patch Common length, which is what a file import should do if
files from other models are ever accepted.

### Read against the Owner's Manual page by page (2026-11-04)

The Owner's Manual in the project (`artifact/docs/xp60studio-legacy/XP60-References/XP-60_80_OM.pdf`) is a scan. Its pages 222 to 225 were rendered to
images and read against `data/xp60-parameters.json`, parameter by parameter (offset, byte count, minimum, maximum, block total size), for all
eight blocks. Result: 8 block sizes agree (System Common 0x60, Scale Tune 0x0C, Performance Common 0x42, Part 0x19, Patch Common 0x49, Patch
Tone 0x0101 = 129 bytes in 7-bit counting, Rhythm Common 0x0C, Rhythm Note 0x3A); **two parameter errors were found and fixed**:

| Parameter | Map said | Manual says |
|---|---|---|
| System Common 00 5A Arpeggio Beat Pattern | 0..40 (1..41) | 0..60 (1..61), p.222 |
| Rhythm Note 00 07 Mute Group | 0..32 (OFF, 1..32) | 0..31 (OFF, 1..31), p.225; the field is `000a aaaa`, five bits |

`tests/golden/manual-verified-map.json` and `crates/xppatch-core/tests/manual_map.rs` now hold the verified offsets, sizes and ranges so the
map cannot drift from the manual unnoticed. Names and display text were read alongside but are not part of the fixture.

### Patch Group ID: Roland's table, found (2026-11-04)

The XP-60/XP-80 manual prints Patch Group ID as 1-127 (System Common, p.222) and 0-127 (Performance Part, p.223) with no ID-to-bank table; it does print the Group Type names (USER&PRESET, <PCM>, EXP) and its own Bank Select table (p.214: User 80/0, PR-A 81/0, PR-B 81/1, PR-C 81/2, GM 81/3, XP-A to XP-D 84/0-7), which agree with the table below. It has only the groups User, PR-A, PR-B, PR-C, GM; "PR-D (GM)" is the XP-30 label for GM. Roland's **XP-30 and JV-1010 Owner's Manuals** (text PDFs, MIDI Implementation, System
Common and Performance Part, next to Patch Group ID) print the table. Read from `XP-30_OM.pdf` p.190 and p.193 and `JV-1010_OM.pdf` p.74 and p.77:

| Patch group | Group Type | Group ID | Number (value) |
|---|---|---|---|
| USER | 0 | 1 | 0 to 127 (001 to 128) |
| `<CARD>` | 0 | 2 | 0 to 127 (the angle brackets mark it as not present on these models) |
| PR-A, PR-B, PR-C, PR-D (GM), PR-E | 0 | 3, 4, 5, 6, 7 | 0 to 127 |
| `<PCM>` | 1 | 1 to 127 | 0 to 127 |
| XP-A (Session), XP-B (Orch.), XP-C (Techno), XP-D, XP-E | 2 | 9, 2, 11, 1 to 127, 1 to 127 | 0 to 254 / 255 (001 to 255 / 256) |

Three independent statements agree: these two Roland manuals, the same list reported by a hardware capture on a JV-2080
([tgoelles/jv2080-max-for-live](https://github.com/tgoelles/jv2080-max-for-live)), and the project's own wave data (Clav and Organ patches resolving to the right waves with
Group ID as the board number). The earlier open point is answered: an absent bank (CARD) does **not** move the others up, because the XP-30 and JV-1010 have no card slot
and PR-A is still ID 3. The XP-60's own page is silent, so the app applies the family's table and ledger H-058 asks the XP-60 to confirm it. The Part patch picker uses
it: USER (0/1) and PR-A to PR-D (0/3 to 0/6) by name from Roland's Patch List for the fixed banks; User Patches by number only because User memory is the owner's own;
installed expansion boards (type 2, ID = board number) by number.

### Controller lists (not resolved: shown as raw numbers)

System Common footnotes 8, 10 and 13 name 64 controllers for ranges of 97 (sources, C1/C2 assign) and 104 (pedals). The manual does not print which value is which
controller. One reading fits both ranges (value = controller number 1 to 95, PITCH BEND 96, AFTERTOUCH 97, and for pedals the seven functions the footnote ends with
as 98 to 104) and an Integra/XV research note in sigma/integral says the same of the Roland list family, but that is an inference, so the app shows the raw numbers.
Ledger H-059 is the test that would turn it into a fact.
