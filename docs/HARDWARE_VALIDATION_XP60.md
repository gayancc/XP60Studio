# XP-60 Hardware Validation Procedure (Phase 1)

Purpose: prove, on a physical Roland XP-60, that the Phase 1 protocol
foundation sends valid Roland SysEx, receives and validates the reply, and
correlates it with the request. Nothing in this procedure writes to the
instrument unless step 8 is explicitly reached and its precondition is met.

The application build used must be recorded (git commit hash), and every
observation is captured from the **Protocol activity** panel of the Devices
screen (click a row to expand it and copy the raw hex).

## Preparation

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
  sends 129 bytes in one message or splits at 128.
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
