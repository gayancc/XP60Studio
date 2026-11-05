# Demo Mode

Demo Mode lets musicians experience every **shipped** XP60Studio workflow without a physical XP-60 and without opening OS MIDI ports.

## How to enter (consumer path)

1. Open **Devices**.
2. Click **Enter Demo Mode** under “Try without hardware”.
3. XP60Studio relaunches into a simulated XP-60 session (no real MIDI).

While Demo Mode is on:

- Header shows **Demo Mode** and **Exit Demo**
- Devices shows **Use my XP-60** to leave

Your choice is remembered for the next launch.

## Developer overrides (optional)

These still work for automation and screenshots; regular users never need them.

```text
XP60Studio --demo
XP60STUDIO_DEMO=1
XP60STUDIO_DEMO=0   # force live even if the in-app preference is on
```

## What it does

1. Skips libremidi entirely.
2. Creates a `LoopbackMidiTransport` with ports named `XP-60 IN (Demo)` / `XP-60 OUT (Demo)`.
3. Runs an in-process `SimulatedXp60` seeded from the embedded user-bank fixture.
4. Starts a `DemoReplyPump` on the Qt event loop so RQ1/DT1 traffic is answered continuously.
5. Seeds the in-memory library from the same embedded verified USER bank fixture.
6. Auto-connects and fetches the temporary Patch so Devices, Patch Editor, Wave Browser, Library, and Bank Builder are immediately usable.
7. Shows a **Demo Mode** badge and **XP-60 SIM** connection label in the shell.

## Safety guarantees

- No OS MIDI ports are opened.
- No SysEx is sent to physical hardware.
- Write / verify / live audition still run through the real `DeviceSession` / `PatchTransfer` stack, but only against the simulator — label them as simulated, never as verified hardware.
- Demo Mode uses an in-memory library so it cannot change the user’s real collection.

## What is simulated today

| Surface | Demo behavior |
|---------|----------------|
| Devices | Connect, health, safe read, transfer against simulator |
| Patch Editor | Seeded patch; Play / Design / Expert editing |
| Wave Browser | Offline catalog (unchanged) |
| Library | In-memory 128-Patch demo library (does not touch the real DB file) |
| Bank Builder | Real arrangement/import/export workflows over the in-memory demo library |
| Write / verify / audition | Against `SimulatedXp60` |

## What is not faked

Unfinished navigation destinations remain unavailable. Demo Mode must not invent fake actions for future phases; extend simulation only when their real screens ship.

## Architecture note

Demo Mode is a composition-root choice in `src/app/main.cpp`, driven by the saved preference (`app/demoMode` in `QSettings`) or developer overrides. Switching from the UI saves the preference and relaunches so the transport can be swapped cleanly. QML still never builds Roland SysEx. Implementation lives in `src/simulation/`.

See also: [ROADMAP.md](ROADMAP.md) (Cross-cutting — Demo Mode), [ARCHITECTURE.md](ARCHITECTURE.md).
