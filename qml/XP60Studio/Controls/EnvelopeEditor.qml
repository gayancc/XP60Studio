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
    // Which node the pointer is currently moving, for the crosshair.
    property int activeIndex: -1
    signal pointMoved(int index, real x, real y)

    color: Theme.surfaceSunken
    radius: Metrics.radiusSm
    border.width: 1
    border.color: Theme.borderSubtle
    implicitHeight: 140

    readonly property int padding: 18
    function plotX(x) { return padding + x * (width - 2 * padding) }
    function plotY(y) { return height - padding - y * (height - 2 * padding) }
    function unplotX(px) { return Math.max(0, Math.min(1, (px - padding) / Math.max(1, width - 2 * padding))) }
    function unplotY(py) { return Math.max(0, Math.min(1, (height - padding - py) / Math.max(1, height - 2 * padding))) }

    Canvas {
        id: canvas
        anchors.fill: parent
        readonly property var pts: root.points
        readonly property int active: root.activeIndex
        onPtsChanged: requestPaint()
        onActiveChanged: requestPaint()
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
            // A guide down from each breakpoint, so the graph and the stage
            // knobs beneath it read as one control rather than two.
            if (root.points && root.points.length > 1) {
                for (var s = 0; s < root.points.length; ++s) {
                    if (root.points[s].draggable !== true)
                        continue
                    var sx = root.plotX(root.points[s].x)
                    ctx.beginPath(); ctx.moveTo(sx, p); ctx.lineTo(sx, height - p); ctx.stroke()
                }
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

            // Crosshair on the node being dragged.
            if (root.activeIndex >= 0 && root.activeIndex < root.points.length) {
                var ax = root.plotX(root.points[root.activeIndex].x)
                var ay = root.plotY(root.points[root.activeIndex].y)
                ctx.strokeStyle = Qt.rgba(root.accentColor.r, root.accentColor.g,
                                          root.accentColor.b, 0.55)
                ctx.lineWidth = 1
                ctx.setLineDash([2, 3])
                ctx.beginPath(); ctx.moveTo(p, ay); ctx.lineTo(width - p, ay); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(ax, p); ctx.lineTo(ax, height - p); ctx.stroke()
                ctx.setLineDash([])
            }
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
        // Count, not the list. `points` is a QVariantList rebuilt on every
        // move, and a Repeater given the list resets its model each time —
        // deleting the node under the pointer halfway through the drag.
        model: root.points.length
        delegate: Item {
            id: handle
            required property int index
            readonly property var modelData: root.points[index]
            readonly property bool present: modelData !== undefined
            readonly property bool draggable: present && modelData.draggable === true

            x: (present ? root.plotX(modelData.x) : 0) - width / 2
            y: (present ? root.plotY(modelData.y) : 0) - height / 2
            width: 22
            height: 22
            visible: present && (draggable || index === 0)

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
                onActiveChanged: root.activeIndex = active ? handle.index : -1
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
                text: handle.present && handle.modelData.label !== undefined ? handle.modelData.label : ""
                role: "overline"
                color: root.accentColor
            }
        }
    }
}
