# FL Studio QuickBtn / toolbar toggle — measured evidence

Source captures (2026-11-07):

- Owner close-up of three **ON** transport toggles (typing keyboard / count-in / loop):
  `assets/…/image-be7922b7-…png` (104×45)
- Live FL main toolbar: `artifact/docs/xppatchstudio/fl-live/fl-main-now.png`
  (toolbar crop saved as `fl-live/toolbar-crop.png`)

Design language only — nothing copied from Image-Line assets.

## Geometry (FL reference only — do not bind our pads to these sizes)

Owner instruction: keep XPPatchStudio pad widths/heights as they are; borrow
activate/deactivate face language and snap behaviour from FL, not the pixel box.

| | Measured in FL |
|---|---|
| Face | ~30×29–33 px square (owner ON crop 30×33 incl. rim) |
| Corner | ~2 px radius (soft, not pill) |
| Gap between neighbours | 2–3 px |
| Host bar | caption `#596267` |

## OFF (deactivate) face

Measured on inactive toolbar QuickBtns in `toolbar-crop.png` (e.g. x 192–217):

| Region | RGB | Hex |
|---|---|---|
| Face average | 74, 82, 89 | `#4a5259` |
| Top edge | 89, 98, 103 | `#596267` |
| Bottom edge | 63, 72, 77 | `#3f484d` |
| Side mid | 69, 78, 83 | `#455055` |
| Icon / label ink | 217, 225, 229 | `#d9e1e5` |

Reading: a **quiet vertical charcoal gradient** (lighter top → darker bottom), flat, no orange,
no outer glow. Icons look etched in light silver.

Hover (from live FL behaviour + prior channel-rack notes): face brightens one notch only
(~5–10% lift). No scale, no outline flash, no colour shift toward orange until the click
lands ON.

Mouse-down on OFF: brief darker inset (pressed-in), then release settles to ON or back to OFF.

## ON (activate) face

Measured on owner close-up (first button bbox 2,4–31,36):

| Region | RGB | Hex |
|---|---|---|
| Rim (full perimeter) | 190, 85, 7 | `#be5507` |
| Hot centre (lamp) | 255, 255, 180 | `#ffffb4` |
| Mid ring | ~255, 218, 141 | `#ffda8d` |
| Outer face | ~251, 166, 90 | `#fba65a` |
| Icon / label ink | 177, 64, 0 | `#b14000` |
| Face average (ex-icon) | 248, 185, 108 | `#f8b96c` |

Reading: the button is a **backlit lamp**, not a flat orange fill and not a simple
top→bottom linear gradient.

- Outer 1 px rim is solid `#be5507` (same token as `--accent-soft`).
- Fill is a soft **radial / elliptical** wash: pale yellow-white at the centre, warm orange
  toward the edges.
- Glyphs are dark brown stencil ink (`#b14000`), never white and never pure black.
- No separate outer bloom beyond the face; the “glow” is the face itself.

## Activating / deactivating motion (MEASURED live, not inferred)

FL Studio 2025 was running. A toolbar QuickBtn (ON lamp group, screen ~3235,35) was
toggled while grabbing that 31×34 px face with `mss` at **~240 fps**. Frames and notes:
`fl-live/anim-frames-hi/`.

### Deactivate (ON → OFF)

| | |
|---|---|
| Sample rate | ~240 fps |
| Visible blend | orange-fraction fell through intermediates `0.64 → 0.56 → 0.48 → 0.39` |
| Blend window | **~45–55 ms** from last full-ON sample to first clearly dimmed sample |
| End state | charcoal OFF face; no outer glow leftovers |

### Activate (OFF → ON)

| | |
|---|---|
| Sample rate | ~240 fps |
| Delay after click | ~45–50 ms before any orange appears (app/input), then ramp |
| Visible blend | partial orange `0.08 → 0.11 → … → 0.32+` |
| Click → settled-ish | **~110 ms** (not a hard cut) |

### What that means

1. **Short lamp fade, not a zero-duration snap.** Earlier “instant / `transition: none`”
   notes were wrong — they were guessed from stills. Live capture shows a brief blend.
2. **No bounce, scale, or outer bloom** in either direction. Only face colour / luminance
   changes inside the same box.
3. **No hover-to-ON.** Hover never turns the face into the lamp; only the latched state does.
4. **CSS implication for our pads:** ease the face over **~50–80 ms**
   (`background` / `border-color` / `color`). Never `transform`, never outer `box-shadow`
   pulse, never multi-hundred-ms theatrical fades.

Static PNGs alone cannot measure this. Face colours still come from stills; latch timing
comes only from the live capture above.

## Recipe for our pads (SUB a/b, BANK, NUMBER)

Face height is the measured 31 px. Width stays the panel's.

```css
/* OFF — convex bevel */
background: linear-gradient(180deg, #363f44 0%, #455055 22%, #545d62 50%, #455055 78%, #363f44 100%);
border: 1px solid #2a3338;
color: #d9e1e5;

/* ON — radial lamp */
color: #b14000;
border: 1px solid #be5507;
background: radial-gradient(
  ellipse 92% 88% at 50% 48%,
  #ffffb4 0%,
  #ffda8d 38%,
  #ffb76a 68%,
  #fba65a 100%
);
transition: background 60ms linear, border-color 60ms linear, color 60ms linear;
```

## What not to invent

- Linear-only orange wash with light top bevel pretending to be the lamp
- White / near-black label on the ON face
- Outer orange glow / drop-shadow halo
- Bounce, scale, or long (>~100 ms) theatrical latch fades
- Claiming latch timing from still screenshots alone
- Hover that previews the full ON lamp
- A hover brighten or a dark pressed-inset on toolbar toggles (remeasured: hover is unchanged, mouse-down is the other face)

## Opening size (measured 2026-11-08, FL Studio 2025, 96 DPI)

The running window was not maximised (`showCmd` normal). Placement and the live
rect were the same:

| | |
|---|---|
| Window | **2555 × 1383** |
| Monitor | 5120 × 1440, work area height 1392 |
| Position | x 2565, y 9 (right half of that display) |

That rect is FL's saved window on a 5120-wide display. It is too large for
XPPatchStudio, which opens at 1280×800. Screen position is not copied.

## OFF face, corrected from a live vertical sample

The earlier top-to-bottom linear gradient was the host bar leaking into the
sample. A quiet face pixel (icon pixels excluded) is a **convex bevel**:

| y in the 31 px face | RGB |
|---|---|
| inner rim | 54, 63, 68 `#363f44` |
| mid | 84, 93, 98 `#545d62` |

Gutter between neighbours is `#2a3338`. Icon ink on OFF stays `#d9e1e5`.

## Hover and mouse-down, remeasured the same day

A toolbar toggle was compared pixel for pixel with the pointer moved onto it
and with the button held down.

- **Hover changes 0 face pixels.** No brighten, no lamp.
- **Mouse-down on OFF paints the ON lamp immediately.** Releasing outside
  restores OFF.
- **Mouse-down on ON paints the OFF bevel immediately.**

The earlier “5–10% hover lift” and “darker inset then snap” notes were not
reproduced on these toolbar toggles. Do not put them back without a new capture.

## Panel captions (same still, Browser bar)

Quiet pixels on the Browser caption, not the toolbar host:

| | |
|---|---|
| Fill | flat `(67, 76, 81)` `#434c51`, about y 90–111 (22 px) |
| Line under the bar | `(31, 40, 45)` `#1f282d` |
| Toolbar host just above | `(89, 98, 103)` `#596267` — that stays `--caption`, not the panel title |

No orange underline on that caption. Title ink was antialiased and not one colour. Channel-rack caption away from icons was `(63, 72, 77)`, the same flat family.

## Channel-rack name buttons (same still)

One name button, icon pixels excluded, is a top-light linear face, not the toolbar bevel:

| | |
|---|---|
| Top | about `(95, 104, 109)` `#5f686d` |
| Bottom | about `(77, 86, 91)` `#4d565b` |
| Height | about 28 px |

The selected name-button skin was not isolated. Do not restyle the selected bank cell from this sample.

## Plugin group frames (Fruity Limiter, evidence shot 04-fx-limiter.png)

The LOUDNESS / ENVELOPE / NOISE GATE frames are not filled slanted tabs.

| | |
|---|---|
| Rule | 1 px, `(142, 148, 150)` `#8e9496`, continuous above the title |
| Title | small-caps, inside the frame under that rule, light ink (ClearType, about `#cdd9d8` on the stems) |
| Dividers | same light rule between groups |

No orange left edge and no caption-coloured fill on the title.

## Picker category tabs (shot 05-effect-picker.png)

The All / Coloration / Controller row is text on the parent ground `(35, 34, 41)`, not a raised tab face.

| | |
|---|---|
| Selected ("All") | solid white `(255, 255, 255)`, heavier than the others |
| Idle label core | about `(118, 116, 122)` `#76747a` |
| Accent bar | none in that band |

Hover was not captured. Do not add a hover face.

## Main-window caption buttons (fl-main-rest.png)

Three squares on the menu-bar host, left to right: minimize, maximize, close.

| | |
|---|---|
| Size | 19 × 19, including the rim |
| Gap | 1 px of the host `#596267` |
| Face | the same convex bevel as a toolbar button, rim `#2a3338` |
| Glyph | `#d9e1e5` |

Hover was not captured. Do not turn Close red on hover.

## Browser list rows

Idle labels on the browser ground are `(137, 144, 147)` `#899093`. The row behind them stays the ground; there is no accent wash.

A selected file row in shot 23 (the filter list) is a flat fill. A quiet pixel is `(47, 48, 55)` `#2f3037`, about 21 px tall. The label core on that fill is about `(182, 183, 181)` `#b6b7b5`.
