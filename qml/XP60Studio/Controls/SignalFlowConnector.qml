import QtQuick
import XP60Studio

// The arrow between two signal-flow nodes.
Item {
    id: root
    property color lineColor: Theme.borderStrong
    implicitWidth: 34
    implicitHeight: 12

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - 7
        height: 1
        color: root.lineColor
    }
    Canvas {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 8
        height: 8
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = root.lineColor
            ctx.beginPath()
            ctx.moveTo(0, 0); ctx.lineTo(8, 4); ctx.lineTo(0, 8)
            ctx.closePath(); ctx.fill()
        }
    }
}
