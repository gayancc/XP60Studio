# Roland XP-60 Protocol Facts

This file is the single place where XP-60 protocol knowledge used by the code is
recorded together with **how well it is established**. Code must not contain a
protocol constant that is missing from this table.

Status vocabulary (mirrors `xp60::VerificationStatus`):

| Status | Meaning |
|---|---|
| **Documentation-derived** | Taken from the Roland XP-60/XP-80 MIDI Implementation (owner's manual appendix). Not yet observed on a physical XP-60 by this project. |
| **Project-defined** | A project choice (for example a partial diagnostic read size) that Roland does not document as such. Safe to use, never presented as a Roland fact. |
| **Hardware-verified** | Observed on a physical XP-60 and recorded in `docs/HARDWARE_VALIDATION_XP60.md` with a capture. |
| **Unknown** | Not established. Recorded so the gap is visible instead of guessed. |

As of Phase 1 **nothing is hardware-verified**. The development environment for
Phase 1 had no XP-60 and no network access to Roland's document library. The
rows below were cross-checked against the XP-60/XP-80 MIDI Implementation by the
project owner during the review of PR #5; that review corrected two facts that
had been recalled wrongly (model ID `00 6A` → `6A`, packet limit 256 → 128
bytes). Each row is promoted to hardware-verified only with a captured message.

---

## 1. Exclusive message framing

| Fact | Value | Status | Where used |
|---|---|---|---|
| SysEx start / end | `F0` … `F7` | MIDI standard | `roland/RolandTypes.h` |
| Roland manufacturer ID | `41H` | Documentation-derived (Roland-wide) | `roland/RolandTypes.h` |
| Device ID byte range | `10H`–`1FH` | Documentation-derived | `roland/RolandDeviceId.h` |
| Device ID display mapping | display number = byte + 1 (10H ↔ "17") | Documentation-derived | `roland/RolandDeviceId.h` |
| Factory default device ID | `10H` (displayed 17) | Documentation-derived | `xp60/Xp60Device.cpp` |
| XP-60 model ID | `6AH` (one byte) | Documentation-derived (cross-checked in PR #5 review) | `xp60/Xp60Device.cpp` |
| Command: Data Request 1 (RQ1) | `11H` | Documentation-derived (Roland-wide) | `roland/RolandCommand.h` |
| Command: Data Set 1 (DT1) | `12H` | Documentation-derived (Roland-wide) | `roland/RolandCommand.h` |
| Address width | 4 bytes, 7 bits each (28-bit value) | Documentation-derived | `roland/RolandAddress.h` |
| Size width (RQ1) | 4 bytes, 7 bits each; value = number of data bytes | Documentation-derived | `roland/RolandSize.h` |
| Address arithmetic | carry at `80H` per byte (7-bit) | Documentation-derived | `roland/SevenBitQuad.h` |
| Checksum coverage | address bytes + (size bytes for RQ1 / data bytes for DT1) | Documentation-derived | `roland/RolandChecksum.h` |
| Checksum formula | `(128 - (sum mod 128)) mod 128` so that `(sum + checksum) mod 128 == 0` | Documentation-derived | `roland/RolandChecksum.h` |

Resulting layouts implemented by `roland/RolandCodec.cpp`:

```text
RQ1: F0 41 dev 6A 11 a0 a1 a2 a3 s0 s1 s2 s3 sum F7
DT1: F0 41 dev 6A 12 a0 a1 a2 a3 d0 ... dn    sum F7
```

Roland-published examples from the XP-60/XP-80 MIDI Implementation, used as
golden fixtures in `tests/cpp/tst_roland_codec.cpp` and
`tests/cpp/tst_roland_checksum.cpp`:

```text
RQ1 (Temporary Performance, size 00 00 1F 19 = 3993 bytes):
  F0 41 10 6A 11 01 00 00 00 00 00 1F 19 47 F7
DT1 (one byte 06 at 01 00 00 28):
  F0 41 10 6A 12 01 00 00 28 06 51 F7
```

Note on the model ID length: the XP-60/XP-80 share the single-byte `6AH` with
the JV-1080 family; later Roland models (XV series) use two-byte IDs such as
`00H 10H`. Because the length is part of the identity, the decoder is told which
model IDs it may accept and reports anything else as `UnsupportedModel` rather
than guessing the command position. An earlier draft of this project recorded
`00H 6AH` for the XP-60; that was wrong and was corrected against the manual.

## 2. Transfer behaviour

| Fact | Value | Status | Notes |
|---|---|---|---|
| DT1 packet size **sent by the XP-60** | whole blocks in one message, observed up to **129 bytes** | **Hardware-verified 2026-09-04** | Settled in §2.1. The 128-byte rule governs data sent *to* the instrument. |
| Gap between successive DT1 messages | **at least 20 ms** (XP-60 itself sends ~37-70 ms apart) | Documentation-derived; device-side gap hardware-observed 2026-09-04 | Applies when sending to the XP-60; also informs `betweenChunkTimeout`. |
| Sending DT1 to the device | ≤ 128 data bytes per message, ≥ 20 ms apart | Documentation-derived | `xp60::transferDefaults()`; configurable via `TransferPacing`. |
| Requests may not be pipelined | send one RQ1 at a time and wait for its reply | **Hardware-verified 2026-09-04** | See §2.3. Requests arriving while the instrument is transmitting a reply are dropped. |
| RQ1 address/size | should use the starting addresses and sizes given in the Parameter Address Map | Documentation-derived | Arbitrary oversized reads are not a strong validation method; the Devices presets use documented blocks. |

### 2.1 Settled — the XP-60 sends 129-byte DT1 payloads

The MIDI Implementation states that data longer than 128 bytes is split into
packets of 128 bytes or less. The golden fixture
`tests/fixtures/xp60/user-bank-amal.syx` (a real XP-60 user bank) contains
**512 DT1 messages whose payload is 129 bytes** — each one a whole Patch Tone
block (`00 00 01 01`) in a single message. Its Patch Common messages are 73
bytes, below the limit.

**Settled on hardware, 2026-09-04.** A physical XP-60 answering an RQ1 for the
temporary Patch (`03 00 00 00`) replied with five DT1 messages: 73 bytes of
Patch Common, then **129 bytes for each of the four Tone blocks**, each in a
single message. Interpretation 1 is correct: the 128-byte rule governs data
sent **to** the XP-60; the instrument itself transmits whole blocks. The golden
fixture `tests/fixtures/xp60/user-bank-amal.syx` is faithful to real device
output. Raw capture in
[`HARDWARE_VALIDATION_XP60.md`](../HARDWARE_VALIDATION_XP60.md) § Hardware log
2026-09-04.

**Engineering stance, unchanged.** Receiving assumes nothing about chunking.
Sending keeps the conservative 128-byte split
(`Xp60PatchCodec::encodeToDataSets`): splitting a 129-byte block into 128 + 1
delivers identical bytes to identical addresses.

**What the XP-60 accepts is now also verified (2026-09-04).** An exported `.syx`
carrying each Tone block as 128 + 1 was sent to the instrument and the Patch
read back equalled the one exported, four times out of four. The asymmetry is
real and both halves are hardware-verified: the XP-60 transmits a whole
129-byte block in one message and accepts the same block split at 128.

### 2.2 RQ1 size is an address span, not a payload byte count

Roland block addresses are **padded**: a Patch Tone block occupies 129 bytes but
the next Tone begins 0x200 address units later; a Performance Part holds 25
bytes with the next beginning 0x80 later. An RQ1 whose size spans several
blocks is therefore answered with only the populated blocks inside that span,
and the payload received is smaller than the size requested.

Observed 2026-09-04 with Roland's own published example
(`F0 41 10 6A 11 01 00 00 00 00 00 1F 19 47 F7`, nominally 3993): 17 DT1
replies totalling **466 payload bytes**, covering `01 00 00 00` plus the 16
Performance Parts at `01 00 10 00`..`01 00 1F 00`, ending exactly at the
requested span's end. Nothing was clipped; the gaps simply hold no data.

**Consequence for `RolandRequestTracker`.** It currently treats
`request.size().value()` as a count of payload bytes and tracks a contiguous
byte-coverage array, so a multi-block read never completes and each block after
the first is flagged as out-of-order. Completion must be modelled in address
space over padded blocks. Recorded in the hardware log; not yet fixed.
| First-response timeout | 1500 ms (project default, not a Roland figure) | Project choice | Adjustable; tune after hardware measurements. |
| Response ordering | chunks in ascending address order | **Hardware-verified 2026-09-04** | Observed across 17 consecutive Performance replies. |
| Whether the XP-60 answers an RQ1 spanning several regions | yes, returning only the populated blocks in the span | **Hardware-verified 2026-09-04** | See §2.2: the reply is sparse, so payload bytes < size requested. |
| Whether an RQ1 with size larger than the region is clipped or ignored | answered, covering the span exactly, without padding the gaps | **Hardware-verified 2026-09-04** | Roland's own 3993 example returns 466 payload bytes. |
| Device behaviour on a wrong device ID | Message ignored | Documentation-derived (Roland-wide) | Diagnostics will show a timeout. |

## 2.4 Memory architecture — what a write to each region actually does

Transcribed from the Roland XP-60/XP-80 Owner's Manual for the patch
synchronization work; see [`../PATCH_SYNCHRONIZATION.md`](../PATCH_SYNCHRONIZATION.md)
§1 for the quoted text and the OCR method. The manual pages are scanned images,
so the address column was cross-checked against §3 below and the golden fixture.

| Fact | Value | Status | Source |
|---|---|---|---|
| Memory types | temporary, rewritable (System + User), non-rewritable (Preset, Wave Expansion) | Documentation-derived | OM p.45 |
| What the instrument sounds | the **temporary area**, not USER memory | Documentation-derived | OM p.45 |
| What editing on the instrument changes | the temporary area only | Documentation-derived | OM p.45 |
| What destroys the temporary area | power-off **and selecting another Patch/Performance/Rhythm Set** | Documentation-derived | OM p.45 |
| Keeping an edit | requires an explicit Write into rewritable memory | Documentation-derived | OM p.45, p.46 |
| Write procedure on the instrument | `[UTILITY]` → `1 Write` → choose destination number → `[F6] Execute` | Documentation-derived | OM p.46 |
| User Memory Protect | an instrument-side setting that refuses writes while ON | Documentation-derived | OM p.46 |
| USER memory capacity | 32 Performances, 128 Patches, 2 Rhythm Sets | Documentation-derived | OM p.45 |
| Panel Patch selection transmits | Bank Select + Program Change, unless the Tx switches are OFF | Documentation-derived | OM p.218-219 |

**Consequence, and the safety rule the application is built on.** A DT1 to
`03 00 00 00` reaches temporary memory: it cannot damage a stored sound, and the
instrument discards it on the next patch change. A DT1 to `11 nn 00 00` reaches
permanent USER Patch memory and is destructive. The destructive/non-destructive
boundary is therefore **one address byte**, which is why `PatchTransfer`
hard-codes the temporary address rather than gating a button.

Because selecting a Patch on the panel both destroys the temporary area and is
announced on MIDI OUT, `DeviceSession::patchSelectionObserved` watches for those
two messages: it is a documented, passive, zero-traffic way to learn that the
application's copy of the temporary area has become worthless.

### The write direction into USER memory

**Reads from `11 nn 00 00` are hardware-verified**: all 128 User Patches were
read from those addresses on a physical XP-60, every parameter in range and
every Patch re-encoding byte-exactly (`HARDWARE_VALIDATION_XP60.md`,
`--verify-bank`). The addresses are right and the region is live.

The **write** direction is Documentation-derived: DT1 is documented as the
message "used when you wish to set the data of the receiving device", Roland
documents no read-only regions, and `.syx` bank files in the wild — including
this project's golden fixture — are exactly these addresses and exist to be sent
back. It is not yet confirmed by this project on hardware, and neither is how
**User Memory Protect** (OM p.46) interacts with it.

That is a reason to **prove every write at runtime**, not to withhold the
feature. `services::UserMemoryWrite` reads each destination before writing it,
reads it back afterwards and compares byte for byte. A write the instrument
refuses — which is what User Memory Protect being ON looks like from the wire —
surfaces as a mismatch naming that cause, never as success. Area 11 of
`DEVICE_ACCEPTANCE.md` promotes the row when the connected session runs it.

## 3. Address map (base addresses only)

Sizes of whole regions are deliberately **not** encoded until a captured DT1
confirms them.

| Region | Base address | Temporary? | Status | Notes |
|---|---|---|---|---|
| System | `00 00 00 00` | no | Documentation-derived | |
| Temporary Performance | `01 00 00 00` | yes | Documentation-derived | |
| Temporary Patch, Performance mode Part 1 | `02 00 00 00` | yes | Documentation-derived | Parts 2–16 at `02 01 00 00` … `02 0F 00 00`. |
| Temporary Rhythm Setup | `02 09 00 00` | yes | Documentation-derived | Performance mode Part 10 (Roland terminology: Rhythm Setup). |
| Temporary Patch, Patch mode | `03 00 00 00` | yes | Documentation-derived | Patch Common starts here; Patch Name 1–12 at offset `00 00` (12 ASCII characters). |
| User Performance bank | `10 00 00 00` | no | Documentation-derived | USER:01 … USER:32, stride `00 01 00 00`. |
| User Rhythm Setup | `10 40 00 00` | no | Documentation-derived | USER:1 at `10 40 00 00`, USER:2 at `10 41 00 00`. |
| User Patch bank | `11 00 00 00` | no | Documentation-derived | USER:001 … USER:128, stride `00 01 00 00`. |
| Temporary Performance size | `00 00 1F 19` (3993 bytes) | Documentation-derived | From Roland's published RQ1 example. |
| Patch Common size | `00 00 00 49` (73 bytes) | Documentation-derived, corroborated by the golden fixture | Parameter Address Map p.224; 128 blocks of exactly 73 bytes in `user-bank-amal.syx`. |
| Patch Tone size | `00 00 01 01` (129 bytes) | Documentation-derived, corroborated by the golden fixture | Parameter Address Map p.225; 512 blocks of exactly 129 bytes in the fixture. |
| Tone 1–4 offsets within a Patch | `10 00`, `12 00`, `14 00`, `16 00` | Documentation-derived, corroborated by the golden fixture | Parameter Address Map p.223; every patch in the fixture uses exactly these four offsets. |
| User Performance layout | Common 66 bytes at `10 nn 00 00`, 16 Parts of 25 bytes at `10 nn 10 00`…`10 nn 1F 00` | Documentation-derived | Parameter Address Map §1-2, §1-2-1, §1-2-2, transcribed in [`XP60_PERFORMANCE_PARAMETER_MAP.md`](XP60_PERFORMANCE_PARAMETER_MAP.md). Roland's total sizes (`00 00 00 42`, `00 00 00 19`) match the fixture's block sizes exactly, and all 32 Performances and 512 Parts in it obey every declared range. |
| User Rhythm Setup layout | Common 12 bytes at `10 4n 00 00`, 64 Notes of 58 bytes at `10 4n 23 00`…`10 4n 62 00` | Documentation-derived | Parameter Address Map §1-4, transcribed in [`XP60_RHYTHM_PARAMETER_MAP.md`](XP60_RHYTHM_PARAMETER_MAP.md). Roland's totals (`00 00 00 0C`, `00 00 00 3A`) match the fixture's block sizes, and a Note's offset is its MIDI note number: Key# 35 at `23 00` because 0x23 is 35. |
| Patch Common size, Tone offsets/sizes (XP-60) | — | Unknown | Phase 2 work; JV-1080 values must not be copied without confirmation. |

## 4. Safe read presets used by the Devices screen

All presets are RQ1 (read-only). None writes to the instrument.

| Preset | Address | Size | Expected content | Status |
|---|---|---|---|---|
| Temporary Patch name | `03 00 00 00` | `00 00 00 0C` (12) | Name of the patch currently shown in Patch mode (Patch Name 1–12) | Documentation-derived |
| User Patch USER:001 name | `11 00 00 00` | `00 00 00 0C` (12) | Name of the first User patch | Documentation-derived |
| Temporary Performance (Roland RQ1 example) | `01 00 00 00` | `00 00 1F 19` (3993) | The whole Temporary Performance, delivered as DT1 packets of ≤ 128 bytes ≥ 20 ms apart | Documentation-derived |
| System, first 16 bytes | `00 00 00 00` | `00 00 00 10` (16) | Opaque bytes; confirms the device answers at the System base. The base address is documented, the 16-byte partial size is a project choice. | **Project-defined** |

## 5. Deliberately not implemented in Phase 1

- ~~**DT1 writes from the UI.**~~ **Shipped.** Temporary-area writes are
  hardware-verified (2026-09-04, 6/6 with read-back). Writes into permanent USER
  memory ship through `services::UserMemoryWrite`, which reads each destination
  before writing it, verifies every write by read-back, and is armed separately
  from the audition path. See §2.4.
- **Bulk dump / handshake commands** (`WSD`, `RQD`, `DAT`, `ACK`, `EOD`, `ERR`,
  `RJC`). The XP family's normal editor path is the one-way RQ1/DT1 pair; the
  handshake commands are not modelled.
- **Identity Request** (`F0 7E dev 06 01 F7`). Whether the XP-60 answers a
  Universal Identity Request is unknown; it is a candidate for the hardware
  session because it would give a documentation-independent model check.
- **The System tables.** Patch, Performance and Rhythm Setup are transcribed
  (`XP60_PATCH_PARAMETER_MAP.md`, `XP60_PERFORMANCE_PARAMETER_MAP.md`,
  `XP60_RHYTHM_PARAMETER_MAP.md`). System Common (§1-1) and its Scale Tune
  (§1-1-2) are the last untranscribed tables; their total sizes read from the
  same appendix are `00 00 00 60` and `00 00 00 0C`.

## 5.1 What the golden fixture corroborates

`tests/fixtures/xp60/user-bank-amal.syx` is a real XP-60 user bank supplied by
the project owner (evidence rank 4 in `AGENTS.md`: known-good supplied SysEx).
`tst_golden_fixture` proves against it that:

- all 1314 messages are Roland DT1 with device ID 17 and the **single-byte
  model ID `6A`**, and every one re-encodes byte-for-byte (checksums included);
- Patch Common is 73 bytes and Patch Tone 129 bytes, at offsets `00 00`,
  `10 00`, `12 00`, `14 00`, `16 00` — identical to `Xp60PatchLayout::fetchPlan`;
- all 128 User Patches decode with **zero** range warnings, i.e. all 74 752
  parameter values lie inside the ranges transcribed from the Address Map;
- every patch round-trips byte-exact through the typed model.

What it does **not** establish:

- **the semantics of any parameter.** No range warnings proves a range is not
  too narrow; it cannot prove a range is not too wide, nor that a parameter
  means what Roland's name suggests;
- **anything about the wire.** The file is not a capture this project observed
  being taken, so it says nothing about how the XP-60 answers an RQ1 or
  accepts a DT1. Those remain the object of Phase 3;
- consequently **no row is promoted to Hardware-verified.** All 200 stay
  `DocumentationDerived`.

## 6. Promotion procedure

To move a row to *Hardware-verified*:

1. Perform the corresponding step in `docs/HARDWARE_VALIDATION_XP60.md`.
2. Paste the raw hex from the Protocol activity panel (expand the row) into the
   hardware log section of that document.
3. Change the status here and, when a constant is involved, change the
   `VerificationStatus` in `src/xp60/Xp60Device.cpp` in the same commit.

## 7. Wave Expansion Boards — what a Wave Group ID means

| Fact | Value | Status | Source |
|---|---|---|---|
| Wave Group Type | 0 = INT, 1 = `<PCM>` (JV-1080 only, ignored on receive), 2 = EXP | Documentation-derived | Parameter Address Map p.224 |
| Wave Group ID field | one 7-bit byte, 0..127, display "same as raw" | Documentation-derived | Parameter Address Map p.224 |
| Group type 0, group ID 1/2 | INT-A / INT-B | **Hardware-verified 2026-09-04** | Front-panel comparison, area 9 |
| **Which board a Wave Group ID denotes** | the SR-JV80 board of that number | **Corroborated** | independent implementation + fixture wave names; see below |
| Group type 1 (`<PCM>`), group ID | the SO-PCM1 card of that number | **Corroborated** | JV PatchEd. wave-ID list |
| Expansion slots | four, EXP-A..EXP-D, one SR-JV80 board each | Documentation-derived | Owner's Manual p.45 |
| Panel selection of expansion waves | by slot group XP-A..XP-D | Documentation-derived | Owner's Manual p.45 |

### "Group ID is the SR-JV80 board number" — adopted as an inference

**Correction (2026-09-05).** An earlier revision of this document refused this
mapping, on the stated ground that the golden fixture uses group **97** and "the
SR-JV80 series is numbered in the low tens, so 97 cannot be a board number".

**That ground was false.** The SR-JV80 series is numbered **01–19 *and* 96–99**;
the high numbers are the Japanese-market and compilation boards:

| Number | Board |
|---|---|
| 96 | *World Collection: Latin* |
| 97 | *Experience III* |
| 98 | *Experience II* |
| 99 | *Experience* |

So SR-JV80-97 exists, the pattern does not fail on the fifth group, and the
refusal rested on a mistake rather than on evidence.

#### The evidence as it actually stands

The fixture `user-bank-amal.syx` carries **192 expansion references among its 512
Tones**, across groups **1, 5, 7, 14 and 97**. Every one of those is a real
SR-JV80 board number, and the patch names sit where the boards' contents predict:

| Group | Board of that number | Patches in the fixture using it |
|---|---|---|
| 1 | -01 *Pop* | `60s Organ x4`, `Accordian 2`, `Clarinet mp`, `Clav 1 x4`, `Dulcimer`, `Flute ALL` |
| 5 | -05 *World* | `Cimbalom`, `Ethno Pipes3`, `Deepawali`, `Shnika`, `TAMIL*TONE`, `Theri Meri` |
| 7 | -07 *Super Sound Set* | `Bandoneon1`, `Brass Fall 1..3`, `Bright TP`, `Flute live`, `Musette det2` |
| 14 | -14 *Asia* | `Sitar` |
| 97 | -97 *Experience III* | `*Poly Xpandr`, `*PromarsLead`, `*Tenor Solo` |

Group 5's contents are unambiguously world instruments and group 14's single
Patch is a sitar. Group 97's three Patches name Roland vintage synths (the
*Promars* is one), and *Experience III* is a compilation whose sources include
*Vintage Synth*. The 0..127 width of the field — rather than the 0..19 the
low-numbered boards alone would need — is itself explained by 96–99 existing.

Nothing observed contradicts the mapping, and four independent name/theme
matches support it.

#### Corroboration from an independent implementation

**JV PatchEd. — JV-XP** (Leandro Cleto, a freeware Ctrlr panel for the JV-1010,
JV-1080, JV-2080 and XP-30/50/60/80, <https://sourceforge.net/projects/jv-patched-jv-xp/>)
sets its Wave ID control from this list, which in Ctrlr's `uiComboContent`
gives `LABEL=value` — the value actually sent:

```
POP=1  ORCHESTRAL=2  PIANO=3  VITAGE SYNTH=4  WORLD=5  DANCE=6
SUPER SOUND SET=7  KEYBOARD 60&70=8  SESSION=9  BASS & DRUMS=10
TECHNO=11  HIP HOP=12  VOCAL=13  ASIA=14  EFX=15  ORCHESTRAL II=16
COUNTRY=17  LATIN=18  HOUSE=19  EXPERIENCE I=99  EXPERIENCE II=98
EXPERIENCE III=97
```

So that editor sends **14 for Asia and 97 for Experience III** — the board
number, exactly as XP60Studio reads it. Its equivalent lists for the other two
group types agree with what this document already records: `INTERNAL A=1`,
`INTERNAL B=2` for group type 0 (hardware-verified here independently), and the
SO-PCM1 card number for group type 1.

*(Its Lua branches on a zero-based combo **index** — `waveIdValue==13` for Asia —
which is an internal UI detail of that panel and not the transmitted value. It is
worth naming because it is an easy thing to misread as a contradiction.)*

#### Corroboration from Roland's own Waveform Lists

Roland's per-board Waveform Lists for SR-JV80-01 *Pop* and SR-JV80-02
*Orchestral* are held at `docs/XP60-References/SR-JV80/`. They make the fixture
checkable against Roland's own data, wave by wave, with no third-party source
involved. All **76** of its references to group 1 resolve to a real wave on the
*Pop* board's 154, and the Patch names give the game away:

| Patch in the fixture | Group | Raw wave | SR-JV80-01 *Pop* wave |
|---|---|---|---|
| `Clav 1 x4` | 1 | 16, 19, 20, 22 | 017 `Clav 2A`, 020 `Clav 3A`, 021 `Clav 3B`, 023 `Clav 4A` |
| `60s Organ x4` | 1 | 32, 33, 34, 35 | 033–036 `60's Organ 1`…`4` |
| `Whistle` | 1 | 108 | 109 `Whistle 1` |
| `Raya Shaku` | 1 | 106 | 107 `Shakuhachi` |
| `Turbo Tenor` | 1 | 102 | 103 `Tenor Sax mf` |

A Patch called `Clav 1 x4` using four Clav waves, and one called `60s Organ x4`
using the four numbered organ waves, is not something a wrong mapping produces.
This also confirms the wave numbering: Roland prints from 1 and the Tone carries
one less, the same relationship hardware-verified for internal waves (area 9).

`tst_expansion_compatibility` checks every fixture reference to a board whose
list is held, so this cannot silently rot.

#### Earlier corroboration from the fixture's own wave numbers

Before Roland's lists were to hand, the same check was run against the wave
names carried in the JV PatchEd. panel. Resolving each of the fixture's 192
expansion references as *(board = group ID, wave = wave number)*:

| Patch in the fixture | Group | Wave | Resolves to |
|---|---|---|---|
| `*Tenor Solo` | 97 | 4 | SR-JV80-97 `*Tenor Solo` |
| `*Poly Xpandr` | 97 | 25 | SR-JV80-97 `*OBXP Str` (OB-Xpander) |
| `Cimbalom` | 5 | 11 | SR-JV80-05 `HmrDulcimer` |
| `Clav 1 x4` | 1 | 16 | SR-JV80-01 `Clav 2A` |

**All 192 resolve to a wave that exists on the board of that number**, and the
names match the Patches using them — `*Tenor Solo` exactly, a cimbalom to a
hammered dulcimer, a Clav patch to a Clav wave. Under the rival reading (group ID
as a zero-based board index) 5 of the 192 do not resolve at all, and the rest
land on nonsense: `Cimbalom` on `Ragga`, `Clav 1 x4` on `Cb Sect Lp`.

#### What this project does with it, and what it still will not do

The mapping is **not documented by Roland** — the Parameter Address Map defines
the field's width and nothing about its meaning — and this project has still not
confirmed it against hardware itself. It is used for **naming and convenience**,
never as authority:

- `library::srJv80BoardName` turns a group into a board name, so XP60Studio can
  say "wave group 14 — SR-JV80-14 Asia" instead of a bare number, and so the
  Expansion Manager can offer a list of real boards to pick from.
- Picking a board from that list fills in its wave group. The field stays
  editable, and **Learn** — reading the group out of a Patch fetched from the
  musician's own instrument — still overrides it. Evidence from the instrument
  outranks the inference.
- **What is installed is still only ever what the musician declared.** The
  mapping names what a Patch is asking for; it never concludes that an
  instrument has a board. `library::PatchCompatibility` still answers **Unknown**
  rather than "missing" whenever the profile is not complete enough for
  "missing" to be true.

Confirming it first-hand is still a hardware task: install a known board, select
one of its waves in a Tone from the front panel, and read the Tone back —
`DEVICE_ACCEPTANCE.md` area 15. Until then a musician who finds a board that
answers to a different number can simply say so, and XP60Studio will believe
them over its own table.

One gap in the corroboration: **SR-JV80-96** (*World Collection: Latin*, a rare
Japanese-market board) is a real product but does not appear in JV PatchEd.'s
list, so its group ID is assumed rather than corroborated. XP60Studio lists it;
nothing depends on it being right.

## Keybed

| Fact | Value | Status | Source |
|---|---|---|---|
| Keys | 61, C2..C7 (MIDI 36..96) | Documentation-derived | Roland XP-60 product specification |
| Velocity | yes, with channel aftertouch | Documentation-derived | Roland XP-60 product specification |

This is the instrument's physical keyboard, not a limit on the Patch Key Range
parameters, which the Parameter Address Map defines over the whole MIDI note
range (0..127, `C-1..G9`). The editor uses it to draw a keyboard at a readable
size and to say when a range falls outside what the XP-60's own keys can play;
it never clamps a value. Implemented as `xp60::keybed()`.


### 2.3 The XP-60 does not answer pipelined requests

Five block RQ1s queued together and spaced only by `interMessageDelay` cost the
last block's reply: the instrument drops requests that arrive while it is still
transmitting. Measured on a USB-MIDI cable, three runs per value, the whole
Patch fetch failed at 20, 25 and 30 ms and succeeded from 33 ms upward.

The cliff follows the wire rate rather than any Roland figure. A 129-byte Tone
block is 140 bytes of SysEx; at the MIDI DIN rate of 31250 baud that is roughly
45 ms of transmission, matching the 53 ms request-to-reply latency measured on
every Tone block.

`DeviceSession::fetchPatch` therefore issues block reads **serially**, waiting
for each reply before sending the next, instead of relying on a delay. The safe
delay is a property of the link — a different figure would apply over Bluetooth
— while waiting for the reply is correct on any link.
