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

### Remaining external input — required for round-trip proof, not for code

A real **Temporary Patch DT1 dump** captured from the user's XP-60 has not
been supplied. Every round trip above is proven on synthetic blocks whose
values lie in the documented ranges; Phase 2 cannot claim real-patch
round-trip proof until a captured XP-60 Patch is committed as a golden
fixture. The Devices screen's *Fetch temporary Patch* action produces exactly
that capture (expand the IN lines in the Protocol activity panel and copy the
raw hex, or use `tools/syx_inspect.py` on a saved `.syx`).

## Known unknowns

- Whether the XP-60 answers a block-sized RQ1 (`00 00 01 01`) in exactly
  one 128-byte + one 1-byte packet; the tracker accepts any chunking.
- Whether Patch Name bytes other than printable ASCII ever occur (raw 32..127
  is accepted verbatim; 7FH is rendered as `?`).
- EFX Parameter 1–12 semantics per EFX Type (the map gives raw 0..127;
  the per-effect tables are a later phase).
- Reserved or undocumented behaviour when values outside documented ranges
  are written back (decoding keeps them; writing is not enabled in the UI).

## Next steps

1. **Hardware capture** (Phase 3 entry): connect the XP-60, *Fetch temporary
   Patch*, save the raw DT1 hex as `tests/fixtures/xp60/temporary-patch-<name>.syx`
   and add a golden test asserting byte-exact round trip and the name shown on
   the XP-60 display. Promote table rows to Hardware-verified as observed.
2. Send-side verification per Phase 3: write the decoded Patch back to the
   temporary area with `Xp60PatchCodec::encodeToDataSets` (≤ 128 bytes,
   ≥ 20 ms), re-fetch, compare.
3. Only then Phase 4 (visual Patch Editor) on top of `Xp60Patch`.
