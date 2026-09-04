import QtQuick
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Global header: current screen title, the live connection indicator in the
// centre (as in the mockup), backend badge and version on the right.
Rectangle {
    id: root

    required property AppShellViewModel shell
    property string backendName: ""

    implicitHeight: Metrics.headerHeight
    color: Theme.headerBackground

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Metrics.screenPadding
        anchors.rightMargin: Metrics.screenPadding
        spacing: Metrics.spacingLg

        XpLabel {
            text: root.shell.currentScreenTitle
            role: "heading"
            Layout.preferredWidth: 220
        }

        Item { Layout.fillWidth: true }

        ConnectionStatusIndicator {
            connectionState: root.shell.connectionState
            label: root.shell.connectionLabel
            detail: root.shell.connectionDetail
            Layout.alignment: Qt.AlignVCenter
        }

        Item { Layout.fillWidth: true }

        StatusPill {
            text: "MIDI"
            tone: "accent"
            showDot: false
            Accessible.description: root.backendName
        }
        XpLabel {
            text: root.backendName
            role: "caption"
            muted: true
            elide: Text.ElideMiddle
            Layout.maximumWidth: 260
        }
        XpLabel {
            text: "v" + root.shell.appVersion
            role: "caption"
            muted: true
        }
    }
}
