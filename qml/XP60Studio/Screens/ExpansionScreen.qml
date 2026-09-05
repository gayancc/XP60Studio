import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Expansion Manager — what is in this XP-60's four Wave Expansion slots, and
// what that means for the Patch on screen.
//
// ── Why this screen asks rather than detects ─────────────────────────────────
//
// Nothing in the XP-60's protocol reports which boards are fitted, so the
// musician says. The board list makes that quick: pick an SR-JV80 board and its
// wave group is filled in, because a Tone's Wave Group ID is inferred to be the
// board's catalogue number (docs/protocol/ROLAND_XP60_PROTOCOL_FACTS.md §7 sets
// out the evidence, and the fact that Roland documents no such mapping).
//
// The screen is built to make the three honest answers legible: a board is here
// and we know its waves, a board is here and we do not yet, or nothing is here
// at all. **Learn** is the way out of the middle state, and it outranks the
// catalogue — select a wave from the board on the instrument, fetch the Patch,
// and XP60Studio reads the group the instrument actually sent.
FocusScope {
    id: root

    required property ExpansionViewModel expansion

    readonly property bool wide: width >= 1040

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.screenMargin(root.width)
        spacing: Metrics.spacingMd

        // Identity ---------------------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            XpLabel { text: qsTr("EXPANSION"); role: "overline"; color: Theme.accentText }
            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingSm
                XpLabel {
                    text: qsTr("What is in your XP-60")
                    role: "title"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                StatusPill {
                    objectName: "expansionInstalledCount"
                    text: qsTr("%n board(s)", "", root.expansion.installedCount)
                    tone: root.expansion.installedCount > 0 ? "info" : "neutral"
                    showDot: false
                }
                StatusPill {
                    objectName: "expansionUnknownWarning"
                    visible: root.expansion.anyGroupUnknown
                    text: qsTr("GROUP UNKNOWN")
                    tone: "warning"
                }
            }
            XpLabel {
                objectName: "expansionAdvice"
                Layout.fillWidth: true
                text: root.expansion.advice
                role: "caption"
                muted: true
                wrapMode: Text.WordWrap
            }
        }

        // The four slots ---------------------------------------------------
        GridLayout {
            Layout.fillWidth: true
            columns: root.wide ? 2 : 1
            columnSpacing: Metrics.spacingMd
            rowSpacing: Metrics.spacingSm

            Repeater {
                model: root.expansion.slots
                delegate: XpCard {
                    id: slotCard
                    required property var modelData
                    Layout.fillWidth: true
                    padding: Metrics.spacingMd

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Metrics.spacingXs

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Metrics.spacingSm
                            XpLabel {
                                text: slotCard.modelData.label
                                role: "overline"
                                color: slotCard.modelData.installed ? Theme.accentText : Theme.textSecondary
                            }
                            Item { Layout.fillWidth: true }
                            // The three states, said plainly. "Group unknown" is
                            // not a failure — it is the honest middle, and it is
                            // what holds every "board missing" verdict back.
                            StatusPill {
                                objectName: "expansionSlotState" + slotCard.modelData.slot
                                text: !slotCard.modelData.installed ? qsTr("EMPTY")
                                      : slotCard.modelData.groupKnown ? qsTr("GROUP %1").arg(slotCard.modelData.groupId)
                                                                      : qsTr("GROUP UNKNOWN")
                                tone: !slotCard.modelData.installed ? "neutral"
                                      : slotCard.modelData.groupKnown ? "success" : "warning"
                                showDot: false
                            }
                        }

                        // Pick a real SR-JV80 board and its wave group is filled
                        // in with it. The free-text field below still works for
                        // a board that is not in Roland's catalogue, or for a
                        // musician who calls theirs something else.
                        XpComboBox {
                            objectName: "expansionSlotBoard" + slotCard.modelData.slot
                            Layout.fillWidth: true
                            Accessible.name: qsTr("Board in %1").arg(slotCard.modelData.label)
                            textRole: "name"
                            model: root.expansion.knownBoards
                            currentIndex: -1
                            displayText: slotCard.modelData.installed ? slotCard.modelData.name
                                                                      : qsTr("Choose an SR-JV80 board…")
                            onActivated: function (index) {
                                root.expansion.declareBoard(slotCard.modelData.slot,
                                                            root.expansion.knownBoards[index].number)
                            }
                        }

                        XpTextField {
                            objectName: "expansionSlotName" + slotCard.modelData.slot
                            Layout.fillWidth: true
                            text: slotCard.modelData.name
                            placeholderText: qsTr("…or name it yourself")
                            onEditingFinished: root.expansion.setBoard(slotCard.modelData.slot, text,
                                                                      slotCard.modelData.groupId)
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Metrics.spacingSm
                            visible: slotCard.modelData.installed

                            XpLabel { text: qsTr("Wave group"); role: "caption"; muted: true }
                            XpSpinField {
                                objectName: "expansionSlotGroup" + slotCard.modelData.slot
                                from: 0
                                to: 127
                                value: slotCard.modelData.groupKnown ? slotCard.modelData.groupId : 0
                                onValueModified: root.expansion.setWaveGroup(slotCard.modelData.slot, value)
                            }
                            // The way out of "unknown" without anybody guessing:
                            // read the group out of a Patch made on the
                            // instrument with this board's wave in it.
                            XpButton {
                                objectName: "expansionLearn" + slotCard.modelData.slot
                                text: qsTr("Learn")
                                compact: true
                                variant: slotCard.modelData.groupKnown ? "ghost" : "primary"
                                QQC.ToolTip.visible: hovered
                                QQC.ToolTip.delay: 400
                                QQC.ToolTip.text: root.expansion.learnAdvice(slotCard.modelData.slot)
                                onClicked: root.expansion.learnFromCurrentPatch(slotCard.modelData.slot)
                            }
                            Item { Layout.fillWidth: true }
                            XpButton {
                                objectName: "expansionClear" + slotCard.modelData.slot
                                text: qsTr("Empty")
                                compact: true
                                variant: "ghost"
                                onClicked: root.expansion.clearSlot(slotCard.modelData.slot)
                            }
                        }
                    }
                }
            }
        }

        // The Patch on screen ----------------------------------------------
        XpCard {
            objectName: "expansionCurrentPatch"
            Layout.fillWidth: true
            padding: Metrics.spacingMd
            visible: Object.keys(root.expansion.currentPatch).length > 0

            ColumnLayout {
                anchors.fill: parent
                spacing: Metrics.spacingXs

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingSm
                    XpLabel { text: qsTr("THE PATCH YOU ARE EDITING"); role: "overline"; muted: true }
                    Item { Layout.fillWidth: true }
                    StatusPill {
                        objectName: "expansionPatchVerdict"
                        text: !root.expansion.currentPatch.usesExpansion ? qsTr("INTERNAL ONLY")
                              : root.expansion.currentPatch.undecided ? qsTr("CANNOT TELL")
                              : root.expansion.currentPatch.playable ? qsTr("PLAYS HERE")
                                                                     : qsTr("BOARD MISSING")
                        tone: !root.expansion.currentPatch.usesExpansion ? "neutral"
                              : root.expansion.currentPatch.undecided ? "warning"
                              : root.expansion.currentPatch.playable ? "success" : "error"
                    }
                }

                XpLabel {
                    objectName: "expansionPatchSummary"
                    Layout.fillWidth: true
                    text: root.expansion.currentPatch.summary ?? ""
                    role: "caption"
                    muted: true
                    wrapMode: Text.WordWrap
                }

                // Per-Tone, because a Patch is four Tones and only some of them
                // may need a board the musician does not have.
                Repeater {
                    model: root.expansion.currentPatch.tones ?? []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: Metrics.spacingSm

                        XpLabel {
                            text: qsTr("Tone %1").arg(modelData.toneNumber)
                            role: "caption"
                            color: modelData.enabled ? Theme.textPrimary : Theme.textDisabled
                        }
                        StatusPill {
                            text: modelData.label
                            tone: modelData.tone
                            showDot: false
                        }
                        XpLabel {
                            visible: modelData.groupId >= 0
                            text: modelData.slot > 0
                                  ? qsTr("%1 · %2").arg(root.expansion.describeGroup(modelData.groupId)).arg(modelData.slotLabel)
                                  : root.expansion.describeGroup(modelData.groupId)
                            role: "caption"
                            muted: true
                        }
                        XpLabel {
                            visible: !modelData.enabled
                            text: qsTr("switched off")
                            role: "caption"
                            muted: true
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }

        XpEmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: Object.keys(root.expansion.currentPatch).length === 0
            title: qsTr("No Patch open")
            message: qsTr("Open a Patch from the Library, a Bank destination, or the XP-60, and this screen will say whether it plays on your instrument.")
        }

        Item { Layout.fillHeight: true; visible: Object.keys(root.expansion.currentPatch).length > 0 }
    }
}
