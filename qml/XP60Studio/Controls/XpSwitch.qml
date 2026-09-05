import QtQuick
import QtQuick.Templates as T
import XP60Studio

// On/off toggle in the XP60Studio language.
//
// Devices used `QQC.Switch` raw, which under the Qt Basic style paints a grey
// track and a white knob: the single most obviously foreign control in the
// application. State is not carried by colour alone — the knob also travels,
// and the track gains a border in the on state.
T.Switch {
    id: control

    implicitWidth: track.implicitWidth
    implicitHeight: Math.max(Metrics.hitTarget, track.implicitHeight)
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.CheckBox
    Accessible.checked: control.checked

    indicator: Rectangle {
        id: track
        implicitWidth: 40
        implicitHeight: 22
        anchors.verticalCenter: parent.verticalCenter
        radius: height / 2
        color: !control.enabled ? Theme.surfaceSunken
             : control.checked ? Theme.accent
             : (control.hovered ? Theme.surfaceHover : Theme.surfaceSunken)
        border.width: Metrics.borderWidth
        border.color: !control.enabled ? Theme.borderSubtle
                    : control.visualFocus ? Theme.focusRing
                    : control.checked ? Theme.accent
                    : Theme.borderStrong

        Behavior on color {
            enabled: !Motion.reducedMotion
            ColorAnimation { duration: Motion.durationFast; easing.type: Motion.easingStandard }
        }

        Rectangle {
            width: 16
            height: 16
            radius: 8
            y: (parent.height - height) / 2
            x: control.checked ? parent.width - width - 3 : 3
            color: !control.enabled ? Theme.textDisabled
                 : control.checked ? Theme.textOnAccent : Theme.textSecondary

            Behavior on x {
                enabled: !Motion.reducedMotion
                NumberAnimation { duration: Motion.durationFast; easing.type: Motion.easingStandard }
            }
            Behavior on color {
                enabled: !Motion.reducedMotion
                ColorAnimation { duration: Motion.durationFast }
            }
        }
    }

    // The label is optional: most uses put their own text in the row beside
    // the switch so it can carry a section overline weight.
    contentItem: XpLabel {
        text: control.text
        visible: control.text.length > 0
        leftPadding: control.text.length > 0 ? control.indicator.width + Metrics.spacingSm : 0
        color: control.enabled ? Theme.textPrimary : Theme.textDisabled
    }
}
