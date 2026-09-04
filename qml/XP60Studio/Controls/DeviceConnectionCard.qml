import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Guided MIDI connection: visual path + primary actions; ports/options secondary.
XpCard {
    id: root

    required property DevicesViewModel devices
    property bool optionsExpanded: false

    implicitHeight: column.implicitHeight + 2 * Metrics.cardPadding

    ColumnLayout {
        id: column
        anchors { left: parent.left; right: parent.right; top: parent.top }
        spacing: Metrics.spacingMd

        XpPanelHeader {
            title: "Device connection"
            iconName: "devices"
            StatusPill {
                objectName: "connectionPill"
                text: root.devices.connectionStateText
                tone: root.devices.connectionState === ConnectionState.Connected
                      ? (root.devices.connectionVerified ? "live" : "warning")
                      : root.devices.connectionState === ConnectionState.Connecting ? "warning"
                      : root.devices.connectionState === ConnectionState.Error ? "error" : "neutral"
                pulsing: root.devices.connectionState === ConnectionState.Connecting
            }
        }

        // Visual Studio ↔ MIDI ↔ XP-60 path
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: pathRow.implicitHeight + 2 * Metrics.spacingMd
            radius: Metrics.radiusSm
            color: Theme.surfaceSunken
            border.width: 1
            border.color: Theme.borderSubtle

            RowLayout {
                id: pathRow
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: Metrics.spacingMd }
                spacing: Metrics.spacingSm

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 56
                    radius: Metrics.radiusSm
                    color: Theme.surfaceRaised
                    border.width: 1
                    border.color: Theme.border
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 2
                        XpLabel { text: "XP60Studio"; role: "caption"; font.weight: Typography.weightMedium; Layout.alignment: Qt.AlignHCenter }
                        XpLabel { text: "App"; role: "overline"; muted: true; Layout.alignment: Qt.AlignHCenter }
                    }
                }

                XpLabel { text: "→"; role: "title"; color: Theme.accentText }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 56
                    radius: Metrics.radiusSm
                    color: Theme.toneBackground(
                        root.devices.connectionState === ConnectionState.Connected ? (root.devices.connectionVerified ? "live" : "warning")
                        : root.devices.connectionState === ConnectionState.Error ? "error" : "neutral")
                    border.width: 1
                    border.color: Theme.border
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 2
                        XpLabel {
                            text: "MIDI"
                            role: "caption"
                            font.weight: Typography.weightMedium
                            Layout.alignment: Qt.AlignHCenter
                            color: Theme.toneForeground(
                                root.devices.connectionState === ConnectionState.Connected ? (root.devices.connectionVerified ? "live" : "warning")
                                : root.devices.connectionState === ConnectionState.Error ? "error" : "neutral")
                        }
                        XpLabel {
                            text: root.devices.connectionState === ConnectionState.Connected ? "Linked" : "Idle"
                            role: "overline"
                            Layout.alignment: Qt.AlignHCenter
                            color: Theme.toneForeground(
                                root.devices.connectionState === ConnectionState.Connected ? (root.devices.connectionVerified ? "live" : "warning")
                                : root.devices.connectionState === ConnectionState.Error ? "error" : "neutral")
                            opacity: 0.85
                        }
                    }
                }

                XpLabel { text: "→"; role: "title"; color: Theme.accentText }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 56
                    radius: Metrics.radiusSm
                    color: Theme.surfaceRaised
                    border.width: 1
                    border.color: root.devices.connectionVerified ? Theme.live : Theme.border
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 2
                        XpLabel { text: "Roland XP-60"; role: "caption"; font.weight: Typography.weightMedium; Layout.alignment: Qt.AlignHCenter }
                        XpLabel {
                            text: root.devices.connectionVerified ? "Live" : (root.devices.connectionState === ConnectionState.Connected ? "Waiting" : "Offline")
                            role: "overline"
                            muted: !root.devices.connectionVerified
                            color: root.devices.connectionVerified ? Theme.live : Theme.textMuted
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }
        }

        XpLabel {
            visible: root.devices.connectionDetail.length > 0
            text: root.devices.connectionDetail
            role: "caption"
            color: root.devices.connectionState === ConnectionState.Error ? Theme.error : Theme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        XpLabel {
            visible: root.devices.selectionMessage.length > 0 && root.devices.connectionState !== ConnectionState.Connected
            text: root.devices.selectionMessage
            role: "caption"
            secondary: true
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: Metrics.spacingSm
            Layout.fillWidth: true
            XpButton {
                objectName: "connectButton"
                text: "Connect"
                variant: "primary"
                enabled: root.devices.canConnect
                onClicked: root.devices.connectDevice()
            }
            XpButton {
                objectName: "disconnectButton"
                text: root.devices.connectionState === ConnectionState.Connecting ? "Cancel connection" : "Disconnect"
                enabled: root.devices.canDisconnect
                onClicked: root.devices.disconnectDevice()
            }
            Item { Layout.fillWidth: true }
            XpButton {
                objectName: "testConnectionButton"
                text: "Test XP-60"
                enabled: root.devices.canTestConnection
                onClicked: root.devices.testConnection()
            }
            StatusPill { text: "Read only"; tone: "info"; showDot: false }
        }

        XpLabel {
            objectName: "connectionTestMessage"
            text: root.devices.connectionTestMessage
            role: "caption"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            color: root.devices.connectionVerified ? Theme.live : Theme.textSecondary
        }

        // Connection options (ports, device ID, pacing) — collapsed when connected & verified
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingSm
            XpButton {
                text: root.optionsExpanded ? "Hide connection options" : "Connection options"
                variant: "ghost"
                compact: true
                onClicked: root.optionsExpanded = !root.optionsExpanded
            }
            Item { Layout.fillWidth: true }
            XpButton {
                text: "Scan"
                compact: true
                variant: "ghost"
                onClicked: root.devices.refreshEndpoints()
            }
        }

        ColumnLayout {
            visible: root.optionsExpanded || root.devices.connectionState !== ConnectionState.Connected
            Layout.fillWidth: true
            spacing: Metrics.spacingSm

            GridLayout {
                columns: 2
                columnSpacing: Metrics.spacingMd
                rowSpacing: Metrics.spacingSm
                Layout.fillWidth: true

                XpLabel { text: "From XP-60"; role: "overline"; secondary: true }
                XpComboBox {
                    id: inputPicker
                    objectName: "inputPicker"
                    Layout.fillWidth: true
                    model: root.devices.inputs
                    textRole: "selectionLabel"
                    enabled: root.devices.connectionState !== ConnectionState.Connected && root.devices.connectionState !== ConnectionState.Connecting && count > 0
                    displayText: count === 0 ? "No MIDI devices detected" : currentIndex < 0 ? "Choose port" : currentText
                    currentIndex: root.devices.selectedInputIndex
                    onActivated: function(index) { root.devices.selectedInputIndex = index }
                    Accessible.name: "MIDI input"
                }

                XpLabel { text: "To XP-60"; role: "overline"; secondary: true }
                XpComboBox {
                    objectName: "outputPicker"
                    Layout.fillWidth: true
                    model: root.devices.outputs
                    textRole: "selectionLabel"
                    enabled: root.devices.connectionState !== ConnectionState.Connected && root.devices.connectionState !== ConnectionState.Connecting && count > 0
                    displayText: count === 0 ? "No MIDI devices detected" : currentIndex < 0 ? "Choose port" : currentText
                    currentIndex: root.devices.selectedOutputIndex
                    onActivated: function(index) { root.devices.selectedOutputIndex = index }
                    Accessible.name: "MIDI output"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingMd
                visible: true
                XpLabel { text: "Device number"; role: "caption"; secondary: true; visible: root.optionsExpanded }
                XpSpinField {
                    objectName: "deviceIdField"
                    visible: root.optionsExpanded
                    enabled: root.devices.connectionState !== ConnectionState.Connecting && !root.devices.transferBusy
                    from: root.devices.deviceIdMinimum
                    to: root.devices.deviceIdMaximum
                    value: root.devices.deviceId
                    onValueModified: root.devices.deviceId = value
                    Accessible.name: "XP-60 device number"
                }
                XpLabel { text: "Speed"; role: "caption"; secondary: true }
                XpComboBox {
                    objectName: "pacingPicker"
                    Layout.fillWidth: true
                    model: ["Normal", "Slow (wireless)"]
                    currentIndex: root.devices.pacingProfile
                    enabled: !root.devices.hasOutstandingRequests && !root.devices.transferBusy
                    onActivated: function(index) { root.devices.pacingProfile = index }
                    Accessible.name: "Connection speed"
                }
            }

            XpLabel {
                visible: root.optionsExpanded
                text: "Usually device 17. Wireless MIDI: pair the adapter in system settings, then Scan."
                role: "caption"
                muted: true
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }
}
