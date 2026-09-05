import QtQuick
import QtQuick.Layouts
import XP60Studio

// Screen title block with optional subtitle and trailing actions.
//
// The title line is only laid out when there is a title. Devices passes an
// empty one — the global header already names the screen — and the old
// unconditional display-size label still reserved its full line height, which
// left a 40 px empty band with the screen's action button orphaned above the
// subtitle.
RowLayout {
    id: root

    property string title: ""
    property string subtitle: ""
    default property alias actions: actionsRow.data

    spacing: Metrics.spacingLg

    ColumnLayout {
        spacing: 2
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignVCenter

        XpLabel {
            visible: root.title.length > 0
            text: root.title
            role: "display"
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        XpLabel {
            visible: root.subtitle.length > 0
            text: root.subtitle
            role: "body"
            secondary: true
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    // Actions sit on the subtitle's centre line when there is no title, and at
    // the top of the block when there is, so a one-line header stays one line.
    Row {
        id: actionsRow
        spacing: Metrics.spacingSm
        Layout.alignment: root.title.length > 0 ? Qt.AlignTop : Qt.AlignVCenter
    }
}
