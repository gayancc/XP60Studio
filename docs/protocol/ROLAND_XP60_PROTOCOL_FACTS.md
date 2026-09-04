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
| DT1 packet size | data longer than 128 bytes is split into packets of **128 bytes or less** | Documentation-derived | The tracker does **not** assume it — it accepts any chunking and completes on range coverage. |
| Gap between successive DT1 messages | **at least 20 ms** | Documentation-derived | Applies when sending to the XP-60; also informs `betweenChunkTimeout`. |
| Sending DT1 to the device | ≤ 128 data bytes per message, ≥ 20 ms apart | Documentation-derived | `xp60::transferDefaults()`; configurable via `TransferPacing`. |
| RQ1 address/size | should use the starting addresses and sizes given in the Parameter Address Map | Documentation-derived | Arbitrary oversized reads are not a strong validation method; the Devices presets use documented blocks. |
| First-response timeout | 1500 ms (project default, not a Roland figure) | Project choice | Adjustable; tune after hardware measurements. |
| Response ordering | chunks in ascending address order | Unknown | Tracker records out-of-order arrivals as notes instead of failing. |
| Whether the XP-60 answers an RQ1 spanning several regions | Unknown | Unknown | Keep diagnostic reads inside one region. |
| Whether an RQ1 with size larger than the region is clipped or ignored | Unknown | Unknown | |
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
  Performance. Phase 2 reads them from the Parameter Address Map.

## 6. Promotion procedure

To move a row to *Hardware-verified*:

1. Perform the corresponding step in `docs/HARDWARE_VALIDATION_XP60.md`.
2. Paste the raw hex from the Protocol activity panel (expand the row) into the
   hardware log section of that document.
3. Change the status here and, when a constant is involved, change the
   `VerificationStatus` in `src/xp60/Xp60Device.cpp` in the same commit.
