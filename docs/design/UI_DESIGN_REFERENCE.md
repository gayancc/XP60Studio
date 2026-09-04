# XP60Studio UI Design Reference

This document records the approved high-level visual direction for XP60Studio.

## Master mockup

![XP60Studio approved UI master mockup](./xp60studio-ui-master-mockup.jpg)

The image is an approved **design direction**, not a pixel-perfect implementation contract. It is the primary visual reference for the application's overall character and the first four major UX surfaces:

1. Dashboard / command center
2. Patch Editor / four-Tone mixer
3. Wave Browser
4. Bank Builder / library intelligence

## Design intent

The product should feel like premium professional music-production software rather than an enterprise CRUD/admin application.

Preserve these characteristics when implementing the UI:

- dark, focused working environment suitable for long sound-design sessions
- strong information hierarchy rather than dense property grids
- clear XP-60 connection state at all times
- musical, visual representation of the four-Tone architecture
- color-coded Tone identity used consistently across related controls and signal-flow visualization
- visual envelopes and signal flow instead of relying only on numeric forms
- compact but readable panels with meaningful grouping
- progressive disclosure: musician-friendly first, exact technical depth available when requested
- clear separation between local state, modified state, and data actually written to the XP-60
- fast scanning for wave compatibility, missing expansions, duplicates, bank occupancy, and warnings
- bank organization that feels like arranging sounds for a performance rather than editing spreadsheet rows

## Screen-specific direction

### Dashboard

Treat this as a command center. Prioritize current patch, four-Tone contribution, device status, library state, bank activity, and obvious next actions. Do not turn it into a generic KPI dashboard.

### Patch Editor / Four-Tone Mixer

The four Tone cards are the main visual anchors. The user should immediately understand which waves are active, their relative contribution, tuning/octave, pan, mute/solo state, and how the Tones flow into Structure, MFX, Chorus, Reverb, and output.

Graphical Pitch/TVF/TVA envelope editing should be central to the design workflow. Exact XP values remain accessible in expert contexts.

### Wave Browser

Make source and expansion compatibility obvious. Searching and filtering should be fast enough for large libraries. Missing expansion requirements must never be hidden or silently substituted.

### Bank Builder

Treat the 128-slot bank as a performance-oriented sound collection. Provide category organization, drag/drop, compatibility warnings, duplicate analysis, comparison, and clear occupancy/state feedback without reducing the experience to a plain table.

## Implementation rule

Do not build these advanced screens before the protocol and XP-60 model phases identified in `docs/ROADMAP.md` are ready. When the project reaches the corresponding UI phases, use this mockup together with `docs/PRODUCT_VISION.md`, `docs/FEATURE_BASELINE.md`, and `docs/ARCHITECTURE.md`.

If an implementation must diverge from the mockup because of real XP-60 behavior, accessibility, platform constraints, or superior validated UX, preserve the design principles rather than blindly reproducing pixels.
