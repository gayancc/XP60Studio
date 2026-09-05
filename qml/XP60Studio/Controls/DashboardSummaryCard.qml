import QtQuick
import QtQuick.Layouts
import XP60Studio

// One summary card on the Dashboard's right column.
//
// A card whose subject does not exist yet still appears, because the
// composition is part of what the Dashboard says — but it states the absence
// plainly instead of showing a plausible number. An empty Bank Builder card
// that reads "arrives in a later phase" is honest; one showing "7 Banks" when
// there are none would not be.
Rectangle {
    id: root

    property string title: ""
    property string iconName: ""
    property color accentColor: Theme.accent
    // A headline sentence, for cards whose subject is a state rather than a
    // set of counts.
    property string headline: ""
    property string detail: ""
    // [{ value, label }] — `value` may be a number or a string such as an
    // em dash where the figure is not established.
    property var metrics: []
    property string actionText: ""
    property bool available: true
    property string unavailableText: ""

    signal activated()

    readonly property bool interactive: root.available && root.actionText.length > 0

    implicitHeight: column.implicitHeight + 2 * Metrics.cardPadding
    radius: Metrics.radiusMd
    color: hover.containsMouse && root.interactive ? Theme.surfaceHover : Theme.surfaceRaised
    border.width: 1
    border.color: hover.containsMouse && root.interactive ? root.accentColor
                 : root.available ? Theme.borderSubtle : Theme.border
    opacity: root.available ? 1.0 : 0.72

    Behavior on color {
        enabled: !Motion.reducedMotion
        ColorAnimation { duration: Motion.durationFast }
    }

    Accessible.role: root.interactive ? Accessible.Button : Accessible.StaticText
    Accessible.name: root.title
    Accessible.description: root.available ? root.detail : root.unavailableText
    activeFocusOnTab: root.interactive
    Keys.onReturnPressed: if (root.interactive) root.activated()
    Keys.onSpacePressed: if (root.interactive) root.activated()

    ColumnLayout {
        id: column
        anchors {
            left: parent.left; right: parent.right; top: parent.top
            margins: Metrics.cardPadding
        }
        spacing: Metrics.spacingSm

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingSm
            Rectangle {
                width: 26; height: 26; radius: Metrics.radiusSm
                color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.16)
                XpIcon {
                    anchors.centerIn: parent
                    name: root.iconName
                    width: 16; height: 16
                    color: root.accentColor
                }
            }
            XpLabel {
                text: root.title
                role: "overline"
                color: root.accentColor
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        // A state, where the card has one.
        XpLabel {
            visible: root.available && root.headline.length > 0
            text: root.headline
            role: "body"
            font.weight: Typography.weightMedium
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
        XpLabel {
            visible: root.available && root.detail.length > 0
            text: root.detail
            role: "caption"
            secondary: true
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        // Counts, where it has those instead.
        RowLayout {
            Layout.fillWidth: true
            visible: root.available && root.metrics.length > 0
            spacing: Metrics.spacingLg
            Repeater {
                model: root.available ? root.metrics : []
                delegate: ColumnLayout {
                    required property var modelData
                    spacing: 0
                    XpLabel {
                        objectName: "metricValue"
                        text: modelData.value
                        role: "title"
                    }
                    XpLabel {
                        text: modelData.label
                        role: "caption"
                        secondary: true
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }

        // Or the reason there is nothing to show.
        XpLabel {
            objectName: "unavailableNote"
            visible: !root.available
            text: root.unavailableText
            role: "caption"
            muted: true
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.interactive
            spacing: Metrics.spacingXs
            XpLabel {
                text: root.actionText
                role: "caption"
                color: hover.containsMouse ? root.accentColor : Theme.textSecondary
            }
            XpIcon {
                name: "chevron-right"
                width: 12; height: 12
                color: hover.containsMouse ? root.accentColor : Theme.textSecondary
            }
            Item { Layout.fillWidth: true }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: Metrics.radiusMd + 3
        color: "transparent"
        border.width: 2
        border.color: Theme.focusRing
        visible: root.activeFocus
    }

    MouseArea {
        id: hover
        anchors.fill: parent
        enabled: root.interactive
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.forceActiveFocus()
            root.activated()
        }
    }
}
