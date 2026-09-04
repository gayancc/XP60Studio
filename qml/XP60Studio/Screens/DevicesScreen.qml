import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Devices / Diagnostics — the Phase 1 working surface.
//
// Left column: MIDI connection card, expert RQ1 read test, operations.
// Right column: protocol activity log with health metrics.
// All values come from DevicesViewModel; nothing here knows Roland bytes.
Item {
    id: root

    required property DevicesViewModel devices

    readonly property bool twoColumns: width >= Metrics.devicesTwoColumnMinWidth

    QQC.ScrollView {
        id: scroller
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
                title: "Devices"
                subtitle: "Connect the XP-60, verify Roland SysEx communication, and inspect protocol activity."
                XpButton { text: "Refresh endpoints"; glyph: "↻"; onClicked: root.devices.refreshEndpoints() }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Metrics.screenPadding
                Layout.rightMargin: Metrics.screenPadding
                Layout.bottomMargin: Metrics.screenPadding
                columns: root.twoColumns ? 2 : 1
                columnSpacing: Metrics.spacingLg
                rowSpacing: Metrics.spacingLg

                // ---------------------------------------------------------------
                // Left column
                // ---------------------------------------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: root.twoColumns ? 5 : 1
                    spacing: Metrics.spacingLg

                    // MIDI connection ---------------------------------------------
                    XpCard {
                        id: connectionCard
                        Layout.fillWidth: true
                        implicitHeight: connectionColumn.implicitHeight + 2 * Metrics.cardPadding

                        ColumnLayout {
                            id: connectionColumn
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            spacing: Metrics.spacingMd

                            XpPanelHeader {
                                title: "MIDI connection"
                                glyph: "⌁"
                                StatusPill {
                                    objectName: "connectionPill"
                                    text: root.devices.connectionStateText
                                    tone: root.devices.connectionState === ConnectionState.Connected ? "live"
                                        : root.devices.connectionState === ConnectionState.Connecting ? "warning"
                                        : root.devices.connectionState === ConnectionState.Error ? "error" : "neutral"
                                    pulsing: root.devices.connectionState === ConnectionState.Connecting
                                }
                            }

                            GridLayout {
                                columns: 2
                                columnSpacing: Metrics.spacingMd
                                rowSpacing: Metrics.spacingSm
                                Layout.fillWidth: true

                                XpLabel { text: "MIDI IN"; role: "overline"; secondary: true }
                                XpComboBox {
                                    id: inputPicker
                                    objectName: "inputPicker"
                                    Layout.fillWidth: true
                                    model: root.devices.inputs
                                    textRole: "displayName"
                                    enabled: root.devices.connectionState !== ConnectionState.Connected && count > 0
                                    displayText: count === 0 ? "No MIDI inputs found" : currentText
                                    currentIndex: root.devices.selectedInputIndex
                                    onActivated: function(index) { root.devices.selectedInputIndex = index }
                                    Accessible.name: "MIDI input"
                                }

                                XpLabel { text: "MIDI OUT"; role: "overline"; secondary: true }
                                XpComboBox {
                                    id: outputPicker
                                    objectName: "outputPicker"
                                    Layout.fillWidth: true
                                    model: root.devices.outputs
                                    textRole: "displayName"
                                    enabled: root.devices.connectionState !== ConnectionState.Connected && count > 0
                                    displayText: count === 0 ? "No MIDI outputs found" : currentText
                                    currentIndex: root.devices.selectedOutputIndex
                                    onActivated: function(index) { root.devices.selectedOutputIndex = index }
                                    Accessible.name: "MIDI output"
                                }

                                XpLabel { text: "Device ID"; role: "overline"; secondary: true }
                                RowLayout {
                                    spacing: Metrics.spacingMd
                                    Layout.fillWidth: true
                                    XpSpinField {
                                        objectName: "deviceIdField"
                                        from: root.devices.deviceIdMinimum
                                        to: root.devices.deviceIdMaximum
                                        value: root.devices.deviceId
                                        onValueModified: root.devices.deviceId = value
                                        Accessible.name: "Roland device ID"
                                    }
                                    XpLabel {
                                        text: "Model ID " + root.devices.modelIdText
                                        role: "mono"
                                        secondary: true
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                            }

                            XpLabel {
                                text: root.devices.connectionDetail
                                role: "caption"
                                color: root.devices.connectionState === ConnectionState.Error ? Theme.error : Theme.textSecondary
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            RowLayout {
                                spacing: Metrics.spacingSm
                                XpButton {
                                    objectName: "connectButton"
                                    text: "Connect"
                                    variant: "primary"
                                    enabled: root.devices.canConnect
                                    onClicked: root.devices.connectDevice()
                                }
                                XpButton {
                                    objectName: "disconnectButton"
                                    text: "Disconnect"
                                    enabled: root.devices.canDisconnect
                                    onClicked: root.devices.disconnectDevice()
                                }
                                Item { Layout.fillWidth: true }
                                XpLabel { text: root.devices.backendName; role: "caption"; muted: true; elide: Text.ElideMiddle; Layout.maximumWidth: 240 }
                            }
                        }
                    }

                    // Expert RQ1 read test ------------------------------------------
                    XpCard {
                        Layout.fillWidth: true
                        implicitHeight: requestColumn.implicitHeight + 2 * Metrics.cardPadding

                        ColumnLayout {
                            id: requestColumn
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            spacing: Metrics.spacingMd

                            XpPanelHeader {
                                title: "Safe read test · RQ1"
                                glyph: "⇣"
                                StatusPill { text: "Read only"; tone: "info"; showDot: false }
                            }

                            XpLabel {
                                text: "Ask the XP-60 for a small block of memory and check that a Data Set 1 reply comes back with a valid checksum."
                                role: "caption"
                                secondary: true
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            GridLayout {
                                columns: 2
                                columnSpacing: Metrics.spacingMd
                                rowSpacing: Metrics.spacingSm
                                Layout.fillWidth: true

                                XpLabel { text: "Preset"; role: "overline"; secondary: true }
                                XpComboBox {
                                    objectName: "presetPicker"
                                    Layout.fillWidth: true
                                    model: root.devices.readPresetNames
                                    currentIndex: root.devices.selectedReadPresetIndex
                                    onActivated: function(index) { root.devices.applyReadPreset(index) }
                                    Accessible.name: "Read preset"
                                }

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
                                text: root.devices.readPresetDescription
                                role: "caption"
                                secondary: true
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            RowLayout {
                                spacing: Metrics.spacingSm
                                Layout.fillWidth: true
                                StatusPill { text: root.devices.readPresetStatusText; tone: "warning"; showDot: false }
                            }

                            XpLabel {
                                objectName: "requestValidation"
                                text: root.devices.requestValidationMessage
                                role: "caption"
                                color: (root.devices.requestAddressValid && root.devices.requestSizeValid) ? Theme.textSecondary : Theme.error
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            RowLayout {
                                spacing: Metrics.spacingSm
                                XpButton {
                                    objectName: "sendRequestButton"
                                    text: "Send request"
                                    variant: "primary"
                                    glyph: "⇣"
                                    enabled: root.devices.canSendRequest
                                    onClicked: root.devices.sendRequest()
                                }
                                XpButton {
                                    objectName: "cancelRequestsButton"
                                    text: "Cancel outstanding"
                                    enabled: root.devices.hasOutstandingRequests
                                    onClicked: root.devices.cancelAllRequests()
                                }
                            }

                            XpDivider { Layout.fillWidth: true }

                            RowLayout {
                                spacing: Metrics.spacingMd
                                Layout.fillWidth: true
                                XpButton {
                                    objectName: "dataSetButton"
                                    text: "Write test (DT1)"
                                    enabled: root.devices.dataSetEnabled
                                }
                                XpLabel {
                                    text: root.devices.dataSetDisabledReason
                                    role: "caption"
                                    muted: true
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }

                    // Operations ---------------------------------------------------
                    XpCard {
                        Layout.fillWidth: true
                        implicitHeight: operationsColumn.implicitHeight + 2 * Metrics.cardPadding

                        ColumnLayout {
                            id: operationsColumn
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            spacing: Metrics.spacingSm

                            XpPanelHeader {
                                title: "Requests"
                                glyph: "≡"
                                XpLabel { text: root.devices.operations.count + " total"; role: "caption"; muted: true }
                            }

                            XpEmptyState {
                                visible: root.devices.operations.count === 0
                                Layout.fillWidth: true
                                implicitHeight: 96
                                glyph: "◌"
                                title: "No requests yet"
                                message: "Requests you send appear here with their state, received bytes and any validation notes."
                            }

                            ListView {
                                id: operationsList
                                visible: count > 0
                                Layout.fillWidth: true
                                implicitHeight: Math.min(contentHeight, 320)
                                clip: true
                                model: root.devices.operations
                                spacing: Metrics.spacingSm
                                QQC.ScrollBar.vertical: XpScrollBar {}
                                delegate: Rectangle {
                                    id: opRow
                                    required property var model
                                    width: ListView.view.width
                                    implicitHeight: opColumn.implicitHeight + 2 * Metrics.spacingMd
                                    radius: Metrics.radiusSm
                                    color: Theme.surfaceRaised
                                    border.width: 1
                                    border.color: Theme.borderSubtle
                                    ColumnLayout {
                                        id: opColumn
                                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: Metrics.spacingMd }
                                        spacing: Metrics.spacingXs
                                        RowLayout {
                                            spacing: Metrics.spacingSm
                                            Layout.fillWidth: true
                                            XpLabel { text: "#" + opRow.model.requestId; role: "mono"; secondary: true }
                                            XpLabel { text: "RQ1 " + opRow.model.addressHex + "  size " + opRow.model.sizeHex; role: "mono"; Layout.fillWidth: true }
                                            StatusPill {
                                                text: opRow.model.stateLabel
                                                tone: opRow.model.isSuccess ? "success" : (opRow.model.isTerminal ? "error" : "warning")
                                                pulsing: !opRow.model.isTerminal
                                            }
                                        }
                                        Rectangle {
                                            Layout.fillWidth: true
                                            implicitHeight: 4
                                            radius: 2
                                            color: Theme.surfaceSunken
                                            Rectangle {
                                                width: parent.width * opRow.model.progress
                                                height: parent.height
                                                radius: 2
                                                color: opRow.model.isSuccess ? Theme.success : (opRow.model.isTerminal ? Theme.error : Theme.accent)
                                                Behavior on width { NumberAnimation { duration: Motion.durationNormal } }
                                            }
                                        }
                                        XpLabel {
                                            text: opRow.model.receivedBytes + " / " + opRow.model.expectedBytes + " bytes · " + opRow.model.chunkCount + " chunk(s)"
                                            role: "caption"
                                            secondary: true
                                        }
                                        XpLabel {
                                            visible: opRow.model.dataHex.length > 0
                                            text: opRow.model.dataHex
                                            role: "mono"
                                            wrapMode: Text.WrapAnywhere
                                            maximumLineCount: 3
                                            Layout.fillWidth: true
                                        }
                                        XpLabel {
                                            visible: opRow.model.isSuccess && opRow.model.dataText.length > 0
                                            text: "Text: " + opRow.model.dataText
                                            role: "mono"
                                            secondary: true
                                            Layout.fillWidth: true
                                        }
                                        XpLabel {
                                            visible: opRow.model.failureReason.length > 0
                                            text: opRow.model.failureReason
                                            role: "caption"
                                            color: Theme.error
                                            wrapMode: Text.WordWrap
                                            Layout.fillWidth: true
                                        }
                                        XpLabel {
                                            visible: opRow.model.notes.length > 0
                                            text: opRow.model.notes
                                            role: "caption"
                                            color: Theme.warning
                                            wrapMode: Text.WordWrap
                                            Layout.fillWidth: true
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // ---------------------------------------------------------------
                // Right column: protocol activity
                // ---------------------------------------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: root.twoColumns ? 7 : 1
                    spacing: Metrics.spacingLg

                    // Health metrics ------------------------------------------------
                    GridLayout {
                        id: metricGrid
                        Layout.fillWidth: true
                        // Wrap by available width so tile text never truncates.
                        columns: Math.max(2, Math.floor((width + Metrics.spacingMd) / (Metrics.metricTileMinWidth + Metrics.spacingMd)))
                        columnSpacing: Metrics.spacingMd
                        rowSpacing: Metrics.spacingMd
                        XpMetricTile { Layout.fillWidth: true; label: "SysEx"; value: root.devices.sysExHealthText; tone: root.devices.sysExHealthTone }
                        XpMetricTile { Layout.fillWidth: true; label: "In"; value: root.devices.messagesIn; hint: root.devices.sysExIn + " SysEx" }
                        XpMetricTile { Layout.fillWidth: true; label: "Out"; value: root.devices.messagesOut; hint: root.devices.sysExOut + " SysEx" }
                        XpMetricTile { Layout.fillWidth: true; label: "Checksum"; value: root.devices.checksumFailures; hint: "errors"; tone: root.devices.checksumFailures > 0 ? "error" : "neutral" }
                        XpMetricTile { Layout.fillWidth: true; label: "Timeouts"; value: root.devices.timeouts; tone: root.devices.timeouts > 0 ? "warning" : "neutral" }
                    }

                    // Protocol log --------------------------------------------------
                    XpCard {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        implicitHeight: Math.max(480, logColumn.implicitHeight + 2 * Metrics.cardPadding)

                        ColumnLayout {
                            id: logColumn
                            anchors.fill: parent
                            spacing: Metrics.spacingSm

                            XpPanelHeader {
                                title: "Protocol activity"
                                glyph: "∿"
                                XpLabel { text: root.devices.log.count + " entries"; role: "caption"; muted: true; anchors.verticalCenter: parent.verticalCenter }
                                XpButton { text: "Clear"; compact: true; variant: "ghost"; onClicked: root.devices.clearLog() }
                            }

                            ListView {
                                id: logList
                                objectName: "protocolLog"
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                implicitHeight: 420
                                clip: true
                                model: root.devices.log
                                spacing: 2
                                QQC.ScrollBar.vertical: XpScrollBar {}
                                onCountChanged: Qt.callLater(function() { if (logList) logList.positionViewAtEnd() })

                                delegate: Rectangle {
                                    id: logRow
                                    required property var model
                                    property bool expanded: false
                                    width: ListView.view.width
                                    implicitHeight: rowColumn.implicitHeight + Metrics.spacingSm
                                    radius: Metrics.radiusSm
                                    color: logRowHover.hovered || expanded ? Theme.surfaceHover : "transparent"

                                    readonly property color directionColor: model.direction === "IN" ? Theme.tone2
                                                                          : model.direction === "OUT" ? Theme.tone1 : Theme.textMuted
                                    readonly property color textColor: model.severity === "Error" ? Theme.error
                                                                     : model.severity === "Warning" ? Theme.warning : Theme.textPrimary

                                    HoverHandler { id: logRowHover }
                                    TapHandler { onTapped: logRow.expanded = !logRow.expanded }

                                    ColumnLayout {
                                        id: rowColumn
                                        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: Metrics.spacingSm; rightMargin: Metrics.spacingSm; topMargin: Metrics.spacingXs }
                                        spacing: 2
                                        RowLayout {
                                            spacing: Metrics.spacingSm
                                            Layout.fillWidth: true
                                            XpLabel { text: logRow.model.timeText; role: "mono"; muted: true }
                                            Rectangle {
                                                implicitWidth: 34
                                                implicitHeight: 16
                                                radius: 3
                                                color: Qt.rgba(logRow.directionColor.r, logRow.directionColor.g, logRow.directionColor.b, 0.18)
                                                XpLabel { anchors.centerIn: parent; text: logRow.model.direction; role: "overline"; color: logRow.directionColor }
                                            }
                                            XpLabel {
                                                text: logRow.model.summary
                                                role: "mono"
                                                color: logRow.textColor
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                            StatusPill {
                                                visible: logRow.model.checksum !== "-"
                                                text: "checksum " + logRow.model.checksum
                                                tone: logRow.model.checksum === "OK" ? "success" : "error"
                                                showDot: false
                                            }
                                        }
                                        XpLabel {
                                            visible: logRow.expanded && logRow.model.detail.length > 0
                                            text: logRow.model.detail
                                            role: "caption"
                                            secondary: true
                                            wrapMode: Text.WordWrap
                                            Layout.fillWidth: true
                                        }
                                        XpLabel {
                                            visible: logRow.expanded && logRow.model.rawHex.length > 0
                                            text: logRow.model.rawHex
                                            role: "mono"
                                            muted: true
                                            wrapMode: Text.WrapAnywhere
                                            Layout.fillWidth: true
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
