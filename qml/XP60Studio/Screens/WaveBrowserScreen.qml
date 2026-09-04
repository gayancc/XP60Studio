import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// M3 catalog workspace. Source labels are not converted to device addresses.
FocusScope {
    id: root
    required property WaveBrowserModel catalog
    signal closed()
    readonly property bool wide: width >= 1040
    onVisibleChanged: if (visible) search.forceActiveFocus()
    Keys.onEscapePressed: root.closed()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.screenPadding
        spacing: Metrics.spacingMd

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                XpLabel { text: qsTr("WAVE BROWSER"); role: "overline"; color: Theme.accentText }
                XpLabel { text: qsTr("Find the source of your sound"); role: "title"; Layout.fillWidth: true; elide: Text.ElideRight }
            }
            StatusPill { text: qsTr("CATALOG ONLY"); tone: "info"; showDot: false }
            XpButton { objectName: "closeWaveBrowser"; text: qsTr("Back to Editor"); onClicked: root.closed() }
        }

        RowLayout {
            Layout.fillWidth: true
            XpTextField {
                id: search
                objectName: "waveSearch"
                Layout.fillWidth: true
                placeholderText: qsTr("Search name, bank or number…")
                text: root.catalog.query
                onTextEdited: root.catalog.query = text
            }
            XpComboBox {
                objectName: "waveSourceFilter"
                Accessible.name: qsTr("Waveform source")
                model: [qsTr("All sources"), "INT-A", "INT-B", qsTr("Expansion")]
                currentIndex: root.catalog.sourceFilter
                onActivated: root.catalog.sourceFilter = currentIndex
            }
        }
        XpLabel {
            text: qsTr("%1 results · 448 documented internal waveforms · Categories unknown").arg(root.catalog.count)
            role: "caption"; secondary: true
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: root.wide ? 2 : 1
            columnSpacing: Metrics.spacingMd
            rowSpacing: Metrics.spacingMd

            XpCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 150
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Metrics.spacingSm
                    RowLayout {
                        XpLabel { text: qsTr("WAVEFORM"); role: "overline"; secondary: true; Layout.fillWidth: true }
                        XpLabel { text: qsTr("SOURCE / NUMBER"); role: "overline"; secondary: true }
                    }
                    XpDivider { Layout.fillWidth: true }
                    ListView {
                        id: results
                        objectName: "waveResults"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        reuseItems: true
                        model: root.catalog
                        currentIndex: root.catalog.selectedRow
                        boundsBehavior: Flickable.StopAtBounds
                        QQC.ScrollBar.vertical: XpScrollBar {}
                        Keys.onDownPressed: {
                            root.catalog.selectRow(Math.min(count - 1, currentIndex + 1))
                            positionViewAtIndex(currentIndex, ListView.Contain)
                        }
                        Keys.onUpPressed: {
                            root.catalog.selectRow(Math.max(0, currentIndex - 1))
                            positionViewAtIndex(currentIndex, ListView.Contain)
                        }
                        delegate: QQC.ItemDelegate {
                            id: waveRow
                            required property int index
                            required property string waveName
                            required property string waveKey
                            width: results.width - 12
                            height: 46
                            Accessible.name: waveName + ", " + waveKey
                            onClicked: {
                                root.catalog.selectRow(index)
                                results.forceActiveFocus()
                            }
                            background: Rectangle {
                                radius: Metrics.radiusSm
                                color: root.catalog.selectedRow === waveRow.index ? Theme.selection : (waveRow.hovered ? Theme.surfaceHover : "transparent")
                                border.width: results.activeFocus && root.catalog.selectedRow === waveRow.index ? 1 : 0
                                border.color: Theme.focusRing
                            }
                            contentItem: RowLayout {
                                XpLabel { text: waveRow.waveName; Layout.fillWidth: true; elide: Text.ElideRight }
                                XpLabel { text: waveRow.waveKey; role: "caption"; secondary: true }
                            }
                        }
                        XpEmptyState {
                            anchors.fill: parent
                            visible: root.catalog.count === 0
                            title: root.catalog.sourceFilter === 3 ? qsTr("Expansion catalog unavailable") : qsTr("No matching waveforms")
                            message: root.catalog.sourceFilter === 3 ? qsTr("Expansion names and installed-board compatibility have not been verified.") : qsTr("Try another name, number or source.")
                        }
                    }
                }
            }

            XpCard {
                Layout.preferredWidth: root.wide ? 320 : -1
                Layout.fillWidth: !root.wide
                Layout.fillHeight: root.wide
                implicitHeight: detailColumn.implicitHeight + 2 * Metrics.cardPadding
                ColumnLayout {
                    id: detailColumn
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    spacing: Metrics.spacingSm
                    XpLabel { text: qsTr("WAVE DETAILS"); role: "overline"; color: Theme.accentText }
                    XpLabel { text: root.catalog.selected.name || qsTr("Select a waveform"); role: "title"; Layout.fillWidth: true; elide: Text.ElideRight }
                    XpLabel { text: root.catalog.selected.key || qsTr("Browse with mouse or arrow keys"); secondary: true }
                    XpDivider { Layout.fillWidth: true }
                    XpLabel {
                        Layout.fillWidth: true
                        text: root.catalog.selected.name ? qsTr("Internal ROM · Roland XP-60/80 Waveform List, page %1").arg(root.catalog.selected.page) : qsTr("Names and bank numbering come from Roland’s waveform list.")
                        wrapMode: Text.WordWrap; role: "caption"; secondary: true
                    }
                    XpLabel {
                        Layout.fillWidth: true
                        text: qsTr("Category, sample rate and loop metadata: unknown. Audio preview is unavailable.")
                        wrapMode: Text.WordWrap; role: "caption"; muted: true
                    }
                    XpLabel {
                        Layout.fillWidth: true
                        text: qsTr("Wave assignment awaits XP-60 bank-mapping validation. Browsing leaves your Patch unchanged.")
                        wrapMode: Text.WordWrap; role: "caption"; color: Theme.warning
                    }
                }
            }
        }
    }
}
