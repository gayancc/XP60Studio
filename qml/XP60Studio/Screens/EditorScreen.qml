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
        objectName: "editorScroll"
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

                GridLayout {
                    id: headerRow
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                    columns: root.wideTones ? 2 : 1
                    columnSpacing: Metrics.spacingMd
                    rowSpacing: Metrics.spacingMd

                    RowLayout {
                        Layout.fillWidth: true
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

                    }
                    RowLayout {
                        Layout.alignment: Qt.AlignRight
                        spacing: Metrics.spacingSm

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
                        enabled: root.editor.modified && !root.editor.comparing
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
                    XpButton {
                        objectName: "cancelWriteButton"
                        text: qsTr("Cancel write")
                        visible: root.editor.writeBusy && !root.editor.liveAudition
                        variant: "danger"
                        onClicked: root.editor.cancelWrite()
                    }
                    }
                }
            }

            // Write outcome, only once something has happened.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                spacing: Metrics.spacingSm
                Flow {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingSm
                    XpButton {
                        objectName: "startLiveButton"
                        text: qsTr("Start live audition")
                        visible: !root.editor.liveAudition
                        enabled: root.editor.canStartLiveAudition
                        onClicked: root.editor.startLiveAudition()
                    }
                    XpButton {
                        objectName: "stopLiveButton"
                        text: root.editor.liveStopping ? qsTr("Finishing audition…") : qsTr("Stop & keep B")
                        visible: root.editor.liveAudition
                        enabled: !root.editor.liveStopping
                        onClicked: root.editor.stopLiveAudition()
                    }
                    XpButton {
                        objectName: "restoreAuditionButton"
                        text: qsTr("Restore before audition")
                        visible: root.editor.liveAudition
                        enabled: !root.editor.liveStopping
                        onClicked: root.editor.restoreBeforeAudition()
                    }
                    XpButton {
                        objectName: "cancelAuditionButton"
                        text: qsTr("Stop now")
                        variant: "danger"
                        visible: root.editor.liveAudition
                        onClicked: root.editor.cancelWrite()
                    }
                }
                XpLabel {
                    objectName: "auditionMessage"
                    Layout.fillWidth: true
                    text: root.editor.auditionMessage
                    role: "caption"
                    wrapMode: Text.WordWrap
                    color: root.editor.liveAudition ? Theme.warning : Theme.textSecondary
                }
            }

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
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                XpSegmentedControl {
                    objectName: "disclosureTabs"
                    model: [qsTr("Play"), qsTr("Design"), qsTr("Expert")]
                    currentIndex: root.editor.disclosure
                    onActivated: function(index) { root.editor.disclosure = index }
                }
                XpLabel {
                    Layout.fillWidth: true
                    text: root.editor.liveAudition ? qsTr("Live audition · updates are paced and verified")
                         : root.editor.comparing ? qsTr("Viewing original · editing paused")
                                                : qsTr("Local editing · Write sends to XP-60")
                    role: "caption"
                    secondary: true
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignRight
                }
            }
            XpSegmentedControl {
                objectName: "sectionTabs"
                visible: root.editor.disclosure === 1
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                model: root.editor.sectionNames
                currentIndex: root.editor.section
                onActivated: function(index) { root.editor.section = index }
            }

            // ── Four Tone cards ─────────────────────────────────────────────
            GridLayout {
                id: toneGrid
                objectName: "toneGrid"
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                columns: root.wideTones ? 4 : 2
                columnSpacing: Metrics.spacingMd
                rowSpacing: Metrics.spacingMd

                Repeater {
                    id: toneCards
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

                // The approved composition connects each coloured Tone to
                // Structure. This is an overview, not a fabricated diagram
                // of an individual Structure algorithm. At two-column widths
                // the cards wrap and these overview lines are omitted.
                Canvas {
                    id: toneConnections
                    objectName: "toneConnections"
                    visible: root.wideTones
                    anchors { left: parent.left; right: parent.right }
                    y: -Metrics.cardPadding - Metrics.spacingLg
                    height: Metrics.cardPadding + Metrics.spacingLg
                    onWidthChanged: requestPaint()
                    onVisibleChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        for (var i = 0; i < 4; ++i) {
                            var card = toneCards.itemAt(i)
                            if (!card) continue
                            var start = card.mapToItem(toneConnections, card.width / 2, card.height)
                            var end = structureNode.mapToItem(toneConnections, structureNode.width / 2 + (i - 1.5) * 6, 0)
                            var channel = 5 + i * 5
                            ctx.beginPath()
                            ctx.moveTo(start.x, 0)
                            ctx.bezierCurveTo(start.x, channel, start.x, channel, start.x - 6, channel)
                            ctx.lineTo(end.x + 6, channel)
                            ctx.quadraticCurveTo(end.x, channel, end.x, channel + 6)
                            ctx.lineTo(end.x, height)
                            ctx.strokeStyle = Theme.toneColor(i + 1)
                            ctx.globalAlpha = card.tone.enabled ? 0.65 : 0.2
                            ctx.lineWidth = 1.5
                            ctx.stroke()
                        }
                    }
                    Connections { target: root.editor; function onPatchChanged() { toneConnections.requestPaint() } }
                    Connections { target: toneGrid; function onWidthChanged() { toneConnections.requestPaint() } }
                    Component.onCompleted: Qt.callLater(requestPaint)
                }

                Flow {
                    id: flowRow
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                    spacing: Metrics.spacingSm

                    SignalFlowNode {
                        id: structureNode
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
            XpLabel {
                objectName: "routingSummary"
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                text: root.editor.routingSummary
                role: "caption"
                secondary: true
                wrapMode: Text.WordWrap
            }

            GridLayout {
                objectName: "designDetails"
                visible: root.editor.disclosure === 1 && root.editor.section < 3
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
                            bipolar: root.editor.section === 0 // only Pitch is bipolar
                            accentColor: Theme.toneColor(root.editor.selectedTone)
                            onPointMoved: function(index, x, y) { root.editor.moveEnvelopePoint(index, x, y) }
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
                                        editable: !root.editor.comparing
                                        onEdited: function(v) { root.editor.setEnvelopeStageRaw(modelData.index, false, v) }
                                    }
                                    ParameterValueEditor {
                                        visible: modelData.hasLevel
                                        label: modelData.hasLevel ? modelData.levelLabel : ""
                                        value: modelData.hasLevel ? modelData.levelRaw : 0
                                        displayText: modelData.hasLevel ? modelData.levelText : ""
                                        minimumValue: 0
                                        maximumValue: 127
                                        editable: !root.editor.comparing
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
                    objectName: "keyRangePanel"
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
                        RowLayout {
                            Layout.fillWidth: true
                            ParameterValueEditor {
                                objectName: "keyLowerEntry"
                                Layout.fillWidth: true
                                label: qsTr("Low note")
                                value: root.editor.keyRangeLower
                                minimumValue: 0
                                maximumValue: root.editor.keyRangeUpper
                                editable: !root.editor.comparing
                                onEdited: function(v) { root.editor.keyRangeLower = v }
                            }
                            ParameterValueEditor {
                                objectName: "keyUpperEntry"
                                Layout.fillWidth: true
                                label: qsTr("High note")
                                value: root.editor.keyRangeUpper
                                minimumValue: root.editor.keyRangeLower
                                maximumValue: 127
                                editable: !root.editor.comparing
                                onEdited: function(v) { root.editor.keyRangeUpper = v }
                            }
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
                            ParameterValueEditor {
                                objectName: "velocityLowerEntry"
                                Layout.fillWidth: true
                                label: qsTr("Softest")
                                value: root.editor.velocityLower
                                minimumValue: 1
                                maximumValue: root.editor.velocityUpper
                                editable: !root.editor.comparing
                                onEdited: function(v) { root.editor.velocityLower = v }
                            }
                            ParameterValueEditor {
                                objectName: "velocityUpperEntry"
                                Layout.fillWidth: true
                                label: qsTr("Hardest")
                                value: root.editor.velocityUpper
                                minimumValue: root.editor.velocityLower
                                maximumValue: 127
                                editable: !root.editor.comparing
                                onEdited: function(v) { root.editor.velocityUpper = v }
                            }
                        }
                    }
                }

                // Section-specific synthesis controls for the selected Tone.
                EditorParameterPanel {
                    objectName: "sectionParameterPanel"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 3
                    Layout.alignment: Qt.AlignTop
                    editor: root.editor
                    parameters: root.editor.sectionParameters
                    title: qsTr("TONE %1 · %2").arg(root.editor.selectedTone).arg(root.editor.sectionNames[root.editor.section].toUpperCase())
                }
            }

            EditorParameterPanel {
                objectName: "motionEffectsPanel"
                visible: root.editor.disclosure === 1 && root.editor.section >= 3
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                editor: root.editor
                parameters: root.editor.sectionParameters
                title: root.editor.section === 3 ? qsTr("MOTION · LFO & CONTROLLERS") : qsTr("EFFECTS & ROUTING")
                note: root.editor.section === 4
                      ? qsTr("EFX types follow Roland's 40-effect list. The 12 effect-specific slots remain raw until their byte mappings and units are verified. Chorus and Reverb follow the parameter map.")
                      : qsTr("Both LFOs include shape, rate, delay, fade and Pitch/Filter/Level/Pan depths. Rates and times use XP values; no Hz or seconds conversion is assumed.")
            }

            EditorParameterPanel {
                objectName: "expertParameterPanel"
                visible: root.editor.disclosure === 2
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                Layout.bottomMargin: Metrics.screenPadding
                editor: root.editor
                parameters: root.editor.expertParameters
                expert: true
                title: qsTr("EXPERT · PATCH PARAMETERS")
                note: qsTr("Numeric fields edit raw XP values; menu labels are from the parameter map. EFX type-specific meanings and physical timing units remain unverified.")
            }

            XpCard {
                visible: root.editor.disclosure !== 2
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                Layout.bottomMargin: Metrics.screenPadding
                implicitHeight: playColumn.implicitHeight + 2 * Metrics.cardPadding
                ColumnLayout {
                    id: playColumn
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    XpPanelHeader { title: qsTr("PATCH PLAY SETTINGS") }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: width >= 900 ? 5 : 2
                        columnSpacing: Metrics.spacingLg
                        Repeater {
                            model: root.editor.toneSettings
                            delegate: ParameterValueEditor {
                                required property var modelData
                                Layout.fillWidth: true
                                label: modelData.name
                                labelWidth: 90
                                value: modelData.raw
                                displayText: modelData.valueText
                                minimumValue: modelData.minimum
                                maximumValue: modelData.maximum
                                editable: !root.editor.comparing
                                onEdited: function(v) { root.editor.setToneSetting(modelData.parameterId, v) }
                            }
                        }
                    }
                }
            }
        }
    }
}
