import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Patch Editor / Four-Tone Mixer — mockup panel M2.
//
// Composition follows the master mockup: patch header with Send to XP temp,
// section tabs, four Tone cards, the signal path, and a bottom row of
// envelope editor, key/velocity ranges and contextual Tone settings.
Item {
    id: root

    required property PatchEditorViewModel editor
    // Optional — see WaveBrowserScreen.expansion.
    property var expansion: null
    property bool browsingWaves: false

    WaveBrowserScreen {
        anchors.fill: parent
        visible: root.browsingWaves
        catalog: root.editor.waves
        editor: root.editor
        expansion: root.expansion
        onWaveUsed: root.browsingWaves = false
        onClosed: {
            root.browsingWaves = false
            browseWavesButton.forceActiveFocus()
        }
    }

    // Below this the four Tone cards would be too narrow to read, so they
    // wrap to two rows of two rather than shrinking.
    readonly property bool wideTones: width >= 1120
    readonly property bool wideBottom: width >= 1180

    XpEmptyState {
        anchors.fill: parent
        visible: !root.editor.hasPatch && !root.browsingWaves
        iconName: "editor"
        title: qsTr("No Patch loaded")
        message: root.editor.emptyStateMessage
    }

    QQC.ScrollView {
        id: scroller
        objectName: "editorScroll"
        anchors.fill: parent
        visible: root.editor.hasPatch && !root.browsingWaves
        contentWidth: availableWidth
        QQC.ScrollBar.vertical: XpScrollBar { parent: scroller; x: scroller.width - width; height: scroller.availableHeight }
        QQC.ScrollBar.horizontal.policy: QQC.ScrollBar.AlwaysOff

        ColumnLayout {
            width: scroller.availableWidth
            spacing: Metrics.spacingMd

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

                    XpButton {
                        id: browseWavesButton
                        objectName: "browseWavesButton"
                        text: qsTr("Waves")
                        iconName: "search"
                        onClicked: {
                            root.browsingWaves = true
                        }
                    }

                    // A badge, not a button. This was a rounded rectangle with
                    // the same fill, border, radius and height as the "Waves"
                    // button beside it, so a static label read as the second
                    // half of a pair of controls.
                    StatusPill {
                        text: qsTr("Patch editor")
                        tone: "accent"
                        showDot: false
                    }

                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        RowLayout {
                            spacing: Metrics.spacingSm
                            XpLabel { text: root.editor.locationText; role: "overline"; secondary: true }
                            // Two indicators, never one. "Is my work kept?" and
                            // "does the XP-60 hold what I am looking at?" are
                            // independent questions — a Patch can be saved here
                            // and absent from the instrument, or auditioning on
                            // the instrument and never saved. One badge
                            // answering both is how an editor ends up saying
                            // "saved" when it means "sent".
                            StatusPill {
                                objectName: "patchStateBadge"
                                visible: root.editor.studioBadgeText.length > 0
                                text: root.editor.studioBadgeText
                                tone: root.editor.studioBadgeTone
                                showDot: false
                            }
                            StatusPill {
                                objectName: "patchDeviceBadge"
                                visible: root.editor.deviceBadgeText.length > 0
                                text: root.editor.deviceBadgeText
                                tone: root.editor.deviceBadgeTone
                                showDot: false
                            }
                        }
                        XpLabel {
                            objectName: "patchNameLabel"
                            text: root.editor.patchName
                            role: "title"
                            Layout.fillWidth: true
                        }
                        XpLabel {
                            objectName: "patchContextLine"
                            // The device message earns the line when there is
                            // one: "the XP-60 selected another Patch" is the
                            // thing a musician most needs to read, and it says
                            // why the badge beside it went quiet.
                            text: root.editor.deviceMessage.length > 0
                                  ? root.editor.deviceMessage
                                  : root.editor.differenceSummary + " · " + root.editor.sourceText
                            role: "caption"
                            muted: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    }
                    RowLayout {
                        Layout.alignment: Qt.AlignRight
                        spacing: Metrics.spacingXs

                    // A/B original vs current
                    XpButton {
                        objectName: "compareButton"
                        text: root.editor.comparing ? qsTr("A · Original") : qsTr("B · Current")
                        iconName: "compare"
                        compact: true
                        variant: root.editor.comparing ? "primary" : "secondary"
                        onClicked: root.editor.comparing = !root.editor.comparing
                    }
                    XpButton {
                        objectName: "undoButton"
                        iconName: "undo"; compact: true; variant: "ghost"
                        enabled: root.editor.canUndo
                        onClicked: root.editor.undo()
                        Accessible.name: qsTr("Undo")
                    }
                    XpButton {
                        objectName: "redoButton"
                        iconName: "redo"; compact: true; variant: "ghost"
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
                    // "Write" is the word the Owner's Manual reserves for the
                    // operation that overwrites a stored USER Patch (p.46).
                    // This button sends to the temporary area instead, which is
                    // what the instrument plays from and what it discards on the
                    // next Patch change (p.45) -- so calling it "Write to XP-60"
                    // told a musician who had read the manual that their stored
                    // sound had just been overwritten. It had not. The name now
                    // says which memory it reaches.
                    XpButton {
                        objectName: "writeToDeviceButton"
                        text: qsTr("Send to XP temp")
                        variant: "primary"
                        enabled: root.editor.canWrite
                        QQC.ToolTip.visible: hovered
                        QQC.ToolTip.delay: 400
                        QQC.ToolTip.text: qsTr("Sends this Patch to the XP-60's temporary area and reads it back to verify it. You will hear it immediately. Nothing in the XP-60's USER memory is changed, and the temporary Patch is lost when the instrument selects another Patch or is powered off.")
                        onClicked: root.editor.writeToDevice()
                    }
                    XpButton {
                        objectName: "cancelWriteButton"
                        text: qsTr("Cancel send")
                        visible: root.editor.writeBusy && !root.editor.liveAudition
                        variant: "danger"
                        onClicked: root.editor.cancelWrite()
                    }
                    }
                }
            }

            // Write outcome, only once something has happened.
            GridLayout {
                columns: root.wideTones && !root.editor.liveAudition ? 2 : 1
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                columnSpacing: Metrics.spacingMd
                rowSpacing: Metrics.spacingSm
                Flow {
                    Layout.fillWidth: !root.wideTones || root.editor.liveAudition
                    Layout.preferredWidth: root.wideTones && !root.editor.liveAudition ? startLive.implicitWidth : -1
                    spacing: Metrics.spacingSm
                    XpButton {
                        id: startLive
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

            // Section and disclosure share one compact toolbar at desktop width.
            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                columns: root.wideTones ? 2 : 1
                columnSpacing: Metrics.spacingXl
                rowSpacing: Metrics.spacingSm
                XpSegmentedControl {
                    objectName: "sectionTabs"
                    visible: root.editor.disclosure === 1
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    model: root.editor.sectionNames
                    currentIndex: root.editor.section
                    onActivated: function(index) { root.editor.section = index }
                }
                XpSegmentedControl {
                    objectName: "disclosureTabs"
                    Layout.alignment: Qt.AlignRight
                    model: [qsTr("Play"), qsTr("Design"), qsTr("Expert")]
                    currentIndex: root.editor.disclosure
                    onActivated: function(index) { root.editor.disclosure = index }
                }
            }

            // ── Four Tone cards ─────────────────────────────────────────────
            FourToneMixer {
                id: toneGrid
                editor: root.editor
                wide: root.wideTones
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                onBrowseWavesRequested: function(toneNumber) {
                    root.editor.selectedTone = toneNumber
                    root.browsingWaves = true
                }
            }

            // ── Signal path ─────────────────────────────────────────────────
            XpCard {
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                implicitHeight: routingView.implicitHeight + 2 * Metrics.cardPadding

                // Connect only the selected Tone/Structure pair. At two-column
                // widths the cards wrap, so the source is identified in text.
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
                            if ((root.editor.routing.sourceTones || []).indexOf(i + 1) < 0) continue
                            var card = toneGrid.itemAt(i)
                            if (!card) continue
                            var start = card.mapToItem(toneConnections, card.width / 2, card.height)
                            var end = routingView.mapToItem(toneConnections, routingView.sourceCenterX + (i - 1.5) * 6, 0)
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

                EffectsCanvas {
                    id: routingView
                    objectName: "effectsCanvas"
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    editor: root.editor
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
                            iconName: "envelope"
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

                        XpPanelHeader { title: qsTr("KEY RANGE"); iconName: "keyboard" }
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

            EffectsWorkbench {
                objectName: "effectsWorkbench"
                visible: root.editor.disclosure === 1 && root.editor.section === 4
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                editor: root.editor
            }

            // Motion / LFO — visual shape first, exact panel secondary
            XpCard {
                objectName: "motionEffectsPanel"
                visible: root.editor.disclosure === 1 && root.editor.section === 3
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                implicitHeight: motionColumn.implicitHeight + 2 * Metrics.cardPadding
                accentColor: Theme.toneColor(root.editor.selectedTone)

                ColumnLayout {
                    id: motionColumn
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    spacing: Metrics.spacingMd

                    XpPanelHeader {
                        title: qsTr("MOTION · LFO")
                        StatusPill {
                            text: qsTr("Tone %1").arg(root.editor.selectedTone)
                            tone: "accent"
                            showDot: false
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: root.wideBottom ? 2 : 1
                        columnSpacing: Metrics.spacingLg
                        rowSpacing: Metrics.spacingMd

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Metrics.spacingSm
                            XpLabel { text: qsTr("LFO 1 SHAPE"); role: "overline"; secondary: true }
                            LfoShapeSelector {
                                Layout.fillWidth: true
                                shapes: {
                                    var _ = root.editor.sectionParameters.valuesTick
                                    return root.editor.sectionParameters.choicesForId("tone.lfo1_waveform")
                                }
                                currentRaw: {
                                    var _ = root.editor.sectionParameters.valuesTick
                                    return root.editor.sectionParameters.rawForId("tone.lfo1_waveform")
                                }
                                accentColor: Theme.toneColor(root.editor.selectedTone)
                                editable: !root.editor.comparing
                                onShapeSelected: function(raw) {
                                    root.editor.sectionParameters.edit("tone.lfo1_waveform", root.editor.selectedTone, raw)
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Metrics.spacingMd
                                ColumnLayout {
                                    XpLabel { text: qsTr("Rate"); role: "label"; secondary: true }
                                    XpKnob {
                                        from: 0; to: 127
                                        value: {
                                            var _ = root.editor.sectionParameters.valuesTick
                                            return root.editor.sectionParameters.rawForId("tone.lfo1_rate")
                                        }
                                        accentColor: Theme.toneColor(root.editor.selectedTone)
                                        onMoved: root.editor.sectionParameters.edit("tone.lfo1_rate", root.editor.selectedTone, Math.round(value))
                                    }
                                    XpLabel {
                                        text: {
                                            var _ = root.editor.sectionParameters.valuesTick
                                            return root.editor.sectionParameters.valueTextForId("tone.lfo1_rate")
                                        }
                                        role: "mono"
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }
                                ColumnLayout {
                                    XpLabel { text: qsTr("Delay"); role: "label"; secondary: true }
                                    XpKnob {
                                        from: 0; to: 127
                                        value: {
                                            var _ = root.editor.sectionParameters.valuesTick
                                            return root.editor.sectionParameters.rawForId("tone.lfo1_delay_time")
                                        }
                                        accentColor: Theme.toneColor(root.editor.selectedTone)
                                        onMoved: root.editor.sectionParameters.edit("tone.lfo1_delay_time", root.editor.selectedTone, Math.round(value))
                                    }
                                    XpLabel {
                                        text: {
                                            var _ = root.editor.sectionParameters.valuesTick
                                            return root.editor.sectionParameters.valueTextForId("tone.lfo1_delay_time")
                                        }
                                        role: "mono"
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }
                                Item { Layout.fillWidth: true }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Metrics.spacingSm
                            XpLabel { text: qsTr("LFO 2 SHAPE"); role: "overline"; secondary: true }
                            LfoShapeSelector {
                                Layout.fillWidth: true
                                shapes: {
                                    var _ = root.editor.sectionParameters.valuesTick
                                    return root.editor.sectionParameters.choicesForId("tone.lfo2_waveform")
                                }
                                currentRaw: {
                                    var _ = root.editor.sectionParameters.valuesTick
                                    return root.editor.sectionParameters.rawForId("tone.lfo2_waveform")
                                }
                                accentColor: Theme.toneColor(root.editor.selectedTone)
                                editable: !root.editor.comparing
                                onShapeSelected: function(raw) {
                                    root.editor.sectionParameters.edit("tone.lfo2_waveform", root.editor.selectedTone, raw)
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Metrics.spacingMd
                                ColumnLayout {
                                    XpLabel { text: qsTr("Rate"); role: "label"; secondary: true }
                                    XpKnob {
                                        from: 0; to: 127
                                        value: {
                                            var _ = root.editor.sectionParameters.valuesTick
                                            return root.editor.sectionParameters.rawForId("tone.lfo2_rate")
                                        }
                                        accentColor: Theme.toneColor(root.editor.selectedTone)
                                        onMoved: root.editor.sectionParameters.edit("tone.lfo2_rate", root.editor.selectedTone, Math.round(value))
                                    }
                                    XpLabel {
                                        text: {
                                            var _ = root.editor.sectionParameters.valuesTick
                                            return root.editor.sectionParameters.valueTextForId("tone.lfo2_rate")
                                        }
                                        role: "mono"
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }
                                ColumnLayout {
                                    XpLabel { text: qsTr("Delay"); role: "label"; secondary: true }
                                    XpKnob {
                                        from: 0; to: 127
                                        value: {
                                            var _ = root.editor.sectionParameters.valuesTick
                                            return root.editor.sectionParameters.rawForId("tone.lfo2_delay_time")
                                        }
                                        accentColor: Theme.toneColor(root.editor.selectedTone)
                                        onMoved: root.editor.sectionParameters.edit("tone.lfo2_delay_time", root.editor.selectedTone, Math.round(value))
                                    }
                                    XpLabel {
                                        text: {
                                            var _ = root.editor.sectionParameters.valuesTick
                                            return root.editor.sectionParameters.valueTextForId("tone.lfo2_delay_time")
                                        }
                                        role: "mono"
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }
                                Item { Layout.fillWidth: true }
                            }
                        }
                    }

                    XpButton {
                        text: motionExact.checked ? qsTr("Hide exact LFO parameters") : qsTr("Exact LFO & controller values")
                        variant: "ghost"
                        compact: true
                        onClicked: motionExact.checked = !motionExact.checked
                    }
                    QQC.Switch { id: motionExact; visible: false; checked: false }

                    EditorParameterPanel {
                        visible: motionExact.checked
                        Layout.fillWidth: true
                        editor: root.editor
                        parameters: root.editor.sectionParameters
                        title: qsTr("EXACT MOTION PARAMETERS")
                        note: qsTr("Both LFOs include shape, rate, delay, fade and Pitch/Filter/Level/Pan depths. Rates and times use XP values; no Hz or seconds conversion is assumed.")
                    }
                }
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

            // Play mode: musical summary — exact patch commons stay behind disclosure
            XpCard {
                visible: root.editor.disclosure === 0
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                Layout.bottomMargin: Metrics.screenPadding
                implicitHeight: playColumn.implicitHeight + 2 * Metrics.cardPadding
                ColumnLayout {
                    id: playColumn
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    spacing: Metrics.spacingMd
                    XpPanelHeader {
                        title: qsTr("PLAY")
                        StatusPill { text: qsTr("Audition & Tone mix"); tone: "info"; showDot: false }
                    }
                    XpLabel {
                        Layout.fillWidth: true
                        text: qsTr("Shape the sound with the four Tone cards above. Use Design for envelopes, ranges and LFO; Expert for every documented value.")
                        role: "caption"
                        secondary: true
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingMd
                        Repeater {
                            model: root.editor.tones
                            delegate: Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: 48
                                radius: Metrics.radiusSm
                                color: Theme.toneBackground("neutral")
                                border.width: 1
                                border.color: Qt.rgba(Theme.toneColor(modelData.toneNumber).r, Theme.toneColor(modelData.toneNumber).g, Theme.toneColor(modelData.toneNumber).b, 0.5)
                                ColumnLayout {
                                    anchors { fill: parent; margins: Metrics.spacingSm }
                                    spacing: 2
                                    XpLabel {
                                        text: qsTr("T%1").arg(modelData.toneNumber)
                                        role: "overline"
                                        color: Theme.toneColor(modelData.toneNumber)
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: 8
                                        radius: 2
                                        color: Theme.surfaceSunken
                                        Rectangle {
                                            width: parent.width * (modelData.enabled ? modelData.level / 127 : 0)
                                            height: parent.height
                                            radius: 2
                                            color: Theme.toneColor(modelData.toneNumber)
                                            opacity: modelData.audible ? 1 : 0.35
                                        }
                                    }
                                }
                            }
                        }
                    }
                    XpButton {
                        text: playExact.checked ? qsTr("Hide exact play settings") : qsTr("Exact Tone play settings")
                        variant: "ghost"
                        compact: true
                        onClicked: playExact.checked = !playExact.checked
                    }
                    QQC.Switch { id: playExact; visible: false; checked: false }
                    GridLayout {
                        visible: playExact.checked
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

            // Design/Expert still expose exact Tone settings when not in Play
            XpCard {
                visible: root.editor.disclosure === 1 && root.editor.section !== 4
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                Layout.bottomMargin: Metrics.screenPadding
                implicitHeight: designPlayColumn.implicitHeight + 2 * Metrics.cardPadding
                ColumnLayout {
                    id: designPlayColumn
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    XpPanelHeader { title: qsTr("TONE PLAY SETTINGS") }
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
