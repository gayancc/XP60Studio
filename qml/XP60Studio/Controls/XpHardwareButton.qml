import QtQuick
import XP60Studio

// A low-profile XP-60-style panel key. Bank/number delegates stretch across
// their destination column, giving the cap the broad horizontal proportions
// of the workstation's physical switches without adding vertical bulk.
Item {
    id: root

    property string text: ""
    property string caption: ""
    property bool showCaption: true
    property bool selected: false
    property bool enabledButton: true
    property bool indicator: false
    property real fill: 0
    property bool dropCandidate: false
    property bool dropActive: false
    property bool large: false

    signal clicked()
    signal pressAndHold()

    readonly property int capWidth: Math.max(38, width - (large ? 4 : 6))
    readonly property int capHeight: large ? 28 : 24
    readonly property bool held: press.pressed && press.containsMouse

    implicitWidth: large ? 86 : 68
    implicitHeight: 34
    activeFocusOnTab: enabledButton

    Accessible.role: Accessible.Button
    Accessible.name: root.text
    Accessible.description: root.caption
    Accessible.focusable: true
    Accessible.onPressAction: if (root.enabledButton) root.clicked()

    Rectangle {
        id: well
        anchors.centerIn: parent
        width: root.capWidth + 4
        height: root.capHeight + 5
        radius: 3
        color: Theme.surfaceSunken
        border.width: 1
        border.color: root.dropActive ? Theme.success : Theme.borderSubtle

        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top }
            anchors.leftMargin: 2
            anchors.rightMargin: 2
            anchors.topMargin: 1
            height: 1
            color: Qt.rgba(1, 1, 1, 0.07)
        }
    }

    Rectangle {
        anchors.centerIn: well
        width: well.width + (root.dropActive ? 4 : 2)
        height: well.height + (root.dropActive ? 4 : 2)
        radius: 4
        color: "transparent"
        visible: root.activeFocus || root.dropCandidate
        border.width: root.activeFocus || root.dropActive ? 2 : 1
        border.color: root.dropActive ? Theme.success : Theme.focusRing
        opacity: root.dropActive || root.activeFocus ? 1 : 0.42

        Behavior on opacity {
            enabled: !Motion.reducedMotion
            NumberAnimation { duration: Motion.durationFast }
        }
    }

    Item {
        id: cap
        width: root.capWidth
        height: root.capHeight
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2) + (root.held ? 2 : 0)

        Behavior on y {
            enabled: !Motion.reducedMotion
            NumberAnimation { duration: Motion.durationFast; easing.type: Motion.easingStandard }
        }

        Rectangle {
            anchors.fill: parent
            radius: 2
            color: {
                if (!root.enabledButton) return Theme.surfaceSunken
                if (root.dropActive) return Theme.successSoft
                if (root.held) return Theme.surfacePressed
                if (root.selected) return Theme.selection
                return press.containsMouse ? Theme.surfaceHover : Theme.surfaceRaised
            }
            border.width: root.selected || root.dropActive ? 2 : 1
            border.color: {
                if (!root.enabledButton) return Theme.borderSubtle
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

            Rectangle {
                anchors { left: parent.left; right: parent.right; top: parent.top }
                anchors.leftMargin: 3
                anchors.rightMargin: 3
                anchors.topMargin: 1
                height: 1
                color: Qt.rgba(1, 1, 1, root.held ? 0.025 : (root.selected ? 0.18 : 0.09))
            }
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                anchors.leftMargin: 3
                anchors.rightMargin: 3
                anchors.bottomMargin: 1
                height: 1
                color: Qt.rgba(0, 0, 0, root.held ? 0.08 : 0.48)
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 3
            width: root.large ? 20 : 16
            height: 2
            radius: 1
            color: root.dropActive ? Theme.success
                   : (root.selected ? Theme.accentText
                      : ((root.indicator || root.fill > 0) ? Theme.live
                                                          : Qt.rgba(0, 0, 0, 0.55)))
            opacity: root.selected || root.dropActive
                     ? 1
                     : ((root.indicator || root.fill > 0)
                        ? 0.45 + 0.55 * Math.min(1, root.fill) : 0.8)
        }

        XpLabel {
            anchors.horizontalCenter: parent.horizontalCenter
            y: root.showCaption && root.caption.length > 0 ? 3 : 7
            text: root.text
            role: "heading"
            font.pixelSize: root.large ? 13 : 12
            font.weight: Typography.weightBold
            color: !root.enabledButton ? Theme.textDisabled
                   : (root.selected ? Theme.textOnAccent : Theme.textPrimary)
        }

        XpLabel {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            visible: root.showCaption && root.caption.length > 0
            text: root.caption
            role: "mono"
            font.pixelSize: 7
            color: root.selected ? Qt.rgba(1, 1, 1, 0.72) : Theme.textMuted
        }
    }

    MouseArea {
        id: press
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabledButton
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
