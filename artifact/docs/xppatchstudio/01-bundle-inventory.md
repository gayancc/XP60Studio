# 01. Patch Base bundle inventory

Phase 0 recon, read-only. Source: `D:\Codes\XP60Studio\artifact\Patch Base.app\Contents`. Nothing in the bundle was
modified, executed or copied. This document describes *what exists and what it is for*, in our own words.

## Method and limits

- Read: `Info.plist`, directory trees, file sizes, Mach-O headers (architectures), printable strings of the four
  main binaries, labels inside the compiled UI archives (`.nib`/`.storyboardc`), and the bundled `.syx` sample files.
- **Not done**: no disassembly or decompilation (no Ghidra/REA binary session yet). The binaries are compiled Swift,
  which stores short strings inline in code, so a strings pass **under-reports** parameter names and labels. Anything
  below marked *inferred* comes from type names or structure, not from a read of the logic.
- Third-party material (licence/updater frameworks) is listed only so it can be excluded.

## Identity (from `Info.plist`)

| Item | Value |
|---|---|
| Product | Patch Base for macOS, version 4.2.2 (build 210) |
| Publisher / bundle id | Coffeeshopped LLC / `com.coffeeshopped.Patch-Base-Mac` |
| Minimum OS | macOS 10.15; universal binaries (x86_64 + arm64) |
| Category | music |
| Registered file type | one exported type for the `.syx` extension (SysEx files). No document types declared |
| Update channel | Sparkle appcast URL (updater, not relevant to us) |
| UI technology | AppKit, storyboard-based (`Main`), principal class `NSApplication` |

## Top-level layout

```
Contents/
  Info.plist, PkgInfo, CodeResources, embedded.provisionprofile, _CodeSignature/   packaging and signing
  MacOS/Patch Base            374 KB   thin launcher (app delegate + main storyboard)
  Resources/                  Assets.car, Base.lproj/Main.storyboardc, Prefs.storyboardc, app icon
  Frameworks/                 22 frameworks (below)
```
1,013 files, 65 MB in total.

## Frameworks

| Framework | Size | Role | Relevance to XPPatchStudio |
|---|---|---|---|
| **PBAPI** | 3.1 MB binary | Generic synth-editing engine: addresses, parameter definitions ("Parm"/"Iso" value mappings), single-patch / bank / backup "truss" containers, MIDI message types, fetch intents, randomizer, morph | **Behaviour reference** for the data model (clean-room only) |
| **PBCore** | 5.7 MB binary, 137 resources | All shared UI: editors, bank/backup/patch browsers, MIDI map and learn, preferences, module manager, licensing | **Behaviour reference** for workflows |
| **Roland** | 6.7 MB binary, 135 resources | Roland module definitions (Swift types for many models including **XP30, XP50, XP60, XP80**, JV/XV/JD/D-series, JP, JX, JDXi, MKS, Alpha Juno) plus sample SysEx files, structure diagrams (PNG) and PDFs | **Primary**: the XP-60 definition and the XP-family samples |
| Yamaha (17.6 MB), Korg, Access, Emu, Behringer, DSI, Novation, Waldorf, Casio, Alesis, Clavia, Kurzweil, Oberheim, Ensoniq, ASM | other manufacturers' modules | **Out of scope** |
| Paddle (72 files) | payment/licence SDK | Out of scope, no licensing in XPPatchStudio |
| Sparkle (154 files) | auto-updater | Out of scope |
| RxSwift, RxRelay | reactive programming runtime | Out of scope (implementation detail) |

## The Roland framework, in detail

- **Module types present** (Swift type names found in the binary): `XP30`, `XP50`, `XP60`, `XP80`, each with a nested
  `Module` type. So the XP-60 is a first-class module. *Confidence: confirmed that the type exists; how it differs
  from XP-80 is **unknown** without disassembly.*
- **Resources by kind**: SysEx sample files (`.syx`), structure/diagram images (PNG), reference PDFs. Models with
  samples include JV-880/1080/2080, XV-2020/3080/5050/5080, XP-50/80, D-10/110/50, JD-990, JD-Xi, JP-8080, JX-8P,
  MKS-50, Alpha Juno.
- **XP-family sample files** (relevant): `xp80-voice.syx`, `xp80-global.syx`, `xp50-global.syx`. There is **no
  XP-60-named sample**, and no XP performance or rhythm sample. See `03-xp60-protocol.md` for what they confirm.
- The sample files are template/seed data (initial patch and global settings), not user content.
- The diagram PNGs and PDFs are reference artwork. **They must not be reused** (clean-room rule).

## PBCore resources that reveal features (window / view names)

| Resource | What it is |
|---|---|
| `Main`, `MainMenu` | Application shell and menu bar |
| `PatchEditorWindowController` | Editor window for a single patch (toolbar: fetch, init, randomize, rename, send, save-to-synth, external MIDI control, in-app keyboard, file-browser toggle) |
| `NonbankableEditorWindowController` | Same toolbar for single-instance data that is not part of a bank (global/system style data) |
| `BankEditor*`, `BankController`, `BankPreviewController`, `BankCollectionViewCell` | Bank editor: grid/collection of patches, fetch/write bank, "Load Bank" preview |
| `BackupEditor*`, `BackupPreviewController`, `BackupHeaderView` | Whole-synth backup editor and preview |
| `FullRefEditor*` | "Refs" editor: a set of references fetched/sent as a group (fetch all, send all, save to synth) |
| `PatchBrowserController`, `PatchPartBrowserController` | Patch browser (banks, folders, patches) and a variant for multi-part data ("Parts") |
| `SysexibleOutlineController` | The file/folder outline of stored SysEx, with new-folder |
| `MyModulesWindowController`, `AllModulesCell/Header`, `ModuleSectionsCell/Header` | "Synth Editors": add, enable and filter the editors, per-module MIDI in/out |
| `BasicSynthSave` | Choose a destination slot in the synth, then save a patch there |
| `MidiComm`, `MidiMap` | MIDI communication settings and the MIDI map/learn table |
| `Prefs` | Preferences (general, MIDI interfaces, throughput) |
| `FetchComplete`, `AutoPaddingController`, `PADWebPopWindowController` | Fetch-finished notice, layout helper, web popup |
| `Licensing` | Trial/purchase/activation. **Out of scope** |

## Conclusion for Phase 1 planning

The bundle gives reliable **workflow and feature evidence** (menus, toolbars, windows, preferences) and **three
real XP-family SysEx files** that confirm framing and checksum. It does **not** give a readable parameter map: that
must come from the official manual (see the blocker in `03-xp60-protocol.md`).
