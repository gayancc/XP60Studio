import QtQuick
import QtQuick.Layouts
import XP60Studio

// Mutually exclusive choices in one connected bar — the editor's
// Sound/Filter/Amp/Motion/Effects navigation and the monitor's In/Out filter.
//
// This was five separately bordered rounded rectangles with 8 px gaps between
// them, which reads as five buttons that happen to sit in a row, not as one
// control with one selection. The master mockup shows a single bar. Segments
// now share a track: no gaps, hairline separators between neighbours, and the
// outer corners rounded while the inner ones are square, so the bar has one
// silhouette.
Item {
    id: root

    property var model: []
    property int currentIndex: 0
    property bool compact: false
    // Uppercase suits the editor's section tabs (and matches the mockup);
    // sentence case suits an inline filter.
    property bool uppercase: true
    signal activated(int index)

    readonly property int rowHeight: compact ? Metrics.controlHeightSm : Metrics.controlHeight

    implicitHeight: rowHeight
    implicitWidth: track.implicitWidth

    Accessible.role: Accessible.PageTabList

    Rectangle {
        id: track
        anchors.fill: parent
        radius: Metrics.radiusSm
        color: Theme.surfaceSunken
        border.width: Metrics.borderWidth
        border.color: Theme.border
        implicitWidth: row.implicitWidth + 2

        RowLayout {
            id: row
            anchors.fill: parent
            anchors.margins: 1
            spacing: 0

            Repeater {
                model: root.model
                delegate: Item {
                    id: segment
                    required property var modelData
                    required property int index
                    readonly property bool current: root.currentIndex === index
                    readonly property bool first: index === 0
                    readonly property bool last: index === root.model.length - 1

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: label.implicitWidth
                                         + 2 * (root.compact ? Metrics.spacingMd : Metrics.spacingLg)

                    Accessible.role: Accessible.PageTab
                    Accessible.name: segment.modelData
                    Accessible.checked: current
                    activeFocusOnTab: true

                    // Separator between neighbours only — never at the ends,
                    // where it would double up with the track's own border.
                    Rectangle {
                        visible: !segment.first && !segment.current
                                 && root.currentIndex !== segment.index - 1
                        width: 1
                        color: Theme.borderSubtle
                        anchors {
                            left: parent.left
                            top: parent.top
                            bottom: parent.bottom
                            topMargin: Metrics.spacingXs
                            bottomMargin: Metrics.spacingXs
                        }
                    }

                    // Fill. Only the bar's outer corners are rounded, so an
                    // end segment matches the track's silhouette while an
                    // inner one stays square against its neighbours. Per-corner
                    // radii do this directly; overpainting the unwanted
                    // corners would need one more rectangle per side.
                    Rectangle {
                        anchors.fill: parent
                        color: segment.current ? Theme.accent
                             : hover.hovered ? Theme.surfaceHover : "transparent"
                        topLeftRadius: segment.first ? Metrics.radiusSm - 1 : 0
                        bottomLeftRadius: segment.first ? Metrics.radiusSm - 1 : 0
                        topRightRadius: segment.last ? Metrics.radiusSm - 1 : 0
                        bottomRightRadius: segment.last ? Metrics.radiusSm - 1 : 0
                        Behavior on color {
                            enabled: !Motion.reducedMotion
                            ColorAnimation { duration: Motion.durationFast; easing.type: Motion.easingStandard }
                        }
                    }

                    Rectangle {
                        visible: segment.activeFocus
                        anchors.fill: parent
                        anchors.margins: -2
                        radius: Metrics.radiusSm + 1
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.focusRing
                    }

                    XpLabel {
                        id: label
                        anchors.centerIn: parent
                        text: segment.modelData
                        role: root.uppercase ? "overline" : "label"
                        font.weight: Typography.weightMedium
                        color: segment.current ? Theme.textOnAccent
                             : hover.hovered ? Theme.textPrimary : Theme.textSecondary
                    }

                    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root.activated(segment.index) }
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                            root.activated(segment.index)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Left && segment.index > 0) {
                            root.activated(segment.index - 1)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Right && segment.index < root.model.length - 1) {
                            root.activated(segment.index + 1)
                            event.accepted = true
                        }
                    }
                }
            }
        }
    }
}
