import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// A shared, virtualised control surface for a focused synthesis section and
// the searchable Expert projection. It edits the existing Patch through C++.
XpCard {
    id: root
    required property EditorParameterModel parameters
    required property PatchEditorViewModel editor
    property bool expert: false
    property string title: ""
    property string note: qsTr("Exact entry uses XP values. Labels show the documented interpretation.")
    implicitHeight: column.implicitHeight + 2 * Metrics.cardPadding
    accentColor: expert && parameters.commonScope ? Theme.accent : Theme.toneColor(editor.selectedTone)

    ColumnLayout {
        id: column
        anchors { left: parent.left; right: parent.right; top: parent.top }
        spacing: Metrics.spacingSm

        XpPanelHeader { title: root.title; Layout.fillWidth: true }
        XpLabel {
            Layout.fillWidth: true
            text: root.editor.liveAudition ? qsTr("%1 · live updates to the temporary Patch").arg(root.parameters.targetText)
                : root.expert ? qsTr("All documented controls · shared undo and A/B")
                             : qsTr("%1 · edits stay local").arg(root.parameters.targetText)
            role: "caption"
            secondary: true
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            XpComboBox {
                objectName: "expertScope"
                visible: root.expert
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                model: [qsTr("Selected Tone"), qsTr("Patch Common")]
                currentIndex: root.parameters.commonScope ? 1 : 0
                onActivated: function(index) { root.parameters.commonScope = index === 1 }
                Accessible.name: qsTr("Parameter scope")
            }
            XpComboBox {
                objectName: "parameterGroup"
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                model: root.parameters.groups
                currentIndex: root.parameters.group
                onActivated: function(index) { root.parameters.group = index }
                Accessible.name: qsTr("Parameter group")
            }
        }
        XpTextField {
            objectName: "parameterSearch"
            visible: root.expert
            Layout.fillWidth: true
            placeholderText: qsTr("Find a parameter in this scope…")
            text: root.parameters.search
            onTextEdited: root.parameters.search = text
            Accessible.name: qsTr("Find a parameter")
        }
        // The name is one domain value, never twelve editable ASCII cells.
        XpTextField {
            objectName: "expertPatchName"
            visible: root.expert && root.parameters.commonScope
            Layout.fillWidth: true
            enabled: !root.editor.comparing
            text: root.editor.patchName
            maximumLength: 12
            onEditingFinished: {
                root.editor.patchName = text
                text = Qt.binding(function() { return root.editor.patchName })
            }
            Accessible.name: qsTr("Patch name")
        }
        XpDivider { Layout.fillWidth: true }
        XpLabel {
            Layout.fillWidth: true
            text: root.note
            role: "caption"
            muted: true
            wrapMode: Text.WordWrap
        }
        GridView {
            id: grid
            objectName: "parameterGrid"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(76, Math.min(root.expert ? 380 : 304,
                                                        Math.ceil(count / columns) * cellHeight))
            readonly property int columns: width >= 860 ? 4 : width >= 620 ? 3 : width >= 410 ? 2 : 1
            cellWidth: width / columns
            cellHeight: 76
            clip: true
            model: root.parameters
            boundsBehavior: Flickable.StopAtBounds
            QQC.ScrollBar.vertical: XpScrollBar {}

            delegate: Item {
                id: cell
                required property string name
                required property string parameterId
                required property int toneNumber
                required property int rawValue
                required property string valueText
                required property int minimum
                required property int maximum
                required property var choices
                width: grid.cellWidth
                height: grid.cellHeight
                clip: true
                objectName: "parameter-" + parameterId

                ColumnLayout {
                    anchors { fill: parent; rightMargin: Metrics.spacingMd; bottomMargin: Metrics.spacingSm }
                    spacing: 2
                    XpLabel {
                        text: cell.name
                        role: "caption"
                        secondary: true
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.maximumWidth: cell.width - Metrics.spacingMd
                        elide: Text.ElideRight
                        QQC.ToolTip.text: cell.name
                        QQC.ToolTip.visible: hover.hovered
                        HoverHandler { id: hover }
                    }
                    XpComboBox {
                        objectName: "parameterChoice"
                        visible: cell.choices.length > 0
                        Layout.fillWidth: true
                        enabled: !root.editor.comparing
                        model: cell.choices
                        currentIndex: cell.rawValue - cell.minimum
                        onActivated: function(index) { root.parameters.edit(cell.parameterId, cell.toneNumber, index + cell.minimum) }
                        Accessible.name: cell.name
                    }
                    ParameterValueEditor {
                        objectName: "parameterValue"
                        visible: cell.choices.length === 0
                        Layout.fillWidth: true
                        value: cell.rawValue
                        displayText: cell.valueText
                        minimumValue: cell.minimum
                        maximumValue: cell.maximum
                        editable: !root.editor.comparing
                        accessibleName: cell.name
                        onEdited: function(v) { root.parameters.edit(cell.parameterId, cell.toneNumber, v) }
                    }
                }
            }
            XpLabel {
                anchors.centerIn: parent
                visible: grid.count === 0
                text: qsTr("No matching parameters")
                secondary: true
            }
        }
    }
}
