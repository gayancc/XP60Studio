import QtQuick
import QtQuick.Layouts
import XP60Studio

ColumnLayout {
    id: root
    required property var editor
    readonly property var values: editor.effectValues
    readonly property var algorithms: editor.effectAlgorithms
    readonly property int algorithmIndex: (values["common.efx_type"] || {}).value || 0
    readonly property var algorithm: algorithms[algorithmIndex] || ({})
    readonly property int reverbType: (values["common.reverb_type"] || {}).value || 0
    property bool browserOpen: false
    property string inspectedTarget: ""
    property string highlightedParameter: ""
    onAlgorithmIndexChanged: inspectedTarget = ""
    readonly property int page: editor.effectPage
    spacing: Metrics.spacingMd

    RowLayout {
        Layout.fillWidth: true
        XpLabel { text: qsTr("PATCH EFFECTS"); role: "overline" }
        XpLabel { text: root.editor.mfxText; role: "caption"; secondary: true; Layout.fillWidth: true; elide: Text.ElideRight }
        StatusPill { text: root.editor.comparing ? "A · ORIGINAL" : "B · CURRENT"; showDot: false }
    }
    XpSegmentedControl {
        objectName: "effectsTabs"
        Layout.fillWidth: true
        model: [qsTr("General"), qsTr("EFX"), qsTr("Control"), qsTr("Chorus"), qsTr("Reverb")]
        currentIndex: root.page
        onActivated: function(index) { root.editor.effectPage = index }
    }

    ColumnLayout {
        visible: root.page === 0; Layout.fillWidth: true; spacing: Metrics.spacingMd
        XpLabel {
            text: qsTr("%1 · %2 owns the output settings").arg(root.editor.routing.source || "").arg("Tone " + (root.editor.routing.outputTone || "?"))
            role: "caption"; color: Theme.toneColor(root.editor.selectedTone)
        }
        GridLayout {
            Layout.fillWidth: true; columns: root.width >= 850 ? 3 : 1; columnSpacing: Metrics.spacingMd
            EffectRouteSelector { Layout.fillWidth: true; Layout.preferredWidth: 1; editor: root.editor; title: qsTr("TONE / STRUCTURE"); parameterId: "tone.output_assign" }
            EffectRouteSelector { Layout.fillWidth: true; Layout.preferredWidth: 1; editor: root.editor; title: qsTr("EFX OUTPUT"); parameterId: "common.efx_output_assign" }
            EffectRouteSelector { Layout.fillWidth: true; Layout.preferredWidth: 1; editor: root.editor; title: qsTr("CHORUS OUTPUT"); parameterId: "common.chorus_output" }
        }
        XpCard {
            Layout.fillWidth: true; implicitHeight: sends.implicitHeight + 2 * Metrics.cardPadding
            ColumnLayout {
                id: sends; anchors { left: parent.left; right: parent.right; top: parent.top }
                XpPanelHeader { title: qsTr("SEND MIXER"); Layout.fillWidth: true }
                Flow {
                    Layout.fillWidth: true;  spacing: Metrics.spacingMd
                    Repeater {
                        model: ["tone.mix_efx_send_level", "tone.chorus_send_level", "tone.reverb_send_level", "common.efx_mix_out_send_level", "common.efx_chorus_send_level", "common.efx_reverb_send_level", "common.chorus_level", "common.reverb_level"]
                        EffectParameterControl { required property string modelData; width: 126; editor: root.editor; parameterId: modelData }
                    }
                }
                XpLabel { Layout.fillWidth: true; text: root.editor.routingSummary; role: "caption"; secondary: true; wrapMode: Text.WordWrap }
            }
        }
    }

    ColumnLayout {
        visible: root.page === 1; Layout.fillWidth: true; spacing: Metrics.spacingMd
        RowLayout {
            Layout.fillWidth: true
            XpLabel { text: (root.algorithm.number || "") + "  " + (root.algorithm.name || ""); role: "heading"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            StatusPill { text: root.algorithm.topology || ""; showDot: false }
            XpButton { objectName: "effectAlgorithmBrowser"; text: root.browserOpen ? qsTr("Close palette") : qsTr("Choose algorithm"); onClicked: root.browserOpen = !root.browserOpen }
        }
        Flow {
            objectName: "effectAlgorithmPalette"
            visible: root.browserOpen; Layout.fillWidth: true;
            spacing: Metrics.spacingSm
            Repeater {
                model: root.algorithms
                XpButton {
                    required property var modelData
                    width: Math.max(240, (root.width - 2 * Metrics.spacingSm) / 3)
                    objectName: "algorithm-" + modelData.value
                    text: modelData.number + "  " + modelData.name
                    variant: modelData.value === root.algorithmIndex ? "primary" : "secondary"
                    enabled: !root.editor.comparing; compact: true
                    onClicked: { root.editor.editEffect("common.efx_type", modelData.value); root.browserOpen = false }
                }
            }
        }
        XpLabel {
            Layout.fillWidth: true; role: "caption"; color: Theme.warning; wrapMode: Text.WordWrap
            text: qsTr("Algorithm identity and output controls are bound. Dashed diagrams and dimmed algorithm controls await verified parameter mappings; they cannot change the Patch.")
        }
        GridLayout {
            Layout.fillWidth: true; columns: root.width >= 850 ? (root.algorithm.stages || []).length : 1
            columnSpacing: Metrics.spacingMd; rowSpacing: Metrics.spacingMd
            Repeater {
                model: root.algorithm.stages || []
                XpCard {
                    id: stage
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.alignment: Qt.AlignTop
                    implicitHeight: stageContent.implicitHeight + 2 * Metrics.cardPadding
                    accentColor: Theme.tone4
                    ColumnLayout {
                        id: stageContent; anchors { left: parent.left; right: parent.right; top: parent.top }
                        XpPanelHeader { title: (root.algorithm.topology === "SINGLE" ? "" : (stage.index === 0 ? "A · " : "B · ")) + stage.modelData.name; Layout.fillWidth: true }
                        XpLabel {
                            visible: root.algorithm.topology !== "SINGLE"; role: "caption"; secondary: true
                            text: root.algorithm.topology === "SERIES" ? qsTr("INPUT → A → B → OUTPUT") : qsTr("INPUT ⇉ A / B ⇉ OUTPUT")
                        }
                        EffectParameterGraph {
                            Layout.fillWidth: true; editor: root.editor; family: stage.modelData.family; accentColor: Theme.tone4
                            tapCount: root.algorithmIndex === 18 ? 3 : root.algorithmIndex === 19 ? 4 : 2
                        }
                        Flow {
                            Layout.fillWidth: true;  spacing: Metrics.spacingSm
                            Repeater {
                                model: stage.modelData.controls
                                ColumnLayout {
                                    required property string modelData
                                    width: 112; height: 116; spacing: Metrics.spacingXs
                                    XpLabel {
                                        Layout.fillWidth: true; Layout.preferredHeight: 44; text: modelData; role: "caption"
                                        color: root.inspectedTarget === modelData || root.inspectedTarget === (stage.index === 0 ? "A · " : "B · ") + modelData ? Theme.tone4 : Theme.textSecondary
                                        horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; elide: Text.ElideNone
                                    }
                                    XpKnob { enabled: false; Layout.alignment: Qt.AlignHCenter; Accessible.name: modelData + qsTr(" unavailable: mapping unverified") }
                                    XpLabel { text: "UNBOUND"; role: "caption"; muted: true; Layout.alignment: Qt.AlignHCenter }
                                }
                            }
                        }
                    }
                }
            }
        }
        Flow {
            Layout.fillWidth: true;  spacing: Metrics.spacingXl
            Repeater {
                model: ["common.efx_mix_out_send_level", "common.efx_chorus_send_level", "common.efx_reverb_send_level"]
                EffectParameterControl { required property string modelData; width: 140; editor: root.editor; parameterId: modelData }
            }
            EffectChoiceControl { width: 240; editor: root.editor; parameterId: "common.efx_output_assign" }
        }
    }

    ColumnLayout {
        visible: root.page === 2; Layout.fillWidth: true; spacing: Metrics.spacingMd
        XpLabel { text: root.algorithm.name || ""; role: "heading" }
        XpLabel {
            Layout.fillWidth: true; role: "caption"; secondary: true; wrapMode: Text.WordWrap
            text: qsTr("Select a documented eligible target to inspect its EFX control. Controller sources and signed depths are bound; target-to-slot/lane bindings still require verification.")
        }
        Flow {
            Layout.fillWidth: true;  spacing: Metrics.spacingSm
            Repeater {
                model: root.algorithm.eligibleTargets || []
                XpButton {
                    required property string modelData
                    text: qsTr("Inspect: %1").arg(modelData)
                    onClicked: { root.inspectedTarget = modelData; root.editor.effectPage = 1 }
                }
            }
        }
        Repeater {
            model: 2
            XpCard {
                id: lane
                required property int index
                Layout.fillWidth: true; implicitHeight: laneContent.implicitHeight + 2 * Metrics.cardPadding; accentColor: Theme.tone4
                GridLayout {
                    id: laneContent; anchors { left: parent.left; right: parent.right; top: parent.top }
                    columns: root.width >= 850 ? 3 : 1; columnSpacing: Metrics.spacingXl
                    EffectChoiceControl {
                        Layout.fillWidth: true; Layout.preferredWidth: root.width >= 850 ? 600 : 400
                        Layout.minimumWidth: root.width >= 850 ? 400 : 0
                        editor: root.editor
                        title: "EFX CTRL " + (lane.index + 1) + " · SOURCE"
                        parameterId: "common.efx_control_source_" + (lane.index + 1)
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.preferredWidth: 200; Layout.minimumWidth: 180
                        XpLabel {
                            readonly property int depth: (root.values["common.efx_control_depth_" + (lane.index + 1)] || {}).value || 0
                            text: depth < 63 ? "←  INVERTED" : depth === 63 ? "—  NO CHANGE" : "→  FORWARD"
                            color: Theme.tone4; role: "overline"
                        }
                        XpLabel { text: qsTr("Algorithm target\nMapping unavailable"); wrapMode: Text.WordWrap; secondary: true; role: "caption" }
                    }
                    EffectParameterControl {
                        Layout.preferredWidth: 140; Layout.minimumWidth: 140; editor: root.editor; title: qsTr("DEPTH · −63 / +63")
                        parameterId: "common.efx_control_depth_" + (lane.index + 1)
                    }
                }
            }
        }
        EffectChoiceControl { Layout.fillWidth: true; editor: root.editor; parameterId: "common.efx_control_hold_peak"; title: qsTr("PEDAL · HOLD / PEAK") }
        XpLabel { Layout.fillWidth: true; role: "caption"; secondary: true; wrapMode: Text.WordWrap; text: qsTr("Hold/Peak also depends on the Tone Hold-1 switch and the System pedal controller setup. This control does not alter those settings.") }
    }

    XpCard {
        visible: root.page === 3 || root.page === 4; Layout.fillWidth: true
        implicitHeight: processor.implicitHeight + 2 * Metrics.cardPadding
        accentColor: root.page === 3 ? Theme.tone2 : Theme.tone3
        ColumnLayout {
            id: processor; anchors { left: parent.left; right: parent.right; top: parent.top } spacing: Metrics.spacingMd
            XpPanelHeader { Layout.fillWidth: true; title: root.page === 3 ? qsTr("CHORUS · STEREO MODULATION") : qsTr("REVERB · SPACE & ECHO") }
            EffectChoiceControl { visible: root.page === 4; Layout.fillWidth: true; editor: root.editor; parameterId: "common.reverb_type"; title: qsTr("SPACE") }
            EffectParameterGraph {
                objectName: "independentEffectGraph"
                Layout.fillWidth: true; implicitHeight: 230; editor: root.editor
                family: root.page === 3 ? "modulation" : root.reverbType === 7 ? "pan-delay" : root.reverbType === 6 ? "delay" : "reverb"
                xParameter: root.page === 3 ? "common.chorus_rate" : "common.reverb_time"
                yParameter: root.page === 3 ? "common.chorus_depth" : "common.reverb_level"
                secondaryParameter: root.page === 3 ? "common.chorus_pre_delay" : "common.delay_feedback"
                feedbackParameter: root.page === 3 ? "common.chorus_feedback" : root.reverbType >= 6 ? "common.delay_feedback" : ""
                dampingParameter: root.page === 4 ? "common.reverb_hf_damp" : ""
                xTitle: root.page === 3 ? qsTr("Rate") : root.reverbType >= 6 ? qsTr("Delay time") : qsTr("Time")
                yTitle: root.page === 3 ? qsTr("Depth") : qsTr("Level")
                accentColor: root.page === 3 ? Theme.tone2 : Theme.tone3
                tapCount: 4
                onParameterSelected: function(parameterId) { root.highlightedParameter = parameterId }
            }
            Flow {
                Layout.fillWidth: true;  spacing: Metrics.spacingMd
                Repeater {
                    model: root.page === 3 ? ["common.chorus_level", "common.chorus_rate", "common.chorus_depth", "common.chorus_pre_delay", "common.chorus_feedback"]
                        : root.reverbType >= 6 ? ["common.reverb_level", "common.reverb_time", "common.reverb_hf_damp", "common.delay_feedback"]
                        : ["common.reverb_level", "common.reverb_time", "common.reverb_hf_damp"]
                    EffectParameterControl {
                        required property string modelData
                        width: 140; editor: root.editor; parameterId: modelData
                        accentColor: root.page === 3 ? Theme.tone2 : Theme.tone3
                        highlighted: root.highlightedParameter === modelData
                        onSelected: function(parameterId) { root.highlightedParameter = parameterId }
                    }
                }
            }
            EffectChoiceControl { visible: root.page === 3; Layout.fillWidth: true; editor: root.editor; parameterId: "common.chorus_output"; title: qsTr("CHORUS DESTINATION") }
            XpLabel { Layout.fillWidth: true; text: qsTr("Drag the point or use arrow keys. Click a value for exact XP entry. Dashed ghost = Original A. These are parameter diagrams, not audio measurements."); role: "caption"; secondary: true; wrapMode: Text.WordWrap }
        }
    }
}
