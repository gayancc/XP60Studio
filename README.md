# XPPatchStudio

**A desktop patch editor and librarian for the Roland XP-60.**

Edit every parameter of a Patch, a Performance or a Rhythm Set with real knobs, ranges and envelopes; keep thousands of sounds in a
searchable library; build 128-patch banks by drag and drop; and send the result to the instrument with a backup before and a read-back
after. XPPatchStudio is a native Windows and macOS application: a Rust core talks to the XP-60 over MIDI, and a Tauri window draws the
interface.

> **Status: built and tested without the instrument.** Everything runs and passes its tests against a virtual XP-60 and a real bank
> file. Nothing has been run against a physical XP-60 yet; that is one planned acceptance session (see [Roadmap](#roadmap)). Until a
> feature has been confirmed on the instrument it is labelled that way in the project's own ledger, and nothing is called done
> before then.

![The Patch editor, Zones tab](artifact/docs/screenshots/patch-zones.png)

## What it does

**Patch editor.** Four Tone cards: a dot to switch the Tone on or off (greyed when off), level and pan, and a wave picker (the instrument's own waves plus the expansion boards you mark as installed in Settings); tabs for Wave, Pitch, Filter, Amp, LFO, Output, Modulation, Patch,
Effects, Chorus/Reverb and Controllers. Envelopes are drawn and dragged, not typed. The **Zones** tab shows the whole keyboard with
one lane per Tone for key ranges, and a slider pair per Tone for velocity range and cross-fade. Undo and redo with a browsable
history, A/B compare, copy and paste of Tones and sections, init, randomize, mutate and morph. The open patch can be exported as .syx or .mid from the File menu.
The **Patch** tab draws the ten Structure types as wiring diagrams you click to choose from; **Effects** names the parameters of all 40 EFX types; a **Tone** tab puts every part of one Tone on a single page; an **All 4 Tones** switch lays one section of the four Tones side by side; controller destinations read Pitch, Cutoff, Pitch LFO1 and so on. With a bank open, previous/next buttons and a **Bank** tab step through its patches.

![Filter envelope and Amp envelope](artifact/docs/screenshots/patch-amp.png)

**Performance and Rhythm Set editors.** Sixteen Parts on two tabs, each with its key range on a shared keyboard and its Patch chosen
in the same row; Common, Effects, MIDI and per-Part tabs underneath. Rhythm Sets edit key by key on a vertical roll.

![The Performance editor](artifact/docs/screenshots/performance.png)

**Librarian.** An on-disk SQLite library with search, tags, favourites and folders, import and export of `.syx` and standard
`.mid` files, find and replace across names, browsable factory patch lists, "hear on hover" through the instrument's temporary memory, and automatic backups before
every write.

![The Library](artifact/docs/screenshots/library.png)

**Bank builder.** A 128-slot grid: drag slots to swap, drag library patches onto slots, load a bank from the instrument or a file,
export it, and write it back. Nothing reaches the instrument until you arm and confirm a write.

![The Bank builder](artifact/docs/screenshots/bank.png)

**Made for a desktop.** Its own title bar and window controls, a status readout in the toolbar that follows the pointer (it shows the
value of the control you hover, as the hardware shows the parameter you touch), a hint bar, drag and drop of files anywhere in the
window, MIDI controller learn (including NRPN and RPN), an editable shortcut map, saved window layouts, and a dark theme with a light
alternative.

![The light theme](artifact/docs/screenshots/patch-zones-light.png)

## Safety of your sounds

Writing to the instrument is a deliberate act, never a side effect of editing.

- **Temporary and permanent memory are different operations.** Live edit and hover audition use the instrument's temporary area and
  never touch User memory.
- **Writes to User memory are armed and confirmed**, the destination is chosen by you, what it overwrites is read and saved first, and
  the result is read back and compared.
- **Fixed and yours are kept apart.** The XP-60's Preset patches and any installed Wave Expansion board are read-only memory, the same on
  every instrument, and the app never writes to them (you edit a copy and save it to User memory). User memory and System settings
  are yours; the factory list of the User bank is shown only as "as shipped" and is never presented as what your instrument holds.
- The state of a sound is always shown in words and colour: LOCAL, EDITED, SENT, TEMP VERIFIED, STALE, SAVED or MISMATCH.
- Damaged or truncated files are errors with a position, never silently shortened.

## Getting started

You need Windows 10/11 (the platform it is developed and tested on) or macOS, an XP-60 and a MIDI interface (the app also works without one: you can edit, organise and export
files offline).

Build it from source:

```bash
rustup target add wasm32-unknown-unknown
cargo install wasm-bindgen-cli --version 0.2.130 --locked
cd ui && npm install && npm run wasm && npm run build && cd ..
cargo build -p xppatch-app --release --features custom-protocol
```

On Windows, double-click `Run XPPatchStudio.bat` to start the app (it builds first if needed) or
`Rebuild and Run XPPatchStudio.bat` to rebuild and start. An installer is produced with `cargo tauri build`
(`cargo install tauri-cli --version "^2"`).

## Technical overview

```
XP-60  <--MIDI SysEx-->  midir  -->  xppatch-midi  -->  xppatch-app (Tauri commands)  -->  Svelte UI
                                         |                                                     |
                              xppatch-sysex, xppatch-core  <-- the same codec, as WebAssembly -+
```

| Path | What it is |
|---|---|
| `data/factory/xp60-factory.json` | The instrument's own reference lists, built by `scripts/build_factory_data.py` from Roland's published documents: 640 factory patch names, 448 internal wave names, the 23 SR-JV80 expansion boards and the wave names of the 17 boards Roland publishes lists for. Names only. |
| `data/xp60-parameters.json` | The single source of truth: every parameter's address, size, encoding, range and display text. The editor controls and the SysEx addressing are both generated from it; nothing is copied by hand between them. |
| `crates/xppatch-sysex` | Roland addressing (7-bit carry), checksum, RQ1/DT1 messages, chunking, and the memory map that tells temporary memory from User memory. |
| `crates/xppatch-core` | The parameter map, the block and patch codec, `.syx` and `.mid` files, display formatting, undo history, randomize, mutate and morph. No I/O. |
| `crates/xppatch-midi` | A `MidiTransport` trait with the real `midir` transport and a **virtual XP-60** that the whole stack is tested against, with injectable timeouts, retries and noise. |
| `crates/xppatch-library` | SQLite library: import and export, tags, folders, search, backups. |
| `crates/xppatch-wasm` | The codec compiled to WebAssembly, so the interface and the Rust side share one implementation. |
| `crates/xppatch-app` | The Tauri 2 desktop shell: windowing, MIDI commands, library and storage commands. |
| `ui/` | Svelte 5 and TypeScript: the control kit (knobs, sliders, envelopes, key map, readout), the editors and the views. |

**Protocol facts the code is built on.** Messages are Roland DT1 (data set) and RQ1 (data request) with a checksum over address and
data; the Patch temporary area is at `03 00 00 00` and User patch memory at `11 nn 00 00`; at most 128 data bytes are sent per
message with at least a 20 ms gap, one request is outstanding at a time. Each of these is recorded with its source, and anything not
yet verified is recorded as unknown rather than guessed. The order of authority is the official Roland manual, then verified
hardware captures, then supplied known-good files, then experiments on the instrument.

**Quality.** `unsafe` code is forbidden across the workspace. The codec is checked by golden-file tests against a real XP-60 bank
(every parameter round-trips), property tests, and malformed-input tests. The interface has component and model tests for every
control and view. Run them with:

```bash
cargo test --workspace
cargo clippy --all-targets -- -D warnings
cd ui && npm test && npm run check
```

**Design.** The interface follows the design language of FL Studio, reinterpreted rather than copied: graphite surfaces, a single
warm accent, small-caps labels, drag-to-set controls with fine adjustment, and a hint bar. All controls are drawn from scratch as
vector graphics; no third-party assets, icons or fonts are bundled.

## Roadmap

| Phase | Scope | State |
|---|---|---|
| 0 | Reconnaissance and written specification | Done |
| 1 | Core, SysEx codec, checksum, golden tests | Done |
| 2 | MIDI transport, fetch and send with timeouts and retries | Done (virtual XP-60) |
| 3 | Control kit, theme, storybook | Done |
| 4 | Patch editor, hint bar, undo, live edit | Done, awaiting instrument |
| 5 | Performance and Rhythm Set editors | Done, awaiting instrument |
| 6 | Librarian, backups and the save ceremony | Done, awaiting instrument |
| 7 | Compare, init, randomize, mutate, morph, copy and paste | Done |
| 8 | Parity audit of every planned feature | Done: 54 rows, 19 done, 31 built and waiting for the instrument |
| 9 | **Hardware acceptance**: one session on a physical XP-60 | **Next** |

After hardware acceptance:

- Free docking and floating windows (the workspaces are tabbed today; named layouts can be saved).
- Writing System settings to the instrument (they can be read and edited locally now).
- Further hardware findings from the acceptance session folded back into the parameter map.
- Other XP and JV models, only after the XP-60 is proven.

## Documentation

Plans, the feature catalogue, the protocol notes, the parity matrix and the hardware acceptance ledger are in
[`artifact/docs/xppatchstudio/`](artifact/docs/xppatchstudio/). The change history is in [`CHANGELOG.md`](CHANGELOG.md).

## Licence and trademarks

All rights reserved for now; a licence will be chosen before a public release. Roland and XP-60 are trademarks of Roland Corporation.
XPPatchStudio is an independent project and is not affiliated with or endorsed by Roland. FL Studio is a trademark of Image-Line;
only its design language was used as a reference.
