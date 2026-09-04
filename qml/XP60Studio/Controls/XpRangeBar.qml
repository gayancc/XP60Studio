import QtQuick
import XP60Studio

// Numeric range with two draggable ends — used for velocity.
Item {
    id: root

    property int lower: 1
    property int upper: 127
    property int minimumValue: 1
    property int maximumValue: 127
    property color accentColor: Theme.accent
    signal rangeEdited(int lower, int upper)

    implicitHeight: 20
    readonly property real span: Math.max(1, maximumValue - minimumValue)
    function pos(v) { return (v - minimumValue) / span * width }
    function valueAt(x) {
        return Math.round(minimumValue + Math.max(0, Math.min(1, x / Math.max(1, width))) * span)
    }

    Rectangle {
        anchors.fill: parent
        radius: Metrics.radiusSm
        color: Theme.surfaceSunken
        border.width: 1
        border.color: Theme.borderSubtle

        Rectangle {
            x: root.pos(root.lower)
            width: Math.max(3, root.pos(root.upper) - root.pos(root.lower))
            height: parent.height - 2
            y: 1
            radius: Metrics.radiusSm - 1
            color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.55)
        }
    }

    MouseArea {
        anchors.fill: parent
        property int grabbed: -1
        cursorShape: Qt.SizeHorCursor
        onPressed: function(mouse) {
            var v = root.valueAt(mouse.x)
            grabbed = Math.abs(v - root.lower) <= Math.abs(v - root.upper) ? 0 : 1
            apply(v)
        }
        onPositionChanged: function(mouse) { if (pressed) apply(root.valueAt(mouse.x)) }
        function apply(v) {
            if (grabbed === 0)
                root.rangeEdited(Math.min(v, root.upper), root.upper)
            else
                root.rangeEdited(root.lower, Math.max(v, root.lower))
        }
    }
}
