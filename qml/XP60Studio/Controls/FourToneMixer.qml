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
            onBrowseWaves: root.browseWavesRequested(modelData.toneNumber)
        }
    }

    function itemAt(index) { return toneCards.itemAt(index) }
}
