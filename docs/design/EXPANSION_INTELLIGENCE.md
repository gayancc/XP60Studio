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

There are two separate obstacles, and keeping them apart is most of the design.

**What a Patch is asking for.** A Tone names an expansion wave by **Wave Group
ID**, a plain 0..127 field the Parameter Address Map defines the width of and
nothing else. XP60Studio reads that ID as the **SR-JV80 board of that number** —
group 14 is SR-JV80-14 *Asia*. Roland documents the mapping nowhere, but its own
per-board Waveform Lists settle it in practice: every reference to group 1 in a
real user bank lands on a real wave of SR-JV80-01 *Pop*, and the Patch called
`Clav 1 x4` uses four Clav waves while `60s Organ x4` uses the four numbered
organ waves. An independent editor for this family transmits the same values.
`../protocol/ROLAND_XP60_PROTOCOL_FACTS.md` §7 sets it out.

*(An earlier revision refused this mapping, on the false ground that the
fixture's group 97 could not be a board number. The SR-JV80 series runs 01–19
**and** 96–99; SR-JV80-97 is* Experience III*, and the fixture's group-97
Patches use its waves. The refusal was a mistake and has been corrected.)*

**What the instrument actually has.** Nothing in the protocol reports which
boards are fitted. No inference closes this one, and the catalogue above does not
try: naming what a Patch wants says nothing about whether the musician owns it.

So the compatibility verdict rests entirely on what the musician declared, and
the catalogue's job is to make declaring it quick and to put names on numbers.

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

`library::ExpansionProfile` holds four slots. Each board carries a name and,
optionally, the Wave Group ID it answers to. **"Installed, group not known" is a
first-class state**, not a blank to be filled in with a plausible number.

The quick path is `library::ExpansionBoardCatalog`: the musician picks an
SR-JV80 board from Roland's list and its wave group is filled in with the board
number. The field stays editable, because the catalogue is a default rather than
a fact about *their* instrument.

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

**Learn outranks the catalogue.** If a musician's board answers to a different
number than the table predicts, the instrument is right and the table is wrong,
and `setWaveGroup` lets them say so directly too.

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

**Wave Browser.** Its Expansion tab reports the declared boards and, for each,
whether Roland's Waveform List for it is held here — the lists are published per
board and `docs/XP60-References/SR-JV80/` has some of them. Board by board,
because the answer differs per board.

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

- **Treating the group → board mapping as hardware-verified.** It is
  corroborated, not confirmed by this project against an instrument, and both
  Learn and a manual edit override it.
- **A complete expansion waveform catalog.** Roland publishes waveform lists per
  board and this project holds some of them (SR-JV80-01 and -02 so far). Waves
  on a board whose list is absent are shown by number with the board's name, not
  by a guessed name; adding a board is dropping its PDF in and re-running the
  generator, per `docs/XP60-References/SR-JV80/README.md`.
- **Concluding an instrument has a board.** Naming what a Patch wants is not the
  same claim, and no verdict rests on the catalogue.
- **Detecting installed boards over MIDI.** No documented request for it.
- **Automatic wave substitution.** See §6.

`DEVICE_ACCEPTANCE.md` area 15 records what would confirm the mapping outright:
install a known board, select one of its waves in a Tone from the front panel,
and read the Tone back.
