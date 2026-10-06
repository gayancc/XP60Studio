import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Patch Editor / Four-Tone Mixer — mockup panel M2.
//
// Compact desktop mixer layout, top to bottom:
//   1. identity + transport (one toolbar)
//   2. section / disclosure
//   3. four Tone cards
//   4. tone-scoped work (envelope · ranges · section params) — next to the
//      Tone that owns it, not below a patch-level routing diagram
//   5. patch-level signal path
//   6. disclosure-specific surfaces (Play intelligence, Expert list, Effects)
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
    // Every module in the Design grid is this tall, and each has one element
    // that absorbs the remainder: the envelope graph, the parameter grid, the
    // keybed, the glide track. The figure is set by the tallest content in the
    // set — Tone Settings stacked — so nothing clips and nothing is left over.
    readonly property int designModuleHeight: 352
    readonly property int editorMargin: Metrics.screenMargin(width)

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
            spacing: Metrics.spacingSm

            // ── Patch toolbar ───────────────────────────────────────────────
            // One dense row: identity left, transport right. The old card spent
            // a badge, a title-size name and a separate action row on what a
            // mixer header can say in 32 px.
            XpCard {
                Layout.fillWidth: true
                Layout.leftMargin: root.editorMargin
                Layout.rightMargin: root.editorMargin
                Layout.topMargin: root.editorMargin
                padding: Metrics.spacingSm
                implicitHeight: headerColumn.implicitHeight + 2 * Metrics.spacingSm

                ColumnLayout {
                    id: headerColumn
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    spacing: Metrics.spacingXs

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingSm

                        XpButton {
                            id: browseWavesButton
                            objectName: "browseWavesButton"
                            iconName: "search"
                            iconOnly: true
                            compact: true
                            Accessible.name: qsTr("Browse waves")
                            QQC.ToolTip.visible: hovered
                            QQC.ToolTip.delay: 400
                            QQC.ToolTip.text: qsTr("Browse waves")
                            onClicked: root.browsingWaves = true
                        }

                        ColumnLayout {
                            spacing: 0
                            Layout.fillWidth: true
                            Layout.minimumWidth: 120
                            RowLayout {
                                spacing: Metrics.spacingXs
                                Layout.fillWidth: true
                                XpLabel {
                                    objectName: "patchNameLabel"
                                    text: root.editor.patchName
                                    role: "heading"
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
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
                                objectName: "patchContextLine"
                                text: root.editor.deviceMessage.length > 0
                                      ? root.editor.deviceMessage
                                      : root.editor.locationText + " · "
                                        + (root.editor.differenceSummary.length > 0
                                           ? root.editor.differenceSummary + " · " : "")
                                        + root.editor.sourceText
                                role: "caption"
                                muted: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // Transport always wraps: at preferred width it stays one
                    // line; at shell-minimum width Arm / Live / Send stay inside
                    // the window instead of overflowing off the right edge.
                    Flow {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingXs

                        XpButton {
                            objectName: "compareButton"
                            text: root.editor.comparing ? qsTr("A · Original") : qsTr("B · Current")
                            iconName: "compare"
                            compact: true
                            variant: root.editor.comparing ? "primary" : "ghost"
                            onClicked: root.editor.comparing = !root.editor.comparing
                        }
                        XpButton {
                            objectName: "undoButton"
                            iconName: "undo"; iconOnly: true; compact: true; variant: "ghost"
                            enabled: root.editor.canUndo
                            onClicked: root.editor.undo()
                            Accessible.name: qsTr("Undo")
                        }
                        XpButton {
                            objectName: "redoButton"
                            iconName: "redo"; iconOnly: true; compact: true; variant: "ghost"
                            enabled: root.editor.canRedo
                            onClicked: root.editor.redo()
                            Accessible.name: qsTr("Redo")
                        }
                        XpButton {
                            objectName: "revertButton"
                            text: qsTr("Revert"); compact: true; variant: "ghost"
                            enabled: root.editor.modified && !root.editor.comparing
                            onClicked: root.editor.revertToOriginal()
                        }
                        XpButton {
                            objectName: "startLiveButton"
                            text: qsTr("Live")
                            compact: true
                            variant: "ghost"
                            visible: !root.editor.liveAudition
                            enabled: root.editor.canStartLiveAudition
                            onClicked: root.editor.startLiveAudition()
                        }
                        XpButton {
                            objectName: "armWriteButton"
                            text: root.editor.writeArmed ? qsTr("Armed") : qsTr("Arm")
                            compact: true
                            variant: root.editor.writeArmed ? "danger" : "ghost"
                            enabled: root.editor.writeArmed || root.editor.canArmWrite
                            onClicked: root.editor.writeArmed ? root.editor.disarmWrite() : root.editor.armWrite()
                        }
                        XpButton {
                            objectName: "writeToDeviceButton"
                            text: qsTr("Send to XP temp")
                            compact: true
                            variant: "primary"
                            enabled: root.editor.canWrite
                            QQC.ToolTip.visible: hovered
                            QQC.ToolTip.delay: 400
                            QQC.ToolTip.text: qsTr("Sends this Patch to the XP-60's temporary area and reads it back to verify it. You will hear it immediately. Nothing in the XP-60's USER memory is changed, and the temporary Patch is lost when the instrument selects another Patch or is powered off.")
                            onClicked: root.editor.writeToDevice()
                        }
                        XpButton {
                            objectName: "cancelWriteButton"
                            text: qsTr("Cancel")
                            compact: true
                            visible: root.editor.writeBusy && !root.editor.liveAudition
                            variant: "danger"
                            onClicked: root.editor.cancelWrite()
                        }
                    }
                }
            }

            // Live audition controls — only while an audition is running.
            Flow {
                Layout.fillWidth: true
                Layout.leftMargin: root.editorMargin
                Layout.rightMargin: root.editorMargin
                spacing: Metrics.spacingXs
                visible: root.editor.liveAudition
                XpButton {
                    objectName: "stopLiveButton"
                    text: root.editor.liveStopping ? qsTr("Finishing audition…") : qsTr("Stop & keep B")
                    compact: true
                    enabled: !root.editor.liveStopping
                    onClicked: root.editor.stopLiveAudition()
                }
                XpButton {
                    objectName: "restoreAuditionButton"
                    text: qsTr("Restore before audition")
                    compact: true
                    variant: "ghost"
                    enabled: !root.editor.liveStopping
                    onClicked: root.editor.restoreBeforeAudition()
                }
                XpButton {
                    objectName: "cancelAuditionButton"
                    text: qsTr("Stop now")
                    variant: "danger"
                    compact: true
                    onClicked: root.editor.cancelWrite()
                }
                XpLabel {
                    objectName: "auditionMessage"
                    text: root.editor.auditionMessage
                    role: "caption"
                    color: Theme.warning
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
                Layout.leftMargin: root.editorMargin
                Layout.rightMargin: root.editorMargin
            }

            // Section and disclosure share one toolbar.
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: root.editorMargin
                Layout.rightMargin: root.editorMargin
                spacing: Metrics.spacingMd
                XpSegmentedControl {
                    objectName: "sectionTabs"
                    visible: root.editor.disclosure === 1
                    Layout.fillWidth: true
                    model: root.editor.sectionNames
                    currentIndex: root.editor.section
                    onActivated: function(index) { root.editor.section = index }
                }
                Item { Layout.fillWidth: root.editor.disclosure !== 1 }
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
                Layout.leftMargin: root.editorMargin
                Layout.rightMargin: root.editorMargin
                onBrowseWavesRequested: function(toneNumber) {
                    root.editor.selectedTone = toneNumber
                    root.browsingWaves = true
                }
            }

            // Motion / LFO — under the mixer, same priority as envelope work.
            XpCard {
                objectName: "motionEffectsPanel"
                visible: root.editor.disclosure === 1 && root.editor.section === 3
                Layout.fillWidth: true
                Layout.leftMargin: root.editorMargin
                Layout.rightMargin: root.editorMargin
                padding: Metrics.spacingSm
                implicitHeight: motionColumn.implicitHeight + 2 * Metrics.spacingSm
                accentColor: Theme.toneColor(root.editor.selectedTone)

                ColumnLayout {
                    id: motionColumn
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    spacing: Metrics.spacingSm

                    XpModuleHeader {
                        title: qsTr("MOTION · LFO")
                        iconName: "wave"
                        accentColor: Theme.toneColor(root.editor.selectedTone)
                        trailing: StatusPill {
                            text: qsTr("Tone %1").arg(root.editor.selectedTone)
                            tone: "accent"
                            showDot: false
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: root.wideBottom ? 2 : 1
                        columnSpacing: Metrics.spacingMd
                        rowSpacing: Metrics.spacingMd

                        LfoLane {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            editor: root.editor
                            parameters: root.editor.sectionParameters
                            lfo: "lfo1"
                            title: qsTr("LFO 1")
                            accentColor: Theme.toneColor(root.editor.selectedTone)
                        }
                        LfoLane {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            editor: root.editor
                            parameters: root.editor.sectionParameters
                            lfo: "lfo2"
                            title: qsTr("LFO 2")
                            accentColor: Theme.toneColor(root.editor.selectedTone)
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

            EffectsWorkbench {
                objectName: "effectsWorkbench"
                visible: root.editor.disclosure === 1 && root.editor.section === 4
                Layout.fillWidth: true
                Layout.leftMargin: root.editorMargin
                Layout.rightMargin: root.editorMargin
                editor: root.editor
            }

            // ── Tone-scoped Design work ────────────────────────────
            // One row of three modules — what the Tone sounds like, and how it
            // answers the keyboard — then its parameter grid beneath. Envelope
            // and Key Range belong to SOUND/FILTER/AMP; Tone Settings is common
            // to every section that shapes a Tone, so it stays for MOTION too
            // and simply takes the whole row when it is alone in it.
            GridLayout {
                objectName: "designDetails"
                visible: root.editor.disclosure === 1 && root.editor.section !== 4
                Layout.fillWidth: true
                Layout.leftMargin: root.editorMargin
                Layout.rightMargin: root.editorMargin
                // Two bands of two modules, each module sized so its own
                // content fills it. A full-width panel holding four knobs
                // spreads them 250 px apart and reads as a mistake; two ~590 px
                // modules per band let every control sit at a fixed pitch with
                // one consistent gutter, which is how a dense instrument packs.
                columns: width >= 680 ? 2 : 1
                columnSpacing: Metrics.spacingSm
                rowSpacing: Metrics.spacingSm

                XpCard {
                    visible: root.editor.section < 3
                    Layout.fillWidth: true
                    Layout.preferredWidth: 6
                    Layout.preferredHeight: root.designModuleHeight
                    padding: Metrics.spacingSm

                    ColumnLayout {
                        id: envColumn
                        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom }
                        spacing: Metrics.spacingXs

                        XpModuleHeader {
                            objectName: "envelopeHeader"
                            title: root.editor.envelopeTitle
                            iconName: "envelope"
                            accentColor: Theme.toneColor(root.editor.selectedTone)
                            trailing: XpLabel {
                                text: qsTr("XP values 0-127")
                                role: "caption"
                                muted: true
                                QQC.ToolTip.text: root.editor.envelopeUnitNote
                                QQC.ToolTip.visible: envNoteHover.hovered
                                QQC.ToolTip.delay: 300
                                HoverHandler { id: envNoteHover }
                            }
                        }

                        EnvelopeEditor {
                            objectName: "envelopeEditor"
                            visible: root.editor.envelopeAvailable
                            Layout.fillWidth: true
                            // Absorbs the card's remaining height.
                            Layout.fillHeight: true
                            Layout.minimumHeight: 120
                            points: root.editor.envelopePoints
                            bipolar: root.editor.section === 0
                            accentColor: Theme.toneColor(root.editor.selectedTone)
                            onPointMoved: function(index, x, y) { root.editor.moveEnvelopePoint(index, x, y) }
                        }

                        // Explicit cell widths, not a GridLayout. A layout
                        // nested in a layout inherits its children's maximum
                        // width, and the knob caption caps that at ~96 px — so
                        // however much room the card offered, the four stages
                        // stayed huddled at its left edge. Dividing the width
                        // by the stage count is unambiguous, and it puts each
                        // stage's knobs under that stage of the graph above.
                        Item {
                            objectName: "envelopeStages"
                            visible: root.editor.envelopeAvailable
                            Layout.fillWidth: true
                            Layout.preferredHeight: stageRow.implicitHeight

                            Row {
                                id: stageRow
                                anchors { left: parent.left; right: parent.right; top: parent.top }
                                readonly property int count: root.editor.envelopeStages.length
                                readonly property real cell: count > 0 ? width / count : width

                                Repeater {
                                    // Count, not the list: envelopeStages is a
                                    // QVariantList that notifies on every edit,
                                    // and a Repeater given the list resets its
                                    // model each time, destroying the very knob
                                    // the pointer is holding.
                                    model: stageRow.count
                                    delegate: Item {
                                        id: stage
                                        required property int index
                                        readonly property var modelData: root.editor.envelopeStages[index]
                                        readonly property bool present: modelData !== undefined
                                        readonly property bool hasLevel: present && modelData.hasLevel
                                        width: stageRow.cell
                                        height: stageColumn.implicitHeight

                                        ColumnLayout {
                                            id: stageColumn
                                            anchors { left: parent.left; right: parent.right; top: parent.top }
                                            spacing: Metrics.spacingXs

                                            MusicalParamKnob {
                                                objectName: "envelopeTime" + (stage.index + 1)
                                                visible: stage.present
                                                Layout.alignment: Qt.AlignHCenter
                                                labelWidth: stageRow.cell - Metrics.spacingSm
                                                label: stage.present ? stage.modelData.timeLabel : ""
                                                value: stage.present ? stage.modelData.timeRaw : 0
                                                from: 0
                                                to: 127
                                                knobSize: Metrics.knobSm
                                                accentColor: Theme.toneColor(root.editor.selectedTone)
                                                editable: !root.editor.comparing
                                                onEdited: function(v) { root.editor.setEnvelopeStageRaw(stage.index, false, v) }
                                            }
                                            MusicalParamKnob {
                                                objectName: "envelopeLevel" + (stage.index + 1)
                                                visible: stage.hasLevel
                                                Layout.alignment: Qt.AlignHCenter
                                                labelWidth: stageRow.cell - Metrics.spacingSm
                                                label: stage.hasLevel ? stage.modelData.levelLabel : ""
                                                value: stage.hasLevel ? stage.modelData.levelRaw : 0
                                                displayText: stage.hasLevel ? stage.modelData.levelText : ""
                                                from: 0
                                                to: root.editor.section === 0 ? 126 : 127
                                                bipolar: root.editor.section === 0
                                                knobSize: Metrics.knobSm
                                                accentColor: Theme.toneColor(root.editor.selectedTone)
                                                editable: !root.editor.comparing
                                                onEdited: function(v) { root.editor.setEnvelopeStageRaw(stage.index, true, v) }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Two lines of standing prose under every envelope was
                        // costing the graph its height. The same sentence now
                        // rides on the module header, where it is one hover away
                        // and never in the way of the controls.
                        Item { Layout.fillHeight: true; Layout.minimumHeight: 0 }
                    }
                }

                EditorParameterPanel {
                    objectName: "sectionParameterPanel"
                    visible: root.editor.section < 3
                    Layout.fillWidth: true
                    Layout.preferredWidth: 6
                    Layout.preferredHeight: root.designModuleHeight
                    editor: root.editor
                    parameters: root.editor.sectionParameters
                    title: qsTr("TONE %1 · %2").arg(root.editor.selectedTone).arg(root.editor.sectionNames[root.editor.section].toUpperCase())
                }

                XpCard {
                    objectName: "keyRangePanel"
                    visible: root.editor.section < 4
                    Layout.fillWidth: true
                    Layout.preferredWidth: 6
                    Layout.preferredHeight: root.designModuleHeight
                    padding: Metrics.spacingSm

                    ColumnLayout {
                        id: rangeColumn
                        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom }
                        spacing: Metrics.spacingXs

                        XpModuleHeader {
                            title: qsTr("KEY RANGE")
                            iconName: "keyboard"
                            accentColor: Theme.toneColor(root.editor.selectedTone)
                            trailing: XpLabel {
                                text: qsTr("%1 – %2").arg(root.editor.keyRangeLowerText).arg(root.editor.keyRangeUpperText)
                                role: "mono"
                                color: Theme.toneColor(root.editor.selectedTone)
                            }
                        }
                        KeyboardStrip {
                            objectName: "keyboardStrip"
                            Layout.fillWidth: true
                            // Absorbs this card's remainder, within reason: all
                            // 128 keys in half the window is already a narrow
                            // white key, and over-stretching makes a moire.
                            Layout.fillHeight: true
                            Layout.minimumHeight: 96
                            Layout.maximumHeight: 150
                            lowerNote: root.editor.keyRangeLower
                            upperNote: root.editor.keyRangeUpper
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
                            Layout.fillHeight: false
                            spacing: Metrics.spacingSm
                            Item { Layout.fillWidth: true }
                            XpButton {
                                objectName: "keyRangeExactToggle"
                                text: keyExact.checked ? qsTr("Hide exact") : qsTr("Exact notes")
                                compact: true
                                variant: "ghost"
                                onClicked: keyExact.checked = !keyExact.checked
                            }
                        }
                        QQC.Switch { id: keyExact; visible: false; checked: false }
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: false
                            spacing: Metrics.spacingSm
                            ParameterValueEditor {
                                objectName: "keyLowerEntry"
                                visible: keyExact.checked
                                Layout.fillWidth: true
                                label: qsTr("Low")
                                value: root.editor.keyRangeLower
                                displayText: root.editor.keyRangeLowerText
                                minimumValue: 0
                                maximumValue: root.editor.keyRangeUpper
                                editable: !root.editor.comparing
                                onEdited: function(v) { root.editor.keyRangeLower = v }
                            }
                            ParameterValueEditor {
                                objectName: "keyUpperEntry"
                                visible: keyExact.checked
                                Layout.fillWidth: true
                                label: qsTr("High")
                                value: root.editor.keyRangeUpper
                                displayText: root.editor.keyRangeUpperText
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

                        XpModuleHeader {
                            title: qsTr("VELOCITY")
                            iconName: "activity"
                            accentColor: Theme.toneColor(root.editor.selectedTone)
                            trailing: XpLabel {
                                text: qsTr("%1 – %2").arg(root.editor.velocityLower).arg(root.editor.velocityUpper)
                                role: "mono"
                                color: Theme.toneColor(root.editor.selectedTone)
                            }
                        }
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
                            Layout.fillHeight: false
                            spacing: Metrics.spacingSm
                            XpLabel {
                                text: qsTr("Soft · Hard")
                                role: "caption"
                                muted: true
                            }
                            Item { Layout.fillWidth: true }
                            XpButton {
                                objectName: "velocityExactToggle"
                                text: velExact.checked ? qsTr("Hide exact") : qsTr("Exact velocity")
                                compact: true
                                variant: "ghost"
                                onClicked: velExact.checked = !velExact.checked
                            }
                        }
                        QQC.Switch { id: velExact; visible: false; checked: false }
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: false
                            spacing: Metrics.spacingSm
                            ParameterValueEditor {
                                objectName: "velocityLowerEntry"
                                visible: velExact.checked
                                Layout.fillWidth: true
                                label: qsTr("Soft")
                                value: root.editor.velocityLower
                                minimumValue: 1
                                maximumValue: root.editor.velocityUpper
                                editable: !root.editor.comparing
                                onEdited: function(v) { root.editor.velocityLower = v }
                            }
                            ParameterValueEditor {
                                objectName: "velocityUpperEntry"
                                visible: velExact.checked
                                Layout.fillWidth: true
                                label: qsTr("Hard")
                                value: root.editor.velocityUpper
                                minimumValue: root.editor.velocityLower
                                maximumValue: 127
                                editable: !root.editor.comparing
                                onEdited: function(v) { root.editor.velocityUpper = v }
                            }
                        }

                        ToneKeyAssign {
                            objectName: "toneKeyAssign"
                            Layout.fillWidth: true
                            Layout.topMargin: Metrics.spacingXs
                            editor: root.editor
                            accentColor: Theme.toneColor(root.editor.selectedTone)
                        }

                    }
                }

                // Common to every Tone-shaping section, so it is the one
                // module in this row that MOTION keeps.
                TonePlaySettingsStrip {
                    objectName: "tonePlaySettings"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 6
                    Layout.preferredHeight: root.designModuleHeight
                    editor: root.editor
                    accentColor: Theme.toneColor(root.editor.selectedTone)
                }

            }

            // ── Patch-level signal path ─────────────────────────────────────
            // After Tone work: the path is about where the mix goes, not about
            // shaping the selected Tone.
            XpCard {
                Layout.fillWidth: true
                Layout.leftMargin: root.editorMargin
                Layout.rightMargin: root.editorMargin
                Layout.bottomMargin: root.editorMargin
                padding: Metrics.spacingSm
                implicitHeight: routingView.implicitHeight + 2 * Metrics.spacingSm

                EffectsCanvas {
                    id: routingView
                    objectName: "effectsCanvas"
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    editor: root.editor
                }
            }

            XpLabel {
                objectName: "routingSummary"
                Layout.fillWidth: true
                Layout.leftMargin: root.editorMargin
                Layout.rightMargin: root.editorMargin
                Layout.bottomMargin: root.editorMargin
                text: root.editor.routingSummary
                role: "caption"
                secondary: true
                wrapMode: Text.WordWrap
            }

            EditorParameterPanel {
                objectName: "expertParameterPanel"
                visible: root.editor.disclosure === 2
                Layout.fillWidth: true
                Layout.leftMargin: root.editorMargin
                Layout.rightMargin: root.editorMargin
                Layout.bottomMargin: root.editorMargin
                editor: root.editor
                parameters: root.editor.expertParameters
                expert: true
                title: qsTr("EXPERT · PATCH PARAMETERS")
                note: qsTr("Numeric fields edit raw XP values; menu labels are from the parameter map. EFX type-specific meanings and physical timing units remain unverified.")
            }

            SoundDnaPanel {
                objectName: "soundDnaPanel"
                visible: root.editor.disclosure === 0
                Layout.fillWidth: true
                Layout.leftMargin: root.editorMargin
                Layout.rightMargin: root.editorMargin
                editor: root.editor
            }

            // Play: exact Tone settings only. The four Tone cards already are
            // the mix surface — a second row of mini meters was redundant.
            XpCard {
                visible: root.editor.disclosure === 0
                Layout.fillWidth: true
                Layout.leftMargin: root.editorMargin
                Layout.rightMargin: root.editorMargin
                Layout.bottomMargin: root.editorMargin
                padding: Metrics.spacingSm
                implicitHeight: playColumn.implicitHeight + 2 * Metrics.spacingSm
                ColumnLayout {
                    id: playColumn
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    spacing: Metrics.spacingXs
                    XpModuleHeader {
                        title: qsTr("PLAY")
                        iconName: "performance"
                        accentColor: Theme.accent
                        trailing: StatusPill { text: qsTr("Audition & Tone mix"); tone: "info"; showDot: false }
                    }
                    XpLabel {
                        Layout.fillWidth: true
                        text: qsTr("Shape the sound with the four Tone cards above. Use Design for envelopes, ranges and LFO; Expert for every documented value.")
                        role: "caption"
                        secondary: true
                        wrapMode: Text.WordWrap
                    }
                    XpButton {
                        text: playExact.checked ? qsTr("Hide Tone play settings") : qsTr("Tone play settings")
                        variant: "ghost"
                        compact: true
                        onClicked: playExact.checked = !playExact.checked
                    }
                    QQC.Switch { id: playExact; visible: false; checked: false }
                    TonePlaySettingsStrip {
                        visible: playExact.checked
                        Layout.fillWidth: true
                        editor: root.editor
                        accentColor: Theme.toneColor(root.editor.selectedTone)
                    }
                }
            }
        }
    }
}
