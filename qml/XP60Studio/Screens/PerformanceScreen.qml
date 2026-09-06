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
                variant: "primary"
                enabled: root.performance.canFetch
                onClicked: root.performance.fetchTemporary()
            }
            XpButton {
                objectName: "performanceSend"
                text: qsTr("Send to XP-60")
                enabled: root.performance.canSend
                QQC.ToolTip.visible: hovered
                QQC.ToolTip.delay: 400
                QQC.ToolTip.text: qsTr("Writes the temporary Performance — what the instrument is playing now. Power-off or selecting another Performance erases it. This does not write a USER Performance.")
                onClicked: root.performance.sendToTemporary()
            }
            XpButton {
                objectName: "performanceUndo"
                text: qsTr("Undo")
                variant: "ghost"
                enabled: root.performance.canUndo
                QQC.ToolTip.visible: hovered && root.performance.canUndo
                QQC.ToolTip.text: root.performance.undoLabel
                onClicked: root.performance.undo()
            }
            XpButton {
                objectName: "performanceRedo"
                text: qsTr("Redo")
                variant: "ghost"
                enabled: root.performance.canRedo
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

            ColumnLayout {
                anchors.fill: parent
                spacing: Metrics.spacingSm

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingSm
                    XpLabel {
                        text: qsTr("PART %1").arg(root.selectedPart)
                        role: "overline"
                        color: Theme.accentText
                    }
                    XpLabel {
                        visible: root.selectedStrip && root.selectedStrip.isRhythmPart
                        text: qsTr("the Rhythm part — its sound is a Rhythm Setup, not a Patch")
                        role: "caption"
                        muted: true
                    }
                    Item { Layout.fillWidth: true }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.wide ? 4 : 2
                    columnSpacing: Metrics.spacingMd
                    rowSpacing: Metrics.spacingSm

                    XpFieldRow {
                        label: qsTr("MIDI channel")
                        XpSpinField {
                            objectName: "inspectorChannel"
                            from: 1
                            to: 16
                            value: root.selectedStrip ? root.selectedStrip.midiChannel : 1
                            onValueModified: root.performance.setPartMidiChannel(root.selectedPart, value)
                        }
                    }
                    XpFieldRow {
                        label: qsTr("Chorus send")
                        XpSpinField {
                            objectName: "inspectorChorus"
                            from: 0
                            to: 127
                            value: root.selectedStrip ? root.selectedStrip.chorusSend : 0
                            onValueModified: root.performance.setPartChorusSend(root.selectedPart, value)
                        }
                    }
                    XpFieldRow {
                        label: qsTr("Reverb send")
                        XpSpinField {
                            objectName: "inspectorReverb"
                            from: 0
                            to: 127
                            value: root.selectedStrip ? root.selectedStrip.reverbSend : 0
                            onValueModified: root.performance.setPartReverbSend(root.selectedPart, value)
                        }
                    }
                    XpFieldRow {
                        // Roland documents 0..64, not 0..127.
                        label: qsTr("Voice reserve")
                        XpSpinField {
                            objectName: "inspectorVoiceReserve"
                            from: 0
                            to: 64
                            value: root.selectedStrip ? root.selectedStrip.voiceReserve : 0
                            onValueModified: root.performance.setPartVoiceReserve(root.selectedPart, value)
                        }
                    }
                    XpFieldRow {
                        label: qsTr("Octave shift")
                        XpSpinField {
                            objectName: "inspectorOctave"
                            from: -3
                            to: 3
                            value: root.selectedStrip ? root.selectedStrip.octaveShift : 0
                            onValueModified: root.performance.setPartOctaveShift(root.selectedPart, value)
                        }
                    }
                    XpFieldRow {
                        label: qsTr("Key range low")
                        XpSpinField {
                            objectName: "inspectorKeyLow"
                            from: 0
                            to: 127
                            value: root.selectedStrip ? root.selectedStrip.keyLowerRaw : 0
                            // Refused rather than clamped when it would cross
                            // the upper bound, so the pair is never half-set.
                            onValueModified: root.performance.setPartKeyRange(
                                root.selectedPart, value, root.selectedStrip.keyUpperRaw)
                        }
                    }
                    XpFieldRow {
                        label: qsTr("Key range high")
                        XpSpinField {
                            objectName: "inspectorKeyHigh"
                            from: 0
                            to: 127
                            value: root.selectedStrip ? root.selectedStrip.keyUpperRaw : 127
                            onValueModified: root.performance.setPartKeyRange(
                                root.selectedPart, root.selectedStrip.keyLowerRaw, value)
                        }
                    }
                    XpFieldRow {
                        label: qsTr("Output")
                        XpLabel {
                            objectName: "inspectorOutput"
                            text: root.selectedStrip ? root.selectedStrip.outputAssignLabel : ""
                            role: "body"
                        }
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
        }

        XpEmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.performance.hasPerformance
            title: qsTr("No Performance open")
            message: qsTr("Read the Performance the XP-60 is playing now, and its sixteen Parts appear here as a mixer.")
        }
    }
}
