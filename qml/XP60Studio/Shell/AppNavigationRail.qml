import QtQuick
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Persistent left navigation as in the master mockup: wordmark, primary
// destinations, and the device summary card at the bottom.
Rectangle {
    id: root

    required property AppShellViewModel shell

    implicitWidth: Metrics.railWidth
    color: Theme.railBackground

    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.border }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.spacingMd
        spacing: Metrics.spacingXs

        // Wordmark
        Row {
            Layout.leftMargin: Metrics.spacingSm
            Layout.topMargin: Metrics.spacingSm
            Layout.bottomMargin: Metrics.spacingLg
            spacing: 0
            XpLabel { text: "XP60"; role: "title"; color: Theme.accentText }
            XpLabel { text: "Studio"; role: "title"; font.weight: Typography.weightRegular }
        }

        Repeater {
            model: root.shell.navigationItems
            delegate: Rectangle {
                id: item
                required property var modelData
                readonly property bool current: root.shell.currentScreen === modelData.key
                readonly property bool available: modelData.enabled === true

                Layout.fillWidth: true
                implicitHeight: 36
                radius: Metrics.radiusSm
                color: current ? Theme.accentSoft : (hover.hovered && available ? Theme.surfaceHover : "transparent")
                border.width: current ? 1 : 0
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.5)

                Accessible.role: Accessible.Button
                Accessible.name: modelData.label + (available ? "" : ", available in " + modelData.availability)
                activeFocusOnTab: available
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                        root.shell.navigate(modelData.key)
                        event.accepted = true
                    }
                }

                Rectangle {
                    visible: item.activeFocus
                    anchors.fill: parent
                    radius: parent.radius
                    color: "transparent"
                    border.width: 1
                    border.color: Theme.focusRing
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Metrics.spacingMd
                    anchors.rightMargin: Metrics.spacingSm
                    spacing: Metrics.spacingMd
                    XpLabel {
                        text: item.modelData.glyph
                        color: item.current ? Theme.accentText : (item.available ? Theme.textSecondary : Theme.textDisabled)
                        Layout.preferredWidth: Metrics.iconSize
                        horizontalAlignment: Text.AlignHCenter
                    }
                    XpLabel {
                        text: item.modelData.label
                        color: item.current ? Theme.textPrimary : (item.available ? Theme.textSecondary : Theme.textDisabled)
                        font.weight: item.current ? Typography.weightMedium : Typography.weightRegular
                        Layout.fillWidth: true
                        Layout.minimumWidth: implicitWidth
                    }
                    XpLabel {
                        visible: !item.available
                        text: item.modelData.availability
                        role: "overline"
                        font.pointSize: Typography.overlineSize - 1
                        color: Theme.textDisabled
                    }
                }

                HoverHandler { id: hover; enabled: item.available }
                TapHandler { enabled: item.available; onTapped: root.shell.navigate(item.modelData.key) }
            }
        }

        Item { Layout.fillHeight: true }

        // Device summary card (mockup: "XP-60 LIVE / Connected" with device art)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: deviceColumn.implicitHeight + 2 * Metrics.spacingMd
            radius: Metrics.radiusMd
            color: Theme.surface
            border.width: Metrics.borderWidth
            border.color: Theme.border

            ColumnLayout {
                id: deviceColumn
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: Metrics.spacingMd }
                spacing: Metrics.spacingSm
                ConnectionStatusIndicator {
                    connectionState: root.shell.connectionState
                    label: root.shell.connectionLabel
                    compact: true
                }
                XpLabel {
                    text: root.shell.deviceName
                    role: "caption"
                    secondary: true
                    Layout.fillWidth: true
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 34
                    radius: Metrics.radiusSm
                    color: Theme.surfaceSunken
                    border.width: 1
                    border.color: Theme.borderSubtle
                    // Stylised keyboard strip standing in for the device image.
                    Row {
                        anchors.centerIn: parent
                        spacing: 2
                        Repeater {
                            model: 14
                            Rectangle { width: 6; height: 18; radius: 1; color: index % 2 ? Theme.borderStrong : Theme.textMuted; opacity: 0.6 }
                        }
                    }
                }
            }
        }
    }
}
