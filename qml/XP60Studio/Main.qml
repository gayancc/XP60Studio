import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Application shell: navigation rail | header + screen area.
QQC.ApplicationWindow {
    id: window

    required property AppShellViewModel shell
    required property DevicesViewModel devices
    required property PatchEditorViewModel editor
    required property LibraryListModel library
    required property LibraryTransferViewModel libraryTransfer
    required property BankBuilderViewModel bankBuilder
    required property ExpansionViewModel expansion
    required property PerformanceViewModel performance
    required property LibraryListModel bankLibrary
    required property DashboardViewModel dashboard

    visible: true
    width: Metrics.windowPreferredWidth
    height: Metrics.windowPreferredHeight
    minimumWidth: Metrics.windowMinWidth
    minimumHeight: Metrics.windowMinHeight
    title: shell.appName + " — " + shell.currentScreenTitle
    color: Theme.windowBackground

    RowLayout {
        anchors.fill: parent
        spacing: 0

        AppNavigationRail {
            shell: window.shell
            Layout.fillHeight: true
            Layout.preferredWidth: Metrics.railWidth
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            AppHeader {
                shell: window.shell
                Layout.fillWidth: true
            }

            Item {
                id: screenHost
                objectName: "screenHost"
                Layout.fillWidth: true
                Layout.fillHeight: true

                DevicesScreen {
                    objectName: "devicesScreen"
                    anchors.fill: parent
                    visible: window.shell.currentScreen === "devices"
                    devices: window.devices
                    shell: window.shell
                }

                EditorScreen {
                    objectName: "editorScreen"
                    anchors.fill: parent
                    visible: window.shell.currentScreen === "editor"
                    editor: window.editor
                    expansion: window.expansion
                }

                DashboardScreen {
                    objectName: "dashboardScreen"
                    anchors.fill: parent
                    visible: window.shell.currentScreen === "dashboard"
                    shell: window.shell
                    editor: window.editor
                    dashboard: window.dashboard
                    devices: window.devices
                }

                LibraryScreen {
                    objectName: "libraryScreen"
                    anchors.fill: parent
                    visible: window.shell.currentScreen === "library"
                    library: window.library
                    transfer: window.libraryTransfer
                    onEditRequested: window.shell.navigate("editor")
                }

                BankBuilderScreen {
                    objectName: "bankBuilderScreen"
                    anchors.fill: parent
                    visible: window.shell.currentScreen === "banks"
                    builder: window.bankBuilder
                    library: window.bankLibrary
                    transfer: window.libraryTransfer
                    onEditRequested: window.shell.navigate("editor")
                }

                PerformanceScreen {
                    objectName: "performanceScreen"
                    anchors.fill: parent
                    visible: window.shell.currentScreen === "performance"
                    performance: window.performance
                }

                ExpansionScreen {
                    objectName: "expansionScreen"
                    anchors.fill: parent
                    visible: window.shell.currentScreen === "expansion"
                    expansion: window.expansion
                }

                UnavailableScreen {
                    anchors.fill: parent
                    visible: window.shell.currentScreen !== "devices" && window.shell.currentScreen !== "editor"
                             && window.shell.currentScreen !== "library"
                             && window.shell.currentScreen !== "banks"
                             && window.shell.currentScreen !== "dashboard"
                             && window.shell.currentScreen !== "expansion"
                             && window.shell.currentScreen !== "performance"
                    screenTitle: window.shell.currentScreenTitle
                    availability: "a later phase"
                }
            }
        }
    }
}
