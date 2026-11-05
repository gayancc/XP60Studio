# XP60Studio UI/UX Audit and Redesign Plan

Evidence base: the shipping QML, rendered headless through
`tests/tools/screenshot_harness.cpp` at 1024x680 (minimum supported) and
1440x900 (preferred), in both **Offline** and **Connected + Verified** states,
compared against `docs/design/xp60studio-ui-master-mockup.jpg` and
`UI_ACCEPTANCE_CRITERIA.md`.

The conclusion up front: the design *system* already exists and is largely
sound (Theme/Metrics/Typography/Motion singletons, 45 `Xp*` components,
synth-specific controls such as `XpKnob`, `EnvelopeEditor`, `FourToneMixer`,
`KeyboardStrip`, `SignalFlowNode`). The failures are **not** missing
foundations. They are four systemic defects that then surface as dozens of
visible finishing errors on every screen.

---

## 1. Root causes

### R1 - Type scale is in points, layout is in pixels

`Typography.qml` declares the ramp in **points** (`bodySize: 12`,
`headingSize: 14`, `displaySize: 26`), while every control height, pill height
and row height in `Metrics.qml` is a **pixel** constant (`controlHeight: 32`,
`StatusPill` 22, nav row 36, `ConnectionStatusIndicator` 26/30/44/46).

At 96 dpi, 12 pt body renders at **16 px** and 14 pt heading at **18.7 px**.
Text is therefore about 25% larger than the containers were sized for, and the
relationship changes again at 125%/150% OS scaling because point sizes also
follow the OS font-size setting while the pixel heights do not.

This one mismatch produces, directly:

- text pressed against container edges and vertical clipping;
- the header pill growing to 46 px inside a 52 px header (3 px clearance);
- navigation labels overflowing the rail;
- a 29 px "display" page title in an application that needs desktop density;
- unpredictable behaviour at non-100% scaling.

It is also why previous passes read as "cosmetic": adjusting individual
paddings cannot fix a global unit mismatch.

### R2 - Connection state has no single owner

`ConnectionState` is re-derived and re-worded independently in `AppHeader`,
`AppNavigationRail`, `ConnectionStatusIndicator`, `DeviceConnectionCard`,
`DeviceHealthCard` and `DevicesScreen`. Each site maps the enum to its own
tone and its own sentence.

Counted on the Offline capture at 1440x900, the shell states "not connected"
**ten** times: header pill (plus an elided instruction sentence),
connection-card pill, flow node, three consecutive guidance paragraphs, four
health chips, the rail chip, the rail detail line, the rail hint - and the rail
hint reads "Open Devices to connect" *while the user is on the Devices screen*.

### R3 - Grouping is expressed with borders instead of structure

`XpCard` is the only grouping primitive, so every nested group becomes another
bordered rounded rectangle. "Current Sound", "Send to XP-60" and "Protocol
Activity" are each `XpCard` -> bordered `Rectangle` -> content, which puts two
parallel 1 px borders 16 px apart. There is no border-free section primitive,
so proximity, surface change and typography are never used for hierarchy.

### R4 - Interactive affordance is optional

`XpButton variant: "ghost"` is fully transparent at rest - no fill, no border.
Every secondary action therefore renders as bare text: "Show counters",
"Connection options", "Scan", "Expand", "Clear", "Show raw parameters
(Expert)", "Browse waves". Meanwhile `StatusPill` is *more* prominent than
those real buttons, so the non-interactive "Read only" badge reads as the
brightest control in its row. Additionally `QQC.Switch` is used raw with the Qt
**Basic** style, so the Advanced-diagnostics toggle is a stock grey/white
widget with no relationship to the design language.

---

## 2. Screenshot findings

Numbered against the Offline capture at 1440x900 (the state in the reported
screenshot) and the 1024x680 minimum.

### Boundary and collision

| # | Finding | Cause |
|---|---|---|
| S1 | `Performance COMING SOON` ends about 13 px **past** the rail's right divider and is drawn over the content region; `Dashboard COMING SOON` touches it. Reproduces at 1024 and 1440. | `AppNavigationRail` sets `Layout.minimumWidth: implicitWidth` on the label, so the row cannot shrink, plus a full-size availability label the mockup does not have at all. |
| S2 | Header identity pill occupies y 6-46 of a 52 px header and its sentence is elided mid-word: `SELECT YOUR MIDI PORT..., THEN CLICK CONNECT.` | R1 plus `ConnectionStatusIndicator` hard-coding 46 px for the two-line variant. |
| S3 | Rail device block wraps mid-token: `MIDI IN XP-60 IN - MIDI` / `OUT XP-60 OUT`. | Full connection detail placed in a 200 px column. |
| S4 | Protocol Activity panel is clipped by the window edge on the Connected capture. | `Layout.fillHeight: true` combined with `implicitHeight: expanded ? 480 : 120` inside a `ScrollView`. |

### Proportion and empty space

| # | Finding |
|---|---|
| S5 | The primary task column (Device connection) is the **narrower** of the two (475 px vs 665 px at 1440), while the wider column holds the mostly-empty "Send to XP-60" card. Column width does not follow content importance. |
| S6 | About 475x180 px of dead space below the left column, and a 62 px dead band across the window bottom, while the connection flow nodes are squeezed (the `Roland XP-60 / OFFLINE` node is 121 px for a 100 px string). |
| S7 | `DevicesScreen` passes `title: ""` to `ScreenHeader`, which still reserves a display-size line. The result is a 40 px empty band with the "Scan for MIDI devices" button orphaned at the top-right. |

### Alignment and tiny detail

| # | Finding |
|---|---|
| S8 | In Connection options, `From XP-60` / `To XP-60` fields start at x = 376 but `Speed` starts at x = 332 - a 44 px misalignment, because Speed lives in a sibling `RowLayout` instead of the two-column `GridLayout`. |
| S9 | The rail wordmark starts at x = 20, the nav icon column at x = 24 - a 4 px misalignment between the two most prominent left-edge elements. |
| S10 | Editor Tone cards: the `Level` / `Pan` labels sit on a roughly 6 px different baseline from `Octave`, because Octave is an `XpSpinField` (32 px) in a row with knob labels. |
| S11 | Selected Tone card uses a 2 px border where unselected use 1 px, so selecting a card shifts its inner content by 1 px. |
| S12 | Nav focus ring is drawn 1 px inside a 1 px current-item border, producing a double line on the current item. |
| S13 | `XpComboBox` uses the text character for a chevron and `DeviceConnectionCard` uses a text arrow for its flow arrows - glyphs with a different stroke weight and optical centre from the `XpIcon` family. |
| S14 | `1 events` - no pluralisation. |
| S15 | Health chips mix semantics in one row: `Studio / XP60STUDIO` (entity/name) beside `MIDI / CLOSED` and `XP-60 / OFFLINE` (entity/state). |
| S16 | Editor: `Waves` sits directly beside `PATCH EDITOR`, which is a *static badge* built as a `Rectangle` with the same fill, border, radius and height as the button — so a non-interactive label reads as the second half of a pair of controls. |
| S17 | ~~Editor Key Range draws pure-white keys~~ **Withdrawn on inspection of the source.** `KeyboardStrip` paints a three-stop gradient from `#E9EDF3` to `#B9C2CF`, tints only the in-range keys and adds a front glow along the sounding span. It is deliberate, it matches the mockup's keybed, and it was misread from a downscaled capture. No change made. |

### Redundancy

| # | Finding |
|---|---|
| S18 | "Read only" appears twice on Devices; "CURRENT SOUND" appears twice within one card (panel header plus nested overline). |
| S19 | Three consecutive guidance paragraphs in the connection card say the same thing: "Select your MIDI ports above, then click Connect." / "Both ports selected. Click Connect to start, then Test XP-60 connection to verify." / "Open both MIDI ports, then test the connection." |
| S20 | `Connection options` reads "collapsed" while the options grid below it is visible, because the grid's `visible` binding ORs in `connectionState !== Connected`. |

---

## 3. Deviations from the authoritative mockup

The mockup already answers several questions the implementation guessed at.

| Mockup | Implementation | Action |
|---|---|---|
| Header shows only a dot, two words and a chevron. No pill fill, no detail sentence, no separate "Verified" badge. | A filled pill containing a truncated instruction sentence plus a nested `Verified` pill. | Reduce the header to awareness plus one action. |
| Rail items are plain rows; **no per-item availability text exists**. | Every unavailable item carries a full-size `COMING SOON` label, which is what overflows the rail. | Remove the inline availability text; convey unavailability by dimming plus tooltip. |
| Parameter labels are **title case** and small: `Wave`, `Level`, `Pan`, `Octave`, `Attack`. Uppercase is reserved for *section overlines* (`CURRENT PATCH`, `FOUR-TONE CONTRIBUTION`), tab labels and table column headers. | `role: "overline"` (uppercase plus letter-spacing) is applied to parameter labels throughout. | Introduce a distinct `label` role; keep `overline` for sections only. |
| The section tabs form one connected segmented bar. | Five separately bordered rounded rects with 8 px gaps. | Make `XpSegmentedControl` a real segmented control. |
| Signal chain is a single clean row (Structure, MFX, Chorus, Reverb, Output) with short tone-coloured brackets feeding in from above. | Connector lines cross each other and pass through node boxes; a large empty band sits below. | Rebuild the routing geometry per the mockup. |
| `Write to XP-60` is a filled blue primary. | Already `variant: "primary"`; it renders dim only because it is genuinely disabled until the write is armed. **No deviation** — withdrawn. | None. |

---

## 4. Design-system proposal

### 4.1 Typography scale

Move the ramp to **device-independent pixels** with explicit roles and line
heights, so text and control geometry hold a fixed relationship and OS scaling
scales both together.

| Role | px | Weight | Case | Use |
|---|---|---|---|---|
| `display` | 22 | Bold | Mixed | Screen title (one per screen) |
| `title` | 17 | Bold | Mixed | Patch name, hero value |
| `heading` | 14 | DemiBold | Mixed | Panel heading |
| `subheading` | 13 | DemiBold | Mixed | Module heading inside a panel |
| `body` | 13 | Regular | Mixed | Default |
| `label` | 12 | Regular | Mixed | Parameter label (`Level`, `Pan`) |
| `value` | 13 | DemiBold | Mixed | Parameter value |
| `caption` | 12 | Regular | Mixed | Helper text, metadata |
| `overline` | 11 | DemiBold | Upper, +0.6 tracking | Section overline only |
| `mono` | 12 | Regular | - | Values, addresses |
| `data` | 12 | Regular | - | Raw SysEx bytes |

Rules: letter-spacing is only ever used on `overline`; no site sets
`font.pointSize` directly; secondary text never falls below `textSecondary`
for body-length content.

### 4.2 Spacing and grid

Strict 4 px rhythm. Existing off-grid values to be corrected: `StatusPill`
22 to 24, `ConnectionStatusIndicator` 26/30/44/46 to 24/28, `controlHeightSm`
26 to 24.

| Token | Value |
|---|---|
| `spacingXs` ... `spacingXxl` | 4, 8, 12, 16, 24, 32 |
| `controlHeightXs / Sm / Md / Lg` | 20 / 24 / 32 / 40 |
| `railWidth` | 200 |
| `headerHeight` | 56 |
| `screenPadding` | 24 (16 below 1200) |
| `cardPadding` | 16 |
| `radiusSm / Md / Lg / Pill` | 6 / 10 / 14 / 999 |

Panel columns get real `Layout.minimumWidth` values derived from their content,
and the two-column breakpoint is raised to the width at which both minima
actually fit.

### 4.3 Colour and surface hierarchy

The current palette has the right ladder but the levels are too close
(`surface #0D131E` against `surfaceRaised #111927` differ by about 4%). Widen
the steps so surface change alone can carry grouping and R3's nested borders
can be removed:

| Level | Role |
|---|---|
| `windowBackground` | Application ground |
| `railBackground` | Navigation (distinct enough to need no divider) |
| `surface` | Panel |
| `surfaceRaised` | Raised group inside a panel - replaces a nested border |
| `surfaceSunken` | Inset field / data well |
| `surfaceHover` / `surfacePressed` | Interaction |

Accent is reserved for: the current navigation item, the primary action, focus,
and the selected Tone. It is not used for decorative headings.

### 4.4 Icon system

`XpIcon` is already a single original 24-unit line family at 1.6 stroke - the
right foundation. Gaps to close: replace the text glyphs used for chevrons,
arrows, disclosure triangles, plus, minus and check with real icons, and add
`search`, `close`, `alert`, `info`, `copy`, `activity`, `midi-in`, `midi-out`.
Interactive icons get a 32 px hit target (WCAG 2.2 target guidance) even at
18 px optical size.

### 4.5 Interaction states

Every interactive component implements default / hover / focus / pressed /
selected / disabled, and never signals state by colour alone. `ghost` gains a
resting affordance. Geometry does not change between states (no border-width
changes; selection is drawn as an inset ring so layout cannot shift).

### 4.6 Synth control strategy

Already correct in kind (`XpKnob`, `XpFader`, `EnvelopeEditor`,
`LfoShapeSelector`, `KeyboardStrip`, `XpRangeBar`, `SignalFlowNode`). The work
is consistency: one shared value-readout treatment, bipolar centre marks where
the parameter is bipolar, Shift for fine adjust and modifier-click to reset
applied uniformly, and value plus unit always visible on the control rather
than in a separate column.

### 4.7 Feedback strategy

One state vocabulary, owned centrally: Offline, Searching, Available,
Connecting, Connected, Verifying, Verified, plus transient Sending / Receiving
activity and a Problem terminal state. Rendered as: a compact chip in the shell
(awareness plus action), inline control highlighting for local edits, transient
acknowledgement for completed sends, and the Devices screen as the only place
that carries full diagnostics prose.

---

## 5. Implementation order

1. **Foundation** - Typography to px plus roles; Metrics to grid; widen surface
   ladder; add `XpSwitch`, a border-free section primitive, redesigned
   `XpEmptyState`, icon additions; give `ghost` a resting affordance.
2. **Connection state** - single owner; header and rail reduced to awareness;
   remove the ten restatements.
3. **Shell geometry** - rail overflow, wordmark alignment, rail-attached
   current-item treatment, focus ring, header clearance, `ScreenHeader` with no
   title.
4. **Devices** - column proportions and minima, de-nesting, form alignment,
   designed empty states, Protocol Activity as a real MIDI monitor.
5. **Editor** - Tone card baselines and stable selection geometry, connected
   segmented control, parameter-label case, key-range contrast, primary-action
   hierarchy, routing geometry.
6. **Library / Wave Browser** - apply the same primitives.
7. **Verification** - capture 1024 / 1280 / 1440 / 1920 in Offline, Connected
   and Editor states after each wave and re-inspect edges.


---

## 6. What was implemented

Built and verified with `tests/tools/screenshot_harness.cpp` (extended in this
pass to render the current shell, which it could no longer do, and to capture
the **Offline** state via `XP60STUDIO_SHOT_OFFLINE`). All 31 tests pass.

### Foundation

| Change | Effect |
|---|---|
| `Typography` moved from points to device-independent pixels, with new `label`, `value`, `subheading` and `data` roles | R1. Text and control geometry now hold a fixed relationship and scale together |
| Letter-spacing restricted to `overline`; `overline` reserved for section markers | Parameter labels are title case, per the mockup |
| `Metrics` on a strict 4 px grid; `devicesTwoColumnMinWidth` computed from real column minima instead of guessed | Off-grid 22/26/30/46 values removed; the two-column breakpoint is now 1104, not 1200 |
| `Theme` surface ladder widened; `localEdit` / `hardwareValue` and a raw-protocol palette added | R3. Grouping can be carried by surface instead of a second border |
| `XpButton`: `ghost` gains a resting outline, new `quiet` variant, `iconOnly` with a 32 px target, border width constant across states | R4. Secondary actions are no longer bare text; buttons do not change size between states |
| New `XpSwitch`, `XpSection`, `XpFieldRow`, `XpValueReadout`, `XpByteView`; `XpEmptyState` redesigned | Replaces the stock Qt Basic switch, nested cards, mis-aligned forms and undesigned empty regions |
| `XpIcon`: chevrons, arrows, plus/minus, check, search, copy, refresh, alert, info, activity, MIDI direction added | S13. Text glyphs used as icons removed from combo boxes, spin fields and flow arrows |
| `XpSegmentedControl` rebuilt as one connected bar with per-corner radii | Mockup deviation closed |

### Connection state

`AppShellViewModel` now owns the vocabulary — `connectionPhase`,
`connectionTone`, `connectionShortLabel`, `connectionActionable`,
`connectionActionLabel`, `midiActive` — and every surface reads it. R2. The ten
restatements on the Offline screen are gone; the header carries a fixed-height
chip of at most two words plus one action; the rail carries a compact chip and
the device name; the Devices screen carries the explanation. `available` is
distinguished from `offline`, and the shell no longer offers "Open Devices" on
the Devices screen.

### Shell geometry

Rail overflow fixed by removing the per-item availability text the mockup does
not have and letting the label elide (S1, with a regression test in
`tst_Shell.qml` asserting no label leaves the rail). Wordmark aligned to the
icon column (S9). Current item is a fill plus a marker attached to the rail's
edge, with the focus ring outside the fill (S12). Header chip has one height in
every state (S2). `ScreenHeader` lays out no title line when there is no title
(S7). Rail no longer repeats the ports (S3).

### Devices

Restructured twice. The first pass gave the connection panel full width as the
"primary task"; that was wrong once verified, because connecting is then
*finished*. The second pass replaced the panel — including its
`XP60Studio -> MIDI -> Roland XP-60` diagram, which was decoration that
restated the state a third time — with **one row**: identity, state, ports, the
single action available now, and a reserved progress line that does not move
the layout when work starts. Rarely-changed settings (device number, speed,
rescan) are behind its disclosure.

The freed height goes to **Protocol Activity**, rebuilt as a real MIDI monitor:
fixed monospace columns shared between header and rows, tail-following that
stops when the user scrolls back, direction by shape and word rather than
colour, and raw SysEx on the technical-data surface via `XpByteView`. The page
column stretches to the viewport so the monitor absorbs the slack instead of
leaving a dead band (S5, S6). Nested cards removed throughout (R3); form rows
share a label column (S8); empty states are designed; `1 events` fixed (S14).

### Editor

Tone cards: `Level` / `Pan` / `Octave` were vertically centred in their row, so
unequal column heights put their labels on three baselines — all three columns
are now top-aligned (S10). Selection is an inset ring rather than a 2 px
border, so a card's interior no longer moves when it becomes current (S11).
Parameter labels are title case; "Browse waves" is a control with a chevron
rather than accent-coloured uppercase text. The `PATCH EDITOR` badge is a
`StatusPill` instead of a rectangle indistinguishable from the button beside it
(S16). Section tabs are one connected bar.

## 7. Not done

- **Signal-flow geometry.** Connector lines still cross each other and pass
  behind nodes, and a band below the chain is unused. The mockup lays the chain
  out as one row with short tone-coloured brackets feeding in from above;
  rebuilding `EffectRoutingView`'s routing is a self-contained piece of work
  that has not been started.
- **Library and Wave Browser** have the new foundation applied through the
  shared components, but have not had a screen-level pass.
- **Live audition** still explains itself in a sentence of prose beside a
  disabled button, rather than through state.
- **OS scaling at 125/150/175 %** has not been captured. The move to pixel
  sizes is what makes it predictable, but it is unverified.
