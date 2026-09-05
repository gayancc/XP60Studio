import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio

// The whole 128-destination bank, as a map of the panel rather than a table of
// it.
//
// Sixteen small blocks — A1..A8 then B1..B8 — each showing its eight
// destinations as eight cells in the order the NUMBER buttons are in. Reading
// it is the same motion as reading the panel: find the subgroup row, find the
// bank, look along the eight. Clicking a block takes the control surface to
// that exact SUBGROUP and BANK, so the map is a way of moving the panel, not a
// second editor.
//
// It is deliberately not editable. Editing happens on the surface above, where
// a destination is big enough to read and drop onto; a 4 mm cell is a map, and
// pretending otherwise would rebuild the spreadsheet this screen exists to
// replace.
Item {
    id: root

    required property var builder
    // Live during a drag, so hovering a block can move the panel there.
    property int hoverBank: -1
    property int hoverSubgroup: -1

    signal bankPicked(int subgroup, int bank)

    implicitHeight: column.implicitHeight

    // Item for a subgroup/bank pair, so the drag layer can hit-test blocks.
    function blockAt(subgroup, bank) {
        var repeater = subgroup === 0 ? rowA.blocks : rowB.blocks
        return repeater.itemAt(bank - 1)
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        spacing: Metrics.spacingXs

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingSm
            XpLabel { text: qsTr("BANK MAP"); role: "overline"; secondary: true }
            XpLabel {
                text: qsTr("128 destinations · A 001–064 · B 065–128")
                role: "caption"
                muted: true
            }
            Item { Layout.fillWidth: true }
            XpLabel {
                text: qsTr("%1 empty").arg(root.builder.emptyCount)
                role: "caption"
                muted: true
            }
        }

        SubgroupRow {
            id: rowA
            subgroup: 0
            builder: root.builder
            hoverBank: root.hoverSubgroup === 0 ? root.hoverBank : -1
            Layout.fillWidth: true
            onPicked: function (bank) { root.bankPicked(0, bank) }
        }

        SubgroupRow {
            id: rowB
            subgroup: 1
            builder: root.builder
            hoverBank: root.hoverSubgroup === 1 ? root.hoverBank : -1
            Layout.fillWidth: true
            onPicked: function (bank) { root.bankPicked(1, bank) }
        }
    }

    // One subgroup: its letter, then its eight banks.
    component SubgroupRow: RowLayout {
        id: subgroupRow
        required property int subgroup
        required property var builder
        property int hoverBank: -1
        readonly property alias blocks: repeater
        signal picked(int bank)

        spacing: Metrics.spacingSm

        readonly property string letter: subgroup === 0 ? "A" : "B"
        readonly property int filled: subgroup === 0
                                      ? builder.subgroupOccupancyA
                                      : builder.subgroupOccupancyB

        // The subgroup's own identity block, so the two rows are never
        // mistaken for one sixteen-bank strip.
        Rectangle {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 44
            radius: Metrics.radiusSm
            color: subgroupRow.builder.subgroup === subgroupRow.subgroup
                   ? Theme.accentSoft : Theme.surfaceSunken
            border.width: 1
            border.color: subgroupRow.builder.subgroup === subgroupRow.subgroup
                          ? Theme.accent : Theme.borderSubtle

            Column {
                anchors.centerIn: parent
                spacing: 0
                XpLabel {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: subgroupRow.letter
                    role: "title"
                    color: subgroupRow.builder.subgroup === subgroupRow.subgroup
                           ? Theme.accentText : Theme.textSecondary
                }
                XpLabel {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: subgroupRow.filled + "/64"
                    role: "mono"
                    font.pixelSize: 9
                    color: Theme.textMuted
                }
            }
        }

        Repeater {
            id: repeater
            model: 8
            delegate: Rectangle {
                id: block
                required property int index
                readonly property int bank: index + 1
                readonly property var entry: subgroupRow.builder.overview[subgroupRow.subgroup * 8 + index]
                readonly property bool isCurrent: entry !== undefined && entry.current === true
                readonly property bool isHovered: subgroupRow.hoverBank === bank

                Layout.fillWidth: true
                Layout.preferredHeight: 44
                radius: Metrics.radiusSm
                color: isHovered ? Theme.selection
                                 : (isCurrent ? Theme.accentSoft : Theme.surfaceSunken)
                border.width: isCurrent || isHovered ? 2 : 1
                border.color: isHovered ? Theme.accentHover
                                        : (isCurrent ? Theme.accent : Theme.borderSubtle)

                Behavior on color {
                    enabled: !Motion.reducedMotion
                    ColorAnimation { duration: Motion.durationFast }
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 3

                    // The eight destinations, in NUMBER order.
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 2
                        Repeater {
                            model: 8
                            delegate: Rectangle {
                                required property int index
                                readonly property bool on: block.entry !== undefined
                                                           && block.entry.filled !== undefined
                                                           && block.entry.filled[index] === true
                                width: 6
                                height: 8
                                radius: 1
                                color: on ? (block.isCurrent ? Theme.accentText : Theme.live)
                                          : Qt.rgba(0, 0, 0, 0.45)
                                opacity: on ? 1 : 0.8
                            }
                        }
                    }

                    XpLabel {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: subgroupRow.letter + block.bank
                        role: "mono"
                        font.pixelSize: 10
                        font.weight: Typography.weightMedium
                        color: block.isCurrent ? Theme.accentText : Theme.textMuted
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onPressed: subgroupRow.picked(block.bank)
                    // The range this block covers, for anyone who thinks in
                    // 001-128 rather than in panel coordinates.
                    QQC.ToolTip.visible: containsMouse && block.entry !== undefined
                    QQC.ToolTip.delay: 400
                    QQC.ToolTip.text: block.entry === undefined
                                  ? ""
                                  : qsTr("%1 · patches %2–%3 · %4 of 8 filled")
                                    .arg(block.entry.label).arg(block.entry.first)
                                    .arg(block.entry.last).arg(block.entry.occupied)
                }
            }
        }
    }
}
