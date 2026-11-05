# XP60Studio UI Acceptance Criteria

This document defines how UI work is accepted against the approved master mockup.

Master mockup: [`xp60studio-ui-master-mockup.jpg`](xp60studio-ui-master-mockup.jpg)

The four anchor screens must be implemented to closely match the mockup, not merely resemble its theme.

---

# 1. General acceptance rule

A UI feature is not complete when it is functionally wired but visually generic.

For anchor screens, completion requires both:

1. correct application/domain behavior; and
2. visual/interaction fidelity to the approved mockup.

Codex should compare implementation screenshots against the master mockup during UI work.

---

# 2. Required visual fidelity

Verify:

- persistent navigation rail structure
- screen header placement
- dark surface hierarchy
- card/panel boundaries
- spacing rhythm
- typography hierarchy
- button hierarchy
- Tone color identity
- control density
- major proportions
- inspector positioning
- table/list/grid density where applicable
- status/warning visual hierarchy
- connection status treatment
- no accidental stock Qt visual appearance

Pixel-identical rendering across Windows/macOS is not required where native font rasterization differs, but composition should remain visibly equivalent.

---

# 3. Dashboard acceptance

Must contain and visually match the mockup structure for:

- current patch hero
- patch character tags
- four-Tone contribution visualization
- Edit Patch / Compare / Save As / Add to Bank quick actions
- Library summary card
- Bank Builder summary card
- Device Diagnostics summary card
- persistent XP-60 LIVE/connection treatment

Reject implementations that replace this with tiles of generic statistics or a table.

---

# 4. Patch Editor acceptance

Must contain and visually match:

- four large Tone cards across the primary editor workspace at normal desktop width
- stable per-Tone color identity
- wave, level, pan, octave, Solo, Mute in each Tone card
- visible mini envelope/contribution information
- Sound / Filter / Amp / Motion / Effects navigation
- visible signal path from Tones to Structure -> MFX -> Chorus -> Reverb -> Output
- large graphical envelope editor
- key range and velocity visual controls
- contextual Tone settings panel
- explicit Write to XP-60 action
- A/B and local/hardware state distinctions when those features are implemented

Reject a property-grid or tabbed form that hides the four-Tone architecture.

---

# 5. Wave Browser acceptance

Must contain and visually match:

- prominent search field
- source filter chips
- category filter chips
- dense virtualized result list
- availability/missing-expansion badges
- clear wave source information
- right-side details inspector
- compatibility panel

Reject a simple ComboBox-only waveform selector as the primary browser.

---

# 6. Bank Builder acceptance

Must contain and visually match:

- bank selector/header
- bank analysis summary metrics
- left category/section rail
- central visual slot grid representing the 128 slots
- readable slot identities
- selection and drag/drop affordances
- patch comparison inspector
- compatibility/warning inspector
- transfer-related actions/status where implemented

Reject a plain 128-row table as the primary experience.

---

# 7. Interaction acceptance

All visible interactive controls must have working behavior for the phase in which they are introduced.

Do not include decorative fake buttons to match the mockup.

Where the mockup shows a future feature not yet backed by the current roadmap phase, either:

- omit it temporarily while preserving layout space intentionally; or
- show a clearly disabled control only when that state is useful and not misleading.

Do not wire placeholder behavior and mark it complete.

---

# 8. Component fidelity

Reusable components must be implemented once and used consistently.

Examples:

- ToneCard
- XpKnob
- EnvelopeEditor
- StatusPill
- ConnectionStatusIndicator
- BankSlot
- WaveAvailabilityBadge
- TransferProgressPanel

If two screens show the same conceptual component but look materially different without a product reason, the design system is not being followed.

---

# 9. Responsive acceptance

At the primary desktop target size, the anchor screens should match the master mockup composition closely.

At reduced widths:

- preserve hierarchy
- collapse secondary inspectors before shrinking core controls below usable size
- use drawers/tabs/scrollable strips for secondary content
- never cause whole-window horizontal scrolling
- preserve four-Tone identity even if the Tone layout must become selectable/scrollable

Responsive adaptation is not permission to redesign the normal desktop layout.

---

# 10. Screenshot review requirement

For every PR that materially implements or changes an anchor screen, include implementation screenshots in the PR or project review output where the development environment permits.

Review the screenshot against:

- the master mockup
- this acceptance document
- the screen/feature map

Any meaningful deviation should be explained.

---

# 11. Visual-regression direction

Once the first polished anchor screen is stable, establish a repeatable screenshot/visual-regression workflow if practical for the chosen CI/platform setup.

Visual regression is supplementary. It does not replace semantic tests, accessibility checks, or human review.

---

# 12. Definition of visually complete

A screen is visually complete when a reviewer can place the implementation screenshot beside the approved mockup and recognize the same product, composition, hierarchy, interaction model, and design system without needing to excuse a generic framework-default interpretation.
