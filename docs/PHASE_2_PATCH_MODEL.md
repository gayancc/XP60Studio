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
| `PatchName` (12 printable ASCII characters, padded) | `src/xpmodel/PatchName.*` | |
| `Xp60PatchLayout` — confirmed Patch Common prefix (Patch Name 1–12), documented bases, User Patch stride, name readers; explicit `nullopt` for every size/offset still unknown | `src/xpmodel/Xp60PatchLayout.*` | table is **Partial** |
| `tools/syx_inspect.py` — research utility listing messages, checksum results, packet sizes and patch names in a `.syx` | `tools/` | Python, not production |

Tests: `tst_parameter_table`, `tst_block_codec`, `tst_memory_image`,
`tst_sysex_stream`, `tst_patch_layout`.

### Blocked on input

The Patch Common and Patch Tone parameter tables, block sizes and Tone
offsets. See `docs/protocol/XP60_PATCH_PARAMETER_MAP.md` § "Inputs still
required". Recalling them from memory would violate the no-guessing rule
(`AGENTS.md`, `ENGINEERING_PRINCIPLES.md` §5), and the Phase 1 review showed
how such recollections fail.

### Next steps once the map is available

1. Transcribe Patch Common into `XP60_PATCH_PARAMETER_MAP.md`, then into
   `Xp60PatchLayout.cpp`; mark the table `Complete`; assert the block size.
2. Same for Patch Tone; add Tone 1–4 offsets and the total Patch size.
3. Add `PatchDecoder` / `PatchEncoder` producing the strongly typed `Patch`
   from a `MemoryImage` slice and back; keep raw bytes alongside the model.
4. Golden fixtures: a Temporary Patch DT1 dump captured from the XP-60 (or a
   known-good `.syx`) must round-trip byte-for-byte; every difference is
   investigated, never normalised away.
5. Add a fetch plan (`RQ1` per block within the 128-byte packet rule) to
   `DeviceSession` so the Devices screen can read and display the current
   Patch's decoded Common block as an inspection aid.
6. Update `ROLAND_XP60_PROTOCOL_FACTS.md` and this report; only then Phase 3.
