import QtQuick
import QtQuick.Layouts
import XP60Studio

// Visual LFO waveform selector using documented XP-60 shape labels.
Item {
    id: root

    property var shapes: ["TRI", "SIN", "SAW", "SQR", "TRP", "S&H", "RND", "CHS"]
    property int currentRaw: 0
    property int minimum: 0
    property color accentColor: Theme.accent
    property bool editable: true
    signal shapeSelected(int raw)

    implicitHeight: flow.implicitHeight
    implicitWidth: flow.implicitWidth

    Accessible.role: Accessible.List
    Accessible.name: qsTr("LFO waveform")

    Flow {
        id: flow
        width: parent.width
        spacing: Metrics.spacingXs
        Repeater {
            model: root.shapes
            delegate: Rectangle {
                id: chip
                required property string modelData
                required property int index
                readonly property int raw: root.minimum + index
                readonly property bool selected: root.currentRaw === raw
                implicitWidth: Math.max(44, label.implicitWidth + 2 * Metrics.spacingSm)
                implicitHeight: 32
                radius: Metrics.radiusSm
                color: selected ? Theme.toneBackground("accent") : (hover.hovered ? Theme.surfaceHover : Theme.surfaceRaised)
                border.width: selected || activeFocus ? 1 : Metrics.borderWidth
                border.color: activeFocus ? Theme.focusRing : (selected ? root.accentColor : Theme.border)

                Accessible.role: Accessible.Button
                Accessible.name: modelData
                Accessible.checkable: true
                Accessible.checked: selected
                activeFocusOnTab: root.editable
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                        if (root.editable) root.shapeSelected(raw)
                        event.accepted = true
                    }
                }

                XpLabel {
                    id: label
                    anchors.centerIn: parent
                    text: chip.modelData
                    role: "caption"
                    font.weight: chip.selected ? Typography.weightMedium : Typography.weightRegular
                    color: chip.selected ? Theme.accentText : Theme.textPrimary
                }
                LfoPreview {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 3 }
                    height: 8
                    shapeIndex: chip.index
                    accentColor: chip.selected ? root.accentColor : Theme.textMuted
                    opacity: 0.85
                }
                HoverHandler { id: hover; enabled: root.editable }
                TapHandler {
                    enabled: root.editable
                    onTapped: root.shapeSelected(chip.raw)
                }
            }
        }
    }
}
