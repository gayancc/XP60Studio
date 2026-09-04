import QtQuick
import QtQuick.Templates as T
import XP60Studio

// Single-line text input. `invalid` switches to the error border; `mono`
// selects the monospaced face for hex entry.
T.TextField {
    id: control

    property bool invalid: false
    property bool mono: false

    implicitWidth: 160
    implicitHeight: Metrics.controlHeight
    leftPadding: Metrics.spacingMd
    rightPadding: Metrics.spacingMd
    color: enabled ? Theme.textPrimary : Theme.textDisabled
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.selection
    selectedTextColor: Theme.textPrimary
    font.family: mono ? Typography.monoFamily : Typography.family
    font.pointSize: mono ? Typography.monoSize : Typography.bodySize
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true

    Accessible.role: Accessible.EditableText
    Accessible.name: control.placeholderText

    background: Rectangle {
        radius: Metrics.radiusSm
        color: control.enabled ? Theme.surfaceSunken : Theme.surface
        border.width: Metrics.borderWidth
        border.color: control.invalid ? Theme.error : (control.activeFocus ? Theme.focusRing : Theme.borderStrong)
        Behavior on border.color { ColorAnimation { duration: Motion.durationFast } }
    }

    Text {
        id: placeholder
        anchors.fill: parent
        anchors.leftMargin: control.leftPadding
        anchors.rightMargin: control.rightPadding
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        verticalAlignment: control.verticalAlignment
        visible: !control.length && !control.preeditText && (!control.activeFocus || control.horizontalAlignment !== Qt.AlignHCenter)
        elide: Text.ElideRight
    }
}
