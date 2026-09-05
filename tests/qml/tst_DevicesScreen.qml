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
        DevicesScreen {}
    }

    function makeScreen(extra) {
        var props = { devices: testDevices, shell: testShell, width: 1300, height: 900 }
        if (extra) {
            for (var key in extra)
                props[key] = extra[key]
        }
        return createTemporaryObject(screenComponent, testCase, props)
    }

    function init() {
        testDevices.disconnectDevice()
        testDevices.applyReadPreset(0)
        testDevices.clearLog()
    }

    function reveal(screen, control) {
        var flickable = findChild(screen, "devicesScroll").contentItem
        var position = control.mapToItem(flickable.contentItem, 0, 0)
        flickable.contentY = Math.max(0, Math.min(position.y - 20, flickable.contentHeight - flickable.height))
        wait(30)
    }

    function test_screen_loads_and_binds() {
        var screen = makeScreen()
        verify(screen)
        verify(screen.twoColumns)
        var connectButton = findChild(screen, "connectButton")
        var disconnectButton = findChild(screen, "disconnectButton")
        // Open advanced diagnostics to access expert controls
        var advToggle = findChild(screen, "advancedToggle")
        advToggle.checked = true
        wait(30)
        var sendButton = findChild(screen, "sendRequestButton")
        verify(connectButton)
        verify(disconnectButton)
        verify(sendButton)
        compare(connectButton.enabled, true)
        compare(disconnectButton.enabled, false)
        compare(sendButton.enabled, false)
        var inputPicker = findChild(screen, "inputPicker")
        verify(inputPicker)
        compare(inputPicker.count, 1)
        compare(inputPicker.displayText, "XP-60 IN · Loopback")
        var addressField = findChild(screen, "addressField")
        compare(addressField.text, "03 00 00 00")
        verify(findChild(screen, "enterDemoModeButton"))
    }

    function test_connect_flow_enables_request() {
        var screen = makeScreen()
        verify(screen)
        var connectButton = findChild(screen, "connectButton")
        var disconnectButton = findChild(screen, "disconnectButton")
        // Open advanced diagnostics to access expert controls
        var advToggle = findChild(screen, "advancedToggle")
        advToggle.checked = true
        wait(30)
        var sendButton = findChild(screen, "sendRequestButton")
        var pill = findChild(screen, "connectionPill")
        compare(pill.text, "Disconnected")
        mouseClick(connectButton)
        tryCompare(testDevices, "connectionState", ConnectionState.Connected)
        compare(pill.text, "Connected")
        compare(pill.tone, "warning")
        compare(connectButton.enabled, false)
        compare(disconnectButton.enabled, true)
        compare(sendButton.enabled, true)
        var inputPicker = findChild(screen, "inputPicker")
        compare(inputPicker.visible, false) // pickers hide while connected

        var logBefore = testDevices.log.count
        reveal(screen, sendButton)
        mouseClick(sendButton)
        verify(testDevices.hasOutstandingRequests)
        verify(testDevices.log.count > logBefore)
        var cancelButton = findChild(screen, "cancelRequestsButton")
        compare(cancelButton.enabled, true)
        reveal(screen, cancelButton)
        mouseClick(cancelButton)
        compare(testDevices.hasOutstandingRequests, false)
        compare(testDevices.operations.count >= 1, true)

        reveal(screen, disconnectButton)
        mouseClick(disconnectButton)
        compare(pill.text, "Disconnected")
        compare(sendButton.enabled, false)
    }

    function test_invalid_address_disables_send_and_shows_message() {
        var screen = makeScreen()
        verify(screen)
        testDevices.connectDevice()
        tryCompare(testDevices, "connectionState", ConnectionState.Connected)
        // Open advanced diagnostics to access expert controls
        var advToggle = findChild(screen, "advancedToggle")
        advToggle.checked = true
        wait(30)
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

    function test_connection_test_and_pacing_are_bound_to_the_session() {
        var screen = makeScreen()
        var probe = findChild(screen, "testConnectionButton")
        var pacing = findChild(screen, "pacingPicker")
        verify(probe)
        verify(pacing)
        compare(probe.enabled, false)
        testDevices.pacingProfile = 1
        compare(pacing.currentIndex, 1)
        testDevices.connectDevice()
        tryCompare(testDevices, "connectionState", ConnectionState.Connected)
        compare(probe.enabled, true)
        reveal(screen, probe)
        mouseClick(probe)
        verify(testDevices.hasOutstandingRequests)
        compare(probe.enabled, false)
        compare(pacing.enabled, false)
        compare(testDevices.connectionVerified, false)
        verify(findChild(screen, "connectionTestMessage").text.indexOf("Reading") >= 0)
        testDevices.cancelAllRequests()
        compare(probe.enabled, true)
        testDevices.pacingProfile = 0
    }

    function test_patch_fetch_button_follows_connection() {
        var screen = makeScreen()
        verify(screen)
        var fetchButton = findChild(screen, "fetchPatchButton")
        var pill = findChild(screen, "patchFetchPill")
        verify(fetchButton)
        verify(pill)
        compare(fetchButton.enabled, false)
        compare(pill.text, "Not read yet")
        testDevices.connectDevice()
        tryCompare(testDevices, "connectionState", ConnectionState.Connected)
        compare(fetchButton.enabled, true)
        mouseClick(fetchButton)
        compare(testDevices.patchFetchInProgress, true)
        compare(pill.text, "Reading…")
        compare(fetchButton.enabled, false)
        compare(testDevices.patchFetchTotalBlocks, 5)
        testDevices.cancelPatchFetch()
        compare(testDevices.patchFetchInProgress, false)
        compare(pill.text, "Failed")
        testDevices.disconnectDevice()
        compare(fetchButton.enabled, false)
    }

    function test_write_is_gated_behind_a_read_then_arming() {
        var screen = makeScreen()
        verify(screen)
        var armButton = findChild(screen, "armWriteButton")
        var writeButton = findChild(screen, "writeVerifyButton")
        var pill = findChild(screen, "transferPill")
        var reason = findChild(screen, "armBlockedReason")
        verify(armButton)
        verify(writeButton)
        verify(pill)

        // Offline: neither arming nor writing is possible, and the reason is shown.
        compare(armButton.enabled, false)
        compare(writeButton.enabled, false)
        compare(pill.text, "Idle")
        compare(reason.visible, true)
        verify(reason.text.indexOf("Connect") >= 0)

        // Connected but no Patch read yet: still refused, with a different reason.
        testDevices.connectDevice()
        tryCompare(testDevices, "connectionState", ConnectionState.Connected)
        compare(armButton.enabled, false)
        compare(writeButton.enabled, false)
        verify(reason.text.indexOf("Read the current sound first") >= 0)

        // Clicking arm does nothing while it is refused.
        mouseClick(armButton)
        compare(testDevices.writeArmed, false)

        // The mismatch panel only exists when there is a mismatch to report.
        var mismatch = findChild(screen, "mismatchPanel")
        compare(mismatch.visible, false)
    }

    function test_stacks_to_one_column_when_narrow() {
        var screen = makeScreen()
        verify(screen)
        screen.width = 900
        compare(screen.twoColumns, false)
    }
}
