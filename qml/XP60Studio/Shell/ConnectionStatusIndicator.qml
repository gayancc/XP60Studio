import QtQuick
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// The XP-60 connection chip.
//
// This is an *awareness* control, not a place to explain anything. It shows a
// dot, at most two words, and — when the shell can usefully offer it — one
// action. The full story lives on the Devices screen.
//
// It used to render the whole connection detail inside itself, which grew the
// chip to 46 px inside a 52 px header and elided the sentence mid-word
// ("SELECT YOUR MIDI PORT..., THEN CLICK CONNECT."). It also derived its own
// tone from ConnectionState, in parallel with five other sites; the tone now
// comes from the shell view-model so there is one mapping.
//
// State is never colour alone: the label says it, the dot animates while
// connecting, and a problem also carries an alert icon.
Rectangle {
    id: root

    // The single owner of connection wording and colour.
    required property AppShellViewModel shell
    // Compact is the rail variant: no action button, tighter padding.
    property bool compact: false
    signal actionTriggered()

    readonly property string tone: shell.connectionTone
    readonly property color foreground: Theme.toneForeground(tone)
    readonly property bool showAction: !compact && shell.connectionActionable

    implicitWidth: content.implicitWidth + 2 * Metrics.spacingMd
    // One height for every state, so the header does not change shape as the
    // connection changes.
    implicitHeight: compact ? Metrics.controlHeightSm : Metrics.controlHeight
    radius: Metrics.radiusPill
    color: Theme.toneBackground(tone)
    border.width: Metrics.borderWidth
    border.color: Theme.toneBorder(tone)

    Behavior on color {
        enabled: !Motion.reducedMotion
        ColorAnimation { duration: Motion.durationNormal; easing.type: Motion.easingStandard }
    }
    Behavior on border.color {
        enabled: !Motion.reducedMotion
        ColorAnimation { duration: Motion.durationNormal; easing.type: Motion.easingStandard }
    }

    Accessible.role: Accessible.StaticText
    Accessible.name: root.shell.connectionShortLabel

    RowLayout {
        id: content
        anchors.centerIn: parent
        spacing: Metrics.spacingSm

        // Dot for every state; it pulses while connecting and while MIDI is
        // actually moving, so traffic is visible without a log.
        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: root.foreground
            Layout.alignment: Qt.AlignVCenter
            SequentialAnimation on opacity {
                running: !Motion.reducedMotion
                         && (root.shell.connectionPhase === "connecting" || root.shell.midiActive)
                loops: Animation.Infinite
                alwaysRunToEnd: true
                NumberAnimation { to: 0.25; duration: 450 }
                NumberAnimation { to: 1.0; duration: 450 }
            }
            // Restored explicitly: a stopped SequentialAnimation leaves
            // whatever opacity it was at when the state changed.
            onOpacityChanged: if (!root.shell.midiActive
                                  && root.shell.connectionPhase !== "connecting"
                                  && opacity !== 1.0) opacity = 1.0
        }

        XpIcon {
            visible: root.shell.connectionNeedsAttention
            name: "alert"
            color: root.foreground
            implicitWidth: Metrics.iconSizeSm
            implicitHeight: Metrics.iconSizeSm
            Layout.alignment: Qt.AlignVCenter
        }

        XpLabel {
            text: root.shell.connectionShortLabel
            role: root.compact ? "caption" : "body"
            font.weight: Typography.weightMedium
            color: root.foreground
            Layout.alignment: Qt.AlignVCenter
        }

        XpButton {
            visible: root.showAction
            text: root.shell.connectionActionLabel
            variant: "quiet"
            compact: true
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: Metrics.spacingXs
            onClicked: root.actionTriggered()
        }
    }
}
