import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Devices / Setup — musician-first connection and health; diagnostics under Advanced.
Item {
    id: root

    required property DevicesViewModel devices

    readonly property bool twoColumns: width >= Metrics.devicesTwoColumnMinWidth

    QQC.ScrollView {
        id: scroller
        objectName: "devicesScroll"
        anchors.fill: parent
        contentWidth: availableWidth
        QQC.ScrollBar.vertical: XpScrollBar { parent: scroller; x: scroller.width - width; height: scroller.availableHeight }
        QQC.ScrollBar.horizontal.policy: QQC.ScrollBar.AlwaysOff

        ColumnLayout {
            width: scroller.availableWidth
            spacing: Metrics.spacingLg

            ScreenHeader {
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                Layout.topMargin: Metrics.screenPadding
                title: ""
                subtitle: "Connect your XP-60, verify communication, and read the current sound."
                XpButton { text: "Scan for MIDI devices"; onClicked: root.devices.refreshEndpoints() }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                Layout.bottomMargin: Metrics.screenPadding
                columns: root.twoColumns ? 2 : 1
                columnSpacing: Metrics.spacingLg
                rowSpacing: Metrics.spacingLg

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: root.twoColumns ? 5 : 1
                    spacing: Metrics.spacingLg

                    DeviceConnectionCard {
                        devices: root.devices
                        Layout.fillWidth: true
                    }

                    XpCard {
                        Layout.fillWidth: true
                        implicitHeight: advancedWrapper.implicitHeight + 2 * Metrics.cardPadding

                        ColumnLayout {
                            id: advancedWrapper
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            spacing: Metrics.spacingSm

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Metrics.spacingSm
                                XpLabel { text: advancedToggle.checked ? "▾" : "▸"; role: "body"; color: Theme.textMuted }
                                XpLabel { text: "Advanced diagnostics"; role: "overline"; secondary: true; Layout.fillWidth: true }
                                QQC.Switch {
                                    id: advancedToggle
                                    objectName: "advancedToggle"
                                    checked: false
                                }
                            }

                            ColumnLayout {
                                visible: advancedToggle.checked
                                Layout.fillWidth: true
                                spacing: Metrics.spacingLg

                                SafeReadFlow {
                                    devices: root.devices
                                    Layout.fillWidth: true
                                }

                                XpCard {
                                    Layout.fillWidth: true
                                    implicitHeight: operationsColumn.implicitHeight + 2 * Metrics.cardPadding

                                    ColumnLayout {
                                        id: operationsColumn
                                        anchors { left: parent.left; right: parent.right; top: parent.top }
                                        spacing: Metrics.spacingSm

                                        XpPanelHeader {
                                            title: "Operations"
                                            XpLabel { text: root.devices.operations.count + " total"; role: "caption"; muted: true }
                                        }

                                        XpEmptyState {
                                            visible: root.devices.operations.count === 0
                                            Layout.fillWidth: true
                                            implicitHeight: 96
                                            title: "No operations yet"
                                            message: "Probes and requests appear here with progress and outcome."
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

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: root.twoColumns ? 7 : 1
                    spacing: Metrics.spacingLg

                    DeviceHealthCard {
                        devices: root.devices
                        Layout.fillWidth: true
                    }

                    XpCard {
                        Layout.fillWidth: true
                        implicitHeight: patchColumn.implicitHeight + 2 * Metrics.cardPadding

                        ColumnLayout {
                            id: patchColumn
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            spacing: Metrics.spacingSm

                            XpPanelHeader {
                                title: "Current Sound"
                                StatusPill {
                                    objectName: "patchFetchPill"
                                    text: root.devices.patchFetchStateText
                                    tone: root.devices.patchFetchTone
                                    pulsing: root.devices.patchFetchInProgress
                                }
                            }

                            RowLayout {
                                spacing: Metrics.spacingMd
                                Layout.fillWidth: true
                                XpButton {
                                    objectName: "fetchPatchButton"
                                    text: "Read current sound"
                                    variant: "primary"
                                    enabled: root.devices.canFetchPatch
                                    onClicked: root.devices.fetchCurrentPatch()
                                }
                                XpButton {
                                    text: "Cancel"
                                    visible: root.devices.patchFetchInProgress
                                    onClicked: root.devices.cancelPatchFetch()
                                }
                                XpLabel {
                                    visible: root.devices.patchFetchInProgress
                                    text: root.devices.patchFetchTotalBlocks > 0
                                        ? Math.round(100 * root.devices.patchFetchCompletedBlocks / root.devices.patchFetchTotalBlocks) + "%"
                                        : "…"
                                    role: "mono"
                                    secondary: true
                                }
                                Item { Layout.fillWidth: true }
                                StatusPill { text: "Read only"; tone: "info"; showDot: false }
                            }

                            XpLabel {
                                objectName: "patchFetchMessage"
                                text: root.devices.patchFetchMessage
                                role: "caption"
                                color: root.devices.patchFetchTone === "error" ? Theme.error : Theme.textSecondary
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            Rectangle {
                                visible: root.devices.currentPatchAvailable
                                Layout.fillWidth: true
                                implicitHeight: identityColumn.implicitHeight + 2 * Metrics.spacingMd
                                radius: Metrics.radiusSm
                                color: Theme.surfaceRaised
                                border.width: 1
                                border.color: Theme.borderSubtle
                                ColumnLayout {
                                    id: identityColumn
                                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: Metrics.spacingMd }
                                    spacing: 2
                                    XpLabel { text: "CURRENT SOUND"; role: "overline"; secondary: true }
                                    XpLabel { objectName: "currentPatchName"; text: root.devices.currentPatchName; role: "title"; Layout.fillWidth: true }
                                    XpLabel { text: root.devices.currentPatchSummary; role: "caption"; secondary: true; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                                    XpLabel {
                                        visible: root.devices.currentPatchDecodeReport.length > 0
                                        text: root.devices.currentPatchDecodeReport
                                        role: "caption"
                                        color: Theme.warning
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }

                            // Raw parameter dump stays Expert-only
                            XpButton {
                                visible: root.devices.patchParameters.count > 0
                                text: paramDumpToggle.checked ? "Hide raw parameters" : "Show raw parameters (Expert)"
                                variant: "ghost"
                                compact: true
                                onClicked: paramDumpToggle.checked = !paramDumpToggle.checked
                            }
                            QQC.Switch {
                                id: paramDumpToggle
                                visible: false
                                checked: false
                            }

                            ListView {
                                id: parameterList
                                objectName: "patchParameterList"
                                visible: paramDumpToggle.checked && root.devices.patchParameters.count > 0
                                Layout.fillWidth: true
                                implicitHeight: Math.min(contentHeight, 280)
                                clip: true
                                model: root.devices.patchParameters
                                QQC.ScrollBar.vertical: XpScrollBar {}
                                section.property: "block"
                                section.delegate: Rectangle {
                                    required property string section
                                    width: ListView.view.width
                                    implicitHeight: 24
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
                                    implicitHeight: 24
                                    color: paramHover.hovered ? Theme.surfaceHover : "transparent"
                                    HoverHandler { id: paramHover }
                                    RowLayout {
                                        anchors { fill: parent; leftMargin: Metrics.spacingSm; rightMargin: Metrics.spacingSm }
                                        spacing: Metrics.spacingSm
                                        XpLabel { text: paramRow.model.category; role: "caption"; muted: true; Layout.preferredWidth: 104; elide: Text.ElideRight }
                                        XpLabel { text: paramRow.model.name; role: "caption"; Layout.fillWidth: true; elide: Text.ElideRight }
                                        XpLabel { text: paramRow.model.valueText; role: "mono"; color: paramRow.model.isEnum ? Theme.accentText : Theme.textPrimary }
                                        XpLabel { text: "(" + paramRow.model.rawValue + ")"; role: "mono"; muted: true; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                                    }
                                }
                            }
                        }
                    }

                    XpCard {
                        visible: root.devices.writeSupported
                        Layout.fillWidth: true
                        implicitHeight: writeColumn.implicitHeight + 2 * Metrics.cardPadding

                        ColumnLayout {
                            id: writeColumn
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            spacing: Metrics.spacingSm

                            XpPanelHeader {
                                title: "Send to XP-60"
                                StatusPill {
                                    objectName: "transferPill"
                                    text: root.devices.transferStateText
                                    tone: root.devices.transferTone
                                    pulsing: root.devices.transferBusy
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: armColumn.implicitHeight + 2 * Metrics.spacingMd
                                radius: Metrics.radiusSm
                                color: root.devices.writeArmed ? Theme.warningSoft : Theme.surfaceSunken
                                border.width: 1
                                border.color: root.devices.writeArmed ? Theme.warning : Theme.borderSubtle

                                ColumnLayout {
                                    id: armColumn
                                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: Metrics.spacingMd }
                                    spacing: Metrics.spacingSm

                                    XpLabel {
                                        text: "Writes to the XP-60 edit buffer only, then reads back to verify. Permanent User memory is not overwritten."
                                        role: "caption"
                                        secondary: true
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                    XpLabel {
                                        objectName: "armBlockedReason"
                                        visible: root.devices.armBlockedReason.length > 0
                                        text: root.devices.armBlockedReason
                                        role: "caption"
                                        color: Theme.textMuted
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                    RowLayout {
                                        spacing: Metrics.spacingSm
                                        XpButton {
                                            objectName: "armWriteButton"
                                            text: root.devices.writeArmed ? "Cancel send arming" : "Enable sending"
                                            variant: root.devices.writeArmed ? "danger" : "secondary"
                                            enabled: root.devices.writeArmed || root.devices.canArmWrite
                                            onClicked: root.devices.writeArmed ? root.devices.disarmWrite()
                                                                               : root.devices.armWrite()
                                        }
                                        XpLabel {
                                            visible: root.devices.writeArmed
                                            text: "Ready to send"
                                            role: "caption"
                                            color: Theme.warning
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                spacing: Metrics.spacingSm
                                Layout.fillWidth: true
                                XpButton {
                                    objectName: "writeVerifyButton"
                                    text: "Send to XP-60"
                                    variant: "primary"
                                    enabled: root.devices.canWrite
                                    onClicked: root.devices.writeBackAndVerify()
                                }
                                XpButton {
                                    objectName: "restoreSnapshotButton"
                                    text: "Restore snapshot"
                                    visible: root.devices.safetySnapshotName.length > 0
                                    enabled: root.devices.canRestoreSnapshot
                                    onClicked: root.devices.restoreSafetySnapshot()
                                }
                                XpButton {
                                    text: "Cancel"
                                    visible: root.devices.transferBusy
                                    onClicked: root.devices.cancelTransfer()
                                }
                                Item { Layout.fillWidth: true }
                                XpLabel {
                                    visible: root.devices.safetySnapshotName.length > 0
                                    text: "Snapshot: " + root.devices.safetySnapshotName
                                    role: "caption"
                                    muted: true
                                    elide: Text.ElideRight
                                    Layout.maximumWidth: 220
                                }
                            }

                            XpLabel {
                                objectName: "transferMessage"
                                visible: text.length > 0
                                text: root.devices.transferMessage
                                role: "caption"
                                color: root.devices.transferTone === "error" ? Theme.error
                                     : root.devices.transferTone === "success" ? Theme.success : Theme.textSecondary
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            TransferMismatchCard {
                                report: root.devices.mismatchReport
                                Layout.fillWidth: true
                            }
                        }
                    }

                    ProtocolActivityPanel {
                        devices: root.devices
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        implicitHeight: expanded ? 480 : 120
                    }
                }
            }
        }
    }
}
