import QtQuick
import QtQuick.Layouts
import XP60Studio

// A send on the canvas, as a thing rather than a number.
//
// The old canvas floated bare values like "127" and "96" beside cables, which
// forced the musician to work out what each one governed. A chip says what it
// is — CHORUS SEND 127 — and is the control for it: drag to adjust, click to
// isolate the route, double-click for exact entry.
//
// Value semantics stay with the C++ model. The chip only ever asks the editor
// to set a raw value inside the range the model published, so XP resolution is
// preserved exactly and nothing is rescaled for the sake of the gesture.
Item {
    id: root

    property var editor
    property string parameterId: ""
    // Short semantic name, e.g. "CHORUS SEND". Never just a number.
    property string caption: ""
    property color tint: Theme.accent
    property bool isolated: false
    property bool dimmed: false
    // Zero send or inactive upstream: the chip stays legible but recedes.
    property bool open: true

    signal isolateRequested()
    signal exactEntryRequested()

    readonly property var info: (editor && parameterId) ? (editor.effectValues[parameterId] || null) : null
    readonly property int value: info ? info.value : 0
    readonly property int minimum: info ? info.minimum : 0
    readonly property int maximum: info ? info.maximum : 127
    readonly property bool modified: info ? info.modified : false
    readonly property string displayValue: info ? info.display : ""
    readonly property bool editable: info !== null && editor && !editor.comparing

    implicitWidth: layout.implicitWidth + 2 * Metrics.spacingSm
    implicitHeight: Math.max(Metrics.controlHeightXs, layout.implicitHeight + 4)

    opacity: root.dimmed ? 0.22 : (root.open ? 1.0 : 0.55)
    Behavior on opacity {
        enabled: !Motion.reducedMotion
        NumberAnimation { duration: Motion.durationNormal; easing.type: Motion.easingStandard }
    }

    Accessible.role: Accessible.Slider
    Accessible.name: root.caption
    Accessible.description: root.info ? root.info.label : ""
    Accessible.focusable: root.editable
    activeFocusOnTab: root.editable
    Keys.onLeftPressed: root.nudge(-1)
    Keys.onRightPressed: root.nudge(1)
    Keys.onUpPressed: root.nudge(1)
    Keys.onDownPressed: root.nudge(-1)
    Keys.onReturnPressed: root.isolateRequested()

    function nudge(delta) {
        if (!root.editable)
            return
        root.editor.beginEffectGesture()
        root.editor.editEffect(root.parameterId, Math.max(root.minimum, Math.min(root.maximum, root.value + delta)))
        root.editor.endEffectGesture()
    }

    Rectangle {
        id: body
        anchors.fill: parent
        radius: Metrics.radiusPill
        color: root.isolated ? Theme.surfaceRaised : Theme.surface
        border.width: root.isolated || drag.active ? 2 : 1
        border.color: root.isolated || drag.active ? root.tint
                     : root.open ? Theme.borderStrong : Theme.borderSubtle

        RowLayout {
            id: layout
            anchors.centerIn: parent
            spacing: Metrics.spacingXs

            Rectangle {
                width: 5; height: 5; radius: 2.5
                color: root.tint
                opacity: root.open ? 1 : 0.4
                Layout.alignment: Qt.AlignVCenter
            }
            XpLabel {
                text: root.caption
                role: "caption"
                color: root.open ? Theme.textSecondary : Theme.textMuted
            }
            XpLabel {
                objectName: "chipValue"
                text: root.displayValue
                role: "mono"
                font.weight: Typography.weightMedium
                color: root.modified ? Theme.localEdit : (root.open ? Theme.textPrimary : Theme.textMuted)
            }
        }
    }

    // Drag to adjust. Vertical, because these sit on horizontal rails and a
    // horizontal drag would fight the canvas.
    MouseArea {
        id: drag
        anchors.fill: parent
        enabled: root.editable
        cursorShape: Qt.SizeVerCursor
        property bool active: false
        property real anchorY: 0
        property int anchorValue: 0

        onPressed: function(mouse) {
            root.forceActiveFocus()
            active = true
            anchorY = mouse.y
            anchorValue = root.value
            root.editor.beginEffectGesture()
        }
        onPositionChanged: function(mouse) {
            if (!active)
                return
            // Whole range over ~180 px, so a full sweep is one comfortable
            // gesture while every raw step remains reachable.
            var span = root.maximum - root.minimum
            var delta = Math.round(((anchorY - mouse.y) / 180) * span)
            var next = Math.max(root.minimum, Math.min(root.maximum, anchorValue + delta))
            if (next !== root.value)
                root.editor.editEffect(root.parameterId, next)
        }
        onReleased: {
            if (!active)
                return
            active = false
            // One gesture is one undo entry, however many values it passed
            // through on the way.
            root.editor.endEffectGesture()
        }
        onCanceled: {
            active = false
            root.editor.endEffectGesture()
        }
        onClicked: if (!active) root.isolateRequested()
        onDoubleClicked: root.exactEntryRequested()
    }

    // Value bubble while dragging: the number the musician is actually setting,
    // large enough to read without looking away from the canvas.
    Rectangle {
        visible: drag.active
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.top
        anchors.bottomMargin: Metrics.spacingXs
        width: bubble.implicitWidth + 2 * Metrics.spacingSm
        height: bubble.implicitHeight + Metrics.spacingXs
        radius: Metrics.radiusSm
        color: Theme.surfaceRaised
        border.width: 1
        border.color: root.tint
        z: 50
        ColumnLayout {
            id: bubble
            anchors.centerIn: parent
            spacing: 0
            XpLabel {
                text: root.info ? root.info.label : ""
                role: "caption"
                secondary: true
                Layout.alignment: Qt.AlignHCenter
            }
            XpLabel {
                text: root.displayValue
                role: "mono"
                font.weight: Typography.weightMedium
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: Metrics.radiusPill
        color: "transparent"
        border.width: 2
        border.color: Theme.focusRing
        visible: root.activeFocus && !drag.active
    }
}
