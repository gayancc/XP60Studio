import QtQuick
import QtQuick.Templates as T
import XP60Studio

T.ScrollBar {
    id: control

    implicitWidth: 8
    implicitHeight: 8
    padding: 2
    minimumSize: 0.08
    policy: T.ScrollBar.AsNeeded

    contentItem: Rectangle {
        implicitWidth: 4
        implicitHeight: 4
        radius: 2
        color: control.pressed ? Theme.accent : (control.hovered ? Theme.scrollBarHover : Theme.scrollBar)
        opacity: control.policy === T.ScrollBar.AlwaysOn || control.size < 1.0 ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: Motion.durationNormal } }
    }
}
