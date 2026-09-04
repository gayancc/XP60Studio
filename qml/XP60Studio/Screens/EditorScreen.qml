import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Patch Editor / Four-Tone Mixer — mockup panel M2.
//
// Composition follows the master mockup: patch header with Write to XP-60,
// section tabs, four Tone cards, the signal path, and a bottom row of
// envelope editor, key/velocity ranges and contextual Tone settings.
Item {
    id: root

    required property PatchEditorViewModel editor

    // Below this the four Tone cards would be too narrow to read, so they
    // wrap to two rows of two rather than shrinking.
    readonly property bool wideTones: width >= 1120
    readonly property bool wideBottom: width >= 1180

    XpEmptyState {
        anchors.fill: parent
        visible: !root.editor.hasPatch
        glyph: "♪"
        title: qsTr("No Patch loaded")
        message: root.editor.emptyStateMessage
    }

    QQC.ScrollView {
        id: scroller
        anchors.fill: parent
        visible: root.editor.hasPatch
        contentWidth: availableWidth
        QQC.ScrollBar.vertical: XpScrollBar { parent: scroller; x: scroller.width - width; height: scroller.availableHeight }
        QQC.ScrollBar.horizontal.policy: QQC.ScrollBar.AlwaysOff

        ColumnLayout {
            width: scroller.availableWidth
            spacing: Metrics.spacingLg

            // ── Patch header ────────────────────────────────────────────────
            XpCard {
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                Layout.topMargin: Metrics.screenPadding
                implicitHeight: headerRow.implicitHeight + 2 * Metrics.cardPadding

                RowLayout {
                    id: headerRow
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                    spacing: Metrics.spacingMd

                    Rectangle {
                        implicitWidth: badge.implicitWidth + 2 * Metrics.spacingMd
                        implicitHeight: 28
                        radius: Metrics.radiusSm
                        color: Theme.surfaceRaised
                        border.width: 1
                        border.color: Theme.borderStrong
                        XpLabel { id: badge; anchors.centerIn: parent; text: qsTr("PATCH EDITOR"); role: "overline" }
                    }

                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        RowLayout {
                            spacing: Metrics.spacingSm
                            XpLabel { text: root.editor.locationText; role: "overline"; secondary: true }
                            StatusPill {
                                objectName: "patchStateBadge"
                                text: root.editor.stateBadgeText
                                tone: root.editor.stateBadgeTone
                                showDot: false
                            }
                        }
                        XpLabel {
                            objectName: "patchNameLabel"
                            text: root.editor.patchName
                            role: "display"
                            Layout.fillWidth: true
                        }
                        XpLabel {
                            text: root.editor.differenceSummary + " · " + root.editor.sourceText
                            role: "caption"
                            muted: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    // A/B original vs current
                    XpButton {
                        objectName: "compareButton"
                        text: root.editor.comparing ? qsTr("A · Original") : qsTr("B · Current")
                        glyph: "⇄"
                        compact: true
                        variant: root.editor.comparing ? "primary" : "secondary"
                        onClicked: root.editor.comparing = !root.editor.comparing
                    }
                    XpButton {
                        objectName: "undoButton"
                        text: "↺"; compact: true; variant: "ghost"
                        enabled: root.editor.canUndo
                        onClicked: root.editor.undo()
                        Accessible.name: qsTr("Undo")
                    }
                    XpButton {
                        objectName: "redoButton"
                        text: "↻"; compact: true; variant: "ghost"
                        enabled: root.editor.canRedo
                        onClicked: root.editor.redo()
                        Accessible.name: qsTr("Redo")
                    }
                    XpButton {
                        objectName: "revertButton"
                        text: qsTr("Revert"); compact: true
                        enabled: root.editor.modified
                        onClicked: root.editor.revertToOriginal()
                    }
                    XpButton {
                        objectName: "armWriteButton"
                        text: root.editor.writeArmed ? qsTr("Armed") : qsTr("Arm")
                        compact: true
                        variant: root.editor.writeArmed ? "danger" : "secondary"
                        enabled: root.editor.writeArmed || root.editor.canArmWrite
                        onClicked: root.editor.writeArmed ? root.editor.disarmWrite() : root.editor.armWrite()
                    }
                    XpButton {
                        objectName: "writeToDeviceButton"
                        text: qsTr("Write to XP-60")
                        variant: "primary"
                        enabled: root.editor.canWrite
                        onClicked: root.editor.writeToDevice()
                    }
                }
            }

            // Write outcome, only once something has happened.
            XpLabel {
                objectName: "writeMessage"
                visible: root.editor.writeStateText.length > 0 && root.editor.writeStateText !== "Idle"
                text: root.editor.writeStateText + " — " + root.editor.writeMessage
                role: "caption"
                color: root.editor.writeTone === "error" ? Theme.error
                     : root.editor.writeTone === "success" ? Theme.success : Theme.warning
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
            }

            // ── Section tabs ────────────────────────────────────────────────
            XpSegmentedControl {
                objectName: "sectionTabs"
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                model: root.editor.sectionNames
                currentIndex: root.editor.section
                onActivated: function(index) { root.editor.section = index }
            }

            // ── Four Tone cards ─────────────────────────────────────────────
            GridLayout {
                objectName: "toneGrid"
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                columns: root.wideTones ? 4 : 2
                columnSpacing: Metrics.spacingMd
                rowSpacing: Metrics.spacingMd

                Repeater {
                    model: root.editor.tones
                    delegate: ToneCard {
                        required property var modelData
                        objectName: "toneCard" + modelData.toneNumber
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        tone: modelData
                        selected: root.editor.selectedTone === modelData.toneNumber
                        onClicked: root.editor.selectedTone = modelData.toneNumber
                    }
                }
            }

            // ── Signal path ─────────────────────────────────────────────────
            XpCard {
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                implicitHeight: flowRow.implicitHeight + 2 * Metrics.cardPadding

                Flow {
                    id: flowRow
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                    spacing: Metrics.spacingSm

                    SignalFlowNode {
                        objectName: "structureNode"
                        title: qsTr("STRUCTURE"); detail: root.editor.structureText
                    }
                    SignalFlowConnector { anchors.verticalCenter: undefined; y: 20 }
                    SignalFlowNode { title: qsTr("MFX"); detail: root.editor.mfxText }
                    SignalFlowConnector { y: 20 }
                    SignalFlowNode { title: qsTr("CHORUS"); detail: root.editor.chorusText }
                    SignalFlowConnector { y: 20 }
                    SignalFlowNode { title: qsTr("REVERB"); detail: root.editor.reverbText }
                    SignalFlowConnector { y: 20 }
                    SignalFlowNode { title: qsTr("OUTPUT"); detail: root.editor.outputText; highlighted: true }
                }
            }

            // ── Envelope · ranges · Tone settings ───────────────────────────
            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                Layout.bottomMargin: Metrics.screenPadding
                columns: root.wideBottom ? 3 : 1
                columnSpacing: Metrics.spacingLg
                rowSpacing: Metrics.spacingLg

                // Envelope editor
                XpCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 5
                    Layout.alignment: Qt.AlignTop
                    implicitHeight: envColumn.implicitHeight + 2 * Metrics.cardPadding

                    ColumnLayout {
                        id: envColumn
                        anchors { left: parent.left; right: parent.right; top: parent.top }
                        spacing: Metrics.spacingSm

                        XpPanelHeader {
                            objectName: "envelopeHeader"
                            title: root.editor.envelopeTitle
                            glyph: "◺"
                        }

                        EnvelopeEditor {
                            objectName: "envelopeEditor"
                            visible: root.editor.envelopeAvailable
                            Layout.fillWidth: true
                            points: root.editor.envelopePoints
                            bipolar: root.editor.section !== 2 // Amp is unipolar
                            accentColor: Theme.toneColor(root.editor.selectedTone)
                            onPointMoved: function(index, x, y) { root.editor.moveEnvelopePoint(index, x, y) }
                        }

                        XpEmptyState {
                            visible: !root.editor.envelopeAvailable
                            Layout.fillWidth: true
                            implicitHeight: 120
                            glyph: "∿"
                            title: root.editor.section === 3 ? qsTr("Motion — not built yet")
                                                             : qsTr("Effects — not built yet")
                            message: root.editor.section === 3
                                     ? qsTr("The LFO editor arrives later in Phase 4. Sound, Filter and Amp are complete.")
                                     : qsTr("The effects editor arrives later in Phase 4. Sound, Filter and Amp are complete.")
                        }

                        // Exact stage values, so nothing is drag-only.
                        GridLayout {
                            objectName: "envelopeStages"
                            visible: root.editor.envelopeAvailable
                            Layout.fillWidth: true
                            // Two stages per row: four would crush the numeric
                            // fields, and every value must stay typeable.
                            columns: 2
                            columnSpacing: Metrics.spacingMd
                            rowSpacing: Metrics.spacingXs

                            Repeater {
                                model: root.editor.envelopeStages
                                delegate: ColumnLayout {
                                    required property var modelData
                                    spacing: 2
                                    Layout.fillWidth: true
                                    ParameterValueEditor {
                                        label: modelData.timeLabel
                                        value: modelData.timeRaw
                                        minimumValue: 0
                                        maximumValue: 127
                                        onEdited: function(v) { root.editor.setEnvelopeStageRaw(modelData.index, false, v) }
                                    }
                                    ParameterValueEditor {
                                        visible: modelData.hasLevel
                                        label: modelData.hasLevel ? modelData.levelLabel : ""
                                        value: modelData.hasLevel ? modelData.levelRaw : 0
                                        displayText: modelData.hasLevel ? modelData.levelText : ""
                                        minimumValue: 0
                                        maximumValue: 127
                                        onEdited: function(v) { root.editor.setEnvelopeStageRaw(modelData.index, true, v) }
                                    }
                                }
                            }
                        }

                        XpLabel {
                            text: root.editor.envelopeUnitNote
                            role: "caption"
                            muted: true
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }

                // Key range and velocity
                XpCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 4
                    Layout.alignment: Qt.AlignTop
                    implicitHeight: rangeColumn.implicitHeight + 2 * Metrics.cardPadding

                    ColumnLayout {
                        id: rangeColumn
                        anchors { left: parent.left; right: parent.right; top: parent.top }
                        spacing: Metrics.spacingSm

                        XpPanelHeader { title: qsTr("KEY RANGE"); glyph: "⌨" }
                        KeyboardStrip {
                            objectName: "keyboardStrip"
                            Layout.fillWidth: true
                            lowerNote: root.editor.keyRangeLower
                            upperNote: root.editor.keyRangeUpper
                            // The window comes from the instrument's own keybed,
                            // widened when the Patch reaches past it; QML does
                            // not decide what the XP-60's keyboard is.
                            firstNote: root.editor.keyboardWindowLower
                            lastNote: root.editor.keyboardWindowUpper
                            velocityLower: root.editor.velocityLower
                            velocityUpper: root.editor.velocityUpper
                            accentColor: Theme.toneColor(root.editor.selectedTone)
                            onRangeEdited: function(lower, upper) {
                                root.editor.keyRangeLower = lower
                                root.editor.keyRangeUpper = upper
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            XpLabel { objectName: "keyLowerText"; text: root.editor.keyRangeLowerText; role: "mono" }
                            Item { Layout.fillWidth: true }
                            XpLabel { text: root.editor.keyRangeUpperText; role: "mono" }
                        }
                        XpLabel {
                            objectName: "keyRangeNote"
                            Layout.fillWidth: true
                            text: root.editor.keyRangeNote
                            role: "caption"
                            muted: !root.editor.keyRangeExceedsKeybed
                            color: root.editor.keyRangeExceedsKeybed ? Theme.warning : Theme.textMuted
                            wrapMode: Text.WordWrap
                        }

                        XpDivider { Layout.fillWidth: true }

                        XpPanelHeader { title: qsTr("VELOCITY"); glyph: "▲" }
                        XpRangeBar {
                            objectName: "velocityRange"
                            Layout.fillWidth: true
                            lower: root.editor.velocityLower
                            upper: root.editor.velocityUpper
                            accentColor: Theme.toneColor(root.editor.selectedTone)
                            onRangeEdited: function(lower, upper) {
                                root.editor.velocityLower = lower
                                root.editor.velocityUpper = upper
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            XpLabel { text: root.editor.velocityLower; role: "mono" }
                            Item { Layout.fillWidth: true }
                            XpLabel { text: root.editor.velocityUpper; role: "mono" }
                        }
                    }
                }

                // Contextual Tone settings
                XpCard {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 3
                    Layout.alignment: Qt.AlignTop
                    implicitHeight: settingsColumn.implicitHeight + 2 * Metrics.cardPadding

                    ColumnLayout {
                        id: settingsColumn
                        anchors { left: parent.left; right: parent.right; top: parent.top }
                        spacing: Metrics.spacingSm

                        XpPanelHeader { title: qsTr("TONE SETTINGS"); glyph: "⚙" }
                        Repeater {
                            model: root.editor.toneSettings
                            delegate: ParameterValueEditor {
                                required property var modelData
                                Layout.fillWidth: true
                                label: modelData.name
                                labelWidth: 96
                                value: modelData.raw
                                displayText: modelData.valueText
                                minimumValue: modelData.minimum
                                maximumValue: modelData.maximum
                                onEdited: function(v) { root.editor.setToneSetting(modelData.parameterId, v) }
                            }
                        }
                    }
                }
            }
        }
    }
}
