# Phase 4 revalidation — 2026-09-04

Status: **revalidation, safety repairs, local sections and explicit live audition
implemented; M2 remains open; M3 searchable catalog implemented with assignment pending.**

Review baseline: `07c4219`, on `origin/claude/phase-2-patch-model`.
The local `main` branch was still at Phase 1 (`214bf30`). Work continues on
`codex/phase4-revalidation`, preserving the existing local agent instructions.

## Execution plan

1. Trace the roadmap, milestone reports, source, tests and fixture evidence.
2. Establish a Windows Qt 6.11 / C++20 build and run all C++/QML tests.
3. Repair transfer/editor data-safety and parameter-display regressions first.
4. Check each M2 requirement against working controls and model behavior;
   compare fresh screenshots with the approved mockup.
5. Finish remaining M2 features, with meaningful regression coverage.
6. Close M2 only after behavioral, visual and hardware prerequisites pass.
   Per the subsequent user direction, continue M3 local work now and retain
   physical-device acceptance as the final gate.

## Evidence and phase status

| Area | Finding |
|---|---|
| Phase 1 | Protocol, MIDI abstraction/backend, pacing, diagnostics and shell implemented. Fresh Windows build and automated suites pass. |
| Phase 2 | Complete generated tables and supplied real bank fixture exist. Generator check passes: 200 rows, digest `sha256:a30c743a97ba9d89`. Python inspection parses 1,314 DT1 packets and 128 User Patch names. C++ golden fixture and round-trip suites pass locally. |
| Phase 3 | Snapshot/send/read-back/compare engine and fake-device tests exist. User confirms physical validation has not been performed. Cancellation and connection-scoped arming defects found during this review. |
| M2 | Four-Tone composition and local editing foundation exist. Missing behavior and correctness defects prevent completion. |
| M3 | No Wave Browser or transcribed waveform catalog exists. Remains next after M2 acceptance. |

## M2 acceptance checklist

| Requirement | Revalidation finding |
|---|---|
| Shared shell, tokens, four colored Tone cards | Implemented. Fresh normal/minimum-width Windows screenshots reviewed. Narrow header clipping repaired. |
| Current Patch header, local/hardware distinctions | Present; transfer reads previously replaced working data and history. Disconnect previously retained an ON XP-60 claim. Repairs pass regression tests. |
| Mixer level/pan/coarse tuning, Tone enable | Local model-backed controls exist. |
| Solo/Mute audition | Implemented as a temporary-Patch overlay in explicitly armed live audition; FakeXp60 tests pass. Physical audio check deferred. |
| Pitch/TVF/TVA | Local section controls now expose Pitch, TVF cutoff/resonance/type, TVA, envelope parameters, Wave/FXM, Tone Delay, Pan and Structure. |
| Graphical envelopes | Implemented; TVF was incorrectly treated as bipolar and could not reach raw 127 by drag. Repaired with passing regression coverage. |
| Key/velocity ranges | Graphical and keyboard interaction plus dedicated numeric entry. Ordered bounds are enforced on all editor paths, including Expert. |
| Motion/LFO | Both LFOs, all four modulation depths each, controllers and switches are locally editable. Hardware/audio validation remains open. |
| Effects | Common EFX, Chorus, Reverb and selected-Tone routing are editable, with all 40 documented EFX type names. EFX-specific byte-slot mappings/names/units remain unverified; raw fields are explicitly labelled. |
| Play/Design/Expert | Implemented over one working Patch and history. Expert has scope/group/search and virtualized enum/numeric controls; the four-Tone architecture remains visible. |
| A/B original/current | Display/history guards repaired; live audition now sends the selected A/B Patch without replacing local history. Physical audio check deferred. |
| Signal flow | Colored Tone-to-Structure overview connectors accompany the common effect nodes at four-column widths. A documentation-derived summary explains Structure pair outputs, DIRECT and effect sends. Replaced by a selected-Tone/Structure-pair branching diagram for MIX, EFX, DIRECT, parallel sends and Chorus output; see `PHASE_4_EFFECT_ROUTING.md`. |
| Safe real-time updates | Implemented as coalesced changed-span updates with one transfer in flight and complete read-back verification. Actual latency/audio behavior awaits hardware testing. |
| Explicit write and result | Present; Cancel write is now exposed and queued DT1 cancellation is repaired and tested. |
| Visual acceptance | Fresh screenshots retain the four-Tone composition, add colored overview connections, and show usable numeric bounds at minimum width. The compact header, shared desktop toolbar and original line icons are now implemented. Screenshot review follows the routing exception documented in `PHASE_4_EFFECT_ROUTING.md`. |

## Repairs in this working branch

- Tag transfer-owned fetches so safety snapshots/read-back never replace local
  working edits, the A original, or undo/redo history.
- Track last known hardware data separately from the local original; invalidate
  it on disconnect/device changes and uncertain transfers.
- Show the A original consistently in all Tone/envelope/range/settings readers;
  refuse history changes and hardware writes while comparing A.
- Validate edits before changing history, preserving redo and the full undo
  capacity on rejected values.
- Cancel queued DT1 batches; consume arming at attempt start; require a new
  read after reconnect/device-ID changes; reject reentrant write attempts
  without replacing the active transfer state.
- Use the documented unsigned TVF envelope range in the graph and drag mapping.
- Rename the knob's private arc angle to avoid redeclaring Qt 6.11's final
  `Dial.startAngle` property, which prevented the entire editor from loading.
- Repair a codec test diagnostic that dereferenced an absent error even on
  successful decoding; correct a TVF test's expected upper bound to 127 using
  the Parameter Address Map as evidence.
- Expose Cancel write in the editor, and retain per-suite text logs so Windows
  test failures remain diagnosable even when stdout is unavailable.
- Select a supported installed body font explicitly; offscreen Windows
  rendering otherwise chose Agency FB and changed the entire UI's typography.
- Wrap header actions below the identity at narrow widths, preserving access
  to Write without whole-window horizontal scrolling; add a QML geometry test.

## Local editor continuation

`EditorParameterModel` projects the generated descriptors into the selected
section or searchable Expert scope. It owns no Patch copy; accepted edits flow
through `PatchEditorViewModel` validation/history. The section model includes
both LFO waveforms with their LFO groups even though the source table categorizes
them under Wave. Menus use the exact transcribed enum labels. No protocol
offsets, guessed EFX meanings, waveform catalog or physical timing conversions
are exposed to QML.

The reusable `EditorParameterPanel` uses a virtualized GridView for dense
controls. Numeric edits preserve delegates/focus; navigation resets the scoped
projection. The twelve Patch name bytes are edited as one validated string in
Expert/Common. Play, Design and Expert never modify the patch merely by changing
mode. Exact note and velocity entry is available alongside the graphical ranges.
Colored Tone-to-Structure overview connections reproduce the approved composition
at four-column widths; wrapped layouts omit the crossing lines. These do not
claim to illustrate the internal algorithm of each Structure type.

Regression coverage includes selected-Tone identity, invalid/stale edits,
undo/A/B, no implicit MIDI sends, both LFOs and all modulation depths, EFX routing,
the two-byte Patch Tempo followed by Patch Level (preventing index/offset
confusion), scope/search, enum keyboard activation and actual numeric typing.

## Local Windows environment

Installed Qt 6.11.2 (qtbase, qtdeclarative, qttools, qtsvg), MinGW 13.1,
CMake 4.4.3 and Ninja 1.13.2. `tools/build_windows.ps1` configures, builds and
runs the full test suite; `-Run` launches the application. Qt archives are
downloaded from the official repository and checksum checked. The installer
adapter handles aqt 3.3.0's missing Windows split-repository mapping.

The native libremidi WinMM and optional Windows Runtime MIDI backends compiled
successfully. Read-only discovery initialized both and listed the system synth
outputs, with no external MIDI input present. No physical MIDI endpoint was
opened or written to.

## Connection reliability continuation

The user's WIDI Master request is implemented as a device-independent connection
workflow: separate persisted MIDI IN/OUT choices, backend-labelled port lists,
identity-based restoration, asynchronous open/cancel, read-only connection test,
and Standard/Conservative pacing. Open ports alone show MIDI OPEN; a matched
reply is required for XP-60 RESPONDED. Endpoint loss and backend errors close
both ports and cancel queued work without replay on reconnect. Connection epoch
checks reject already queued replies from older sessions. Tests cover these
state transitions, selection ambiguity, persistence, wrong-device replies,
timeouts, failed output opens, cancellation and queued-write disposal.

See [MIDI connections](MIDI_CONNECTIONS.md) for setup, upstream references,
driver-wait limitations, and physical acceptance checks. This establishes a
software path for OS-paired Bluetooth MIDI; WIDI Master interoperability and
full SysEx transfers still need a physical XP-60 test.

The automated regression suites pass. This establishes local software behavior,
not physical hardware verification or completion of the missing M2 features.

## Hardware gate

The user confirmed on 2026-09-04 that no physical validation has been performed.
Run `HARDWARE_VALIDATION_XP60.md`, including the temporary-Patch round trip and
parameter spot checks. Record firmware/interface/device ID, exact request and
reply bytes, decoded patch name against the display, response packet sizes,
read-back comparison, and unchanged permanent User memory. No hardware writes
were performed during this review.

## Validation log

- PASS: `python tools/generate_patch_tables.py --check`.
- PASS: Python fixture inspection; 112,206 input bytes, 97,752 image bytes,
  payload sizes 12/25/58/66/73/129. The existing 129-byte discrepancy remains open.
- PASS: `git diff --check`.
- Graph updated with `graphify update .`; parser reports partial extraction for
  39 files (including Qt macro-bearing headers and conditional catch blocks), so the graph is navigation
  evidence, not a compiler result.
- PASS: native Windows Qt 6.11.2 / MinGW 13.1 application, WinMM and Windows Runtime backend build.
- PASS: all **22/22 CTest suites**, including generator freshness. Qt Test
  reports 45 passing editor checks, 20 transfer checks, 28 session checks,
  18 presentation checks, and 55 QML checks
  (these totals include setup/cleanup cases).
- Reviewed fresh captures: `design/screenshots/phase4-editor-windows.png`
  (1440×1040) and `design/screenshots/phase4-editor-windows-minimum.png`
  (1024×680). Both use the supplied fixture through FakeXp60 and software
  rendering, not a connected instrument. The minimum-width capture confirms
  the repaired Write action is fully visible.
- Reviewed continuation captures: `design/screenshots/phase4-motion-windows.png`,
  `phase4-effects-windows.png` and `phase4-expert-windows.png` (1440×1040),
  plus `phase4-ranges-windows-minimum.png` (1024×680). These use the real
  scroll bounds to bring the new panels into view. Tone cards, section controls,
  exact fields and the documented limits are readable without horizontal scroll.
- Build warnings remain: optional Vulkan headers absent and legacy QTP0004
  policy unset. They do not block the tested software rendering or QML loading.
- The C++ model tester emits an existing Qt warning about unregistered GUI
  type 4097 (`QPixmap`) in both presentation suites; model assertions pass.
  The QML suite reports no warnings or failures.
- Pending: physical validation and the M2 feature/visual gaps listed above.
- Reviewed connection captures: `design/screenshots/devices-connections-windows.png`
  (1440×1040) and `design/screenshots/devices-connections-windows-minimum.png`
  (1024×680). Both are explicitly Loopback/FakeXp60 evidence. Separate selectors,
  pacing, Connect/Disconnect and Test connection remain readable; longer content
  scrolls vertically. The QML suite exercises scrolling to diagnostic actions.
- Reviewed live-audition captures: `design/screenshots/phase4-live-audition-windows.png`
  (1440×1040), `phase4-live-audition-windows-minimum.png` (1024×680), and
  `phase4-effects-catalog-windows.png` (1440×1040). Start/stop/restore states,
  wrapped guidance, named EFX selection and routing text are readable. These
  are Loopback/FakeXp60 captures; LIVE · VERIFIED describes simulated read-back,
  not physical hardware acceptance. The fixed processor overview remains an
  open visual/behavioral gap.

## Remaining execution order

### Routing continuation

The fixed series chain is replaced by the documented selected-Tone/Structure-pair
graph. MIX/DIRECT destinations, independent Tone sends, EFX sends and all three
Chorus output modes update with selection, edits, history and A/B. Effects and
Expert expand exact send values; other sections keep a compact graph. See
`PHASE_4_EFFECT_ROUTING.md` for source references, scope and the remaining EFX
mapping evidence procedure.

Windows validation: **23/23 CTest suites pass**. The new routing suite has
8 passing Qt checks; editor has 46 and QML has 56, including setup/cleanup.
Reviewed normal and minimum-width FakeXp60 captures:

- `design/screenshots/phase4-routing-compact-windows.png`
- `design/screenshots/phase4-routing-effects-windows.png`
- `design/screenshots/phase4-routing-effects-windows-minimum.png`

Final QML layout recheck passes (56 Qt checks). MIX and DIRECT variants were
also rendered at 1024×680 from local fixture edits; DIRECT has no effect-send
arrows and MIX bypasses EFX. `git diff --check` passes. `graphify update .`
updated the code graph (2,409 nodes / 5,050 edges); its parser reports partial
extraction in 40 files and no nodes for `hooks.json`. Compilation and tests,
not the graph, are the validation evidence; semantic document updates are separate.

No hardware writes were performed. M2 is still open for EFX mappings, physical
acceptance and remaining shell/header/glyph visual fidelity; M3 has not started.

### Next acceptance work

The user explicitly deferred physical validation to a separate manual task and
authorized continued implementation. This changes execution order, not evidence:
the Phase 3 hardware loop and M2 hardware acceptance remain unperformed.

The user authorized continuation of local controls before that gate. Pitch/TVF/TVA,
LFO, documented Effects fields, disclosure and exact range entry are now
implemented and tested locally. Explicit paced/coalesced audition and live
updates are now implemented, along with all 40 EFX type labels and a documented
Structure-aware routing summary. See [live audition](PHASE_4_LIVE_AUDITION.md).
Remaining M2 behavior work is EFX-specific byte-slot/unit mappings and physical acceptance.
The header/glyph refinement is implemented and reviewed below.
The fixed-chain routing defect is repaired and locally tested; the evidence needed
for EFX mappings is specified in `PHASE_4_EFFECT_ROUTING.md`.
Physical validation must establish the hardware path before hardware-dependent
behavior can be accepted. Only close M2 after those gaps pass. The user subsequently
authorized continuing local work before final physical validation. M3 catalog
development may proceed with documented names and bank numbering; unknown categories
and expansion mappings remain explicit. See `PHASE_4_WAVE_BROWSER.md`.

## Header, icons and keyboard refinement

The editor now uses the title type scale for Patch identity and places section
navigation beside Play/Design/Expert at normal desktop width. Idle audition
controls and guidance share a row; narrow windows and active audition wrap.
The four Tone cards begin roughly 100 pixels higher at 1440×1040 than the
previous routing capture. Unused DIRECT destinations no longer reserve a row;
DIRECT and unknown routes still display their destination explicitly.

`XpIcon` supplies original, scalable line icons for navigation, A/B, undo/redo,
Tone power, envelope and keyboard headings. It uses Theme colors and Metrics
sizes and avoids platform font/emoji substitutions. The existing button and
panel-header controls consume it; no external icon dependency was added.
Tone enable, Solo and Mute now support Space/Enter with visible focus and retain
their existing model/history/live-audition behavior.

Reviewed captures (fixture/FakeXp60, no physical verification):

- `design/screenshots/phase4-editor-refined-windows.png` — 1440×1040.
- `design/screenshots/phase4-editor-refined-windows-minimum.png` — 1024×680.
- `design/screenshots/phase4-editor-refined-live-minimum.png` — simulated audition.

The four-Tone composition, colored routing, envelope and key-range panels
remain the anchors. Exact-value controls continue below the fold, accessible by
vertical scrolling. Hardware-truth exceptions for routing, wave identifiers and
raw units still apply. This is local visual/interaction evidence; M2 cannot close
until EFX slot mappings and physical hardware acceptance are established.

Validation: all 23 CTest suites pass; the final QML recheck reports 58 passing checks, including keyboard Tone switches and full envelope visibility at 1440x1040. Normal/minimum/live captures were reviewed. The code graph was refreshed (2,410 nodes / 5,051 edges), with the same partial-parser limitations noted above. No physical MIDI writes were performed.

## Hardware-session evidence tooling

The two remaining M2/M3 gaps — EFX Parameter 1-12 byte slots and the INT-A/INT-B
wave group mapping — are both blocked on the same deferred physical session, and
both produce the same artifact: a before capture, one deliberate front-panel
change, an after capture, and a statement of which Patch bytes moved. Doing that
by eye over 2,945 bytes per Patch is where a wrong slot assignment would enter
the codebase.

`tools/capture_diff.py` (research utility, not part of the runtime) performs the
comparison. It assembles both captures into address images through the existing
`syx_inspect` parser, then resolves every changed address against the same
transcribed Parameter Address Map that generates the C++ tables, via
`generate_patch_tables.parse_tables`. A reported parameter name is therefore a
row in `docs/protocol/XP60_PATCH_PARAMETER_MAP.md`, not a second transcription
that could drift from the generated tables. Multi-byte nibble parameters are
decoded and reported once with all their bytes; display text follows the same
rules as `ParameterDescriptor::formatDisplay`.

Nothing is normalized away. Addresses present in only one capture are reported
separately from value differences, so a partial capture cannot masquerade as an
unchanged parameter. Addresses outside a documented Patch region, offsets in the
gaps between documented blocks, Performance Part 10 (the untranscribed Rhythm
Setup) and raw values outside the transcribed range are each reported with the
reason. Captures with a bad checksum are reported and the tool exits non-zero,
because such a capture is not usable as evidence. `--markdown` prints evidence
rows in the shape `PHASE_4_EFFECT_ROUTING.md` asks for. Input may be a binary
`.syx` or hex text copied from the Devices screen's Protocol activity panel, so
a session needs no separate SysEx utility.

This does not establish any EFX slot or wave mapping. A changed byte is a
correlation; the procedures in `PHASE_4_EFFECT_ROUTING.md` and
`PHASE_4_WAVE_BROWSER.md` are still what turns one into evidence, and both still
require the physical instrument. What changes is that the session now has a
checkable instrument-independent reading of its own captures instead of hand
comparison, and that `HARDWARE_VALIDATION_XP60.md` step 8 gains a second reading
of the read-back that does not come from the verifier being tested.

Validation: `tools/capture_diff.py --self-test` is registered as the CTest
`tst_capture_diff` beside the two existing generator checks. It covers 7-bit,
nibble and ASCII parameters, Tone placement, User Patch numbering, coverage
differences, unresolved regions, block gaps, the Rhythm Setup part,
out-of-range values, hex-text input and checksum rejection. Run against the
real fixture, the tool reports zero changes for a file compared with itself and
names exactly the two edited parameters in a deliberately modified copy. No
production, presentation or QML code was changed, and no hardware writes were
performed.
