import QtQuick
import QtQuick.Layouts
import XP60Studio

// Mutually exclusive choices shown side by side — the editor's
// Sound/Filter/Amp/Motion/Effects navigation.
Item {
    id: root

    property var model: []
    property int currentIndex: 0
    signal activated(int index)

    implicitHeight: Metrics.controlHeight
    implicitWidth: row.implicitWidth

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: Metrics.spacingSm

        Repeater {
            model: root.model
            delegate: Rectangle {
                id: segment
                required property var modelData
                required property int index
                readonly property bool current: root.currentIndex === index

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: label.implicitWidth + 2 * Metrics.spacingLg
                radius: Metrics.radiusSm
                color: current ? Theme.accent
                     : hover.hovered ? Theme.surfaceHover : Theme.surfaceRaised
                border.width: Metrics.borderWidth
                border.color: current ? Theme.accent : Theme.border

                Behavior on color { ColorAnimation { duration: Motion.durationFast } }

                Accessible.role: Accessible.RadioButton
                Accessible.name: segment.modelData
                Accessible.checked: current
                activeFocusOnTab: true

                Rectangle {
                    visible: segment.activeFocus
                    anchors.fill: parent
                    anchors.margins: -2
                    radius: parent.radius + 2
                    color: "transparent"
                    border.width: 1
                    border.color: Theme.focusRing
                }

                XpLabel {
                    id: label
                    anchors.centerIn: parent
                    text: segment.modelData
                    role: "overline"
                    color: segment.current ? Theme.textOnAccent : Theme.textSecondary
                    font.weight: Typography.weightMedium
                }

                HoverHandler { id: hover }
                TapHandler { onTapped: root.activated(segment.index) }
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                        root.activated(segment.index)
                        event.accepted = true
                    }
                }
            }
        }
    }
}
