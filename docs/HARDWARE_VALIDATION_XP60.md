# XP-60 Hardware Validation Procedure (Phase 1)

Purpose: prove, on a physical Roland XP-60, that the Phase 1 protocol
foundation sends valid Roland SysEx, receives and validates the reply, and
correlates it with the request. Nothing in this procedure writes to the
instrument unless step 8 is explicitly reached and its precondition is met.

The application build used must be recorded (git commit hash), and every
observation is captured from the **Protocol activity** panel of the Devices
screen (click a row to expand it and copy the raw hex).

## Preparation

Execution note (2026-09-04): physical validation is deferred to the final device
acceptance pass at the user's direction. Include Phase 3 round-trip checks,
`PHASE_4_LIVE_AUDITION.md`, `PHASE_4_EFFECT_ROUTING.md` and the bank-boundary
capture checklist in `PHASE_4_WAVE_BROWSER.md`. Local development may continue;
unperformed hardware checks remain open.

| Item | Value to record |
|---|---|
| XP-60 firmware version (Utility → Information, or power-on display) | |
| MIDI interface (USB/DIN/wireless), driver, OS | |
| XP-60 System → MIDI → Device ID (default 17) | |
| XP-60 System → MIDI → Rx Exclusive / Tx Exclusive (must be ON) | |
| XP60Studio commit hash | |

Connect the interface's MIDI OUT to XP-60 MIDI IN and XP-60 MIDI OUT to the
interface's MIDI IN. Put the XP-60 in **Patch mode** and select any patch whose
name you can read on its display (write the name down).

## Steps

Each step lists the action, the expected observation, and what to record.

### 1. MIDI IN/OUT open

- Devices → choose MIDI IN and MIDI OUT for the interface → **Connect**.
- Expect: header indicator reads **XP-60 LIVE**; card pill reads *Connected*;
  the log shows `SYS Connected. MIDI IN: …  MIDI OUT: …`.
- Record: both endpoint names exactly as displayed.

### 2. Device answers a safe RQ1

- Preset **Temporary Patch name (12 bytes)** → **Send request**.
- Expect within 1.5 s: an `OUT Roland RQ1 device=17 address=03 00 00 00 size=00 00 00 0C`
  line followed by an `IN Roland DT1 device=17 address=03 00 00 00 bytes=12 checksum=OK req=#n`
  line, and the request card turns **Completed**.
- If instead the request **Times out**: check Rx Exclusive, the device ID, the
  cabling direction, then retry once. Record which change fixed it.
- Record: raw hex of both lines.

### 3. DT1 parsed successfully

- Same reply as step 2. Expect the request card to show 12 / 12 bytes in 1
  chunk and `Text: <patch name>`.
- Record: the text shown versus the name on the XP-60 display. They must match
  (trailing spaces are part of the name).

### 4. Checksum validates

- Same reply. Expect `checksum OK` pill on the IN line and *Checksum errors* = 0.
- Record: the checksum byte from the raw hex (second-to-last byte).

### 5. Device / model IDs match

- Same reply. Expect `device=17` (or the configured ID) and the raw hex to
  contain `41 <dev> 6A 12` immediately after `F0` (single-byte model ID `6A`,
  as in the XP-60/XP-80 MIDI Implementation).
- If the XP-60 instead replies with a **two-byte** model ID (`41 <dev> 00 6A 12`),
  the log will show `Roland SysEx rejected: UnsupportedModel`. Record the raw
  hex — this would contradict the manual and must be fed back into
  `docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md` before anything else changes.
- Record: the exact bytes between `F0` and the address.

### 6. Roland-published RQ1 example: address, size and packet behaviour

Roland documents this exact request in the MIDI Implementation, so model ID,
address, size and checksum are all independently verifiable:

```text
F0 41 10 6A 11 01 00 00 00 00 00 1F 19 47 F7
```

- Preset **Temporary Performance (Roland RQ1 example, 3993 bytes)** → Send.
  With device ID 17 the OUT line's raw hex must be byte-for-byte the packet
  above.
- Expect the reply as **several DT1 packets, each carrying at most 128 data
  bytes**, arriving **at least 20 ms apart** in ascending address order
  starting at `01 00 00 00`, until the request card reaches 3993 / 3993 bytes
  and turns **Completed** with no *notes*.
- If the request times out mid-way, or packets are larger than 128 bytes, or
  arrive out of order (the card shows notes), record the exact packet sizes,
  addresses and log timestamps — these observations set the real
  `TransferPacing` defaults.
- Also send preset **User Patch USER:001 name**. Expect address `11 00 00 00`,
  12 bytes, one packet.
- Record: packet count, packet sizes, timestamps between packets, any notes.

### 7. Repeated reads are stable

- Send the Temporary Patch name request five times without touching the XP-60.
- Expect five *Completed* cards with identical `Text:` and identical raw hex.
- Record: any difference, and the time between OUT and IN lines (latency).

### 7a. Fetch and decode the whole temporary Patch

- Devices → **Current Patch · inspection** → **Fetch temporary Patch**.
- Expect five RQ1/DT1 exchanges (`03 00 00 00` 73 bytes, then `03 00 10 00`,
  `03 00 12 00`, `03 00 14 00`, `03 00 16 00`, 129 bytes each) and the card to
  show the decoded patch name, a summary line and all 584 parameters.
- **Compare the decoded name with the XP-60's display.** They must match.
- Spot-check three or four parameters against the XP-60's own edit pages
  (Tone switches, a wave number, cutoff, a reverb type). This is the only step
  that tests parameter *meaning* rather than structure.
- Record: the payload size of each incoming DT1 (the `bytes=` figure on the IN
  lines). **This settles the open 129-byte question** in
  `docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md` §2.1: note whether the XP-60
  sends 129 bytes in one message or splits at 128. Saving the captured hex and
  running `tools/capture_diff.py` over it prints the distinct DT1 payload sizes
  seen, which is the observation §2.1 asks for.
- Record: the gap between consecutive DT1s from the timestamps.

### 8. Temporary-area write and read-back — **only after 1–7a pass**

This is the Phase 3 round trip: FETCH → DECODE → ENCODE → SEND → FETCH AGAIN →
COMPARE. The application performs it; nothing needs to be assembled by hand.

Safety properties built into the action, worth understanding before using it:

- the only writable target is the **temporary** Patch area (`03 00 00 00`), the
  edit buffer. Permanent User memory is not reachable from this screen at all;
- the write must be **armed** immediately beforehand, and one arming permits
  exactly one write;
- arming is refused until a temporary-Patch read has succeeded in this session
  — which is what steps 1–7a establish;
- the Patch present beforehand is captured automatically as a **safety
  snapshot** and can be written back with one action.

Procedure:

- Devices → **Write and verify · DT1**. Read the plan text, then **Arm write**.
- Press **Write back & verify**. The card walks through: capturing safety
  snapshot → sending → reading back → comparing.
- Expect **Verified**: the read-back equals what was sent, parameter for
  parameter.
- If it reports **Read-back mismatch**, the differences are listed by name
  ("Tone 2 Cutoff Frequency: 84 → 83"). Record every one. Do not dismiss them;
  a mismatch means the codec, the address map or the transfer is wrong.
- Confirm permanent memory is untouched: read USER:001's name (step 2 preset)
  before and after. They must be identical.
- Press **Restore snapshot** (arming again) and confirm the original patch
  returns.
- Power-cycle the XP-60. The temporary edit **must not persist**; the startup
  state follows the XP-60's **Power Up Mode** setting (`LAST-SET` or
  `DEFAULT`), so do not require the exact previous temporary patch to reappear.
- Record: the raw hex of the outgoing DT1s and the read-back DT1s, and the
  transfer card's final message.
- Save that hex to two files and run
  `tools/capture_diff.py sent.txt readback.txt` as an independent check of the
  application's own comparison. The tool resolves each address against the
  transcribed Parameter Address Map, so an agreement is a second reading of the
  same bytes rather than the verifier grading itself. It accepts hex text
  copied from the Protocol activity panel as well as binary `.syx`, and reports
  address coverage differences separately from value differences.

### 8a. Deliberate mismatch check

Confidence in a verifier that has never failed is worth little.

- Fetch the temporary Patch, arm, and write it back. Verified.
- Now change one parameter on the **XP-60's own front panel** (for example
  Tone 1 level), then press **Write back & verify** again without re-fetching.
- Expect **Verified** still, because the app rewrites its own copy over the
  panel edit.
- Then fetch, arm, write, and while it is sending, change a value on the panel.
  A mismatch here is informative rather than a defect.
- The dependable check: confirm that the mismatch report names the parameter
  you changed when one does occur.

## Additional observations worth capturing

- Send a request with a **wrong device ID** (e.g. 18 while the XP-60 is 17).
  Expect a timeout and no reply. Confirms the device-ID filter.
- Send the **System, first 16 bytes (project-defined)** preset. Record the raw
  hex; it becomes the first System fixture. The 16-byte size is a project
  choice, so a differently sized reply is information, not a failure.
- Note whether the XP-60 transmits anything unsolicited while idle (Active
  Sensing is ignored by the transport; anything else appears in the log).

## Reporting

Append a section `## Hardware log <date>` to this file with the table above
filled in and the raw hex for each step. Then update the status column in
`docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md` and the `VerificationStatus`
values in `src/xp60/Xp60Device.cpp` for every fact that was observed.

## Hardware log 2026-09-04

First physical XP-60 session. Steps 1–7 pass; steps 7a and 8 are **not** done
(this session read only — no DT1 was transmitted at any point).

| Item | Value |
|---|---|
| XP-60 firmware version | not recorded this session |
| MIDI interface | CME **U2MIDI Pro** USB-MIDI cable, class-compliant, Windows 11 26200 |
| Backend | `libremidi 5.4.3 / Windows Multimedia / Windows UWP`, ports opened via Windows Multimedia |
| XP-60 Device ID | 17 (transmitted byte `10`) — confirmed by the device answering |
| Rx / Tx Exclusive | ON (implied: the device answers RQ1) |
| XP60Studio commit | `415f9df63bf4da632425a71b23d4254d25c076d3` (working tree modified) |

Captured with `tests/tools/hardware_probe.cpp`
(`xp60studio_hardware_probe`), which sends RQ1 only.

### Steps 1–5, 7 — temporary Patch name, three consecutive reads

```text
OUT  F0 41 10 6A 11 03 00 00 00 00 00 00 0C 71 F7
IN   F0 41 10 6A 12 03 00 00 00 53 61 72 70 69 6E 61 20 4B 61 73 74 7C F7
```

- Model ID `6A` **single byte**, exactly as documented — `41 10 6A 12` follows `F0`.
- Checksum byte `7C`, validates.
- Decoded text `"Sarpina Kast"`; **still to be compared against the XP-60 display.**
- Three reads returned byte-identical replies. Latency 13–15 ms.

### Additional read-only reads

```text
User Patch USER:001 name
OUT  F0 41 10 6A 11 11 00 00 00 00 00 00 0C 63 F7
IN   F0 41 10 6A 12 11 00 00 00 53 74 72 69 6E 73 20 20 20 20 20 20 2C F7   "Strins      "

System area, first 16 bytes (project-defined size)
OUT  F0 41 10 6A 11 00 00 00 00 00 00 00 10 70 F7
IN   F0 41 10 6A 12 00 00 00 00 01 00 00 01 00 05 3F 00 01 01 01 00 00 00 00 00 37 F7
```

The project-defined 16-byte System read is answered with exactly 16 bytes.

### §2.1 settled — the XP-60 sends 129-byte Tone blocks in one message

RQ1 `03 00 00 00`, size `00 00 18 00`. Five DT1 replies, ~70 ms apart:

| Address | Payload bytes |
|---|---|
| `03 00 00 00` | 73 (Patch Common) |
| `03 00 10 00` | **129** (Tone 1) |
| `03 00 12 00` | **129** (Tone 2) |
| `03 00 14 00` | **129** (Tone 3) |
| `03 00 16 00` | **129** (Tone 4) |

Total payload 589 bytes. This answers the open question in
`docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md` §2.1 and
`docs/DEVICE_ACCEPTANCE.md` area 3: the instrument transmits a whole 129-byte
Tone block in a **single** DT1 and does **not** split at 128. Interpretation 1
in §2.1 is the correct one — the 128-byte rule governs data sent *to* the
XP-60. The golden fixture `user-bank-amal.syx` was right and the conservative
reading of the manual was wrong.

### Roland's published RQ1 example — the reply is sparse

RQ1 `01 00 00 00`, size `00 00 1F 19` (Roland's own documented example). The
OUT packet matched Roland's published bytes exactly:

```text
OUT  F0 41 10 6A 11 01 00 00 00 00 00 1F 19 47 F7
```

17 DT1 replies, ~37 ms apart, in ascending address order:

| Address | Payload bytes |
|---|---|
| `01 00 00 00` | 66 (Performance Common) |
| `01 00 10 00` … `01 00 1F 00` | 25 each (16 Performance Parts) |

Total payload **466 bytes, not 3993**, and no packet exceeded 128 bytes.

**The RQ1 size is an address span, not a payload byte count.** Roland block
addresses are padded: Tone blocks sit 0x200 address units apart but hold 129
bytes; Performance Parts sit 0x80 apart but hold 25. A request covering a span
is answered with only the populated blocks inside it, so the payload received
is always smaller than the size requested. The device covered the requested
span exactly — the last reply ends at `01 00 1F 19` — so nothing was clipped
or dropped.

This also answers two rows previously marked Unknown: the XP-60 **does** answer
an RQ1 spanning several blocks, and replies arrive in **ascending address
order**.

### Defect this uncovered in `RolandRequestTracker`

`src/protocol/RolandRequestTracker.cpp:29` sets
`expectedBytes = request.size().value()` and allocates a byte-coverage array of
that length, then requires each DT1 to land at the first uncovered **byte**
offset. Against the real instrument that model does not hold:

- a multi-block read can never satisfy `isComplete()` (589 covered of 3072
  requested), so it stalls until the timeout and reports
  `No further data after 589 of 3072`;
- every block after the first is recorded as an out-of-order note (offset 2048
  arriving where offset 73 was expected), which is normal device behaviour, not
  an anomaly.

Single-block reads — every current Devices preset except the Performance
example — are unaffected, which is why local testing never caught this.
Completion must be modelled in **address space** with padded blocks, not as a
contiguous byte range. Not fixed in this session.

### Tracker fix verified on the instrument

`RolandRequestTracker` now models completion over the padded address span
(`RequestOperation::isComplete`). Re-running Roland's published example against
the XP-60 with the application's own correlation logic in the loop:

```text
packets: 17, payload bytes: 466 of 3993 address units requested
tracker: Completed, 466 bytes in 17 chunk(s), covered through 3993 of 3993, no notes
```

Previously this stalled to a timeout at 466 of 3993 with 16 spurious
out-of-order notes. Single-block reads are unchanged and still complete in one
chunk with no notes.

### Reads reflect live instrument state

The temporary-Patch name read returned `"Sarpina Kast"` early in the session and
`"C-Z  Flute  "` later, after the Patch was changed on the XP-60's front panel
between runs. The read is of the live edit buffer, not a cached or synthesised
value. Step 3's requirement — that the decoded name matches what the display
shows — is therefore **still formally open**: both names were plausible, but
neither was read off the instrument's display at the moment of capture.

### Step 7a (partial) — whole temporary Patch fetched and decoded

`xp60studio_hardware_probe --patch` requests the documented Patch span
(`Xp60PatchLayout::patchSpan()` = 2945) at `03 00 00 00`, correlates the reply
through `RolandRequestTracker` and decodes it with `Xp60PatchCodec`:

```text
packets: 5, payload bytes: 589 of 2945 address units requested
tracker: Completed, 589 bytes in 5 chunk(s), covered through 2945 of 2945, no notes
patch decoded: "Childlike", 4 of 4 Tones enabled
  Tone 1: ON   wave INT group 1 #36  cutoff 127
  Tone 2: ON   wave INT group 1 #37  cutoff 127
  Tone 3: ON   wave INT group 2 #9   cutoff 127
  Tone 4: ON   wave INT group 2 #5   cutoff 127
```

The generated layout constant `kPatchSpan = 2945` matches the instrument
exactly: the last Tone block begins at offset 2816 and carries 129 bytes. The
decode reported no structural issues and no missing blocks.

**What this does and does not establish.** It establishes that the whole Patch
fetches, correlates and decodes without structural error against a real
instrument — the software half of `DEVICE_ACCEPTANCE.md` area 4. It does **not**
establish that the decoded values are *correct*: no value here was compared with
the XP-60's own edit pages. Cutoff reading 127 on all four Tones is plausible
for this Patch but is exactly the kind of value that a wrong offset would also
produce, so it is one of the spot checks the display comparison must cover.

### Area 1 remainder — failure modes give useful errors

- **Wrong device ID** (18 while the XP-60 is 17): RQ1 transmitted, no reply
  within 1500 ms, non-zero exit, no partial or "responding" state. Confirms the
  instrument's device-ID filter and, independently, that its ID really is 17.
- **Wrong MIDI OUT** (Microsoft GS Wavetable Synth, so nothing reaches the
  XP-60): same clean timeout with the cabling hint.

Endpoint loss during a transfer (unplugging mid-read) is still untested.
