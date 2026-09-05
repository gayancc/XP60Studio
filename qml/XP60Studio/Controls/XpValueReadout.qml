import QtQuick
import QtQuick.Layouts
import XP60Studio

// The one way a parameter value is shown.
//
// A synth editor shows thousands of values, so they have to look the same
// everywhere: same face, same alignment, same place for the unit, and the same
// signal when the value has been changed locally but not yet sent to the
// XP-60. Before this, each control formatted its own readout and the editor
// had several different treatments side by side.
//
// The changed state is not colour-only: the value also gains a leading dot.
Item {
    id: root

    property string value: ""
    property string unit: ""
    // The value the XP-60 is known to hold, when it differs from `value`.
    property string hardwareValue: ""
    // Set when the user has moved this parameter locally.
    property bool changed: false
    property bool enabled: true
    property int horizontalAlignment: Text.AlignHCenter

    implicitWidth: row.implicitWidth
    implicitHeight: Math.max(row.implicitHeight, Metrics.controlHeightXs)

    Accessible.role: Accessible.StaticText
    Accessible.name: root.value + (root.unit.length ? " " + root.unit : "")
                     + (root.changed ? ", changed locally" : "")
                     + (root.hardwareValue.length ? ", on the XP-60 " + root.hardwareValue : "")

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Metrics.spacingXs

        Rectangle {
            visible: root.changed
            width: 4
            height: 4
            radius: 2
            color: Theme.localEdit
            anchors.verticalCenter: parent.verticalCenter
        }

        XpLabel {
            text: root.value
            role: "mono"
            color: !root.enabled ? Theme.textDisabled
                 : root.changed ? Theme.localEdit : Theme.textPrimary
            anchors.verticalCenter: parent.verticalCenter
        }

        XpLabel {
            visible: root.unit.length > 0
            text: root.unit
            role: "caption"
            color: root.enabled ? Theme.textMuted : Theme.textDisabled
            anchors.verticalCenter: parent.verticalCenter
        }

        // Shown only while local and hardware disagree, so the musician can
        // see what sending would replace.
        XpLabel {
            visible: root.hardwareValue.length > 0 && root.hardwareValue !== root.value
            text: "(" + root.hardwareValue + ")"
            role: "mono"
            color: Theme.hardwareValue
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
