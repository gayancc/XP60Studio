import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import XP60Studio

// Processor or destination in a configuration routing diagram.
Rectangle {
    id: root

    property string title: ""
    property string detail: ""
    property bool highlighted: false
    property color accentColor: Theme.accent
    property int detailLines: 2

    implicitWidth: Math.max(120, column.implicitWidth + 2 * Metrics.spacingMd)
    implicitHeight: 52
    radius: Metrics.radiusSm
    color: highlighted ? Theme.accentSoft : Theme.surfaceRaised
    border.width: Metrics.borderWidth
    border.color: highlighted ? accentColor : Theme.borderStrong

    Accessible.role: Accessible.StaticText
    Accessible.name: root.title + " " + root.detail
    HoverHandler { id: hover }
    QQC.ToolTip.visible: hover.hovered
    QQC.ToolTip.delay: 500
    QQC.ToolTip.text: root.title + " · " + root.detail

    ColumnLayout {
        id: column
        anchors { fill: parent; margins: Metrics.spacingXs }
        spacing: 1
        XpLabel {
            text: root.title
            role: "overline"
            color: root.highlighted ? Theme.accentText : Theme.textSecondary
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
        XpLabel {
            visible: root.detail.length > 0
            text: root.detail
            role: "caption"
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            maximumLineCount: root.detailLines
            elide: Text.ElideRight
        }
    }
}
