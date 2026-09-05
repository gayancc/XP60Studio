import QtQuick
import QtQuick.Layouts
import XP60Studio

ColumnLayout {
    id: root
    required property var editor
    required property string parameterId
    property string title: parameter.name || ""
    readonly property var parameter: editor.effectValues[parameterId] || ({})
    XpLabel { text: root.title; role: "overline"; secondary: true }
    Flow {
        Layout.fillWidth: true
        spacing: Metrics.spacingSm
        Repeater {
            model: root.parameter.choices || []
            XpButton {
                required property string modelData
                required property int index
                objectName: "effectChoice-" + root.parameterId + "-" + index
                text: modelData; compact: true
                variant: root.parameter.value === index ? "primary" : "secondary"
                enabled: !root.editor.comparing
                onClicked: root.editor.editEffect(root.parameterId, index)
                Accessible.name: root.title + ": " + modelData
            }
        }
    }
}
