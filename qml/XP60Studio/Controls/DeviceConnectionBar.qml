import QtQuick
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// The MIDI connection, as one row.
//
// This replaces a full-width panel whose centrepiece was an
// `XP60Studio -> MIDI -> Roland XP-60` diagram. That diagram was decoration:
// it restated the connection state a third time, in boxes, and told a musician
// nothing they could act on. Connecting to a MIDI port is a two-field job, and
// giving it a 230 px banner took that space from the sound.
//
// What the bar actually carries, in every state:
//
//   identity | state | ports | the one action available now | details
//
// and, while something is in flight, a progress line and what is happening.
// The ports drop down inline; everything rarer (device number, speed) is behind
// the details disclosure.
Rectangle {
    id: root

    required property DevicesViewModel devices
    property bool demoMode: false
    property bool detailsExpanded: false

    readonly property bool connected: devices.connectionState === ConnectionState.Connected
    readonly property bool connecting: devices.connectionState === ConnectionState.Connecting
    readonly property bool failed: devices.connectionState === ConnectionState.Error
    readonly property bool verified: devices.connectionVerified
    readonly property bool busy: connecting || devices.hasOutstandingRequests
    // One tone for the whole bar, derived once.
    readonly property string tone: verified ? "live"
                                 : connected || connecting ? "warning"
                                 : failed ? "error" : "neutral"
    readonly property color accentEdge: Theme.toneForeground(tone)

    // The one thing to say about what is happening now. Empty when the state
    // is self-evident from the bar itself, so a settled connection is silent.
    readonly property string activity: {
        if (failed) return devices.connectionDetail
        if (connecting) return qsTr("Opening MIDI ports")
        if (connected && !verified) return qsTr("Ports open. Test to confirm the XP-60 is answering.")
        if (!connected && !devices.canConnect) return devices.selectionMessage
        return ""
    }

    readonly property int labelColumn: 96

    color: Theme.surface
    radius: Metrics.radiusMd
    border.width: Metrics.borderWidth
    border.color: root.tone === "neutral" ? Theme.border : Theme.toneBorder(root.tone)
    implicitHeight: layout.implicitHeight + 2 * Metrics.spacingMd

    Behavior on implicitHeight {
        enabled: !Motion.reducedMotion
        NumberAnimation { duration: Motion.durationNormal; easing.type: Motion.easingStandard }
    }
    Behavior on border.color {
        enabled: !Motion.reducedMotion
        ColorAnimation { duration: Motion.durationNormal }
    }

    // State also reads as a colour on the leading edge, so the bar's condition
    // is legible from the far side of a desk without reading the pill.
    Rectangle {
        width: 3
        radius: 1.5
        color: root.accentEdge
        opacity: root.tone === "neutral" ? 0.35 : 1
        anchors {
            left: parent.left; leftMargin: 1
            top: parent.top; topMargin: Metrics.spacingSm
            bottom: parent.bottom; bottomMargin: Metrics.spacingSm
        }
    }

    ColumnLayout {
        id: layout
        anchors {
            left: parent.left; right: parent.right; top: parent.top
            leftMargin: Metrics.spacingLg
            rightMargin: Metrics.spacingMd
            topMargin: Metrics.spacingMd
        }
        spacing: Metrics.spacingSm

        // --- The row --------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            XpIcon {
                name: "devices"
                color: root.tone === "neutral" ? Theme.textMuted : root.accentEdge
                Layout.alignment: Qt.AlignVCenter
            }

            XpLabel {
                text: root.demoMode ? qsTr("XP-60 (Simulated)") : qsTr("Roland XP-60")
                role: "body"
                font.weight: Typography.weightMedium
                Layout.alignment: Qt.AlignVCenter
            }

            StatusPill {
                objectName: "connectionPill"
                text: root.devices.connectionStateText
                tone: root.tone
                pulsing: root.busy
                Layout.alignment: Qt.AlignVCenter
            }

            StatusPill {
                visible: root.demoMode
                text: qsTr("Simulated")
                tone: "info"
                showDot: false
                Layout.alignment: Qt.AlignVCenter
            }

            // Ports. Once connected they are a fact and take no field width;
            // before that they are the job, and the pickers are right here.
            XpLabel {
                visible: root.connected
                text: root.devices.connectedInputName + "  →  " + root.devices.connectedOutputName
                role: "caption"
                muted: true
                elide: Text.ElideMiddle
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
            }

            XpComboBox {
                objectName: "inputPicker"
                visible: !root.connected
                Layout.fillWidth: true
                Layout.maximumWidth: 260
                Layout.alignment: Qt.AlignVCenter
                model: root.devices.inputs
                textRole: "selectionLabel"
                enabled: !root.connecting && count > 0
                displayText: count === 0 ? qsTr("No MIDI in") : currentIndex < 0 ? qsTr("MIDI in") : currentText
                currentIndex: root.devices.selectedInputIndex
                onActivated: function(index) { root.devices.selectedInputIndex = index }
                Accessible.name: qsTr("MIDI input")
            }

            XpComboBox {
                objectName: "outputPicker"
                visible: !root.connected
                Layout.fillWidth: true
                Layout.maximumWidth: 260
                Layout.alignment: Qt.AlignVCenter
                model: root.devices.outputs
                textRole: "selectionLabel"
                enabled: !root.connecting && count > 0
                displayText: count === 0 ? qsTr("No MIDI out") : currentIndex < 0 ? qsTr("MIDI out") : currentText
                currentIndex: root.devices.selectedOutputIndex
                onActivated: function(index) { root.devices.selectedOutputIndex = index }
                Accessible.name: qsTr("MIDI output")
            }

            Item { visible: root.connected; Layout.fillWidth: true }

            // The action that matters now, and only that one, as primary.
            XpButton {
                objectName: "connectButton"
                visible: !root.connected
                text: root.connecting ? qsTr("Cancel") : qsTr("Connect")
                variant: root.connecting ? "secondary" : "primary"
                compact: true
                enabled: root.connecting ? root.devices.canDisconnect : root.devices.canConnect
                Layout.alignment: Qt.AlignVCenter
                onClicked: root.connecting ? root.devices.disconnectDevice() : root.devices.connectDevice()
            }

            XpButton {
                objectName: "testConnectionButton"
                visible: root.connected
                text: qsTr("Test")
                variant: root.verified ? "ghost" : "primary"
                compact: true
                enabled: root.devices.canTestConnection
                Layout.alignment: Qt.AlignVCenter
                onClicked: root.devices.testConnection()
            }

            XpButton {
                objectName: "disconnectButton"
                visible: root.connected
                text: qsTr("Disconnect")
                variant: "ghost"
                compact: true
                enabled: root.devices.canDisconnect
                Layout.alignment: Qt.AlignVCenter
                onClicked: root.devices.disconnectDevice()
            }

            XpButton {
                objectName: "connectionOptionsButton"
                variant: "quiet"
                compact: true
                iconOnly: true
                iconName: root.detailsExpanded ? "chevron-up" : "chevron-down"
                Accessible.name: root.detailsExpanded ? qsTr("Hide MIDI settings")
                                                      : qsTr("Show MIDI settings")
                Layout.alignment: Qt.AlignVCenter
                onClicked: root.detailsExpanded = !root.detailsExpanded
            }
        }

        // --- Progress -------------------------------------------------------
        // An indeterminate sweep while a handshake is in flight. It occupies a
        // reserved 2 px line whether or not it is running, so the row below it
        // never moves when work starts.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 2
            radius: 1
            color: root.busy ? Theme.surfaceSunken : "transparent"
            clip: true

            Rectangle {
                id: sweep
                width: parent.width * 0.3
                height: parent.height
                radius: parent.radius
                color: root.accentEdge
                visible: root.busy && !Motion.reducedMotion
                SequentialAnimation on x {
                    running: sweep.visible
                    loops: Animation.Infinite
                    NumberAnimation { from: -sweep.width; to: sweep.parent.width; duration: 900; easing.type: Easing.InOutQuad }
                }
            }

            // With reduced motion the same state is shown as a static fill
            // rather than nothing at all.
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: root.accentEdge
                visible: root.busy && Motion.reducedMotion
            }
        }

        // --- What is happening ---------------------------------------------
        XpLabel {
            objectName: "connectionGuidance"
            visible: root.activity.length > 0
            text: root.activity
            role: "caption"
            color: root.failed ? Theme.error : Theme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // The verified handshake result, once, where it is the answer rather
        // than an instruction.
        XpLabel {
            objectName: "connectionTestMessage"
            visible: root.verified && root.devices.connectionTestMessage.length > 0
            text: root.devices.connectionTestMessage
            role: "caption"
            color: Theme.live
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // --- Rarely-changed settings ---------------------------------------
        ColumnLayout {
            visible: root.detailsExpanded
            Layout.fillWidth: true
            Layout.topMargin: Metrics.spacingXs
            Layout.bottomMargin: Metrics.spacingXs
            spacing: Metrics.spacingSm

            XpDivider { Layout.fillWidth: true; color: Theme.borderSubtle }

            XpFieldRow {
                label: qsTr("Device number")
                labelWidth: root.labelColumn
                hint: qsTr("Usually 17. For wireless MIDI, pair the adapter in system settings, then rescan.")
                XpSpinField {
                    objectName: "deviceIdField"
                    enabled: !root.connecting && !root.devices.transferBusy
                    from: root.devices.deviceIdMinimum
                    to: root.devices.deviceIdMaximum
                    value: root.devices.deviceId
                    onValueModified: root.devices.deviceId = value
                    Accessible.name: qsTr("XP-60 device number")
                }
            }

            XpFieldRow {
                label: qsTr("Speed")
                labelWidth: root.labelColumn
                XpComboBox {
                    objectName: "pacingPicker"
                    Layout.fillWidth: true
                    Layout.maximumWidth: 260
                    model: [qsTr("Normal"), qsTr("Slow (wireless)")]
                    currentIndex: root.devices.pacingProfile
                    enabled: !root.devices.hasOutstandingRequests && !root.devices.transferBusy
                    onActivated: function(index) { root.devices.pacingProfile = index }
                    Accessible.name: qsTr("Connection speed")
                }
            }

            XpFieldRow {
                label: qsTr("MIDI ports")
                labelWidth: root.labelColumn
                XpButton {
                    text: qsTr("Rescan")
                    iconName: "refresh"
                    variant: "ghost"
                    compact: true
                    enabled: !root.demoMode
                    onClicked: root.devices.refreshEndpoints()
                }
                XpLabel {
                    text: root.devices.backendName
                    role: "caption"
                    muted: true
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }
    }
}
