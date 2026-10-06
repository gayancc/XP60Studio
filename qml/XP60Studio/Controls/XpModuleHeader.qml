import QtQuick
import QtQuick.Layouts
import XP60Studio

// Compact header strip that starts a module inside a panel.
//
// Every sound-shaping module reads the same way: a status lamp, the module's
// name, whatever selector belongs to that module, then trailing badges. Giving
// the row its own tinted band instead of a floating overline is what lets the
// modules stack tightly and still be told apart — a panel heading that is only
// text needs whitespace around it to register as a heading, and that whitespace
// is what pushed the Editor past two screens.
Rectangle {
    id: root

    property string title: ""
    property string iconName: ""
    // Lamp colour. Transparent hides the lamp entirely (modules the XP-60 does
    // not switch on and off, such as Key Range).
    property color lampColor: "transparent"
    property bool lampOn: true
    // Tints the band and the title, for Tone-owned modules.
    property color accentColor: "transparent"
    // Inline module controls (a selector, a mode switch) sit between the title
    // and the trailing badges, the way a hardware module labels its own row.
    default property alias controls: controlRow.data
    property alias trailing: trailingRow.data

    Layout.fillWidth: true
    implicitHeight: Metrics.moduleHeaderHeight
    radius: Metrics.radiusSm
    color: root.accentColor.a > 0 ? Qt.rgba(root.accentColor.r, root.accentColor.g,
                                            root.accentColor.b, 0.10)
                                  : Theme.surfaceRaised
    border.width: Metrics.borderWidth
    border.color: root.accentColor.a > 0 ? Qt.rgba(root.accentColor.r, root.accentColor.g,
                                                   root.accentColor.b, 0.28)
                                         : Theme.borderSubtle

    RowLayout {
        anchors {
            fill: parent
            leftMargin: Metrics.spacingSm
            rightMargin: Metrics.spacingXs
        }
        spacing: Metrics.spacingSm

        // Status lamp, in the position a hardware module puts its LED.
        Rectangle {
            visible: root.lampColor.a > 0
            Layout.alignment: Qt.AlignVCenter
            width: 7
            height: 7
            radius: 3.5
            color: root.lampOn ? root.lampColor : Theme.surfaceSunken
            border.width: 1
            border.color: root.lampOn ? root.lampColor : Theme.borderStrong
        }

        XpIcon {
            visible: root.iconName.length > 0
            name: root.iconName
            implicitWidth: Metrics.iconSizeSm
            implicitHeight: Metrics.iconSizeSm
            color: root.accentColor.a > 0 ? root.accentColor : Theme.textMuted
            Layout.alignment: Qt.AlignVCenter
        }

        XpLabel {
            text: root.title
            role: "overline"
            color: root.accentColor.a > 0 ? root.accentColor : Theme.textSecondary
            Layout.alignment: Qt.AlignVCenter
        }

        RowLayout {
            id: controlRow
            spacing: Metrics.spacingXs
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
        }

        Row {
            id: trailingRow
            spacing: Metrics.spacingXs
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
