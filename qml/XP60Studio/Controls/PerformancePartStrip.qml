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
            text: root.part.isRhythmPart
                  ? qsTr("Rhythm %1").arg(root.part.patchNumber)
                  : qsTr("%1 %2").arg(root.part.patchGroupLabel).arg(root.part.patchNumber)
            role: "caption"
            muted: !root.live
            elide: Text.ElideRight
        }

        XpDivider { Layout.fillWidth: true }

        // Level ------------------------------------------------------------
        XpFader {
            objectName: "partLevel" + root.partNumber
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
            Layout.minimumHeight: 120
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
            implicitWidth: 44
            implicitHeight: 44
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
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Metrics.spacingXs
            rowSpacing: 1
            XpLabel { text: qsTr("cho"); role: "caption"; muted: true }
            XpLabel { text: String(root.part.chorusSend); role: "caption"; horizontalAlignment: Text.AlignRight; Layout.fillWidth: true }
            XpLabel { text: qsTr("rev"); role: "caption"; muted: true }
            XpLabel { text: String(root.part.reverbSend); role: "caption"; horizontalAlignment: Text.AlignRight; Layout.fillWidth: true }
            XpLabel { text: qsTr("vcs"); role: "caption"; muted: true }
            XpLabel {
                objectName: "partVoiceReserve" + root.partNumber
                text: String(root.part.voiceReserve)
                role: "caption"
                horizontalAlignment: Text.AlignRight
                Layout.fillWidth: true
                // Voice Reserve 0 means this Part can be starved by the others.
                color: root.part.voiceReserve === 0 ? Theme.warning : Theme.textPrimary
            }
        }

        XpLabel {
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
