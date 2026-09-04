import QtQuick
import QtQuick.Layouts
import XP60Studio

// Metric card as in the Bank Builder summary row of the mockup:
// overline label, large value, tinted border.
Rectangle {
    id: root

    property string label: ""
    property string value: ""
    property string tone: "neutral"
    property string hint: ""

    readonly property color toneColor: Theme.toneForeground(tone)

    implicitWidth: 140
    implicitHeight: column.implicitHeight + 2 * Metrics.spacingMd
    radius: Metrics.radiusMd
    color: tone === "neutral" ? Theme.surfaceRaised : Theme.toneBackground(tone)
    border.width: Metrics.borderWidth
    border.color: tone === "neutral" ? Theme.border : Qt.rgba(toneColor.r, toneColor.g, toneColor.b, 0.45)

    ColumnLayout {
        id: column
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: Metrics.spacingMd }
        spacing: 2
        XpLabel { text: root.label; role: "overline"; color: root.tone === "neutral" ? Theme.textSecondary : root.toneColor; Layout.fillWidth: true }
        XpLabel { text: root.value; role: "title"; Layout.fillWidth: true }
        XpLabel { visible: root.hint.length > 0; text: root.hint; role: "caption"; muted: true; Layout.fillWidth: true }
    }
}
