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

### 8. Temporary-area write and read-back — **only after 1–7 pass**

Precondition: steps 2–7 confirm the `03 00 00 00` temporary Patch area and its
12-byte name field. The Devices screen has no write action in Phase 1; this
step is performed with the Phase 2 tooling once it exists, and is listed here
so the expectation is fixed now.

- Write the 12-byte name `XP60STUDIO  ` to `03 00 00 00` with a DT1.
- Read it back with the step 2 request. Expect the read-back to equal the
  written bytes and the XP-60 display to show the new name.
- Confirm that permanent User memory is untouched: read USER:001's name before
  and after; they must be identical.
- Power-cycle the XP-60. The temporary edit **must not persist**; the startup
  state follows the XP-60 **Power Up Mode** setting (`LAST-SET` or `DEFAULT`),
  so do not require the exact previous temporary patch to reappear.
- Record: all four raw messages.

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
