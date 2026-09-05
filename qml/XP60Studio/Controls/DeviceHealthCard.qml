import QtQuick
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Communication health as a Studio → MIDI → XP-60 path, not KPI tiles.
XpCard {
    id: root

    required property DevicesViewModel devices
    property bool countersExpanded: false

    implicitHeight: column.implicitHeight + 2 * Metrics.cardPadding

    readonly property string healthTone: root.devices.sysExHealthTone
    readonly property color healthFg: Theme.toneForeground(healthTone)

    ColumnLayout {
        id: column
        anchors { left: parent.left; right: parent.right; top: parent.top }
        spacing: Metrics.spacingMd

        XpPanelHeader {
            title: "Communication health"
            StatusPill {
                text: root.devices.sysExHealthText
                tone: root.healthTone
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingSm

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 52
                radius: Metrics.radiusSm
                color: Theme.surfaceRaised
                border.width: 1
                border.color: Theme.borderSubtle
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 1
                    XpLabel { text: "Studio"; role: "caption"; font.weight: Typography.weightMedium; Layout.alignment: Qt.AlignHCenter }
                    XpLabel { text: "XP60Studio"; role: "overline"; muted: true; Layout.alignment: Qt.AlignHCenter }
                }
            }
            XpLabel { text: "→"; role: "body"; color: Theme.textMuted }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 52
                radius: Metrics.radiusSm
                color: Theme.surfaceRaised
                border.width: 1
                border.color: Theme.borderSubtle
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 1
                    XpLabel { text: "MIDI"; role: "caption"; font.weight: Typography.weightMedium; Layout.alignment: Qt.AlignHCenter }
                    XpLabel {
                        text: root.devices.connectionState === ConnectionState.Connected ? "Open" : "Closed"
                        role: "overline"; muted: true; Layout.alignment: Qt.AlignHCenter
                    }
                }
            }
            XpLabel { text: "→"; role: "body"; color: Theme.textMuted }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 52
                radius: Metrics.radiusSm
                color: Theme.toneBackground(root.healthTone)
                border.width: 1
                border.color: Theme.borderSubtle
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 1
                    XpLabel { text: "XP-60"; role: "caption"; font.weight: Typography.weightMedium; Layout.alignment: Qt.AlignHCenter; color: root.healthFg }
                    XpLabel {
                        text: root.devices.connectionVerified ? "Responding"
                              : (root.devices.connectionState === ConnectionState.Connected ? "Silent" : "Offline")
                        role: "overline"; Layout.alignment: Qt.AlignHCenter; color: root.healthFg; opacity: 0.9
                    }
                }
            }
            XpLabel { text: "→"; role: "body"; color: Theme.textMuted }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 52
                radius: Metrics.radiusSm
                color: Theme.surfaceRaised
                border.width: 1
                border.color: Theme.borderSubtle
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 1
                    XpLabel { text: "Studio"; role: "caption"; font.weight: Typography.weightMedium; Layout.alignment: Qt.AlignHCenter }
                    XpLabel {
                        text: root.devices.messagesIn > 0 ? "Receiving" : "Idle"
                        role: "overline"; muted: true; Layout.alignment: Qt.AlignHCenter
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            XpButton {
                text: root.countersExpanded ? "Hide counters" : "Show counters"
                variant: "ghost"
                compact: true
                onClicked: root.countersExpanded = !root.countersExpanded
            }
            Item { Layout.fillWidth: true }
            StatusPill {
                visible: root.devices.checksumFailures > 0
                text: root.devices.checksumFailures + " integrity"
                tone: "error"
                showDot: false
            }
            StatusPill {
                visible: root.devices.timeouts > 0
                text: root.devices.timeouts + " missed"
                tone: "warning"
                showDot: false
            }
        }

        GridLayout {
            visible: root.countersExpanded
            Layout.fillWidth: true
            columns: 4
            columnSpacing: Metrics.spacingMd
            rowSpacing: Metrics.spacingXs
            XpLabel { text: "In"; role: "label"; secondary: true }
            XpLabel { text: root.devices.messagesIn; role: "mono" }
            XpLabel { text: "Out"; role: "label"; secondary: true }
            XpLabel { text: root.devices.messagesOut; role: "mono" }
            XpLabel { text: "SysEx in"; role: "label"; secondary: true }
            XpLabel { text: root.devices.sysExIn; role: "mono" }
            XpLabel { text: "SysEx out"; role: "label"; secondary: true }
            XpLabel { text: root.devices.sysExOut; role: "mono" }
            XpLabel { text: "Checksum"; role: "label"; secondary: true }
            XpLabel { text: root.devices.checksumFailures; role: "mono"; color: root.devices.checksumFailures > 0 ? Theme.error : Theme.textPrimary }
            XpLabel { text: "Timeouts"; role: "label"; secondary: true }
            XpLabel { text: root.devices.timeouts; role: "mono"; color: root.devices.timeouts > 0 ? Theme.warning : Theme.textPrimary }
        }
    }
}
