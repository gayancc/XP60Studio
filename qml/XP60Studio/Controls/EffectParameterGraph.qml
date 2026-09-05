import QtQuick
import QtQuick.Controls as QQC
import XP60Studio

// A parameter-space diagram, never sampled audio. Coordinates are normalized
// XP values. Unbound families show a dashed schematic, never a made-up Patch curve.
Rectangle {
    id: root
    required property var editor
    property string family: "modulation"
    property string xParameter: ""
    property string yParameter: ""
    property string secondaryParameter: ""
    property string feedbackParameter: ""
    property string dampingParameter: ""
    property string xTitle: ""
    property string yTitle: ""
    property string selectedParameter: ""
    property color accentColor: Theme.tone2
    property int tapCount: 2
    signal parameterSelected(string parameterId)
    readonly property var xv: editor.effectValues[xParameter] || ({})
    readonly property var yv: editor.effectValues[yParameter] || ({})
    readonly property var sv: editor.effectValues[secondaryParameter] || ({})
    readonly property var feedback: editor.effectValues[feedbackParameter] || ({})
    readonly property var damping: editor.effectValues[dampingParameter] || ({})
    readonly property bool bound: xParameter.length > 0 && xv.value !== undefined && yv.value !== undefined
    readonly property real nx: normalized(xv, false)
    readonly property real ny: normalized(yv, false)
    readonly property real ns: normalized(sv, false)
    readonly property real ax: normalized(xv, true)
    readonly property real ay: normalized(yv, true)
    implicitHeight: 200
    color: Theme.surfaceSunken; radius: Metrics.radiusSm
    border.color: Theme.borderSubtle
    function normalized(data, original) {
        return data.value === undefined ? 0 : ((original ? data.original : data.value) - data.minimum) / Math.max(1, data.maximum - data.minimum)
    }
    function adjust(x, y) {
        if (!bound || editor.comparing) return
        editor.editEffect(xParameter, Math.round(xv.minimum + Math.max(0, Math.min(1, x)) * (xv.maximum - xv.minimum)))
        editor.editEffect(yParameter, Math.round(yv.minimum + Math.max(0, Math.min(1, y)) * (yv.maximum - yv.minimum)))
    }
    onNxChanged: graph.requestPaint()
    onNyChanged: graph.requestPaint()
    onNsChanged: graph.requestPaint()
    onAxChanged: graph.requestPaint()
    onAyChanged: graph.requestPaint()
    onFamilyChanged: graph.requestPaint()
    onBoundChanged: graph.requestPaint()
    onFeedbackChanged: graph.requestPaint()
    onDampingChanged: graph.requestPaint()
    Canvas {
        id: graph
        anchors { fill: parent; margins: Metrics.spacingXl }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            var c = getContext("2d"); c.reset()
            c.strokeStyle = Theme.borderSubtle; c.lineWidth = 1
            for (var i = 0; i <= 8; ++i) {
                c.beginPath(); c.moveTo(i * width / 8, 0); c.lineTo(i * width / 8, height); c.stroke()
            }
            for (i = 0; i <= 4; ++i) {
                c.beginPath(); c.moveTo(0, i * height / 4); c.lineTo(width, i * height / 4); c.stroke()
            }
            function curve(a, b, ghost) {
                c.strokeStyle = ghost ? Theme.textMuted : root.accentColor
                c.lineWidth = ghost ? 1 : 2
                c.setLineDash(!root.bound || ghost ? [4, 5] : [])
                var f = root.family
                if (f === "rotary") {
                    for (var r = 0; r < 2; ++r) {
                        var cx = width * (r ? 0.7 : 0.3), cy = height / 2, radius = height * (r ? 0.27 : 0.38)
                        c.beginPath(); c.arc(cx, cy, radius, 0, 2 * Math.PI); c.stroke()
                        c.beginPath(); c.moveTo(cx - radius, cy); c.lineTo(cx + radius, cy); c.stroke()
                    }
                } else if (f === "delay" || f === "pan-delay") {
                    for (var tap = 1; tap <= root.tapCount; ++tap) {
                        var tx = width * (0.12 + 0.65 * a) * tap / root.tapCount
                        var amp = (0.15 + b * 0.8) * Math.pow(0.2 + root.ns * 0.75, tap - 1)
                        var base = f === "pan-delay" ? (tap % 2 ? height * 0.42 : height * 0.94) : height * 0.9
                        c.beginPath(); c.moveTo(tx, base); c.lineTo(tx, base - amp * height * (f === "pan-delay" ? 0.35 : 0.8)); c.stroke()
                        c.fillStyle = ghost ? Theme.textMuted : root.accentColor
                        c.font = "11px sans-serif"; c.fillText(f === "pan-delay" ? (tap % 2 ? "L" : "R") : String(tap), tx + 4, base)
                    }
                } else if (f === "pitch") {
                    for (i = 0; i < 3; ++i) {
                        c.beginPath(); c.moveTo(width * i * 0.08, height * (0.2 + i * 0.3)); c.lineTo(width, height * (0.2 + i * 0.3)); c.stroke()
                    }
                } else {
                    var lanes = f === "modulation" ? 2 : 1
                    for (var lane = 0; lane < lanes; ++lane) {
                        c.beginPath()
                        for (i = 0; i <= 180; ++i) {
                            var t = i / 180, y
                            if (f === "modulation") y = (lane ? 0.72 : 0.28) + Math.sin((t - root.ns * 0.2) * (2 + a * 6) * Math.PI + lane * Math.PI) * b * 0.2
                            else if (f === "reverb" || f === "gate") y = 0.9 - b * 0.85 * Math.exp(-t * (8 - 6 * a)) * (f === "gate" && t > 0.6 ? 0 : 1)
                            else if (f === "drive") y = 0.5 - Math.tanh((t - 0.5) * 7) * 0.4
                            else if (f === "compressor" || f === "limiter") y = 0.9 - Math.min(t, 0.5) - Math.max(0, t - 0.5) * 0.3
                            else if (f === "eq" || f === "spectrum" || f === "enhancer") y = 0.5 - Math.sin(t * Math.PI * (f === "spectrum" ? 8 : 2)) * 0.18
                            else y = 0.8 - Math.exp(-Math.pow((t - 0.5) * 4, 2)) * 0.6
                            if (i === 0) c.moveTo(0, height * y); else c.lineTo(t * width, height * y)
                        }
                        c.stroke()
                    }
                }
            }
            if (root.bound && (root.xv.modified || root.yv.modified)) curve(root.ax, root.ay, true)
            curve(root.bound ? root.nx : 0.5, root.bound ? root.ny : 0.6, false)
            if (root.bound && root.feedback.value !== undefined) {
                c.setLineDash([]); c.strokeStyle = root.accentColor
                c.globalAlpha = 0.25 + root.normalized(root.feedback, false) * 0.7
                c.lineWidth = 1 + root.normalized(root.feedback, false) * 2
                c.beginPath(); c.moveTo(width * 0.85, height * 0.8)
                c.lineTo(width * 0.85, height * 0.94); c.lineTo(width * 0.15, height * 0.94)
                c.lineTo(width * 0.15, height * 0.8); c.stroke()
            }
        }
    }
    XpLabel {
        anchors { left: parent.left; top: parent.top; margins: Metrics.spacingSm }
        role: "caption"; muted: true
        text: root.bound ? root.yTitle + " ↑" : qsTr("SCHEMATIC · parameter binding unavailable")
    }
    XpLabel {
        anchors { right: parent.right; top: parent.top; margins: Metrics.spacingSm }
        role: "caption"; secondary: true
        text: (root.feedback.value !== undefined ? qsTr("Feedback ↶ %1").arg(root.feedback.display) : "")
            + (root.damping.value !== undefined ? qsTr("  ·  HF Damp %1").arg(root.damping.display) : "")
    }
    XpLabel {
        anchors { right: parent.right; bottom: parent.bottom; margins: Metrics.spacingSm }
        role: "caption"; muted: true
        text: root.bound ? root.xTitle + " → · XP scale" : qsTr("Not the current Patch response")
    }
    Rectangle {
        id: point
        objectName: "effectGraphPoint"
        visible: root.bound
        x: Metrics.spacingXl + root.nx * (root.width - 2 * Metrics.spacingXl) - 7
        y: Metrics.spacingXl + (1 - root.ny) * (root.height - 2 * Metrics.spacingXl) - 7
        width: 14; height: 14; radius: 7
        color: pointMouse.pressed ? Theme.textPrimary : root.accentColor
        border.width: 2; border.color: activeFocus ? Theme.focusRing : Theme.surfaceSunken
        activeFocusOnTab: root.bound && !root.editor.comparing
        Accessible.role: Accessible.Slider
        Accessible.name: root.xTitle + ": " + root.xv.display + ", " + root.yTitle + ": " + root.yv.display
        Keys.onPressed: function(event) {
            if (root.editor.comparing) return
            if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
                root.editor.editEffect(root.xParameter, root.xv.value + (event.key === Qt.Key_Right ? 1 : -1)); event.accepted = true
            } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                root.editor.editEffect(root.yParameter, root.yv.value + (event.key === Qt.Key_Up ? 1 : -1)); event.accepted = true
            }
        }
        MouseArea {
            id: pointMouse
            anchors { fill: parent; margins: -7 }
            enabled: root.bound && !root.editor.comparing
            cursorShape: Qt.SizeAllCursor
            onPressed: { point.forceActiveFocus(); root.editor.beginEffectGesture(); root.parameterSelected(root.xParameter) }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                var p = mapToItem(root, mouse.x, mouse.y)
                root.adjust((p.x - Metrics.spacingXl) / (root.width - 2 * Metrics.spacingXl), 1 - (p.y - Metrics.spacingXl) / (root.height - 2 * Metrics.spacingXl))
            }
            onReleased: root.editor.endEffectGesture()
            onCanceled: root.editor.endEffectGesture()
        }
        QQC.ToolTip.visible: pointMouse.pressed
        QQC.ToolTip.text: root.xTitle + ": " + root.xv.display + " · " + root.yTitle + ": " + root.yv.display
    }
}

