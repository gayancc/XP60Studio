# FL Studio pop-up menu: measured evidence

Captured 2026-11-05 from the owner's running FL Studio 2025 (FL64.exe, default theme as installed on this machine) by screenshotting its **VIEW** menu and sampling
pixels (scripts: scratchpad `fl.ps1`, `measure.ps1`; screenshots `fl-view.png`, `fl-hover.png`). Nothing here is copied from FL (no assets); only numbers.

| Item | Measured |
|---|---|
| Menu width (incl. border) | 244 px (x 172-415) |
| Border | 2 px, `#151E23` |
| Body | `#BBC3C8` (light), text `#01010D` |
| Group heading bar ("Windows", "Layout", "Browser", "News and notifications") | 16 px high, `#929DA4`, text `#D7E1DA`, text 4 px in from the border |
| Entry rows | 19 px high, contiguous (no gap, no separator lines; headings divide groups) |
| Check mark column | check about x 180-188; entry text starts about 18 px in from the inner left edge |
| Active-entry mark | a check mark; some check marks are drawn grey instead of black (Piano roll, Tempo tapper, Touch controller, Script output, Plugin performance monitor, Tests): the reason is NOT established, so the app draws every check in the entry's text colour |
| Shortcut text | right-aligned, `#525A62` |
| Sub-menu | entries with a sub-menu show a `>` at the right edge (Toolbars, Arrange windows, Background) |
| Hover | full-width fill `#5E6C75`, text `#E0EFEE`, shortcut `#ADBFCE`; no accent colour, no outline |
| Closing | the menu closes when FL loses focus |

## Combo-box list (the snap "Line" combo in the toolbar), measured the same day

| Item | Measured |
|---|---|
| Body | dark, a subtle vertical gradient `#21262B` (top) to `#1D2327` (bottom) |
| Border | 2 px, `#0D1216` |
| Entry rows | 19 px high, text `#A9B7B5`, text starts 4 px in from the inner left edge; no check column |
| Hover | row filled `#7F8B92`, text `#1E2428` |
| Selected entry | NOT marked (the current "Line" has no fill and no mark in the open list) |
| Groups | separated by a gap of about 9 px (between "Cell" and "(none)"), no heading bar |
| Width | the list is as wide as the combo that opened it; it opens directly below it |

Closed face of that same combo, remeasured 2026-11-08 on the toolbar: 23 px tall (y 56–78). Quiet fill runs from about `(78, 83, 87)` at the top to `(39, 44, 48)` at the bottom. Label core about `(187, 195, 189)`. No separate border. Hover was not captured.

The app uses this look for plain lists and the light menu look above for grouped lists (headings), each where FL uses it.

**Not yet measured:** the sub-menu cascade geometry, the disabled-entry colour, the
right-click menu of a knob, keyboard behaviour. The app applies the measured values to every pop-up (drop-downs, context menu, top menus) as one widget family; the ones above are unverified for those
other menus until measured the same way.
