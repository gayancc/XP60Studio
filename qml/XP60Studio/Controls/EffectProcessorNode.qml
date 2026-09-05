import QtQuick
import QtQuick.Layouts
import XP60Studio

// One stage on the effects canvas.
//
// Not a generic box. A node carries its role (source, processor, destination),
// its identity, a drawing of what it does, and the state a musician needs at a
// glance: whether it is in the path, whether it has been edited, and how much
// is arriving. Role changes the silhouette, so source and destination never
// read as two more effects.
Item {
    id: root

    property string role: "processor"   // "source" | "processor" | "destination"
    property string title: ""
    property string detail: ""
    property string kind: "generic"
    property color accentColor: Theme.accent
    property real vizAmount: 0.5
    property real vizSecondary: 0.5

    // In the configured signal path at all. Out-of-path nodes stay visible --
    // the topology should not appear to change shape -- but recede.
    property bool active: true
    // The hero of the current view.
    property bool focused: false
    // Dimmed because attention is elsewhere, not because it is inactive.
    property bool dimmed: false
    property bool modified: false
    property bool interactive: true
    // Shown on the destination node, and wherever a value arrives.
    property string badgeText: ""

    signal activated()

    implicitWidth: 150
    implicitHeight: 78

    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.description: detail + (root.modified ? qsTr(" · edited") : "")
                            + (root.active ? "" : qsTr(" · not in the signal path"))
    Accessible.focusable: interactive
    activeFocusOnTab: interactive
    Keys.onReturnPressed: if (root.interactive) root.activated()
    Keys.onSpacePressed: if (root.interactive) root.activated()

    // Focus lifts the node rather than moving it, so the canvas keeps its
    // spatial memory through the transition.
    scale: root.focused ? 1.06 : (hover.containsMouse ? 1.02 : 1.0)
    opacity: root.dimmed ? 0.28 : 1.0
    Behavior on scale {
        enabled: !Motion.reducedMotion
        NumberAnimation { duration: Motion.durationNormal; easing.type: Motion.easingStandard }
    }
    Behavior on opacity {
        enabled: !Motion.reducedMotion
        NumberAnimation { duration: Motion.durationNormal; easing.type: Motion.easingStandard }
    }

    Rectangle {
        id: body
        anchors.fill: parent
        color: root.focused ? Theme.surfaceRaised : Theme.surface
        // The destination is a terminal, so it loses the left shoulder; the
        // source loses the right. Only processors are symmetrical.
        radius: Metrics.radiusMd
        border.width: root.focused ? 2 : 1
        border.color: root.focused ? root.accentColor
                     : hover.containsMouse ? root.accentColor
                     : root.active ? Theme.borderStrong : Theme.borderSubtle

        Behavior on border.color {
            enabled: !Motion.reducedMotion
            ColorAnimation { duration: Motion.durationFast }
        }

        // Role marker: a filled spine down the leading edge for a source, a
        // trailing edge for a destination, and a slim accent bar for a
        // processor. Shape, not colour alone, carries the role.
        Rectangle {
            visible: root.role !== "destination"
            width: root.role === "source" ? 4 : 3
            height: root.role === "source" ? parent.height - 2 * Metrics.spacingSm : parent.height * 0.38
            x: 0
            anchors.verticalCenter: parent.verticalCenter
            topLeftRadius: Metrics.radiusMd
            bottomLeftRadius: Metrics.radiusMd
            color: root.accentColor
            opacity: root.active ? 1.0 : 0.35
        }
        Rectangle {
            visible: root.role === "destination"
            width: 4
            height: parent.height - 2 * Metrics.spacingSm
            x: parent.width - width
            anchors.verticalCenter: parent.verticalCenter
            topRightRadius: Metrics.radiusMd
            bottomRightRadius: Metrics.radiusMd
            color: root.accentColor
            opacity: root.active ? 1.0 : 0.35
        }

        ColumnLayout {
            anchors {
                fill: parent
                leftMargin: Metrics.spacingSm + 4
                rightMargin: Metrics.spacingSm + (root.role === "destination" ? 4 : 0)
                topMargin: Metrics.spacingXs
                bottomMargin: Metrics.spacingXs
            }
            spacing: 1

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingXs
                XpLabel {
                    text: root.title
                    role: "overline"
                    color: root.active ? root.accentColor : Theme.textMuted
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                // A dot, not a word: "edited" needs to be noticeable without
                // competing with the algorithm name.
                Rectangle {
                    visible: root.modified
                    width: 6; height: 6; radius: 3
                    color: Theme.localEdit
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            XpLabel {
                text: root.detail
                role: "body"
                font.weight: root.focused ? Typography.weightMedium : Typography.weightRegular
                color: root.active ? Theme.textPrimary : Theme.textMuted
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            EffectMicroViz {
                objectName: "microViz"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 18
                kind: root.kind
                tint: root.accentColor
                amount: root.vizAmount
                secondary: root.vizSecondary
                muted: !root.active
            }

            XpLabel {
                visible: root.badgeText.length > 0
                text: root.badgeText
                role: "caption"
                secondary: true
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }
    }

    // Bypassed / out-of-path stages say so in words as well as in weight,
    // because a faded box alone is ambiguous.
    Rectangle {
        visible: !root.active
        anchors { right: body.right; top: body.top; margins: Metrics.spacingXs }
        width: bypassLabel.implicitWidth + Metrics.spacingSm
        height: bypassLabel.implicitHeight + 2
        radius: Metrics.radiusPill
        color: Theme.surfaceSunken
        XpLabel {
            id: bypassLabel
            anchors.centerIn: parent
            text: qsTr("NO PATH")
            role: "caption"
            muted: true
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
