# 05. Behaviour spec from binary analysis (Patch Base, x86_64, Ghidra)

Source: Ghidra headless analysis of `PBAPI` (engine), `PBCore` (workflows) and `Roland` inside the bundle. Written in our
own words as a **specification of observable behaviour**; no code, names or assets are reused. Every item carries a
confidence tag:

- **CODE**: read directly from decompiled logic or a decoded constant.
- **NAME**: inferred from symbol names only (the code was not read yet).
- Hardware confirmation is a separate, final gate and is **not** claimed anywhere here.

Function names below are the bundle's own symbol names, quoted only to make the evidence re-findable.

## B-01 Roland checksum. CODE

`RolandChecksum(address, dataBytes, count)` (`PBAPI` at `0x2c350`): add every byte of the address field (emitted at the
message's address width) and every data byte, with overflow trapping. The result is the two's-complement negation of
the sum reduced to 7 bits: `checksum = (-sum) & 0x7F`. This equals `(128 - sum mod 128) mod 128` and matches the 7 sample
messages already verified in `03-xp60-protocol.md`.

## B-02 Default device ID. CODE

`RolandDefaultDeviceId` returns `0x10` (`PBAPI` at `0x2c340`). The device ID is a value passed into message
construction (`deviceId` transform on the truss builder), so it is **configurable per editor**, with `0x10` as default.

## B-03 Address field. CODE (packing) / NAME (carry rules)

`RolandAddress` is a single integer whose bytes are the SysEx address bytes. `sysexBytes(count)` emits the **most
significant byte first**, `count` bytes (the module supplies `count`, 4 for these synths). There are 7-bit helpers
(`bytes7bit`, `msbFirst`) and arithmetic members (`Parm.offset`, `Parm.inc`, `+`-style lane members) so addresses add
with 7-bit lane carry; the exact carry rule was **not** read yet and must be re-derived and unit-tested against the manual.

## B-04 Fetch request (RQ1) assembly. CODE (shape) / NAME (command byte)

`RolandEditorTrussWerk.fetchBytes(forAddress, size, addressCount)` (`PBAPI` at `0x2f590`) builds one request as:

```
[ header prefix captured from the module definition ] [ address, addressCount bytes ] [ size, addressCount bytes ]
[ checksum over address + size bytes ] [ 0xF7 ]
```

The header prefix (`F0`, manufacturer, device ID, model ID, command) is **supplied by the module**, so the command
byte is not visible in this function. It is expected to be `0x11` (RQ1) and **must be confirmed** in the manual and on
hardware. Note the **size field is encoded with the same byte width as the address** and is included in the
checksum, which is the Roland convention.

## B-05 Send pacing (the "throttle"). CODE

Each MIDI destination (`PBMidiDestination`) holds three settings, set up in its initialiser (`PBCore` at `0x1362c0`):

| Setting | Default | Meaning |
|---|---|---|
| chunk size (16-bit) | **32 bytes** | outgoing data is cut into chunks of this many bytes |
| bytes per second (16-bit) | **3125** | the target rate; this is the MIDI wire rate (31,250 baud / 10 bits per byte) |
| throttle (boolean) | **off** | when on, sending is paced; when off, data goes out unpaced |

Pacing: a repeating timer fires every `chunkSize / bytesPerSecond` **seconds**; with the defaults that is
`32 / 3125 = 10.24 ms`. A queue holds the *next chunks* and a single *chunk in progress*; each tick sends the next chunk.
A flush operation drains the queue. Settings are stored **per MIDI destination** (and therefore per interface), matching
the preferences screen. The sources have their own preference for NRPN handling (`withNRPN`), and a separate throttle
interval exists for the input handler (`addMidiInHandler(throttle:)`), so incoming controller floods can be rate-limited.
Open item: whether the timer starts only when throttle is on (the field is read inside the tick path, not the setup path).

For XPPatchStudio: expose the same three settings per output; keep the unthrottled path for fast interfaces.

## B-06 Fetch transforms and intents. NAME

The engine has a `FetchTransform` family with distinct cases: **single** (one request, with an *intent*), **multi**
(several requests with a **wait interval** between them), **bank** (per-patch requests with a **wait interval**), and
**send**. A fetch *intent* (`RxMidi.FetchIntent`) carries an "exact" flag, so a fetch can require a response of exactly
the requested size versus accepting a partial one. The Roland truss builder exposes `singleFetchTransform(forPath)`,
`multiFetchTransform(path)` and `multiFetchTransform(start, intents)`, plus `defaultFetchTransforms`.
Sends of multi-part data use a **composite send wait interval** between messages. Numeric wait values were not read yet.
Design consequence: bulk fetch and bulk send are **sequences with configurable inter-message waits**, not bursts.

## B-07 Chunked patch messages. NAME

Single patches, banks and "compact" patches can be parsed from and written to messages with a **chunk size** and an
**offset** (`makeChunks(bytes, offset, chunkSize)`, `singleMsg(parseBytes, chunkSize)`, `bankLikeSingleMsg`). This is
about splitting one logical block across several messages, which is separate from B-05's wire-level chunking.

## B-08 Randomize. NAME

Randomization is **per parameter span** (`Parm.Span.randomize`), invoked by a truss-level `randomize`, which for a Roland
patch walks the parameter map and draws values inside each parameter's range. Editors expose a toolbar *Randomize* action.
Open items: per-parameter exclusions and the random distribution were not read.

## B-09 Other workflow facts. NAME

- **MIDI learn / map**: a learn mode on the patch editor window, a handler that binds an incoming control message to the
  parameter under the pointer, and a persisted MIDI map table.
- **Undo/redo**: handled in the shared editor controller layer; commands are reversible edits.
- **Backups and banks**: `backup` constructors take a start address, a list of paths and "other valid sizes" (some backups
  accept alternate sizes), i.e. a backup is an **ordered list of blocks with validated sizes**.

## Second pass: items resolved

### B-03 (resolved) Address arithmetic is base-128. CODE

A `RolandAddress` is held as a packed integer whose bytes each carry 7 bits. All arithmetic works on a **compact 28-bit
integer** made of the four 7-bit lanes:

```
compact(a) = (a.byte3 << 21) | (a.byte2 << 14) | (a.byte1 << 7) | a.byte0          (each byte masked to 0x7F)
a + b      = expand( compact(a) + compact(b) )        // ordinary binary add, so carry flows across 7-bit lanes
a / b      = compact(a) / compact(b)                  // integer division; division by zero is an error
expand(c)  = (c>>21 & 0x7F, c>>14 & 0x7F, c>>7 & 0x7F, c & 0x7F)  as bytes, most significant first
```

Consequences: `00 00 00 7F + 1 = 00 00 01 00`; `03 00 10 00 + 00 00 02 00 = 03 00 12 00`; the maximum value is
`7F 7F 7F 7F` (28 bits). Offsets inside a block and block-to-block strides use this arithmetic, so it must be
implemented once and covered by golden tests. `RolandAddress(intValue:)` is the inverse of the compact form.

### B-06 (partly resolved) Wait values. CODE (value) / unit unverified

The fetch and send transforms only **carry** a wait-interval integer; the numbers come from the module definition.
In the Roland truss builder the outgoing-message transforms for patches and banks are all created with the constant
**30**, which is almost certainly milliseconds between messages. Treat 30 ms as the bundle's default, **unverified
against the manual or hardware**. Interplay with B-05: when throttle is on, B-05 paces bytes; the wait interval spaces
messages.

### B-08 (resolved) Randomize. CODE

Each parameter has a *span*: either an inclusive numeric **range** or an explicit **list of named options**.
Randomizing one parameter draws uniformly: for a range, `min + uniform(0 ..< (max - min + 1))`; for an options list,
a uniformly chosen entry. The system random source (a cryptographic-quality uniform generator) is used. A patch-level
randomize simply applies that to the parameters the module chooses. Exclusions (names, output assign and similar)
are decided by which parameters the module exposes to the randomizer and were not enumerated.

### Model ID for the XP-60. Strongly inferred

The XP-60 module definition **reuses the XP-80 type metadata and the JV-1080 definitions** (it references them
directly), so it shares the XP-80's header prefix: manufacturer `0x41`, model ID **`0x6A`**, command `0x12` for data
sets. The request command byte remains expected `0x11` and unconfirmed. Hardware and the manual remain the arbiters.

## Open items for the next pass

1. Confirm the request command byte (`0x11`) and the 30 ms unit on hardware.
2. Read the "other valid sizes" backup rule and the randomizer exclusions.
3. Recover the XP-60 data tables (addresses, sizes, parameter ranges): see `03-xp60-protocol.md` section 6.
4. Write golden tests for the base-128 address arithmetic (section B-03) and the checksum.

Everything here remains **unverified on hardware** until you confirm it on your XP-60.
