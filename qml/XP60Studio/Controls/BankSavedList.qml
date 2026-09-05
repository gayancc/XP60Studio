import QtQuick
import QtQuick.Layouts
import XP60Studio

// The saved User banks, as the Bank Builder's drawer shows them.
//
// A saved bank is an arrangement, so the row says what matters about an
// arrangement: its name, how many of the 128 destinations are filled, when it
// was last written, and — in the one case that needs saying out loud — how
// many of its destinations reference a Patch that has since been deleted from
// the library.
//
// Deleting a saved bank deletes the arrangement and nothing else. The wording
// says so, because "Delete" next to a list of banks is exactly where a user
// would reasonably fear losing sounds.
ColumnLayout {
    id: root

    required property var builder

    signal bankOpened()
    signal closeRequested()

    spacing: Metrics.spacingSm

    RowLayout {
        Layout.fillWidth: true
        XpLabel { text: qsTr("SAVED BANKS"); role: "overline"; secondary: true }
        Item { Layout.fillWidth: true }
        XpButton {
            objectName: "bankSavedClose"
            iconName: "close"
            iconOnly: true
            compact: true
            variant: "ghost"
            onClicked: root.closeRequested()
        }
    }

    XpEmptyState {
        Layout.fillWidth: true
        visible: root.builder.savedBanks.length === 0
        title: qsTr("No saved banks yet")
        message: qsTr("Arrange destinations on the panel, then choose Save as new bank.")
    }

    Repeater {
        model: root.builder.savedBanks
        delegate: Rectangle {
            required property var modelData

            Layout.fillWidth: true
            Layout.preferredHeight: 48
            radius: Metrics.radiusSm
            color: openArea.containsMouse ? Theme.surfaceHover : Theme.surfaceSunken
            border.width: 1
            border.color: Theme.borderSubtle

            Behavior on color {
                enabled: !Motion.reducedMotion
                ColorAnimation { duration: Motion.durationFast }
            }

            MouseArea {
                id: openArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    root.builder.loadBank(modelData.id)
                    root.bankOpened()
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: Metrics.spacingSm
                spacing: Metrics.spacingSm

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    XpLabel {
                        Layout.fillWidth: true
                        text: modelData.name
                        role: "value"
                        elide: Text.ElideRight
                    }
                    XpLabel {
                        Layout.fillWidth: true
                        text: modelData.missing > 0
                              ? qsTr("%1 of 128 · %2 missing · %3")
                                .arg(modelData.occupied).arg(modelData.missing).arg(modelData.updated)
                              : qsTr("%1 of 128 · %2").arg(modelData.occupied).arg(modelData.updated)
                        role: "caption"
                        color: modelData.missing > 0 ? Theme.warning : Theme.textMuted
                        elide: Text.ElideRight
                    }
                }

                XpButton {
                    text: qsTr("Delete")
                    compact: true
                    variant: "ghost"
                    // Deletes the arrangement only. Every Patch it referenced
                    // stays in the library.
                    onClicked: root.builder.deleteBank(modelData.id)
                }
            }
        }
    }
}
