pragma Singleton
import QtQuick

// Spacing, radii and control dimensions.
//
// Everything here is a multiple of 4 so components stack without producing
// half-step gaps, and so a row of controls of different kinds still shares one
// baseline grid. A value that is not on the grid is a bug, not a tuning.
QtObject {
    // Two scales, deliberately.
    //
    // The shell scale (spacing*) spaces screens, dialogs and prose — places
    // where a reader's eye needs room. The instrument scale (below) spaces
    // sound-design surfaces, where the eye is aiming at controls and every
    // gap is a control that did not fit. A dense instrument works at about
    // half a business layout's spacing; using one scale for both is what made
    // the editor twice as tall as it needed to be.
    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12
    readonly property int spacingLg: 16
    readonly property int spacingXl: 24
    readonly property int spacingXxl: 32

    // Instrument scale. Between a module's own controls.
    readonly property int tight: 2
    readonly property int gapXs: 4
    readonly property int gap: 6
    readonly property int gapMd: 10
    // Panel inset for a sound-design module, against cardPadding's 16.
    readonly property int panelPadding: 8

    readonly property int radiusSm: 6
    readonly property int radiusMd: 10
    readonly property int radiusLg: 14
    readonly property int radiusPill: 999

    readonly property int borderWidth: 1

    // A module header band. Tall enough to seat a compact control inline, short
    // enough that stacking six modules does not cost a screen of height.
    readonly property int moduleHeaderHeight: 22

    // Knob diameters, chosen by what the parameter is worth on the screen it
    // is on — not by how much room happens to be free. Four steps, so a row of
    // knobs of the same importance is always the same size.
    readonly property int knobXs: 28   // dense grids: LFO depths, sends
    readonly property int knobSm: 34   // envelope stages, Tone card mix
    readonly property int knobMd: 42   // a section's own parameters
    readonly property int knobLg: 54   // the one or two that lead a module

    readonly property int controlHeightXs: 18
    readonly property int controlHeightSm: 22
    readonly property int controlHeight: 28
    readonly property int controlHeightLg: 36

    // A pointer target is never smaller than this, even when the glyph inside
    // it is `iconSize` (WCAG 2.2 target-size guidance).
    readonly property int hitTarget: 32

    readonly property int iconSizeSm: 14
    readonly property int iconSize: 18
    readonly property int iconSizeLg: 22

    readonly property int railWidth: 200
    readonly property int headerHeight: 56
    readonly property int screenPadding: 24
    readonly property int screenPaddingCompact: 16
    readonly property int cardPadding: 16
    // Rows in a list of records. A row is as tall as its content plus one
    // instrument gap, never as tall as its padding.
    readonly property int listRowHeight: 34
    readonly property int listRowHeightTall: 40
    readonly property int inspectorWidth: 340

    // Navigation row: 32 of content plus 4 above and below keeps the current
    // item's fill clear of its neighbours without a divider.
    readonly property int navRowHeight: 36
    readonly property int navRowInset: 8

    readonly property int windowMinWidth: 1024
    readonly property int windowMinHeight: 680
    readonly property int windowPreferredWidth: 1440
    readonly property int windowPreferredHeight: 900

    // Devices stacks its two columns below the width at which both columns can
    // still hold their content. The connection column needs 440 for its MIDI
    // path diagram, the diagnostics column 400 for its four health chips, plus
    // one gutter and both screen margins.
    readonly property int devicesConnectionMinWidth: 440
    readonly property int devicesDiagnosticsMinWidth: 400
    readonly property int devicesTwoColumnMinWidth:
        railWidth + devicesConnectionMinWidth + devicesDiagnosticsMinWidth
        + spacingLg + 2 * screenPadding
    readonly property int metricTileMinWidth: 148

    // Below this width the screen uses the compact margin.
    readonly property int compactWidth: 1200

    function screenMargin(width) {
        return width < compactWidth ? screenPaddingCompact : screenPadding
    }
}
