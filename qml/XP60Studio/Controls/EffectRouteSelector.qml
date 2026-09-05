import QtQuick
import QtQuick.Layouts
import XP60Studio

// One output socket, a bounded palette of destinations, and a keyboard alternative.
// The presentation model supplies choices and validates the requested destination.
XpCard {
    id: root
    required property var editor
    required property string parameterId
    required property string title
    readonly property var parameter: editor.effectValues[parameterId] || ({})
    property bool dragging: false
    property int candidate: -1
    property point pointer: Qt.point(0, 0)
    implicitHeight: contents.implicitHeight + 2 * Metrics.cardPadding
    accentColor: Theme.accent
    activeFocusOnTab: true
    Keys.onEscapePressed: { dragging = false; candidate = -1 }
    ColumnLayout {
        id: contents
        anchors { left: parent.left; right: parent.right; top: parent.top }
        RowLayout {
            Layout.fillWidth: true
            XpLabel { text: root.title; role: "overline"; Layout.fillWidth: true }
            Rectangle {
                id: socket
                objectName: "routeSocket-" + root.parameterId
                implicitWidth: 32; implicitHeight: 32; radius: 16
                color: root.dragging ? Theme.accent : Theme.surfaceRaised
                border.color: Theme.accent; border.width: 2
                XpLabel { anchors.centerIn: parent; text: "↗" }
                MouseArea {
                    anchors.fill: parent; enabled: !root.editor.comparing
                    cursorShape: Qt.DragLinkCursor
                    onPressed: function(mouse) { root.forceActiveFocus(); root.pointer = mapToItem(root, mouse.x, mouse.y); root.dragging = true }
                    onPositionChanged: function(mouse) {
                        if (!root.dragging) return
                        root.pointer = mapToItem(root, mouse.x, mouse.y)
                        root.candidate = -1
                        for (var i = 0; i < targets.count; ++i) {
                            var target = targets.itemAt(i), p = mapToItem(target, mouse.x, mouse.y)
                            if (p.x >= 0 && p.x < target.width && p.y >= 0 && p.y < target.height) root.candidate = i
                        }
                    }
                    onReleased: {
                        if (root.dragging && root.candidate >= 0) root.editor.editEffect(root.parameterId, root.candidate)
                        root.dragging = false; root.candidate = -1
                    }
                    onCanceled: { root.dragging = false; root.candidate = -1 }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Repeater {
                id: targets
                model: root.parameter.choices || []
                XpButton {
                    required property string modelData
                    required property int index
                    objectName: "routeTarget-" + root.parameterId + "-" + index
                    Layout.fillWidth: true
                    text: (root.dragging && root.candidate === index ? "→ " : "") + modelData
                    variant: (root.dragging ? root.candidate === index : root.parameter.value === index) ? "primary" : "secondary"
                    enabled: !root.editor.comparing
                    onClicked: root.editor.editEffect(root.parameterId, index)
                    Accessible.name: root.title + qsTr(" to ") + modelData
                }
            }
        }
        XpLabel {
            Layout.fillWidth: true; role: "caption"; secondary: true; wrapMode: Text.WordWrap
            text: root.dragging ? root.candidate >= 0
                ? qsTr("Release: %1 → %2").arg(root.title).arg(root.parameter.choices[root.candidate])
                : qsTr("Drag to a destination below · elsewhere cancels · Esc cancels")
                : qsTr("Drag the socket or select a destination · %1").arg(root.parameter.display || "Unknown")
        }
    }
    Canvas {
        id: cable
        parent: root
        anchors.fill: parent
        visible: root.dragging
        onVisibleChanged: requestPaint()
        Connections { target: root; function onPointerChanged() { cable.requestPaint() } }
        onPaint: {
            var c = getContext("2d"); c.reset()
            var start = socket.mapToItem(root, socket.width / 2, socket.height / 2)
            var end = root.pointer
            if (root.candidate >= 0) {
                var target = targets.itemAt(root.candidate)
                end = target.mapToItem(root, target.width / 2, target.height / 2)
            }
            c.strokeStyle = root.candidate >= 0 ? Theme.accent : Theme.textMuted
            c.lineWidth = 2; c.setLineDash(root.candidate >= 0 ? [] : [4, 4])
            c.beginPath(); c.moveTo(start.x, start.y); c.bezierCurveTo(start.x, end.y, end.x, start.y, end.x, end.y); c.stroke()
        }
    }
}

