# Phase 1 Exit Report — Protocol Foundation + Application Shell

Status: **implementation complete, automated verification green, hardware
verification pending.** Phase 1 is *not* closed until the procedure in
`HARDWARE_VALIDATION_XP60.md` has been executed on a physical XP-60.

Build used for this report: Ubuntu 24.04, GCC 13.3, CMake 3.28, Ninja, Qt 6.4.2
(distribution packages; the project baseline is Qt 6.11.x — see "Known
limitations"), libremidi v5.4.3 (`390707b5d18b590509e823386f03fa712ef6ac1b`).

---

## Implemented

Layer by layer (lower layers never depend on higher ones; none of the C++
below QML knows about Qt Quick, and nothing above `roland/` builds SysEx bytes
by hand):

| Layer | Target | Contents |
|---|---|---|
| Roland SysEx protocol (no Qt) | `xp60studio_roland` | `RolandAddress`, `RolandSize` (7-bit quads with carry arithmetic), `RolandChecksum`, `RolandCommand` (RQ1/DT1), `RolandDeviceId` (10H–1FH ↔ 17–32), `RolandModelId` (variable length), `RolandSysExMessage` (factory-constructed, always encodes with a correct checksum), `RolandCodec` (decode with 13 distinguishable failure reasons), hex utilities. |
| XP-60 facts (no Qt) | `xp60studio_xp60` | Model ID `00 6A`, factory device ID, six base addresses, three safe read presets, transfer defaults — every fact tagged with a `VerificationStatus`. |
| MIDI transport (no Qt) | `xp60studio_midi` | `IMidiTransport` (separate IN/OUT endpoints, enumerate/open/close/send, receive/error/hot-plug handlers), `SysExAssembler` (fragment reassembly, realtime interleaving, abort/overflow/stray accounting), `LoopbackMidiTransport` test double. |
| libremidi backend (no Qt) | `xp60studio_midi_libremidi` | `LibremidiTransport` on libremidi 5.4.3: observer-based enumeration and hot-plug, MIDI 1 input/output on the platform default API (ALSA / CoreMIDI / WinMM), SysEx enabled, only file that includes libremidi headers. |
| Protocol operations (no Qt) | `xp60studio_protocol` | `RolandRequestTracker` — request states `RequestSent → AwaitingData → Receiving → Completed / TimedOut / Cancelled / FailedValidation`; DT1↔RQ1 correlation by device, model and address range with a per-byte coverage map (no chunking assumptions); first-response and between-chunk timeouts driven by an injected clock; `TransferPacing` + `chunkDataSet`. |
| Diagnostics (no Qt) | `xp60studio_diagnostics` | `ProtocolLogEntry` with timestamp, direction, endpoint, device/model/command/address/size, payload length, checksum status, raw hex, correlated request id; `formatLogLine` produces `20:14:03.115 OUT Roland RQ1 device=17 address=11 00 00 00 size=00 00 0C 00`. |
| Services (Qt Core) | `xp60studio_services` | `DeviceSession`: owns the transport, hops backend-thread callbacks to the Qt thread via queued invocation, paced send queue, decode → correlate → log pipeline, timeout polling, statistics, bounded log. |
| Presentation (Qt Core/Qml) | `xp60studio_presentation` | `DevicesViewModel`, `AppShellViewModel`, `MidiEndpointListModel`, `ProtocolLogModel`, `RequestOperationModel`, `ConnectionState` enum, QML type registration. QML receives display strings and typed enums only. |
| QML design system | `xp60studio_ui` (module `XP60Studio`) | Tokens `Theme` / `Typography` / `Metrics` / `Motion`; controls `XpButton`, `XpCard`, `XpComboBox`, `XpTextField`, `XpSpinField`, `XpMetricTile`, `XpPanelHeader`, `StatusPill`, `XpLabel`, `XpScrollBar`, `XpDivider`, `XpEmptyState`; shell `AppNavigationRail`, `AppHeader`, `ConnectionStatusIndicator`, `ScreenHeader`; screens `DevicesScreen`, `UnavailableScreen`; `Main.qml`. |
| Application | `XP60Studio` | Wires libremidi → `DeviceSession` → view models → QML. Falls back to the loopback transport if the MIDI backend cannot start. `XP60STUDIO_SCREENSHOT=<png>` renders the shell once for headless review. |

Devices / Diagnostics screen (the only working destination in Phase 1):
MIDI IN / MIDI OUT pickers, device ID field (17–32) with model ID readout,
Connect / Disconnect with a text+colour state pill, a **Safe read test (RQ1)**
area with presets, hex address/size fields validated in C++, a disabled
**Write test (DT1)** with the reason shown, a **Requests** list (state, progress,
bytes, decoded text, notes, failure reason), health metric tiles and the
**Protocol activity** log with expandable raw hex and checksum pills. The
navigation rail lists all eight destinations from the master mockup; the
seven future ones are disabled with their roadmap phase shown.

Screenshot (headless software render, Qt 6.4.2):
`docs/design/screenshots/phase1-devices-screen.png`.

## Automated verification

`ctest` — 10 executables, all passing (Qt Test + Qt Quick Test, offscreen):

| Test | Covers |
|---|---|
| `tst_roland_checksum` | known values (incl. the 0x63 / 0x71 examples), split address+body, 500 generated bodies satisfying `(sum + checksum) mod 128 == 0`, rejection of every other checksum value and of single-bit corruption |
| `tst_roland_address_size` | byte serialisation, 7-bit carry (`03 00 00 7F + 1 = 03 00 01 00`, `+256 = 03 00 02 00`, user-patch stride), overflow/underflow, distance, hex parsing forms, size semantics (`00 00 01 00 = 128`), device-ID mapping, model-ID length identity |
| `tst_roland_codec` | RQ1/DT1 encode, decode, 256-byte DT1, `decode(encode(m)) == m`, `encode(decode(b)) == b`, 19 distinguishable failure fixtures (truncated, non-Roland, wrong model, unsupported command, bad address/size/data bytes, empty data, corrupted checksum), multi-model acceptance |
| `tst_xp60_device` | facts are ordered, sourced, and none claims hardware verification; presets are small and read-only |
| `tst_sysex_assembler` | complete, fragmented, byte-at-a-time and 20 kB SysEx, realtime interleaving, channel messages split across chunks, abort/overflow/stray handling |
| `tst_request_tracker` | full lifecycle, multi-chunk in order, out of order, overlap, out-of-range rejection, unsolicited data, unsent requests never match or time out, both timeouts, cancel/fail, earliest-request-wins, history bound, `chunkDataSet` |
| `tst_protocol_log` | log line shape from the phase brief, DT1/invalid-checksum/unsupported-model/non-Roland/raw MIDI/system entries |
| `tst_device_session` | loopback end-to-end: connect/disconnect/failures, RQ1 bytes on the wire, correlation, fragmented chunked responses, checksum failure counting, injected-clock timeout, cancellation, disconnect cancels, pacing, send failure, foreign traffic logged, endpoint removal, device ID, bounded log |
| `tst_presentation` | endpoint models (with `QAbstractItemModelTester`), connect enablement, hot-plug, error surfacing, device-ID validation, RQ1 field validation and presets, request flow into log/operation models, timeout/cancel, shell navigation rules, bounded log model |
| `tst_qml` | theme tokens, `XpButton` mouse/keyboard/variants, `StatusPill`, `XpTextField` invalid state, `XpComboBox`, `XpMetricTile`/`XpCard`, `AppNavigationRail`, `ConnectionStatusIndicator`, `AppHeader`, `DevicesScreen` load/bind/connect/send/cancel/validation/responsive stacking against the real view models |

Not covered automatically: `LibremidiTransport` itself (it needs a platform
MIDI API; it compiles and links, and the CI machine exposes no ports).

## Hardware verified

**Nothing.** No physical XP-60 was available. No behaviour in this repository
may be described as hardware-verified.

## Requires hardware verification

Run `docs/HARDWARE_VALIDATION_XP60.md` steps 1–8. In particular:

1. The XP-60 replies to RQ1 with model ID `00 6A` (two bytes) — if it uses
   `6A`, `xp60::modelId()` changes.
2. Device ID byte ↔ display number (17 ↔ 10H).
3. The `03 00 00 00` temporary-patch name read returns the name on the display.
4. `11 00 00 00` is USER:001.
5. DT1 chunking (≤ 256 bytes, ~20 ms gaps, ascending order) and the resulting
   timeout defaults.
6. Round-trip latency on the user's interface to tune `TransferPacing`.
7. Whether the XP-60 answers a Universal Identity Request (optional).
8. Temporary-area write and read-back — only after 1–7 and only with the Phase 2
   tooling (the Phase 1 UI has no write path).

## Known unknowns

Recorded in `docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md`: region sizes, Patch
Common / Tone layout for the XP-60, User Rhythm addresses, behaviour for
requests spanning regions or exceeding a region, response ordering guarantees.
The Phase 1 environment had no access to Roland's document library, so all
address-map rows are recalled from the XP-80/XP-60 MIDI Implementation and must
be cross-checked against the printed chart before the hardware session.

## UI foundation status

Exists: the permanent shell (rail, header with the authoritative connection
indicator, screen host), the token singletons, the foundation controls listed
above, and the Devices screen built from them. Tone colours are defined as
tokens (`Theme.tone1..4`) and used by the log direction badges only, so far.

Intentionally deferred (their backing layers do not exist yet): Dashboard,
Patch Editor / Four-Tone Mixer, Wave Browser, Bank Builder, Library, Compare,
Performance, Settings; icon set (the rail uses text glyphs); knobs, faders,
envelope editor and other music-specific controls; drawers, dialogs, toasts,
global operation tray; reduced-motion OS detection (`Motion.reducedMotion` is a
settable token).

Known limitations of this build environment:

- Verified against **Qt 6.4.2**, below the 6.11.x baseline. The code uses only
  APIs present in both (`font.families` was avoided for that reason). A 6.11
  build should be run on the developer machine; no API differences are expected.
- Linux/ALSA was the only MIDI backend compiled here. Windows (WinMM) and macOS
  (CoreMIDI) paths in `LibremidiTransport` follow libremidi's documented API
  but have not been compiled on those platforms in this session.
- The master mockup JPEG committed earlier was corrupt (all black, 14 KB). It
  was replaced with the image supplied during this task.

## Next step

Hardware validation session with the user's XP-60 (see above). Only after the
protocol facts are promoted should Phase 2 — XP-60 Patch Model — begin, using
captured DT1 dumps of the temporary Patch as its first fixtures.
