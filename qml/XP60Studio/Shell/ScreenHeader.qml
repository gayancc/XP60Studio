import QtQuick
import QtQuick.Layouts
import XP60Studio

// Screen title block with optional subtitle and trailing actions.
RowLayout {
    id: root

    property string title: ""
    property string subtitle: ""
    default property alias actions: actionsRow.data

    spacing: Metrics.spacingLg

    ColumnLayout {
        spacing: 2
        Layout.fillWidth: true
        XpLabel { text: root.title; role: "display"; Layout.fillWidth: true }
        XpLabel { visible: root.subtitle.length > 0; text: root.subtitle; secondary: true; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
    Row {
        id: actionsRow
        spacing: Metrics.spacingSm
        Layout.alignment: Qt.AlignTop
    }
}
