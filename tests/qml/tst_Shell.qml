import QtQuick
import QtTest
import XP60Studio
import XP60Studio.Presentation

// Shell components bound to the real AppShellViewModel (loopback backed).
TestCase {
    id: testCase
    name: "Shell"
    when: windowShown
    width: 900
    height: 600
    visible: true

    Component {
        id: railComponent
        AppNavigationRail { shell: testShell; height: 600 }
    }

    Component {
        id: headerComponent
        AppHeader { shell: testShell; backendName: "Loopback"; width: 800 }
    }

    Component {
        id: indicatorComponent
        ConnectionStatusIndicator { connectionState: ConnectionState.Disconnected; label: "XP-60 OFFLINE" }
    }

    function init() {
        testDevices.disconnectDevice()
    }

    function test_navigation_rail_lists_all_destinations_and_gates_unbuilt_ones() {
        var rail = createTemporaryObject(railComponent, testCase)
        verify(rail)
        compare(testShell.navigationItems.length, 8)
        var enabled = 0
        for (var i = 0; i < testShell.navigationItems.length; ++i) {
            if (testShell.navigationItems[i].enabled) enabled++
        }
        compare(enabled, 2) // Devices and Editor
        compare(testShell.currentScreen, "devices")
        // Disabled destinations must not navigate.
        compare(testShell.navigate("library"), false)
        compare(testShell.currentScreen, "devices")
        // Enabled ones must.
        compare(testShell.navigate("editor"), true)
        compare(testShell.currentScreen, "editor")
        testShell.navigate("devices")
    }

    function test_connection_indicator_reflects_state_by_text_and_tone() {
        var indicator = createTemporaryObject(indicatorComponent, testCase)
        verify(indicator)
        compare(indicator.tone, "neutral")
        indicator.connectionState = ConnectionState.Connected
        compare(indicator.tone, "live")
        indicator.connectionState = ConnectionState.Error
        compare(indicator.tone, "error")
        indicator.connectionState = ConnectionState.Connecting
        compare(indicator.tone, "warning")
    }

    function test_header_follows_shell_connection() {
        var header = createTemporaryObject(headerComponent, testCase)
        verify(header)
        compare(testShell.connectionLabel, "XP-60 OFFLINE")
        testDevices.connectDevice()
        compare(testShell.connectionState, ConnectionState.Connected)
        compare(testShell.connectionLabel, "XP-60 LIVE")
        testDevices.disconnectDevice()
        compare(testShell.connectionLabel, "XP-60 OFFLINE")
    }
}
