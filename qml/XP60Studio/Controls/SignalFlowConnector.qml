import QtQuick
import QtQuick.Controls as QQC
import XP60Studio

// Routed arrow; points are view geometry, never protocol or audio decisions.
Canvas {
    id: root
    property color lineColor: Theme.borderStrong
    property var points: [Qt.point(0, height / 2), Qt.point(width, height / 2)]
    property bool open: true
    property string label: ""
    property point labelPosition: Qt.point(width / 2, height / 2)
    property bool interactive: false
    property string description: ""
    property real amount: -1
    signal activated()
    implicitWidth: 34
    implicitHeight: 12
    onPointsChanged: requestPaint()
    onLineColorChanged: requestPaint()
    onOpenChanged: requestPaint()
    onAmountChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        if (points.length < 2) return
        ctx.strokeStyle = lineColor
        ctx.fillStyle = lineColor
        ctx.globalAlpha = open ? 0.85 : 0.35
        ctx.lineWidth = open ? (amount < 0 ? 1.5 : 1 + 3 * amount) : 1
        ctx.setLineDash(open ? [] : [3, 4])
        ctx.beginPath()
        ctx.moveTo(points[0].x, points[0].y)
        for (var i = 1; i < points.length; ++i) ctx.lineTo(points[i].x, points[i].y)
        ctx.stroke()
        var end = points[points.length - 1]
        var prev = points[points.length - 2]
        var angle = Math.atan2(end.y - prev.y, end.x - prev.x)
        ctx.setLineDash([])
        ctx.beginPath()
        ctx.moveTo(end.x, end.y)
        ctx.lineTo(end.x - 6 * Math.cos(angle - 0.5), end.y - 6 * Math.sin(angle - 0.5))
        ctx.lineTo(end.x - 6 * Math.cos(angle + 0.5), end.y - 6 * Math.sin(angle + 0.5))
        ctx.closePath()
        ctx.fill()
    }
    Rectangle {
        visible: root.label.length > 0
        x: root.labelPosition.x - width / 2
        y: root.labelPosition.y - height / 2
        width: readout.implicitWidth + Metrics.spacingXs
        height: readout.implicitHeight
        color: Theme.surface
        activeFocusOnTab: root.interactive
        Accessible.role: Accessible.Button
        Accessible.name: root.description
        Keys.onReturnPressed: if (root.interactive) root.activated()
        Keys.onSpacePressed: if (root.interactive) root.activated()
        border.color: activeFocus ? Theme.focusRing : "transparent"
        border.width: 1
        MouseArea {
            id: routeMouse
            anchors { fill: parent; margins: -4 }
            enabled: root.interactive
            hoverEnabled: true; cursorShape: Qt.PointingHandCursor
            onClicked: root.activated()
        }
        QQC.ToolTip.visible: routeMouse.containsMouse
        QQC.ToolTip.text: root.description
        XpLabel {
            id: readout
            anchors.centerIn: parent
            text: root.label
            role: "caption"
            color: root.open ? root.lineColor : Theme.textMuted
        }
    }
}
