import QtQuick
import XP60Studio

// Compact LFO shape preview for documented waveforms (TRI…CHS).
Canvas {
    id: root

    property int shapeIndex: 0
    property color accentColor: Theme.accent
    property real phase: 0

    onShapeIndexChanged: requestPaint()
    onAccentColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        if (width < 4 || height < 4) return
        ctx.strokeStyle = accentColor
        ctx.lineWidth = 1.25
        ctx.beginPath()
        var mid = height / 2
        var amp = height / 2 - 1
        var steps = Math.max(16, Math.floor(width))
        for (var i = 0; i <= steps; ++i) {
            var t = i / steps
            var y = mid
            switch (shapeIndex) {
            case 0: // TRI
                y = mid - amp * (t < 0.5 ? (4 * t - 1) : (3 - 4 * t))
                break
            case 1: // SIN
                y = mid - amp * Math.sin(t * Math.PI * 2)
                break
            case 2: // SAW
                y = mid - amp * (2 * t - 1)
                break
            case 3: // SQR
                y = mid - amp * (t < 0.5 ? 1 : -1)
                break
            case 4: // TRP (trapezoid-ish)
                if (t < 0.2) y = mid - amp * (t / 0.2)
                else if (t < 0.5) y = mid - amp
                else if (t < 0.7) y = mid - amp * (1 - (t - 0.5) / 0.2)
                else y = mid + amp
                break
            case 5: // S&H
                y = mid - amp * (Math.floor(t * 4) % 2 === 0 ? 0.7 : -0.4)
                break
            case 6: // RND
                y = mid - amp * Math.sin(t * 17.3) * Math.cos(t * 9.1)
                break
            default: // CHS
                y = mid - amp * (t < 0.5 ? (1 - 2 * t) : (2 * t - 3))
                break
            }
            if (i === 0) ctx.moveTo(t * width, y)
            else ctx.lineTo(t * width, y)
        }
        ctx.stroke()
    }
}
