# XP-60 golden fixtures

Real Roland SysEx used to validate the Phase 2 Patch model. Per `AGENTS.md`,
"known-good supplied SysEx files" is evidence rank 4 — above secondary
references, below a capture observed being made from the instrument.

Fixtures are **never** modified to make a test pass. If a fixture and the code
disagree, the disagreement is investigated and recorded in
`docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md`.

## `user-bank-amal.syx`

| | |
|---|---|
| Origin | Supplied by the project owner (their own XP-60 user data) on 2026-09-04 |
| SHA-256 | `13d709c210dce6403aebf60b07f23cc52feaa4ef0fb1844c926eb27fa6f09d53` |
| Size | 112 206 bytes, 1314 DT1 messages, no other MIDI, no stray bytes |
| Device ID | 17 (byte `10H`) throughout |
| Model ID | `6A` (single byte) throughout |
| Evidence rank | 4 — known-good supplied SysEx. **Not** a capture this project observed being taken, so it does not by itself promote any fact to Hardware-verified. |

Contents:

| Address range | Payload | Count | Interpretation |
|---|---|---|---|
| `10 00 00 00` … | 66 bytes | 32 | User Performance Common (one per Performance) |
| `10 xx 10 00` … `10 xx 1F 00` | 25 bytes | 512 | User Performance Parts (16 per Performance) |
| `10 xx xx xx` | 58 bytes | 128 | Performance-area blocks, **meaning not yet established** (Phase 8) |
| `10 xx xx xx` | 12 bytes | 2 | Performance-area blocks, **meaning not yet established** |
| `11 nn 00 00` | 73 bytes | 128 | Patch Common — matches the documented size `00 00 00 49` |
| `11 nn 10/12/14/16 00` | 129 bytes | 512 | Patch Tone 1–4 — matches the documented size `00 00 01 01` |

What `tst_golden_fixture` asserts against it: every message re-encodes
byte-for-byte, all 128 User Patches decode with zero issues, each patch
round-trips byte-exact through the model, and the block addresses and sizes
equal `Xp60PatchLayout::fetchPlan()`.

The Performance blocks are only counted, not decoded: the Performance
Parameter Address Map has not been transcribed (Phase 8).
