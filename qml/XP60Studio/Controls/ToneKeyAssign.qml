import QtQuick
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// POLY / SOLO for the selected Tone, as two cards that show what they mean —
// three note stems against one.
//
// This lives with Key Range and Velocity rather than with bend and portamento:
// all three answer the same question, which is what the Tone does when the
// player presses keys. Bend and portamento are what the player does after.
ColumnLayout {
    id: root

    required property PatchEditorViewModel editor
    property color accentColor: Theme.accent

    readonly property var mode: {
        var rows = root.editor.toneSettings || []
        for (var i = 0; i < rows.length; ++i) {
            if (rows[i].parameterId === "common.key_assign_mode")
                return rows[i]
        }
        return ({ raw: 0, choices: [] })
    }

    spacing: Metrics.spacingXs

    XpModuleHeader {
        title: qsTr("KEY ASSIGN")
        iconName: "performance"
        accentColor: root.accentColor
        trailing: XpLabel {
            text: root.mode.raw === 0 ? qsTr("Several notes together")
                                      : qsTr("New notes steal the voice")
            role: "caption"
            muted: true
            elide: Text.ElideRight
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: false
        spacing: Metrics.spacingXs

        Repeater {
            objectName: "keyAssignChoices"
            model: root.mode.choices.length > 0 ? root.mode.choices
                                                : [qsTr("POLY"), qsTr("SOLO")]
            Rectangle {
                required property string modelData
                required property int index
                objectName: "keyAssign" + index
                Layout.fillWidth: true
                implicitHeight: 42
                radius: Metrics.radiusSm
                color: root.mode.raw === index
                       ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.16)
                       : Theme.surfaceSunken
                border.width: 1
                border.color: root.mode.raw === index ? root.accentColor : Theme.borderSubtle
                opacity: root.editor.comparing ? 0.55 : 1

                RowLayout {
                    anchors.centerIn: parent
                    spacing: Metrics.spacingSm

                    // POLY: three note stems. SOLO: one.
                    Row {
                        spacing: 3
                        Repeater {
                            model: index === 0 ? 3 : 1
                            Rectangle {
                                width: 3
                                height: 16
                                radius: 1
                                color: root.mode.raw === index ? root.accentColor : Theme.textMuted
                            }
                        }
                    }
                    XpLabel {
                        text: modelData
                        role: "caption"
                        font.weight: Typography.weightMedium
                        color: root.mode.raw === index ? root.accentColor : Theme.textSecondary
                    }
                }

                TapHandler {
                    enabled: !root.editor.comparing
                    onTapped: root.editor.setToneSetting("common.key_assign_mode", index)
                }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
            }
        }
    }
}
