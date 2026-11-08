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

Keep whatever width/height the panel already uses (8-col bank grid, etc.).
Only the face colours / lamp / snap latch come from FL:

```css
/* OFF — face only; size stays ours */
background: linear-gradient(180deg, #596267 0%, #4a5259 55%, #3f484d 100%);
border: 1px solid #2a3236;
border-top-color: #6a757b;
color: #d9e1e5;

/* ON — radial lamp; size stays ours */
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
