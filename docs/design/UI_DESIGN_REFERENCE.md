# XP60Studio UI Design Reference

This document records the approved visual target for XP60Studio.

## Master mockup

![XP60Studio approved UI master mockup](./xp60studio-ui-master-mockup.jpg)

The image is the **authoritative implementation target** for the visual language and the four anchor screens:

1. Dashboard / Command Center
2. Patch Editor / Four-Tone Mixer
3. Wave Browser
4. Bank Builder / Library Intelligence

Codex should reproduce these screens as closely as technically possible rather than treating the image as loose inspiration.

## Fidelity requirement

For the four anchor screens, match the mockup closely in:

- application shell and navigation rail
- major panel placement
- content hierarchy
- dark theme and surface layering
- spacing and density
- component proportions
- tone-card layout
- Tone 1/2/3/4 color semantics
- signal-flow composition
- envelope-editor placement and visual treatment
- quick-action placement
- right-side summary/inspector panels
- search/filter composition
- bank-grid composition
- status badges and warning placement
- typography hierarchy
- border/radius/shadow character
- control grouping
- overall premium music-software character

Do not simplify the mockup into a generic form, property grid, admin dashboard, or stock Qt application.

## Allowed deviations

A deviation from the mockup is acceptable only when one of these applies:

1. verified XP-60 behavior makes the depicted interaction inaccurate;
2. accessibility requires a change;
3. Windows/macOS platform behavior requires a change;
4. the mockup contains illustrative data that is not supported by verified device metadata;
5. a measured layout must adapt at smaller desktop sizes;
6. implementation testing demonstrates a clear usability problem.

When a major deviation is necessary, document it in the relevant implementation/PR notes rather than silently redesigning the screen.

The rule is: **preserve the mockup exactly where it represents valid product behavior; change only what must change.**

## Design intent

The product should feel like premium professional music-production software rather than an enterprise CRUD/admin application.

Preserve these characteristics:

- dark, focused working environment suitable for long sound-design sessions
- strong information hierarchy rather than dense property grids
- clear XP-60 connection state at all times
- musical, visual representation of the four-Tone architecture
- stable color-coded Tone identity across related controls and routing
- visual envelopes and signal flow instead of relying only on numeric forms
- compact but readable panels with meaningful grouping
- progressive disclosure: musician-friendly first, exact technical depth available when requested
- clear separation between local state, modified state, and data actually written to the XP-60
- fast scanning for wave compatibility, missing expansions, duplicates, bank occupancy, and warnings
- bank organization that feels like arranging sounds for a performance rather than editing spreadsheet rows

## Screen-specific direction

### Dashboard

Match the top-left mockup panel: persistent left navigation, current-patch hero, four-Tone contribution, Quick Actions, and right-side Library / Bank Builder / Device Diagnostics cards.

Do not turn it into a generic KPI dashboard.

### Patch Editor / Four-Tone Mixer

Match the top-right mockup panel: four large Tone cards, Sound/Filter/Amp/Motion/Effects navigation, signal flow from Tones through Structure/MFX/Chorus/Reverb/Output, large envelope editor, key/velocity controls, and contextual Tone settings.

The four Tone cards are the main visual anchors. The user should immediately understand active waves, contribution, tuning/octave, pan, mute/solo state, and routing.

### Wave Browser

Match the bottom-left mockup panel: search, source/category filters, high-density result list, availability badges, and a right-side details/compatibility inspector.

Missing expansion requirements must never be hidden or silently substituted.

### Bank Builder

Match the bottom-right mockup panel: bank selector, summary metrics, category rail, visual 128-slot bank grid, drag/reorder affordance, comparison panel, and compatibility/warning panel.

Do not reduce the bank experience to a plain table.

## Supporting implementation documents

- [`UI_IMPLEMENTATION_ARCHITECTURE.md`](UI_IMPLEMENTATION_ARCHITECTURE.md)
- [`COMPONENT_CATALOG.md`](COMPONENT_CATALOG.md)
- [`SCREEN_AND_FEATURE_MAP.md`](SCREEN_AND_FEATURE_MAP.md)
- [`UI_ACCEPTANCE_CRITERIA.md`](UI_ACCEPTANCE_CRITERIA.md)

## Implementation timing

Do not build advanced screens before their backing protocol/domain roadmap phases are ready. The existence of a finished mockup does not justify placeholder backend logic.

When a screen becomes eligible for implementation, build it against the approved mockup and acceptance criteria from the beginning rather than creating a temporary generic UI intended to be redesigned later.
