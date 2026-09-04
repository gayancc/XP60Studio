import QtQuick
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// One of the four Tone cards that anchor the Patch Editor.
//
// Tone identity colour is a shared semantic token, so this card, its mini
// envelope, the signal-flow connector and the big envelope editor all agree.
Rectangle {
    id: root

    required property ToneViewModel tone
    property bool selected: false
    signal clicked()

    readonly property color toneColor: Theme.toneColor(tone.toneNumber)
    readonly property bool dimmed: !tone.enabled || !tone.audible

    radius: Metrics.radiusMd
    color: Theme.surface
    border.width: selected ? 2 : Metrics.borderWidth
    border.color: selected ? toneColor : Qt.rgba(toneColor.r, toneColor.g, toneColor.b, 0.45)
    implicitHeight: layout.implicitHeight + 2 * Metrics.spacingMd

    // Faint tone-coloured wash, as in the mockup.
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(root.toneColor.r, root.toneColor.g, root.toneColor.b, 0.16) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Tone %1, %2, %3").arg(tone.toneNumber).arg(tone.waveText)
                        .arg(tone.enabled ? qsTr("enabled") : qsTr("disabled"))
    activeFocusOnTab: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            root.clicked(); event.accepted = true
        }
    }
    TapHandler { onTapped: root.clicked() }

    Rectangle {
        visible: root.activeFocus
        anchors.fill: parent
        anchors.margins: -2
        radius: parent.radius + 2
        color: "transparent"
        border.width: 1
        border.color: Theme.focusRing
    }

    ColumnLayout {
        id: layout
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: Metrics.spacingMd }
        spacing: Metrics.spacingSm
        opacity: root.dimmed ? 0.55 : 1.0
        Behavior on opacity { NumberAnimation { duration: Motion.durationFast } }

        // Identity
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingSm
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true
                XpLabel {
                    text: qsTr("TONE %1").arg(root.tone.toneNumber)
                    role: "overline"
                    color: root.toneColor
                }
                XpLabel {
                    objectName: "toneWaveName"
                    text: root.tone.waveText
                    role: "title"
                    Layout.fillWidth: true
                }
            }
            // Explicit enable marker, with mouse and keyboard parity.
            Rectangle {
                objectName: "toneEnableButton"
                implicitWidth: 26
                implicitHeight: 26
                radius: Metrics.radiusSm
                color: enableHover.hovered ? Theme.surfaceHover : "transparent"
                border.width: 1
                border.color: activeFocus ? Theme.focusRing : root.tone.enabled ? root.toneColor : Theme.border
                Accessible.role: Accessible.CheckBox
                Accessible.name: qsTr("Tone %1 enabled").arg(root.tone.toneNumber)
                Accessible.checked: root.tone.enabled
                activeFocusOnTab: true
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        root.tone.enabled = !root.tone.enabled
                        event.accepted = true
                    }
                }
                XpIcon {
                    anchors.centerIn: parent
                    name: "power"
                    color: root.tone.enabled ? root.toneColor : Theme.textDisabled
                }
                HoverHandler { id: enableHover }
                TapHandler { onTapped: root.tone.enabled = !root.tone.enabled }
            }
        }

        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Qt.rgba(root.toneColor.r, root.toneColor.g, root.toneColor.b, 0.35) }

        // Wave
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            XpLabel { text: qsTr("Wave"); role: "overline"; secondary: true }
            XpLabel {
                text: root.tone.waveSourceText
                role: "caption"
                muted: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        // Level / Pan / Octave
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingSm

            ColumnLayout {
                spacing: 2
                XpLabel { text: qsTr("Level"); role: "overline"; secondary: true; Layout.alignment: Qt.AlignHCenter }
                XpKnob {
                    objectName: "toneLevelKnob"
                    from: 0; to: 127
                    value: root.tone.level
                    accentColor: root.toneColor
                    defaultValue: 100
                    valueText: qsTr("Level %1").arg(root.tone.level)
                    onMoved: root.tone.level = Math.round(value)
                    Layout.alignment: Qt.AlignHCenter
                }
                XpLabel { text: root.tone.levelText; role: "mono"; Layout.alignment: Qt.AlignHCenter }
            }

            ColumnLayout {
                spacing: 2
                XpLabel { text: qsTr("Pan"); role: "overline"; secondary: true; Layout.alignment: Qt.AlignHCenter }
                XpKnob {
                    objectName: "tonePanKnob"
                    from: 0; to: 127
                    value: root.tone.pan
                    bipolar: true
                    accentColor: root.toneColor
                    defaultValue: 64
                    valueText: qsTr("Pan %1").arg(root.tone.panText)
                    onMoved: root.tone.pan = Math.round(value)
                    Layout.alignment: Qt.AlignHCenter
                }
                XpLabel { text: root.tone.panText; role: "mono"; Layout.alignment: Qt.AlignHCenter }
            }

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                XpLabel { text: qsTr("Octave"); role: "overline"; secondary: true; Layout.alignment: Qt.AlignHCenter }
                Rectangle {
                    objectName: "toneOctaveField"
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 46
                    implicitHeight: 30
                    radius: Metrics.radiusSm
                    color: Theme.surfaceSunken
                    border.width: 1
                    border.color: Theme.borderStrong
                    XpLabel { anchors.centerIn: parent; text: root.tone.octaveText; role: "mono" }
                    Accessible.role: Accessible.SpinBox
                    Accessible.name: qsTr("Tone %1 octave").arg(root.tone.toneNumber)
                    // Wheel and keyboard both step by a whole octave.
                    WheelHandler { onWheel: function(e) { root.tone.nudgeOctave(e.angleDelta.y > 0 ? 1 : -1) } }
                    activeFocusOnTab: true
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Up) { root.tone.nudgeOctave(1); event.accepted = true }
                        else if (event.key === Qt.Key_Down) { root.tone.nudgeOctave(-1); event.accepted = true }
                    }
                }
                XpLabel {
                    // Coarse Tune is per-semitone on the XP-60. Name the exact
                    // tuning only when the Octave box cannot express it; a
                    // whole-octave value would just repeat the box above.
                    text: root.tone.coarseTune % 12 === 0 ? "" : qsTr("%1 st").arg(root.tone.coarseTune)
                    role: "caption"
                    muted: true
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }

        // Mini envelope + Solo/Mute
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingSm
            ToneMiniEnvelope {
                objectName: "toneMiniEnvelope"
                Layout.fillWidth: true
                points: root.tone.miniEnvelope
                strokeColor: root.toneColor
                dimmed: root.dimmed
            }
            RowLayout {
                spacing: 4
                Layout.alignment: Qt.AlignBottom
                Repeater {
                    model: [{ key: "S", solo: true }, { key: "M", solo: false }]
                    delegate: Rectangle {
                        id: auditionButton
                        required property var modelData
                        readonly property bool active: modelData.solo ? root.tone.solo : root.tone.mute
                        implicitWidth: 28
                        implicitHeight: 26
                        radius: Metrics.radiusSm
                        objectName: modelData.solo ? "toneSoloButton" : "toneMuteButton"
                        color: active ? (modelData.solo ? Theme.warning : Theme.error)
                             : auditionHover.hovered ? Theme.surfaceHover : Theme.surfaceRaised
                        border.width: 1
                        border.color: activeFocus ? Theme.focusRing : active ? "transparent" : Theme.borderStrong
                        Accessible.role: Accessible.Button
                        Accessible.name: (modelData.solo ? qsTr("Solo Tone %1") : qsTr("Mute Tone %1")).arg(root.tone.toneNumber)
                        Accessible.checked: active
                        activeFocusOnTab: true
                        Keys.onPressed: function(event) {
                            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                if (modelData.solo) root.tone.solo = !root.tone.solo
                                else root.tone.mute = !root.tone.mute
                                event.accepted = true
                            }
                        }
                        XpLabel {
                            anchors.centerIn: parent
                            text: auditionButton.modelData.key
                            role: "caption"
                            font.weight: Typography.weightBold
                            color: auditionButton.active ? Theme.textOnAccent : Theme.textSecondary
                        }
                        HoverHandler { id: auditionHover }
                        TapHandler {
                            onTapped: {
                                if (auditionButton.modelData.solo)
                                    root.tone.solo = !root.tone.solo
                                else
                                    root.tone.mute = !root.tone.mute
                            }
                        }
                    }
                }
            }
        }
    }
}
