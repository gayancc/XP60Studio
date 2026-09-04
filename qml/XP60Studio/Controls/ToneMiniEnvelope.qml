import QtQuick
import XP60Studio

// The small amplitude curve on a Tone card. Read-only: it shows the shape,
// the full editor below changes it.
Canvas {
    id: root

    // [{x, y}] normalised 0..1, in time order.
    property var points: []
    property color strokeColor: Theme.accent
    property bool dimmed: false

    implicitHeight: 46
    onPointsChanged: requestPaint()
    onStrokeColorChanged: requestPaint()
    onDimmedChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        if (!points || points.length < 2)
            return

        var pad = 3
        var w = width - 2 * pad
        var h = height - 2 * pad
        function px(p) { return pad + p.x * w }
        function py(p) { return pad + (1 - p.y) * h }

        ctx.globalAlpha = dimmed ? 0.35 : 1.0

        // Soft fill under the curve so the shape reads at a glance.
        ctx.beginPath()
        ctx.moveTo(px(points[0]), pad + h)
        for (var i = 0; i < points.length; ++i)
            ctx.lineTo(px(points[i]), py(points[i]))
        ctx.lineTo(px(points[points.length - 1]), pad + h)
        ctx.closePath()
        ctx.fillStyle = Qt.rgba(strokeColor.r, strokeColor.g, strokeColor.b, 0.14)
        ctx.fill()

        ctx.beginPath()
        ctx.moveTo(px(points[0]), py(points[0]))
        for (var j = 1; j < points.length; ++j)
            ctx.lineTo(px(points[j]), py(points[j]))
        ctx.strokeStyle = strokeColor
        ctx.lineWidth = 1.5
        ctx.lineJoin = "round"
        ctx.stroke()

        ctx.fillStyle = strokeColor
        for (var k = 1; k < points.length; ++k) {
            ctx.beginPath()
            ctx.arc(px(points[k]), py(points[k]), 2, 0, 2 * Math.PI)
            ctx.fill()
        }
    }
}
