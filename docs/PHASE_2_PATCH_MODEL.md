# Phase 2 — XP-60 Patch Model

## Objective

Decode and encode XP-60 Patch data accurately so that known-good Patch
fixtures round-trip without unexplained differences (`ROADMAP.md`, Phase 2).

## Approach

Phase 2 is **table-driven**. The XP-60 Patch is a set of Roland data blocks
(Patch Common, Tone 1–4). Each block is described by a `ParameterTable` of
`ParameterDescriptor`s taken from the Parameter Address Map, and one generic
`BlockCodec` turns bytes into values and back. The strongly typed Patch model
(`Patch`, `PatchCommon`, `Tone` with `Wave`, `Pitch`, `TVF`, `TVA`, envelopes,
`LFO`, controllers) is layered on top of the decoded values once the tables
are complete, so no offset or range is ever hand-copied twice.

Design rules carried over from Phase 1:

- Every table entry carries a `VerificationStatus`; nothing is promoted to
  hardware-verified without a capture.
- Decoding never clamps or normalises. Bytes the table does not describe are
  preserved verbatim, so `encode(decode(bytes)) == bytes` holds for every
  structurally valid block even while the table is Partial.
- Out-of-range raw values are **warnings** that keep the value; only
  structural violations (size, bit 7, nibble high bits) are errors.
- Tables declare `Complete` or `Partial`; UI and services must treat Partial
  blocks as "not fully understood".

## Status

### Implemented and tested

| Piece | Where | Notes |
|---|---|---|
| `ParameterDescriptor` / `ParameterEncoding` (7-bit, nibble ×2/×4, ASCII), `DisplayStyle` (number, pan `L64..63R`, note name `C-1..G9`), display scale/offset, `formatDisplay` | `src/xpmodel/ParameterDescriptor.*` | self-check for structural mistakes |
| `ParameterTable` with validation (overlap, out-of-block, duplicate id, order), reserved ranges, completeness | `src/xpmodel/ParameterTable.*` | |
| `BlockCodec` + `BlockValues` (decode / encode, no silent clamping, reserved bytes preserved) | `src/xpmodel/BlockCodec.*` | randomised round-trip tests |
| **Generated XP-60 tables**: Patch Common (72 rows, 73 bytes) and Patch Tone (128 rows, 129 bytes), Tone offsets `10 00`/`12 00`/`14 00`/`16 00`, enumerations from Roland's footnotes | `src/xpmodel/generated/Xp60PatchTables.*` | produced by `tools/generate_patch_tables.py` from `docs/protocol/XP60_PATCH_PARAMETER_MAP.md`; `tst_generated_tables_current` fails when stale; both tables `Complete` |
| `Xp60PatchLayout` — complete layout, five blocks, documented bases, User Patch stride, block-by-block `fetchPlan`, name readers | `src/xpmodel/Xp60PatchLayout.*` | |
| **`Xp60Patch`** — typed access via `CommonParameter` / `ToneParameter` enumerations, `PatchName`, tone switches, `WaveReference`, pitch / filter / level `Envelope` views, `summary()` | `src/xpmodel/Xp60Patch.*` | a typo in a parameter name is a compile error |
| **`Xp60PatchCodec`** — decode from a `MemoryImage` with per-block issues and precise missing-block coverage; encode to image or to DT1s of ≤ 128 bytes | `src/xpmodel/Xp60PatchCodec.*` | `encode(decode(bytes)) == bytes` byte-for-byte on every block |
| `MemoryImage`, `parseSysExStream` / `imageFromStream`, `PatchName` (raw 32..127, `displayText()` renders 7FH as `?`) | `src/xpmodel/` | |
| `DeviceSession::fetchTemporaryPatch()` — five RQ1s per the Address Map, correlation, assembly, decode, failure/cancel handling | `src/services/DeviceSession.*` | |
| Devices screen **Current Patch · inspection** card: fetch, progress, decoded name/summary, all 584 parameters with display text and raw value, decode report | `qml/XP60Studio/Screens/DevicesScreen.qml`, `presentation/PatchParameterModel.*` | read-only |
| `tools/syx_inspect.py` — research utility for `.syx` files | `tools/` | Python, not production |

Tests: `tst_parameter_table`, `tst_block_codec`, `tst_memory_image`,
`tst_sysex_stream`, `tst_patch_layout`, `tst_generated_tables`,
`tst_xp60_patch`, `tst_generated_tables_current`, plus patch-fetch coverage
in `tst_device_session`, `tst_presentation` and `tst_qml`.

### Parameter Address Map source — resolved

`docs/protocol/XP60_PATCH_PARAMETER_MAP.md` is a source-verified
transcription of the Roland XP-60/XP-80 MIDI Implementation Parameter Address
Map (manual pp.223–225): Patch Common `00 00`–`00 48` (73 bytes), Patch Tone
`00 00`–`01 00` (129 bytes), Tone offsets `10 00`, `12 00`, `14 00`, `16 00`.
The C++ tables are generated from it, never hand-copied. All entries remain
`DocumentationDerived` until verified on hardware.

Display mappings implemented from the map: `1..40` style offsets, `-63..+63`
(offset −63), `-100..+150` over raw `0..125` (scale 2), `0..-48` (scale −1),
`L64..63R` pan, `C-1..G9` note names, and every footnote enumeration.

### Golden fixture — Phase 2 success condition met

`tests/fixtures/xp60/user-bank-amal.syx` is a real XP-60 user bank supplied by
the project owner (evidence rank 4 in `AGENTS.md`). `tst_golden_fixture`
asserts against it, and all ten assertions pass:

| Assertion | Result |
|---|---|
| Stream parses with no stray bytes, aborted SysEx or rejected Roland messages | 1314 DT1, all clean |
| Every message re-encodes byte-for-byte, checksums included | 1314 / 1314 |
| Device ID and model ID | 17 and single-byte `6A` throughout |
| Block sizes and offsets equal `Xp60PatchLayout::fetchPlan()` | exact match |
| All 128 User Patches decode | 128, **zero** issues |
| Parameter values inside their transcribed ranges | 74 752 / 74 752 |
| Every patch round-trips byte-exact (`encode(decode(b)) == b`) | 128 / 128 |
| `decode(encode(patch)) == patch` | 128 / 128 |
| Transmitting a patch as DT1s reproduces its bytes | exact |
| All 128 names readable, every patch has an enabled Tone and resolvable enums | pass |

The roadmap's success condition — "known-good Patch fixtures round-trip
without unexplained differences" — is therefore met. There were no
differences to explain.

**What this does not establish.** The fixture is a supplied file, not a
capture this project observed being taken. It says nothing about how the XP-60
answers an RQ1 or accepts a DT1, and it cannot confirm what any parameter
*means*; zero range warnings proves only that no transcribed range is too
narrow. All 200 table rows remain `DocumentationDerived`. A live *Fetch
temporary Patch* against the instrument is still wanted, both to exercise the
`03 00 00 00` area and to check a decoded name against the XP-60's display.

## Known unknowns

- **129-byte DT1 payloads.** The fixture carries whole 129-byte Tone blocks in
  single messages, above the 128-byte split the MIDI Implementation states.
  Recorded as an open discrepancy in `ROLAND_XP60_PROTOCOL_FACTS.md` §2.1;
  receiving assumes nothing, sending stays conservative at 128.
- How the XP-60 itself packetises a reply to a block-sized RQ1
  (`00 00 01 01`); the tracker accepts any chunking.
- Parameter *semantics*. The fixture exercises the ranges, not the meanings.
- EFX Parameter 1–12 per EFX Type (the map gives raw 0..127; the per-effect
  tables are a later phase).
- The Performance area: the fixture shows Common 66 bytes + 16 Parts of 25
  bytes per Performance, plus two block shapes of unknown meaning (58 bytes
  ×128, 12 bytes ×2). The Performance Address Map is not transcribed — Phase 8.
- Behaviour when out-of-range values are written back (decoding keeps them;
  writing is not enabled in the UI).

## Phase 2 verdict

Complete. Every roadmap deliverable is implemented and the success condition is
met against real XP-60 data.

What is unproven is now entirely about the wire, not the tables: whether the
instrument answers an RQ1 the way the fetch plan expects, and whether it
accepts a DT1 write. Both are exactly what Phase 3 exists to establish.

## Next steps (Phase 3)

The send-and-verify engine is now built (`services::PatchTransfer`,
`xpmodel::Xp60PatchDiff`) and covered on the loopback transport. What remains
is the hardware session itself:

1. **Hardware session** per `docs/HARDWARE_VALIDATION_XP60.md`, steps 1–8a:
   FETCH → DECODE → ENCODE → SEND → FETCH AGAIN → COMPARE against the
   temporary Patch area, plus the parameter spot-checks that test *meaning*.
2. Record the payload sizes the XP-60 sends, which settles the §2.1
   129-byte discrepancy.
3. Promote table rows to `HardwareVerified` only for behaviour actually
   observed, then Phase 4 (visual Patch Editor) on top of `Xp60Patch`.
