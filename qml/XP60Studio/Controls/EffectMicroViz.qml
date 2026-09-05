import QtQuick
import XP60Studio

// A small drawing of what a processor *is*, inside its node.
//
// These are topology and parameter pictures, never audio analysis: nothing here
// is sampled from the instrument or animated to imply metering. A phaser draws
// its modulation shape, a delay draws its taps, a hexa-chorus draws six voices
// across the stereo field. The point is that the node says what it does without
// the musician reading its title.
//
// Honesty constraint, and the reason `kind` is derived from an algorithm *name*
// rather than from EFX parameter bytes: which byte carries which EFX control is
// established for only 3 of the XP-60's 40 algorithms (DEVICE_ACCEPTANCE.md
// area 8). So the EFX drawings are keyed to the algorithm's identity, which is
// verified, and never to `common.efx_parameter_*`, which is not. Chorus and
// Reverb do read real values, because those parameters are supported and
// carried by the C++ model.
Item {
    id: root

    // "chorus" | "reverb" | "delay" | "phaser" | "rotary" | "compressor"
    // | "drive" | "eq" | "filter" | "pitch" | "flanger" | "generic" | "source"
    // | "output"
    property string kind: "generic"
    property color tint: Theme.accent
    // 0..1, drives how far the drawing is "open". Only ever fed from values the
    // model actually supplies.
    property real amount: 0.5
    property real secondary: 0.5
    property bool muted: false

    implicitWidth: 64
    implicitHeight: 26

    onKindChanged: canvas.requestPaint()
    onTintChanged: canvas.requestPaint()
    onAmountChanged: canvas.requestPaint()
    onSecondaryChanged: canvas.requestPaint()
    onMutedChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    // A kind change is a different processor, so the drawing crossfades rather
    // than snapping: the transition is what tells the eye the algorithm moved.
    opacity: root.muted ? 0.35 : 1.0
    Behavior on opacity {
        enabled: !Motion.reducedMotion
        NumberAnimation { duration: Motion.durationFast; easing.type: Motion.easingStandard }
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        renderStrategy: Canvas.Cooperative

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var w = width, h = height
            if (w <= 2 || h <= 2)
                return
            var mid = h / 2
            var a = Math.max(0, Math.min(1, root.amount))
            var b = Math.max(0, Math.min(1, root.secondary))

            ctx.strokeStyle = root.tint
            ctx.fillStyle = root.tint
            ctx.lineWidth = 1.4
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            function baseline(alpha) {
                ctx.globalAlpha = alpha
                ctx.beginPath()
                ctx.moveTo(0, mid)
                ctx.lineTo(w, mid)
                ctx.stroke()
            }

            switch (root.kind) {

            // Six modulated voices spread across the stereo field. The count is
            // the identity of HEXA-CHORUS; the spread follows chorus depth.
            case "chorus": {
                var voices = 6
                for (var v = 0; v < voices; ++v) {
                    var t = voices === 1 ? 0.5 : v / (voices - 1)
                    var spread = (t - 0.5) * 2
                    ctx.globalAlpha = 0.35 + 0.45 * (1 - Math.abs(spread))
                    ctx.beginPath()
                    for (var x = 0; x <= w; x += 2) {
                        var phase = (x / w) * Math.PI * 2 + t * Math.PI * 1.1
                        var y = mid + Math.sin(phase) * (h * 0.30 * a) + spread * (h * 0.16)
                        if (x === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                    }
                    ctx.stroke()
                }
                break
            }

            // Decay envelope of a room. Longer time, longer tail.
            case "reverb": {
                var tail = 0.25 + 0.7 * a
                ctx.globalAlpha = 0.9
                ctx.beginPath()
                ctx.moveTo(1, h - 1)
                ctx.lineTo(1, 2)
                ctx.stroke()
                ctx.globalAlpha = 0.55
                ctx.beginPath()
                for (var rx = 1; rx <= w; rx += 2) {
                    var rt = rx / w
                    var ry = h - 1 - (h - 3) * Math.exp(-rt / Math.max(0.08, tail * 0.5))
                    if (rx === 1) ctx.moveTo(rx, ry); else ctx.lineTo(rx, ry)
                }
                ctx.stroke()
                // Sparse reflections under the curve read as "space".
                ctx.globalAlpha = 0.4
                for (var k = 1; k < 7; ++k) {
                    var kx = (k / 7) * w
                    var kh = (h - 4) * Math.exp(-(kx / w) / Math.max(0.08, tail * 0.5))
                    ctx.beginPath()
                    ctx.moveTo(kx, h - 1)
                    ctx.lineTo(kx, h - 1 - kh)
                    ctx.stroke()
                }
                break
            }

            // Taps on a timeline; feedback shortens each successive tap.
            case "delay": {
                baseline(0.25)
                ctx.globalAlpha = 0.85
                var taps = 4
                for (var d = 0; d < taps; ++d) {
                    var dx = 2 + (d / taps) * (w - 4) * (0.35 + 0.65 * a)
                    var dh = (h / 2 - 2) * Math.pow(0.62, d)
                    ctx.beginPath()
                    ctx.moveTo(dx, mid - dh)
                    ctx.lineTo(dx, mid + dh)
                    ctx.stroke()
                }
                break
            }

            // Phase/modulation sweep.
            case "phaser": {
                ctx.globalAlpha = 0.8
                ctx.beginPath()
                for (var px = 0; px <= w; px += 2) {
                    var pt = px / w
                    var py = mid - Math.sin(pt * Math.PI * 2.4) * (h * 0.34) * (0.35 + 0.65 * a)
                    if (px === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py)
                }
                ctx.stroke()
                ctx.globalAlpha = 0.3
                ctx.beginPath()
                for (var qx = 0; qx <= w; qx += 2) {
                    var qt = qx / w
                    var qy = mid - Math.sin(qt * Math.PI * 2.4 + 0.9) * (h * 0.24)
                    if (qx === 0) ctx.moveTo(qx, qy); else ctx.lineTo(qx, qy)
                }
                ctx.stroke()
                break
            }

            // Two rotors, fast above and slow below.
            case "rotary": {
                var cy1 = h * 0.32, cy2 = h * 0.72, r1 = h * 0.22, r2 = h * 0.17
                ctx.globalAlpha = 0.8
                ctx.beginPath(); ctx.ellipse(w * 0.5 - r1 * (1.6), cy1 - r1, r1 * 3.2, r1 * 2); ctx.stroke()
                ctx.globalAlpha = 0.45
                ctx.beginPath(); ctx.ellipse(w * 0.5 - r2 * 2.2, cy2 - r2, r2 * 4.4, r2 * 2); ctx.stroke()
                ctx.globalAlpha = 0.9
                ctx.beginPath(); ctx.moveTo(w * 0.5, cy1); ctx.lineTo(w * 0.5 + r1 * 2.4 * a, cy1); ctx.stroke()
                break
            }

            // Transfer curve with a knee; more amount, more compression.
            case "compressor": {
                ctx.globalAlpha = 0.22
                ctx.beginPath(); ctx.moveTo(1, h - 1); ctx.lineTo(w - 1, 1); ctx.stroke()
                ctx.globalAlpha = 0.9
                var knee = 0.35 + 0.3 * (1 - a)
                ctx.beginPath()
                ctx.moveTo(1, h - 1)
                ctx.lineTo(1 + (w - 2) * knee, (h - 1) - (h - 2) * knee)
                ctx.lineTo(w - 1, (h - 1) - (h - 2) * (knee + (1 - knee) * (1 - 0.65 * a)))
                ctx.stroke()
                break
            }

            // Clipped waveform: the shape of drive.
            case "drive": {
                var ceiling = (h * 0.42) * (1 - 0.55 * a)
                ctx.globalAlpha = 0.85
                ctx.beginPath()
                for (var sx = 0; sx <= w; sx += 2) {
                    var sv = Math.sin((sx / w) * Math.PI * 2) * (h * 0.42)
                    sv = Math.max(-ceiling, Math.min(ceiling, sv * (1 + 2.2 * a)))
                    var sy = mid - sv
                    if (sx === 0) ctx.moveTo(sx, sy); else ctx.lineTo(sx, sy)
                }
                ctx.stroke()
                break
            }

            // Band curve: two shelves and a peak.
            case "eq": {
                baseline(0.2)
                ctx.globalAlpha = 0.9
                ctx.beginPath()
                for (var ex = 0; ex <= w; ex += 2) {
                    var et = ex / w
                    var ey = mid
                        - Math.exp(-Math.pow((et - 0.25) / 0.16, 2)) * (h * 0.30) * (a * 2 - 1)
                        - Math.exp(-Math.pow((et - 0.72) / 0.16, 2)) * (h * 0.30) * (b * 2 - 1)
                    if (ex === 0) ctx.moveTo(ex, ey); else ctx.lineTo(ex, ey)
                }
                ctx.stroke()
                break
            }

            // Resonant sweep.
            case "filter": {
                ctx.globalAlpha = 0.9
                var cut = 0.15 + 0.7 * a
                ctx.beginPath()
                ctx.moveTo(1, mid - h * 0.2)
                ctx.lineTo(w * cut - h * 0.18, mid - h * 0.2)
                ctx.lineTo(w * cut, mid - h * 0.36)
                ctx.lineTo(w * cut + h * 0.3, h - 1)
                ctx.stroke()
                break
            }

            // Two pitches diverging.
            case "pitch": {
                ctx.globalAlpha = 0.8
                ctx.beginPath(); ctx.moveTo(1, mid); ctx.lineTo(w - 1, mid - (h * 0.34) * a); ctx.stroke()
                ctx.globalAlpha = 0.45
                ctx.beginPath(); ctx.moveTo(1, mid); ctx.lineTo(w - 1, mid + (h * 0.34) * a); ctx.stroke()
                break
            }

            // Comb notches.
            case "flanger": {
                ctx.globalAlpha = 0.8
                ctx.beginPath()
                for (var fx = 0; fx <= w; fx += 2) {
                    var ft = fx / w
                    var fy = mid - Math.abs(Math.cos(ft * Math.PI * (3 + 4 * a))) * (h * 0.34)
                    if (fx === 0) ctx.moveTo(fx, fy); else ctx.lineTo(fx, fy)
                }
                ctx.stroke()
                break
            }

            // The Tone(s) entering the chain: a source, not a processor.
            case "source": {
                ctx.globalAlpha = 0.85
                for (var g = 0; g < 3; ++g) {
                    var gy = mid + (g - 1) * (h * 0.26)
                    ctx.beginPath()
                    ctx.moveTo(2, gy)
                    ctx.lineTo(w - 6, gy)
                    ctx.stroke()
                }
                ctx.beginPath()
                ctx.moveTo(w - 9, mid - 4); ctx.lineTo(w - 2, mid); ctx.lineTo(w - 9, mid + 4)
                ctx.stroke()
                break
            }

            // Destination: level arriving at an output stage.
            case "output": {
                var bars = 7
                for (var o = 0; o < bars; ++o) {
                    var ot = o / (bars - 1)
                    var lit = ot <= a
                    ctx.globalAlpha = lit ? 0.9 : 0.18
                    var oh = (h - 4) * (0.35 + 0.65 * ot)
                    var ox = 2 + ot * (w - 6)
                    ctx.beginPath()
                    ctx.moveTo(ox, h - 2)
                    ctx.lineTo(ox, h - 2 - oh)
                    ctx.stroke()
                }
                break
            }

            default: {
                baseline(0.35)
                ctx.globalAlpha = 0.7
                ctx.beginPath()
                for (var gx = 0; gx <= w; gx += 3) {
                    var gyy = mid - Math.sin((gx / w) * Math.PI * 2) * (h * 0.22)
                    if (gx === 0) ctx.moveTo(gx, gyy); else ctx.lineTo(gx, gyy)
                }
                ctx.stroke()
                break
            }
            }
            ctx.globalAlpha = 1
        }
    }
}
