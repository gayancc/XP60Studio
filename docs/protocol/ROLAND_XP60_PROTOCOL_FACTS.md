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
delivers identical bytes to identical addresses, so there is no reason to relax
it on the strength of what the device *transmits*. What the XP-60 *accepts* is
a separate question and is still untested.

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
| User Performance layout | Common 66 bytes at `10 nn 00 00`, 16 Parts of 25 bytes at `10 nn 10 00`…`10 nn 1F 00` | **Observed in the fixture only** — the Performance Address Map is not transcribed | Phase 8. Two further block shapes (58 bytes ×128, 12 bytes ×2) are present with meaning unknown. |
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

- **DT1 writes from the UI.** The write test button exists but is disabled with
  an explanation until temporary-area semantics are hardware-verified.
- **Bulk dump / handshake commands** (`WSD`, `RQD`, `DAT`, `ACK`, `EOD`, `ERR`,
  `RJC`). The XP family's normal editor path is the one-way RQ1/DT1 pair; the
  handshake commands are not modelled.
- **Identity Request** (`F0 7E dev 06 01 F7`). Whether the XP-60 answers a
  Universal Identity Request is unknown; it is a candidate for the hardware
  session because it would give a documentation-independent model check.
- **System Common size and the per-region sizes** other than Temporary
  Performance and Patch. The Patch tables live in
  `XP60_PATCH_PARAMETER_MAP.md`; Performance, Rhythm Setup and System tables
  are later phases.

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
