import QtQuick
import QtQuick.Layouts
import XP60Studio

// One Tone's contribution to the current Patch, on the Dashboard.
//
// The mockup draws these as meters reading in dB. The XP-60 stores a Tone level
// as a 0-127 value and the manual gives no conversion to decibels, so inventing
// one here would put a number on screen that the instrument never said. The bar
// therefore shows the level as what it is, and the readout is the XP value.
Rectangle {
    id: root

    required property var tone
    required property int toneNumber

    signal openRequested()

    readonly property color toneTint: Theme.toneColor(root.toneNumber)
    readonly property bool audible: tone ? tone.audible : false
    readonly property int level: tone ? tone.level : 0

    implicitHeight: column.implicitHeight + 2 * Metrics.spacingSm
    radius: Metrics.radiusSm
    color: hover.containsMouse ? Theme.surfaceHover : Theme.surfaceRaised
    border.width: 1
    border.color: hover.containsMouse ? root.toneTint
                 : root.audible ? Theme.borderStrong : Theme.borderSubtle
    // A Tone that is off is still shown: the Patch has four, and hiding the
    // silent ones would misrepresent its shape.
    opacity: root.audible ? 1.0 : 0.55

    Behavior on color {
        enabled: !Motion.reducedMotion
        ColorAnimation { duration: Motion.durationFast }
    }

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Tone %1").arg(root.toneNumber)
    Accessible.description: (root.tone ? root.tone.waveText : "")
                            + qsTr(", level %1").arg(root.level)
                            + (root.audible ? "" : qsTr(", not sounding"))
    activeFocusOnTab: true
    Keys.onReturnPressed: root.openRequested()
    Keys.onSpacePressed: root.openRequested()

    ColumnLayout {
        id: column
        anchors {
            left: parent.left; right: parent.right; top: parent.top
            margins: Metrics.spacingSm
        }
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingXs
            XpLabel {
                text: qsTr("TONE %1").arg(root.toneNumber)
                role: "overline"
                color: root.toneTint
            }
            Item { Layout.fillWidth: true }
            // Why a Tone is silent matters: switched off is not the same as
            // muted by another Tone's solo.
            XpLabel {
                visible: !root.audible
                text: root.tone && !root.tone.enabled ? qsTr("OFF") : qsTr("MUTED")
                role: "caption"
                muted: true
            }
        }

        XpLabel {
            Layout.fillWidth: true
            text: root.tone ? root.tone.waveText : ""
            role: "body"
            elide: Text.ElideRight
        }
        XpLabel {
            Layout.fillWidth: true
            text: root.tone ? root.tone.waveSourceText : ""
            role: "caption"
            secondary: true
            elide: Text.ElideRight
        }

        // Level as a proportion of the XP range, with the raw value beside it.
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Metrics.spacingXs
            spacing: Metrics.spacingXs

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 6
                radius: 3
                color: Theme.surfaceSunken
                Rectangle {
                    width: parent.width * Math.max(0, Math.min(1, root.level / 127))
                    height: parent.height
                    radius: parent.radius
                    color: root.toneTint
                    opacity: root.audible ? 1.0 : 0.5
                    Behavior on width {
                        enabled: !Motion.reducedMotion
                        NumberAnimation { duration: Motion.durationNormal; easing.type: Motion.easingStandard }
                    }
                }
            }
            XpLabel {
                objectName: "toneLevelValue"
                text: root.tone ? root.tone.levelText : ""
                role: "mono"
                font.weight: Typography.weightMedium
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: Metrics.radiusSm + 3
        color: "transparent"
        border.width: 2
        border.color: Theme.focusRing
        visible: root.activeFocus
    }

    MouseArea {
        id: hover
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.forceActiveFocus()
            root.openRequested()
        }
    }
}
