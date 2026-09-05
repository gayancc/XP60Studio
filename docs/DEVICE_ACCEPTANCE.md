# Physical device acceptance — the single final pass

Every check in this project that needs a real Roland XP-60 is deferred to one
session, run once, with the instrument connected. This document is the index
for that session: it does not restate the procedures, it links each functional
area to the document that owns its steps and says what evidence closes it.

**Status: in progress. First physical session 2026-09-04** — areas 2 and 3 pass,
area 1 partly passes; everything else is still untouched. Evidence is in
[`HARDWARE_VALIDATION_XP60.md`](HARDWARE_VALIDATION_XP60.md) § Hardware log
2026-09-04. Nothing was written to the instrument: that session sent RQ1 only.

Nothing in the codebase may be promoted to `HardwareVerified` until the
relevant row below passes with recorded evidence.

## Why one pass

The user deferred physical validation to final device acceptance on 2026-09-04
and authorized continued local work. Local development therefore proceeded
past several hardware gates. The gates did not move — they accumulated here.
Running them together is deliberate: most of them need the same setup, the same
backup, and the same temporary-Patch safety discipline, and several of them
answer each other's open questions from one capture.

## Before anything is connected

| Step | Where |
|---|---|
| Record firmware, interface, driver, OS, Device ID, Rx/Tx Exclusive, commit hash | [`HARDWARE_VALIDATION_XP60.md`](HARDWARE_VALIDATION_XP60.md) § Preparation |
| Back up the instrument's User memory before any write | [`HARDWARE_VALIDATION_XP60.md`](HARDWARE_VALIDATION_XP60.md) § 8, [`PHASE_4_WAVE_BROWSER.md`](PHASE_4_WAVE_BROWSER.md) § Final physical-device acceptance |

Writes are only ever to the **temporary** Patch area (`03 00 00 00`). Permanent
User memory is not a write target anywhere in the application.

## Functional areas

| # | Functional area | What the session must establish | Owning document | Blocks | Result |
|---|---|---|---|---|---|
| 1 | **MIDI transport and connection** | Ports open; a small read returns the expected name; endpoint loss cancels without replay; wrong Device ID, swapped ports and reconnects give useful errors and no false "responding" state | [`MIDI_CONNECTIONS.md`](MIDI_CONNECTIONS.md) § Physical acceptance still required | Phase 1 | **Partly 2026-09-04**: ports open and a small read returns the expected name over a CME U2MIDI Pro. Wrong Device ID and wrong output port both time out cleanly with a useful hint and no false "responding" state. Endpoint loss mid-transfer still untested. |
| 2 | **Roland protocol foundation** | RQ1 answered; DT1 parsed; checksum valid; device/model IDs as documented; repeated reads stable | [`HARDWARE_VALIDATION_XP60.md`](HARDWARE_VALIDATION_XP60.md) steps 1–7 | Phase 1 | **Passed 2026-09-04**: RQ1 answered in 13-15 ms, DT1 parsed, checksum valid, model ID `6A` single-byte as documented, three consecutive reads byte-identical. |
| 3 | **DT1 payload size** | Whether the XP-60 sends a 129-byte Tone block in one message or splits at 128 | [`ROLAND_XP60_PROTOCOL_FACTS.md`](protocol/ROLAND_XP60_PROTOCOL_FACTS.md) § 2.1, via [`HARDWARE_VALIDATION_XP60.md`](HARDWARE_VALIDATION_XP60.md) step 7a | Open discrepancy; export chunking | **Passed 2026-09-04**: the XP-60 sends a whole 129-byte Tone block in one DT1; it does not split at 128. Settles `ROLAND_XP60_PROTOCOL_FACTS.md` §2.1. |
| 4 | **Patch model correctness** | Whole temporary Patch fetches and decodes; decoded name matches the display; parameter spot checks against the instrument's own edit pages | [`HARDWARE_VALIDATION_XP60.md`](HARDWARE_VALIDATION_XP60.md) step 7a | Phase 2 | **Software half passed 2026-09-04**: the whole Patch fetches, correlates and decodes with no structural issues (`"Childlike"`, 4 Tones, wave refs). All 584 parameters decode in range and re-encode byte-exactly across 7 Patches spanning the temporary area and all of User memory. Parameter *meaning* not yet compared against the instrument's own edit pages. |
| 5 | **Round trip** | FETCH → DECODE → ENCODE → SEND → FETCH AGAIN → COMPARE with no unexplained differences; permanent memory untouched; snapshot restores | [`HARDWARE_VALIDATION_XP60.md`](HARDWARE_VALIDATION_XP60.md) steps 8, 8a | Phase 3 | Not started |
| 6 | **Editor live behaviour** | Audible response, drag latency, note tails under Solo/Mute, A/B switching, interruption during update or read-back, recovery without queued-write replay | [`PHASE_4_LIVE_AUDITION.md`](PHASE_4_LIVE_AUDITION.md) § Verification | Phase 4 M2 | Not started |
| 7 | **Effect routing** | The routing graph matches what the instrument actually does for MIX, EFX, DIRECT, parallel sends and all three Chorus output modes | [`PHASE_4_EFFECT_ROUTING.md`](PHASE_4_EFFECT_ROUTING.md) § Verification | Phase 4 M2 | Not started |
| 8 | **EFX parameter slots** | Which Patch Common byte each algorithm's front-panel parameter occupies, and its raw-to-display conversion | [`PHASE_4_EFFECT_ROUTING.md`](PHASE_4_EFFECT_ROUTING.md) § Remaining EFX slot evidence, steps 1–6 | Phase 4 M2 close | Not started |
| 9 | **Wave bank identifiers** | Group type, group ID and zero-based number at the INT-A/INT-B boundaries; interior names against the display | [`PHASE_4_WAVE_BROWSER.md`](PHASE_4_WAVE_BROWSER.md) § Final physical-device acceptance, steps 1–3 | Phase 4 M3 close; **Use in Tone** | Not started |
| 10 | **Librarian export** | A `.syx` XP60Studio exported is accepted by the instrument and reproduces the Patches it was exported from | [`PHASE_5_LIBRARIAN.md`](PHASE_5_LIBRARIAN.md) § Remaining in this phase | Phase 5 | Not started |

Areas 8 and 9 are the two that currently block a milestone from closing; the
rest confirm behaviour that is already implemented and locally tested.

## Tooling for the session

`tools/capture_diff.py` compares a before and an after capture and names every
Patch byte that moved, resolving each address against the same transcribed
Parameter Address Map that generates the C++ tables. It accepts a binary `.syx`
or hex text copied from the Devices screen's Protocol activity panel, and
prints the DT1 payload sizes each capture used — which is the observation area
3 needs. Areas 5, 8 and 9 all produce before/after pairs and all use it:

```bash
tools/capture_diff.py before.syx after.syx --markdown
```

A changed byte is a correlation, not a proven mapping. The procedures in areas
8 and 9 are what turn one into evidence.

## Recording results

Append `## Hardware log <date>` to
[`HARDWARE_VALIDATION_XP60.md`](HARDWARE_VALIDATION_XP60.md) with the raw hex
for each step, then update:

- the status column in [`ROLAND_XP60_PROTOCOL_FACTS.md`](protocol/ROLAND_XP60_PROTOCOL_FACTS.md);
- the `VerificationStatus` values in `src/xp60/Xp60Device.cpp`;
- the milestone status in the owning phase document;
- the row above, with the date it passed.

Record differences without normalising them away. A capture that disagrees with
a document is evidence about the document, not a problem to be smoothed over —
`AGENTS.md`: never hide a mismatch to make a test pass.

## What this pass does not cover

Simulated or fixture-backed results are not evidence here. `FakeXp60`, the demo
mode's simulated instrument, and the Loopback transport exercise the software
path only; a screenshot or a passing suite that used them says nothing about
the instrument. Where a document says LIVE · VERIFIED against a simulated
device, it describes simulated read-back.


## Session log

### 2026-09-04 — first physical session (read-only)

Interface: CME U2MIDI Pro USB-MIDI cable. Device ID 17. Tool:
`xp60studio_hardware_probe` (`tests/tools/hardware_probe.cpp`), which transmits
RQ1 only and therefore cannot alter the instrument.

Closed: area 2 in full, area 3 in full, area 1 in part.

Found and fixed in this session:

- **`RolandRequestTracker` completion model was wrong against real hardware.**
  RQ1 size is an address span over *padded* blocks, not a count of payload
  bytes, so a multi-block read never reached `isComplete()` and every block
  after the first was flagged out-of-order. See
  `ROLAND_XP60_PROTOCOL_FACTS.md` §2.2. Completion is now modelled over the
  span, and the fix was re-verified against the instrument: Roland's published
  example reports `Completed, 466 bytes in 17 chunk(s)` with no notes.
  Single-block reads were never affected, which is why the local suite passed.

Still open before area 4 can close: compare a decoded Patch name and
spot-checked parameters against the XP-60's **own display**. Reads are
confirmed to reflect live edit-buffer state (the name changed when the Patch
was changed on the front panel), but no capture in this session was taken with
the display read at the same moment.