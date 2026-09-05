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
        AppHeader { shell: testShell; width: 800 }
    }

    Component {
        id: indicatorComponent
        ConnectionStatusIndicator { shell: testShell }
    }

    function init() {
        testDevices.disconnectDevice()
        testShell.navigate("devices")
    }

    // Collects every descendant carrying `name`, so a delegate deep inside a
    // Repeater can be measured.
    function descendants(item, name, found) {
        found = found || []
        for (var i = 0; i < item.children.length; ++i) {
            var child = item.children[i]
            if (child.objectName === name)
                found.push(child)
            descendants(child, name, found)
        }
        return found
    }

    function test_navigation_rail_lists_all_destinations_and_gates_unbuilt_ones() {
        var rail = createTemporaryObject(railComponent, testCase)
        verify(rail)
        compare(testShell.navigationItems.length, 8)
        var enabled = 0
        for (var i = 0; i < testShell.navigationItems.length; ++i) {
            if (testShell.navigationItems[i].enabled) enabled++
        }
        compare(enabled, 5) // Dashboard, Library, Editor, Banks and Devices
        compare(testShell.currentScreen, "devices")
        // Disabled destinations must not navigate.
        compare(testShell.navigate("performance"), false)
        compare(testShell.currentScreen, "devices")
        // Enabled ones must.
        compare(testShell.navigate("editor"), true)
        compare(testShell.currentScreen, "editor")
        testShell.navigate("devices")
    }

    // Regression guard. Navigation labels used to be pinned to their implicit
    // width, so "Performance" plus its availability text was drawn past the
    // rail's own divider and over the content area. No label may leave the
    // rail, at the rail's real width, for any destination.
    function test_navigation_labels_stay_inside_the_rail() {
        var rail = createTemporaryObject(railComponent, testCase)
        verify(rail)
        rail.width = Metrics.railWidth
        wait(0)
        var labels = descendants(rail, "navLabel")
        compare(labels.length, testShell.navigationItems.length)
        for (var i = 0; i < labels.length; ++i) {
            var right = labels[i].mapToItem(rail, labels[i].width, 0).x
            verify(right <= rail.width,
                   "navigation label " + i + " right edge " + right
                   + " exceeds rail width " + rail.width)
        }
    }

    // The shell view-model is the only place that maps connection state to a
    // tone and to wording; the indicator must not re-derive either.
    function test_shell_owns_the_connection_vocabulary() {
        var indicator = createTemporaryObject(indicatorComponent, testCase)
        verify(indicator)
        compare(testShell.connectionTone, indicator.tone)

        testDevices.connectDevice()
        tryCompare(testDevices, "connectionState", ConnectionState.Connected)
        compare(testShell.connectionPhase, "connected")
        compare(testShell.connectionTone, "warning")
        compare(testShell.connectionShortLabel, "Connected")
        compare(indicator.tone, "warning")

        testDevices.disconnectDevice()
        // Endpoints are still selected, so this is "available", not "offline":
        // the shell should not tell the user to go and find ports it can see.
        compare(testShell.connectionPhase, "available")
        compare(testShell.connectionShortLabel, "Ready to connect")
        compare(testShell.connectionTone, "neutral")
    }

    // The shell offers a jump to Devices only when that is useful.
    function test_connection_action_is_not_offered_on_devices() {
        testShell.navigate("devices")
        compare(testShell.connectionActionable, false)
        compare(testShell.connectionActionLabel, "")

        testShell.navigate("editor")
        compare(testShell.connectionActionable, true)
        compare(testShell.connectionActionLabel, "Set up")

        testShell.navigate("devices")
    }

    function test_header_follows_shell_connection() {
        var header = createTemporaryObject(headerComponent, testCase)
        verify(header)
        compare(testShell.connectionLabel, "Offline")
        testDevices.connectDevice()
        tryCompare(testDevices, "connectionState", ConnectionState.Connected)
        compare(testShell.connectionState, ConnectionState.Connected)
        compare(testShell.connectionLabel, "Connected")
        testDevices.disconnectDevice()
        compare(testShell.connectionLabel, "Offline")
    }
}
