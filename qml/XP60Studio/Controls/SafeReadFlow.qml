import QtQuick
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Preset-driven probe with Request → Receive → Validate → Result visualization.
// Lives under Advanced diagnostics; hex fields remain available but secondary.
Item {
    id: root

    required property DevicesViewModel devices
    property bool showCustomAddress: true

    implicitHeight: column.implicitHeight
    implicitWidth: column.implicitWidth

    ColumnLayout {
        id: column
        anchors { left: parent.left; right: parent.right; top: parent.top }
        width: parent.width
        spacing: Metrics.spacingMd

        XpPanelHeader {
            title: "Keyboard probe"
            StatusPill { text: "Read only"; tone: "info"; showDot: false }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingXs
            Repeater {
                model: ["Request", "Receive", "Validate", "Result"]
                delegate: Rectangle {
                    required property string modelData
                    required property int index
                    Layout.fillWidth: true
                    implicitHeight: 36
                    radius: Metrics.radiusSm
                    color: {
                        if (root.devices.hasOutstandingRequests && index <= 1)
                            return Theme.toneBackground("warning")
                        if (!root.devices.hasOutstandingRequests && root.devices.operations.count > 0 && index === 3)
                            return Theme.toneBackground("success")
                        if (index === 0)
                            return Theme.accentSoft
                        return Theme.surfaceSunken
                    }
                    border.width: 1
                    border.color: Theme.borderSubtle
                    XpLabel {
                        anchors.centerIn: parent
                        text: modelData
                        role: "caption"
                        font.weight: Typography.weightMedium
                    }
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: Metrics.spacingXs
            Repeater {
                model: root.devices.readPresetNames
                delegate: XpButton {
                    text: modelData.split(" (")[0]
                    compact: true
                    variant: root.devices.selectedReadPresetIndex === index ? "primary" : "ghost"
                    onClicked: root.devices.applyReadPreset(index)
                }
            }
        }

        XpLabel {
            text: root.devices.readPresetDescription
            role: "caption"
            secondary: true
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: Metrics.spacingSm
            StatusPill { text: root.devices.readPresetStatusText; tone: "warning"; showDot: false }
            Item { Layout.fillWidth: true }
            XpButton {
                objectName: "sendRequestButton"
                text: "Send probe"
                variant: "primary"
                enabled: root.devices.canSendRequest
                onClicked: root.devices.sendRequest()
            }
            XpButton {
                objectName: "cancelRequestsButton"
                text: "Cancel"
                enabled: root.devices.hasOutstandingRequests
                onClicked: root.devices.cancelAllRequests()
            }
        }

        XpButton {
            text: root.showCustomAddress ? "Hide custom address" : "Custom address"
            variant: "ghost"
            compact: true
            onClicked: root.showCustomAddress = !root.showCustomAddress
        }

        // Keep fields in the tree for keyboard tests; collapse visually when hidden.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingSm
            opacity: root.showCustomAddress ? 1 : 0
            height: root.showCustomAddress ? implicitHeight : 0
            clip: true

            XpComboBox {
                objectName: "presetPicker"
                Layout.fillWidth: true
                model: root.devices.readPresetNames
                currentIndex: root.devices.selectedReadPresetIndex
                onActivated: function(index) { root.devices.applyReadPreset(index) }
                Accessible.name: "Read preset"
            }

            GridLayout {
                columns: 2
                columnSpacing: Metrics.spacingMd
                rowSpacing: Metrics.spacingSm
                Layout.fillWidth: true

                XpLabel { text: "Address"; role: "overline"; secondary: true }
                XpTextField {
                    objectName: "addressField"
                    Layout.fillWidth: true
                    mono: true
                    text: root.devices.requestAddress
                    invalid: !root.devices.requestAddressValid
                    placeholderText: "03 00 00 00"
                    onTextEdited: root.devices.requestAddress = text
                    Accessible.name: "Roland address, four hex bytes"
                }

                XpLabel { text: "Size"; role: "overline"; secondary: true }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingMd
                    XpTextField {
                        objectName: "sizeField"
                        Layout.fillWidth: true
                        mono: true
                        text: root.devices.requestSize
                        invalid: !root.devices.requestSizeValid
                        placeholderText: "00 00 00 0C"
                        onTextEdited: root.devices.requestSize = text
                        Accessible.name: "Request size, four hex bytes"
                    }
                    XpLabel {
                        text: root.devices.requestByteCount + " bytes"
                        role: "mono"
                        secondary: true
                    }
                }
            }

            XpLabel {
                objectName: "requestValidation"
                text: root.devices.requestValidationMessage
                role: "caption"
                color: (root.devices.requestAddressValid && root.devices.requestSizeValid) ? Theme.textSecondary : Theme.error
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }
}
