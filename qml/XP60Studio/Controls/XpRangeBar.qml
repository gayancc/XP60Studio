import QtQuick
import XP60Studio

// Numeric range with two draggable ends — used for velocity.
//
// The track is a soft-to-hard ramp rather than a plain bar: velocity is a
// dynamic, not a number line, and the ramp says which end is a light touch
// without adding a legend. The active window animates so a drag, an undo or a
// re-fetch reads as movement.
Item {
    id: root

    property int lower: 1
    property int upper: 127
    property int minimumValue: 1
    property int maximumValue: 127
    property color accentColor: Theme.accent
    property bool interactive: true
    signal rangeEdited(int lower, int upper)

    implicitHeight: 22
    readonly property real span: Math.max(1, maximumValue - minimumValue)
    function pos(v) { return (v - minimumValue) / span * width }
    function valueAt(x) {
        return Math.round(minimumValue + Math.max(0, Math.min(1, x / Math.max(1, width))) * span)
    }

    property real animLower: lower
    property real animUpper: upper
    Behavior on animLower { NumberAnimation { duration: Motion.durationFast; easing.type: Motion.easingStandard } }
    Behavior on animUpper { NumberAnimation { duration: Motion.durationFast; easing.type: Motion.easingStandard } }
    onLowerChanged: animLower = lower
    onUpperChanged: animUpper = upper

    property int grabbedEdge: -1

    Rectangle {
        id: track
        anchors.fill: parent
        radius: Metrics.radiusSm
        color: Theme.surfaceSunken
        border.width: 1
        border.color: Theme.borderSubtle
        clip: true

        // The soft-to-hard ramp, dimmed outside the active window.
        Canvas {
            id: ramp
            anchors { fill: parent; margins: 1 }
            readonly property real lo: root.animLower
            readonly property real hi: root.animUpper
            readonly property color accent: root.accentColor
            onLoChanged: requestPaint()
            onHiChanged: requestPaint()
            onAccentChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            Component.onCompleted: requestPaint()

            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                var w = width
                var h = height
                var x0 = Math.max(0, root.pos(lo) - 1)
                var x1 = Math.min(w, root.pos(hi) - 1)

                // Whole ramp, faint: the part of the dynamic this Tone ignores.
                ctx.fillStyle = Qt.rgba(accent.r, accent.g, accent.b, 0.12)
                ctx.beginPath()
                ctx.moveTo(0, h)
                ctx.lineTo(w, h - h * 0.82)
                ctx.lineTo(w, h)
                ctx.closePath()
                ctx.fill()

                // Active window, full strength.
                ctx.save()
                ctx.beginPath()
                ctx.rect(x0, 0, Math.max(1, x1 - x0), h)
                ctx.clip()
                var grad = ctx.createLinearGradient(0, 0, w, 0)
                grad.addColorStop(0, Qt.rgba(accent.r, accent.g, accent.b, 0.45))
                grad.addColorStop(1, Qt.rgba(accent.r, accent.g, accent.b, 0.95))
                ctx.fillStyle = grad
                ctx.beginPath()
                ctx.moveTo(0, h)
                ctx.lineTo(w, h - h * 0.82)
                ctx.lineTo(w, h)
                ctx.closePath()
                ctx.fill()
                ctx.restore()
            }
        }

        // Edge handles.
        Repeater {
            model: 2
            delegate: Rectangle {
                required property int index
                readonly property real value: index === 0 ? root.animLower : root.animUpper
                x: Math.max(0, Math.min(root.width - width, root.pos(value) - width / 2))
                width: root.grabbedEdge === index ? 4 : 2
                height: parent.height
                color: root.accentColor
                opacity: root.grabbedEdge === index ? 1 : 0.8
                Behavior on width { NumberAnimation { duration: Motion.durationFast } }
                Behavior on opacity { NumberAnimation { duration: Motion.durationFast } }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.interactive
        cursorShape: Qt.SizeHorCursor
        onPressed: function(mouse) {
            root.forceActiveFocus()
            var v = root.valueAt(mouse.x)
            root.grabbedEdge = Math.abs(v - root.lower) <= Math.abs(v - root.upper) ? 0 : 1
            apply(v)
        }
        onPositionChanged: function(mouse) { if (pressed) apply(root.valueAt(mouse.x)) }
        onReleased: root.grabbedEdge = -1
        function apply(v) {
            if (root.grabbedEdge === 0)
                root.rangeEdited(Math.min(v, root.upper), root.upper)
            else
                root.rangeEdited(root.lower, Math.max(v, root.lower))
        }
    }

    activeFocusOnTab: root.interactive
    Keys.onPressed: function(event) {
        var edge = root.grabbedEdge < 0 ? 0 : root.grabbedEdge
        var step = (event.modifiers & Qt.ShiftModifier) ? 10 : 1
        if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
            var delta = event.key === Qt.Key_Right ? step : -step
            if (edge === 0)
                root.rangeEdited(Math.max(root.minimumValue, Math.min(root.lower + delta, root.upper)), root.upper)
            else
                root.rangeEdited(root.lower, Math.min(root.maximumValue, Math.max(root.upper + delta, root.lower)))
            event.accepted = true
        } else if (event.key === Qt.Key_Space) {
            root.grabbedEdge = edge === 0 ? 1 : 0
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Metrics.radiusSm
        color: "transparent"
        border.width: 2
        border.color: Theme.focusRing
        visible: root.activeFocus
    }

    Accessible.role: Accessible.Slider
    Accessible.name: qsTr("Velocity range")
    Accessible.description: qsTr("%1 to %2. Space switches edge, arrows move it.").arg(root.lower).arg(root.upper)
}
