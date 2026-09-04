# Roland XP-60 Protocol Facts

This file is the single place where XP-60 protocol knowledge used by the code is
recorded together with **how well it is established**. Code must not contain a
protocol constant that is missing from this table.

Status vocabulary (mirrors `xp60::VerificationStatus`):

| Status | Meaning |
|---|---|
| **Documentation-derived** | Taken from the Roland XP-80/XP-60 MIDI Implementation (owner's manual appendix) and consistent with the JV-1080/XP family. Not yet observed on a physical XP-60 by this project. |
| **Hardware-verified** | Observed on a physical XP-60 and recorded in `docs/HARDWARE_VALIDATION_XP60.md` with a capture. |
| **Unknown** | Not established. Recorded so the gap is visible instead of guessed. |

As of Phase 1 **nothing is hardware-verified**. The development environment for
Phase 1 had no XP-60 and no network access to Roland's document library, so the
documentation-derived rows below must be cross-checked against the printed
"MIDI Implementation" chart before the hardware validation session, and each row
is promoted only with a captured message.

---

## 1. Exclusive message framing

| Fact | Value | Status | Where used |
|---|---|---|---|
| SysEx start / end | `F0` … `F7` | MIDI standard | `roland/RolandTypes.h` |
| Roland manufacturer ID | `41H` | Documentation-derived (Roland-wide) | `roland/RolandTypes.h` |
| Device ID byte range | `10H`–`1FH` | Documentation-derived | `roland/RolandDeviceId.h` |
| Device ID display mapping | display number = byte + 1 (10H ↔ "17") | Documentation-derived | `roland/RolandDeviceId.h` |
| Factory default device ID | `10H` (displayed 17) | Documentation-derived | `xp60/Xp60Device.cpp` |
| XP-60 model ID | `00H 6AH` (two bytes) | Documentation-derived | `xp60/Xp60Device.cpp` |
| Command: Data Request 1 (RQ1) | `11H` | Documentation-derived (Roland-wide) | `roland/RolandCommand.h` |
| Command: Data Set 1 (DT1) | `12H` | Documentation-derived (Roland-wide) | `roland/RolandCommand.h` |
| Address width | 4 bytes, 7 bits each (28-bit value) | Documentation-derived | `roland/RolandAddress.h` |
| Size width (RQ1) | 4 bytes, 7 bits each; value = number of data bytes | Documentation-derived | `roland/RolandSize.h` |
| Address arithmetic | carry at `80H` per byte (7-bit) | Documentation-derived | `roland/SevenBitQuad.h` |
| Checksum coverage | address bytes + (size bytes for RQ1 / data bytes for DT1) | Documentation-derived | `roland/RolandChecksum.h` |
| Checksum formula | `(128 - (sum mod 128)) mod 128` so that `(sum + checksum) mod 128 == 0` | Documentation-derived | `roland/RolandChecksum.h` |

Resulting layouts implemented by `roland/RolandCodec.cpp`:

```text
RQ1: F0 41 dev 00 6A 11 a0 a1 a2 a3 s0 s1 s2 s3 sum F7
DT1: F0 41 dev 00 6A 12 a0 a1 a2 a3 d0 ... dn    sum F7
```

Note on the model ID length: JV-1080 / XP-50 documents use the single byte
`6AH`; the XP-80/XP-60 implementation prefixes it with `00H`. Because the
length is part of the identity, the decoder is told which model IDs it may
accept and reports anything else as `UnsupportedModel` rather than guessing the
command position.

## 2. Transfer behaviour

| Fact | Value | Status | Notes |
|---|---|---|---|
| DT1 chunk size sent by the device | ≤ 256 data bytes per DT1 | Documentation-derived | Roland's family-wide rule; the tracker does **not** assume it — it accepts any chunking and completes on range coverage. |
| Gap between DT1 chunks from the device | ~20 ms | Documentation-derived | Informs `betweenChunkTimeout`, not a hard requirement. |
| Required gap when *sending* DT1 to the device | ≥ 20 ms between messages, ≤ 256 bytes each | Documentation-derived | `xp60::transferDefaults()`; configurable via `TransferPacing`. |
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
| Temporary Patch, Performance mode Part 1 | `02 00 00 00` | yes | Documentation-derived | Parts 2–16 at `02 01 00 00` … `02 0F 00 00`; Part 10 is the temporary Rhythm Set. |
| Temporary Patch, Patch mode | `03 00 00 00` | yes | Documentation-derived | Patch Common starts here; first 12 bytes are the patch name (ASCII). |
| User Performance bank | `10 00 00 00` | no | Documentation-derived | USER:01 … USER:32, stride `00 01 00 00`. |
| User Patch bank | `11 00 00 00` | no | Documentation-derived | USER:001 … USER:128, stride `00 01 00 00`. |
| User Rhythm Set(s) | — | Unknown | Not recorded until confirmed. |
| Patch Common size, Tone offsets/sizes (XP-60) | — | Unknown | Phase 2 work; JV-1080 values must not be copied without confirmation. |

## 4. Safe read presets used by the Devices screen

All presets are RQ1 (read-only). None writes to the instrument.

| Preset | Address | Size | Expected content | Status |
|---|---|---|---|---|
| Temporary Patch name | `03 00 00 00` | `00 00 00 0C` (12) | Name of the patch currently shown in Patch mode | Documentation-derived |
| User Patch USER:001 name | `11 00 00 00` | `00 00 00 0C` (12) | Name of the first User patch | Documentation-derived |
| System, first 16 bytes | `00 00 00 00` | `00 00 00 10` (16) | Opaque bytes; confirms the device answers at the System base | Documentation-derived |

## 5. Deliberately not implemented in Phase 1

- **DT1 writes from the UI.** The write test button exists but is disabled with
  an explanation until temporary-area semantics are hardware-verified.
- **Bulk dump / handshake commands** (`WSD`, `RQD`, `DAT`, `ACK`, `EOD`, `ERR`,
  `RJC`). The XP family's normal editor path is the one-way RQ1/DT1 pair; the
  handshake commands are not modelled.
- **Identity Request** (`F0 7E dev 06 01 F7`). Whether the XP-60 answers a
  Universal Identity Request is unknown; it is a candidate for the hardware
  session because it would give a documentation-independent model check.

## 6. Promotion procedure

To move a row to *Hardware-verified*:

1. Perform the corresponding step in `docs/HARDWARE_VALIDATION_XP60.md`.
2. Paste the raw hex from the Protocol activity panel (expand the row) into the
   hardware log section of that document.
3. Change the status here and, when a constant is involved, change the
   `VerificationStatus` in `src/xp60/Xp60Device.cpp` in the same commit.
