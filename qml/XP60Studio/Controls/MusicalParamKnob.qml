import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Templates as T
import QtQuick.Layouts
import XP60Studio

// Compact synth knob: drag first, exact entry on demand.
// Used by envelope stages, Design parameter grids and Tone play settings.
ColumnLayout {
    id: root

    property string label: ""
    property int value: 0
    property int from: 0
    property int to: 127
    property string displayText: ""
    property bool bipolar: false
    property bool editable: true
    property color accentColor: Theme.accent
    property int knobSize: Metrics.knobMd
    // Parameter names are documented Roland names ("Cutoff Frequency",
    // "Random Pitch Depth"). A knob-width label elides them into nonsense, so
    // the caption is allowed to use the cell it sits in and wrap to two lines.
    property int labelWidth: 0
    // Passed through to XpKnob: a documented modulation reach for this
    // parameter, drawn as a second arc. Zero means nothing modulates it.
    property real modDepth: 0
    property color modColor: Theme.tone2
    property int defaultValue: bipolar ? Math.round((from + to) / 2) : from
    // Tests and shared panels bind to ParameterValueEditor via this name.
    property string exactObjectName: "musicalKnobExact"
    signal edited(int value)

    spacing: 1

    XpKnob {
        id: knob
        objectName: "musicalKnob"
        Layout.alignment: Qt.AlignHCenter
        // Forgiving hitbox: interactive area extends beyond the drawn arc.
        implicitWidth: Math.round(root.knobSize * 1.2)
        implicitHeight: Math.round(root.knobSize * 1.2)
        from: root.from
        to: root.to
        value: root.value
        bipolar: root.bipolar
        modDepth: root.modDepth
        modColor: root.modColor
        defaultValue: root.defaultValue
        accentColor: root.accentColor
        enabled: root.editable
        snapMode: T.Dial.SnapAlways
        valueText: root.label + ": " + (root.displayText.length > 0 ? root.displayText : String(root.value))
        onMoved: root.edited(Math.round(value))
        HoverHandler { cursorShape: Qt.SizeVerCursor }
    }

    // Caption under the dial, the way a hardware panel silk-screens a control.
    // Reading downward — dial, name, value — puts the name next to the number
    // it names, and drops a row of leading whitespace from every knob in a
    // grid. Roland's documented names are long ("Random Pitch Depth"), so the
    // caption may still take two lines and tells the full name on hover.
    XpLabel {
        id: caption
        text: root.label
        role: "caption"
        secondary: true
        visible: root.label.length > 0
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 1
        Layout.maximumWidth: root.labelWidth > 0 ? root.labelWidth
                                                 : Math.max(root.knobSize + 12, 96)
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
        lineHeight: 0.92
        QQC.ToolTip.text: root.label
        QQC.ToolTip.visible: captionHover.hovered && caption.truncated
        QQC.ToolTip.delay: 400
        HoverHandler { id: captionHover }
    }

    // The value reads as a value, not as a button. It is still the way into
    // exact entry — click it — but a row of knobs should not look like a row of
    // buttons with dials above them.
    Rectangle {
        objectName: "musicalKnobReadout"
        Layout.alignment: Qt.AlignHCenter
        implicitWidth: Math.max(readoutText.implicitWidth + 2 * Metrics.gapXs, 34)
        implicitHeight: Metrics.controlHeightXs
        radius: Metrics.radiusSm - 2
        color: readoutHover.hovered ? Theme.surfaceHover : "transparent"
        border.width: 1
        border.color: readoutHover.hovered ? Theme.borderStrong : "transparent"

        XpLabel {
            id: readoutText
            anchors.centerIn: parent
            text: root.displayText.length > 0 ? root.displayText : String(root.value)
            role: "mono"
            color: root.editable ? Theme.textPrimary : Theme.textDisabled
        }

        HoverHandler { id: readoutHover; cursorShape: Qt.IBeamCursor }
        TapHandler { onTapped: exact.checked = !exact.checked }
        Accessible.role: Accessible.Button
        Accessible.name: root.label + qsTr(" exact entry")
    }

    ParameterValueEditor {
        objectName: root.exactObjectName
        visible: exact.checked
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 96
        value: root.value
        displayText: root.displayText
        minimumValue: root.from
        maximumValue: root.to
        editable: root.editable
        accessibleName: root.label
        onEdited: function(v) { root.edited(v) }
    }

    QQC.Switch { id: exact; visible: false; checked: false }
}
