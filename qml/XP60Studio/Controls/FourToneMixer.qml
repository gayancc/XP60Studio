import QtQuick
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Four Tone cards as the dominant Patch Editor mixer surface.
GridLayout {
    id: root

    required property PatchEditorViewModel editor
    property bool wide: true
    signal toneActivated(int toneNumber)
    signal browseWavesRequested(int toneNumber)

    objectName: "toneGrid"
    columns: wide ? 4 : 2
    columnSpacing: Metrics.spacingMd
    rowSpacing: Metrics.spacingMd

    Repeater {
        id: toneCards
        model: root.editor.tones
        delegate: ToneCard {
            required property var modelData
            objectName: "toneCard" + modelData.toneNumber
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            tone: modelData
            selected: root.editor.selectedTone === modelData.toneNumber
            onClicked: {
                root.editor.selectedTone = modelData.toneNumber
                root.toneActivated(modelData.toneNumber)
            }
            compatibility: root.editor.toneCompatibility[modelData.toneNumber - 1] ?? ({})
            onBrowseWaves: root.browseWavesRequested(modelData.toneNumber)
            // "Find replacement" opens the same browser the wave row opens: the
            // musician picks, exactly as they would for any other Tone.
            onFindReplacement: {
                root.editor.findReplacementFor(modelData.toneNumber)
                root.browseWavesRequested(modelData.toneNumber)
            }
            onDisableTone: root.editor.disableTone(modelData.toneNumber)
            onKeepAnyway: root.editor.keepToneAnyway(modelData.toneNumber)
        }
    }

    function itemAt(index) { return toneCards.itemAt(index) }
}
