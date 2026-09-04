import QtQuick
import XP60Studio

// A piano keybed showing a Patch's key range.
//
// Drawn rather than photographed: the strip has to survive any width, both
// themes and the Tone accent colours, and stay crisp on a HiDPI display, which
// a bitmap of a keyboard cannot. The geometry is a real keyboard's, not equal
// slices — seven white keys per octave with the five black keys at their true
// widths and offsets, so the octave pattern is readable at a glance.
//
// Notes are MIDI 0..127 (C-1..G9), the range Roland's Key Range parameters
// use. `firstNote`/`lastNote` choose which part of that is worth drawing; the
// range itself is never limited by the window.
Item {
    id: root

    property int lowerNote: 0
    property int upperNote: 127
    property int firstNote: 36 // C2
    property int lastNote: 96  // C7
    property color accentColor: Theme.accent
    // Velocity window of the same Tone. A range that only responds to hard
    // playing is drawn faintly, so the two controls read as one setting.
    property int velocityLower: 1
    property int velocityUpper: 127
    property bool interactive: true
    property bool showOctaveLabels: true

    signal rangeEdited(int lower, int upper)

    implicitHeight: 78
    // Height of the range bar drawn under the keys.
    readonly property int barHeight: 7

    // ── Geometry ────────────────────────────────────────────────────────────

    function isBlack(note) {
        var p = ((note % 12) + 12) % 12
        return p === 1 || p === 3 || p === 6 || p === 8 || p === 10
    }

    // Where each black key sits, in white-key widths from the start of its
    // octave. The middle black of a group is centred on the boundary between
    // its neighbours; the outer ones shift outward, as on a real keyboard.
    readonly property var blackCentre: ({ 1: 0.90, 3: 2.10, 6: 3.85, 8: 5.00, 10: 6.15 })

    readonly property int windowFirst: Math.max(0, Math.min(root.firstNote, root.lastNote))
    readonly property int windowLast: Math.min(127, Math.max(root.firstNote, root.lastNote))
    readonly property int noteCount: windowLast - windowFirst + 1

    readonly property int whiteCount: {
        var n = 0
        for (var i = windowFirst; i <= windowLast; ++i)
            if (!isBlack(i)) n++
        return Math.max(1, n)
    }
    readonly property real whiteWidth: keyArea.width / whiteCount
    readonly property real blackWidth: Math.max(2, whiteWidth * 0.60)

    // Number of white keys before `note` within the window.
    function whiteIndex(note) {
        var n = 0
        for (var i = windowFirst; i < note; ++i)
            if (!isBlack(i)) n++
        return n
    }

    // Left edge of a key, in pixels inside keyArea.
    //
    // The widths are arguments rather than reads of `whiteWidth`/`blackWidth`
    // inside the body: a binding that calls a function only re-evaluates when
    // something it mentions changes, so passing them in is what makes the
    // handles and labels follow a resize.
    function keyLeft(note, ww, bw) {
        if (!isBlack(note))
            return whiteIndex(note) * ww
        var octaveStart = Math.floor(note / 12) * 12
        var whitesBefore = whiteIndex(Math.max(octaveStart, windowFirst))
        var offset = blackCentre[((note % 12) + 12) % 12]
        // When the window starts mid-octave the octave's own left edge is off
        // screen; measure from where C would have been.
        if (octaveStart < windowFirst) {
            var missing = 0
            for (var i = octaveStart; i < windowFirst; ++i)
                if (!isBlack(i)) missing++
            whitesBefore -= missing
        }
        return (whitesBefore + offset) * ww - bw / 2
    }

    // Right edge, accounting for the key's own width.
    function keyRight(note, ww, bw) {
        return keyLeft(note, ww, bw) + (isBlack(note) ? bw : ww)
    }

    // The note under a pixel position: black keys win where they overlap.
    function noteAt(x, y) {
        for (var n = windowFirst; n <= windowLast; ++n) {
            if (!isBlack(n)) continue
            var left = keyLeft(n, whiteWidth, blackWidth)
            if (x >= left && x <= left + blackWidth && y <= keyArea.height * 0.62)
                return n
        }
        var index = Math.floor(x / Math.max(1, whiteWidth))
        var count = 0
        for (n = windowFirst; n <= windowLast; ++n) {
            if (isBlack(n)) continue
            if (count === index) return n
            count++
        }
        return windowLast
    }

    // ── Animated state ──────────────────────────────────────────────────────
    // The highlight glides to a new range instead of jumping, so a drag or an
    // undo reads as a change rather than a redraw.

    property real animLower: lowerNote
    property real animUpper: upperNote
    Behavior on animLower { NumberAnimation { duration: Motion.durationFast; easing.type: Motion.easingStandard } }
    Behavior on animUpper { NumberAnimation { duration: Motion.durationFast; easing.type: Motion.easingStandard } }
    onLowerNoteChanged: animLower = lowerNote
    onUpperNoteChanged: animUpper = upperNote

    // Velocity window as a 0..1 emphasis: a narrow or high-only window reads
    // as a quieter highlight.
    readonly property real velocityEmphasis: {
        var span = Math.max(0, Math.min(127, velocityUpper) - Math.max(1, velocityLower)) / 126
        return 0.35 + 0.65 * span
    }
    property real animEmphasis: velocityEmphasis
    Behavior on animEmphasis { NumberAnimation { duration: Motion.durationNormal; easing.type: Motion.easingStandard } }
    onVelocityEmphasisChanged: animEmphasis = velocityEmphasis

    property int hoverNote: -1
    property int grabbedEdge: -1 // 0 lower, 1 upper

    Rectangle {
        anchors.fill: parent
        radius: Metrics.radiusSm
        color: Theme.surfaceSunken
        border.width: 1
        border.color: Theme.borderSubtle
        clip: true

        Item {
            id: keyArea
            anchors { left: parent.left; right: parent.right; top: parent.top }
            anchors.margins: 1
            height: parent.height - root.barHeight - 3
                    - (root.showOctaveLabels && root.whiteWidth >= 9 ? 14 : 0)

            Canvas {
                id: keys
                anchors.fill: parent
                renderStrategy: Canvas.Cooperative

                // Everything the drawing depends on, so a change repaints once.
                readonly property real lo: root.animLower
                readonly property real hi: root.animUpper
                readonly property real emphasis: root.animEmphasis
                readonly property color accent: root.accentColor
                readonly property int hover: root.hoverNote
                readonly property int first: root.windowFirst
                readonly property int last: root.windowLast
                onLoChanged: requestPaint()
                onHiChanged: requestPaint()
                onEmphasisChanged: requestPaint()
                onAccentChanged: requestPaint()
                onHoverChanged: requestPaint()
                onFirstChanged: requestPaint()
                onLastChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                Component.onCompleted: requestPaint()

                function roundedBottom(ctx, x, y, w, h, r) {
                    r = Math.min(r, w / 2, h / 2)
                    ctx.beginPath()
                    ctx.moveTo(x, y)
                    ctx.lineTo(x + w, y)
                    ctx.lineTo(x + w, y + h - r)
                    ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h)
                    ctx.lineTo(x + r, y + h)
                    ctx.quadraticCurveTo(x, y + h, x, y + h - r)
                    ctx.closePath()
                }

                function tint(base, amount) {
                    return Qt.rgba(base.r + (accent.r - base.r) * amount,
                                   base.g + (accent.g - base.g) * amount,
                                   base.b + (accent.b - base.b) * amount, 1)
                }

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    var H = height
                    var ww = root.whiteWidth
                    var bw = root.blackWidth
                    var bh = H * 0.62
                    var radius = Math.min(3, ww * 0.3)
                    var n, left, inRange, grad

                    // White keys.
                    for (n = first; n <= last; ++n) {
                        if (root.isBlack(n)) continue
                        left = root.keyLeft(n, ww, bw)
                        inRange = n >= Math.round(lo) && n <= Math.round(hi)
                        var lift = n === hover ? 0.10 : 0

                        var top = Qt.rgba(0.914, 0.929, 0.953, 1)   // #E9EDF3
                        var mid = Qt.rgba(0.835, 0.859, 0.894, 1)   // #D5DBE4
                        var bot = Qt.rgba(0.725, 0.761, 0.812, 1)   // #B9C2CF
                        // A wash, not a repaint: at full range a heavy tint
                        // turns the keybed into a coloured block and stops
                        // reading as keys. The range bar below carries the
                        // reading; the keys only echo it.
                        if (inRange) {
                            top = tint(top, 0.08 * emphasis)
                            mid = tint(mid, 0.12 * emphasis)
                            bot = tint(bot, 0.22 * emphasis)
                        }
                        if (lift > 0) {
                            top = Qt.lighter(top, 1 + lift)
                            mid = Qt.lighter(mid, 1 + lift)
                        }

                        grad = ctx.createLinearGradient(0, 0, 0, H)
                        grad.addColorStop(0, top)
                        grad.addColorStop(0.72, mid)
                        grad.addColorStop(1, bot)
                        ctx.fillStyle = grad
                        roundedBottom(ctx, left, 0, ww, H, radius)
                        ctx.fill()

                        // Separator between keys, and the shadow line along the front.
                        ctx.strokeStyle = Qt.rgba(0, 0, 0, 0.42)
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(left + ww - 0.5, 0)
                        ctx.lineTo(left + ww - 0.5, H - radius)
                        ctx.stroke()
                    }

                    // Black keys, over the whites, each with its own shadow.
                    for (n = first; n <= last; ++n) {
                        if (!root.isBlack(n)) continue
                        left = root.keyLeft(n, ww, bw)
                        inRange = n >= Math.round(lo) && n <= Math.round(hi)

                        ctx.fillStyle = Qt.rgba(0, 0, 0, 0.30)
                        roundedBottom(ctx, left + 1, 0, bw, bh + 2, radius)
                        ctx.fill()

                        var btop = Qt.rgba(0.165, 0.192, 0.251, 1)  // #2A3140
                        var bbot = Qt.rgba(0.020, 0.027, 0.043, 1)  // #05070B
                        if (inRange) {
                            btop = tint(btop, 0.20 * emphasis)
                            bbot = tint(bbot, 0.08 * emphasis)
                        }
                        if (n === hover) {
                            btop = Qt.lighter(btop, 1.35)
                        }
                        grad = ctx.createLinearGradient(0, 0, 0, bh)
                        grad.addColorStop(0, btop)
                        grad.addColorStop(0.62, Qt.rgba((btop.r + bbot.r) / 2, (btop.g + bbot.g) / 2,
                                                        (btop.b + bbot.b) / 2, 1))
                        grad.addColorStop(1, bbot)
                        ctx.fillStyle = grad
                        roundedBottom(ctx, left, 0, bw, bh, radius)
                        ctx.fill()

                        // Gloss along the top edge.
                        ctx.fillStyle = Qt.rgba(1, 1, 1, 0.16)
                        ctx.fillRect(left + 1, 1, bw - 2, Math.max(1, bh * 0.06))
                    }

                    // Glow along the front of the sounding keys: the range is
                    // legible even where the tint is subtle.
                    if (hi >= first && lo <= last) {
                        var gl = root.keyLeft(Math.max(first, Math.round(lo)), ww, bw)
                        var hiNote = Math.min(last, Math.round(hi))
                        var gr = root.keyRight(hiNote, ww, bw)
                        var glow = ctx.createLinearGradient(0, H - H * 0.22, 0, H)
                        glow.addColorStop(0, Qt.rgba(accent.r, accent.g, accent.b, 0))
                        glow.addColorStop(1, Qt.rgba(accent.r, accent.g, accent.b, 0.60 * emphasis))
                        ctx.fillStyle = glow
                        ctx.fillRect(gl, H - H * 0.22, Math.max(1, gr - gl), H * 0.22)

                        // And a bright line across the top of the span, so the
                        // two edges are exact even where the wash is subtle.
                        ctx.fillStyle = Qt.rgba(accent.r, accent.g, accent.b, 0.85 * emphasis)
                        ctx.fillRect(gl, 0, Math.max(1, gr - gl), 2)
                    }
                }
            }

            // Range edge handles, drawn over the keys so an edge can be grabbed
            // where it actually is.
            Repeater {
                model: 2
                delegate: Rectangle {
                    required property int index
                    readonly property int note: index === 0 ? Math.round(root.animLower) : Math.round(root.animUpper)
                    visible: root.interactive && note >= root.windowFirst && note <= root.windowLast
                    x: (index === 0 ? root.keyLeft(note, root.whiteWidth, root.blackWidth)
                                    : root.keyRight(note, root.whiteWidth, root.blackWidth)) - width / 2
                    width: root.grabbedEdge === index ? 5 : 3
                    height: parent.height
                    radius: width / 2
                    color: root.accentColor
                    opacity: root.grabbedEdge === index ? 1 : 0.85
                    Behavior on width { NumberAnimation { duration: Motion.durationFast } }
                    Behavior on opacity { NumberAnimation { duration: Motion.durationFast } }
                }
            }
        }

        // The range as a bar, the way the mockup reads it: exact edges that
        // stay legible however many keys the window holds.
        Rectangle {
            id: rangeBar
            anchors { left: keyArea.left; right: keyArea.right; top: keyArea.bottom; topMargin: 3 }
            height: root.barHeight
            radius: height / 2
            color: Theme.surface

            Rectangle {
                readonly property real lo: root.keyLeft(Math.max(root.windowFirst, Math.round(root.animLower)),
                                                        root.whiteWidth, root.blackWidth)
                readonly property int hiNote: Math.min(root.windowLast, Math.round(root.animUpper))
                x: lo
                width: Math.max(height, root.keyRight(hiNote, root.whiteWidth, root.blackWidth) - lo)
                height: parent.height
                radius: height / 2
                color: root.accentColor
                opacity: 0.35 + 0.65 * root.animEmphasis
            }
        }

        // Octave marks, so a key is identifiable without counting.
        Item {
            anchors { left: keyArea.left; right: keyArea.right; top: rangeBar.bottom }
            height: 14
            visible: root.showOctaveLabels && root.whiteWidth >= 9

            Repeater {
                model: root.noteCount
                delegate: XpLabel {
                    required property int index
                    readonly property int note: root.windowFirst + index
                    visible: note % 12 === 0
                    // Centred on the C key but free to be wider than it —
                    // constraining the label to one key width would elide it
                    // away, and octave marks are 7 white keys apart anyway.
                    width: 28
                    // Kept inside the strip so the first and last octave marks
                    // are readable rather than half cut off.
                    x: Math.max(0, Math.min(parent.width - width,
                                            root.keyLeft(note, root.whiteWidth, root.blackWidth)
                                            + root.whiteWidth / 2 - width / 2))
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideNone
                    text: "C" + (Math.floor(note / 12) - 1)
                    role: "caption"
                    muted: true
                }
            }
        }
    }

    // ── Interaction ─────────────────────────────────────────────────────────

    HoverHandler {
        enabled: root.interactive
        onPointChanged: root.hoverNote = active ? root.noteAt(point.position.x - 1, point.position.y - 1) : -1
        onActiveChanged: if (!active) root.hoverNote = -1
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.interactive
        cursorShape: root.interactive ? Qt.SizeHorCursor : Qt.ArrowCursor
        onPressed: function(mouse) {
            root.forceActiveFocus()
            var n = root.noteAt(mouse.x - 1, mouse.y - 1)
            // Grab whichever edge is nearer, so a click always means something.
            root.grabbedEdge = Math.abs(n - root.lowerNote) <= Math.abs(n - root.upperNote) ? 0 : 1
            apply(n)
        }
        onPositionChanged: function(mouse) { if (pressed) apply(root.noteAt(mouse.x - 1, mouse.y - 1)) }
        onReleased: root.grabbedEdge = -1
        function apply(n) {
            if (root.grabbedEdge === 0)
                root.rangeEdited(Math.min(n, root.upperNote), root.upperNote)
            else
                root.rangeEdited(root.lowerNote, Math.max(n, root.lowerNote))
        }
    }

    activeFocusOnTab: root.interactive
    Keys.onPressed: function(event) {
        var edge = root.grabbedEdge < 0 ? 0 : root.grabbedEdge
        var step = (event.modifiers & Qt.ShiftModifier) ? 12 : 1
        if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
            var delta = event.key === Qt.Key_Right ? step : -step
            if (edge === 0)
                root.rangeEdited(Math.max(0, Math.min(root.lowerNote + delta, root.upperNote)), root.upperNote)
            else
                root.rangeEdited(root.lowerNote, Math.min(127, Math.max(root.upperNote + delta, root.lowerNote)))
            event.accepted = true
        } else if (event.key === Qt.Key_Tab && !(event.modifiers & Qt.ShiftModifier) && root.grabbedEdge === 0) {
            root.grabbedEdge = 1 // move to the upper edge before leaving the control
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
        border.width: root.activeFocus ? 2 : 0
        border.color: Theme.focusRing
        visible: root.activeFocus
    }

    Accessible.role: Accessible.Slider
    Accessible.name: qsTr("Key range")
    Accessible.description: qsTr("Notes %1 to %2. Space switches edge, arrows move it, Shift for whole octaves.")
                            .arg(root.lowerNote).arg(root.upperNote)
}
