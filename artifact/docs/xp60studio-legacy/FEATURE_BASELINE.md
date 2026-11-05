# XP60Studio Feature Baseline and Differentiation

This document separates the **minimum mature editor/librarian baseline** from the capabilities XP60Studio should add beyond that baseline.

Patch Base is used only as a market/product reference for expected functional coverage. XP60Studio must not copy its proprietary implementation, source, UI assets, or branding.

---

# Baseline: Mature XP-60 Editor / Librarian

XP60Studio should eventually support the practical capabilities expected from a serious XP-60 editor:

## Patch editing

- Patch Common parameters
- all four Tone layers
- Tone enablement
- waveform selection
- Pitch
- Pitch envelopes
- TVF
- TVF envelopes
- TVA
- TVA envelopes
- LFO
- key/velocity behavior
- modulation/controllers
- structure parameters
- MFX
- Chorus
- Reverb

## Editing productivity

- Tone copy/paste
- envelope copy/paste
- patch copy/duplicate
- exact-value editing
- live parameter updates
- undo/redo where appropriate
- randomization / variation support

## Hardware communication

- select MIDI IN/OUT
- fetch temporary/current data
- send temporary/current data
- receive User data
- send User data
- robust SysEx handling
- device ID support

## Librarian

- browse patches
- organize patches
- import SysEx
- export SysEx
- individual-patch management
- bank management
- User Patch bank handling

## Additional XP-60 areas

- Performance editing
- 16-Part handling
- Rhythm editing
- relevant System data

This baseline must be accurate before XP60Studio is described as a complete XP-60 editor.

---

# XP60Studio Differentiators

The product should go significantly beyond a conventional parameter editor in the following areas.

## 1. Visual sound architecture

Instead of making users mentally reconstruct Tone routing from many parameter pages, show the patch structure visually.

Users should immediately understand:

- which Tones are active
- which waveforms each Tone uses
- octave/tuning relationships
- level/pan relationships
- filter/amplitude path
- effects path

## 2. Four-Tone audition workflow

Provide temporary Solo/Mute behavior so the user can isolate each Tone without destructively rewriting the patch.

## 3. Searchable waveform browser

Replace raw numeric waveform selection with:

- search
- categories
- source board
- availability
- expansion requirements

## 4. Expansion-aware compatibility

Model the user's EXP-A/B/C/D boards and analyze imported patches against that physical configuration.

Show compatibility at Patch and Tone level.

## 5. Verified transfer workflow

Treat writes as operations that can be checked.

Where hardware permits:

`send -> read back -> compare -> verify`

Report individual failures rather than declaring a bulk operation successful merely because bytes were transmitted.

## 6. Large-library analysis

Allow the user to import many old banks and analyze them together.

Report:

- total patches
- unique patches
- exact duplicates
- near duplicates
- unsupported patches
- invalid/corrupt data
- source provenance

## 7. Parameter-level duplicate detection

Two differently named patches with identical synthesis parameters should be recognized as duplicates.

## 8. Explainable near-duplicate detection

Show exactly which parameters differ and by how much.

## 9. Patch DNA

Derive useful human-readable descriptors from actual patch parameters while clearly labeling them as calculated metadata, not Roland data.

## 10. Structural search

Eventually search by concepts such as:

- number of active Tones
- internal vs expansion waves
- specific expansion board
- envelope character
- stereo width
- filter type
- effects usage
- compatibility

## 11. Visual comparison

Compare two patches side-by-side while suppressing unchanged noise and emphasizing meaningful differences.

## 12. Component transplant

Copy meaningful components between patches, such as:

- one Tone
- an envelope
- LFO settings
- effects setup

## 13. A/B and local version history

Keep the original state available while editing and retain lightweight local versions.

## 14. Intelligent constrained variation

Let users randomize or vary selected portions of a patch rather than generating arbitrary unusable parameter combinations.

## 15. Mutation/evolution workflow

Generate multiple controlled descendants, audition them, select one, and continue evolving from that point.

## 16. Semantically safe morphing

Interpolate only continuous parameters. Handle discrete values such as waveform IDs with explicit rules.

## 17. Bank engineering

Treat a 128-patch bank as a musical collection with user-defined sections, categories, warnings, provenance, and drag/drop management.

## 18. Smart bank proposal

Build an editable 128-slot proposal from the user's own library using category allocation, compatibility, favourites, and duplicate avoidance.

## 19. Snapshot confidence

Capture and restore supported XP-60 state with clear verification and safety-backup workflows.

## 20. Live-performance mode

Organize songs/setlists and provide a stage-oriented UI without exposing destructive editing controls.

## 21. Optional audio previews

Later, record actual XP-60 audio previews so large libraries can be auditioned quickly without constantly changing hardware patches.

## 22. Sound-aware search

Only after deterministic metadata and/or real audio previews exist, support natural musical searches such as "bright brass with long fall" or "soft wide strings."

---

# Product Position

A conventional editor answers:

> Which parameter do you want to change?

XP60Studio should additionally answer:

> What is this patch made of?

> Why do these two sounds differ?

> Will this patch work on my XP-60?

> Which of my thousands of patches are actually unique?

> What did I change?

> Did the keyboard really receive the data?

> Can I safely get back to where I was?

That distinction should guide product decisions throughout the project.
