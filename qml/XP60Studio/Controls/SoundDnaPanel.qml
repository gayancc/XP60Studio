import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Play-mode perceptual workspace. Values are supplied only by the C++
// evidence gate; this component cannot invent or opt into candidate dimensions.
XpCard {
    id: root
    objectName: "soundDnaPanel"

    required property PatchEditorViewModel editor
    implicitHeight: content.implicitHeight + 2 * Metrics.cardPadding

    ColumnLayout {
        id: content
        anchors { left: parent.left; right: parent.right; top: parent.top }
        spacing: Metrics.spacingLg

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                XpLabel { text: qsTr("SOUND DNA"); role: "overline"; color: Theme.accent }
                XpLabel { text: qsTr("Perceptual profile"); role: "title" }
                XpLabel {
                    Layout.fillWidth: true
                    text: root.editor.soundDnaAvailable
                        ? qsTr("Percentiles are relative to comparable, professionally designed XP-60 patches.")
                        : root.editor.soundDnaStatusText
                    role: "caption"
                    secondary: true
                    wrapMode: Text.WordWrap
                }
            }
            StatusPill {
                text: root.editor.soundDnaAvailable ? qsTr("VALIDATED") : qsTr("EVIDENCE GATED")
                tone: root.editor.soundDnaAvailable ? "success" : "warning"
                showDot: root.editor.soundDnaAvailable
            }
        }

        Rectangle {
            visible: !root.editor.soundDnaAvailable
            Layout.fillWidth: true
            implicitHeight: gateColumn.implicitHeight + 2 * Metrics.spacingLg
            radius: Metrics.radiusMd
            color: Theme.surfaceSunken
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                id: gateColumn
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                anchors.margins: Metrics.spacingLg
                spacing: Metrics.spacingSm
                XpLabel { text: qsTr("NO VALIDATED PERCEPTUAL MODEL"); role: "overline"; color: Theme.warning }
                XpLabel {
                    Layout.fillWidth: true
                    text: qsTr("XP60Studio will not turn raw parameter guesses into authoritative-looking scores. Capture, listener, holdout and transformation evidence must pass before a dimension appears here.")
                    role: "body"
                    wrapMode: Text.WordWrap
                }
                XpLabel {
                    text: qsTr("Model %1").arg(root.editor.soundDnaModelVersion)
                    role: "mono"
                    muted: true
                }
            }
        }

        ColumnLayout {
            visible: root.editor.soundDnaAvailable
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            Repeater {
                model: root.editor.soundDnaDimensions
                delegate: Item {
                    id: lane
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: laneColumn.implicitHeight

                    ColumnLayout {
                        id: laneColumn
                        width: parent.width
                        spacing: Metrics.spacingXs

                        RowLayout {
                            Layout.fillWidth: true
                            XpLabel { text: lane.modelData.label.toUpperCase(); role: "overline" }
                            Item { Layout.fillWidth: true }
                            XpLabel {
                                text: qsTr("%1–%2 · %3 confidence").arg(lane.modelData.intervalLow)
                                      .arg(lane.modelData.intervalHigh).arg(lane.modelData.confidence)
                                role: "caption"
                                color: lane.modelData.confidence === "Low" ? Theme.warning : Theme.textSecondary
                            }
                            XpLabel { text: lane.modelData.score; role: "title"; color: Theme.accent }
                        }

                        QQC.Slider {
                            id: dnaSlider
                            Layout.fillWidth: true
                            implicitHeight: 30
                            from: 0
                            to: 100
                            stepSize: 1
                            value: lane.modelData.score
                            enabled: lane.modelData.editable && !root.editor.comparing
                            live: true
                            Accessible.name: lane.modelData.label
                            Accessible.description: qsTr("Sound DNA percentile within %1").arg(lane.modelData.cohort)
                            onPressedChanged: {
                                if (pressed) root.editor.beginSoundDnaGesture()
                                else root.editor.endSoundDnaGesture()
                            }
                            onMoved: root.editor.previewSoundDnaTarget(lane.modelData.id, Math.round(value))

                            background: Rectangle {
                                x: dnaSlider.leftPadding
                                y: dnaSlider.topPadding + dnaSlider.availableHeight / 2 - height / 2
                                implicitHeight: 8
                                width: dnaSlider.availableWidth
                                height: implicitHeight
                                radius: 4
                                color: Theme.surfaceSunken
                                border.width: 1
                                border.color: Theme.border

                                Rectangle {
                                    x: parent.width * lane.modelData.intervalLow / 100
                                    width: parent.width * (lane.modelData.intervalHigh - lane.modelData.intervalLow) / 100
                                    height: parent.height
                                    radius: parent.radius
                                    color: Theme.accentSoft
                                }
                                Rectangle {
                                    width: dnaSlider.visualPosition * parent.width
                                    height: parent.height
                                    radius: parent.radius
                                    color: dnaSlider.enabled ? Theme.accent : Theme.textDisabled
                                    opacity: 0.72
                                    Behavior on width { NumberAnimation { duration: Motion.durationFast; easing.type: Motion.easingStandard } }
                                }
                            }

                            handle: Rectangle {
                                x: dnaSlider.leftPadding + dnaSlider.visualPosition * (dnaSlider.availableWidth - width)
                                y: dnaSlider.topPadding + dnaSlider.availableHeight / 2 - height / 2
                                implicitWidth: 18
                                implicitHeight: 18
                                radius: 9
                                color: dnaSlider.pressed ? Theme.accent : Theme.surfaceRaised
                                border.width: 2
                                border.color: dnaSlider.enabled ? Theme.accent : Theme.textDisabled
                                scale: dnaSlider.pressed ? 1.15 : 1
                                Behavior on scale { NumberAnimation { duration: Motion.durationFast } }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Repeater {
                                model: lane.modelData.tones
                                delegate: Rectangle {
                                    required property var modelData
                                    Layout.preferredWidth: Math.max(1, (lane.width - 8) * modelData.magnitudePercent / 100)
                                    implicitHeight: 5
                                    radius: 2
                                    color: Theme.toneColor(modelData.toneNumber)
                                    opacity: modelData.signedContribution < 0 ? 0.42 : 0.9
                                    visible: modelData.magnitudePercent > 0
                                }
                            }
                            Rectangle {
                                Layout.preferredWidth: Math.max(1, (lane.width - 8)
                                                                * lane.modelData.patchWideContributionPercent / 100)
                                implicitHeight: 5
                                radius: 2
                                color: Theme.textSecondary
                                opacity: 0.65
                                visible: lane.modelData.patchWideContributionPercent > 0
                                Accessible.name: qsTr("Patch-wide contribution")
                            }
                            Item { Layout.fillWidth: true }
                        }

                        XpLabel {
                            Layout.fillWidth: true
                            text: lane.modelData.referenceText + " · " + lane.modelData.confidenceReason
                            role: "caption"
                            muted: true
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }

        Rectangle {
            visible: root.editor.soundDnaLastExplanation.length > 0
            Layout.fillWidth: true
            implicitHeight: explanation.implicitHeight + 2 * Metrics.spacingMd
            radius: Metrics.radiusSm
            color: Theme.surfaceRaised
            XpLabel {
                id: explanation
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: Metrics.spacingMd }
                text: root.editor.soundDnaLastExplanation
                role: "caption"
                wrapMode: Text.WordWrap
            }
        }
    }
}
