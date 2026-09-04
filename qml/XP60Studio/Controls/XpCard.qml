import QtQuick
import XP60Studio

// Surface container used for every panel. Children go into `contentItem`.
Rectangle {
    id: root

    default property alias contentData: content.data
    property alias padding: content.anchors.margins
    property bool raised: false
    property color accentColor: "transparent"

    color: raised ? Theme.surfaceRaised : Theme.surface
    radius: Metrics.radiusMd
    border.width: Metrics.borderWidth
    border.color: Theme.border
    implicitWidth: content.implicitWidth + 2 * content.anchors.margins
    implicitHeight: content.implicitHeight + 2 * content.anchors.margins

    // Optional coloured accent along the left edge (mockup metric tiles).
    Rectangle {
        visible: root.accentColor.a > 0
        width: 3
        radius: 2
        color: root.accentColor
        anchors { left: parent.left; leftMargin: 1; top: parent.top; bottom: parent.bottom; topMargin: 10; bottomMargin: 10 }
    }

    Item {
        id: content
        anchors.fill: parent
        anchors.margins: Metrics.cardPadding
        implicitWidth: childrenRect.width
        implicitHeight: childrenRect.height
    }
}
