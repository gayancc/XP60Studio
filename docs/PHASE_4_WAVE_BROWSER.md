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

Retain the Phase 3 fetch/decode/encode/send/refetch comparison and M2 audio,
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
