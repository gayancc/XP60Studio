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
| `10 40/41 23 00` … `10 40/41 62 00` | 58 bytes | 128 | User Rhythm Setup Notes — 64 per Setup, matching `00 00 00 3A` |
| `10 40 00 00`, `10 41 00 00` | 12 bytes | 2 | User Rhythm Setup Common — matching `00 00 00 0C` |
| `11 nn 00 00` | 73 bytes | 128 | Patch Common — matches the documented size `00 00 00 49` |
| `11 nn 10/12/14/16 00` | 129 bytes | 512 | Patch Tone 1–4 — matches the documented size `00 00 01 01` |

What `tst_golden_fixture` asserts against it: every message re-encodes
byte-for-byte, all 128 User Patches decode with zero issues, each patch
round-trips byte-exact through the model, and the block addresses and sizes
equal `Xp60PatchLayout::fetchPlan()`.

Despite its name this file is a dump of the instrument's whole **user memory**,
not only its Patch bank: 32 User Performances, 2 User Rhythm Setups and 128 User
Patches. The Performance and Rhythm Parameter Address Maps have since been
transcribed (`docs/protocol/XP60_PERFORMANCE_PARAMETER_MAP.md`,
`XP60_RHYTHM_PARAMETER_MAP.md`), so the blocks once recorded here as "meaning not
yet established" are now identified, and all of them decode and round-trip.

There is no System data in this file. A user-memory dump carries none, which is
why `DEVICE_ACCEPTANCE.md` area 19 is what would corroborate the System tables,
and why `tst_snapshot_restore` uses this fixture to check that a restore plan
**refuses** a System restore rather than quietly restoring what it does have.
