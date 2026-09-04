import QtQuick
import XP60Studio

// Compact semantic badge. Never colour-only: the text carries the meaning.
// tone: "neutral" | "success" | "warning" | "error" | "info" | "accent" | "live"
Rectangle {
    id: root

    property string text: ""
    property string tone: "neutral"
    property bool showDot: true
    property bool pulsing: false

    readonly property color foreground: Theme.toneForeground(tone)

    implicitWidth: row.implicitWidth + 2 * Metrics.spacingSm + 2
    implicitHeight: 22
    radius: Metrics.radiusPill
    color: Theme.toneBackground(tone)
    border.width: Metrics.borderWidth
    border.color: Qt.rgba(foreground.r, foreground.g, foreground.b, 0.35)

    Accessible.role: Accessible.StaticText
    Accessible.name: root.text

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6
        Rectangle {
            visible: root.showDot
            width: 7
            height: 7
            radius: 4
            color: root.foreground
            anchors.verticalCenter: parent.verticalCenter
            SequentialAnimation on opacity {
                running: root.pulsing && !Motion.reducedMotion
                loops: Animation.Infinite
                NumberAnimation { to: 0.3; duration: 600 }
                NumberAnimation { to: 1.0; duration: 600 }
            }
        }
        XpLabel {
            text: root.text
            role: "caption"
            color: root.foreground
            font.weight: Typography.weightMedium
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
