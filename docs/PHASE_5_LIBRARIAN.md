# Phase 5 — Librarian + Dashboard

Status: **librarian half complete and tested — fingerprints, provenance,
library entries, `.syx` import and export, local persistence with search, the
observable import service, and the virtualized Library screen. The M1 Dashboard
has not started.**

## Execution order

Phase 4's remaining work is EFX Parameter byte-slot mappings, the INT-A/INT-B
wave group mapping, and physical acceptance. All three are blocked on the same
deferred hardware session (`PHASE_4_EFFECT_ROUTING.md`,
`PHASE_4_WAVE_BROWSER.md`, `HARDWARE_VALIDATION_XP60.md`), which the user
deferred to final device acceptance on 2026-09-04 while authorizing continued
local work. Phase 5 therefore begins with its librarian half; the M1 Dashboard
is deliberately last, because the roadmap requires it to be built "once
meaningful data exists" and forbids fake actions for unimplemented features.

Nothing here promotes any hardware fact. This phase reads files and builds
local metadata; it opens no MIDI port and sends nothing.

## The new layer

`src/library` sits between `xpmodel` and `services` and uses Qt Core.
Everything in it is derived from, or metadata about, the canonical Roland data
that `xpmodel` owns. It never becomes a second source of hardware truth: a
Patch's parameters and its original bytes always come from `xpmodel`.

| Type | Responsibility |
|---|---|
| `PatchFingerprint` | SHA-256 over the five encoded blocks. Derived, versioned, domain-separated. |
| `PatchProvenance` | Where an entry came from, recorded once at import. |
| `PatchUserMetadata` | Favourite, rating, category, tags, notes — the user's own data. |
| `LibraryEntry` | One Patch: canonical data, provenance and derived metadata, kept apart. |
| `SyxImport` | Reads every XP-60 Patch a `.syx` byte stream contains. |
| `SyxExport` | Writes entries back out as a `.syx` stream. |
| `LibraryDatabase` | The persistent local library: SQLite through Qt SQL. |
| `LibraryImportService` | Importing files as one observable, cancellable operation (`src/services`). |
| `LibraryListModel` | The virtualized result list for the Library screen (`src/presentation`). |

### Fingerprints

The fingerprint hashes `Xp60PatchCodec::blockBytes` — Patch Common and Tone 1-4
exactly as the Parameter Address Map describes them — and nothing else. Two
Patches therefore fingerprint alike when their parameters match, whatever
device ID, chunking, checksums or address the SysEx that delivered them used.
A regression test proves this by re-transmitting a Patch under a different
device ID at a 16-byte payload cap and re-importing it: every framing byte
differs, the fingerprint does not.

Nothing is normalised on the way in. Bytes the tables do not describe and
values outside the transcribed ranges are hashed exactly as stored.

The fingerprint is an index, not an answer. `LibraryEntry::hasSameParameters`
uses it only to decide what is worth comparing and then confirms against the
parameters themselves, so a hash collision cannot report two different Patches
as duplicates. This is the "initial exact-duplicate foundation" the roadmap
asks of the import workflow; near-duplicate similarity and Patch DNA remain
Phase 9.

`kVersion` is part of the hashed payload, so a future change to what is
fingerprinted produces different digests instead of silently colliding with
digests written by an earlier build.

### Provenance and raw preservation

Every entry keeps the exact bytes of the messages that carried it, in stream
order, complete with their device IDs, chunking and checksums — never a
re-encoding from the model. A test asserts that an entry's `originalSysEx()` is
a verbatim slice of the source file at the byte offset its provenance records.
This is what makes an import reversible and makes any later disagreement
between the model and the source investigable against what actually arrived.

Provenance records the origin, the source name verbatim, the source file's
SHA-256, the byte range, the Roland address, the User bank slot, and the device
and model IDs the messages carried. Facts that are not established stay unset:
a Patch assembled from messages that disagree about the device ID keeps
neither ID rather than picking one.

User metadata lives in its own type and never touches the Roland data. A test
asserts that setting favourite, rating, category and tags leaves the Patch, its
original SysEx and its fingerprint unchanged. `PatchUserMetadata::category` is
explicitly the **user's** category: the XP-60 Parameter Address Map defines no
category byte, so a Roland-looking one would be an invented hardware fact.

### Import

`importSyxStream` parses the stream, lays its data sets into a `MemoryImage`,
and then checks each documented Patch base for coverage of the five blocks. A
Patch is imported only when all five are completely covered at a base the
protocol document establishes. Nothing is inferred from message order or file
size.

Every other outcome is reported rather than discarded:

| Outcome | Reported as |
|---|---|
| Some blocks present, not all | `partial`, naming each missing block and the byte counts |
| All blocks present, codec refused | `rejected`, with the codec's own description |
| Decoded with out-of-range values | `warnings`; the Patch is imported unchanged |
| Identical parameters twice in one file | `duplicates`, as index pairs |
| Data sets at addresses no Patch covers | `unattributedDataSets`, counted |
| Stray bytes, aborted or invalid SysEx | the full `SysExStreamResult`, unchanged |

A truncated bank is the case where importing "the patches that happened to be
complete" would hide data loss, so it is a first-class result, not a silence.

Performance-mode temporary Part areas (`02 00 00 00`..) are off by default: a
Performance dump is a Phase 8 subject, and Part 10 there is the Rhythm Setup,
whose Address Map is not transcribed. With `includePerformanceParts` set, Part
10 is still skipped rather than decoded as a Patch it is not.

`importSyxStream` does no file I/O and no persistence, and has no event-loop,
timer or thread dependency, so the import service can run it on a worker thread
and own progress and cancellation itself (ARCHITECTURE.md §8).

## Persistence

The technology choice `ARCHITECTURE.md` §12 deferred is now made: **SQLite
through Qt SQL**, using the `QSQLITE` driver Qt already ships. It needs no new
dependency, and it answers the query patterns this phase actually produced —
indexed fingerprint lookup, tag/category/favourite/rating filters, name search,
and paged ordered results over thousands of rows. `Qt6::Sql` is now a required
component in the root `CMakeLists.txt`.

### What is stored, and what is not

The decoded Patch is deliberately **not** stored. A row keeps the searchable
metadata, the provenance, the fingerprint and the original SysEx bytes; the
Patch is rebuilt from those bytes through the same codec an import uses. There
is therefore never a second, decoded copy of the same data that could drift
from the first. `loadEntry` re-checks the recomputed fingerprint against the
stored one and refuses rather than returning a Patch that no longer matches
what was recorded.

`LibraryRecord` — the row without the Patch or its bytes — is what a search
returns, so scrolling a library of thousands never decodes a Patch it is not
going to show. This is the shape the virtualized Library model will consume.

### Behaviour the tests pin down

| Rule | Why |
|---|---|
| `insertAll` is one transaction | A failed or cancelled bank import leaves the library untouched, never half-populated |
| The database never deduplicates | Re-importing a known file yields 128 more entries with their own provenance; `findDuplicatesOf` reports them and the user decides |
| Ratings are validated at the boundary | An out-of-range rating is refused and changes nothing, rather than being clamped into looking valid |
| An unknown id is an error | `updateUserMetadata` and `remove` say so instead of silently affecting no rows |
| Deleting an entry deletes its tags | `PRAGMA foreign_keys = ON`, so nothing is orphaned |
| A newer schema refuses to open | A library written by a later build is not opened and risked |
| Metadata writes never touch Roland data | Asserted against the fingerprint and the preserved bytes |

Filters combine as narrowing, which is how the question is actually asked
("favourite pads rated four or more"). Requesting several tags requires all of
them, not any. Name search is case-insensitive across whitespace-separated
terms, all of which must match; it compares against a stored lower-cased copy
so the real name bytes are never altered to make a match. `count()` reports
matches independently of `limit`/`offset`, which is the row count a virtualized
model needs.

Name search uses an indexed `LIKE` rather than SQLite FTS5, which is an
optional compile-time feature this project should not silently depend on. If
name search ever needs more than substring matching over 12-character names,
FTS5 is the upgrade — after checking it is present.

## Export

`exportEntries` defaults to **original bytes**: the exact messages that arrived,
concatenated in order, not parsed, not re-chunked, not re-checksummed. That is
the only mode that can promise the user gets back precisely what the instrument
or the file gave them, and it is what "export what I imported" has to mean.

Original bytes are refused for anything that would move a Patch — a different
User slot, the temporary area, a different device ID. Re-addressing requires
re-encoding, which changes the bytes, so the caller has to say
`ReencodedFromModel` and mean it. An export labelled "original" that returned
re-encoded bytes would be a lie about provenance.

Re-encoding reports every change it made that the caller did not literally ask
for: each Patch that landed in a different slot than it came from, and each
substituted device ID. Exports that would lose data are refused with a reason
rather than truncated — several Patches into the one-Patch temporary area, a
run of slots past the 128-slot bank, a payload above the documented 128-byte
DT1 limit.

A test pins down a fact worth knowing: re-encoding does **not** reproduce the
instrument's own message shape. The fixture delivers each 129-byte Tone block in
a single DT1; the encoder splits at 128, so a Patch becomes nine messages rather
than five. Which behaviour the XP-60 actually requires is the open question in
`ROLAND_XP60_PROTOCOL_FACTS.md` §2.1, so sending stays conservative — and this
is exactly why the original bytes are preserved rather than regenerated.

## Import service

`services::LibraryImportService` turns importing files into one observable
operation. It owns what a service owns — file I/O, the worker thread, progress
and cancellation — while the decisions stay below it: `importSyxStream` decides
what a file contains, `LibraryDatabase` decides what is stored.

| Rule | Why |
|---|---|
| One transaction per file | A file lands whole or not at all |
| A failed file does not abort the batch | Work the user already waited for is not thrown away; the failure is reported per file |
| Cancellation is checked between files and before each write | A cancelled import never leaves a half-written file; committed work stays committed and is reported |
| Duplicates are counted before writing | The number means "already in the library", not "there because we just wrote it" |
| A file with no complete Patch is not an error | A Performance-only dump legitimately holds none; the counts say what was in it instead |
| Missing, empty and oversized files are refused with a reason | Not silently skipped |

Nothing deduplicates. Importing the same file twice stores both copies with
their own provenance and reports the overlap; the user decides what a duplicate
means.

## The Library screen

The result list is virtualized. The model asks the database how many rows match,
then fetches a page of 64 records only when a row is actually asked for — for a
`ListView`, the rows on screen plus its cache buffer. Scrolling a library of
thousands costs a handful of small indexed queries, and no Patch is decoded at
all: a row carries the name, the user's own metadata and enough provenance to
answer "where did this come from". A test jumps straight to the last row of 128
and reads it without touching the rows before it.

The screen is built the way a librarian scans, not the way the schema is shaped:

- the row leads with the name, then the state a scan picks up — favourite,
  rating, category, tags — then the provenance line;
- favourite and rating are direct-manipulation controls, not fields. Clicking
  the current rating clears it, so a mis-tap is undone by the same gesture;
- filter chips carry the vocabulary the library actually contains. Categories
  and tags come from `categoriesInUse` / `tagsInUse`, so no category is ever
  invented — the XP-60 Parameter Address Map defines no category byte;
- the count reads "12 of 128" whenever a filter is in force, so a filter can
  never quietly hide the rest of the library;
- the empty state distinguishes "the library is empty" from "no patch matches
  these filters", and offers no action this phase does not implement;
- narrow windows put the details panel below the results rather than crushing
  both into unreadable columns.

The inspector answers what the list cannot: full provenance as recorded, how
many bytes of original SysEx are preserved, and whether other copies of the same
sound exist. Nothing is merged or removed on its own.

`XpIcon` gained `star` and `star-filled`. Filled is the only icon in the set
that paints a solid shape: a favourite and a rating have to read as on/off from
across the screen, which an outline at that size does not.

The model exposes the user's own metadata for editing because that is the
library's job. It cannot touch Patch parameters, cannot send MIDI, and cannot
reach the protocol layer. A rejected edit changes nothing locally, so the row on
screen keeps saying what is actually stored.

The **Library** destination is now enabled in the navigation rail. In Demo Mode
the library is opened in memory, so a demo can never add to, or delete from, the
user's real collection; otherwise it is one file in the platform's
application-data directory. A library that fails to open is a warning, not a
refusal to start: the screen shows an empty library instead.

## Evidence

Tested against `tests/fixtures/xp60/user-bank-amal.syx`, a real XP-60 user bank
(evidence rank 4; see `tests/fixtures/xp60/README.md`), so the structure and the
queries run against real names, real provenance and real Patch data:

- all **128** User Patches import, in slot order, with no partial, rejected or
  range-warning results and a clean stream report;
- each entry's kept bytes equal the source file slice its provenance names;
- provenance reports device ID 17, single-byte model `6A`, and the fixture's
  documented SHA-256 for all 128 entries;
- the fixture's **1 314** data sets less the 640 that make up the 128 Patches
  are reported as unattributed — the Performance-area blocks this project has
  not transcribed are counted, never decoded as something they might not be;
- the whole bank stores in one transaction, survives a close and reopen on
  disk, and every entry's bytes come back byte-for-byte;
- Patches rebuilt from stored bytes match the originals parameter for
  parameter, with the same fingerprint, name, slot, address and import time;
- an original-bytes export of the whole bank is byte-identical to the
  concatenated preserved bytes, and re-imports to the same 128 Patches with a
  clean stream;
- a re-encoded export re-imports to the same parameters and fingerprints, and
  into the requested slots when re-addressed;
- the service imports real files off the UI thread, reports per-file progress,
  survives an unreadable file mid-batch, and cancels between files with the
  already-committed file intact;
- the screen lists all 128 real names, pages without reading rows it does not
  show, states what a filter hides, and keeps results and details usable at the
  1024-pixel minimum width.

The supplied bank turns out to contain **11 pairs** of patches whose parameters
are byte-for-byte identical. That is a fact about the user's own data, not a
defect — real libraries accumulate copies — and it is asserted so that a change
to what the fingerprint covers shows up as a change in that number. It is also
why the duplicate reporting is worth having.

Constructed streams cover the partial, empty-slot, duplicate, stream-anomaly
and Performance-part cases. Those streams carry real Patch bytes taken from the
fixture: a first attempt used constant filler, which the codec correctly
refused because a nibble parameter rejects any byte above `0FH`. The test was
wrong, not the importer.

Validation: **31/31 CTest suites pass** on the Windows Qt 6.11.2 / MinGW 13.1
build, including `tst_library_import` (16 checks), `tst_library_database`
(20 checks), `tst_library_export` (15 checks), `tst_library_model` (18 checks)
and the QML suite's new `LibraryScreen` tests. No MIDI port was opened and no hardware write was performed.

The one hardware-dependent claim in this phase — that a `.syx` XP60Studio
exports is accepted by the instrument — is area 10 of
[`DEVICE_ACCEPTANCE.md`](DEVICE_ACCEPTANCE.md) and is not claimed here.

## Remaining in this phase

1. **Import and export from the Library screen** — the service and the export
   are implemented and tested, but no on-screen action calls them yet, so the
   screen is currently read-and-organise only. This is the next step.
2. **M1 Dashboard**, last, once there is real data behind its summaries.
3. ~~Area 10 of [`DEVICE_ACCEPTANCE.md`](DEVICE_ACCEPTANCE.md): confirm on the
   instrument that an exported `.syx` is accepted and reproduces its Patches.~~
   **Done 2026-09-04.** An exported file was sent to a physical XP-60 and the
   Patch read back equalled the one exported, parameter for parameter, four
   times out of four. See `HARDWARE_VALIDATION_XP60.md` § area 10. The
   verification covers a single Patch exported to the temporary area; a
   multi-Patch export to User bank slots writes permanent memory and is
   deliberately not part of it.
4. Screenshot review of the Library screen against the master mockup, per
   `design/UI_ACCEPTANCE_CRITERIA.md`.
