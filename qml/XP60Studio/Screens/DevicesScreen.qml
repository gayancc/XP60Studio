import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Devices / Setup.
//
// Restructured around what the musician is actually doing, rather than into
// two arbitrary columns:
//
//   1. connect            — full width, because its signal path is horizontal
//   2. read / send        — the two things you do once connected, side by side
//   3. diagnostics        — everything technical, behind one disclosure
//
// The previous 5:7 two-column split gave the *primary* task the narrower
// column (475 px against 665 px at 1440), squeezed the MIDI path diagram
// inside it, and left roughly 475x180 px of dead space below it while the
// wider column carried a mostly empty "Send to XP-60" card. It also stacked
// only below 1200 px, which meant the two columns were still being crammed at
// widths where their content did not fit.
Item {
    id: root

    required property DevicesViewModel devices
    required property AppShellViewModel shell
    readonly property bool demoMode: shell.demoMode

    // Both work columns can hold their content, or they stack. The threshold
    // is computed from the columns' real minima instead of being guessed.
    readonly property bool twoColumns: width >= Metrics.devicesTwoColumnMinWidth
    readonly property int margin: Metrics.screenMargin(width)

    QQC.ScrollView {
        id: scroller
        objectName: "devicesScroll"
        anchors.fill: parent
        contentWidth: availableWidth
        QQC.ScrollBar.vertical: XpScrollBar { parent: scroller; x: scroller.width - width; height: scroller.availableHeight }
        QQC.ScrollBar.horizontal.policy: QQC.ScrollBar.AlwaysOff

        ColumnLayout {
            id: page
            width: scroller.availableWidth
            // Stretch to the viewport when the content is shorter than it, so
            // the monitor at the bottom absorbs the slack. Without this a
            // settled connection left a 200 px dead band across the window.
            height: Math.max(implicitHeight, scroller.availableHeight)
            spacing: Metrics.spacingLg

            ScreenHeader {
                Layout.fillWidth: true
                Layout.leftMargin: root.margin
                Layout.rightMargin: root.margin
                Layout.topMargin: root.margin
                // The global header already names the screen.
                title: ""
                subtitle: root.devices.connectionVerified
                    ? ""
                    : root.demoMode
                      ? qsTr("Demo Mode — explore connection, fetch and transfers against a simulated XP-60.")
                      : qsTr("Connect your XP-60, verify communication, and read the current sound.")
                XpButton {
                    visible: !root.devices.connectionVerified && !root.demoMode
                    text: qsTr("Scan for MIDI devices")
                    iconName: "refresh"
                    onClicked: root.devices.refreshEndpoints()
                }
                XpButton {
                    visible: root.demoMode
                    text: root.shell.demoModeSwitchPending ? qsTr("Switching…") : qsTr("Use my XP-60")
                    variant: "secondary"
                    enabled: !root.shell.demoModeSwitchPending
                    onClicked: root.shell.exitDemoMode()
                }
            }

            // Consumer entry to Demo Mode — no flags or env vars required.
            XpCard {
                visible: !root.demoMode
                Layout.fillWidth: true
                Layout.leftMargin: root.margin
                Layout.rightMargin: root.margin
                implicitHeight: demoInvite.implicitHeight + 2 * Metrics.cardPadding

                RowLayout {
                    id: demoInvite
                    anchors {
                        left: parent.left; right: parent.right; top: parent.top
                        margins: Metrics.cardPadding
                    }
                    spacing: Metrics.spacingMd

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingXs

                        XpLabel {
                            text: qsTr("Try without hardware")
                            role: "subtitle"
                            font.weight: Typography.weightMedium
                        }
                        XpLabel {
                            Layout.fillWidth: true
                            text: qsTr("Explore the editor and device workflows with a simulated XP-60. Nothing is sent to real MIDI ports.")
                            role: "caption"
                            muted: true
                            wrapMode: Text.WordWrap
                        }
                    }

                    XpButton {
                        objectName: "enterDemoModeButton"
                        text: root.shell.demoModeSwitchPending ? qsTr("Starting…") : qsTr("Enter Demo Mode")
                        variant: "primary"
                        enabled: !root.shell.demoModeSwitchPending
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: root.shell.enterDemoMode()
                    }
                }
            }

            // 1 — Connect. One row, not a banner: the state, the ports, the
            // one action available now, and progress while something is in
            // flight.
            DeviceConnectionBar {
                devices: root.devices
                demoMode: root.demoMode
                Layout.fillWidth: true
                Layout.leftMargin: root.margin
                Layout.rightMargin: root.margin
            }

            // 2 — Read and send, side by side.
            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: root.margin
                Layout.rightMargin: root.margin
                columns: root.twoColumns ? 2 : 1
                columnSpacing: Metrics.spacingLg
                rowSpacing: Metrics.spacingLg

                CurrentSoundPanel {
                    devices: root.devices
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    Layout.minimumWidth: Metrics.devicesConnectionMinWidth
                }

                SendPanel {
                    devices: root.devices
                    visible: root.devices.writeSupported
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    Layout.minimumWidth: Metrics.devicesDiagnosticsMinWidth
                }
            }

            // 3 — The monitor. Always visible, and it takes whatever height
            // the panels above did not use.
            ProtocolActivityPanel {
                devices: root.devices
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 200
                Layout.leftMargin: root.margin
                Layout.rightMargin: root.margin
            }

            // 4 — Counters and the raw request tool, behind one disclosure.
            XpCard {
                Layout.fillWidth: true
                Layout.leftMargin: root.margin
                Layout.rightMargin: root.margin
                Layout.bottomMargin: root.margin
                implicitHeight: advancedColumn.implicitHeight + 2 * Metrics.cardPadding

                ColumnLayout {
                    id: advancedColumn
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    spacing: Metrics.spacingLg

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingSm
                        XpIcon {
                            name: advancedToggle.checked ? "chevron-down" : "chevron-right"
                            color: Theme.textMuted
                        }
                        XpLabel {
                            text: qsTr("Diagnostics")
                            role: "overline"
                            secondary: true
                        }
                        StatusPill {
                            text: root.devices.sysExHealthText
                            tone: root.devices.sysExHealthTone
                        }
                        Item { Layout.fillWidth: true }
                        XpSwitch {
                            id: advancedToggle
                            objectName: "advancedToggle"
                            checked: false
                            Accessible.name: qsTr("Show diagnostics")
                        }
                    }

                    ColumnLayout {
                        visible: advancedToggle.checked
                        Layout.fillWidth: true
                        spacing: Metrics.spacingLg

                        DeviceHealthCard {
                            devices: root.devices
                            Layout.fillWidth: true
                        }

                        SafeReadFlow {
                            devices: root.devices
                            Layout.fillWidth: true
                        }

                        // Operations: a section, not another card inside a
                        // card inside a card.
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Metrics.spacingSm

                            XpPanelHeader {
                                title: qsTr("Operations")
                                XpLabel {
                                    text: root.devices.operations.count === 1
                                          ? qsTr("1 operation")
                                          : qsTr("%1 operations").arg(root.devices.operations.count)
                                    role: "caption"
                                    muted: true
                                }
                            }

                            XpSection {
                                visible: root.devices.operations.count === 0
                                Layout.fillWidth: true
                                sunken: true
                                XpEmptyState {
                                    iconName: "activity"
                                    title: qsTr("No operations yet")
                                    message: qsTr("Every probe, read and transfer appears here with its progress and its outcome.")
                                    Layout.fillWidth: true
                                }
                            }

                            ListView {
                                visible: count > 0
                                Layout.fillWidth: true
                                implicitHeight: Math.min(contentHeight, 360)
                                clip: true
                                model: root.devices.operations
                                spacing: Metrics.spacingSm
                                QQC.ScrollBar.vertical: XpScrollBar {}
                                delegate: OperationProgressRow {
                                    required property var model
                                    width: ListView.view.width
                                    operation: model
                                }
                            }
                        }

                    }
                }
            }
        }
    }

    // ---------------------------------------------------------------------
    // Panels local to this screen. Each was previously inline in the column,
    // which is how "CURRENT SOUND" ended up printed twice in one card and how
    // the arming block became a bordered rectangle inside a bordered card.
    // ---------------------------------------------------------------------

    component CurrentSoundPanel: XpCard {
        id: soundPanel
        required property DevicesViewModel devices

        implicitHeight: soundColumn.implicitHeight + 2 * Metrics.cardPadding

        ColumnLayout {
            id: soundColumn
            anchors { left: parent.left; right: parent.right; top: parent.top }
            spacing: Metrics.spacingMd

            XpPanelHeader {
                title: qsTr("Current sound")
                iconName: "library"
                StatusPill {
                    objectName: "patchFetchPill"
                    text: soundPanel.devices.patchFetchStateText
                    tone: soundPanel.devices.patchFetchTone
                    pulsing: soundPanel.devices.patchFetchInProgress
                }
                StatusPill { text: qsTr("Read only"); tone: "neutral"; showDot: false }
            }

            // The patch identity, when there is one. No second "CURRENT SOUND"
            // overline: the panel header already said it.
            XpSection {
                visible: soundPanel.devices.currentPatchAvailable
                Layout.fillWidth: true

                XpLabel {
                    objectName: "currentPatchName"
                    text: soundPanel.devices.currentPatchName
                    role: "title"
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                XpLabel {
                    text: soundPanel.devices.currentPatchSummary
                    role: "caption"
                    secondary: true
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                XpLabel {
                    visible: soundPanel.devices.currentPatchDecodeReport.length > 0
                    text: soundPanel.devices.currentPatchDecodeReport
                    role: "caption"
                    color: Theme.warning
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            XpEmptyState {
                visible: !soundPanel.devices.currentPatchAvailable
                         && !soundPanel.devices.patchFetchInProgress
                iconName: "library"
                title: qsTr("No sound read yet")
                message: soundPanel.devices.canFetchPatch
                    ? qsTr("Reading fetches the Patch the XP-60 currently has in its temporary area. Nothing on the instrument is changed.")
                    : qsTr("Connect and verify the XP-60 first. Reading never changes anything on the instrument.")
                Layout.fillWidth: true
            }

            RowLayout {
                spacing: Metrics.spacingSm
                Layout.fillWidth: true
                XpButton {
                    objectName: "fetchPatchButton"
                    text: qsTr("Read current sound")
                    variant: "primary"
                    enabled: soundPanel.devices.canFetchPatch
                    onClicked: soundPanel.devices.fetchCurrentPatch()
                }
                XpButton {
                    text: qsTr("Cancel")
                    visible: soundPanel.devices.patchFetchInProgress
                    onClicked: soundPanel.devices.cancelPatchFetch()
                }
                Item { Layout.fillWidth: true }
                XpButton {
                    visible: soundPanel.devices.patchParameters.count > 0
                    text: paramDump.checked ? qsTr("Hide raw parameters") : qsTr("Raw parameters")
                    iconName: paramDump.checked ? "chevron-up" : "chevron-down"
                    variant: "quiet"
                    compact: true
                    onClicked: paramDump.checked = !paramDump.checked
                }
            }

            // Progress. A determinate bar rather than a percentage that
            // appears and disappears next to the button and shifts the row.
            ColumnLayout {
                visible: soundPanel.devices.patchFetchInProgress
                Layout.fillWidth: true
                spacing: Metrics.spacingXs
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 4
                    radius: 2
                    color: Theme.surfaceSunken
                    Rectangle {
                        height: parent.height
                        radius: parent.radius
                        color: Theme.accent
                        width: soundPanel.devices.patchFetchTotalBlocks > 0
                               ? parent.width * soundPanel.devices.patchFetchCompletedBlocks
                                 / soundPanel.devices.patchFetchTotalBlocks
                               : 0
                        Behavior on width {
                            enabled: !Motion.reducedMotion
                            NumberAnimation { duration: Motion.durationFast }
                        }
                    }
                }
                XpLabel {
                    text: soundPanel.devices.patchFetchTotalBlocks > 0
                          ? qsTr("Block %1 of %2").arg(soundPanel.devices.patchFetchCompletedBlocks)
                                                  .arg(soundPanel.devices.patchFetchTotalBlocks)
                          : qsTr("Starting")
                    role: "caption"
                    muted: true
                }
            }

            // Suppressed once the patch identity is on screen: the section
            // above already carries the same summary, and printing it twice
            // 80 px apart is what made this panel look unedited.
            XpLabel {
                objectName: "patchFetchMessage"
                visible: text.length > 0
                         && (soundPanel.devices.patchFetchTone === "error"
                             || soundPanel.devices.patchFetchInProgress)
                text: soundPanel.devices.patchFetchMessage
                role: "caption"
                color: soundPanel.devices.patchFetchTone === "error" ? Theme.error : Theme.textSecondary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            QQC.Switch { id: paramDump; visible: false; checked: false }

            ListView {
                objectName: "patchParameterList"
                visible: paramDump.checked && soundPanel.devices.patchParameters.count > 0
                Layout.fillWidth: true
                implicitHeight: Math.min(contentHeight, 280)
                clip: true
                model: soundPanel.devices.patchParameters
                QQC.ScrollBar.vertical: XpScrollBar {}
                section.property: "block"
                section.delegate: Rectangle {
                    required property string section
                    width: ListView.view.width
                    implicitHeight: Metrics.controlHeightSm
                    color: Theme.surfaceSunken
                    XpLabel {
                        anchors { left: parent.left; leftMargin: Metrics.spacingSm; verticalCenter: parent.verticalCenter }
                        text: section
                        role: "overline"
                        color: section === "Common" ? Theme.accentText : Theme.toneColor(parseInt(section.substr(5)))
                    }
                }
                delegate: Rectangle {
                    id: paramRow
                    required property var model
                    width: ListView.view.width
                    implicitHeight: Metrics.controlHeightSm
                    color: paramHover.hovered ? Theme.surfaceHover : "transparent"
                    HoverHandler { id: paramHover }
                    RowLayout {
                        anchors { fill: parent; leftMargin: Metrics.spacingSm; rightMargin: Metrics.spacingSm }
                        spacing: Metrics.spacingSm
                        XpLabel { text: paramRow.model.category; role: "caption"; muted: true; Layout.preferredWidth: 100; elide: Text.ElideRight }
                        XpLabel { text: paramRow.model.name; role: "caption"; Layout.fillWidth: true; elide: Text.ElideRight }
                        XpLabel { text: paramRow.model.valueText; role: "mono"; color: paramRow.model.isEnum ? Theme.accentText : Theme.textPrimary }
                        XpLabel { text: paramRow.model.rawValue; role: "mono"; color: Theme.dataDim; Layout.preferredWidth: 36; horizontalAlignment: Text.AlignRight }
                    }
                }
            }
        }
    }

    component SendPanel: XpCard {
        id: sendPanel
        required property DevicesViewModel devices

        implicitHeight: sendColumn.implicitHeight + 2 * Metrics.cardPadding

        ColumnLayout {
            id: sendColumn
            anchors { left: parent.left; right: parent.right; top: parent.top }
            spacing: Metrics.spacingMd

            XpPanelHeader {
                title: qsTr("Send to XP temp")
                iconName: "midi-out"
                StatusPill {
                    objectName: "transferPill"
                    text: sendPanel.devices.transferStateText
                    tone: sendPanel.devices.transferTone
                    pulsing: sendPanel.devices.transferBusy
                }
            }

            // Arming is a state, so the section is tinted while armed — that
            // is what the tone parameter is for, instead of a nested bordered
            // rectangle that changes its border colour.
            XpSection {
                Layout.fillWidth: true
                tone: sendPanel.devices.writeArmed ? "warning" : ""
                sunken: !sendPanel.devices.writeArmed
                title: sendPanel.devices.writeArmed ? qsTr("Armed") : ""

                XpLabel {
                    text: qsTr("Writes to the XP-60 edit buffer only, then reads back to verify. Permanent User memory is not overwritten.")
                    role: "caption"
                    secondary: true
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                XpLabel {
                    objectName: "armBlockedReason"
                    visible: sendPanel.devices.armBlockedReason.length > 0
                    text: sendPanel.devices.armBlockedReason
                    role: "caption"
                    muted: true
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                XpButton {
                    objectName: "armWriteButton"
                    text: sendPanel.devices.writeArmed ? qsTr("Disarm") : qsTr("Enable sending")
                    variant: sendPanel.devices.writeArmed ? "danger" : "secondary"
                    enabled: sendPanel.devices.writeArmed || sendPanel.devices.canArmWrite
                    onClicked: sendPanel.devices.writeArmed ? sendPanel.devices.disarmWrite()
                                                            : sendPanel.devices.armWrite()
                }
            }

            RowLayout {
                spacing: Metrics.spacingSm
                Layout.fillWidth: true
                XpButton {
                    objectName: "writeVerifyButton"
                    text: qsTr("Send to XP temp")
                    variant: "primary"
                    enabled: sendPanel.devices.canWrite
                    onClicked: sendPanel.devices.writeBackAndVerify()
                }
                XpButton {
                    objectName: "restoreSnapshotButton"
                    text: qsTr("Restore snapshot")
                    visible: sendPanel.devices.safetySnapshotName.length > 0
                    enabled: sendPanel.devices.canRestoreSnapshot
                    onClicked: sendPanel.devices.restoreSafetySnapshot()
                }
                XpButton {
                    text: qsTr("Cancel")
                    visible: sendPanel.devices.transferBusy
                    onClicked: sendPanel.devices.cancelTransfer()
                }
                Item { Layout.fillWidth: true }
            }

            XpLabel {
                visible: sendPanel.devices.safetySnapshotName.length > 0
                text: qsTr("Snapshot: %1").arg(sendPanel.devices.safetySnapshotName)
                role: "caption"
                muted: true
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }

            XpLabel {
                objectName: "transferMessage"
                visible: text.length > 0
                text: sendPanel.devices.transferMessage
                role: "caption"
                color: sendPanel.devices.transferTone === "error" ? Theme.error
                     : sendPanel.devices.transferTone === "success" ? Theme.success : Theme.textSecondary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            TransferMismatchCard {
                report: sendPanel.devices.mismatchReport
                Layout.fillWidth: true
            }
        }
    }
}
