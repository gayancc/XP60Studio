import QtQuick
import QtQuick.Layouts
import XP60Studio

// Centered message for empty panels and unavailable destinations.
Item {
    id: root

    property string glyph: "◌"
    property string title: ""
    property string message: ""

    implicitHeight: column.implicitHeight + 2 * Metrics.spacingXl

    ColumnLayout {
        id: column
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Metrics.spacingXl, 420)
        spacing: Metrics.spacingSm
        XpLabel { text: root.glyph; role: "display"; muted: true; Layout.alignment: Qt.AlignHCenter }
        XpLabel { text: root.title; role: "heading"; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        XpLabel { text: root.message; secondary: true; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    }
}
