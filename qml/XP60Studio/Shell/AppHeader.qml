import QtQuick
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Global header: screen title on the left, XP-60 identity in the centre,
// version on the right.
//
// The centre used to be a pill containing the whole connection detail, which
// overflowed the header. It is now a fixed-height chip carrying awareness and
// at most one action; anything longer belongs on Devices.
Rectangle {
    id: root

    required property AppShellViewModel shell

    implicitHeight: Metrics.headerHeight
    color: Theme.headerBackground

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }

    // Three-part layout with the two outer cells given equal width, so the
    // chip is optically centred in the header rather than centred in whatever
    // space the title happened to leave. A fixed 220 px title cell used to
    // elide long titles and mis-centre the chip at the same time.
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Metrics.screenPadding
        anchors.rightMargin: Metrics.screenPadding
        spacing: Metrics.spacingLg

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignVCenter
            spacing: Metrics.spacingSm

            XpLabel {
                text: root.shell.currentScreenTitle
                role: "heading"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: Metrics.spacingSm

            StatusPill {
                visible: root.shell.demoMode
                text: "Demo Mode"
                tone: "info"
                showDot: false
            }

            XpButton {
                visible: root.shell.demoMode
                text: root.shell.demoModeSwitchPending ? qsTr("Switching…") : qsTr("Exit Demo")
                variant: "ghost"
                compact: true
                enabled: !root.shell.demoModeSwitchPending
                onClicked: root.shell.exitDemoMode()
            }

            ConnectionStatusIndicator {
                shell: root.shell
                onActionTriggered: root.shell.navigate("devices")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignVCenter
            spacing: Metrics.spacingSm

            Item { Layout.fillWidth: true }

            XpLabel {
                text: "v" + root.shell.appVersion
                role: "caption"
                muted: true
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }
}
