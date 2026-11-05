# XP60Studio — instrument density audit

**Date:** 2026-09-08
**Method:** headless captures of every screen at 1440×900 (and 1024×680 /
1180×1150 for the responsive checks), read against the Serum 2 reference and the
FabFilter Pro-Q 3 principles named in the brief.
**Captures:** `shots/ui-audit/`

The finding common to every screen is not decoration, it is **scale**. The
spacing tokens were written for a business dashboard: 24 px screen padding,
16 px card padding, 12–24 px group gaps, 32 px control height. A dense
instrument works at roughly half that. Every symptom below is that one cause
expressed differently.

---

## Screen findings

### Editor — reworked earlier this session, remaining items only
| # | Finding | Severity |
|---|---|---|
| E1 | Section tabs band is 40 px tall and full width for five words | med |
| E2 | Tone cards: 175 px tall, ~30 px of internal padding, mini-envelope squeezed to 30 px | med |
| E3 | Envelope graph has no grid, no node hover state, no value guide while dragging | high |
| E4 | LFO lanes leave ~20 % of each lane empty on the right | low |
| E5 | Effects canvas: routing lines cross a large empty middle | med |

### Dashboard
| # | Finding | Severity |
|---|---|---|
| D1 | ~230 px of unusable height: the screen has ~500 px of content for a 736 px area | high |
| D2 | Hero card padding 16, section gaps 12–16; the four Tone meters are the only real graphic | med |
| D3 | Summary cards are 3 × ~90 px of mostly label | med |

### Library
| # | Finding | Severity |
|---|---|---|
| L1 | Result rows are 54 px for two short lines — 12 rows visible out of 128 | high |
| L2 | Star rating sits ~450 px from the patch name it rates | high |
| L3 | Inspector: ~280 px empty below the duplicate list | high |
| L4 | Search field 40 px tall, full width | med |
| L5 | Metadata is a 4-row label/value table at 21 px per row | med |

### Bank Builder
| # | Finding | Severity |
|---|---|---|
| B1 | ~60 px gaps between SUBGROUP / BANK / NUMBER / tiles / map — four of them | high |
| B2 | Destination tiles 145 px tall to show a number, "EMPTY" and an address | high |
| B3 | Source list rows 41 px for two lines | med |
| B4 | The hardware LCD panel is the one part that reads as an instrument — the rest does not match it | med |

### Expansion
| # | Finding | Severity |
|---|---|---|
| X1 | ~300 px empty below content | high |
| X2 | Four slot cards in 2×2, each 110 px tall for one dropdown and one field | high |
| X3 | Tone 1–4 compatibility as four stacked 28 px rows instead of one row of chips | med |

### Devices
| # | Finding | Severity |
|---|---|---|
| V1 | "Try without hardware" banner is 70 px for one sentence | med |
| V2 | Current Sound / Send to XP Temp cards end at different heights (470 vs 495) | low |
| V3 | Protocol log is the correct density already — it is the model for the rest | — |

### Performance
| # | Finding | Severity |
|---|---|---|
| P1 | Part strips: ~60 px of dead space above each fader | med |
| P2 | Inspector knob row leaves the right half of the panel empty | med |

---

## Root causes, in priority order

1. **Spacing scale is 2× too generous** for a sound-design surface.
2. **Row heights are set by padding, not by content** — 54 px list rows, 145 px
   bank tiles, 110 px slot cards.
3. **Values are separated from the things they describe** — the Library's star
   column, the bank tile's address.
4. **Graphics are not prioritised.** Pro-Q 3 gives the curve the surface and
   pushes chrome to the edge; our graphs are one element among many.
5. **Screens with little content stretch their cards** instead of being compact
   and letting the page end.

---

## Plan

| Phase | Work | Status |
|---|---|---|
| 1 | Density scale in `Metrics`/`Typography`; instrument tokens distinct from shell tokens | done |
| 2 | `XpKnob` size scale by importance + arc/ticks, hover/focus/active/disabled states | done |
| 3 | Envelope: stage guides, drag crosshair | done |
| 4 | Apply per screen | Library, Bank Builder, Expansion, Devices, Performance done; Editor done in the earlier pass |
| 5 | Responsive re-check at 1024×680 and 1440×900 (1600 for Performance) | done — `shots/responsive/`, 14 captures, none clipped |
| 6 | Binding + interaction verification (full `ctest`), final review | done |

Nothing here changes a parameter, a range, a default, or a SysEx path. The XP-60
model is untouched; only its presentation is.

---

## Resolutions

| # | Resolution |
|---|---|
| E1, E2 | Fixed by the density scale: tabs band 40→28, Tone cards 175→~155 |
| E3 | Stage guides drop from each breakpoint; dashed crosshair on the dragged node |
| D1–D3 | Not fixed. The Dashboard has ~500 px of content for a 736 px area; three attempts to fill it moved the empty space inside the modules instead. Left at natural heights with the mockup's vertical Tone meters. Needs more content, not more stretching |
| L1 | Rows 54→34 px, single line — 12 → 17 visible |
| L2, L5 | Fixed columns: name, slot, source, a fixed-width badge column, then rating. Every field lines up down the list |
| L3 | Still open — the inspector runs out of content below the duplicate list |
| B1 | The four ~60 px gaps are gone: the selector rows are pinned and one greedy spacer takes the column's slack at the end |
| B2 | Destination tiles 145→~120, minimum 96 |
| X1–X3 | Four slots in one row, Tone compatibility in one row of chips; screen 610→~300 px |
| V1 | Banner is one line with the button beside it. Shrinking its inset first caused a regression — a wrapping `Text` in a `RowLayout` reports its *unwrapped* implicit height, so the card sized itself too short and the second line spilled past its own border. Removing the wrap removed the problem |
| V2 | The two mid panels fill the row, so "read the current sound" and "send one back" end on the same line |
| P1, P2 | Inspector is one horizontal band — sends and output at the left, keyboard filling the rest. ~300→~180 px |

### Left as they are, with reasons

| # | Why |
|---|---|
| L3 | The Library inspector is content-limited, like the Dashboard. Its card fills the row the list sets, and the slack sits below the content as padding. Forcing it to fill would repeat the Dashboard mistake — moving empty space inside a module rather than removing it |
| E4 | Each LFO lane's knobs are left-packed at a fixed pitch, so the spare width is one region at the lane's right edge. That is how a dense instrument packs a module; spreading them to the edge is the fault this whole pass was fixing |
| E5 | The effects canvas is a routing diagram. The space between nodes is the routing, not padding |

## Defect found while doing this

A `Row` was given `verticalItemAlignment`, which only `Grid` and `Flow` have.
`ExpansionScreen` then failed to **compile**, `Main.qml` could not resolve the
type, and the application would not start — while all 71 tests and 161 QML
assertions passed, because no test instantiates that screen and none loads
`Main.qml`.

`tests/qml/tst_ScreensCompile.qml` closes it: `Qt.createComponent` compiles all
nine screens and `Main.qml` without instantiating them, so it needs none of
their view models and catches unknown properties, bad imports, renamed controls
and syntax errors anywhere in the tree. Validated by reintroducing the defect
and confirming the failure, then removing it.

Related gap, not addressed: `Expansion` and `Performance` have no view models in
the QML test harness, so their behavioural coverage is thin. The compile test
covers "does it load" only.
