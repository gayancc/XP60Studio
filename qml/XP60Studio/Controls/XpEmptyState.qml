import QtQuick
import QtQuick.Layouts
import XP60Studio

// A designed empty region.
//
// "No operations yet" dropped into a bordered rectangle told the user nothing:
// not what the area is for, not why it is empty, not what will make it fill.
// An empty state here states the subject, says what will appear, and — when
// there is one — offers the action that ends the empty state. It draws no
// border of its own: it lives inside a panel that already has one.
//
// It is deliberately compact. This is desktop software, and an empty
// diagnostics list should not occupy the space of a hero banner.
Item {
    id: root

    property string iconName: ""
    property string title: ""
    // What will appear here, and what produces it. One sentence.
    property string message: ""
    // Optional action that resolves the empty state.
    property string actionText: ""
    property bool actionEnabled: true
    signal actionTriggered()

    // Sized by its content rather than by an arbitrary height, so a panel does
    // not gain or lose height when its list becomes empty.
    implicitHeight: column.implicitHeight + 2 * Metrics.spacingXl
    implicitWidth: column.implicitWidth

    ColumnLayout {
        id: column
        anchors.centerIn: parent
        width: Math.min(root.width - 2 * Metrics.spacingLg, 360)
        spacing: Metrics.spacingSm

        XpIcon {
            visible: root.iconName.length > 0
            name: root.iconName
            color: Theme.textDisabled
            implicitWidth: Metrics.iconSizeLg
            implicitHeight: Metrics.iconSizeLg
            Layout.alignment: Qt.AlignHCenter
        }

        XpLabel {
            visible: root.title.length > 0
            text: root.title
            role: "subheading"
            secondary: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        XpLabel {
            visible: root.message.length > 0
            text: root.message
            role: "caption"
            muted: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        XpButton {
            visible: root.actionText.length > 0
            text: root.actionText
            enabled: root.actionEnabled
            compact: true
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Metrics.spacingXs
            onClicked: root.actionTriggered()
        }
    }
}
