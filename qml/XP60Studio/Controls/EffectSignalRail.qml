import QtQuick
import XP60Studio

// One connection on the effects canvas.
//
// Rails are not all equal. The main audio spine is drawn heavy and opaque; a
// send is thinner and takes its weight from how much is actually being sent; a
// path carrying nothing is a faint dashed line that still shows the topology
// exists without competing for attention. That ordering is the whole point --
// seven equally weighted cables is what made the previous canvas read as a
// wiring diagram.
Item {
    id: root

    // "main" | "send" | "return" | "direct"
    property string rail: "send"
    property color tint: Theme.accent
    // Orthogonal waypoints in this item's coordinates.
    property var points: []
    // Carrying signal. False means a zero send or a dead upstream stage.
    property bool open: true
    // 0..1 from the send level; scales weight and opacity within the class.
    property real strength: 1.0
    property bool isolated: false
    property bool dimmed: false
    // Restrained travelling pulse, only while live audition is on and only on
    // paths that actually carry signal.
    property bool flowing: false

    readonly property real baseWidth: rail === "main" ? 3.0
                                    : rail === "return" ? 2.0
                                    : rail === "direct" ? 1.8 : 1.6

    onPointsChanged: canvas.requestPaint()
    onOpenChanged: canvas.requestPaint()
    onStrengthChanged: canvas.requestPaint()
    onIsolatedChanged: canvas.requestPaint()
    onTintChanged: canvas.requestPaint()
    onRailChanged: canvas.requestPaint()

    opacity: root.dimmed ? 0.12 : 1.0
    Behavior on opacity {
        enabled: !Motion.reducedMotion
        NumberAnimation { duration: Motion.durationNormal; easing.type: Motion.easingStandard }
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        renderStrategy: Canvas.Cooperative

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var pts = root.points
            if (!pts || pts.length < 2)
                return

            var s = Math.max(0, Math.min(1, root.strength))
            // Within a class, a bigger send is a heavier line -- but a send
            // never outweighs the spine, so the hierarchy cannot inverted by
            // turning a knob up.
            var w = root.baseWidth * (root.rail === "main" ? 1.0 : 0.55 + 0.45 * s)
            if (root.isolated)
                w += 1.2

            ctx.lineWidth = root.open ? w : 1.0
            ctx.strokeStyle = root.tint
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.globalAlpha = root.open
                ? (root.isolated ? 1.0 : (root.rail === "main" ? 0.92 : 0.30 + 0.55 * s))
                : 0.20
            if (!root.open)
                ctx.setLineDash([3, 4])

            // Rounded corners on the waypoints: orthogonal lanes stay readable
            // but stop looking like a schematic.
            var r = 7
            ctx.beginPath()
            ctx.moveTo(pts[0].x, pts[0].y)
            for (var i = 1; i < pts.length - 1; ++i) {
                var prev = pts[i - 1], cur = pts[i], next = pts[i + 1]
                var d1 = Math.hypot(cur.x - prev.x, cur.y - prev.y)
                var d2 = Math.hypot(next.x - cur.x, next.y - cur.y)
                var r1 = Math.min(r, d1 / 2), r2 = Math.min(r, d2 / 2)
                var e1 = Qt.point(cur.x - (cur.x - prev.x) / (d1 || 1) * r1,
                                  cur.y - (cur.y - prev.y) / (d1 || 1) * r1)
                var e2 = Qt.point(cur.x + (next.x - cur.x) / (d2 || 1) * r2,
                                  cur.y + (next.y - cur.y) / (d2 || 1) * r2)
                ctx.lineTo(e1.x, e1.y)
                ctx.quadraticCurveTo(cur.x, cur.y, e2.x, e2.y)
            }
            ctx.lineTo(pts[pts.length - 1].x, pts[pts.length - 1].y)
            ctx.stroke()

            // Arrowhead only where the path lands, so direction is legible
            // without decorating every segment.
            if (root.open) {
                var end = pts[pts.length - 1], before = pts[pts.length - 2]
                var ang = Math.atan2(end.y - before.y, end.x - before.x)
                var head = 5 + w
                ctx.setLineDash([])
                ctx.globalAlpha = root.isolated ? 1.0 : 0.85
                ctx.beginPath()
                ctx.moveTo(end.x, end.y)
                ctx.lineTo(end.x - head * Math.cos(ang - 0.42), end.y - head * Math.sin(ang - 0.42))
                ctx.lineTo(end.x - head * Math.cos(ang + 0.42), end.y - head * Math.sin(ang + 0.42))
                ctx.closePath()
                ctx.fillStyle = root.tint
                ctx.fill()
            }
            ctx.globalAlpha = 1
            ctx.setLineDash([])
        }
    }

    // The audition pulse. One small travelling dot, not a glow or a marquee:
    // it says "signal is moving through this configured path" and nothing more.
    Rectangle {
        id: pulse
        visible: root.flowing && root.open && !Motion.reducedMotion && !root.dimmed
        width: 5; height: 5; radius: 2.5
        color: root.tint
        opacity: 0.9

        property real t: 0
        // Walks the polyline so the dot follows the lane rather than cutting
        // across it.
        readonly property var pos: {
            var pts = root.points
            if (!pts || pts.length < 2)
                return Qt.point(-10, -10)
            var total = 0, segs = []
            for (var i = 1; i < pts.length; ++i) {
                var d = Math.hypot(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y)
                segs.push(d); total += d
            }
            if (total <= 0)
                return Qt.point(-10, -10)
            var want = t * total, acc = 0
            for (var j = 0; j < segs.length; ++j) {
                if (acc + segs[j] >= want) {
                    var f = segs[j] > 0 ? (want - acc) / segs[j] : 0
                    return Qt.point(pts[j].x + (pts[j + 1].x - pts[j].x) * f,
                                    pts[j].y + (pts[j + 1].y - pts[j].y) * f)
                }
                acc += segs[j]
            }
            return pts[pts.length - 1]
        }
        x: pos.x - width / 2
        y: pos.y - height / 2

        NumberAnimation on t {
            running: pulse.visible
            loops: Animation.Infinite
            from: 0; to: 1
            duration: 2200
        }
    }
}
