pragma Singleton
import QtQuick

// Spacing, radii and control dimensions.
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

    readonly property int controlHeightSm: 26
    readonly property int controlHeight: 32
    readonly property int controlHeightLg: 40

    readonly property int iconSizeSm: 14
    readonly property int iconSize: 18
    readonly property int iconSizeLg: 22

    readonly property int railWidth: 236
    readonly property int headerHeight: 52
    readonly property int screenPadding: 24
    readonly property int cardPadding: 16
    readonly property int inspectorWidth: 360

    readonly property int windowMinWidth: 1024
    readonly property int windowMinHeight: 680
    readonly property int windowPreferredWidth: 1440
    readonly property int windowPreferredHeight: 900

    // Below this width the Devices screen stacks its two columns.
    readonly property int devicesTwoColumnMinWidth: 1200
    readonly property int metricTileMinWidth: 150
}
