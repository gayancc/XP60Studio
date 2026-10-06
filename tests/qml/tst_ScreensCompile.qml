import QtQuick
import QtTest

// Every screen, and the shell that assembles them, must compile.
//
// This exists because a real defect got past the whole suite: a `Row` was given
// `verticalItemAlignment`, which only `Grid` and `Flow` have. Every test passed
// — none of them instantiates the Expansion screen — while the application
// itself could not start, because Main.qml could not resolve the type.
//
// `Qt.createComponent` compiles a component without instantiating it, so this
// needs none of the screens' view models and still catches an unknown property,
// a bad import, a renamed control or a syntax error anywhere in the tree that
// Main.qml pulls in.
TestCase {
    id: testCase
    name: "ScreensCompile"

    readonly property var screens: [
        "BankBuilderScreen", "DashboardScreen", "DevicesScreen", "EditorScreen",
        "ExpansionScreen", "LibraryScreen", "PerformanceScreen",
        "UnavailableScreen", "WaveBrowserScreen"
    ]

    function compile(url) {
        var component = Qt.createComponent(url, Component.PreferSynchronous)
        return component
    }

    function test_every_screen_compiles_data() {
        var rows = []
        for (var i = 0; i < screens.length; ++i)
            rows.push({ tag: screens[i], file: screens[i] })
        return rows
    }

    function test_every_screen_compiles(row) {
        var component = compile("qrc:/qt/qml/XP60Studio/Screens/" + row.file + ".qml")
        verify(component, row.file + " produced no component")
        compare(component.status, Component.Ready,
                row.file + " did not compile: " + component.errorString())
        component.destroy()
    }

    // The shell is the one component that reaches every screen and nearly every
    // control, so it is the broadest single guard against a type going missing.
    function test_the_application_root_compiles() {
        var component = compile("qrc:/qt/qml/XP60Studio/Main.qml")
        verify(component, "Main.qml produced no component")
        compare(component.status, Component.Ready,
                "Main.qml did not compile: " + component.errorString())
        component.destroy()
    }
}
