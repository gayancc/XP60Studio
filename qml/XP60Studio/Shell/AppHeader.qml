import QtQuick
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Global header: screen title, live XP-60 identity in the centre, muted version.
Rectangle {
    id: root

    required property AppShellViewModel shell

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
            verified: root.shell.connectionVerified
            label: root.shell.connectionLabel
            detail: root.shell.connectionDetail
            showDetail: root.width >= 1100
            Layout.alignment: Qt.AlignVCenter
        }

        Item { Layout.fillWidth: true }

        XpLabel {
            text: "v" + root.shell.appVersion
            role: "caption"
            muted: true
        }
    }
}
