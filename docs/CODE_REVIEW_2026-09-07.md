# Deep review — every deliverable mapped, 2026-09-07

Not a surface audit. Every roadmap deliverable is listed and marked against what
the code actually does, and the findings below were each traced to a specific
line rather than inferred from a name.

Scope: 35,955 lines of C++ across 13 modules, 82 QML files, 55 test executables,
66 CTest targets (all passing; `tst_qml` cannot run under this container's
Qt 6.4).

---

## 1. Findings, worst first

### F1 — A single drag can destroy the entire undo history · **severe**

`PatchWorkspace::beginGesture()` exists for exactly this and its own comment
names the cases: *"a gesture is a run of edits the user experiences as one: a
knob drag, an envelope point being pulled."*

**It is called from one place in the application** —
`PatchEditorViewModel::beginSoundDnaGesture()`
(`PatchEditorViewModel.cpp:282`). The envelope drag, every knob and every fader
never bracket a gesture.

Without a gesture, `PatchWorkspace::commitEdit` reaches `pushUndo` on every
commit (`PatchWorkspace.cpp:83`), and `kUndoLimit` is **64**
(`PatchWorkspace.h:63`). A drag emitting more than 64 changing samples — under a
second of pointer movement — evicts every earlier step in the session. The
user's prior work does not become harder to reach; it becomes unreachable.

Not caught by tests: `tst_patch_editor` asserts undo/redo thoroughly but always
one edit at a time. Nothing drives a multi-sample drag and asserts it collapses
to one step.

**Fix:** bracket the gesture where the drag begins and ends — `moveEnvelopePoint`
needs a begin/end pair from QML's `DragHandler.active`, and so does every
`XpKnob`/`XpFader`. Then a test that drives 200 samples and asserts `canUndo()`
walks back exactly one step.

#### Resolved — see *F1 addendum* below

Two corrections to the finding as written. Gestures were declared in **two**
places, not one: `beginSoundDnaGesture()` and `beginEffectGesture()`. And the
effect one did not work — `endEffectGesture()` called `m_workspace.endGesture()`
unconditionally while `commitEdit` calls it on every commit, so a Sound DNA drag
was torn down by its own first commit. That is fixed too.

### F2 — The shipped envelope editor has the stickiness the new engine fixes, and does not use it · **high**

I need to correct something I told you. I said the envelope view "is not built
yet". **It is built**: `Controls/EnvelopeEditor.qml` (182 lines, `DragHandler`,
keyboard adjustment) wired to `PatchEditorViewModel::moveEnvelopePoint`
(`PatchEditorViewModel.cpp:1093`). I built `interaction::EnvelopeDrag` without
checking, so the tested engine sits beside a working-but-inferior path instead of
replacing it. That is my error, not a gap in the plan.

The existing path has two of the three defects the engine was written for:

- **Redraw from the quantised value.** `moveEnvelopePoint` writes the int;
  `envelopePoints()` (line 1022) then recomputes `x` from the stored times. The
  handle can only land on 128 positions — the sticky feel.
- **`total` recomputed every sample** (line 1110). The axis is normalized by the
  sum of the four times, so widening one stage narrows the others *while the
  drag is in progress* and the handle accelerates away from the pointer. This is
  the "fighting back" failure; `EnvelopeDrag` fixes the scale at the grab
  precisely to avoid it.
- No hysteresis, so a resting hand toggles the value across a boundary.

The value mapping itself is **correct** and I checked it rather than assuming:
`bipolar = m_section == Sound` is right because `Sound` is the Pitch envelope,
and Pitch levels really are the only bipolar ones (raw 0..126, display -63..+63).

**Fix:** route `moveEnvelopePoint` through `EnvelopeDrag` — begin on
`DragHandler.active`, feed samples, draw from `DragFrame::handleX/handleY`.

### F3 — Nothing built since Phase 9 has a consumer · **high, and it is the headline**

Mechanically verified by searching for each class outside its own module and
tests:

| Has no production consumer at all | Reached only by other engine code |
|---|---|
| `PatchStructure` / `StructuralQuery` | `PatchSimilarity`, `PatchSignature` |
| `LibraryDuplicateAnalysis` | `InstrumentSnapshot`, `RestorePlan` |
| `SnapshotRestore` | `SnapshotStore`, `SnapshotCapture` |
| `UserPerformanceWrite` | `CategorySuggester` |
| `ParameterDrag`, `LfoGeometry` | `EnvelopeGeometry`, `EnvelopeDrag` |
| `PatchComponentCopy`, `PatchVariation` | |

Zero QML references to any of them. The `interaction` module appears in no other
module's include graph.

This is not a defect in any one class — each is tested against real data. It is
the accurate shape of the project: **the engine is far ahead of the face.** Two
of these are user-visible capabilities that exist and cannot be reached at all:
persistent USER Performance write, and the whole snapshot/restore workflow.

### F4 — `RestorePlan.cpp:30` can throw from a lambda · **low**

`RolandAddress(b0, b1, 0x00, 0x00)` throws `std::invalid_argument` on any byte
with bit 7 set. Currently only reached with literals ≤ `0x11`, so it cannot fire
today, but it is the one remaining non-literal construction in the tree and the
same shape crashed a probe during development (`0x80` is not a legal address
byte — which is why the area bounds are computed by arithmetic). Prefer
`RolandAddress::fromValue`, which returns `std::optional`.

### F5 — Two coalescing mechanisms, one of them redundant · **informational**

I claimed `EnvelopeDrag`'s coalescing is what makes dragging safe for a
31250-baud link. `PatchTransfer::queueLivePreview` (`PatchTransfer.cpp:310`)
already coalesces — *"only the latest desired Patch survives while a verified
transfer is in flight"*. The engine's coalescing is a second line at a different
layer, which is fine, but the claim as I first made it overstated its necessity.

---

## 2. What was checked and found clean

- **Layering.** Every module's include graph respects
  `presentation → services → {library, sounddna, simulation} → xpmodel → {xp60,
  protocol} → {roland, midi, diagnostics}`. No upward dependency anywhere.
- **Dangling temporaries.** Swept for `for (x : Call(...).member())` and
  `f().begin(), f().end()`. The five hits all resolve to `const&` returns of
  static tables. (Two real instances of this class were found and fixed during
  development; both were in test code.)
- **Qt connection lifetime.** Every `connect` passes a context object; no
  three-argument lambda form anywhere.
- **Object ownership.** The three raw pointers in headers are either
  `QObject`-parented (`PatchEditorViewModel.cpp:57-58`) or non-owning and
  null-guarded (`DevicesViewModel`).
- **Undo bounded.** `kUndoLimit = 64` with `pop_front`; F1 is churn, not a leak.

---

## 3. Deliverable map

Legend: **✅** delivered and tested · **🔷** engine done, no UI · **⛔** blocked
· **⬜** not started

### Phase 1 — Protocol Foundation (20)
All ✅. C++20/CMake, Qt shell, QML boundary, `IMidiTransport`, libremidi,
discovery, open/close, SysEx send/receive, long SysEx, checksum, address/size
types, DT1, RQ1, parse/serialize, correlation, pacing/timeouts/cancellation,
raw+decoded logging, deterministic tests, Devices/Diagnostics screen.

### Phase 2 — Patch Model (18)
All ✅. Common, Tones 1–4, wave references, Pitch, Pitch Env, TVF, TVF Env, TVA,
TVA Env, LFO, controllers, structure, effects, parameter metadata, decoder,
encoder, golden fixture, presentation scaffolding.

### Phase 3 — Hardware round-trip
⛔ deferred to the connected session by your instruction. `DEVICE_ACCEPTANCE`
areas 1–3 closed read-only on 2026-09-04; area 4 needs the instrument's own
display read at capture time.

### Phase 4 — Patch Editor + Wave Browser (21)
✅ ×20: design system, catalog components, Current Patch header,
Play/Design/Expert, four-Tone architecture and mixer, Solo/Mute, wave browser,
Pitch/TVF/TVA editors, **graphical envelope editors** (see F1/F2), LFO editor,
effects editor, key/velocity range, signal-flow, Expert view, A/B foundation,
safe real-time updates, state distinctions.
⛔ ×1: screenshot review (Qt 6.5+).

### Phase 5 — Librarian + Dashboard (18)
✅ ×18. Import/export, per-Patch, persistence, raw SysEx preserved, provenance,
search, tags/categories, favourites, ratings, source tracking, virtualized
models, progress/cancellation, duplicate foundation, Dashboard, hero, Tone
contribution, summary cards, quick actions.
⛔ screenshot review.

### Phase 6 — Bank Management (17)
✅ ×15. 128-slot workspace, section rail, drag/drop, keyboard alternatives,
multi-select, replace/insert/copy/delete, undo/redo, duplicate warnings,
compatibility warnings, provenance display, comparison inspector, bank
import/export, bank fetch/send, read-back verification, mismatch/retry.
⚠️ **transfer pacing controls** — pacing exists in `DeviceSession` and is
adjustable in code, but no QML surfaces it (`grep -r Pacing qml` → nothing).
Either wire it or drop the deliverable.
⛔ screenshot review.

### Phase 7 — Expansion Intelligence (10)
All ✅. Profile, Expansion Manager, SR-JV metadata (328 wave names from Roland's
own PDFs), group→board mapping, compatibility analysis, per-Tone, missing-board
warnings, Find Replacement / Disable Tone / Keep Anyway, Wave Browser
integration, Library/Bank filters.

### Phase 8 — Complete XP-60 Editing (9)
✅ ×2: Performance editor, 16-Part mixer.
🔷 ×3: snapshots, restore workflows, safety snapshot — `InstrumentSnapshot`,
`SnapshotStore`, `SnapshotCapture`, `RestorePlan`, `SnapshotRestore`, all tested,
**none reachable** (F3).
🔷 ×1: persistent USER Performance write — `UserPerformanceWrite`, tested, no UI.
⬜ ×2: **Rhythm editor**, **System editor** — tables transcribed and generated,
no view model, no screen, no nav entry.
⬜ ×1: shared transfer/verification UI.

### Phase 9 — Library Intelligence (12)
✅ ×3: fingerprints, exact duplicates, explainable diff (all reachable).
🔷 ×5: near-duplicate similarity, categorization assistance, structural search,
source/bank cross-analysis, Patch DNA (`sounddna` *is* wired to
`PatchEditorViewModel`, but publishes no dimensions until its gates pass).
✅ ×1: compatibility filters.
⬜ ×3: full Compare screen (nav says "Coming soon" — honest), Bank Builder
analysis summaries, Dashboard intelligence summaries.

### Phase 10 — Advanced Sound Design (8)
🔷 ×2: component copy, constrained variation.
⬜ ×6: mutation/evolution, morphing, advanced A/B, richer version history,
musical transformations (deliberately withheld — no justified mappings exist),
sound-design surfaces.

### Phase 11 — Live Mode (9) · ⬜ all
### Phase 12 — Audio Intelligence (7) · ⛔ all (needs capture hardware)
### Phase 13 — Other XP/JV hardware · ⬜ (correctly gated on XP-60 verification)

**Totals: 149 deliverables — 96 delivered, 11 engine-only, 26 not started, 16
blocked.**

---

## 4. The three walls, unchanged

| Wall | Behind it |
|---|---|
| A connected XP-60 | `DEVICE_ACCEPTANCE` 4, 6–8, 11–19 |
| Qt 6.5+ | every screenshot review; every screen not yet written |
| A research corpus | every `sounddna` dimension |

---

## 5. Recommended order

1. **F1.** A drag that destroys undo history is worse than a missing feature.
   Small fix, needs a test that drives a real gesture.
2. **F2.** Route the existing envelope view through the tested engine and delete
   the duplicate path.
3. **F3, selectively.** Snapshot/restore and USER Performance write are finished
   capabilities nobody can reach; a plain surface for each is worth more than any
   new engine.
4. Rhythm and System editors, then the Compare screen.
5. Phase 11 Live Mode's data layer — the largest remaining body of testable
   non-UI work.

Items 2–4 need Qt 6.5+ to see. Item 1 does not.

---

## 6. F1 addendum — what was actually done

The finding said to bracket every drag from QML. That is the exact fix, and it
is also the fragile one: it makes undo integrity depend on 21 call sites across
five QML files remembering to bracket, in a layer this container's Qt 6.4 cannot
run. A control that forgets does not degrade — it erases the session's history.

So the grouping is now inferred as well as declared, and the inference is what
carries the safety:

- **`PatchWorkspace::commit` takes a coalescing key.** Consecutive commits with
  the same non-empty key, arriving within `kCoalesceWindowMs` (500 ms), are one
  undo step: the first pushes, the rest amend. `breakCoalescing()` ends a run
  early, and is called from `adopt`, `clear`, `undo`, `redo`, `revert` and
  `beginGesture`. A `setClockForTesting` seam makes the window deterministic.
- **The key names the adjustment, not the parameter.** A knob is
  `tone:<n>:<parameter>`; an envelope point is `envelope:<section>:<tone>:<point>`
  and is shared by the stage *time* and *level* the drag writes together.
  Keyed per parameter they would alternate and each break the other's run —
  which is exactly the failure `anEnvelopeDragIsOneStepThoughItWritesATimeAndALevel`
  pins.
- **The time window is what keeps it honest.** Two deliberate edits of the same
  knob a moment apart stay two steps. Only genuinely continuous movement merges.
- **`beginEditGesture()` / `endEditGesture()`** are on `PatchEditorViewModel` for
  the view to bracket exactly when it can — nestable, and independent of the
  window, so a musician who pauses mid-drag still gets one step. Wiring them
  into `EnvelopeEditor.qml` and the knobs stays an open UI task; it is now an
  improvement in precision rather than a correctness requirement.

Six tests were added to `tst_patch_editor` — a 200-sample knob drag that must
leave three earlier edits reachable, a 200-sample envelope drag writing two
parameters, two consecutive point drags staying distinct, a declared gesture
grouping a drag however slow, a declared gesture returning to its origin
recording nothing, and nested brackets counting once. Three existing tests
encoded per-commit granularity and now say what they mean by advancing the
clock between deliberate edits.

66/66 CTest targets pass (`tst_qml` still cannot run under Qt 6.4).

---

## 7. F3 addendum — the two capabilities nobody could reach

Of the twelve classes in F3's table, two were user-visible capabilities that
existed and had no way in at all. Both now have a view model, tested against the
loopback instrument and real files; the QML screens over them stay deferred.

- **`presentation::BackupViewModel`** sequences capture → save → load → plan →
  arm → restore. Nineteen tests: a cancelled capture is kept but says it did not
  finish, a save refuses to overwrite without being told, a `.syx` with no
  manifest is not listed as a snapshot, a plan reports coverage in slots,
  loading a snapshot drops the plan built from the previous one, and a restore
  writes a *loadable* safety snapshot to disk before any byte goes out.
- **`presentation::PerformanceViewModel`** gained the persistent USER
  Performance write beside its audition send. Its class comment said the write
  "is not implemented yet"; that had been untrue since `UserPerformanceWrite`
  landed, and is corrected. Three tests: refused offline and unarmed, the plan
  names the destination before anything is armed, and a connected write verifies
  and is put back by its own snapshot.

The rest of F3's table is engine reached only by other engine code, which is
what a library layer is supposed to look like. What was wrong was not that
`PatchSimilarity` has no QML reference — it is that a musician could not take a
backup.

---

## 8. Addendum (2026-09-08) — Phase 12 removed, not just deferred

§3's `Phase 12 — Audio Intelligence (7) · ⛔ all (needs capture hardware)`
line is superseded: the phase has been removed from `ROADMAP.md`, not merely
left blocked. On review, the deliverables — captured previews, sound-aware
search and similarity built from recordings — do not have hardware as their
real obstacle. A recording is one instant of the XP-60 (one note, one
velocity, one effects state); the instrument's actual sound is produced
live, continuously, by envelopes, LFOs, and key/velocity tracking. A library
of recordings cannot represent that, connected instrument or not, so
building it would misrepresent the instrument rather than merely wait on
hardware. See `ROADMAP.md` "Rejected scope — Audio Intelligence" for the
full reasoning.

The totals in §3 (149 deliverables) predate this removal and are not
restated here; Phase 12's 7 no longer count toward "not started" or
"blocked" in any future tally, and what was Phase 13 is renumbered Phase 12.
