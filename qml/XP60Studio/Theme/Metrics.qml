pragma Singleton
import QtQuick

// Spacing, radii and control dimensions.
//
// Everything here is a multiple of 4 so components stack without producing
// half-step gaps, and so a row of controls of different kinds still shares one
// baseline grid. A value that is not on the grid is a bug, not a tuning.
QtObject {
    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12
    readonly property int spacingLg: 16
    readonly property int spacingXl: 24
    readonly property int spacingXxl: 32

    readonly property int radiusSm: 6
    readonly property int radiusMd: 10
    readonly property int radiusLg: 14
    readonly property int radiusPill: 999

    readonly property int borderWidth: 1

    readonly property int controlHeightXs: 20
    readonly property int controlHeightSm: 24
    readonly property int controlHeight: 32
    readonly property int controlHeightLg: 40

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
