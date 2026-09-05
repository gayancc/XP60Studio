import QtQuick
import QtQuick.Templates as T
import XP60Studio

// Integer field with stepper buttons and exact keyboard entry.
T.SpinBox {
    id: control

    implicitWidth: 120
    implicitHeight: Metrics.controlHeight
    editable: true
    leftPadding: Metrics.spacingSm
    rightPadding: Metrics.spacingSm
    font.pixelSize: Typography.bodySize
    font.family: Typography.monoFamily

    Accessible.role: Accessible.SpinBox
    Accessible.name: "Value"

    validator: IntValidator { bottom: control.from; top: control.to }

    contentItem: TextInput {
        text: control.displayText
        font: control.font
        color: control.enabled ? Theme.textPrimary : Theme.textDisabled
        selectionColor: Theme.selection
        selectedTextColor: Theme.textPrimary
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhDigitsOnly
        selectByMouse: true
        leftPadding: control.down.indicator.width
        rightPadding: control.up.indicator.width
    }

    up.indicator: Rectangle {
        x: control.mirrored ? 0 : parent.width - width
        height: parent.height
        width: Metrics.hitTarget - 4
        radius: Metrics.radiusSm
        color: control.up.pressed ? Theme.surfacePressed : (control.up.hovered ? Theme.surfaceHover : "transparent")
        XpIcon {
            anchors.centerIn: parent
            name: "plus"
            color: control.enabled ? Theme.textSecondary : Theme.textDisabled
            implicitWidth: Metrics.iconSizeSm
            implicitHeight: Metrics.iconSizeSm
        }
    }

    down.indicator: Rectangle {
        x: control.mirrored ? parent.width - width : 0
        height: parent.height
        width: Metrics.hitTarget - 4
        radius: Metrics.radiusSm
        color: control.down.pressed ? Theme.surfacePressed : (control.down.hovered ? Theme.surfaceHover : "transparent")
        XpIcon {
            anchors.centerIn: parent
            name: "minus"
            color: control.enabled ? Theme.textSecondary : Theme.textDisabled
            implicitWidth: Metrics.iconSizeSm
            implicitHeight: Metrics.iconSizeSm
        }
    }

    background: Rectangle {
        radius: Metrics.radiusSm
        color: control.enabled ? Theme.surfaceSunken : Theme.surface
        border.width: Metrics.borderWidth
        border.color: control.activeFocus ? Theme.focusRing : Theme.borderStrong
    }
}
