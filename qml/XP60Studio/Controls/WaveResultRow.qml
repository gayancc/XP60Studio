import QtQuick
import QtQuick.Layouts
import XP60Studio

Rectangle {
    id: root

    required property int index
    required property string waveName
    required property string waveKey
    required property string bank
    required property int number
    required property string availability

    property bool catalogMissing: false
    property bool selected: false
    property bool listFocused: false
    signal activated()

    readonly property string effectiveAvailability: catalogMissing ? "missing_catalog" : availability

    height: 48
    radius: Metrics.radiusSm
    color: selected ? Theme.selection : (hover.hovered ? Theme.surfaceHover : "transparent")
    border.width: (listFocused || activeFocus) && selected ? 1 : 0
    border.color: Theme.focusRing

    Accessible.role: Accessible.ListItem
    Accessible.name: waveName + ", " + waveKey
    Accessible.selected: selected
    activeFocusOnTab: true
    Keys.onSpacePressed: root.activated()
    Keys.onReturnPressed: root.activated()
    Keys.onEnterPressed: root.activated()

    HoverHandler { id: hover }
    TapHandler { onTapped: root.activated() }

    RowLayout {
        anchors { fill: parent; leftMargin: Metrics.spacingSm; rightMargin: Metrics.spacingSm }
        spacing: Metrics.spacingSm
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            XpLabel {
                text: root.waveName
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.weight: root.selected ? Typography.weightMedium : Typography.weightRegular
            }
            XpLabel {
                text: root.waveKey
                role: "caption"
                secondary: true
            }
        }
        WaveAvailabilityBadge { availability: root.effectiveAvailability }
    }
}
