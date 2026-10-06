import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Shared control surface for a focused synthesis section and the searchable
// Expert projection. Design mode leads with knobs and choice chips; Expert
// keeps dense exact entry. Both edit the same Patch through C++.
XpCard {
    id: root
    required property EditorParameterModel parameters
    required property PatchEditorViewModel editor
    property bool expert: false

    // The four destinations the XP-60's LFOs actually reach, and the parameter
    // each one moves. A knob in this list carries a second arc showing the
    // combined LFO depth aimed at it, the way a modulator's reach is shown on a
    // hardware-modelled synth. The arc is the depth setting itself — the manual
    // gives no conversion from LFO depth to cents or decibels, so no span in
    // those units is drawn or implied.
    readonly property var modulationDestinations: ({
        "tone.cutoff_frequency": { key: "filter", tint: Theme.tone2 },
        "tone.tva_level":        { key: "level",  tint: Theme.tone3 },
        "tone.coarse_tune":      { key: "pitch",  tint: Theme.tone1 },
        "tone.pan":              { key: "pan",    tint: Theme.tone4 }
    })

    // Signed, as a fraction of the knob's range. Depths are raw 0-126 around a
    // musical zero of 63; the two LFOs are summed because both reach the same
    // destination at once.
    function modulationDepth(parameterId) {
        // Touch the model's change tick so this re-evaluates when a depth is
        // edited. rawForId() is a plain Q_INVOKABLE — reading it alone gives
        // the binding nothing to depend on, so the arc would be drawn once at
        // load and then never again.
        var _ = root.parameters.valuesTick
        var dest = root.modulationDestinations[parameterId]
        if (!dest || root.expert)
            return 0
        var total = 0
        for (var i = 1; i <= 2; ++i) {
            var raw = root.parameters.rawForId("tone." + dest.key + "_lfo" + i + "_depth")
            if (raw >= 0)
                total += (raw - 63) / 63
        }
        return Math.max(-1, Math.min(1, total / 2))
    }

    function modulationTint(parameterId) {
        var dest = root.modulationDestinations[parameterId]
        return dest ? dest.tint : Theme.tone2
    }
    property string title: ""
    property string note: qsTr("Exact entry uses XP values. Labels show the documented interpretation.")
    padding: Metrics.spacingSm
    implicitHeight: column.implicitHeight + 2 * Metrics.spacingSm
    accentColor: expert && parameters.commonScope ? Theme.accent : Theme.toneColor(editor.selectedTone)

    ColumnLayout {
        id: column
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom }
        spacing: Metrics.spacingSm

        // Title, scope and the group chips share one band, the way a hardware
        // module labels its own row. What used to be two paragraphs of prose
        // above the knobs is now the trailing note in that band.
        XpModuleHeader {
            title: root.title
            iconName: "sliders"
            accentColor: root.expert && root.parameters.commonScope
                         ? Theme.accent : Theme.toneColor(root.editor.selectedTone)
            trailing: XpLabel {
                text: root.editor.liveAudition
                      ? qsTr("%1 · live to XP temp").arg(root.parameters.targetText)
                      : root.expert ? qsTr("Shared undo and A/B")
                                    : qsTr("%1 · edits stay local").arg(root.parameters.targetText)
                role: "caption"
                muted: true
                elide: Text.ElideRight
            }
        }
        RowLayout {
            Layout.fillHeight: false
            Layout.fillWidth: true
            XpComboBox {
                objectName: "expertScope"
                visible: root.expert
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                model: [qsTr("Selected Tone"), qsTr("Patch Common")]
                currentIndex: root.parameters.commonScope ? 1 : 0
                onActivated: function(index) { root.parameters.commonScope = index === 1 }
                Accessible.name: qsTr("Parameter scope")
            }
            XpComboBox {
                objectName: "parameterGroup"
                // Expert is the raw-value surface, where a dense dropdown is
                // the right density. Design selects the group with chips.
                visible: root.expert
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                model: root.parameters.groups
                currentIndex: root.parameters.group
                onActivated: function(index) { root.parameters.group = index }
                Accessible.name: qsTr("Parameter group")
            }
        }
        Flow {
            Layout.fillHeight: false
            objectName: "parameterGroupChips"
            visible: !root.expert && root.parameters.groups.length > 1
            Layout.fillWidth: true
            spacing: Metrics.spacingXs
            Repeater {
                model: root.parameters.groups
                XpButton {
                    required property string modelData
                    required property int index
                    text: modelData
                    compact: true
                    variant: root.parameters.group === index ? "primary" : "ghost"
                    onClicked: root.parameters.group = index
                    Accessible.name: qsTr("Parameter group: %1").arg(modelData)
                }
            }
        }
        XpTextField {
            objectName: "parameterSearch"
            visible: root.expert
            Layout.fillWidth: true
            placeholderText: qsTr("Find a parameter in this scope…")
            text: root.parameters.search
            onTextEdited: root.parameters.search = text
            Accessible.name: qsTr("Find a parameter")
        }
        XpTextField {
            objectName: "expertPatchName"
            visible: root.expert && root.parameters.commonScope
            Layout.fillWidth: true
            enabled: !root.editor.comparing
            text: root.editor.patchName
            maximumLength: 12
            onEditingFinished: {
                root.editor.patchName = text
                text = Qt.binding(function() { return root.editor.patchName })
            }
            Accessible.name: qsTr("Patch name")
        }
        XpLabel {
            // Expert carries a real explanation of its scope. Design's note is
            // identical on every section and only cost the knobs a row: the
            // documented reading is already under each dial.
            visible: root.expert
            Layout.fillWidth: true
            text: root.note
            role: "caption"
            muted: true
            wrapMode: Text.WordWrap
        }
        // Slack is split above and below the knob block rather than stretching
        // the rows apart. A group of four knobs centred in its module reads as
        // a module with padding; the same four spread 150 px apart reads as a
        // mistake.
        Item {
            visible: !root.expert
            Layout.fillHeight: true
            Layout.minimumHeight: 0
        }

        GridView {
            id: grid
            objectName: "parameterGrid"
            Layout.fillWidth: true
            Layout.fillHeight: root.expert
            Layout.preferredHeight: root.expert ? -1 : rows * cellHeight
            Layout.minimumHeight: root.expert ? 76 : 0
            // The pitch decides how many knobs fit; the row then shares the
            // width between exactly that many. Dividing the panel by the number
            // of parameters instead put a 44 px dial in the middle of a 300 px
            // cell, and a fixed pitch left a ragged margin down one side —
            // this bounds the gutter between the two.
            readonly property int cellPitch: root.expert ? 240 : 152
            readonly property int fits: Math.max(1, Math.min(6, Math.floor(width / cellPitch)))
            // Balanced, not merely as many as fit: four knobs in a three-wide
            // grid leave an orphan on a row of its own with two empty cells
            // beside it. Taking the row count first and then dividing gives a
            // 2x2 instead, which fills the module.
            readonly property int columns: root.expert || count === 0
                ? fits
                : Math.max(1, Math.ceil(count / Math.ceil(count / fits)))
            cellWidth: width / columns
            // Design rows share the panel's height between them, so a group of
            // four knobs fills its module instead of sitting in the top half of
            // it. Reads height only — preferredHeight must not read cellHeight
            // back, or the two chase each other.
            readonly property int rows: Math.max(1, Math.ceil(count / columns))
            // One height for a Design knob cell everywhere in the app.
            cellHeight: root.expert ? 76 : 112
            clip: true
            model: root.parameters
            boundsBehavior: Flickable.StopAtBounds
            QQC.ScrollBar.vertical: XpScrollBar {}

            delegate: Item {
                id: cell
                required property string name
                required property string parameterId
                required property int toneNumber
                required property int rawValue
                required property string valueText
                required property int minimum
                required property int maximum
                required property var choices
                required property bool bipolar
                width: grid.cellWidth
                height: grid.cellHeight
                clip: true
                objectName: "parameter-" + parameterId

                readonly property bool hasChoices: cell.choices.length > 0
                readonly property bool chipChoices: hasChoices && cell.choices.length <= 8

                // ── Expert: dense exact entry ─────────────────────────────
                ColumnLayout {
                    visible: root.expert
                    anchors { fill: parent; rightMargin: Metrics.spacingMd; bottomMargin: Metrics.spacingSm }
                    spacing: 2
                    XpLabel {
                        text: cell.name
                        role: "caption"
                        secondary: true
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.maximumWidth: cell.width - Metrics.spacingMd
                        elide: Text.ElideRight
                        QQC.ToolTip.text: cell.name
                        QQC.ToolTip.visible: expertHover.hovered
                        HoverHandler { id: expertHover }
                    }
                    XpComboBox {
                        objectName: "parameterChoice"
                        visible: cell.hasChoices
                        Layout.fillWidth: true
                        enabled: !root.editor.comparing
                        model: cell.choices
                        currentIndex: cell.rawValue - cell.minimum
                        onActivated: function(index) { root.parameters.edit(cell.parameterId, cell.toneNumber, index + cell.minimum) }
                        Accessible.name: cell.name
                    }
                    ParameterValueEditor {
                        objectName: "parameterValue"
                        visible: !cell.hasChoices
                        Layout.fillWidth: true
                        value: cell.rawValue
                        displayText: cell.valueText
                        minimumValue: cell.minimum
                        maximumValue: cell.maximum
                        editable: !root.editor.comparing
                        accessibleName: cell.name
                        onEdited: function(v) { root.parameters.edit(cell.parameterId, cell.toneNumber, v) }
                    }
                }

                // ── Design: knobs + chips ─────────────────────────────────
                ColumnLayout {
                    visible: !root.expert
                    anchors { fill: parent; rightMargin: Metrics.spacingSm; bottomMargin: Metrics.spacingXs }
                    spacing: 2

                    MusicalParamKnob {
                        visible: !cell.hasChoices
                        Layout.alignment: Qt.AlignHCenter
                        label: cell.name
                        value: cell.rawValue
                        from: cell.minimum
                        to: cell.maximum
                        displayText: cell.valueText
                        bipolar: cell.bipolar
                        knobSize: Metrics.knobMd
                        labelWidth: grid.cellWidth - 2 * Metrics.spacingSm
                        accentColor: Theme.toneColor(root.editor.selectedTone)
                        modDepth: root.modulationDepth(cell.parameterId)
                        modColor: root.modulationTint(cell.parameterId)
                        editable: !root.editor.comparing
                        exactObjectName: "parameterValue"
                        onEdited: root.parameters.edit(cell.parameterId, cell.toneNumber, value)
                    }

                    // Discrete enums with few labels → selectable chips. Centred
                    // on the same axis as the knobs so a mixed row still reads
                    // as one row of controls.
                    ColumnLayout {
                        visible: cell.chipChoices
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 2
                        XpLabel {
                            text: cell.name
                            role: "caption"
                            secondary: true
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                            QQC.ToolTip.text: cell.name
                            QQC.ToolTip.visible: chipHover.hovered
                            HoverHandler { id: chipHover }
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 4
                            // Chip rows read from the middle of the cell, like
                            // the dials they sit beside.
                            Repeater {
                                model: cell.choices
                                XpButton {
                                    required property string modelData
                                    required property int index
                                    text: modelData
                                    compact: true
                                    variant: cell.rawValue === index + cell.minimum ? "primary" : "ghost"
                                    enabled: !root.editor.comparing
                                    onClicked: root.parameters.edit(cell.parameterId, cell.toneNumber, index + cell.minimum)
                                    Accessible.name: cell.name + ": " + modelData
                                }
                            }
                        }
                    }

                    // Long enums (Random Pitch Depth, Keyfollow, …) → stepped knob
                    // showing the documented label, not a tall dropdown.
                    MusicalParamKnob {
                        visible: cell.hasChoices && !cell.chipChoices
                        Layout.alignment: Qt.AlignHCenter
                        label: cell.name
                        value: cell.rawValue
                        from: cell.minimum
                        to: cell.maximum
                        displayText: cell.valueText
                        bipolar: cell.bipolar || (cell.choices.length > 2
                                 && String(cell.choices[0]).indexOf("-") === 0)
                        knobSize: Metrics.knobMd
                        labelWidth: grid.cellWidth - 2 * Metrics.spacingSm
                        accentColor: Theme.toneColor(root.editor.selectedTone)
                        editable: !root.editor.comparing
                        onEdited: root.parameters.edit(cell.parameterId, cell.toneNumber, value)
                    }

                    // Keep ComboBox in the tree for keyboard / QML tests when
                    // Design uses chips or a discrete knob as the primary UI.
                    XpComboBox {
                        objectName: "parameterChoice"
                        visible: false
                        enabled: cell.hasChoices && !root.editor.comparing
                        model: cell.choices
                        currentIndex: cell.rawValue - cell.minimum
                        onActivated: function(index) { root.parameters.edit(cell.parameterId, cell.toneNumber, index + cell.minimum) }
                        Accessible.name: cell.name
                    }
                }
            }
            XpLabel {
                anchors.centerIn: parent
                visible: grid.count === 0
                text: qsTr("No matching parameters")
                secondary: true
            }
        }

        Item {
            visible: !root.expert
            Layout.fillHeight: true
            Layout.minimumHeight: 0
        }
    }
}
