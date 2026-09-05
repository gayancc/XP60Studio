# Expansion Intelligence

*Phase 7. How XP60Studio answers "will this Patch play on **my** XP-60?" — and
where it declines to answer.*

---

## 1. The question, and why it is hard

An XP-60 has four Wave Expansion slots, EXP-A to EXP-D, each holding one SR-JV80
board (Owner's Manual p.45). A Tone that points at a wave from a board the
instrument does not have has nothing to sound. A musician importing somebody
else's bank wants to know which of those 128 Patches will actually work, and
wants to know it before a gig rather than during one.

The obstacle is not the analysis. It is that **XP60Studio cannot tell which board
a Patch is asking for.**

A Tone names an expansion wave by **Wave Group ID**, which the Parameter Address
Map gives as a plain 0..127 field with no stated meaning. Which board answers to
which group is not documented anywhere this project has, and the project's own
evidence declines to settle it. The golden fixture's 192 expansion references use
groups **1, 5, 7, 14 and 97**. A Patch called `Sitar` on group 14 lines up with
SR-JV80-14 "Asia" and `Ethno Pipes3` on group 5 with SR-JV80-05 "World" — a
tempting pattern that then fails, because **there is no SR-JV80-97**. Slot
indices would be 1–4, so it is not that either. The full argument, and the
refusal, are in `../protocol/ROLAND_XP60_PROTOCOL_FACTS.md` §7.

So this phase is built around a gap rather than around a table.

## 2. The three-valued verdict

Every compatibility answer in XP60Studio has three values, not two:

| Verdict | Means | Shown as |
| --- | --- | --- |
| **available** | An expansion wave from a group one of the declared boards provides. | `EXP`, success |
| **missing** | An expansion wave from a group no declared board provides, on a profile complete enough for that to mean something. | `NEEDS BOARD`, error |
| **unknown** | Cannot be decided: nothing is declared at all, or a declared board's wave group is not yet known. | `EXP?`, warning |

Plus **internal** (no board can be missing) and **unscanned** (a stored Patch
whose bytes no longer decode — reported, never quietly counted as safe).

The rule that generates the third value: **one declared board with an unknown
wave group holds back every "missing" verdict in the application.** That board
could be the one providing what a Patch is asking for, and reporting "missing"
on a profile nobody filled in would be worse than useless — it would train
musicians to ignore the warning.

`library::analysePatch` is the whole of the logic, and it is pure: no I/O, no
clock, no Qt.

## 3. What the musician declares, and what XP60Studio learns

`library::ExpansionProfile` holds four slots. Each board carries a name the
musician chose (never parsed for meaning) and, optionally, the Wave Group ID it
answers to. **"Installed, group not known" is a first-class state**, not a blank
to be filled in with a plausible number.

The Expansion Manager (`qml/…/Screens/ExpansionScreen.qml`,
`presentation::ExpansionViewModel`) is where that is declared, and it offers one
way out of the unknown state that involves nobody guessing:

**Learn.** The musician selects a wave from a known board in a Tone on the
instrument's front panel, fetches the temporary Patch, and XP60Studio reads the
group ID straight out of it. Evidence from the user's own instrument, rather than
a table this project could not honestly write.

`learnFromCurrentPatch` **refuses an ambiguous Patch** — one whose Tones span two
groups nothing already claims — rather than picking the first. A wrong
association would make every later verdict wrong, silently.

The profile is stored in the library database (schema 4, `expansion_slots`).

## 4. Derived data: how the Library answers without decoding

`LibraryListModel` never materialises the whole result set and never decodes a
Patch to draw a row (ARCHITECTURE.md §11). Compatibility would break that, since
the Wave Group IDs live inside the Patch.

Schema **5** resolves it by deriving them once, at import:

- `patch_expansion_groups(patch_id, group_id)` — the groups a Patch refers to.
- `patch_expansion_scan(patch_id)` — a marker that the Patch *was* analysed.

Two tables, because "uses no expansion wave" and "not analysed yet" are different
facts and a missing row cannot say which. Calling an unanalysed Patch
internal-only is the one mistake that would read as a clean pass, so an
internal-only Patch has a scan row and no group rows.

This is derived data: the stored SysEx remains the only source of truth, and
these rows can be dropped and rebuilt from it. A library written before schema 5
is backfilled on open, from bytes already stored — nothing is asked of the user,
nothing stored is altered, and an entry whose bytes no longer decode is left
unscanned rather than recorded as safe.

The payoff is that "which of these play on my XP-60?" is one indexed query over
a library of thousands, and a scrolling list pays for its badges once per page.

## 5. Where the answer appears

**Library.** Two chips — *Plays on my XP-60* and *Needs a board* — which are
exact complements, plus a per-row badge and a caption that says what the chips
can currently promise. With no boards declared, *Needs a board* lists every Patch
that uses an expansion wave at all, and the caption says so: the widest honest
answer, not a prediction.

**Bank Builder.** A whole-bank summary, shown in the header and again beside the
USER-memory write. Putting a bank on the keyboard is exactly when "half of these
will have nothing to sound" is worth knowing, and it is the last moment to
notice. (The fixture bank is a good example of a true and awkward answer: it
needs five wave groups, and an XP-60 has four slots.)

**Wave Browser.** Its Expansion tab reports the declared boards, and separately
that this project has **no waveform-name list for any SR-JV80 board** — Roland
publishes those per board and none is transcribed here. Two different gaps: the
first the musician can close, the second they cannot close from that screen, and
it does not go away when the profile is complete.

**Patch Editor.** Per-Tone, on the Tone card, with the workflow below.

## 6. Missing waves: three ways out, and no fourth

> **Never silently replace missing waves.**

There is no mapping from an expansion wave to an internal one that this project
could write honestly, and a Patch quietly re-pointed at a substitute is worse
than one that plainly does not sound — the musician would have no way to know
their sound had been changed. So `PatchEditorViewModel` contains **no
auto-replace path at all**, and these three are the whole of what is offered:

| Action | What it does | What it does not do |
| --- | --- | --- |
| **Find replacement** | Selects the Tone and asks the screen to open the Wave Browser, where the musician picks. | Chooses nothing. Changes no data. |
| **Disable Tone** | Turns the Tone's switch off — the same ordinary, undoable edit the Tone card's own switch makes. | Does not touch the wave it points at, so it is reversible. |
| **Keep anyway** | Dismisses the prompt. | Changes nothing at all. The Patch is byte for byte what it was, and it will still not sound. |

Two details that follow from taking the words seriously:

- A **disabled** Tone keeps its expansion verdict (turning it back on is a normal
  edit and the board would still be missing) but stops asking for attention.
  Disabling it *is* one of the three resolutions.
- **Keep anyway** is per-Patch and is forgotten when another Patch is opened. A
  different Patch's Tone 2 is a different question, and a dismissal is not a
  setting.

## 7. What is deliberately not built

- **A Wave Group ID → board table.** Refused; see §1 and protocol facts §7.
- **An expansion waveform catalog.** Not transcribed for any board.
- **Detecting installed boards over MIDI.** No documented request for it.
- **Automatic wave substitution.** See §6.

Each of these is a place where XP60Studio says "cannot tell" instead of guessing.
`DEVICE_ACCEPTANCE.md` area 15 records what evidence would settle the first, if a
musician with a known board ever supplies it.
