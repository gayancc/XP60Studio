import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Templates as T
import QtQuick.Layouts
import XP60Studio

ColumnLayout {
    id: root
    required property var editor
    required property string parameterId
    property string title: parameter.label || parameter.name || ""
    property color accentColor: parameter.unit === "tone" ? Theme.toneColor(editor.selectedTone)
        : parameter.unit === "chorus" ? Theme.tone2 : parameter.unit === "reverb" ? Theme.tone3 : Theme.tone4
    readonly property var parameter: editor.effectValues[parameterId] || ({})
    property bool exact: false
    property bool highlighted: false
    signal selected(string parameterId)
    spacing: Metrics.spacingXs
    implicitWidth: 126
    XpLabel {
        Layout.fillWidth: true; Layout.preferredHeight: 32
        text: root.title; role: "caption"; color: root.highlighted ? root.accentColor : Theme.textSecondary
        horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
        elide: Text.ElideNone
        maximumLineCount: 2
        QQC.ToolTip.visible: labelHover.hovered
        QQC.ToolTip.text: root.parameter.name || root.title
        HoverHandler { id: labelHover }
    }
    Item {
        Layout.alignment: Qt.AlignHCenter
        implicitWidth: 68; implicitHeight: 68
        XpKnob {
            id: knob
            objectName: "effectKnob-" + root.parameterId
            anchors.fill: parent
            from: root.parameter.minimum || 0; to: root.parameter.maximum || 127
            value: root.parameter.value || 0
            bipolar: root.parameter.bipolar || false
            snapMode: T.Dial.SnapAlways
            accentColor: root.accentColor
            enabled: root.parameter.value !== undefined && !root.editor.comparing
            valueText: root.title + ": " + (root.parameter.display || "")
            onPressedChanged: {
                if (pressed) { root.selected(root.parameterId); root.editor.beginEffectGesture() }
                else root.editor.endEffectGesture()
            }
            onMoved: root.editor.editEffect(root.parameterId, Math.round(value))
            QQC.ToolTip.visible: pressed
            QQC.ToolTip.text: root.parameter.display || ""
            HoverHandler { cursorShape: Qt.SizeVerCursor }
        }
        // Original A marker has the same sweep as XpKnob; no duplicate value model.
        Rectangle {
            visible: root.parameter.modified || false
            x: 33; y: 1; width: 2; height: 6; radius: 1
            color: Theme.textSecondary
            transform: Rotation {
                origin.x: 1; origin.y: 33
                angle: -140 + 280 * ((root.parameter.original || 0) - knob.from) / Math.max(1, knob.to - knob.from)
            }
        }
    }
    XpButton {
        objectName: "effectReadout-" + root.parameterId
        Layout.alignment: Qt.AlignHCenter
        text: (root.parameter.display || "—") + (root.parameter.modified ? " •" : "")
        compact: true; variant: "ghost"
        onClicked: root.exact = !root.exact
        Accessible.name: root.title + qsTr(" exact entry")
    }
    ParameterValueEditor {
        visible: root.exact; Layout.fillWidth: true
        value: root.parameter.value || 0
        minimumValue: root.parameter.minimum || 0; maximumValue: root.parameter.maximum || 127
        editable: !root.editor.comparing
        accessibleName: root.title
        onEdited: function(value) { root.editor.editEffect(root.parameterId, value) }
    }
    Component.onDestruction: editor.endEffectGesture()
}

