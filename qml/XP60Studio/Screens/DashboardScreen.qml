import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Dashboard / Command Center — mockup panel M1.
//
// A command center, not an analytics dashboard: what the XP-60 is holding right
// now, what the four Tones are contributing to it, and the few actions worth
// taking from here. Composition follows the mockup — hero patch and Tone
// contribution on the left, summary cards down the right, quick actions beneath
// the hero.
//
// Where it departs from the mockup it is because the mockup shows illustrative
// data this application cannot honestly produce yet, which AGENTS.md allows and
// the roadmap requires:
//
//   the mockup's category tags (Warm, Wide, Layered) have no source — the XP-60
//   Parameter Address Map defines no category byte — so the hero shows facts
//   the Patch actually carries instead;
//
//   its Tone meters read in dB, and the manual gives no raw-to-dB conversion,
//   so levels are shown as the XP values they are;
//
//   its expansion and bank counts are not knowable yet, so those cards say so
//   rather than displaying a number;
//
//   Save As and Add to Bank have nowhere to go until their phases, and the
//   roadmap forbids fake actions, so the quick actions are the ones that work.
FocusScope {
    id: root

    required property var shell
    required property var editor
    required property var dashboard
    required property var devices

    readonly property bool wide: width >= 1180
    readonly property bool hasPatch: editor.hasPatch

    onVisibleChanged: if (visible && dashboard) dashboard.refresh()

    QQC.ScrollView {
        id: scroller
        objectName: "dashboardScroll"
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroller.availableWidth
            height: Math.max(implicitHeight, scroller.availableHeight)
            spacing: Metrics.spacingMd

            // ── Identity ──────────────────────────────────────────────────
            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: Metrics.screenPadding
                Layout.bottomMargin: 0
                spacing: 2
                XpLabel { text: qsTr("DASHBOARD"); role: "overline"; color: Theme.accentText }
                XpLabel {
                    text: qsTr("Your XP-60, at a glance")
                    role: "title"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                Layout.bottomMargin: Metrics.screenPadding
                columns: root.wide ? 2 : 1
                columnSpacing: Metrics.spacingMd
                rowSpacing: Metrics.spacingMd

                // ── Hero: current patch, Tones, quick actions ─────────────
                XpCard {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    // Stretch factors only divide surplus space, and the hero
                    // content is wide enough that there is none: without a
                    // preferred width it takes the row and squeezes the summary
                    // column off the screen.
                    Layout.preferredWidth: 1
                    Layout.horizontalStretchFactor: 3
                    Layout.fillHeight: true
                    Layout.minimumHeight: hero.implicitHeight + 2 * Metrics.cardPadding
                    implicitHeight: hero.implicitHeight + 2 * Metrics.cardPadding

                    ColumnLayout {
                        id: hero
                        anchors {
                            left: parent.left; right: parent.right; top: parent.top
                            margins: Metrics.cardPadding
                        }
                        spacing: Metrics.spacingMd

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Metrics.spacingSm
                            XpLabel { text: qsTr("CURRENT PATCH"); role: "overline"; secondary: true }
                            Item { Layout.fillWidth: true }
                            StatusPill {
                                objectName: "dashboardPatchState"
                                visible: root.hasPatch && root.editor.stateBadgeText.length > 0
                                text: root.editor.stateBadgeText
                                tone: root.editor.stateBadgeTone
                                showDot: false
                            }
                        }

                        XpLabel {
                            objectName: "dashboardPatchName"
                            Layout.fillWidth: true
                            text: root.hasPatch ? root.editor.patchName : qsTr("No patch loaded")
                            role: "display"
                            elide: Text.ElideRight
                        }
                        XpLabel {
                            objectName: "dashboardPatchLocation"
                            Layout.fillWidth: true
                            text: root.hasPatch
                                  ? root.editor.locationText + " · " + root.editor.sourceText
                                  : qsTr("Read a Patch from the Devices screen to begin")
                            role: "caption"
                            secondary: true
                            elide: Text.ElideRight
                        }

                        // Facts the Patch actually carries. The mockup's mood
                        // tags would have to be invented: the XP-60 stores no
                        // category, and guessing one from the sound would be a
                        // claim the instrument never made.
                        Flow {
                            Layout.fillWidth: true
                            visible: root.hasPatch
                            spacing: Metrics.spacingXs
                            StatusPill {
                                text: qsTr("Structure %1").arg(root.editor.structureText)
                                tone: "neutral"; showDot: false
                            }
                            StatusPill {
                                text: qsTr("%n Tone(s) on", "", root.editor.enabledToneCount)
                                tone: "info"; showDot: false
                            }
                            StatusPill { text: root.editor.mfxText; tone: "accent"; showDot: false }
                            StatusPill { text: root.editor.outputText; tone: "neutral"; showDot: false }
                        }

                        XpDivider { Layout.fillWidth: true; visible: root.hasPatch }

                        // ── Four-Tone contribution ───────────────────────
                        XpLabel {
                            text: qsTr("FOUR-TONE CONTRIBUTION")
                            role: "overline"
                            secondary: true
                            visible: root.hasPatch
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            visible: root.hasPatch
                            columns: root.width >= 720 ? 4 : 2
                            columnSpacing: Metrics.spacingSm
                            rowSpacing: Metrics.spacingSm

                            Repeater {
                                model: root.editor.tones
                                delegate: DashboardToneCard {
                                    required property var modelData
                                    required property int index
                                    objectName: "dashboardTone" + (index + 1)
                                    Layout.fillWidth: true
                                    tone: modelData
                                    toneNumber: index + 1
                                    // Straight to this Tone in the Editor: the
                                    // Dashboard is a way in, not a dead end.
                                    onOpenRequested: {
                                        root.editor.selectedTone = index + 1
                                        root.shell.currentScreen = "editor"
                                    }
                                }
                            }
                        }

                        XpDivider { Layout.fillWidth: true }

                        // ── Quick actions ────────────────────────────────
                        XpLabel { text: qsTr("QUICK ACTIONS"); role: "overline"; secondary: true }
                        Flow {
                            Layout.fillWidth: true
                            spacing: Metrics.spacingSm

                            XpButton {
                                objectName: "quickEditPatch"
                                text: qsTr("Edit Patch")
                                iconName: "editor"
                                variant: "primary"
                                enabled: root.hasPatch
                                onClicked: root.shell.currentScreen = "editor"
                            }
                            XpButton {
                                objectName: "quickCompare"
                                // The Compare screen is a later phase; the A/B
                                // comparison in the Editor is real today, so the
                                // action says which one it is.
                                text: qsTr("Compare A/B")
                                iconName: "compare"
                                enabled: root.hasPatch
                                onClicked: {
                                    root.editor.comparing = true
                                    root.shell.currentScreen = "editor"
                                }
                            }
                            XpButton {
                                objectName: "quickBrowseWaves"
                                text: qsTr("Browse Waves")
                                iconName: "search"
                                enabled: root.hasPatch
                                onClicked: root.shell.currentScreen = "editor"
                            }
                            XpButton {
                                objectName: "quickDevices"
                                text: qsTr("Devices")
                                iconName: "devices"
                                onClicked: root.shell.currentScreen = "devices"
                            }
                        }
                        XpLabel {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            role: "caption"
                            muted: true
                            text: qsTr("Save As and Add to Bank arrive with the Bank Builder.")
                        }
                    }
                }

                // ── Summary cards ────────────────────────────────────────
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: 1
                    Layout.horizontalStretchFactor: 2
                    // Below this the cards stop being readable, so the grid
                    // drops to one column rather than crushing them.
                    Layout.minimumWidth: 250
                    Layout.fillHeight: true
                    spacing: Metrics.spacingMd

                    DashboardSummaryCard {
                        objectName: "libraryCard"
                        Layout.fillWidth: true
                        title: qsTr("LIBRARY")
                        iconName: "library"
                        accentColor: Theme.accent
                        available: root.dashboard.libraryAvailable
                        unavailableText: qsTr("The local library could not be opened.")
                        actionText: qsTr("Browse & search sounds")
                        metrics: [
                            { "value": root.dashboard.libraryPatchCount, "label": qsTr("Patches") },
                            { "value": root.dashboard.waveCount, "label": qsTr("Waves") },
                            // Which expansion boards are fitted is not
                            // established, so this reports the gap rather than
                            // a number that would be invented.
                            { "value": "—", "label": qsTr("Expansions") }
                        ]
                        onActivated: root.shell.currentScreen = "library"
                    }

                    DashboardSummaryCard {
                        objectName: "banksCard"
                        Layout.fillWidth: true
                        title: qsTr("BANK BUILDER")
                        iconName: "banks"
                        accentColor: Theme.tone2
                        available: root.dashboard.banksAvailable
                        unavailableText: qsTr("Bank building arrives in a later phase. Nothing is counted yet.")
                        actionText: qsTr("Build & organize live banks")
                        metrics: []
                    }

                    DashboardSummaryCard {
                        objectName: "deviceCard"
                        Layout.fillWidth: true
                        title: qsTr("DEVICE DIAGNOSTICS")
                        iconName: "activity"
                        accentColor: root.shell.connectionNeedsAttention ? Theme.warning : Theme.success
                        available: true
                        actionText: qsTr("Check device health & data")
                        headline: root.shell.connectionLabel
                        detail: root.shell.connectionDetail
                        onActivated: root.shell.currentScreen = "devices"
                    }

                    // Holds the cards to the top; the column itself fills the
                    // frame so the two sides end level.
                    Item { Layout.fillWidth: true; Layout.fillHeight: true }
                }
            }
        }
    }
}
