import QtQuick
import QtTest
import XP60Studio
import XP60Studio.Presentation

// The Devices screen against the real DevicesViewModel + loopback transport.
TestCase {
    id: testCase
    name: "DevicesScreen"
    when: windowShown
    width: 1300
    height: 900
    visible: true

    Component {
        id: screenComponent
        DevicesScreen { devices: testDevices; width: 1300; height: 900 }
    }

    function init() {
        testDevices.disconnectDevice()
        testDevices.applyReadPreset(0)
        testDevices.clearLog()
    }

    function test_screen_loads_and_binds() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen)
        verify(screen.twoColumns)
        var connectButton = findChild(screen, "connectButton")
        var disconnectButton = findChild(screen, "disconnectButton")
        var sendButton = findChild(screen, "sendRequestButton")
        var dataSetButton = findChild(screen, "dataSetButton")
        verify(connectButton)
        verify(disconnectButton)
        verify(sendButton)
        verify(dataSetButton)
        compare(connectButton.enabled, true)
        compare(disconnectButton.enabled, false)
        compare(sendButton.enabled, false)
        compare(dataSetButton.enabled, false)
        var inputPicker = findChild(screen, "inputPicker")
        verify(inputPicker)
        compare(inputPicker.count, 1)
        compare(inputPicker.displayText, "XP-60 IN")
        var addressField = findChild(screen, "addressField")
        compare(addressField.text, "03 00 00 00")
    }

    function test_connect_flow_enables_request() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen)
        var connectButton = findChild(screen, "connectButton")
        var disconnectButton = findChild(screen, "disconnectButton")
        var sendButton = findChild(screen, "sendRequestButton")
        var pill = findChild(screen, "connectionPill")
        compare(pill.text, "Disconnected")
        mouseClick(connectButton)
        compare(testDevices.connectionState, ConnectionState.Connected)
        compare(pill.text, "Connected")
        compare(pill.tone, "live")
        compare(connectButton.enabled, false)
        compare(disconnectButton.enabled, true)
        compare(sendButton.enabled, true)
        var inputPicker = findChild(screen, "inputPicker")
        compare(inputPicker.enabled, false) // pickers lock while connected

        var logBefore = testDevices.log.count
        mouseClick(sendButton)
        verify(testDevices.hasOutstandingRequests)
        verify(testDevices.log.count > logBefore)
        var cancelButton = findChild(screen, "cancelRequestsButton")
        compare(cancelButton.enabled, true)
        mouseClick(cancelButton)
        compare(testDevices.hasOutstandingRequests, false)
        compare(testDevices.operations.count >= 1, true)

        mouseClick(disconnectButton)
        compare(pill.text, "Disconnected")
        compare(sendButton.enabled, false)
    }

    function test_invalid_address_disables_send_and_shows_message() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen)
        testDevices.connectDevice()
        var sendButton = findChild(screen, "sendRequestButton")
        var addressField = findChild(screen, "addressField")
        var validation = findChild(screen, "requestValidation")
        compare(sendButton.enabled, true)
        addressField.forceActiveFocus()
        addressField.selectAll()
        keyClick(Qt.Key_8)
        keyClick(Qt.Key_0)
        compare(testDevices.requestAddress, "80")
        compare(addressField.invalid, true)
        compare(sendButton.enabled, false)
        verify(validation.text.indexOf("Address") === 0)
        compare(validation.color, Theme.error)
    }

    function test_stacks_to_one_column_when_narrow() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen)
        screen.width = 900
        compare(screen.twoColumns, false)
    }
}
