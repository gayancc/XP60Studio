# Bank Builder — the Virtual XP-60 Patch Bank

Screen: `qml/XP60Studio/Screens/BankBuilderScreen.qml`, navigation key `banks`.

The Bank Builder is where a musician assembles a new 128-Patch User bank out of
whatever they already own — an imported bank, an old backup, a device read, a
saved custom bank — and it is designed to feel like operating the instrument
rather than editing a data structure.

## The mental model

The XP-60 does not ask for "patch 21". It asks for a PATCH GROUP, then a
subgroup, then BANK 1–8, then NUMBER 1–8:

```
USER  ->  A  ->  BANK 3  ->  NUMBER 5
```

The User group holds 128 Patches, which is two subgroups of 8 × 8:

| Panel | Linear | Panel | Linear |
|---|---|---|---|
| `A11` | `001` | `B11` | `065` |
| `A18` | `008` | `B18` | `072` |
| `A21` | `009` | `B21` | `073` |
| `A35` | `021` | `B35` | `085` |
| `A88` | `064` | `B88` | `128` |

Subgroup **A** is patches 001–064, subgroup **B** is 065–128. Within a
subgroup, BANK *b* NUMBER *n* is `(b − 1) × 8 + n`.

**The panel identity is primary everywhere in the UI; the linear 001–128 number
is always shown beside it as supporting information.** The musician never
calculates the mapping, and no screen re-derives it.

### This is a front-panel convention, not a protocol fact

The mapping lives in one place, `xpmodel::Xp60BankLocation`, and it is
explicitly documented there as a *selection convention of the front panel*. The
protocol identity of a User Patch is its linear number 001–128 — what
`library::PatchProvenance::userNumber` records, and what the User Patch bank
address `11 nn 00 00` is derived from
(`docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md`). Nothing in the Bank Builder
changes, invents or reorders an address.

`tests/cpp/tst_bank_location.cpp` checks the bijection exhaustively over all
128 destinations — every panel coordinate resolves to exactly one linear
number, every linear number round-trips back through the printed label, and
nothing outside the panel's range is clamped into range.

## Source and target are different kinds of object

| | SOURCE LIBRARY | TARGET XP BANK |
|---|---|---|
| What it is | where Patches come from | the new 128-position bank being built |
| Looks like | a flat list on a plain surface | an instrument panel with tactile buttons |
| Grouping | one import = one "source bank" | SUBGROUP / BANK / NUMBER |
| Model | `LibraryListModel` (virtualized) | `BankBuilderViewModel` over `library::BankDraft` |

A source bank is simply everything that arrived from one `.syx` file or one
device read, grouped by the digest its provenance recorded — so the grouping is
a fact about where the data came from, not a category anyone maintains. Any
source can supply any destination: a piano bank, an old backup and an imported
XP-50 bank can all feed the same new User bank.

The Bank Builder gets its **own** `LibraryListModel` instance
(`bankLibrary` in `Main.qml`). A source filter or search set while building a
bank must not silently change what the Library screen is showing.

## Layout

```
BANK BUILDER   [name] [MODIFIED]   undo redo | Saved banks  New  Save  Export bank  Save as new bank
┌── SOURCE LIBRARY ───┬── TARGET XP BANK ─────────────────────────────────────┐
│ source chips + fill │  ┌ recessed instrument display ──────────────────────┐│
│ search              │  │ USER  TARGET XP BANK                  [ASSIGNED]  ││
│ ┌─────────────────┐ │  │ A35   SUBGROUP A · BANK 3 · NUMBER 5    27 / 128  ││
│ │ patch rows with │ │  │       PATCH 021                            FILLED ││
│ │ drag grips      │ │  │ Warm Strings                                      ││
│ │ ...             │ │  │ from piano-bank.syx · USER:007                    ││
│ └─────────────────┘ │  └───────────────────────────────────────────────────┘│
│                     │   [last action]                  Audition   Clear     │
│                     │  ┌ panel plate ──────────────────────────────────────┐│
│                     │  │ SUBGROUP  [A][B]                                  ││
│                     │  │ BANK      [1][2][3][4][5][6][7][8]                ││
│                     │  │ NUMBER    [1][2][3][4][5][6][7][8]  ← drop targets││
│                     │  │  ┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐  eight tiles,   ││
│                     │  │  └──┘└──┘└──┘└──┘└──┘└──┘└──┘└──┘  same columns    ││
│                     │  │ BANK MAP  A [8 mini banks]  B [8 mini banks]      ││
│                     │  └───────────────────────────────────────────────────┘│
└─────────────────────┴───────────────────────────────────────────────────────┘
```

The eight destination tiles sit in the **same eight columns** as the NUMBER
buttons, joined by a short lit stem. Selecting NUMBER 5 lights the button and
its tile together, so a player sees one control rather than a button and a list
row that happen to correspond. Both are the same drop destination.

## Components

| File | Job |
|---|---|
| `Controls/XpHardwareButton.qml` | the tactile panel-button primitive: moulded cap, inverting bevel, 1 px press travel, illuminated selected state, status LED whose brightness follows occupancy, focus ring, and an armed ring for drop targeting |
| `Controls/BankPanelDisplay.qml` | the recessed instrument display: group, state, `A35`, `SUBGROUP A · BANK 3 · NUMBER 5`, `PATCH 021`, name, provenance, occupancy |
| `Controls/BankDestinationTile.qml` | one of the eight destinations; engraved NUMBER, occupancy LED, name, both identities, drop action badge, placement flash |
| `Controls/BankOverviewMap.qml` | all 128 destinations as 2 × 8 mini banks of eight cells; click or drag-dwell to move the panel there |
| `Controls/BankSourcePanel.qml` | the source library: source-bank chips, search, virtualized patch list with drag grips, Import, and *Arrange this bank from the source* |
| `Screens/BankBuilderScreen.qml` | composition, the drag layer, keyboard operation, save/open dialogs |

`XpHardwareButton` is deliberately **not** `XpButton` with a different fill. A
software button is a rectangle that changes colour; a panel button is a moulded
cap in a recess with a light behind it. The difference a player feels is depth
(a bevel that inverts and a cap that travels on press), light (a selected cap
is *lit*, and a separate LED carries occupancy), and speed (every transition is
one `Motion.durationFast` step — a panel button that fades over a quarter of a
second feels broken).

## The drag

One pointer gesture, owned by the item it started on, tracked in the screen's
own coordinate space, hit-tested against the real geometry of the buttons and
tiles. **There are no `DropArea`s** — nested drop areas under a moving item is
what made an earlier canvas drag flicker between targets. Three rules keep it
steady:

1. **Lift threshold** — a drag needs 6 px of movement, so a click on a filled
   destination still selects it instead of starting a one-pixel drag.
2. **Sticky targets** — an acquired destination is only given up once the
   pointer is clearly (22 px) outside both its tile and its NUMBER button, so a
   shaky hand near an edge does not flicker the drop in and out.
3. **Dwell navigation** — hovering BANK or SUBGROUP (or a block in the bank map)
   while dragging changes the visible eight only after a deliberate 420 ms
   dwell, so passing over BANK 5 on the way to NUMBER 3 never moves the panel
   out from under the drop.

What a drop will do is stated before it happens, in three places at once: the
instrument display previews the destination and the action, the target tile
carries a `PLACE` / `REPLACE` / `MOVE` / `SWAP` badge, and the ghost following
the pointer names the Patch, the action, `A35` and `PATCH 021`. The action word
comes from `BankBuilderViewModel::dropPreview` — the surface never decides for
itself whether a drop replaces something.

Feedback:

| Situation | What the user sees |
|---|---|
| picked up | source row or tile dims to 35 % |
| valid target under pointer | green ring on both the NUMBER button and the tile, action badge, display preview |
| every other destination | quiet accent candidate ring (not an error flash) |
| occupied target | `REPLACE` in warning colour, and the occupant is named |
| dropped successfully | one green pulse on the destination, and the action line says what happened |
| dropped nowhere | the ghost shakes rather than silently vanishing |
| long patch name | elided in the tile, full name on hover |

`stageDrag(patchId, name, slotIndex, fromSlot)` puts the surface into a mid-drag
state without a pointer. Tests and the documentation captures use it, so a
captured drag state is always one a real gesture can reach.

## Operating it without a mouse

The whole panel is keyboard-operable: `←`/`→` walk NUMBER, `↑`/`↓` walk BANK,
`1`–`8` jump to a NUMBER, `A`/`B` switch subgroup, `Delete` empties the selected
destination, and the platform Undo/Redo shortcuts walk the arrangement.
Every hardware button is tab-focusable with a visible focus ring.

## Rearranging, undo, and what is never destroyed

Dragging a filled destination onto another **moves** into an empty one and
**swaps** with an occupied one — either way nothing is lost, and either way it
is a single undo step. Undo is snapshot-based over the whole 128-position
arrangement (`library::BankDraft`), which makes a compound edit like a swap one
step by construction, and restores the saved/modified state as well as the
contents.

A bank is an arrangement of **references**. Placing a Patch never copies,
moves or rewrites it; the same Patch can fill four destinations and the library
still holds one Patch. Clearing a destination or deleting a saved bank never
deletes a Patch.

## Saving

`Save as new bank` writes all 128 positions exactly as arranged, empty
positions included, and the draft then *becomes* that saved bank so the
musician carries on from where they were. `Save` overwrites the bank the draft
was loaded from or last saved as. Saved banks are listed in the Saved banks
drawer with their occupancy and can be reopened, which makes them usable later
as a source for the existing export/transfer workflows
(`BankBuilderViewModel::arrangementIds()` returns the 128 library ids in slot
order, empty positions as `0`, so the caller decides what an empty destination
means).

### Missing Patches are never silently dropped

`bank_slots.patch_id` is `ON DELETE SET NULL`, not `CASCADE`: deleting a Patch
from the library must not delete the destination it occupied in somebody's
bank. The cached name survives, the destination reports itself as `MISSING` in
red, the bank header counts them, and the destination is never offered as free
space. Re-saving keeps the hole a hole rather than inventing a reference.

## Auditioning

Auditioning reuses `services::PatchTransfer` unchanged. The Patch is rebuilt
from its preserved original SysEx by the library and sent to the XP-60's
**temporary** Patch area, which is not saved across a power cycle; permanent
User memory is not reachable from this path at all. The same safety rules as
the Editor apply — a verified temporary-Patch read must have succeeded in the
session, arming is spent by one attempt, and the write is proved by reading it
back. When those conditions are not met the Audition button is disabled and its
tooltip says exactly why.

## Getting a bank in and out as a file

The Bank Builder reads and writes `.syx` through the same import and export
services the Library screen uses (`LibraryTransferViewModel`), so a bank
imported from either place is one source bank in both, and the result card is
the same card.

### Arranging a bank from a source — *Arrange this bank from the source*

An imported `.syx` already records which User slot every Patch came from, so a
bank that arrived as a bank can be laid out the way it arrived instead of being
carried across 128 destinations by hand. `BankBuilderViewModel::fillFromSource`
places every Patch of one source at the destination its provenance recorded.

It never guesses:

| Situation | What happens |
|---|---|
| the Patch records a User number | placed at that destination |
| the Patch records none (read from the temporary area, say) | **left unplaced** and counted; dropping it into the first free destination would be inventing provenance |
| two Patches claim one destination | the first is kept, the collision is counted, and the action line says so |
| a destination the source says nothing about | left exactly as it is, so filling from a second source adds to the bank |

The whole fill is **one undo step** (`BankDraft::assignAll`) — undoing it puts
the entire arrangement back, rather than removing one of 128 placements at a
time. The action is offered only while a single source bank is open: "fill from
all sources" has no arrangement to reproduce.

### Exporting the bank — *Export bank*

`LibraryTransferViewModel::exportBankArrangement` writes the arrangement, not
the library. Each Patch is addressed to the User slot it occupies **here**,
using the `SyxExportTarget::Kind::UserBankSlots` target — one explicit
destination per Patch:

* a built bank has holes in it, and consecutive addressing (`UserBankFrom`)
  would close them, silently moving a musician's Patches to destinations they
  did not choose;
* an **empty destination writes nothing at all**, not a blank Patch. Whether a
  gap means "erase whatever the instrument holds there" is not this
  application's decision to make;
* two Patches addressed to one slot is refused outright — only the second would
  survive on the instrument, which is a loss that happens after the file looks
  fine;
* re-addressing requires re-encoding from the model, so the export says
  `ReencodedFromModel` rather than claiming to be original bytes.

With `UserBankSlots` the export emits **no per-Patch re-address note**. A note
reports what an export did that the caller did not literally ask for, and here
the caller named every destination; a note per Patch would bury a real one. The
destination each Patch came from is already on its tile.

An empty bank is refused rather than written, and the action is disabled while
the bank has nothing in it.

## Getting a bank on and off the keyboard

The file half of this is above. This is the instrument half, and the two halves
are deliberately asymmetric: reading is free and safe, writing is armed,
confirmed and reversible.

### Read XP-60 bank — `services::UserBankRead`

RQ1 only. RQ1 cannot modify device memory, so a whole-bank read is safe to run
against an instrument whatever else is going on, and the class has no send path
at all.

All 128 USER Patches come into the library as **one source bank** — provenance
`FetchedFromDevice`, the User number each came from, and the **exact DT1 bytes
the instrument sent** rather than a re-encoding of them — and the draft is
arranged from them at the slots they occupied, as one undo step. So a musician
can take what is on their keyboard, search it, rearrange it, export it as a
`.syx`, and put it back.

Patches are read one at a time because the XP-60 drops requests that arrive
while it is transmitting (§2.3, hardware-verified). A full bank is roughly 640
block reads at ~53 ms, so it takes a couple of minutes; the operation reports
progress and can be stopped, and a stopped or failed run keeps everything it
read, because a partial backup is worth having.

### Write to XP-60 USER — `services::UserMemoryWrite`

The point of building a bank. Also the only destructive thing the application
does, so:

| Rule | Why |
|---|---|
| A **separate service** from `PatchTransfer`, with its own arming | `PatchTransfer` hard-codes `03 00 00 00` and cannot be pointed at USER memory; this class must be handed a slot number and cannot write the temporary area. Arming one authorises nothing in the other. |
| **Two presses** (Arm, then Write) behind a confirmation naming the range | The XP-60's own front panel makes a Write a separate, destination-chosen, confirmed act (Owner's Manual p.46). This mirrors it. |
| **Every destination is read before it is written** | The previous Patch is always in hand, so `restore()` — *Put back what was there* — can undo the whole run, most recent first, verifying each. |
| **Every write is read back and compared** | A destination that reads back unchanged is reported as a mismatch naming **User Memory Protect**, which is what that setting looks like from the wire. It can never be reported as success. |
| A **partly illegal plan writes nothing** | Two Patches at one slot, or a slot outside 1–128, refuses the whole run rather than leaving permanent memory half rewritten. |
| **Empty destinations are skipped, not erased** | Whether a gap in a bank means "wipe whatever the instrument holds there" is not this application's decision. A destination whose Patch was deleted from the library is skipped too, and counted. |
| Stopping lands **between** Patches | Stopping inside one would leave a destination holding a mixture of two sounds. |

The write direction into `11 nn 00 00` is documentation-derived; reads from
those addresses are hardware-verified. That asymmetry is why every write is
proved at runtime instead of trusted. See `PATCH_SYNCHRONIZATION.md` §3 U1 and
`DEVICE_ACCEPTANCE.md` area 11.

## Duplicates

A destination whose sound also sits somewhere else in the bank carries a quiet
hollow ring beside its occupancy LED, and the header counts them. Hovering the
ring says which destination it matches and, importantly, *which kind* of
duplicate it is:

| Kind | What it means |
|---|---|
| "The same Patch is also at A35" | One library Patch placed in two destinations. Legitimate and documented — a bank is an arrangement of references, and the same Patch can fill four destinations. |
| "The same sound, under another name, is also at A35" | Two *different* library Patches whose parameters are byte-identical. Usually the same sound imported twice. |

Neither is an error and neither is ever acted on: the musician decides whether
it was meant. The mark is deliberately not a warning colour for that reason.

Matching is by the stored `library::PatchFingerprint`, so it compares the
Patches the bank actually references — which is what the bank would save and
write. Fingerprints are cached by library id, because a stored Patch's
fingerprint does not change while it sits in the library, so a drag does not
re-read the database 128 times.

## Persistence

Library schema version **2** adds two tables. The migration is additive: every
statement is `CREATE TABLE IF NOT EXISTS`, so opening a version 1 library
creates the new tables, touches no existing row, and stamps the new version.

```sql
banks       (id, name, created_at, updated_at)
bank_slots  (bank_id -> banks ON DELETE CASCADE,
             slot_index 0..127,
             patch_id -> patches ON DELETE SET NULL,
             patch_name, source_name, source_slot,
             PRIMARY KEY (bank_id, slot_index))
```

Only occupied destinations are stored — an empty destination is the absence of
a row — so a mostly empty bank costs almost nothing and the 128 positions are
always reconstructed from the slot index.

## Layer boundaries

```
BankBuilderScreen.qml            presentation/interaction only
  BankBuilderViewModel           panel selection, projections, actions
    library::BankDraft           the arrangement, undo/redo, modified state
    library::LibraryDatabase     saved banks, source summaries
    xpmodel::Xp60BankLocation    A/B · BANK · NUMBER <-> 001..128
    services::PatchTransfer      audition (temporary area, armed, verified)
```

QML computes no domain rule. It does not know an address, a checksum or a slot
formula; `slotIndexFor`, `panelLabelFor`, `linearLabelFor`, `dropPreview` and
`visibleDestinations` all come from C++.

## Tests

| Test | Covers |
|---|---|
| `tests/cpp/tst_bank_location.cpp` | the panel ↔ linear bijection, exhaustively over 128; manual anchors; refusal of out-of-range coordinates |
| `tests/cpp/tst_bank_draft.cpp` | place / replace / clear / move / swap, undo & redo including the saved state, redo-branch truncation, missing destinations, a many-destination fill as one undo step, and a fill that is refused whole when any part of it is illegal |
| `tests/cpp/tst_bank_builder.cpp` | filling from a source: the slots a source records, Patches with no recorded slot left unplaced, the first Patch kept when two claim one destination, one undo step, adding to what is already there; exporting: each Patch addressed to the destination it occupies with the gaps left as gaps, and an empty bank refused |
| `tests/cpp/tst_library_export.cpp` | `UserBankSlots`: chosen non-consecutive destinations, no re-address notes, and refusal of a count mismatch, a duplicate destination, an out-of-range slot and original-bytes re-addressing |
| `tests/cpp/tst_bank_store.cpp` | save & reload an arrangement, saving leaves every Patch untouched, update in place, refusal of >128, deleted Patch leaves a MISSING destination, deleting a bank leaves the library alone, source grouping, version 1 → 2 migration |
| `tests/qml/tst_BankBuilderScreen.qml` | panel selection naming the destination, the visible eight, NUMBER-button selection, placement preserving the source, drop previews (PLACE / REPLACE / MOVE / SWAP), a drop that lands nowhere, move & swap, undo/redo, the 16-bank overview and its navigation, occupancy reporting, Save as new bank preserving all 128 positions, new empty bank, opening one source bank, arranging a bank from the open source and undoing it in one step, and the export action's availability |

## Screenshots

Captured from the shipping QML by `build-windows/xp60studio_screenshot.exe`,
against the golden fixture's 128 real User patches:

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software XP60STUDIO_SHOT_OFFLINE=1 XP60STUDIO_SHOT_BANK_FILL=0-19,64-70 XP60STUDIO_SHOT_BANK_SUBGROUP=0 XP60STUDIO_SHOT_BANK_BANK=3 XP60STUDIO_SHOT_BANK_NUMBER=5 build-windows/xp60studio_screenshot.exe docs/design/screenshots/bank-builder/subgroup-a.png banks 1440 900
```

| File | State |
|---|---|
| `screenshots/bank-builder/subgroup-a.png` | Subgroup A, BANK 3, NUMBER 5 — `A35` / `PATCH 021`, an empty destination |
| `screenshots/bank-builder/subgroup-b.png` | Subgroup B, BANK 1 — `B11` / `PATCH 065` |
| `screenshots/bank-builder/occupied-bank.png` | A fully occupied BANK 1 (8 / 8) |
| `screenshots/bank-builder/drag-placement.png` | A drag in progress over `A35`: ghost, `PLACE` badge, armed NUMBER button, display preview |
| `screenshots/bank-builder/drag-swap.png` | A destination dragged onto another inside the bank: source dimmed, `SWAP` badge, and the display naming what it swaps with |
| `screenshots/bank-builder/full-overview.png` | 98 / 128 filled — the bank map with A full and B partly filled |

The harness environment variables are `XP60STUDIO_SHOT_BANK_FILL`
(comma-separated `first-last` slot ranges), `..._SUBGROUP`, `..._BANK`,
`..._NUMBER`, `..._DRAG` (a slot index to stage a drag over) and
`..._DRAG_FROM` (a slot index, to stage a move instead of a placement).
