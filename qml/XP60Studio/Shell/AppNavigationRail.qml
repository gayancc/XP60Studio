import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Persistent left navigation: wordmark, destinations, device summary.
//
// Three things were wrong with the previous geometry and all three are visible
// in a screenshot:
//
//  * every unavailable item carried a full-size "COMING SOON" label, and the
//    row was pinned to `Layout.minimumWidth: implicitWidth`, so the row could
//    not shrink and "Performance COMING SOON" was drawn *past* the rail's own
//    divider and over the content area. The master mockup has no per-item
//    availability text at all: unavailability is carried by dimming, a small
//    dot, and a tooltip;
//  * the wordmark sat 4 px to the left of the icon column, which is the one
//    left edge a user reads down;
//  * the current item was a rounded rectangle floating inside the rail with a
//    1 px border, and the focus ring was drawn 1 px inside that border, so the
//    current item showed a double line. The current item now takes a fill plus
//    a marker attached to the rail's own edge, so it reads as part of the rail.
Rectangle {
    id: root

    required property AppShellViewModel shell

    // The single content inset. The wordmark, every nav row's icon and the
    // device block all start here, so the rail has one left edge.
    readonly property int inset: Metrics.spacingMd

    implicitWidth: Metrics.railWidth
    color: Theme.railBackground

    // The rail is a full surface step below the workspace, so it needs no
    // divider to separate itself. A hairline is kept only because the header
    // sits at the same colour and the corner would otherwise be ambiguous.
    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.borderSubtle }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // Wordmark, on the same baseline grid and the same left edge as the
        // navigation icons below it.
        Item {
            Layout.fillWidth: true
            implicitHeight: Metrics.headerHeight
            Row {
                anchors {
                    left: parent.left
                    leftMargin: root.inset + Metrics.navRowInset
                    verticalCenter: parent.verticalCenter
                }
                spacing: 0
                XpLabel { text: "XP60"; role: "title"; color: Theme.accentText }
                XpLabel { text: "Studio"; role: "title"; font.weight: Typography.weightRegular }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: root.inset
            Layout.rightMargin: root.inset
            Layout.topMargin: Metrics.spacingSm
            spacing: 2

            Repeater {
                model: root.shell.navigationItems
                delegate: Item {
                    id: item
                    required property var modelData
                    readonly property bool current: root.shell.currentScreen === modelData.key
                    readonly property bool available: modelData.enabled === true

                    Layout.fillWidth: true
                    implicitHeight: Metrics.navRowHeight

                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.label
                                     + (available ? "" : ", " + modelData.availability)
                    Accessible.description: available ? "" : modelData.availability
                    activeFocusOnTab: available
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                            root.shell.navigate(modelData.key)
                            event.accepted = true
                        }
                    }

                    // Fill. Geometry is identical in every state — only the
                    // colour changes — so selecting an item cannot move its
                    // label by a pixel.
                    Rectangle {
                        anchors.fill: parent
                        radius: Metrics.radiusSm
                        color: item.current ? Theme.accentSoft
                             : (hover.hovered && item.available ? Theme.surfaceHover : "transparent")
                        Behavior on color {
                            enabled: !Motion.reducedMotion
                            ColorAnimation { duration: Motion.durationFast; easing.type: Motion.easingStandard }
                        }
                    }

                    // Current-item marker, attached to the rail's left edge
                    // rather than floating: this is what makes the selection
                    // belong to the navigation instead of looking pasted on.
                    Rectangle {
                        visible: item.current
                        width: 3
                        radius: 1.5
                        color: Theme.accent
                        anchors {
                            left: parent.left
                            leftMargin: -root.inset
                            verticalCenter: parent.verticalCenter
                        }
                        height: parent.height - 2 * Metrics.spacingXs
                    }

                    // Focus ring sits outside the fill, so it can never form a
                    // second line against it.
                    Rectangle {
                        visible: item.activeFocus
                        anchors.fill: parent
                        anchors.margins: -2
                        radius: Metrics.radiusSm + 2
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.focusRing
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Metrics.navRowInset
                        anchors.rightMargin: Metrics.navRowInset
                        spacing: Metrics.spacingMd

                        XpIcon {
                            name: item.modelData.key
                            color: item.current ? Theme.accentText
                                 : (item.available ? Theme.textSecondary : Theme.textDisabled)
                            Layout.preferredWidth: Metrics.iconSize
                            Layout.alignment: Qt.AlignVCenter
                        }

                        // Allowed to elide. The rail is a fixed-width column
                        // and no label may leave it.
                        XpLabel {
                            objectName: "navLabel"
                            text: item.modelData.label
                            color: item.current ? Theme.textPrimary
                                 : (item.available ? Theme.textSecondary : Theme.textDisabled)
                            font.weight: item.current ? Typography.weightMedium : Typography.weightRegular
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Layout.alignment: Qt.AlignVCenter
                        }

                        // Unavailability, at the smallest weight that still
                        // reads: a dot. The wording is in the tooltip, so it
                        // costs no width and cannot compete with the label.
                        Rectangle {
                            visible: !item.available
                            width: 4
                            height: 4
                            radius: 2
                            color: Theme.textDisabled
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }

                    QQC.ToolTip.visible: hover.hovered && !item.available
                    QQC.ToolTip.delay: 400
                    QQC.ToolTip.text: item.modelData.availability

                    HoverHandler {
                        id: hover
                        cursorShape: item.available ? Qt.PointingHandCursor : Qt.ArrowCursor
                    }
                    TapHandler { enabled: item.available; onTapped: root.shell.navigate(item.modelData.key) }
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Device summary. No card border: a surface step and the rail's own
        // edge already separate it, and the previous bordered card put a
        // rounded rectangle inside a panel that needed no second boundary.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: deviceColumn.implicitHeight + 2 * Metrics.spacingMd
            color: Theme.surface

            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.borderSubtle }

            ColumnLayout {
                id: deviceColumn
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    leftMargin: root.inset + Metrics.navRowInset
                    rightMargin: root.inset
                    topMargin: Metrics.spacingMd
                }
                spacing: Metrics.spacingXs

                ConnectionStatusIndicator {
                    shell: root.shell
                    compact: true
                }

                XpLabel {
                    text: root.shell.deviceName
                    role: "body"
                    font.weight: Typography.weightMedium
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    Layout.topMargin: Metrics.spacingXs
                }

            }
        }
    }
}
