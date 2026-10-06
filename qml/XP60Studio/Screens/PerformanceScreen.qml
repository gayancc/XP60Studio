import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Performance editor — the 16-Part mixer.
//
// A Performance is how the XP-60 plays sixteen Patches at once, so the screen
// is a mixer: sixteen channel strips side by side, the way the instrument's own
// Part page and every mixing desk arranges them. What a musician wants at a
// glance is which Parts are live, how loud, and where in the stereo field; that
// is what a strip shows, and the inspector below carries the rest.
//
// ── Sending ─────────────────────────────────────────────────────────────────
//
// **Send to XP-60** writes the *temporary* Performance, which is what the
// instrument is playing now and is erased by power-off or by selecting another
// Performance. That is audition. Writing a USER Performance is a persistent
// write and is not offered yet — see PerformanceViewModel.
FocusScope {
    id: root

    required property PerformanceViewModel performance

    readonly property bool wide: width >= 1180
    property int selectedPart: 1

    readonly property var selectedStrip: root.performance.hasPerformance
        ? root.performance.parts[root.selectedPart - 1] : null

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.screenMargin(root.width)
        spacing: Metrics.spacingMd

        // Identity and transport --------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            XpLabel { text: qsTr("PERFORMANCE"); role: "overline"; color: Theme.accentText }
            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingSm
                XpLabel {
                    objectName: "performanceName"
                    text: root.performance.hasPerformance ? root.performance.name : qsTr("No Performance open")
                    role: "title"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                StatusPill {
                    objectName: "performanceActiveParts"
                    visible: root.performance.hasPerformance
                    text: qsTr("%1 of %2 parts on").arg(root.performance.activePartCount).arg(root.performance.partCount)
                    tone: "info"
                    showDot: false
                }
                StatusPill {
                    objectName: "performanceModified"
                    visible: root.performance.modified
                    text: qsTr("EDITED")
                    tone: "warning"
                }
            }
            XpLabel {
                visible: root.performance.hasPerformance
                Layout.fillWidth: true
                text: root.performance.sourceText + " · " + qsTr("tempo %1").arg(root.performance.tempo)
                      + " · " + root.performance.keyboardMode
                role: "caption"
                muted: true
                elide: Text.ElideRight
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: Metrics.spacingXs

            XpButton {
                objectName: "performanceFetchTemporary"
                text: qsTr("Read from XP-60")
                iconName: "midi-in"
                variant: "primary"
                enabled: root.performance.canFetch
                onClicked: root.performance.fetchTemporary()
            }
            XpButton {
                objectName: "performanceSend"
                text: qsTr("Send to XP-60")
                iconName: "midi-out"
                enabled: root.performance.canSend
                QQC.ToolTip.visible: hovered
                QQC.ToolTip.delay: 400
                QQC.ToolTip.text: qsTr("Writes the temporary Performance — what the instrument is playing now. Power-off or selecting another Performance erases it. This does not write a USER Performance.")
                onClicked: root.performance.sendToTemporary()
            }
            XpButton {
                objectName: "performanceUndo"
                iconName: "undo"
                iconOnly: true
                variant: "ghost"
                enabled: root.performance.canUndo
                Accessible.name: qsTr("Undo")
                QQC.ToolTip.visible: hovered && root.performance.canUndo
                QQC.ToolTip.delay: 400
                QQC.ToolTip.text: root.performance.undoLabel
                onClicked: root.performance.undo()
            }
            XpButton {
                objectName: "performanceRedo"
                iconName: "redo"
                iconOnly: true
                variant: "ghost"
                enabled: root.performance.canRedo
                Accessible.name: qsTr("Redo")
                QQC.ToolTip.visible: hovered && root.performance.canRedo
                QQC.ToolTip.delay: 400
                QQC.ToolTip.text: qsTr("Redo")
                onClicked: root.performance.redo()
            }
            XpButton {
                objectName: "performanceRevert"
                text: qsTr("Revert")
                variant: "ghost"
                enabled: root.performance.modified
                onClicked: root.performance.revert()
            }
        }

        XpLabel {
            objectName: "performanceTransferMessage"
            Layout.fillWidth: true
            visible: root.performance.transferMessage.length > 0
            text: root.performance.transferMessage
            role: "caption"
            wrapMode: Text.WordWrap
            color: root.performance.transferTone === "error" ? Theme.error
                   : root.performance.transferTone === "success" ? Theme.success
                   : root.performance.transferTone === "warning" ? Theme.warning : Theme.textSecondary
        }

        // The sixteen strips -------------------------------------------------
        XpCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 260
            visible: root.performance.hasPerformance
            padding: Metrics.spacingSm

            QQC.ScrollView {
                anchors.fill: parent
                clip: true
                QQC.ScrollBar.horizontal: XpScrollBar {}

                RowLayout {
                    height: parent.height
                    spacing: Metrics.spacingXs
                    Repeater {
                        model: root.performance.parts
                        delegate: PerformancePartStrip {
                            required property var modelData
                            objectName: "partStrip" + modelData.partNumber
                            Layout.fillHeight: true
                            part: modelData
                            selected: root.selectedPart === modelData.partNumber
                            onClicked: root.selectedPart = modelData.partNumber
                            onLevelChanged: function (level) {
                                root.performance.setPartLevel(modelData.partNumber, level)
                            }
                            onPanChanged: function (pan) {
                                root.performance.setPartPan(modelData.partNumber, pan)
                            }
                            onReceivesToggled: function (receives) {
                                root.performance.setPartReceives(modelData.partNumber, receives)
                            }
                        }
                    }
                }
            }
        }

        // The selected Part, in full ------------------------------------------
        XpCard {
            objectName: "performancePartInspector"
            Layout.fillWidth: true
            visible: root.performance.hasPerformance && root.selectedStrip !== null
            padding: Metrics.spacingMd
            implicitHeight: inspectorColumn.implicitHeight + 2 * Metrics.spacingMd

            ColumnLayout {
                id: inspectorColumn
                anchors { left: parent.left; right: parent.right; top: parent.top }
                spacing: Metrics.spacingSm

                XpModuleHeader {
                    title: qsTr("PART %1").arg(root.selectedPart)
                    iconName: "sliders"
                    lampColor: root.selectedStrip && root.selectedStrip.receiveSwitch
                               ? Theme.live : Theme.offline
                    accentColor: Theme.accent
                    trailing: XpLabel {
                        visible: root.selectedStrip && root.selectedStrip.isRhythmPart
                        text: qsTr("Rhythm part — a Rhythm Setup, not a Patch")
                        role: "caption"
                        muted: true
                    }
                }

                // What the Part is set to, then where on the keyboard it
                // answers — side by side across the panel. Stacked, the sends
                // and the output chips each used the left third of a 1560 px
                // strip and left the rest empty; abreast, the keyboard gets the
                // width its drag handles want and the panel loses two thirds of
                // its height.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.gapMd

                    ColumnLayout {
                        Layout.fillWidth: false
                        Layout.alignment: Qt.AlignTop
                        spacing: Metrics.gap

                        RowLayout {
                            Layout.fillWidth: false
                            spacing: Metrics.gap

                            MusicalParamKnob {
                                objectName: "inspectorChannel"
                                label: qsTr("MIDI Ch")
                                value: root.selectedStrip ? root.selectedStrip.midiChannel : 1
                                from: 1
                                to: 16
                                displayText: root.selectedStrip ? String(root.selectedStrip.midiChannel) : "1"
                                knobSize: Metrics.knobSm
                                onEdited: root.performance.setPartMidiChannel(root.selectedPart, value)
                            }
                            MusicalParamKnob {
                                objectName: "inspectorChorus"
                                label: qsTr("Chorus")
                                value: root.selectedStrip ? root.selectedStrip.chorusSend : 0
                                from: 0
                                to: 127
                                knobSize: Metrics.knobSm
                                accentColor: Theme.tone2
                                onEdited: root.performance.setPartChorusSend(root.selectedPart, value)
                            }
                            MusicalParamKnob {
                                objectName: "inspectorReverb"
                                label: qsTr("Reverb")
                                value: root.selectedStrip ? root.selectedStrip.reverbSend : 0
                                from: 0
                                to: 127
                                knobSize: Metrics.knobSm
                                accentColor: Theme.tone3
                                onEdited: root.performance.setPartReverbSend(root.selectedPart, value)
                            }
                            MusicalParamKnob {
                                objectName: "inspectorVoiceReserve"
                                label: qsTr("Voices")
                                value: root.selectedStrip ? root.selectedStrip.voiceReserve : 0
                                from: 0
                                to: 64
                                knobSize: Metrics.knobSm
                                onEdited: root.performance.setPartVoiceReserve(root.selectedPart, value)
                            }
                            MusicalParamKnob {
                                objectName: "inspectorOctave"
                                label: qsTr("Octave")
                                value: root.selectedStrip ? root.selectedStrip.octaveShift : 0
                                from: -3
                                to: 3
                                displayText: {
                                    var v = root.selectedStrip ? root.selectedStrip.octaveShift : 0
                                    return v > 0 ? ("+" + v) : String(v)
                                }
                                bipolar: true
                                knobSize: Metrics.knobSm
                                onEdited: root.performance.setPartOctaveShift(root.selectedPart, value)
                            }
                            Item { Layout.fillWidth: true }
                        }

                        // Where the Part leaves the XP-60 is a destination
                        // choice with five documented values, so it selects
                        // like one rather than printing its current label.
                        ColumnLayout {
                            Layout.fillWidth: false
                            spacing: Metrics.gapXs
                            XpModuleHeader {
                                title: qsTr("OUTPUT")
                                iconName: "midi-out"
                                accentColor: Theme.accent
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Metrics.spacingXs
                                Repeater {
                                    model: ["MIX", "EFX", "DIR", "<OUTPUT-2>", "PAT"]
                                    XpButton {
                                        required property string modelData
                                        required property int index
                                        objectName: index === 0 ? "inspectorOutput" : ""
                                        text: modelData
                                        compact: true
                                        variant: root.selectedStrip && root.selectedStrip.outputAssign === index
                                                 ? "primary" : "ghost"
                                        onClicked: root.performance.setPartOutputAssign(root.selectedPart, index)
                                        Accessible.name: qsTr("Part output: %1").arg(modelData)
                                    }
                                }
                                Item { Layout.fillWidth: true }
                            }
                        }

                        XpLabel {
                            Layout.fillWidth: true
                            visible: root.selectedStrip && root.selectedStrip.voiceReserve === 0
                            text: qsTr("Voice reserve 0: this Part keeps no voices of its own, so a busy Performance can starve it.")
                            role: "caption"
                            color: Theme.warning
                            wrapMode: Text.WordWrap
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        spacing: Metrics.gapXs

                        XpModuleHeader {
                            title: qsTr("KEY RANGE")
                            iconName: "keyboard"
                            accentColor: Theme.accent
                            trailing: XpLabel {
                                text: root.selectedStrip
                                      ? qsTr("%1 – %2").arg(root.selectedStrip.keyLowerNote).arg(root.selectedStrip.keyUpperNote)
                                      : ""
                                role: "mono"
                                color: Theme.accentText
                            }
                        }
                        KeyboardStrip {
                            objectName: "inspectorKeyRange"
                            Layout.fillWidth: true
                            lowerNote: root.selectedStrip ? root.selectedStrip.keyLowerRaw : 0
                            upperNote: root.selectedStrip ? root.selectedStrip.keyUpperRaw : 127
                            accentColor: Theme.accent
                            onRangeEdited: function(lower, upper) {
                                root.performance.setPartKeyRange(root.selectedPart, lower, upper)
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Item { Layout.fillWidth: true }
                            // Precision path kept for scripting/tests; not the primary UI.
                            ParameterValueEditor {
                                objectName: "inspectorKeyLow"
                                visible: false
                                value: root.selectedStrip ? root.selectedStrip.keyLowerRaw : 0
                                minimumValue: 0
                                maximumValue: root.selectedStrip ? root.selectedStrip.keyUpperRaw : 127
                                onEdited: function(v) {
                                    root.performance.setPartKeyRange(root.selectedPart, v, root.selectedStrip.keyUpperRaw)
                                }
                            }
                            ParameterValueEditor {
                                objectName: "inspectorKeyHigh"
                                visible: false
                                value: root.selectedStrip ? root.selectedStrip.keyUpperRaw : 127
                                minimumValue: root.selectedStrip ? root.selectedStrip.keyLowerRaw : 0
                                maximumValue: 127
                                onEdited: function(v) {
                                    root.performance.setPartKeyRange(root.selectedPart, root.selectedStrip.keyLowerRaw, v)
                                }
                            }
                        }
                    }
                }
            }
        }

        XpEmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.performance.hasPerformance
            iconName: "performance"
            // The heading above already says "No Performance open"; this
            // region says what will appear and offers the action that fills it.
            title: qsTr("The 16-Part mixer appears here")
            message: qsTr("Read the Performance the XP-60 is playing now, and each of its sixteen Parts becomes a channel strip.")
            actionText: qsTr("Read from XP-60")
            actionEnabled: root.performance.canFetch
            onActionTriggered: root.performance.fetchTemporary()
        }
    }
}
