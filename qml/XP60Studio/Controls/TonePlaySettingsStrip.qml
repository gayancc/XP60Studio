import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// How the player shapes a note after pressing it.
//
// Bend: up/down as a pair of knobs around a centre zero, the way a pitch wheel
// feels. Portamento: an armable glide with a note-to-note trail whose length is
// Time, plus NORMAL/LEGATO mode chips.
//
// Key assign moved out to `ToneKeyAssign`, which sits with Key Range and
// Velocity: POLY/SOLO answers what the Tone does when keys go down, which is
// the same question those two ask.
XpCard {
    id: root

    required property PatchEditorViewModel editor
    property color accentColor: Theme.accent

    readonly property var settings: {
        var map = ({})
        var rows = root.editor.toneSettings || []
        for (var i = 0; i < rows.length; ++i)
            map[rows[i].parameterId] = rows[i]
        return map
    }

    function setting(id) {
        return root.settings[id] || ({ raw: 0, valueText: "", minimum: 0, maximum: 127, choices: [] })
    }

    padding: Metrics.spacingSm
    implicitHeight: column.implicitHeight + 2 * Metrics.spacingSm

    ColumnLayout {
        id: column
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom }
        spacing: Metrics.spacingSm

        XpModuleHeader {
            title: qsTr("TONE SETTINGS")
            iconName: "sliders"
            accentColor: root.accentColor
        }

        GridLayout {
            Layout.fillWidth: true
            // Stacks into its column when it shares the Design row, and only
            // spreads across when it has the whole width to itself.
            columns: root.width >= 760 ? 2 : 1
            columnSpacing: Metrics.spacingMd
            rowSpacing: Metrics.spacingSm

            // ── Pitch bend ────────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                implicitHeight: bendColumn.implicitHeight + 2 * Metrics.spacingSm
                radius: Metrics.radiusSm
                color: Theme.surfaceRaised
                border.width: 1
                border.color: Theme.borderSubtle

                ColumnLayout {
                    id: bendColumn
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: Metrics.spacingSm }
                    spacing: Metrics.spacingXs

                    XpLabel { text: qsTr("PITCH BEND"); role: "overline"; secondary: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingMd

                        MusicalParamKnob {
                            objectName: "bendRangeDown"
                            label: qsTr("Down")
                            value: root.setting("common.bend_range_down").raw
                            from: root.setting("common.bend_range_down").minimum
                            to: root.setting("common.bend_range_down").maximum
                            displayText: root.setting("common.bend_range_down").valueText
                            knobSize: Metrics.knobSm
                            accentColor: root.accentColor
                            editable: !root.editor.comparing
                            onEdited: root.editor.setToneSetting("common.bend_range_down", value)
                        }

                        // The bend span itself, filling the width between the
                        // two knobs: a centre zero with the Down reach drawn
                        // left of it and the Up reach right. A vertical wheel
                        // graphic here was 22 px wide in a cell four times that,
                        // so the module read as mostly empty.
                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44

                            readonly property real downSpan:
                                root.setting("common.bend_range_down").raw
                                / Math.max(1, root.setting("common.bend_range_down").maximum)
                            readonly property real upSpan:
                                root.setting("common.bend_range_up").raw
                                / Math.max(1, root.setting("common.bend_range_up").maximum)

                            Rectangle {
                                id: bendTrack
                                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                                height: 18
                                radius: 9
                                color: Theme.surfaceSunken
                                border.width: 1
                                border.color: Theme.borderStrong

                                // Down reach, left of centre.
                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: parent.width / 2 - width
                                    width: Math.max(2, (parent.width / 2 - 3) * parent.parent.downSpan)
                                    height: 10
                                    radius: 5
                                    color: root.accentColor
                                    opacity: 0.9
                                }
                                // Up reach, right of centre.
                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: parent.width / 2
                                    width: Math.max(2, (parent.width / 2 - 3) * parent.parent.upSpan)
                                    height: 10
                                    radius: 5
                                    color: root.accentColor
                                    opacity: 0.9
                                }
                                // Centre detent — the note as played.
                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: parent.width / 2 - 1
                                    width: 2
                                    height: 16
                                    color: Theme.textPrimary
                                }
                            }
                        }

                        MusicalParamKnob {
                            objectName: "bendRangeUp"
                            label: qsTr("Up")
                            value: root.setting("common.bend_range_up").raw
                            from: root.setting("common.bend_range_up").minimum
                            to: root.setting("common.bend_range_up").maximum
                            displayText: root.setting("common.bend_range_up").valueText
                            knobSize: Metrics.knobSm
                            accentColor: root.accentColor
                            editable: !root.editor.comparing
                            onEdited: root.editor.setToneSetting("common.bend_range_up", value)
                        }
                    }
                }
            }

            // ── Portamento / glide ────────────────────────────────────────
            Rectangle {
                id: portaCard
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.preferredWidth: 1.4
                implicitHeight: portaColumn.implicitHeight + 2 * Metrics.spacingSm
                radius: Metrics.radiusSm
                color: portaOn ? Theme.accentSoft : Theme.surfaceRaised
                border.width: 1
                border.color: portaOn ? Theme.toneBorder("accent") : Theme.borderSubtle

                readonly property bool portaOn: root.setting("common.portamento_switch").raw > 0
                readonly property real glide: root.setting("common.portamento_time").raw
                                             / Math.max(1, root.setting("common.portamento_time").maximum)

                ColumnLayout {
                    id: portaColumn
                    anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom; margins: Metrics.spacingSm }
                    spacing: Metrics.spacingXs

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingSm
                        XpLabel { text: qsTr("PORTAMENTO"); role: "overline"; secondary: true }
                        Item { Layout.fillWidth: true }
                        XpSwitch {
                            objectName: "portamentoSwitch"
                            checked: portaCard.portaOn
                            enabled: !root.editor.comparing
                            Accessible.name: qsTr("Portamento")
                            onToggled: root.editor.setToneSetting("common.portamento_switch", checked ? 1 : 0)
                        }
                        XpLabel {
                            text: portaCard.portaOn ? qsTr("ON") : qsTr("OFF")
                            role: "caption"
                            color: portaCard.portaOn ? Theme.accentText : Theme.textMuted
                        }
                    }

                    // Note → note glide trail; dimmed when off. Absorbs the
                    // card's remaining height.
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 56
                        opacity: portaCard.portaOn ? 1 : 0.4

                        Rectangle {
                            id: startNote
                            x: 8
                            y: parent.height - 28
                            width: 22
                            height: 22
                            radius: 4
                            color: Theme.surfaceSunken
                            border.width: 1
                            border.color: root.accentColor
                            XpLabel {
                                anchors.centerIn: parent
                                text: "C"
                                role: "caption"
                                color: root.accentColor
                            }
                        }
                        Rectangle {
                            id: endNote
                            // Pinned to the right end of the track. The
                            // destination note does not move — Time is how long
                            // the pitch takes to get there, which the curve
                            // below shows by how late it rises. Letting Time
                            // shorten the whole graphic instead left most of
                            // the module empty at small values.
                            x: parent.width - width - 8
                            y: 6
                            width: 22
                            height: 22
                            radius: 4
                            color: Theme.surfaceSunken
                            border.width: 1
                            border.color: Theme.tone2
                            XpLabel {
                                anchors.centerIn: parent
                                text: "E"
                                role: "caption"
                                color: Theme.tone2
                            }
                        }
                        Rectangle {
                            anchors {
                                left: startNote.right; right: endNote.left
                                verticalCenter: parent.verticalCenter
                                leftMargin: Metrics.spacingXs; rightMargin: Metrics.spacingXs
                            }
                            height: 1
                            color: Theme.borderSubtle
                        }
                        Canvas {
                            id: glideCanvas
                            anchors.fill: parent
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.reset()
                                var x0 = startNote.x + startNote.width
                                var y0 = startNote.y + startNote.height / 2
                                var x1 = endNote.x
                                var y1 = endNote.y + endNote.height / 2
                                ctx.strokeStyle = portaCard.portaOn ? Theme.accentText : Theme.textMuted
                                ctx.lineWidth = 2
                                ctx.lineCap = "round"
                                ctx.beginPath()
                                ctx.moveTo(x0, y0)
                                // A long Time keeps the pitch near the start
                                // note and arrives late; a short one leaves
                                // immediately.
                                var cx = x0 + (x1 - x0) * (0.15 + 0.75 * portaCard.glide)
                                ctx.quadraticCurveTo(cx, y0, x1, y1)
                                ctx.stroke()
                                ctx.fillStyle = ctx.strokeStyle
                                ctx.beginPath()
                                ctx.moveTo(x1, y1)
                                ctx.lineTo(x1 - 7, y1 - 4)
                                ctx.lineTo(x1 - 7, y1 + 4)
                                ctx.closePath()
                                ctx.fill()
                            }
                            Connections {
                                target: portaCard
                                function onGlideChanged() { glideCanvas.requestPaint() }
                                function onPortaOnChanged() { glideCanvas.requestPaint() }
                            }
                            Component.onCompleted: requestPaint()
                            onWidthChanged: requestPaint()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingMd

                        MusicalParamKnob {
                            objectName: "portamentoTime"
                            label: qsTr("Time")
                            value: root.setting("common.portamento_time").raw
                            from: root.setting("common.portamento_time").minimum
                            to: root.setting("common.portamento_time").maximum
                            displayText: root.setting("common.portamento_time").valueText
                            knobSize: Metrics.knobSm
                            accentColor: root.accentColor
                            editable: !root.editor.comparing && portaCard.portaOn
                            onEdited: root.editor.setToneSetting("common.portamento_time", value)
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Metrics.spacingXs
                            XpLabel { text: qsTr("Mode"); role: "caption"; secondary: true }
                            RowLayout {
                                spacing: Metrics.spacingXs
                                Repeater {
                                    model: root.setting("common.portamento_mode").choices.length > 0
                                           ? root.setting("common.portamento_mode").choices
                                           : [qsTr("NORMAL"), qsTr("LEGATO")]
                                    XpButton {
                                        required property string modelData
                                        required property int index
                                        objectName: "portamentoMode" + index
                                        text: modelData
                                        compact: true
                                        variant: root.setting("common.portamento_mode").raw === index
                                                 ? (portaCard.portaOn ? "primary" : "secondary")
                                                 : "ghost"
                                        enabled: !root.editor.comparing && portaCard.portaOn
                                        opacity: portaCard.portaOn ? 1 : 0.7
                                        onClicked: root.editor.setToneSetting("common.portamento_mode", index)
                                    }
                                }
                            }
                            XpLabel {
                                Layout.fillWidth: true
                                text: portaCard.portaOn
                                      ? qsTr("Glide between notes · longer Time = slower slide")
                                      : qsTr("Arm Portamento to hear note-to-note glide")
                                role: "caption"
                                muted: true
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

        }
    }
}
