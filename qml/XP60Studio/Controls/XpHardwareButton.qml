import QtQuick
import XP60Studio

// A physical button on an instrument panel.
//
// This is not XpButton with a different fill. A software button is a rectangle
// that changes colour; a panel button is a piece of moulded plastic sitting in
// a recess, with a light behind it that tells you whether it is the one
// currently selected. The difference the player feels is:
//
//   * depth  — a highlight along the top edge and a shadow under the bottom
//              edge, both of which invert when the button is held down, and a
//              one-pixel downward shift of the cap while pressed;
//   * light  — a selected button is *lit*, not merely tinted: the cap carries
//              the accent, and a status LED above the label carries occupancy
//              so a glance answers "is there anything in this bank?";
//   * speed  — every state transition is one short animation (Motion's fast
//              step). A panel button that fades over a quarter of a second
//              feels broken, so nothing here is allowed to be slow.
//
// It is also a drop destination. During a drag the surface tells the button
// whether it is a valid target and whether the pointer is on it; the button
// shows that as an armed ring rather than as another colour of fill, so a
// lit-because-selected button and a lit-because-you-are-about-to-drop-here
// button never look the same.
Item {
    id: root

    // The face of the button. Kept short: these are 1-8 and A/B.
    property string text: ""
    // Small caption under the cap, e.g. the linear range a bank covers.
    property string caption: ""
    property bool selected: false
    property bool enabledButton: true
    // Lights the LED. For BANK/NUMBER this is "something lives here".
    property bool indicator: false
    // 0..1 — how full the thing behind this button is. Drives the LED's
    // brightness so a half-filled bank does not look like a full one.
    property real fill: 0
    // Drag state, driven by the surface.
    property bool dropCandidate: false
    property bool dropActive: false
    // Larger caps for SUBGROUP, which is a bigger decision than a NUMBER.
    property bool large: false

    signal clicked()
    signal pressAndHold()

    implicitWidth: large ? 64 : 46
    implicitHeight: large ? 52 : 46

    activeFocusOnTab: enabledButton
    Accessible.role: Accessible.Button
    Accessible.name: root.text
    Accessible.description: root.caption
    Accessible.focusable: true
    Accessible.onPressAction: if (root.enabledButton) root.clicked()

    readonly property bool held: press.pressed && press.containsMouse
    readonly property color litColor: root.dropActive ? Theme.success : Theme.accent

    // The recess the cap sits in. It does not move, which is what makes the
    // cap look like it moves.
    Rectangle {
        anchors.fill: parent
        radius: Metrics.radiusSm + 2
        color: Theme.surfaceSunken
        border.width: 1
        border.color: Theme.borderSubtle
    }

    // The armed ring: only ever drawn during a drag.
    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: Metrics.radiusSm + 4
        color: "transparent"
        visible: root.dropCandidate
        border.width: root.dropActive ? 2 : 1
        border.color: root.dropActive ? Theme.success : Theme.accent
        opacity: root.dropActive ? 1.0 : 0.5
        Behavior on opacity {
            enabled: !Motion.reducedMotion
            NumberAnimation { duration: Motion.durationFast }
        }
    }

    // The cap.
    Item {
        id: cap
        anchors.fill: parent
        anchors.margins: 2
        y: root.held ? 3 : 2

        Behavior on y {
            enabled: !Motion.reducedMotion
            NumberAnimation { duration: Motion.durationFast; easing.type: Motion.easingStandard }
        }

        Rectangle {
            id: capFace
            anchors.fill: parent
            radius: Metrics.radiusSm
            color: {
                if (!root.enabledButton) return Theme.surfaceSunken
                if (root.dropActive) return Theme.successSoft
                if (root.selected) return root.held ? Theme.accentPressed : Theme.accent
                if (root.held) return Theme.surfacePressed
                return press.containsMouse ? Theme.surfaceHover : Theme.surfaceRaised
            }
            border.width: 1
            border.color: {
                if (root.activeFocus) return Theme.focusRing
                if (root.dropActive) return Theme.success
                if (root.selected) return Theme.accentHover
                return press.containsMouse ? Theme.borderStrong : Theme.border
            }

            Behavior on color {
                enabled: !Motion.reducedMotion
                ColorAnimation { duration: Motion.durationFast }
            }
            Behavior on border.color {
                enabled: !Motion.reducedMotion
                ColorAnimation { duration: Motion.durationFast }
            }

            // Moulding: a light top edge and a dark bottom edge. Held inverts
            // them, which is the whole trick — the cap reads as pushed in
            // before the colour has finished changing.
            Rectangle {
                anchors { left: parent.left; right: parent.right; top: parent.top }
                anchors.margins: 2
                height: 1
                radius: 1
                color: Qt.rgba(1, 1, 1, root.held ? 0.03 : (root.selected ? 0.22 : 0.08))
            }
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                anchors.margins: 2
                height: 1
                radius: 1
                color: Qt.rgba(0, 0, 0, root.held ? 0.05 : 0.35)
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: 2

            // Status LED. Off is a dark well rather than nothing, so the row
            // of lights reads as a row of lights even when none is on.
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 16
                height: 3
                radius: 2
                visible: root.indicator || root.fill > 0 || root.selected
                color: root.indicator || root.fill > 0
                       ? (root.selected ? Theme.textOnAccent : Theme.live)
                       : Qt.rgba(0, 0, 0, 0.4)
                opacity: root.fill > 0 ? 0.45 + 0.55 * Math.min(1, root.fill) : 1
                Behavior on opacity {
                    enabled: !Motion.reducedMotion
                    NumberAnimation { duration: Motion.durationFast }
                }
            }

            XpLabel {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.text
                role: root.large ? "title" : "heading"
                color: !root.enabledButton
                       ? Theme.textDisabled
                       : (root.selected ? Theme.textOnAccent : Theme.textPrimary)
                font.weight: Typography.weightBold
            }

            XpLabel {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: root.caption.length > 0
                text: root.caption
                role: "mono"
                font.pixelSize: 9
                color: root.selected ? Qt.rgba(1, 1, 1, 0.75) : Theme.textMuted
            }
        }
    }

    MouseArea {
        id: press
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabledButton
        // A panel button responds on press, not on release: that is what makes
        // switching banks feel instant rather than acknowledged.
        onPressed: {
            root.forceActiveFocus()
            root.clicked()
        }
        onPressAndHold: root.pressAndHold()
    }

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.clicked()
            event.accepted = true
        }
    }
}
