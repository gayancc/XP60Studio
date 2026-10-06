import QtQuick
import QtQuick.Templates as T
import XP60Studio

// Rotary control for a continuous synth parameter.
//
// Drag vertically to change, Shift for fine steps, arrows/PageUp/PageDown from
// the keyboard, double-click to reset. Every knob also has an exact numeric
// readout beside it (see ParameterValueEditor) so no value is knob-only.
T.Dial {
    id: control

    // Draws the arc from the centre rather than from the minimum, for
    // parameters whose neutral value is the middle (pan, tuning, depths).
    property bool bipolar: false
    property color accentColor: Theme.accent
    property int defaultValue: bipolar ? Math.round((from + to) / 2) : from
    property string valueText: ""

    // How far the pointer travels to sweep the whole range. T.Dial's own
    // Vertical input maps the range onto the control's own height, which on a
    // 44 px knob is about three XP steps per pixel — a value a musician cannot
    // aim at. The drag is handled below over this fixed distance instead:
    // 220 px across 0-127 is ~1.7 px per step, so every documented value is
    // reachable and a full sweep is still one comfortable gesture.
    property int dragPixelsForFullRange: 220
    // Shift slows the same gesture to ~7 px per step for single-value work.
    property real fineDivisor: 4
    // True while the pointer is turning this knob. T.Dial's own `pressed` is
    // read-only and its pointer handling is replaced below, so this is the
    // signal call sites bracket an undo gesture on.
    readonly property bool dragging: dragArea.pressed

    // A second arc showing how far a modulator moves this parameter, drawn
    // outside the value ring in the modulator's own colour. Set `modDepth` to
    // the signed depth as a fraction of the full range (-1..1); leave it at 0
    // and nothing is drawn. Only ever driven by a documented XP-60 routing —
    // there is no free modulation matrix on this instrument to invent one from.
    property real modDepth: 0
    property color modColor: Theme.tone2

    implicitWidth: 44
    implicitHeight: 44
    stepSize: 1
    wrap: false
    hoverEnabled: true
    inputMode: T.Dial.Vertical

    Accessible.role: Accessible.Dial
    Accessible.name: valueText.length > 0 ? valueText : String(value)

    readonly property real arcStartAngle: -140
    readonly property real sweep: 280
    readonly property real normalised: control.to > control.from
                                       ? (control.value - control.from) / (control.to - control.from) : 0

    // Moving the dial must never assign `value` from JavaScript. Call sites
    // bind it to their model, and a JS write deletes that binding for good —
    // undo, A/B and Compare would stop reaching the knob ever after. The
    // control's own increase()/decrease() change the value through C++, which
    // leaves the binding in place, so the model stays authoritative.
    function stepTo(target) {
        const wanted = Math.max(control.from, Math.min(control.to, Math.round(target)))
        // The range bounds the loop even if a caller leaves stepSize at 0.
        let guard = Math.abs(control.to - control.from) + 2
        while (control.value < wanted && guard-- > 0)
            control.increase()
        while (control.value > wanted && guard-- > 0)
            control.decrease()
        return control.value
    }

    // Reports whether anything actually moved, so callers only emit moved()
    // for a real change.
    function applyValue(target) {
        const before = control.value
        return control.stepTo(target) !== before
    }

    Keys.onPressed: function(event) {
        var step = (event.modifiers & Qt.ShiftModifier) ? 1 : Math.max(1, Math.round((to - from) / 32))
        var target = value
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Right) {
            target = value + step; event.accepted = true
        } else if (event.key === Qt.Key_Down || event.key === Qt.Key_Left) {
            target = value - step; event.accepted = true
        } else if (event.key === Qt.Key_PageUp) {
            target = value + step * 4; event.accepted = true
        } else if (event.key === Qt.Key_PageDown) {
            target = value - step * 4; event.accepted = true
        } else if (event.key === Qt.Key_Home) {
            target = defaultValue; event.accepted = true
        }
        if (event.accepted && control.applyValue(target))
            control.moved()
    }

    background: Item {
        implicitWidth: control.implicitWidth
        implicitHeight: control.implicitHeight

        Canvas {
            id: track
            anchors.fill: parent
            // Repaint whenever anything it draws changes.
            readonly property real v: control.normalised
            readonly property color arc: !control.enabled ? Theme.textDisabled
                                       : control.dragging ? Qt.lighter(control.accentColor, 1.25)
                                       : control.hovered ? Qt.lighter(control.accentColor, 1.12)
                                       : control.accentColor
            readonly property bool focused: control.visualFocus
            readonly property bool bip: control.bipolar
            readonly property real mod: control.modDepth
            readonly property color modTint: control.modColor
            onVChanged: requestPaint()
            onArcChanged: requestPaint()
            onFocusedChanged: requestPaint()
            onBipChanged: requestPaint()
            onModChanged: requestPaint()
            onModTintChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            Component.onCompleted: requestPaint()

            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                var cx = width / 2
                var cy = height / 2
                var r = Math.min(width, height) / 2 - 3
                var a0 = (control.arcStartAngle - 90) * Math.PI / 180
                var a1 = (control.arcStartAngle + control.sweep - 90) * Math.PI / 180
                // The ring scales with the knob so a 28 px dial and a 54 px one
                // read as the same control at two sizes, not two controls.
                var ring = Math.max(2, Math.round(Math.min(width, height) * 0.075))

                ctx.lineCap = "butt"
                ctx.lineWidth = ring
                ctx.strokeStyle = Theme.borderStrong
                ctx.beginPath(); ctx.arc(cx, cy, r, a0, a1); ctx.stroke()

                // End stops and, on a bipolar knob, the musical zero. Reading a
                // value off an arc needs something to read it against.
                ctx.lineWidth = 1
                ctx.strokeStyle = Theme.textDisabled
                function tick(angle, inner) {
                    ctx.beginPath()
                    ctx.moveTo(cx + Math.cos(angle) * (r - ring / 2 - 1),
                               cy + Math.sin(angle) * (r - ring / 2 - 1))
                    ctx.lineTo(cx + Math.cos(angle) * (r - ring / 2 - 1 - inner),
                               cy + Math.sin(angle) * (r - ring / 2 - 1 - inner))
                    ctx.stroke()
                }
                var centreAngle = (a0 + a1) / 2
                if (control.bipolar) {
                    ctx.strokeStyle = track.arc
                    tick(centreAngle, Math.max(2, ring))
                }

                var mid = control.bipolar ? centreAngle : a0
                var cur = a0 + (a1 - a0) * control.normalised
                ctx.lineWidth = ring
                ctx.lineCap = "round"
                ctx.strokeStyle = track.arc
                if (Math.abs(cur - mid) > 0.001) {
                    ctx.beginPath()
                    ctx.arc(cx, cy, r, Math.min(mid, cur), Math.max(mid, cur))
                    ctx.stroke()
                }

                // Modulation reach: from where the parameter sits now to where
                // the modulator can push it, on its own ring outside the value.
                if (Math.abs(control.modDepth) > 0.001) {
                    var mr = r + ring / 2 + 2.5
                    var span = (a1 - a0) * control.modDepth
                    var to = Math.max(a0, Math.min(a1, cur + span))
                    ctx.lineWidth = Math.max(1.5, ring - 1)
                    ctx.lineCap = "butt"
                    ctx.strokeStyle = Qt.rgba(control.modColor.r, control.modColor.g,
                                              control.modColor.b, 0.85)
                    ctx.beginPath()
                    ctx.arc(cx, cy, mr, Math.min(cur, to), Math.max(cur, to))
                    ctx.stroke()
                }

                // Focus ring, drawn outside the value arc rather than as a box
                // around the cell, so a focused knob still reads as a knob.
                if (control.visualFocus) {
                    ctx.lineWidth = 1
                    ctx.strokeStyle = Theme.focusRing
                    ctx.beginPath(); ctx.arc(cx, cy, r + ring / 2 + 5, a0, a1); ctx.stroke()
                }
            }
        }

        // Body
        Rectangle {
            anchors.centerIn: parent
            width: Math.round(Math.min(parent.width, parent.height) * 0.72)
            height: width
            radius: width / 2
            color: !control.enabled ? Theme.surface
                 : control.dragging ? Theme.surfacePressed
                 : control.hovered ? Theme.surfaceHover : Theme.surfaceRaised
            border.width: 1
            border.color: control.dragging ? control.accentColor
                        : control.hovered ? Theme.borderStrong : Theme.border
            Behavior on color {
                enabled: !Motion.reducedMotion
                ColorAnimation { duration: Motion.durationFast }
            }
        }

        // Pointer
        Rectangle {
            id: pointer
            readonly property real inset: Math.round(Math.min(parent.width, parent.height) * 0.17)
            width: Math.max(2, Math.round(Math.min(parent.width, parent.height) * 0.055))
            height: Math.min(parent.width, parent.height) / 2 - inset - 2
            radius: width / 2
            color: !control.enabled ? Theme.textDisabled
                 : control.dragging ? Theme.textPrimary : control.accentColor
            antialiasing: true
            x: parent.width / 2 - width / 2
            y: pointer.inset
            transform: Rotation {
                origin.x: pointer.width / 2
                origin.y: Math.min(control.width, control.height) / 2 - pointer.inset
                angle: control.arcStartAngle + control.sweep * control.normalised
            }
        }
    }

    handle: Item {}

    MouseArea {
        id: dragArea
        anchors.fill: parent
        enabled: control.enabled
        acceptedButtons: Qt.LeftButton
        // Knobs live inside the Editor's Flickable. Without this a vertical
        // turn is stolen by the page and scrolls it instead.
        preventStealing: true

        // The gesture is measured from where it started, not accumulated per
        // event, so a slow drag cannot drift and rounding never compounds.
        property real anchorY: 0
        property int anchorValue: 0
        property bool fine: false

        function anchorAt(y, shift) {
            anchorY = y
            anchorValue = control.value
            fine = shift
        }

        onPressed: function(mouse) {
            control.forceActiveFocus(Qt.MouseFocusReason)
            anchorAt(mouse.y, (mouse.modifiers & Qt.ShiftModifier) !== 0)
        }

        onPositionChanged: function(mouse) {
            if (!dragArea.pressed)
                return
            const shift = (mouse.modifiers & Qt.ShiftModifier) !== 0
            // Taking Shift mid-gesture re-anchors, so the knob does not jump.
            if (shift !== dragArea.fine)
                dragArea.anchorAt(mouse.y, shift)
            const span = control.to - control.from
            const perPixel = span / Math.max(1, control.dragPixelsForFullRange)
                             / (dragArea.fine ? Math.max(1, control.fineDivisor) : 1)
            if (control.applyValue(dragArea.anchorValue + (dragArea.anchorY - mouse.y) * perPixel))
                control.moved()
        }

        onDoubleClicked: {
            if (control.applyValue(control.defaultValue))
                control.moved()
            dragArea.anchorAt(dragArea.anchorY, dragArea.fine)
        }
    }
}
