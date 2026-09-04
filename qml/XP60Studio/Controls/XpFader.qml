import QtQuick
import QtQuick.Templates as T
import XP60Studio

// Vertical fader for continuous musical parameters (level-like).
T.Slider {
    id: control

    property bool bipolar: false
    property color accentColor: Theme.accent
    property string valueText: ""
    property int defaultValue: bipolar ? Math.round((from + to) / 2) : from

    orientation: Qt.Vertical
    implicitWidth: 28
    implicitHeight: 96
    stepSize: 1
    live: true
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.Slider
    Accessible.name: valueText.length > 0 ? valueText : String(value)

    Keys.onPressed: function(event) {
        var step = (event.modifiers & Qt.ShiftModifier) ? 1 : Math.max(1, Math.round((to - from) / 32))
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Right) {
            value = Math.min(to, value + step); event.accepted = true
        } else if (event.key === Qt.Key_Down || event.key === Qt.Key_Left) {
            value = Math.max(from, value - step); event.accepted = true
        } else if (event.key === Qt.Key_Home) {
            value = defaultValue; event.accepted = true
        }
    }

    background: Item {
        implicitWidth: 10
        implicitHeight: control.implicitHeight
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 4
            height: parent.height
            radius: 2
            color: Theme.borderStrong
            // Fill from bottom (or from centre when bipolar)
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                radius: 2
                color: control.enabled ? control.accentColor : Theme.textDisabled
                readonly property real n: control.to > control.from
                    ? (control.value - control.from) / (control.to - control.from) : 0
                height: control.bipolar ? Math.abs(n - 0.5) * parent.height : n * parent.height
                y: control.bipolar
                   ? (n >= 0.5 ? parent.height * 0.5 - height : parent.height * 0.5)
                   : parent.height - height
            }
        }
    }

    handle: Rectangle {
        width: 18
        height: 10
        radius: 3
        x: (control.availableWidth - width) / 2
        y: control.visualPosition * (control.availableHeight - height)
        color: control.pressed ? Theme.surfacePressed : Theme.surfaceRaised
        border.width: 1
        border.color: control.visualFocus ? Theme.focusRing
                    : control.hovered ? Theme.borderStrong : Theme.border
    }
}
