# Patch Synchronization Architecture

How one Patch stays coherent across the Patch Editor, the Library, the Bank
Builder and a connected Roland XP-60 — without ever turning an edit into a
permanent USER-memory overwrite.

This document is research first and design second. Every hardware statement
below carries its evidence. Where the instrument's behaviour could not be
established from an authoritative source it is listed in
[§3 Unknowns](#3-what-is-still-unknown) rather than assumed.

---

## 1. Verified XP-60 memory architecture

### 1.1 Source material

| Source | How it was read | Rank (AGENTS.md) |
|---|---|---|
| Roland XP-60/XP-80 Owner's Manual, printed p.45 "Memory and data storage" | `docs/XP60-References/XP-60_80_OM.pdf`, PDF page 47, OCR'd for this work | 1 (official manual) |
| Same manual, printed p.46 "Storing a sound you modify into user memory" | PDF page 48, OCR'd | 1 |
| Same manual, MIDI Implementation §1 (RQ1/DT1), printed p.218 | PDF page 220, OCR'd | 1 (official MIDI implementation) |
| Same manual, MIDI Implementation §5 "Parameter address map", printed p.221 | PDF page 223, OCR'd | 1 |
| Same manual, Program Change transmission, printed p.219 | PDF page 221, OCR'd | 1 |
| `tests/fixtures/xp60/user-bank-amal.syx` | a real XP-60 User bank supplied by the project owner | 4 |
| `docs/HARDWARE_VALIDATION_XP60.md` hardware log 2026-09-04 | this project's own captures from a physical XP-60 | 3/5 |

The PDF has no text layer; the pages above were rendered at 200 dpi and OCR'd.
OCR noise is visible in the address column (`11` read as `21`/`13`), so every
address quoted here is cross-checked against
`docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md` §3 and against the golden fixture,
both of which independently give `11 nn 00 00` for the User Patch bank.

### 1.2 The three memories

Owner's Manual p.45 states the architecture directly. Quoting the parts that
govern this feature:

> The location where Patch and Performance settings, etc. are stored is
> 'memory.' There are three memory types: temporary memory, rewritable memory
> and non-rewritable memory.

> **Temporary area** — This area holds data for the Performance, Patch, and
> Rhythm Set you select using the front panel buttons, etc. When you play the
> keyboard or play back a sequence, sound is produced based on data in the
> temporary area. **When you modify a Performance, Patch or Rhythm Set, you are
> modifying the data that has been called into the temporary area instead of
> the data in memory.**

> Settings in the temporary area are temporary, and **will be lost when the
> power is turned off or when you select another Performance/Patch/Rhythm Set**.
> To keep the settings you have modified, **you must write them into rewritable
> memory**.

> **User memory** contains data for 32 Performances, 128 Patches and two Rhythm
> Sets.

> **System memory** stores system parameter settings … When you modify these
> settings, the system memory settings are rewritten directly.

Five consequences, all load-bearing for this design:

| # | Verified fact | Status |
|---|---|---|
| M1 | The instrument makes sound from the **temporary area**, not from USER memory. | Documentation-derived (OM p.45) |
| M2 | Editing on the instrument changes the temporary area only. | Documentation-derived (OM p.45) |
| M3 | The temporary area is destroyed by power-off **and by selecting another Patch/Performance/Rhythm Set**. | Documentation-derived (OM p.45) |
| M4 | Keeping an edit requires an explicit **Write** into rewritable memory. | Documentation-derived (OM p.45, p.46) |
| M5 | USER memory holds exactly 32 Performances, 128 Patches, 2 Rhythm Sets. | Documentation-derived (OM p.45) |

M3 is the single most important fact in this document. It means the XP-60's
temporary area is a *volatile scratch buffer that the instrument itself will
throw away without warning*, and therefore that XP60Studio can use it freely for
audition — and must never treat it as storage.

### 1.3 What the instrument's own Write does

Owner's Manual p.46:

> The modified settings you make are only temporary, and will be lost if you
> turn the power off or select another Patch, Performance or Rhythm Set. To keep
> the modified settings, you must write them into user memory.
>
> `[UTILITY]` → `[1]` → `[ENTER]` → choose the destination number →
> `[F6] (Execute)`.
>
> If Write Operation parameter (UTILITY/Protect/User Memory Protect) is OFF, the
> specified Patch, Performance or Rhythm Set will be overwritten by your new
> edited settings.
> If Write Operation parameter … is ON, the window will open. Change the ON
> setting to OFF, and you'll be ready to write your data into user memory.

| # | Verified fact | Status |
|---|---|---|
| M6 | A persistent write is a **deliberate, destination-chosen, confirmed** operation on the instrument — never a side effect of editing. | Documentation-derived (OM p.46) |
| M7 | **User Memory Protect** is a real instrument-side setting that refuses writes while ON. | Documentation-derived (OM p.46) |

M6 is the interaction model XP60Studio should mirror: the XP-60 itself makes a
persistent write a separate, addressed, confirmed act. M7 means a persistent
write can fail for a reason that has nothing to do with our transfer, and the UI
must be able to say so instead of reporting a mysterious mismatch.

### 1.4 The address map decides what is destructive

MIDI Implementation §5, printed p.221 (`1. XP-60/XP-80 (Model ID=6AH)`):

| Address | Region | Volatile? |
|---|---|---|
| `00 00 00 00` | System | no — rewritten directly (OM p.45) |
| `02 00 00 00` … `02 0F 00 00` | Performance Mode Temporary Patch, Parts 1–16 | **yes** |
| `02 09 00 00` | Temporary Rhythm Setup (Part 10) | **yes** |
| `03 00 00 00` | **Patch Mode Temporary Patch** | **yes** |
| `10 00 00 00` … `10 1F 00 00` | User Performance USER:01–32 | no — persistent |
| `10 40 00 00`, `10 41 00 00` | User Rhythm Set | no — persistent |
| `11 00 00 00` … `11 7F 00 00` | **User Patch USER:001–128** | no — persistent |

| # | Verified fact | Status |
|---|---|---|
| M8 | A DT1 addressed to `03 00 00 00` reaches **temporary** memory. It cannot damage a stored sound; the instrument discards it on the next patch change or power cycle. | Documentation-derived (address map + M3) |
| M9 | A DT1 addressed to `11 nn 00 00` reaches **persistent USER Patch memory**. This is destructive and irreversible without a prior backup. | Documentation-derived (address map) |

**The destructive/non-destructive boundary in this application is therefore a
property of one address byte.** That is the whole safety argument, and it is why
the design below makes the address the thing that is gated, not the button that
happens to be pressed.

### 1.5 Verified transfer behaviour

MIDI Implementation §1, printed p.218, on **Data Set 1 (DT1)**:

> This message transmits the actual data, and is used when you wish to set the
> data of the receiving device.
>
> `F0H 41H dev 6AH 12H aaH bbH ccH ddH … eeH sum F7H`
>
> * Data whose size is greater than 128 bytes should be divided into packets of
>   128 bytes or less and transmitted. Successive "Data Set 1" messages should
>   have at least 20 ms of time interval between them.
> * This message is not received if Rx.Sys.Excl parameter … is OFF.
> * This message is not received in GM mode.

and on **Data Request 1 (RQ1)**:

> * The amount of data that is transmitted at one time is fixed for the type of
>   data, and only data of the fixed starting address and size will be
>   transmitted. Refer to the address and size given in "5. Parameter address
>   map".

| # | Verified fact | Status |
|---|---|---|
| T1 | DT1 payloads to the instrument are ≤ 128 bytes, ≥ 20 ms apart. | Documentation-derived; already implemented in `xp60::transferDefaults()` |
| T2 | A DT1 may address a **sub-block offset with a short payload**. Roland's own published example is a single data byte at `01 00 00 28`. | Documentation-derived (`ROLAND_XP60_PROTOCOL_FACTS.md` §1) |
| T3 | The XP-60 **accepts** a Tone block split as 128 + 1, including a one-byte DT1 at offset `01 00`, and reproduces the Patch exactly. | **Hardware-verified 2026-09-04** (`HARDWARE_VALIDATION_XP60.md`, export round trip, 4/4) |
| T4 | Writing the temporary Patch and reading it back verifies byte-for-byte. | **Hardware-verified 2026-09-04** (write round trip, 6/6) |
| T5 | The XP-60 **drops RQ1s that arrive while it is transmitting a reply**. Reads must be serialised, not merely delayed. | **Hardware-verified 2026-09-04** (§2.3) |
| T6 | A whole five-block Patch read costs roughly 5 × 53 ms ≈ 265 ms on USB-MIDI. | **Hardware-measured 2026-09-04** |
| T7 | SysEx reception can be switched off on the instrument (Rx.Sys.Excl), and is off in GM mode. | Documentation-derived |

T2 + T3 together are what make responsive real-time editing legitimate: a single
changed parameter is a single short DT1, not a 644-byte Patch dump. T6 is what
makes verifying *every* such change unaffordable, and is the reason the current
live-audition design feels slow.

### 1.6 The instrument announces its own Patch changes

MIDI Implementation §2 "Data transmission (sound source section)", printed
p.219:

> **Program Change** — `CnH ppH` … * This message is not transmitted when the Tx
> Program Change parameter (SYSTEM/MIDI/MIDI Param 2) is OFF.

and printed p.218 for Bank Select:

> * This message is not transmitted if Tx Program Change parameter … or Tx Bank
>   Select parameter … is OFF.

| # | Verified fact | Status |
|---|---|---|
| C1 | Selecting a Patch on the XP-60's front panel **transmits** Bank Select + Program Change on MIDI OUT, unless the user disabled those Tx switches. | Documentation-derived (OM p.218–219) |

By M3, that same panel action **destroys the temporary area**. So C1 gives us a
documented, passive, zero-traffic signal that our copy of the temporary area has
just become worthless. This design uses it as the primary staleness detector.

---

## 2. Where XP60Studio is today

### 2.1 What already exists and is correct

The repository is in better shape than a greenfield design would assume, and
this work reuses rather than replaces:

| Component | What it already does right |
|---|---|
| `services::DeviceSession` | Owns the connection, the paced send queue, request correlation and serialised block reads (fix for T5). `sendDataSets()` is a general paced DT1 path. |
| `services::PatchTransfer` | The armed, snapshot-taking, read-back-verifying write to `03 00 00 00`. Permanent memory is unreachable through it *by construction* — the address is hard-coded. |
| `Xp60PatchCodec::encodeChangesToDataSets` | Already emits **only the changed spans**, preserving whole parameters across span and 128-byte boundaries. Exactly the T2 mechanism, already written and tested. |
| `PatchTransfer` live preview | Already coalesces: at most one pending target, latest wins, 120 ms timer. The "10 → 11 → … → 90" backlog problem is already solved. |
| `library::BankDraft` / `LibraryDatabase` | Bank arrangement is references, never copies; `bank_slots.patch_id` is `ON DELETE SET NULL`. Rearranging a bank already touches no hardware. |

**Finding: the transmission layer does not need rebuilding.** The gaps are above
it.

### 2.2 Gaps and defects

**G1 — The Editor is unreachable from the Library and the Bank Builder.**
`PatchEditorViewModel` obtains a Patch from exactly one place:

```cpp
connect(&m_session, &DeviceSession::patchFetchChanged, this, &adoptFetchedPatch);
```

There is no code path that opens a library entry or a bank destination in the
Editor. The three screens do not disagree about a Patch today only because they
can never be looking at the same one.

**G2 — Patch state is duplicated with no owner.** `PatchEditorViewModel` holds
`m_original`, `m_current`, `m_hardware` plus `m_undo`/`m_redo` deques of whole
Patches. `BankBuilderViewModel` holds `BankSlotContent` with a *cached copy* of
the patch name. `LibraryListModel` caches `LibraryRecord`s including the name.
A rename in the Editor cannot reach either cache — which is precisely the
failure the task names.

**G3 — "Write to XP-60" is ambiguous in the UI, though correct underneath.**
The button reads *Write to XP-60*, next to *Arm*. The word "write" is the same
word the Owner's Manual reserves for the persistent USER-memory operation
(M4/M6), but the code sends to `03 00 00 00` — temporary. A user reading the
manual and the button together would reasonably conclude their USER patch had
been overwritten. The behaviour is safe; the name is not.

**G4 — Every live update pays for a full read-back.** `drainLivePreview()` sends
the changed spans and then reads all five blocks before the next update may go
out. By T6 that is ~265 ms of dead time per parameter move, on top of the 120 ms
coalescing window. Integrity is prioritised absolutely over latency, which is
defensible but makes real-time sound design feel disconnected — the explicit
concern in this task.

**G5 — Staleness is detected only from our own failures.** Audition ends on
disconnect, device-ID change, transport failure, read timeout or mismatch.
Nothing watches for C1, so the most likely real-world desync — *the musician
pressed a Patch button on the keyboard* — is invisible until a later read-back
happens to disagree.

**G6 — The Bank Builder auditions by a different mechanism.**
`BankBuilderViewModel::auditionSlot` rebuilds the Patch from preserved SysEx and
drives `PatchTransfer` with a whole-Patch write. It is safe and reuses the right
service, but it is a second, parallel notion of "what the instrument is
currently holding" that the Editor knows nothing about.

**G7 — No persistent-write path exists at all.** There is no way to put a Patch
into USER memory from XP60Studio, and no state that distinguishes "saved in the
Studio library" from "written to the instrument". Bank export writes a `.syx`
file; sending that file to the instrument is left to the user.

### 2.3 Assumptions checked against the documentation

| Current assumption in the code | Verdict |
|---|---|
| `03 00 00 00` is safe to write repeatedly | **Correct** — M8. Confirmed non-destructive by M3. |
| A write must be armed and preceded by a verified read | Stronger than the hardware requires, and worth keeping for the *persistent* path; unnecessarily heavy for temporary audition. |
| Permanent User memory is "not reachable at all" | **Correct today**, and this remains true for `PatchTransfer`. The new persistent path is a separate, separately-gated service. |
| A completed send is not a verified state | **Correct** and retained. |

---

## 3. What is still unknown

Listed rather than guessed, per `AGENTS.md`.

| # | Unknown | Why it matters | How it would be settled |
|---|---|---|---|
| U1 | Whether the XP-60 honours a DT1 to `11 nn 00 00` (USER Patch), and whether **User Memory Protect** (M7) blocks it. The address map lists the region; it does not say the region is writable over SysEx. | The persistent-write feature depends on it entirely. | One armed write to a USER slot whose contents were fetched first, then read back and compared, with Protect both ON and OFF. Added to `DEVICE_ACCEPTANCE.md`. |
| U2 | Whether writing `03 00 00 00` while the instrument is in **Performance mode** is audible. The map has separate Performance-mode temporary Patches at `02 0n 00 00`. | Audition would silently do nothing in Performance mode. | Put the XP-60 in Performance mode, send a temporary Patch, listen and read back. |
| U3 | Whether the XP-60 reports its current **mode** (Patch / Performance / Rhythm) over MIDI. | Would let the app pick the right temporary address instead of asking. | Probe System Common; inspect what the panel transmits on a mode change. |
| U4 | Whether the XP-60 transmits anything when a **parameter** is edited on the front panel (as opposed to a Patch being selected, C1). | Would allow finer conflict detection than "assume stale". | Watch MIDI IN while turning the panel's VALUE dial. |
| U5 | Whether a Program Change **we** transmit changes the temporary area the same way a panel press does. | Would give the app a way to select a Patch on the instrument deliberately. | Send a Program Change, then read `03 00 00 00`. |

**Nothing in the implementation depends on U2–U5.** U1 gates the persistent-write
execution path, which is why that path ships armed, documented and disabled from
touching hardware until the deferred session closes it.

---

## 4. The state model

Two questions about a Patch are independent and must not be collapsed into one
badge, because a Patch can be saved in the Studio and absent from the instrument,
or auditioning on the instrument and never saved.

### 4.1 Studio storage state — *is my work kept?*

| State | Meaning |
|---|---|
| `Untracked` | A working Patch with no library row behind it (a fresh device read). |
| `Saved` | Byte-identical to the library row it came from. |
| `Edited` | Differs from its library row. Closing without saving loses the difference. |

### 4.2 Device state — *does the instrument hold what I am looking at?*

| State | Meaning | Entered when |
|---|---|---|
| `Offline` | No connection. The question does not apply. | disconnected |
| `NotSent` | Never transmitted this session. | a working Patch is adopted while connected |
| `Sending` | DT1s are in flight. | a transfer starts |
| `Assumed` | The bytes were transmitted and the transport accepted them, but nothing has been read back. **Not** a claim of synchronization. | a live send completes without verification |
| `InSync` | A read-back proved the temporary area equals the working Patch. | verification succeeds |
| `Diverged` | The working Patch has changed since the last verified state. | any local edit after `InSync`/`Assumed` |
| `Stale` | The instrument's temporary area is believed replaced or unknowable. Nothing may be inferred from earlier verification. | Program Change / Bank Select seen from the device (C1); disconnect; device-ID change; transport error |
| `Failed` | A transfer did not complete. The temporary area may be partly written. | send or read-back failure |

`Assumed` is the state this architecture adds, and it is the honest answer to
G4/T6: it says *we sent it, we did not check*, which is exactly true, instead of
either pretending to be synchronized or paying 265 ms to prove it on every knob
movement.

### 4.3 Persistent state — *is it in the instrument's USER memory?*

Deliberately **not** modelled as a live status. XP60Studio cannot know what is in
a USER slot without reading it, and a stale "written" badge is worse than none.
The only persistent facts recorded are events: *this working Patch was written to
USER:nnn at time T and the read-back verified*. Anything older than the current
session is reported as "last written", never as "in sync".

### 4.4 Legal transitions

```
                    edit
  Saved ─────────────────────────────► Edited
    ▲                                    │
    └──────── save to library ───────────┘

  Offline ──connect──► NotSent ──send──► Sending ──┬─► Assumed ──verify──► InSync
                          ▲                        └─► Failed              │
                          │                                                │ edit
              Stale ◄─────┴──── PC/BankSel, disconnect, error ◄─── Diverged ◄
```

`Stale` is absorbing until a fresh read or a fresh send re-establishes ground
truth. No transition anywhere in this graph reaches USER memory.

---

## 5. Architecture

### 5.1 One working Patch, many views

```
                       ┌──────────────────────────────┐
   Patch Editor ──────►│                              │
   Library row  ──────►│   services::PatchWorkspace   │◄──── DeviceSession
   Bank tile    ──────►│                              │      (fetch / PC watch)
   Dashboard    ──────►│  • working Xp60Patch         │
                       │  • origin (library id/slot)  │◄──── PatchTransfer
                       │  • saved baseline            │      (temporary only)
                       │  • device baseline           │
                       │  • undo/redo                 │
                       │  • SyncStatus (§4)           │
                       └──────────────────────────────┘
```

`PatchWorkspace` is the **single owner** of the Patch being worked on. It lives
in `services/` beside `DeviceSession`, because it is orchestration, not
presentation: it must be usable and testable without QML.

Views do not hold Patch copies. They ask the workspace, and they re-read when it
says `changed()`. A rename in the Editor reaches the Bank Builder tile because
the tile's name comes from the workspace when the workspace's origin matches that
tile's patch id, and from the database otherwise. That is the whole propagation
mechanism — no signal graph between screens, no cache invalidation protocol.

### 5.2 Origin: what a working Patch *is a copy of*

```cpp
struct PatchOrigin {
    enum class Kind { None, LibraryEntry, DeviceTemporary, DeviceUserSlot };
    Kind kind = Kind::None;
    std::int64_t libraryId = 0;   // LibraryEntry
    int userNumber = 0;           // DeviceUserSlot, 1..128
};
```

The origin is what makes cross-screen synchronization possible *and* what keeps
it honest: a working Patch adopted from library id 42 overlays library row 42 and
every bank destination referencing 42, and nothing else.

### 5.3 Data flow

**Editing** — Editor mutates the workspace; the workspace records undo, marks
studio state `Edited`, marks device state `Diverged`, emits `changed()`; every
view re-reads. Nothing is transmitted.

**Auditioning** — the workspace hands `PatchTransfer` the working Patch;
`PatchTransfer` diffs against its last known temporary state and sends only the
changed spans (`encodeChangesToDataSets`, T2/T3) to `03 00 00 00` (M8).

**Saving** — writes the working Patch to the library database. Studio state
becomes `Saved`. Nothing is transmitted.

**Assigning to a bank destination** — the Bank Builder stores a *reference* to
the library id. If the working Patch is unsaved, saving is required first; the UI
says so rather than silently assigning a stale copy.

**Persistent write** — a separate, separately-armed service targeting
`11 nn 00 00` (M9). See §7.

### 5.4 Layer boundaries

```
qml                        presentation only; no addresses, no sync rules
  PatchEditorViewModel     projects the workspace
  LibraryListModel         database rows + workspace overlay
  BankBuilderViewModel     draft + workspace overlay
    services::PatchWorkspace     the working Patch, origin, sync status
      services::PatchTransfer    temporary-area transfer (03 00 00 00)
      services::UserMemoryWrite  persistent write (11 nn 00 00), separately armed
        services::DeviceSession  connection, pacing, correlation
```

---

## 6. Transmission strategy

The task's worked example — dragging 10 → 11 → … → 90 — must not leave a backlog
of obsolete values, and must not feel disconnected. The existing coalescing
already solves the first half. This design adds the second.

| Concern | Mechanism | Evidence |
|---|---|---|
| Backlog of obsolete values | One pending target, replaced not queued. The value that arrives while a transfer is in flight overwrites the previous pending value. | already in `PatchTransfer::queueLivePreview` |
| Traffic volume | Only changed spans are sent, at documented offsets, whole parameters preserved. A one-byte parameter is one DT1. | T2, T3 |
| Instrument pacing | ≤128-byte payloads, ≥20 ms apart, via the existing paced send queue. | T1 |
| Read collisions | Reads stay serialised and are never issued while a send batch is outstanding. | T5 |
| **Latency** | **During a gesture, do not read back.** Send, mark `Assumed`, allow the next send immediately. Verify once the gesture settles (no edit for the settle interval) or when the user asks. | T6 — a read-back costs ~265 ms, five times the 20 ms pacing floor |
| Integrity | Verification still happens, and `InSync` is still only ever claimed after a byte comparison. The difference is *when*, not *whether*. | T4 |

The coalescing window stays at 120 ms and the settle interval is 400 ms, both
project choices rather than Roland figures, and both configurable. Neither is
presented as a hardware guarantee.

**Diff baseline while unverified.** Sending only changes requires knowing what
the instrument holds. While `Assumed`, the baseline is what we last *sent*, not
what we last *read*. That is sound because the transport reported the batch
delivered; it is not proof, which is exactly why the state is called `Assumed`
and why the settle-time verification does a full five-block comparison rather
than trusting the chain of diffs.

---

## 7. The persistent-write boundary

The safety rule is absolute: no synchronization mechanism may turn editing into a
USER-memory overwrite. This design enforces it structurally, not by convention.

1. **Different address, different service.** `PatchTransfer` hard-codes
   `03 00 00 00` and cannot be pointed at USER memory. Persistent writes live in
   a separate service that must be handed a USER slot number explicitly.
2. **Different arming.** Temporary arming does not authorise a persistent write.
3. **The destination is chosen, never inherited.** A working Patch that came from
   USER:007 does not default to writing back to USER:007.
4. **A safety snapshot of the destination is fetched first**, so the overwritten
   Patch can be put back. Refusing to write is preferred to writing without one.
5. **Read-back verification is mandatory**, not optional as it is for audition.
6. **U1 gates execution.** Until a hardware session proves the XP-60 accepts a
   DT1 to `11 nn 00 00` and reveals how User Memory Protect (M7) interacts with
   it, the path is built, tested against the simulator, and **refuses to
   transmit to real hardware**, saying exactly why. This mirrors how Phase 1
   handled DT1 writes before they were verified.

### Renaming "Write to XP-60"

Per G3, the Editor's action is renamed to match the Owner's Manual's vocabulary:

| Was | Becomes | Means |
|---|---|---|
| *Write to XP-60* | **Send to XP temp** | DT1 → `03 00 00 00`. Audible immediately; lost on patch change or power-off (M3). |
| — | **Write to USER…** | DT1 → `11 nn 00 00`. Destructive, destination chosen, snapshot taken (§7). |

---

## 8. Behaviour of every named transition

Each is decided by the memory model, not by convenience.

| Action | Studio state | Device state | Rationale |
|---|---|---|---|
| Select another Patch in the Editor | prompts if `Edited`; discards only on confirmation | → `NotSent` | Local work is never dropped silently. |
| Select another Library Patch | as above | → `NotSent` | Same rule; the origin changes. |
| Switch Bank destination | unaffected | unaffected | Selecting a destination is navigation, not editing. |
| Close the Editor | working Patch retained | unchanged | The workspace outlives the screen; nothing is lost by navigating away. |
| Disconnect the XP-60 | unaffected | → `Offline` | Editing continues; no claim about the instrument survives. |
| Reconnect | unaffected | → `Stale` | **Never auto-push.** A connection appearing proves nothing about the temporary area. The user re-sends or re-reads deliberately. |
| Load another bank | unaffected | unaffected | A bank is an arrangement of references. |
| Undo / redo | recomputed vs. the saved baseline | → `Diverged` | Undo is a local edit like any other. |
| Discard changes | → `Saved`/`Untracked` | → `Diverged` | The instrument still holds the edited version until re-sent. |
| Send to XP temp | unaffected | → `Sending` → `Assumed`/`InSync` | Transmission changes nothing about storage. |
| Save in XP60Studio | → `Saved` | unaffected | Storage changes nothing about the instrument. |
| Write to USER memory | unaffected | unaffected | Persistent memory is a third axis (§4.3). |
| Panel Patch change (C1) | unaffected | → `Stale` | M3: the temporary area was destroyed. |

The two "unaffected" columns that look surprising — saving does not touch the
instrument, sending does not save — are the point of separating the axes.

---

## 9. UI feedback

Compact and hardware-inspired, integrated into the Editor's existing header and
LCD-style readouts. Two indicators, never a wall of badges:

- **Studio**: silence when `Saved`; a single `EDITED` mark otherwise.
- **XP**: an LCD-style lamp reading `XP TEMP` when `InSync`, `SENT` when
  `Assumed`, `SENDING` while in flight, `NOT SENT`, `STALE`, or `OFFLINE`.

`STALE` is the one that earns its place: it is the state a musician most needs to
see and the one no existing XP-60 editor shows, because it means *"you touched
the keyboard; what you hear is no longer what you see."*

The persistent write is not a badge. It is an action with a confirmation naming
the destination slot and the Patch it will overwrite, because M6 says that is
what the instrument itself does.

---

## 10. Implementation stages

| Stage | Content |
|---|---|
| **1** | `PatchSyncState` + `PatchWorkspace`: the working Patch, origin, baselines, undo/redo, the §4 state machine. C++ tests over the golden fixture. |
| **2** | `PatchEditorViewModel` backed by the workspace instead of `m_original`/`m_current`/`m_hardware`. Library and Bank Builder overlays. Open-in-Editor from both. |
| **3** | Staleness from C1: watch MIDI IN for Bank Select / Program Change. Deferred verification (§6). |
| **4** | `UserMemoryWrite` service and its UI, gated on U1. |

Stages 1–2 deliver the cross-screen synchronization the task asks for; stage 3
delivers honest device state; stage 4 completes the persistent boundary.

---

## 11. Device acceptance additions

New rows for `DEVICE_ACCEPTANCE.md`, all requiring the physical instrument:

| Ref | Check |
|---|---|
| U1 | DT1 to `11 nn 00 00` — accepted? read-back equal? behaviour with User Memory Protect ON and OFF? |
| U2 | Temporary-Patch write while the instrument is in Performance mode — audible? |
| C1 | Selecting a Patch on the panel transmits Bank Select + Program Change, and the temporary area changes accordingly. |
| §6 | Deferred verification: after a burst of parameter sends without read-back, one full read-back equals the working Patch. |
