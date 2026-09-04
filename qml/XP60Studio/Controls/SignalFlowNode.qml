import QtQuick
import QtQuick.Layouts
import XP60Studio

// One box in the Tones → Structure → MFX → Chorus → Reverb → Output path.
Rectangle {
    id: root

    property string title: ""
    property string detail: ""
    property bool highlighted: false

    implicitWidth: Math.max(120, column.implicitWidth + 2 * Metrics.spacingMd)
    implicitHeight: 52
    radius: Metrics.radiusSm
    color: highlighted ? Theme.accentSoft : Theme.surfaceRaised
    border.width: Metrics.borderWidth
    border.color: highlighted ? Theme.accent : Theme.borderStrong

    Accessible.role: Accessible.StaticText
    Accessible.name: root.title + " " + root.detail

    ColumnLayout {
        id: column
        anchors.centerIn: parent
        spacing: 1
        XpLabel {
            text: root.title
            role: "overline"
            color: root.highlighted ? Theme.accentText : Theme.textSecondary
            Layout.alignment: Qt.AlignHCenter
        }
        XpLabel {
            visible: root.detail.length > 0
            text: root.detail
            role: "caption"
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
