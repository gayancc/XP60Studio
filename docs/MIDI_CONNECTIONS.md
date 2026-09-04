# MIDI connections and wireless adapters

XP60Studio selects MIDI IN and MIDI OUT independently. These are the computer's
directions: computer OUT goes to XP-60 IN, and computer IN receives XP-60 OUT.
USB interfaces, DIN adapters and wireless bridges use the same session and
Roland protocol code. A port name alone does not prove that an XP-60 is connected.

## Connection workflow

1. Connect the interface. For Bluetooth MIDI, pair/connect it through the
   operating system first. For CME WIDI Master, follow CME's orientation and
   power instructions for its two DIN plugs.
2. Open **Devices**, select **Refresh endpoints**, then choose both MIDI ports.
   Port labels include their backend. An interface may appear under both
   Windows Multimedia and Windows UWP; normally choose both directions from
   the same backend. Windows UWP is the native Windows Runtime MIDI path.
3. Match **Device ID** to the XP-60. Choose **Conservative / wireless** pacing
   when establishing a wireless path or diagnosing timeouts.
4. Select **Connect**. The app opens both ports in a worker thread and shows
   **MIDI ports open**. Opening ports sends no MIDI messages.
5. Put the XP-60 in Patch mode and select **Test connection**. This issues the
   existing documented 12-byte temporary Patch name read. **XP-60 responded**
   requires a complete reply matching the request, model, Device ID and checksum.
   It is evidence of communication at that moment, not full hardware acceptance
   or continuous monitoring. Wrong-device, corrupt and unrelated replies do not
   pass the test. A subsequent timeout clears the responding state.
6. Use the existing temporary-Patch fetch and explicit write/read-back/compare
   workflow only after the small read succeeds.

Port preferences, Device ID and pacing are stored with QSettings for the current
user. The app does not automatically connect or write on startup. It restores
ports by identity, with a unique name/backend fallback when an OS handle changes
on replug. Missing or ambiguous preferences require a selection. Identically
named interfaces can still be physically indistinguishable to an OS backend;
check the chosen routing before writing.

If either endpoint disappears or the transport reports an error, both ports
close, outstanding reads and queued write batches are cancelled, and the editor's
hardware state is invalidated. Reconnecting requires an explicit action and
never replays the abandoned write queue. The write workflow requires a fresh
read and arming after reconnect. Replies already queued from an earlier session
are discarded. A USB bridge may remain enumerated when its radio link fails;
in that case a read timeout, rather than endpoint removal, reveals the problem.

**Cancel connection** discards an in-progress open and closes any resulting
ports. libremidi's Windows Runtime open waits on the OS MIDI operation; it does
not expose an abort handle. The UI remains responsive, but cancellation and
application shutdown must wait for that driver operation to return.

## Supported software paths and evidence

| Route | Implementation | Physical evidence here |
|---|---|---|
| Windows USB/DIN or USB wireless bridge | libremidi Windows Multimedia (WinMM) | Discovery tested; XP-60 transfer pending |
| Windows OS-paired BLE MIDI, including a candidate WIDI Master route | Optional libremidi Windows UWP / Windows.Devices.Midi | Backend build, initialization and discovery tested; WIDI transfer pending |
| macOS USB/DIN or OS-connected BLE MIDI | Existing libremidi CoreMIDI backend | Not tested during this Windows session |

The app does not implement Bluetooth discovery/pairing, a proprietary CME
protocol, or automatic firmware management. A WIDI Bud Pro/Uhost USB bridge is
another way to expose wireless MIDI to an ordinary MIDI application. It is not
required by the new Windows Runtime backend, and purchasing one is not a
prerequisite for attempting the native route.

Microsoft documents Windows.Devices.Midi support for USB and Bluetooth LE.
CME documents the WIDI Master connection routes and warns about long SysEx
receive limitations of the Korg BLE MIDI driver. That is one reason to test
complete Patch reads rather than infer SysEx reliability from note playback.
The native Windows Runtime route does not use that Korg driver.

## Build and diagnostics

The project remains pinned to libremidi **5.4.3**, commit
`390707b5d18b590509e823386f03fa712ef6ac1b`. On the local MinGW setup:

```powershell
./tools/install_windows_winrt.ps1
./tools/build_windows.ps1
```

The installer downloads the MSYS2 C++/WinRT **2.0.250303.1-2** archive, verifies
SHA-256 `ca3cb1fee300c4b8c8d21437cb964dccccae8b9a131e10aad911fb7fcd351c8d`,
and extracts it under `.qt/cppwinrt`. It installs no driver. The build script
enables `XP60STUDIO_ENABLE_WINUWP` when these headers exist; without them it
prints a warning and retains WinMM. Other Windows builds can enable the CMake
option with suitable `CPPWINRT_PATH` and `WINRT_HEADER_PATH` include roots.

`build-windows/xp60studio_midi_probe.exe` lists backends and discovered port
names without opening ports or sending messages. It needs the MinGW runtime
on PATH, like the application. On 2026-09-04 it exited successfully with
Windows Multimedia and Windows UWP, zero inputs and the system GS synth output
under each API. This is software discovery evidence only.

## Pacing policy

| Profile | Outgoing message gap | Maximum outgoing DT1 data payload | First reply timeout | Between-chunk timeout |
|---|---:|---:|---:|---:|
| Standard | Project XP-60 transfer defaults | Project XP-60 transfer defaults | Project XP-60 transfer defaults | Project XP-60 transfer defaults |
| Conservative / wireless | 60 ms | 64 bytes | 5 seconds | 3 seconds |

Conservative settings are an application policy to try on slower links, not a
measured WIDI guarantee. They cannot control the XP-60's reply packet sizes or
repair a driver that truncates incoming SysEx. Changing the profile is disabled
while requests or a Patch transfer are active. There are no automatic write retries.

## Physical acceptance still required

Record the OS version, Bluetooth radio/interface, WIDI firmware, XP-60 firmware,
backend, both selected port names, Device ID and pacing for each route tested.
Use [the XP-60 hardware procedure](HARDWARE_VALIDATION_XP60.md), then record:

- Correct small-read name, exact request/reply bytes and elapsed time.
- Repeated complete five-block Patch fetches with valid checksums and no
  missing/truncated chunks; retain observed packet sizes and timeout counts.
- Explicit temporary-Patch write, fetch again and byte comparison; confirm
  permanent User memory is unchanged.
- Adapter removal or radio loss during a read and a paced write; confirm
  cancellation, no replay after reconnect, fresh read/arming required.
- Wrong Device ID, swapped input/output, occupied ports and repeated reconnects;
  confirm useful errors and no false responding state.

No XP-60 or WIDI Master hardware validation has been performed in this work.

## Primary references

- [Microsoft: MIDI with Windows.Devices.Midi](https://learn.microsoft.com/en-us/windows/apps/develop/media-authoring-processing/midi)
- [libremidi compilation and supported backends](https://celtera.github.io/libremidi/compiling.html)
- [Pinned libremidi source](https://github.com/celtera/libremidi/tree/390707b5d18b590509e823386f03fa712ef6ac1b)
- [CME WIDI Master start guide](https://www.cme-pro.com/widi-master-start-guide-bluetooth-midi/)
- [CME WIDI Bud Pro start guide](https://www.cme-pro.com/widi-bud-pro-start-guide-bluetooth-midi-dongle/)
- [MSYS2 C++/WinRT package](https://packages.msys2.org/packages/mingw-w64-x86_64-cppwinrt)
