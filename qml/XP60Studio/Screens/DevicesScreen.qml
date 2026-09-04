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
                title: "Setup"
                subtitle: "Set up your MIDI connection and check that XP60Studio can talk to your keyboard."
                XpButton { text: "Scan for MIDI devices"; glyph: "↻"; onClicked: root.devices.refreshEndpoints() }
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
                                    tone: root.devices.connectionState === ConnectionState.Connected ? (root.devices.connectionVerified ? "live" : "warning")
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
                                    id: outputPicker
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

                                XpLabel { text: "XP-60 device number"; role: "overline"; secondary: true }
                                RowLayout {
                                    spacing: Metrics.spacingMd
                                    Layout.fillWidth: true
                                    XpSpinField {
                                        objectName: "deviceIdField"
                                        enabled: root.devices.connectionState !== ConnectionState.Connecting && !root.devices.transferBusy
                                        from: root.devices.deviceIdMinimum
                                        to: root.devices.deviceIdMaximum
                                        value: root.devices.deviceId
                                        onValueModified: root.devices.deviceId = value
                                        Accessible.name: "XP-60 device number"
                                    }
                                    XpLabel {
                                        text: "Usually 17 — only change if you set a custom ID on the XP-60"
                                        role: "caption"
                                        muted: true
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                            }

                            XpLabel {
                                text: root.devices.selectionMessage
                                role: "caption"
                                secondary: true
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            // From/To labels are self-explanatory; no wiring paragraph needed.
                            RowLayout {
                                Layout.fillWidth: true
                                XpLabel { text: "Connection speed"; role: "caption"; secondary: true }
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
                                    text: root.devices.connectionState === ConnectionState.Connecting ? "Cancel connection" : "Disconnect"
                                    enabled: root.devices.canDisconnect
                                    onClicked: root.devices.disconnectDevice()
                                }

                            }
                            RowLayout {
                                Layout.fillWidth: true
                                XpButton {
                                    objectName: "testConnectionButton"
                                    text: "Test XP-60 connection"
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
                            XpLabel {
                                text: "Using wireless MIDI? Pair your adapter in your system settings first, then click 'Scan for MIDI devices'."
                                role: "caption"
                                muted: true
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            // Backend name (e.g. libremidi version) hidden from default view;
                            // available in Advanced diagnostics section.
                        }
                    }

                    // Advanced diagnostics (collapsed by default) -----------------
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
                                XpLabel {
                                    text: "MIDI backend: " + root.devices.backendName + "  ·  Model ID " + root.devices.modelIdText
                                    role: "caption"
                                    muted: true
                                    visible: advancedToggle.checked
                                }
                                QQC.Switch {
                                    id: advancedToggle
                                    objectName: "advancedToggle"
                                    checked: false
                                }
                            }

                            // All expert content lives inside this column, shown only when toggled.
                            ColumnLayout {
                                visible: advancedToggle.checked
                                Layout.fillWidth: true
                                spacing: Metrics.spacingLg

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
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: Metrics.spacingXs
                                                XpComboBox {
                                                    id: presetPicker
                                                    objectName: "presetPicker"
                                                    Layout.fillWidth: true
                                                    model: root.devices.readPresetNames
                                                    currentIndex: root.devices.selectedReadPresetIndex
                                                    onActivated: function(index) { root.devices.applyReadPreset(index) }
                                                    Accessible.name: "Read preset"
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
                        XpMetricTile { Layout.fillWidth: true; label: "Connection"; value: root.devices.sysExHealthText; tone: root.devices.sysExHealthTone }
                        XpMetricTile { Layout.fillWidth: true; label: "Received"; value: root.devices.messagesIn; hint: root.devices.sysExIn + " messages" }
                        XpMetricTile { Layout.fillWidth: true; label: "Sent"; value: root.devices.messagesOut; hint: root.devices.sysExOut + " messages" }
                        XpMetricTile { Layout.fillWidth: true; label: "Data integrity"; value: root.devices.checksumFailures; hint: "errors"; tone: root.devices.checksumFailures > 0 ? "error" : "neutral" }
                        XpMetricTile { Layout.fillWidth: true; label: "Missed replies"; value: root.devices.timeouts; tone: root.devices.timeouts > 0 ? "warning" : "neutral" }
                    }

                    // Current Patch inspection (Phase 2) ---------------------------------
                    XpCard {
                        Layout.fillWidth: true
                        implicitHeight: patchColumn.implicitHeight + 2 * Metrics.cardPadding

                        ColumnLayout {
                            id: patchColumn
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            spacing: Metrics.spacingSm

                            XpPanelHeader {
                                title: "Current Sound"
                                glyph: "♫"
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
                                    glyph: "⇣"
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

                            // Decoded patch identity, echoing the Dashboard "current patch" hierarchy.
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

                            ListView {
                                id: parameterList
                                objectName: "patchParameterList"
                                visible: root.devices.patchParameters.count > 0
                                Layout.fillWidth: true
                                implicitHeight: Math.min(contentHeight, 320)
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

                    // Write and verify (Phase 3) ----------------------------------------
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
                                glyph: "⇅"
                                StatusPill {
                                    objectName: "transferPill"
                                    text: root.devices.transferStateText
                                    tone: root.devices.transferTone
                                    pulsing: root.devices.transferBusy
                                }
                            }

                            // Writing to the instrument is an armed action: the plan is
                            // stated in full before the control becomes usable.
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
                                        text: "Sends the sound to your XP-60's edit buffer (temporary — not permanently saved). After sending, XP60Studio reads it back to confirm everything arrived correctly. Your keyboard's permanent sounds are not affected."
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
                                            text: root.devices.writeArmed ? "Sending enabled — cancel" : "Enable sending"
                                            variant: root.devices.writeArmed ? "danger" : "secondary"
                                            glyph: root.devices.writeArmed ? "⏻" : "⚿"
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
                                    glyph: "⇅"
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

                            // A read-back mismatch is shown parameter by parameter,
                            // never summarised away.
                            Rectangle {
                                objectName: "mismatchPanel"
                                visible: root.devices.mismatchReport.length > 0
                                Layout.fillWidth: true
                                implicitHeight: mismatchColumn.implicitHeight + 2 * Metrics.spacingMd
                                radius: Metrics.radiusSm
                                color: Theme.errorSoft
                                border.width: 1
                                border.color: Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.45)
                                ColumnLayout {
                                    id: mismatchColumn
                                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: Metrics.spacingMd }
                                    spacing: 2
                                    XpLabel { text: "VERIFICATION MISMATCH"; role: "overline"; color: Theme.error }
                                    XpLabel {
                                        text: root.devices.mismatchReport
                                        role: "mono"
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
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
                                title: "Communication Log"
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
                                                text: logRow.model.checksum === "OK" ? "✓ valid" : "✗ error"
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
