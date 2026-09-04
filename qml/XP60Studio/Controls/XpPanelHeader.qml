import QtQuick
import QtQuick.Layouts
import XP60Studio

// Overline title row with optional trailing content (actions / pills).
RowLayout {
    id: root

    property string title: ""
    property string glyph: ""
    property string iconName: ""
    default property alias trailing: trailingRow.data

    spacing: Metrics.spacingSm
    Layout.fillWidth: true
    XpIcon {
        visible: root.iconName.length > 0
        name: root.iconName
        color: Theme.accentText
    }

    XpLabel {
        visible: root.glyph.length > 0
        text: root.glyph
        color: Theme.accentText
        role: "heading"
    }
    XpLabel {
        text: root.title
        role: "overline"
        secondary: true
        Layout.fillWidth: true
    }
    Row {
        id: trailingRow
        spacing: Metrics.spacingSm
    }
}
