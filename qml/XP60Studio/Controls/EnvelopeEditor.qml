import QtQuick
import XP60Studio

// The large envelope editor: draggable points, keyboard adjustment, stage
// labels and a grid. Emits normalised positions; the view model converts them
// to Roland raw values so no unit conversion happens in QML.
Rectangle {
    id: root

    // [{x, y, draggable, hasLevel, timeRaw, levelRaw, label}]
    property var points: []
    property color accentColor: Theme.accent
    // Pitch and filter envelopes swing around a centre line; the amp envelope
    // rises from silence.
    property bool bipolar: false
    property int selectedIndex: -1
    signal pointMoved(int index, real x, real y)

    color: Theme.surfaceSunken
    radius: Metrics.radiusSm
    border.width: 1
    border.color: Theme.borderSubtle
    implicitHeight: 180

    readonly property int padding: 18
    function plotX(x) { return padding + x * (width - 2 * padding) }
    function plotY(y) { return height - padding - y * (height - 2 * padding) }
    function unplotX(px) { return Math.max(0, Math.min(1, (px - padding) / Math.max(1, width - 2 * padding))) }
    function unplotY(py) { return Math.max(0, Math.min(1, (height - padding - py) / Math.max(1, height - 2 * padding))) }

    Canvas {
        id: canvas
        anchors.fill: parent
        readonly property var pts: root.points
        onPtsChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Component.onCompleted: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var p = root.padding

            // Grid
            ctx.strokeStyle = Theme.borderSubtle
            ctx.lineWidth = 1
            for (var g = 0; g <= 4; ++g) {
                var gy = p + (height - 2 * p) * g / 4
                ctx.beginPath(); ctx.moveTo(p, gy); ctx.lineTo(width - p, gy); ctx.stroke()
            }
            // Centre line for bipolar envelopes
            if (root.bipolar) {
                ctx.strokeStyle = Theme.border
                ctx.setLineDash([3, 3])
                var cy = root.plotY(0.5)
                ctx.beginPath(); ctx.moveTo(p, cy); ctx.lineTo(width - p, cy); ctx.stroke()
                ctx.setLineDash([])
            }

            if (!root.points || root.points.length < 2)
                return

            // Filled area
            ctx.beginPath()
            ctx.moveTo(root.plotX(root.points[0].x), root.plotY(root.bipolar ? 0.5 : 0))
            for (var i = 0; i < root.points.length; ++i)
                ctx.lineTo(root.plotX(root.points[i].x), root.plotY(root.points[i].y))
            ctx.lineTo(root.plotX(root.points[root.points.length - 1].x), root.plotY(root.bipolar ? 0.5 : 0))
            ctx.closePath()
            ctx.fillStyle = Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.12)
            ctx.fill()

            // Curve
            ctx.beginPath()
            ctx.moveTo(root.plotX(root.points[0].x), root.plotY(root.points[0].y))
            for (var j = 1; j < root.points.length; ++j)
                ctx.lineTo(root.plotX(root.points[j].x), root.plotY(root.points[j].y))
            ctx.strokeStyle = root.accentColor
            ctx.lineWidth = 2
            ctx.lineJoin = "round"
            ctx.stroke()
        }
    }

    // Axis hints
    XpLabel {
        anchors { left: parent.left; leftMargin: 4; top: parent.top; topMargin: 2 }
        text: root.bipolar ? "+" : "Level"
        role: "overline"
        muted: true
    }
    XpLabel {
        visible: root.bipolar
        anchors { left: parent.left; leftMargin: 4; verticalCenter: parent.verticalCenter }
        text: "0"
        role: "overline"
        muted: true
    }
    XpLabel {
        anchors { left: parent.left; leftMargin: 4; bottom: parent.bottom; bottomMargin: 2 }
        text: root.bipolar ? "−" : "0"
        role: "overline"
        muted: true
    }

    Repeater {
        model: root.points
        delegate: Item {
            id: handle
            required property var modelData
            required property int index
            readonly property bool draggable: modelData.draggable === true

            x: root.plotX(modelData.x) - width / 2
            y: root.plotY(modelData.y) - height / 2
            width: 22
            height: 22
            visible: draggable || index === 0

            Rectangle {
                anchors.centerIn: parent
                width: handle.draggable ? (drag.active || hover.hovered ? 13 : 10) : 7
                height: width
                radius: width / 2
                color: handle.draggable ? root.accentColor : Theme.textMuted
                border.width: 2
                border.color: Theme.surfaceSunken
                Behavior on width { NumberAnimation { duration: Motion.durationFast } }
            }
            Rectangle {
                visible: handle.activeFocus
                anchors.centerIn: parent
                width: 20; height: 20; radius: 10
                color: "transparent"
                border.width: 1
                border.color: Theme.focusRing
            }

            HoverHandler { id: hover; enabled: handle.draggable; cursorShape: Qt.SizeAllCursor }

            DragHandler {
                id: drag
                enabled: handle.draggable
                target: null
                onCentroidChanged: {
                    if (!active)
                        return
                    root.pointMoved(handle.index,
                                    root.unplotX(centroid.position.x + handle.x),
                                    root.unplotY(centroid.position.y + handle.y))
                }
            }

            // Keyboard adjustment, so the envelope is reachable without a mouse.
            activeFocusOnTab: handle.draggable
            Keys.onPressed: function(event) {
                if (!handle.draggable)
                    return
                var stepX = 0, stepY = 0
                var fine = (event.modifiers & Qt.ShiftModifier) ? 0.005 : 0.02
                if (event.key === Qt.Key_Left) stepX = -fine
                else if (event.key === Qt.Key_Right) stepX = fine
                else if (event.key === Qt.Key_Up) stepY = fine
                else if (event.key === Qt.Key_Down) stepY = -fine
                else return
                event.accepted = true
                root.pointMoved(handle.index,
                                Math.max(0, Math.min(1, handle.modelData.x + stepX)),
                                Math.max(0, Math.min(1, handle.modelData.y + stepY)))
            }

            XpLabel {
                visible: handle.draggable && (hover.hovered || drag.active)
                anchors { bottom: parent.top; horizontalCenter: parent.horizontalCenter }
                text: handle.modelData.label !== undefined ? handle.modelData.label : ""
                role: "overline"
                color: root.accentColor
            }
        }
    }
}
