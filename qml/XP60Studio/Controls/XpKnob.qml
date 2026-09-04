import QtQuick
import QtQuick.Templates as T
import XP60Studio

// Rotary control for a continuous synth parameter.
//
// Drag vertically to change, Shift for fine steps, arrows/PageUp/PageDown from
// the keyboard, double-click to reset. Every knob also has an exact numeric
// readout beside it (see ParameterValueEditor) so no value is knob-only.
T.Dial {
    id: control

    // Draws the arc from the centre rather than from the minimum, for
    // parameters whose neutral value is the middle (pan, tuning, depths).
    property bool bipolar: false
    property color accentColor: Theme.accent
    property int defaultValue: bipolar ? Math.round((from + to) / 2) : from
    property string valueText: ""

    implicitWidth: 44
    implicitHeight: 44
    stepSize: 1
    wrap: false
    hoverEnabled: true
    // A full sweep needs a deliberate drag, not a flick.
    inputMode: T.Dial.Vertical

    Accessible.role: Accessible.Dial
    Accessible.name: valueText.length > 0 ? valueText : String(value)

    readonly property real arcStartAngle: -140
    readonly property real sweep: 280
    readonly property real normalised: control.to > control.from
                                       ? (control.value - control.from) / (control.to - control.from) : 0

    Keys.onPressed: function(event) {
        var step = (event.modifiers & Qt.ShiftModifier) ? 1 : Math.max(1, Math.round((to - from) / 32))
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Right) {
            value = Math.min(to, value + step); event.accepted = true
        } else if (event.key === Qt.Key_Down || event.key === Qt.Key_Left) {
            value = Math.max(from, value - step); event.accepted = true
        } else if (event.key === Qt.Key_PageUp) {
            value = Math.min(to, value + step * 4); event.accepted = true
        } else if (event.key === Qt.Key_PageDown) {
            value = Math.max(from, value - step * 4); event.accepted = true
        } else if (event.key === Qt.Key_Home) {
            value = defaultValue; event.accepted = true
        }
        if (event.accepted)
            control.moved()
    }

    background: Item {
        implicitWidth: control.implicitWidth
        implicitHeight: control.implicitHeight

        Canvas {
            id: track
            anchors.fill: parent
            // Repaint whenever anything it draws changes.
            readonly property real v: control.normalised
            readonly property color arc: control.enabled ? control.accentColor : Theme.textDisabled
            onVChanged: requestPaint()
            onArcChanged: requestPaint()
            Component.onCompleted: requestPaint()

            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                var cx = width / 2
                var cy = height / 2
                var r = Math.min(width, height) / 2 - 4
                var a0 = (control.arcStartAngle - 90) * Math.PI / 180
                var a1 = (control.arcStartAngle + control.sweep - 90) * Math.PI / 180

                ctx.lineCap = "round"
                ctx.lineWidth = 3
                ctx.strokeStyle = Theme.borderStrong
                ctx.beginPath(); ctx.arc(cx, cy, r, a0, a1); ctx.stroke()

                var mid = control.bipolar ? (a0 + a1) / 2 : a0
                var cur = a0 + (a1 - a0) * control.normalised
                ctx.strokeStyle = track.arc
                ctx.beginPath()
                ctx.arc(cx, cy, r, Math.min(mid, cur), Math.max(mid, cur))
                ctx.stroke()
            }
        }

        // Body
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width, parent.height) - 14
            height: width
            radius: width / 2
            color: control.pressed ? Theme.surfacePressed : Theme.surfaceRaised
            border.width: 1
            border.color: control.visualFocus ? Theme.focusRing
                        : control.hovered ? Theme.borderStrong : Theme.border
        }

        // Pointer
        Rectangle {
            width: 2
            height: Math.min(parent.width, parent.height) / 2 - 9
            radius: 1
            color: control.enabled ? control.accentColor : Theme.textDisabled
            antialiasing: true
            x: parent.width / 2 - width / 2
            y: 7
            transform: Rotation {
                origin.x: 1
                origin.y: Math.min(control.width, control.height) / 2 - 7
                angle: control.arcStartAngle + control.sweep * control.normalised
            }
        }
    }

    handle: Item {}

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onDoubleTapped: { control.value = control.defaultValue; control.moved() }
    }
}
