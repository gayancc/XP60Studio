import QtQuick
import XP60Studio
import XP60Studio.Presentation

// The single authoritative XP-60 connection indicator (header + rail).
// State is conveyed by text and dot colour together, never colour alone.
Rectangle {
    id: root

    required property int connectionState
    property string label: ""
    property string detail: ""
    property bool compact: false

    readonly property string tone: {
        switch (connectionState) {
        case ConnectionState.Connected: return "live"
        case ConnectionState.Connecting: return "warning"
        case ConnectionState.Error: return "error"
        }
        return "neutral"
    }
    readonly property color foreground: Theme.toneForeground(tone)

    implicitWidth: row.implicitWidth + 2 * Metrics.spacingMd
    implicitHeight: compact ? 26 : 30
    radius: Metrics.radiusPill
    color: Theme.toneBackground(tone)
    border.width: Metrics.borderWidth
    border.color: Qt.rgba(foreground.r, foreground.g, foreground.b, 0.35)

    Accessible.role: Accessible.StaticText
    Accessible.name: root.label + (root.detail.length ? ", " + root.detail : "")

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Metrics.spacingSm
        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: root.foreground
            anchors.verticalCenter: parent.verticalCenter
            SequentialAnimation on opacity {
                running: root.connectionState === ConnectionState.Connecting && !Motion.reducedMotion
                loops: Animation.Infinite
                NumberAnimation { to: 0.25; duration: 500 }
                NumberAnimation { to: 1.0; duration: 500 }
            }
        }
        XpLabel {
            text: root.label
            role: compact ? "caption" : "body"
            font.weight: Typography.weightMedium
            font.letterSpacing: 0.6
            color: root.foreground
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
