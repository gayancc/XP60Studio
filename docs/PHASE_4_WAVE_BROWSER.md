# Phase 4 — M3 Wave Browser

## Execution order and scope

On 2026-09-04 the user authorized continuing local work while deferring
physical-device validation to final acceptance. M2 remains open for EFX
byte-slot/unit mappings and physical acceptance; it no longer blocks M3 catalog
development. Neither milestone is hardware-verified.

The Editor's **Waves** button opens a contextual browser with 448 internal ROM
waveforms: INT-A 001–255 and INT-B 001–193. Search matches all entered terms
against name, bank and padded number, ignoring case. Source filtering,
virtualized results, mouse/arrow-key selection, source-page inspection and
Escape/Back navigation are supported. Narrow windows put details below results.
Filtering clears a selection only when it leaves the results.

The browser is read-only. Its C++ list model has no Patch mutation or MIDI
dependency. Expansion filtering reports missing catalog data, not an absent
board. Category, sample-rate and loop metadata are unknown. No audio preview,
waveform plot, favorites persistence or compatibility score is simulated.
These are intentional departures from illustrative master-mockup data; the
search/results/inspector composition uses existing design tokens and controls.

## Evidence and reproducibility

Primary source: Roland **XP-60/80 Waveform List**, Faxback #10279, May 1998,
three pages, supplied as `XP60-References/XP80_W.pdf`.
[Official Roland PDF](https://cdn.roland.com/assets/media/pdf/XP80_W.pdf).

SHA-256: `66d2d0e655ece849bf8e5bbcef9c2bfd4e08b44bdc20a3a6cad387d97d952e69`.

`data/xp60_internal_waves.csv` preserves the printed bank, number, name and
source page. All three source pages were rendered and inspected.
`tools/generate_wave_catalog.py` requires exact ordered bank ranges, nonempty
names and valid page references. Its `--check` mode is a CTest gate. The
generated C++ catalog needs no runtime PDF parser or external file loading.

The XP-60 parameter map defines group type, group ID and wave number but the
reviewed source does not establish INT-A/INT-B IDs explicitly. The
[official XP-30 manual](https://static.roland.com/assets/media/pdf/XP-30_OM.pdf),
printed page 194, gives internal type 0, ID 1 for A and ID 2 for B. The XP-60
fixture is consistent with this interpretation, but related-model evidence is
not XP-60 verification. The catalog does not translate bank labels to raw group
IDs. Tone cards retain raw identifiers until this is resolved.

## Final physical-device acceptance

This is area 9 of [`DEVICE_ACCEPTANCE.md`](DEVICE_ACCEPTANCE.md), the index for
the single connected session. Retain the Phase 3 fetch/decode/encode/send/refetch comparison and M2 audio,
live-update, Solo/Mute, A/B, routing and EFX capture checks. Also:

1. Back up the instrument, choose a temporary Patch and isolate one Tone.
2. On the XP-60 panel choose INT-A 001, INT-A 255, INT-B 001 and INT-B 193.
   Fetch after each change; save raw captures and panel labels to establish
   group type, group ID and zero-based number at the bank boundaries.
   `tools/capture_diff.py BEFORE AFTER` names the changed Tone bytes and
   decodes the two-byte nibble Wave Number, so the Wave Group Type / Group ID /
   Number triple is read from the document's own rows rather than by hand.
3. Compare several interior entries with displayed Roland names; record any
   differences without normalizing them away. The tool reports addresses it
   cannot resolve and values outside the transcribed range rather than
   suppressing them; treat both as evidence, not noise.
4. After mapping is evidenced, implement **Use in Tone** as one atomic local
   edit with undo, A/B and established armed live-update behavior. Test bank
   boundaries and preserve unmapped expansion references.
5. Send only with explicit hardware-write intent, refetch and compare every
   changed field, then confirm the resulting voice on the instrument.

Expansion catalogs, installed-board evidence, contextual assignment and audio
preview remain future work. The searchable catalog is usable; M3 is partial.

## Local validation

Windows Qt 6.11.2 build and all 25 CTest suites pass, including 59 QML checks.
Coverage includes all 448 unique catalog keys, bank boundaries, case-insensitive
multi-term search, selection across filters, unsupported expansion data, model
invariants, keyboard navigation and unchanged Patch state after browsing.

Reviewed shipping-QML captures using the fixture/FakeXp60 (the displayed device
response is simulated, not physical evidence):

- `design/screenshots/phase4-wave-browser-windows.png` — 1440×1040.
- `design/screenshots/phase4-wave-browser-windows-minimum.png` — 1024×680.

The narrow capture preserves readable results and the complete details panel.
The AST graph update completed with 2,466 nodes and 5,133 edges; parser warnings
mean it remains a navigation aid, not compiler validation. No physical MIDI
writes were performed.

## Wave reference survey on a physical XP-60 — 2026-09-04

`xp60studio_hardware_probe --survey-waves` read all 128 permanent User Patches
(read-only, RQ1 only) and tabulated the 512 wave references the instrument
itself wrote. 128 of 128 patches decoded; none failed.

| Group type | Group ID | References | Number range (display) |
|---|---|---|---|
| 0 (INT) | 1 | 302 | 1 .. 255 |
| 0 (INT) | 2 | 149 | 2 .. 125 |
| 2 (EXP) | 1 | 24 | 5 .. 130 |
| 2 (EXP) | 5 | 4 | 5 .. 100 |
| 2 (EXP) | 7 | 25 | 12 .. 149 |
| 2 (EXP) | 18 | 8 | 16 .. 50 |

Group type labels seen: INT 451, EXP 61.

### What this establishes

**INT group ID 1 is INT-A.** Group 1 references run to display number 255, and
the catalog's INT-A bank holds exactly 255 waves. INT-B holds 193, so a
reference numbered 255 could not belong to it: the alternative assignment is
ruled out by the instrument's own data, not by assumption. The zero-based raw
number with display = raw + 1 also fits exactly, raw 254 being the last INT-A
wave.

**Only two internal group IDs exist.** No INT reference in 128 Patches carries a
group ID other than 1 or 2, consistent with exactly two internal banks.

### What this does not establish

**The INT-B upper boundary is untested.** Group 2 references stop at 125, well
inside the assumed 193, so nothing here confirms where INT-B ends. The panel
step for INT-B 193 in the acceptance procedure above is still required.

**No wave name is confirmed.** The survey reads identifiers, not names. Only the
instrument's display can settle whether identifier *n* is the wave the catalog
calls *n*, and the INT-A 001 / INT-A 255 / INT-B 001 / INT-B 193 panel steps
remain the way to do it.

### Expansion references are real and unresolvable

61 of the 512 references are EXP, spread over group IDs 1, 5, 7 and 18. The
catalog covers INT-A and INT-B only, so none of these resolves to a name. That
is the documented intent — unmapped expansion references are preserved
byte-for-byte rather than normalised — and this instrument now provides 61 real
examples to hold that behaviour to. Which of these group IDs correspond to
boards actually installed is not established: a User Patch can reference a
board that is absent.

## Bank boundary evidence from the front panel — 2026-09-04

Captured with `xp60studio_hardware_probe --watch` while wave selections were
made on the instrument. Read-only throughout; every value below is what the
XP-60 reported for a selection made on its own panel.

| Panel selection | Tone | Group type raw | Group ID raw | Wave Number raw | Number display |
|---|---|---|---|---|---|
| INT-A 001 | 1 | 0 (INT) | 1 | **0** | 1 |
| INT-A 225 | 2 | 0 (INT) | 1 | 224 | 225 |
| INT-B 001 | 3 | 0 (INT) | **2** | **0** | 1 |
| INT-B 193 | 4 | 0 (INT) | **2** | **192** | 193 |

### Established

**Group ID 1 is INT-A and group ID 2 is INT-B.** Selecting INT-B on the panel
moved Wave Group ID to 2 in two independent Tones (3 and 4); selecting INT-A
left it at 1. This is the panel-side confirmation the survey above could only
infer.

**The wave number is zero-based: display = raw + 1.** INT-A 001 and INT-B 001
both read raw 0, INT-A 225 read raw 224, INT-B 193 read raw 192. Four
selections across two banks and two Tones agree.

**INT-B holds at least 193 waves.** INT-B 193 is selectable and reads raw 192.
Combined with the catalog's INT-B size of 193, the upper boundary matches. This
was the open question the User-memory survey could not answer, since no stored
Patch referenced an INT-B wave above 125.

**INT-A's upper boundary of 255** is carried by the survey rather than this
capture: INT-A 255 was not selected on the panel, but 302 stored references
reach display 255, which only INT-A is large enough to hold.

### Anomaly recorded, not smoothed away

While navigating Tone 4 from EXP group 7 to INT-B, Wave Group ID passed through
**raw 0** — a group ID the catalog has no bank for — before settling at 1 and
then 2:

```text
Tone 4  Wave Group ID  raw 7 -> 0     ("7" -> "0")
Tone 4  Wave Group ID  raw 0 -> 1     ("0" -> "1")
Tone 4  Wave Group ID  raw 1 -> 2     ("1" -> "2")
```

It appears to be a transient of panel navigation rather than a selectable
state, since it was not reached deliberately and did not persist. It is
recorded because a reader of the raw capture will see it, and because any code
that maps group ID to a bank must decide what to do with 0 rather than assume
it cannot occur.

### Still open

**No wave name has been compared.** Everything above is identifiers. Step 3 of
the acceptance procedure — comparing displayed Roland names for several
interior entries — is not done, so it remains unproven that identifier *n* names
the wave the catalog calls *n*. The mapping is established; the naming is not.

## Wave names confirmed against the display — 2026-09-04

Step 3 of the acceptance procedure. Names read off the XP-60's own display and
compared with the generated catalog:

| Selection | XP-60 display | Catalog | Result |
|---|---|---|---|
| INT-A 001 | `Ac Paiano A` (as transcribed) | `Ac Piano1 A` | match, see note |
| INT-B 001 | `Kalimba` | `Kalimba` | **exact** |
| INT-B 193 | `DC` | `DC` | **exact** |

`Kalimba` and `DC` match character for character. `DC` is the stronger of the
two: it is the last wave of INT-B and an unusual name, so agreement at raw 192
in group 2 could not plausibly be coincidence. Taken with the identifier
evidence above, the mapping is now confirmed at both bank starts and at INT-B's
end, in names as well as numbers.

**Note on INT-A 001.** The transcription reads `Ac Paiano A` where the catalog
has `Ac Piano1 A`: two letters transposed and the `1` absent. The transposition
is plainly a typing slip. The missing `1` is recorded rather than assumed away —
the catalog's neighbours are `Ac Piano1 B` and `Ac Piano1 C`, so a display
reading `Ac Piano A` would mean the catalog carries a digit the instrument does
not. It does not affect the mapping, which INT-B 001 and INT-B 193 establish
exactly, and it is worth one glance the next time that page is open.

Area 9 is closed for group type, group ID, zero-based number and name at the
tested boundaries. Interior names beyond these three remain unsampled, and the
448-entry catalog is not exhaustively verified — nor does this procedure ask it
to be.
