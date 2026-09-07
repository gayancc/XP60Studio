# XP60Studio Roadmap

The phases below are intentionally ordered. Do not skip foundational phases merely because later features are visually more interesting.

For every phase containing UI work, read and follow:

- `design/xp60studio-ui-master-mockup.jpg`
- `design/UI_DESIGN_REFERENCE.md`
- `design/UI_IMPLEMENTATION_ARCHITECTURE.md`
- `design/COMPONENT_CATALOG.md`
- `design/SCREEN_AND_FEATURE_MAP.md`
- `design/UI_ACCEPTANCE_CRITERIA.md`

The four anchor screens shown in the master mockup must be implemented to closely match that mockup when their roadmap phase begins.

---

# Cross-cutting — Demo Mode (Simulation)

Status: **added** (2026-09-04). See [`DEMO_MODE.md`](DEMO_MODE.md).

Goal: let musicians and reviewers experience every *shipped* workflow without a physical XP-60 or OS MIDI ports.

Entry:

- **In-app (primary):** Devices → **Enter Demo Mode** / header **Exit Demo** (preference saved; app relaunches into the chosen mode)
- Developer overrides: `XP60Studio --demo` or `XP60STUDIO_DEMO=1` (`XP60STUDIO_DEMO=0` forces live)

Behavior:

- Loopback MIDI transport with named demo ports (no libremidi / no OS MIDI)
- In-process `SimulatedXp60` answering RQ1 and accepting DT1
- Continuous reply pump on the Qt event loop
- Auto-connect + seed temporary Patch from the embedded bank fixture
- Shell shows **DEMO MODE** and **XP-60 SIM** (never unqualified LIVE)

Simulated today: Devices connection/health/fetch/transfer, Patch Editor (Play/Design/Expert), Wave Browser, write/verify/live audition against the simulator.

Deferred with their phases: Dashboard, Library, Bank Builder, and other unfinished nav destinations stay unavailable — do not invent fake Dashboard actions. Extend Demo Mode when those screens ship.

Success condition: launch in Demo Mode, edit a seeded patch, and run a simulated write/verify without opening real MIDI ports.

---

# Phase 1 — Protocol Foundation + Application Shell

Goal: establish a trustworthy C++/Qt application foundation, MIDI transport, and Roland SysEx infrastructure.

Deliverables:

- C++20 + CMake project foundation
- Qt 6.11.x application shell
- minimal Qt Quick/QML shell and design tokens sufficient for diagnostics
- explicit C++ presentation/QML boundary
- `IMidiTransport` abstraction
- libremidi 5.x integration behind that abstraction
- MIDI input/output discovery
- open/close MIDI endpoints
- SysEx send/receive
- long SysEx handling
- Roland checksum
- Roland address/size types
- DT1 support
- RQ1 support
- message parsing/serialization
- request/response correlation foundation
- pacing/timeouts/cancellation foundation
- raw/decoded diagnostic logging
- deterministic protocol tests
- minimal Devices/Diagnostics screen only

Do not build the polished Dashboard/Patch Editor/Wave Browser/Bank Builder in Phase 1.

Success condition:

```text
Detect MIDI ports
        ↓
Open MIDI IN/OUT
        ↓
Construct valid Roland SysEx
        ↓
Calculate / verify checksum
        ↓
Send SysEx
        ↓
Receive SysEx
        ↓
Parse messages
        ↓
Correlate expected response
        ↓
Report useful diagnostics
```

See `PHASE_1_PROTOCOL_FOUNDATION.md`.

---

# Phase 2 — XP-60 Patch Model

Goal: decode and encode XP-60 Patch data accurately.

Deliverables:

- Patch Common model
- Tone 1–4 models
- Wave references
- Pitch
- Pitch Envelope
- TVF
- TVF Envelope
- TVA
- TVA Envelope
- LFO
- controller/modulation behavior
- structure settings
- effects
- parameter metadata
- Patch decoder
- Patch encoder
- golden fixture tests
- presentation-model scaffolding only where useful for inspection/testing

Success condition: known-good Patch fixtures round-trip without unexplained differences.

---

# Phase 3 — Physical Hardware Round-Trip Validation

Goal: prove the model and transfer logic against an actual XP-60.

Required flow:

```text
FETCH
  ↓
DECODE
  ↓
ENCODE
  ↓
SEND
  ↓
FETCH AGAIN
  ↓
COMPARE
```

Resolve all unexplained differences before calling the Patch codec mature.

This phase requires clear hardware test scripts when Codex cannot access the physical XP-60 directly.

Execution update (2026-09-04): physical validation for this and every later
phase is deferred to one final connected session. `DEVICE_ACCEPTANCE.md` indexes
every deferred check by functional area and links each to the document that
owns its steps. Local work continues; no hardware fact is promoted meanwhile.

UI remains diagnostic/inspection focused.

---

# Phase 4 — Visual Patch Editor + Wave Browser

Execution update (2026-09-04): the user deferred physical validation to final
device acceptance and authorized remaining local work. M3 catalog development
can proceed while M2 EFX mappings and hardware checks remain open. See
`PHASE_4_WAVE_BROWSER.md`; this does not promote any hardware behavior to verified.

Goal: implement the first polished product surfaces and make Patch editing musically understandable.

Visual targets:

- **M2** — top-right Patch Editor / Four-Tone Mixer in `design/xp60studio-ui-master-mockup.jpg`
- **M3** — bottom-left Wave Browser in the same mockup

Deliverables:

- production XP60Studio design-system foundation
- reusable components from `design/COMPONENT_CATALOG.md`
- Current Patch header
- Play / Design / Expert disclosure model
- visual four-Tone architecture
- four-Tone mixer matching M2
- Tone Solo/Mute
- waveform browser matching M3
- Pitch editor
- TVF editor
- TVA editor
- graphical envelope editors
- LFO editor
- effects editor
- key/velocity range controls
- signal-flow visualization
- exact-value Expert view
- A/B original/current audition foundation
- safe real-time parameter updates
- local/dirty/hardware state visual distinctions
- screenshot review against the approved mockup

Do not send excessive MIDI while dragging controls; use throttling/coalescing.

Acceptance: follow `design/UI_ACCEPTANCE_CRITERIA.md`.

---

# Phase 5 — Librarian + Dashboard

Execution update (2026-09-04): the librarian half is complete and tested
against the real bank fixture — patch fingerprints, provenance, library entries,
`.syx` import and export, local persistence with search over SQLite, the
observable import service with progress and cancellation, and the virtualized
Library screen. Import/export actions on that screen, the M1 Dashboard and its
screenshot review remain open. Phase 4's outstanding items are all
blocked on the deferred physical session and are not superseded by this.
See `PHASE_5_LIBRARIAN.md`.

Goal: make SysEx collections searchable/manageable and introduce the polished command center once meaningful data exists.

Visual target:

- **M1** — top-left Dashboard / Command Center in the master mockup
- Library screen inherits M3/M4 design language

Deliverables:

- `.syx` import/export
- individual Patch import/export
- local persistence
- original raw SysEx preservation
- provenance
- search
- tags/categories
- favourites
- user ratings
- Patch source tracking
- virtualized Library result models
- import progress/cancellation
- initial exact-duplicate foundation if appropriate to import workflow
- polished Dashboard matching M1
- current patch hero
- four-Tone contribution
- Library / Bank / Device summary cards
- working quick actions appropriate to implemented features

The library must remain responsive with thousands of patches.

Do not add fake Dashboard actions for future phases.

---

# Phase 6 — Bank Management

Execution update (2026-09-05): the 128-slot workspace, its panel interaction,
drag/keyboard operation, undo/redo, saved banks and audition are in; bank `.syx`
import/export closes the file half; and the instrument half is now closed too.
A bank can be **read** off the XP-60 (`services::UserBankRead`, RQ1 only, whole
bank into the library as one source and arranged at the slots it came from) and
**written** into its permanent USER memory (`services::UserMemoryWrite`, armed
separately, every destination read before it is written, every write verified by
read-back, and Put back what was there as the undo). See
`design/BANK_BUILDER.md` and `PATCH_SYNCHRONIZATION.md` §7.

Duplicate warnings and the mini comparison inspector are in too — both built on
`library::PatchFingerprint` and `xpmodel::Xp60PatchDiff`, which already existed.

Multi-select is in: a marked set kept separate from the panel's own selection,
with `Space`/`Delete`/`Escape` and a bulk clear that is one undo step.

Mismatch inspection and retry are in: a run stops at the destination that did
not take, names User Memory Protect as the likely cause, and can be resumed from
there with the backup intact.

The user-defined section rail is in as well, with schema version 3 carrying it.

Everything in this phase is implemented except the **screenshot review against
the master mockup**, which needs the QML screenshot harness. That harness does
not run on the Qt 6.4 available in the current development container — the
screens use `Layout.horizontalStretchFactor`, which is Qt 6.5+, and the project
baseline is 6.11 — so the review is owed from a build on the baseline Qt.

Goal: make the 128-slot User Patch bank easy and safe to engineer.

Visual target:

- **M4** — bottom-right Bank Builder / Library Intelligence in the master mockup

Deliverables:

- 128-slot visual bank workspace matching M4
- user-defined section/category rail
- drag/drop reorder
- keyboard/menu alternatives to drag/drop
- multi-select
- replace/insert/copy/delete
- undo/redo
- duplicate warnings where data exists
- compatibility warning placeholders only if backed by verified metadata
- source/provenance display
- mini comparison inspector
- bank import/export
- bank fetch/send
- transfer pacing controls
- read-back verification where supported
- mismatch inspection/retry
- screenshot review against the approved mockup

Do not reduce the primary bank experience to a plain table.

---

# Phase 7 — Expansion Intelligence

Goal: understand whether imported patches are playable on the user's physical XP-60 configuration.

Deliverables:

- EXP-A/B/C/D profile
- Expansion Manager UI using the shared design system
- SR-JV board metadata
- waveform-to-board mapping
- patch compatibility analysis
- per-Tone compatibility
- missing-board warnings
- explicit Find Replacement / Disable Tone / Keep Anyway workflows
- Wave Browser compatibility integration
- Library/Bank compatibility filters and warnings

Never silently replace missing waves.

Execution update (2026-09-05): every deliverable built.

The EXP-A/B/C/D profile, SR-JV board metadata, waveform-to-board mapping, per-Tone
and per-Patch compatibility analysis, the Expansion Manager, Wave Browser
integration, Library and Bank compatibility filters and warnings, and the Find
Replacement / Disable Tone / Keep Anyway workflows are all in. See
`design/EXPANSION_INTELLIGENCE.md`.

On the mapping: a Tone names an expansion wave by Wave Group ID, and XP60Studio
reads that ID as the SR-JV80 board of that number — `library::ExpansionBoardCatalog`
carries the series (01–19 and 96–99). Roland documents no such mapping, so it is
an inference; it is labelled as one, a group **learned** from the musician's own
instrument overrides it, and it never concludes that an instrument *has* a board.
That is still only what the musician declared, and every verdict stays
three-valued so "cannot tell" remains sayable.
`protocol/ROLAND_XP60_PROTOCOL_FACTS.md` §7 sets out the evidence — including the
earlier, mistaken refusal of this mapping — and `DEVICE_ACCEPTANCE.md` area 15
records what would confirm it outright.

---

# Phase 8 — Complete XP-60 Editing

Goal: reach broad editor/librarian completeness.

Deliverables:

- Performance editor using the mixer/workstation visual language from `SCREEN_AND_FEATURE_MAP.md`
- visual 16-Part mixer
- Rhythm editor using keyboard/drum-map interaction
- supported System data
- snapshots
- restore workflows
- safety snapshot before destructive restore
- shared transfer/verification UI
- complete major XP-60 editor/librarian baseline

Execution update (2026-09-06): started with the Performance model, the
foundation the Performance editor and 16-Part mixer rest on.

The Performance Address Map was never transcribed — the layout was carried as
"observed in the fixture only" — so that came first
(`protocol/XP60_PERFORMANCE_PARAMETER_MAP.md`), and the model follows the Patch
pattern exactly: `tools/generate_performance_tables.py` turns the document into
`generated/Xp60PerformanceTables.{h,cpp}`, and `Xp60PerformanceLayout`,
`Xp60Performance` and `Xp60PerformanceCodec` sit on top. The generator refuses a
document whose rows do not tile each block exactly, so a dropped row or a
mistyped offset fails the build rather than silently shifting every later byte.

Three independent checks pass. The block offsets and sizes span **3993 bytes**,
which is exactly the size in Roland's own published RQ1 example for the
Temporary Performance (`00 00 1F 19`). All 32 Performances in the golden fixture
decode with no range warnings. And every one of them round-trips byte for byte
through the codec.

The Performance editor and its **16-Part mixer** are in on top of that:
`PerformanceViewModel` owns the working Performance with its own undo history —
separate from `PatchWorkspace`, because a Performance names sixteen Patches and
opening a Part's Patch must not destroy the Performance being edited — and
`PerformanceScreen` draws it as sixteen channel strips with a Part inspector.
Reading a Performance off the instrument reuses the Patch fetch machinery: the
block state machine is now layout-agnostic, so the hardware pacing lesson
(§2.3, one request at a time) applies to seventeen blocks as it did to five.

**Send writes the temporary Performance only** (`01 00 00 00`) — audition.
Writing a USER Performance is a persistent write and is not offered yet: it
needs the read-before-write, verify-by-read-back and restore machinery
`services::UserMemoryWrite` provides for Patches. Keeping the two apart by
address is `PATCH_SYNCHRONIZATION.md` §7.

**Rhythm Setup is transcribed** (`protocol/XP60_RHYTHM_PARAMETER_MAP.md`) and
generated the same way: Common is 12 bytes of name, a Note is 58, and a Note's
offset *is* its MIDI key number — Key# 35 at `23 00` because 0x23 is 35. Both
User Rhythm Setups in the golden fixture decode with no range warnings and all
128 Notes round-trip byte for byte. The Rhythm *editor* is not built yet.

**System Common and Scale Tune are transcribed too**
(`protocol/XP60_SYSTEM_PARAMETER_MAP.md`), which completes the Parameter Address
Map: all four regions are now machine-checkable sources with generated tables.
Two things that document turned up worth knowing — there are **seventeen** Scale
Tune blocks (one per Performance Part plus one for Patch mode), not the single
global one it would be easy to assume; and several rows print an enumeration
that is *not* one label per raw value (the controller assignments name 64
selectable CCs across a 97-value field), so those carry no labels rather than
mis-naming every value.

The System tables are the least corroborated of the four: the golden fixture is
a User bank dump and carries no System data, so there is no local cross-check.
`DEVICE_ACCEPTANCE.md` area 19 is what would settle them, and nothing ships on
them yet.

**Snapshots and restore workflows are in.** `library::InstrumentSnapshot` keeps
the instrument's own DT1 bytes verbatim rather than a re-encoding of them, so a
snapshot restores byte for byte even for regions this application has no model
for, and a later version that learns to decode more does not invalidate one
taken before it. `services::SnapshotStore` saves it as a plain `.syx` with a
JSON manifest beside it — the backup is restorable by any other librarian, or by
`amidi`, or by a build of this application that no longer exists; a backup
readable only by the program that wrote it is a worse backup. The manifest adds
provenance and a SHA-256, and losing it costs those, not the data. A digest
mismatch on load is reported, never enforced silently.

`services::RestorePlan` is a separate, inspectable step rather than an argument
to a write call, because a restore is the most destructive thing the application
can do. It enforces three rules: an area is restored whole or not at all and
says so when it cannot be; nothing outside the requested areas is written even
when the snapshot contains it; and every plan that writes anything requires a
safety snapshot first — the snapshot being restored is not the safety net, it is
what will replace what is there now. Coverage is counted in the area's own units
(Patch slots, Performance slots) rather than bytes, because Roland leaves large
gaps of unused address space between slots and a byte percentage would report a
complete bank as a small fraction of itself. Where no block layout exists —
Rhythm Setups and System — completeness is reported as *unknown* rather than
assumed.

Working on this corrected a long-standing misreading of the golden fixture.
`user-bank-amal.syx` is not a Patch bank: it is a dump of the whole **user
memory** — 32 Performances, 2 Rhythm Setups and 128 Patches, 1314 messages. The
58-byte and 12-byte blocks its README recorded as "meaning not yet established"
are the Rhythm Setup Notes and Commons, identified by the transcriptions done
earlier in this phase. The README has been corrected.

**Persistent USER Performance write is in** (`services::UserPerformanceWrite`),
carrying the same safety envelope as the Patch writer: nothing is written to a
destination that has not first been read and kept, every write is verified by
reading it back and comparing, the snapshots are the undo, and arming is
separate and single-use. It is a separate class from `UserMemoryWrite` rather
than a mode of it, deliberately: merging them would save perhaps two hundred
lines and cost the property that matters most about both — each hard-codes the
region it can reach and **cannot be pointed at the other**, whatever a caller
passes. In the one place where a bug overwrites permanent memory, a wrong
argument being unrepresentable is worth more than the duplication. Its
confirmation text also names the trap peculiar to Performances: a Performance
names sixteen Patches by bank and number but does not carry them, so writing one
does not bring its sounds with it.

**Taking a snapshot from the instrument is in** (`services::SnapshotCapture`),
which is what makes the destructive half usable: every restore plan requires a
safety snapshot, and this is what takes it. It is RQ1-only and has no send path,
so it cannot alter the instrument even if it is wrong, and it keeps the DT1
bytes the XP-60 actually sent (`PatchFetchStatus::originalSysEx`) rather than a
re-encoding of the decoded model — which is what makes the result a backup
rather than a recreation.

It reads User Patches and User Performances, the two areas with a documented
block layout and a fetch plan. Rhythm Setups and System have neither yet, so
asking for them is **refused by name, and refuses the whole request** rather
than quietly returning a snapshot missing an area the user asked for. A run that
ends early — cancelled, disconnected, a read that failed — keeps what arrived,
and `RestorePlan` then reports that snapshot as partial rather than complete.

**Sending a restore is in** (`services::SnapshotRestore`), which completes the
workflow: capture → save → plan → restore → verify. Three rules it enforces
rather than documents:

- **The safety snapshot is saved to disk before a single byte goes out.** Saved,
  not held: a backup that dies with the process is not a backup, and the moment
  it is needed is usually the moment something has gone wrong. If the capture
  fails or the file cannot be written, the restore aborts having written
  nothing. There is no flag to skip it.
- **A plan whose areas cannot be captured is refused.** Without a safety
  snapshot there is no undo, so restoring an area this build cannot read is
  refused rather than performed unprotected — which is why Rhythm Setups, which
  a plan *can* restore, cannot be restored yet.
- **Everything written is read back and compared, address by address**, against
  what the plan intended and nothing else. A restore that did not take is a
  mismatch, never a success, and the message names User Memory Protect as the
  likely cause and says where the previous contents are.

Cancel is honoured only before the send begins. Once DT1s are going out,
stopping half-way would leave the instrument holding a mixture of two states —
the one outcome the whole design avoids — so the run is seen through and
verified, and the safety snapshot is the way back.

**Both are reachable now.** `presentation::BackupViewModel` sequences the whole
backup workflow — capture → save → load → plan → arm → restore — holding the
state between steps and converting Roland types into rows QML can display. It
adds no safety rules of its own; what it enforces is sequencing: a plan is
dropped whenever the snapshot it describes is replaced or forgotten, arming
without a plan is refused, and an area this build does not know is refused
outright rather than dropped from the request. Areas that cannot be captured are
named in `unreadableAreaNote` rather than left off a list the user reads as
complete. `presentation::PerformanceViewModel` surfaces the persistent USER
Performance write beside the audition send it already had — arm, write to a
slot, restore what was overwritten, retry what did not land — refusing to start
one while a fetch or an audition send is still in flight.

Still to come in this phase: the Rhythm and System editors on top of those
tables, the shared transfer/verification UI, and the QML screens over the two
view models above (deferred with the other screen work — they need Qt 6.5+ and
could be neither run nor looked at here).

Hardware verification for everything Phase 8 has built so far is open as
`DEVICE_ACCEPTANCE.md` areas 16–18.

At the end of this phase XP60Studio should cover the major practical capabilities expected from a mature XP-60 editor.

---

# Phase 9 — Library Intelligence + Full Compare

Goal: make large libraries understandable.

Deliverables:

- exact parameter fingerprints
- exact duplicate detection
- near-duplicate similarity
- explainable patch diff
- full Compare screen
- automatic categorization assistance
- Patch DNA
- structural search
- compatibility filters
- source/bank cross-analysis
- Bank Builder analysis summaries
- Dashboard intelligence summaries only where useful

Derived metadata must remain separate from Roland hardware data.

## Status

Done, with tests:

- exact parameter fingerprints (`PatchFingerprint`, Phase 5)
- exact duplicate detection (`LibraryDatabase::findDuplicatesOf`, Phase 5)
- explainable patch diff (`Xp60PatchDiff`, Phase 4)
- near-duplicate similarity (`PatchSimilarity`, `PatchSignature`)
- source/bank cross-analysis and library-wide duplicate sweeps
  (`LibraryDuplicateAnalysis`)
- compatibility filters (`LibraryQuery::Expansion`, Phase 7)
- structural search (`PatchStructure`, `StructuralQuery`)
- automatic categorization assistance (`CategorySuggester`) — the user's
  own labels carried along measured similarity, with the neighbours the
  suggestion came from and the dissenters among them. There is no
  category byte in the Parameter Address Map, a Patch name is text
  somebody typed, and structure alone cannot separate a pad from a bass,
  so this is the only form of it the project can honestly offer. It
  refuses on a tie or on too little agreement, and never writes.
- Patch DNA — the evidence-gated `sounddna` engine. Implemented and
  tested; it publishes no dimensions until a reviewed model passes the
  gates in `SOUND_DNA_ENGINE.md`, which needs a research corpus that does
  not exist yet.

### Open — screen work held back deliberately

The **full Compare screen**, a duplicates view over the sweep report, and
the **Bank Builder / Dashboard intelligence summaries** are all QML
surfaces. They are deferred rather than written, for the same reason the
Phase 4–6 screenshot reviews are still open: this container has Qt 6.4,
the screens need 6.5+, and `tst_qml` cannot run. Writing screens that
cannot be executed or looked at would produce code whose only evidence of
correctness is that it compiles.

The C++ underneath them is finished and tested, so each is a
presentation-layer job when a Qt 6.5+ environment is available:

- Compare screen → a view model over `PatchSimilarity` (per-block scores,
  the parameter diff, `summary()`).
- Duplicates view → a view model over `LibraryDuplicateReport`, including
  its `truncated` / `minimumPercent` honesty fields.
- Bank Builder and Dashboard summaries → the same report, narrowed by
  `LibraryQuery`.
- Gesture brackets on drags → `PatchEditorViewModel::beginEditGesture()` /
  `endEditGesture()` exist and are nestable; `EnvelopeEditor.qml`'s
  `DragHandler.active` and every `XpKnob`/`XpFader` press should call them.
  This is a precision improvement, not a correctness fix: since 2026-09-07 the
  workspace infers the same grouping from the coalescing key every edit carries,
  so a control that never brackets still cannot flood the undo history. See
  `docs/CODE_REVIEW_2026-09-07.md` §6.

Nothing else in this phase is outstanding.

---

# Phase 10 — Advanced Sound Design

Goal: help users create new patches without losing expert control.

Deliverables:

- component copy between patches
- intelligent constrained variation
- mutation/evolution workflow
- morphing where parameter semantics permit
- advanced A/B
- richer version history
- musical transformations such as warmer/wider/brighter where deterministic parameter mappings are justified
- dedicated sound-design surfaces that inherit the Patch Editor design language

Do not interpolate discrete IDs blindly.

## Status

**Component copy between Patches is in** (`library::PatchComponentCopy`), the
first Phase 10 deliverable and item 7 of the long-term success standard.

The components are Roland's own groupings: each is exactly the set of
parameters the Parameter Address Map files under one category, so "copy the
filter" means the five TVF parameters the instrument itself calls TVF rather
than a selection somebody thought looked right. Two tests assert the partition
holds in both directions — nothing in the instrument's table is left out of
`WholeTone`, and nothing is listed that the table does not have — so a UI
showing what a copy will touch shows the same thing the copy does.

Three rules:

- **It never interpolates.** Every value is carried across exactly as stored.
  Wave numbers, filter types and controller destinations are identities rather
  than quantities, so the way this obeys the phase's "do not interpolate
  discrete IDs blindly" rule is by not interpolating at all.
- **It never copies the Patch name.** A Patch that took on the name of the one
  a filter came from is a Patch nobody could find again.
- **It applies whole or not at all**, on a working copy, so a refused copy
  leaves nothing half-changed.

The part that earns it its place is what it says stayed behind: a Structure
pairing that lives in Patch Common and cannot travel with a Tone, effect sends
that route into Patch-level settings the copy did not bring, a wave that needs
an expansion board, a Tone switch that came along and turned a Tone on. Nothing
is refused on those grounds — the user may know exactly what they are doing —
but nothing is silent either.

**Constrained variation is in** (`library::PatchVariation`). "Vary this Patch"
is where invention creeps into a synthesizer editor, so the constraints are the
design:

- **Nothing discrete is touched.** A parameter is left alone when it carries an
  enumeration, when its range is 0..1 (every switch), or when it belongs to the
  Wave category. Those hold identities, not quantities: Filter Type 3 is not "a
  bit more" than 2 and wave 118 is not "near" 119. `whyNotVaried()` gives the
  reason in words, so a UI can show it beside a control it will not move.
- **Nothing in a Tone that is switched off**, because changing what nobody
  hears is change without effect.
- **Never the Patch name.** A variation deserves a new name and choosing one is
  the musician's job.
- The amount is a fraction of each parameter's **own** documented range, so a
  coarse field and a fine one move by comparable musical amounts rather than by
  the same number of steps. Clamping at a range edge is counted and reported,
  not hidden.
- Deterministic from a seed, so a variation a musician liked can be reproduced.

It makes no claim about how the result sounds — it says which parameters moved
and by how much. Perceptual claims live in `sounddna`, which publishes none.

**The envelope interaction engine is in** (`src/interaction/`), which is the
hard half of the graphical envelope editors — the half that decides whether
dragging a point feels smooth or sticky. `EnvelopeGeometry` turns a Tone's
Pitch, Filter or Amplifier envelope into draggable points; `EnvelopeDrag` runs
the gesture. The four mechanisms and the reasoning are in
[`design/ENVELOPE_INTERACTION.md`](design/ENVELOPE_INTERACTION.md); in short, a
continuous shadow the view draws from, hysteresis at step boundaries, exact
accumulation from the grab origin, and coalescing to at most one message per
parameter per drain.

`ParameterDrag` generalises the same four mechanisms to every ordinary knob and
slider, and `LfoGeometry` draws the LFO on the same terms — cycles proportional
to Rate rather than an invented frequency axis, Delay and Fade shading the start
of the plot, and the waveform switched rather than blended because it is an
identity.

That work also found a real bug: the table generator's category rules match in
order and the `Wave` rule sat above `LFO1`/`LFO2`, so "LFO1 Waveform" — which
contains the word "Wave" — was filed under Wave. `PatchComponentCopy` had
therefore been making "copy the wave" change a Tone's LFO shapes and "copy
LFO 1" leave its own shape behind. Fixed, regenerated, and pinned by a test.

The QML surfaces on top are deferred with the other screen work: they need
Qt 6.5+ and could be neither run nor looked at here.

Still to come in this phase: mutation/evolution, morphing where parameter
semantics permit, advanced A/B, richer version history, and the sound-design
surfaces (QML, deferred with the other screen work).

---

# Phase 11 — Live Mode

Goal: support stage use.

Deliverables:

- setlists
- songs/sections
- Patch and Performance assignment
- large stage display
- next-sound preview
- safe program switching
- optional MIDI-triggered navigation
- persistent connection status
- destructive editing hidden/disabled in Live Mode

Live Mode must inherit the XP60Studio visual identity while using lower density for stage readability.

---

# Phase 12 — Audio Intelligence

Goal: make library auditioning and search richer without emulating the XP-60 synthesizer.

Potential deliverables:

- capture XP-60 audio output
- associate previews with patches
- standardized preview notes/velocities
- local preview playback
- audio-derived metadata research
- sound-aware search
- similarity based on actual recordings

Do not implement fake audio understanding without real data.

---

# Phase 13 — Additional Roland XP/JV Hardware

Goal: reuse verified abstractions for related devices.

Potential targets:

- XP-50
- XP-80
- XP-30
- JV-1080
- JV-2080

Extract common abstractions based on verified shared behavior.

XP-60 correctness must not be weakened for premature generic support.

---

# Long-Term Success Standard

A mature XP60Studio should let a musician:

1. connect an XP-60 reliably;
2. inspect and edit any major Patch component;
3. understand which Tone contributes what;
4. safely import years of old SysEx banks;
5. detect exact and near duplicates by real parameter content;
6. identify unsupported expansion dependencies;
7. compare and copy patch components visually;
8. create clean custom 128-patch banks;
9. transfer those banks with verifiable results;
10. back up and restore the keyboard confidently;
11. organize live-performance sounds;
12. retain complete expert access to the XP-60 underneath the simplified UX;
13. present the four approved anchor screens with the same composition and design system as the master mockup, except for documented justified deviations.
