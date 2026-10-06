import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio

// One channel strip of the 16-Part mixer.
//
// Laid out the way a mixer is read: identity at the top, the fader dominating
// the middle because level is what a musician reaches for, and the settings
// that are set once and left alone below it.
Rectangle {
    id: root

    // One entry of PerformanceViewModel.parts.
    required property var part
    property bool selected: false

    signal clicked()
    signal levelChanged(int level)
    signal panChanged(int pan)
    signal receivesToggled(bool receives)

    readonly property int partNumber: part.partNumber
    // A Part that receives nothing cannot sound, and the whole strip says so
    // rather than only its switch.
    readonly property bool live: part.receives
    // At short window heights the strip keeps its interactive controls and
    // sheds the read-only rows first — they stay one click away in the
    // inspector — instead of clipping the receive switch off the bottom.
    readonly property bool showSends: height >= 330
    readonly property bool showKeyRange: height >= 290

    implicitWidth: 96
    radius: Metrics.radiusMd
    color: Theme.surface
    border.width: Metrics.borderWidth
    border.color: selected ? Theme.accent : Theme.border

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Part %1, %2, level %3, %4")
        .arg(root.partNumber)
        .arg(root.live ? qsTr("on") : qsTr("off"))
        .arg(root.part.level)
        .arg(root.part.panText)

    TapHandler { onTapped: root.clicked() }

    ColumnLayout {
        anchors { fill: parent; margins: Metrics.spacingSm }
        spacing: Metrics.spacingXs

        // Identity ---------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 2
            XpLabel {
                text: root.part.isRhythmPart ? qsTr("R%1").arg(root.partNumber) : qsTr("%1").arg(root.partNumber)
                role: "overline"
                color: root.live ? Theme.accentText : Theme.textDisabled
            }
            Item { Layout.fillWidth: true }
            XpLabel {
                text: qsTr("ch %1").arg(root.part.midiChannel)
                role: "caption"
                muted: true
            }
        }

        // Part 10 is the Rhythm part: its Patch fields name a Rhythm Setup, and
        // saying so is better than showing a Patch number that is not one.
        StatusPill {
            objectName: "partRhythmPill" + root.partNumber
            visible: root.part.isRhythmPart
            Layout.fillWidth: true
            text: qsTr("RHYTHM")
            tone: "info"
            showDot: false
        }

        XpLabel {
            objectName: "partPatch" + root.partNumber
            Layout.fillWidth: true
            // Number first: at strip width Roland's group vocabulary
            // ("USER&PRESET") elides, and the number is the part a musician
            // scans for. The full assignment stays readable in the tooltip.
            text: root.part.isRhythmPart
                  ? qsTr("Rhythm %1").arg(root.part.patchNumber)
                  : qsTr("%1 · %2").arg(root.part.patchNumber).arg(root.part.patchGroupLabel)
            role: "caption"
            muted: !root.live
            elide: Text.ElideRight
            QQC.ToolTip.visible: patchHover.hovered && truncated
            QQC.ToolTip.delay: 400
            QQC.ToolTip.text: root.part.isRhythmPart
                              ? qsTr("Rhythm %1").arg(root.part.patchNumber)
                              : qsTr("%1 %2").arg(root.part.patchGroupLabel).arg(root.part.patchNumber)
            HoverHandler { id: patchHover }
        }

        XpDivider { Layout.fillWidth: true }

        // Level ------------------------------------------------------------
        XpFader {
            objectName: "partLevel" + root.partNumber
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
            Layout.minimumHeight: 80
            from: 0
            to: 127
            enabled: root.live
            value: root.part.level
            valueText: String(root.part.level)
            Accessible.name: qsTr("Part %1 level").arg(root.partNumber)
            onMoved: root.levelChanged(Math.round(value))
        }

        XpKnob {
            objectName: "partPan" + root.partNumber
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: Metrics.knobSm
            implicitHeight: Metrics.knobSm
            from: 0
            to: 127
            bipolar: true
            enabled: root.live
            value: root.part.pan
            valueText: root.part.panText
            Accessible.name: qsTr("Part %1 pan").arg(root.partNumber)
            onMoved: root.panChanged(Math.round(value))
        }

        // Sends and reserve, read-only here: the inspector edits them, and a
        // strip crowded with every control is harder to read at a glance.
        ColumnLayout {
            visible: root.showSends
            Layout.fillWidth: true
            spacing: 2
            Repeater {
                model: [
                    { key: "cho", value: root.part.chorusSend, max: 127, tint: Theme.tone2,
                      name: qsTr("Chorus send"), warn: false, tag: "" },
                    { key: "rev", value: root.part.reverbSend, max: 127, tint: Theme.tone3,
                      name: qsTr("Reverb send"), warn: false, tag: "" },
                    // Voice Reserve 0 means this Part can be starved by the others.
                    { key: "vcs", value: root.part.voiceReserve, max: 64, tint: Theme.accent,
                      name: qsTr("Voice reserve"), warn: root.part.voiceReserve === 0,
                      tag: "partVoiceReserve" + root.partNumber }
                ]
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Metrics.spacingXs
                    Accessible.role: Accessible.ProgressBar
                    Accessible.name: qsTr("Part %1 %2 %3")
                        .arg(root.partNumber).arg(modelData.name).arg(modelData.value)
                    XpLabel { text: modelData.key; role: "caption"; muted: true }
                    // A send is a continuous amount, so the strip shows how much
                    // rather than only printing the number.
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 4
                        radius: 2
                        color: Theme.surfaceSunken
                        Rectangle {
                            anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                            width: Math.round(parent.width
                                   * Math.max(0, Math.min(1, modelData.value / modelData.max)))
                            radius: 2
                            color: modelData.warn ? Theme.warning : modelData.tint
                            opacity: root.live ? 1.0 : 0.45
                        }
                    }
                    XpLabel {
                        objectName: modelData.tag
                        text: String(modelData.value)
                        role: "caption"
                        horizontalAlignment: Text.AlignRight
                        Layout.minimumWidth: 22
                        color: modelData.warn ? Theme.warning : Theme.textPrimary
                    }
                }
            }
        }

        XpLabel {
            visible: root.showKeyRange
            Layout.fillWidth: true
            text: root.part.keyLowerNote + "–" + root.part.keyUpperNote
            role: "caption"
            muted: true
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }

        XpSwitch {
            objectName: "partReceive" + root.partNumber
            Layout.alignment: Qt.AlignHCenter
            checked: root.live
            Accessible.name: qsTr("Part %1 receives").arg(root.partNumber)
            onToggled: root.receivesToggled(checked)
        }
    }
}
