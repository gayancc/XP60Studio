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
