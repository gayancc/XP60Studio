import QtQuick
import QtQuick.Layouts
import XP60Studio

// One Tone's contribution to the current Patch, on the Dashboard.
//
// The mockup draws these as vertical meters reading in dB. The XP-60 stores a
// Tone level as a 0-127 value and the manual gives no conversion to decibels,
// so inventing one here would put a number on screen that the instrument never
// said. The meter therefore keeps the mockup's shape — a vertical column that
// makes four Tones comparable at a glance — and reads in XP values.
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
            left: parent.left; right: parent.right
            top: parent.top; bottom: parent.bottom
            margins: Metrics.spacingSm
        }
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
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

        // Level as a proportion of the XP range. The only item here that grows,
        // so whatever height the Dashboard has spare becomes meter resolution
        // rather than empty card.
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: Metrics.spacingXs
            Layout.minimumHeight: 44
            radius: Metrics.radiusSm
            color: Theme.surfaceSunken
            border.width: 1
            border.color: Theme.borderSubtle
            clip: true

            // Quarter marks, so two Tones can be compared without reading the
            // numbers. Not a dB scale — the XP-60 does not give one.
            Repeater {
                model: 3
                Rectangle {
                    required property int index
                    anchors { left: parent.left; right: parent.right }
                    y: Math.round(parent.height * (index + 1) / 4)
                    height: 1
                    color: Theme.borderSubtle
                }
            }

            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 2 }
                height: Math.max(2, (parent.height - 4)
                        * Math.max(0, Math.min(1, root.level / 127)))
                radius: 2
                color: root.toneTint
                opacity: root.audible ? 1.0 : 0.5
                Behavior on height {
                    enabled: !Motion.reducedMotion
                    NumberAnimation { duration: Motion.durationNormal; easing.type: Motion.easingStandard }
                }
            }

            XpLabel {
                objectName: "toneLevelValue"
                anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 4 }
                text: root.tone ? root.tone.levelText : ""
                role: "mono"
                font.weight: Typography.weightMedium
                color: Theme.textPrimary
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
