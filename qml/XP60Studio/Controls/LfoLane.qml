import QtQuick
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// One of a Tone's two LFOs, as a musician reads it: what shape it is, how fast
// it moves, when it arrives, and how much of it reaches each destination.
//
// Every destination here is documented for the XP-60 (Pitch / Filter / Level /
// Pan LFO Depth). No routing this instrument does not have is drawn.
ColumnLayout {
    id: root

    required property EditorParameterModel parameters
    required property PatchEditorViewModel editor
    // "lfo1" or "lfo2".
    required property string lfo
    property string title: ""
    property color accentColor: Theme.accent

    readonly property int shapeRaw: {
        var _ = root.parameters.valuesTick
        return root.parameters.rawForId("tone." + root.lfo + "_waveform")
    }

    spacing: Metrics.spacingXs

    function raw(id) {
        var _ = root.parameters.valuesTick
        return root.parameters.rawForId(id)
    }
    function valueText(id) {
        var _ = root.parameters.valuesTick
        return root.parameters.valueTextForId(id)
    }
    function apply(id, value) {
        root.parameters.edit(id, root.editor.selectedTone, value)
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Metrics.spacingSm
        XpLabel { text: root.title; role: "overline"; secondary: true }
        // The shape the LFO is actually set to, drawn once so the chip row is
        // not the only place the waveform exists.
        Rectangle {
            Layout.preferredWidth: 92
            Layout.preferredHeight: 26
            radius: Metrics.radiusSm
            color: Theme.surfaceSunken
            border.width: Metrics.borderWidth
            border.color: Theme.border
            LfoPreview {
                objectName: root.lfo + "Preview"
                anchors { fill: parent; margins: 4 }
                shapeIndex: root.shapeRaw
                accentColor: root.accentColor
            }
        }
        Item { Layout.fillWidth: true }
    }

    LfoShapeSelector {
        objectName: root.lfo + "ShapeSelector"
        Layout.fillWidth: true
        shapes: {
            var _ = root.parameters.valuesTick
            return root.parameters.choicesForId("tone." + root.lfo + "_waveform")
        }
        currentRaw: root.shapeRaw
        accentColor: root.accentColor
        editable: !root.editor.comparing
        onShapeSelected: function(raw) { root.apply("tone." + root.lfo + "_waveform", raw) }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Metrics.spacingSm

        MusicalParamKnob {
            objectName: root.lfo + "Rate"
            label: qsTr("Rate")
            from: 0; to: 127
            value: root.raw("tone." + root.lfo + "_rate")
            displayText: root.valueText("tone." + root.lfo + "_rate")
            knobSize: Metrics.knobXs
            accentColor: root.accentColor
            editable: !root.editor.comparing
            onEdited: function(v) { root.apply("tone." + root.lfo + "_rate", v) }
        }
        MusicalParamKnob {
            objectName: root.lfo + "Delay"
            label: qsTr("Delay")
            from: 0; to: 127
            value: root.raw("tone." + root.lfo + "_delay_time")
            displayText: root.valueText("tone." + root.lfo + "_delay_time")
            knobSize: Metrics.knobXs
            accentColor: root.accentColor
            editable: !root.editor.comparing
            onEdited: function(v) { root.apply("tone." + root.lfo + "_delay_time", v) }
        }

        XpDivider {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            Layout.topMargin: Metrics.spacingXs
            Layout.bottomMargin: Metrics.spacingXs
        }

        // Destination depths. Bipolar: raw 0…126 displays −63…+63, so the arc
        // grows out of musical zero in whichever direction the value went.
        // Coloured by what each one moves, not by the LFO.
        Repeater {
            model: [
                { id: "pitch",  label: qsTr("Pitch"),  tint: Theme.tone1 },
                { id: "filter", label: qsTr("Filter"), tint: Theme.tone2 },
                { id: "level",  label: qsTr("Level"),  tint: Theme.tone3 },
                { id: "pan",    label: qsTr("Pan"),    tint: Theme.tone4 }
            ]
            delegate: MusicalParamKnob {
                required property var modelData
                readonly property string paramId: "tone." + modelData.id + "_" + root.lfo + "_depth"
                objectName: root.lfo + "Depth-" + modelData.id
                label: modelData.label
                from: 0; to: 126
                bipolar: true
                defaultValue: 63
                value: root.raw(paramId)
                displayText: root.valueText(paramId)
                knobSize: Metrics.knobXs
                accentColor: modelData.tint
                editable: !root.editor.comparing
                onEdited: function(v) { root.apply(paramId, v) }
            }
        }
        Item { Layout.fillWidth: true }
    }

    XpLabel {
        Layout.fillWidth: true
        text: qsTr("Pitch · Filter · Level · Pan are this LFO's documented depths.")
        role: "caption"
        muted: true
        wrapMode: Text.WordWrap
    }
}
