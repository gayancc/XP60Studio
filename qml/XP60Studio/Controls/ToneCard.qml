import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
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
    // This Tone's entry from PatchEditorViewModel.toneCompatibility, or an
    // empty map where no instrument has been declared and no verdict exists.
    property var compatibility: ({})
    signal clicked()
    signal browseWaves()
    // The three explicit ways out of a missing wave. XP60Studio offers them and
    // takes none of them by itself; nothing here replaces a wave.
    signal findReplacement()
    signal disableTone()
    signal keepAnyway()

    readonly property color toneColor: Theme.toneColor(tone.toneNumber)
    readonly property bool dimmed: !tone.enabled || !tone.audible

    radius: Metrics.radiusMd
    color: Theme.surface
    border.width: Metrics.borderWidth
    border.color: selected ? toneColor : Qt.rgba(toneColor.r, toneColor.g, toneColor.b, 0.45)
    // Dense desktop mixer card: the four sit side-by-side, so every vertical
    // pixel spent on padding is one less for the envelope editor below.
    implicitHeight: layout.implicitHeight + 2 * Metrics.spacingSm

    // Selection is drawn as an inset ring rather than a thicker border, so the
    // card's content does not move by a pixel when it becomes current.
    Rectangle {
        visible: root.selected
        anchors.fill: parent
        anchors.margins: 2
        radius: parent.radius - 2
        color: "transparent"
        border.width: Metrics.borderWidth
        border.color: root.toneColor
    }

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
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: Metrics.spacingSm }
        spacing: Metrics.spacingXs
        opacity: root.dimmed ? 0.55 : 1.0
        Behavior on opacity { NumberAnimation { duration: Motion.durationFast } }

        // Identity + enable on one row; wave source sits under the name so the
        // card does not spend a whole section on "Wave / Browse waves".
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
                    role: "subheading"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingXs
                    XpLabel {
                        text: root.tone.waveSourceText
                        role: "caption"
                        muted: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    RowLayout {
                        spacing: 2
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Browse waves for Tone %1").arg(root.tone.toneNumber)
                        activeFocusOnTab: true
                        Keys.onPressed: function(event) {
                            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                                root.browseWaves(); event.accepted = true
                            }
                        }
                        XpLabel {
                            text: qsTr("Browse")
                            role: "caption"
                            color: waveHover.hovered ? Theme.textPrimary : Theme.accentText
                        }
                        XpIcon {
                            name: "chevron-right"
                            color: waveHover.hovered ? Theme.textPrimary : Theme.accentText
                            implicitWidth: Metrics.iconSizeSm
                            implicitHeight: Metrics.iconSizeSm
                        }
                        HoverHandler { id: waveHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: root.browseWaves() }
                    }
                }
            }
            // Explicit enable marker, with mouse and keyboard parity.
            Rectangle {
                objectName: "toneEnableButton"
                implicitWidth: Metrics.controlHeightSm
                implicitHeight: Metrics.controlHeightSm
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
                    implicitWidth: Metrics.iconSizeSm
                    implicitHeight: Metrics.iconSizeSm
                }
                HoverHandler { id: enableHover }
                TapHandler { onTapped: root.tone.enabled = !root.tone.enabled }
            }
        }

        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Qt.rgba(root.toneColor.r, root.toneColor.g, root.toneColor.b, 0.35) }

        // ── When the wave is on a board this instrument may not have ────────
        //
        // Shown, never fixed. There is no mapping from an expansion wave to an
        // internal one that this project could write honestly, so the three
        // buttons below are the whole of what is offered — and "Keep anyway"
        // changes nothing at all, which is the point of naming it.
        ColumnLayout {
            objectName: "toneCompatibilityPrompt" + root.tone.toneNumber
            Layout.fillWidth: true
            spacing: Metrics.spacingXs
            visible: root.compatibility.needsAttention === true

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingXs
                StatusPill {
                    text: root.compatibility.status === "expansion-missing" ? qsTr("BOARD MISSING")
                                                                            : qsTr("BOARD UNKNOWN")
                    tone: root.compatibility.tone ?? "warning"
                    showDot: false
                }
                Item { Layout.fillWidth: true }
            }
            XpLabel {
                Layout.fillWidth: true
                text: root.compatibility.status === "expansion-missing"
                      ? qsTr("This wave is on expansion wave group %1, which no board you have declared provides. It will not sound.").arg(root.compatibility.groupId)
                      : qsTr("This wave is on expansion wave group %1. Declare your boards in the Expansion Manager and XP60Studio can say whether you have it.").arg(root.compatibility.groupId)
                role: "caption"
                muted: true
                wrapMode: Text.WordWrap
            }
            Flow {
                Layout.fillWidth: true
                spacing: Metrics.spacingXs
                XpButton {
                    objectName: "toneFindReplacement" + root.tone.toneNumber
                    text: qsTr("Find replacement")
                    compact: true
                    variant: "primary"
                    onClicked: root.findReplacement()
                }
                XpButton {
                    objectName: "toneDisable" + root.tone.toneNumber
                    text: qsTr("Disable Tone")
                    compact: true
                    variant: "ghost"
                    enabled: root.tone.enabled
                    onClicked: root.disableTone()
                }
                XpButton {
                    objectName: "toneKeepAnyway" + root.tone.toneNumber
                    text: qsTr("Keep anyway")
                    compact: true
                    variant: "ghost"
                    onClicked: root.keepAnyway()
                }
            }
        }

        // Level / Pan / Octave — compact knobs so the four cards stay readable
        // as a mixer strip rather than four tall panels.
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingXs

            ColumnLayout {
                spacing: 1
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                XpLabel { text: qsTr("Level"); role: "caption"; secondary: true; Layout.alignment: Qt.AlignHCenter }
                XpKnob {
                    objectName: "toneLevelKnob"
                    implicitWidth: Metrics.knobSm; implicitHeight: Metrics.knobSm
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
                spacing: 1
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                XpLabel { text: qsTr("Pan"); role: "caption"; secondary: true; Layout.alignment: Qt.AlignHCenter }
                XpKnob {
                    objectName: "tonePanKnob"
                    implicitWidth: Metrics.knobSm; implicitHeight: Metrics.knobSm
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
                spacing: 1
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                XpLabel { text: qsTr("Octave"); role: "caption"; secondary: true; Layout.alignment: Qt.AlignHCenter }
                // Octave transpose is a musical control, so it turns like the
                // other two. Coarse Tune is per-semitone on the XP-60: the
                // knob moves whole octaves and the readout keeps any
                // leftover semitones visible.
                XpKnob {
                    objectName: "toneOctaveField"
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: Metrics.knobSm; implicitHeight: Metrics.knobSm
                    from: -4; to: 4
                    stepSize: 1
                    snapMode: T.Dial.SnapAlways
                    bipolar: true
                    defaultValue: 0
                    value: root.tone.octave
                    // Eight steps across the whole sweep, so one octave is a
                    // deliberate move rather than a twitch.
                    dragPixelsForFullRange: 160
                    accentColor: root.toneColor
                    valueText: qsTr("Octave %1").arg(root.tone.octaveText)
                    Accessible.name: qsTr("Tone %1 octave").arg(root.tone.toneNumber)
                    onMoved: root.tone.nudgeOctave(Math.round(value) - root.tone.octave)
                }
                XpLabel { text: root.tone.octaveText; role: "mono"; Layout.alignment: Qt.AlignHCenter }
            }
        }

        // Mini envelope + Solo/Mute
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingXs
            ToneMiniEnvelope {
                objectName: "toneMiniEnvelope"
                Layout.fillWidth: true
                implicitHeight: 32
                points: root.tone.miniEnvelope
                strokeColor: root.toneColor
                dimmed: root.dimmed
            }
            RowLayout {
                spacing: 2
                Layout.alignment: Qt.AlignBottom
                Repeater {
                    model: [{ key: "S", solo: true }, { key: "M", solo: false }]
                    delegate: Rectangle {
                        id: auditionButton
                        required property var modelData
                        readonly property bool active: modelData.solo ? root.tone.solo : root.tone.mute
                        implicitWidth: Metrics.controlHeightSm
                        implicitHeight: Metrics.controlHeightSm
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
