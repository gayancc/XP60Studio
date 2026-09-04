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

### Implemented (map-independent, tested)

| Piece | Where | Notes |
|---|---|---|
| `ParameterDescriptor`, `ParameterEncoding` (7-bit, nibble ×2/×4, ASCII) | `src/xpmodel/ParameterDescriptor.*` | self-check for structural mistakes |
| `ParameterTable` with validation (overlap, out-of-block, duplicate id, order), reserved ranges, completeness | `src/xpmodel/ParameterTable.*` | |
| `BlockCodec` + `BlockValues` (decode / encode, display offsets, enum labels, text spans, no silent clamping) | `src/xpmodel/BlockCodec.*` | randomised round-trip tests |
| `MemoryImage` — sparse address-indexed image from DT1s in any order, merge/overlap/gap accounting, coverage queries | `src/xpmodel/MemoryImage.*` | |
| `parseSysExStream` / `imageFromStream` — `.syx` contents → messages with per-message diagnostics → image | `src/xpmodel/SysExStream.*` | nothing dropped silently |
| `PatchName` (12-character field, padded) | `src/xpmodel/PatchName.*` | protocol range must now be reconciled with the source transcription: Roland lists 32..127 |
| `Xp60PatchLayout` — confirmed Patch Common prefix, documented bases, User Patch stride, name readers | `src/xpmodel/Xp60PatchLayout.*` | currently Partial until the newly transcribed tables are implemented |
| `tools/syx_inspect.py` — research utility listing messages, checksum results, packet sizes and patch names in a `.syx` | `tools/` | Python, not production |

Tests: `tst_parameter_table`, `tst_block_codec`, `tst_memory_image`,
`tst_sysex_stream`, `tst_patch_layout`.

### Parameter Address Map source — resolved

`docs/protocol/XP60_PATCH_PARAMETER_MAP.md` now contains a source-verified
transcription of the Roland XP-60/XP-80 MIDI Implementation Parameter Address
Map for:

- Patch Common: every row from `00 00` through `00 48`, including the 2-byte
  nibble-encoded Patch Tempo, raw/display ranges, enums, and the documented
  total size `00 00 00 49` (73 bytes);
- Patch Tone: every row from `00 00` through `01 00`, including the 2-byte
  nibble-encoded Wave Number, raw/display ranges, Roland footnote enums, and
  the documented total size `00 00 01 01` (129 bytes);
- Patch block offsets: Common `00 00`, Tone 1 `10 00`, Tone 2 `12 00`, Tone 3
  `14 00`, Tone 4 `16 00`.

The transcription uses the XP-60/XP-80 manual's own pages 223-225. It does not
substitute JV-1080, XP-30, or recalled family values. All entries remain
`DocumentationDerived` until verified against physical hardware.

### Remaining external input — optional for implementation, required for final round-trip proof

A real **Temporary Patch DT1 dump** captured from the user's XP-60 has not yet
been supplied. The table/model implementation can proceed from the official
map, but Phase 2 cannot claim real-patch round-trip proof until a captured or
otherwise independently known-good XP-60 Patch dump is committed as a golden
fixture.

## Next steps now that the map is available

1. Transcribe the completed Patch Common rows from
   `XP60_PATCH_PARAMETER_MAP.md` into `Xp60PatchLayout.cpp`; mark the table
   `Complete`; assert block size `00 00 00 49`.
2. Do the same for Patch Tone; add Tone offsets `10 00`, `12 00`, `14 00`,
   `16 00`; assert Tone block size `00 00 01 01`.
3. Reconcile `PatchName` protocol validation with Roland's documented raw range
   `32..127` without introducing destructive normalisation.
4. Add `PatchDecoder` / `PatchEncoder` producing the strongly typed `Patch`
   from a `MemoryImage` slice and back; keep raw bytes alongside the model.
5. Golden fixture: a Temporary Patch DT1 dump captured from the XP-60 (or an
   independently known-good XP-60 `.syx`) must round-trip byte-for-byte; every
   difference is investigated, never normalised away.
6. Add a fetch plan (`RQ1` per block within the 128-byte packet rule) to
   `DeviceSession` so the Devices screen can read and display the current
   Patch's decoded Common block as an inspection aid.
7. Update `ROLAND_XP60_PROTOCOL_FACTS.md` and this report; only then Phase 3.
