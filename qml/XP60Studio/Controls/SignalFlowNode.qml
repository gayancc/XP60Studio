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
    property bool interactive: false
    property bool selected: false
    property color accentColor: Theme.accent
    property int detailLines: 2
    signal activated()

    implicitWidth: Math.max(120, column.implicitWidth + 2 * Metrics.spacingMd)
    implicitHeight: 52
    radius: Metrics.radiusSm
    color: selected ? Theme.accentSoft : (highlighted ? Theme.accentSoft : Theme.surfaceRaised)
    border.width: selected || (interactive && activeFocus) ? 2 : Metrics.borderWidth
    border.color: selected ? accentColor : (highlighted ? accentColor : Theme.borderStrong)

    Behavior on color {
        enabled: !Motion.reducedMotion
        ColorAnimation { duration: Motion.durationFast; easing.type: Motion.easingStandard }
    }

    Accessible.role: interactive ? Accessible.Button : Accessible.StaticText
    Accessible.name: root.title + " " + root.detail
    activeFocusOnTab: interactive
    Keys.onPressed: function(event) {
        if (!interactive) return
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            root.activated(); event.accepted = true
        }
    }
    HoverHandler { id: hover }
    TapHandler { enabled: root.interactive; onTapped: root.activated() }
    QQC.ToolTip.visible: hover.hovered
    QQC.ToolTip.delay: 500
    QQC.ToolTip.text: root.title + " · " + root.detail + (interactive ? qsTr(" — click to open") : "")

    ColumnLayout {
        id: column
        anchors { fill: parent; margins: Metrics.spacingXs }
        spacing: 1
        XpLabel {
            text: root.title
            role: "overline"
            color: root.highlighted || root.selected ? Theme.accentText : Theme.textSecondary
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

    Rectangle {
        visible: hover.hovered && root.interactive
        anchors.fill: parent
        radius: parent.radius
        color: Theme.surfaceHover
        opacity: 0.35
        z: -1
    }
}
