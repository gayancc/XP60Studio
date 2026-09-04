import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// M3 catalog workspace — visual exploration; assignment awaits hardware mapping.
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

        WaveSearchBar {
            id: search
            Layout.fillWidth: true
            text: root.catalog.query
            onTextEdited: root.catalog.query = text
        }

        Flow {
            Layout.fillWidth: true
            spacing: Metrics.spacingXs
            Repeater {
                model: [
                    { label: qsTr("All"), value: 0 },
                    { label: "INT-A", value: 1 },
                    { label: "INT-B", value: 2 },
                    { label: qsTr("Expansion"), value: 3 }
                ]
                delegate: XpButton {
                    required property var modelData
                    text: modelData.label
                    compact: true
                    variant: root.catalog.sourceFilter === modelData.value ? "primary" : "ghost"
                    onClicked: root.catalog.sourceFilter = modelData.value
                }
            }
            // Keep ComboBox for accessibility / tests
            XpComboBox {
                objectName: "waveSourceFilter"
                Accessible.name: qsTr("Waveform source")
                visible: false
                width: 0
                height: 0
                model: [qsTr("All sources"), "INT-A", "INT-B", qsTr("Expansion")]
                currentIndex: root.catalog.sourceFilter
                onActivated: root.catalog.sourceFilter = currentIndex
            }
        }

        XpLabel {
            text: qsTr("%1 results · Internal ROM catalog · Category taxonomy not verified").arg(root.catalog.count)
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
                        XpLabel { text: qsTr("AVAILABILITY"); role: "overline"; secondary: true }
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
                        delegate: WaveResultRow {
                            width: results.width - 12
                            catalogMissing: root.catalog.sourceFilter === 3
                            selected: root.catalog.selectedRow === index
                            listFocused: results.activeFocus
                            onActivated: {
                                root.catalog.selectRow(index)
                                results.forceActiveFocus()
                            }
                        }
                        XpEmptyState {
                            anchors.fill: parent
                            visible: root.catalog.count === 0
                            title: root.catalog.sourceFilter === 3 ? qsTr("Expansion catalog unavailable") : qsTr("No matching waveforms")
                            message: root.catalog.sourceFilter === 3
                                     ? qsTr("Expansion names and installed-board compatibility have not been verified.")
                                     : qsTr("Try another name, number or source.")
                        }
                    }
                }
            }

            XpCard {
                Layout.preferredWidth: root.wide ? 340 : -1
                Layout.fillWidth: !root.wide
                Layout.fillHeight: root.wide
                implicitHeight: detailColumn.implicitHeight + 2 * Metrics.cardPadding
                ColumnLayout {
                    id: detailColumn
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    spacing: Metrics.spacingSm
                    XpLabel { text: qsTr("WAVE DETAILS"); role: "overline"; color: Theme.accentText }
                    XpLabel {
                        text: root.catalog.selected.name || qsTr("Select a waveform")
                        role: "title"
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        WaveAvailabilityBadge {
                            availability: root.catalog.sourceFilter === 3 ? "missing_catalog"
                                          : (root.catalog.selected.name ? "available" : "unknown")
                        }
                        StatusPill {
                            visible: root.catalog.selected.bank !== undefined && root.catalog.selected.bank.length > 0
                            text: root.catalog.selected.bank || ""
                            tone: "accent"
                            showDot: false
                        }
                        Item { Layout.fillWidth: true }
                    }
                    XpDivider { Layout.fillWidth: true }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: Metrics.spacingMd
                        rowSpacing: Metrics.spacingXs
                        visible: root.catalog.selected.name !== undefined && root.catalog.selected.name.length > 0
                        XpLabel { text: qsTr("Bank"); role: "overline"; secondary: true }
                        XpLabel { text: root.catalog.selected.bank || "—"; role: "body" }
                        XpLabel { text: qsTr("Number"); role: "overline"; secondary: true }
                        XpLabel { text: root.catalog.selected.key || "—"; role: "mono" }
                        XpLabel { text: qsTr("Source page"); role: "overline"; secondary: true }
                        XpLabel { text: root.catalog.selected.page || "—"; role: "mono" }
                        XpLabel { text: qsTr("Category"); role: "overline"; secondary: true }
                        XpLabel { text: qsTr("Unknown"); role: "caption"; muted: true }
                    }

                    XpLabel {
                        visible: !root.catalog.selected.name
                        Layout.fillWidth: true
                        text: qsTr("Browse with mouse or arrow keys. Names and bank numbering come from Roland’s waveform list.")
                        wrapMode: Text.WordWrap; role: "caption"; secondary: true
                    }
                    XpLabel {
                        Layout.fillWidth: true
                        text: qsTr("Audio preview unavailable. Wave assignment awaits XP-60 bank-mapping validation.")
                        wrapMode: Text.WordWrap; role: "caption"; color: Theme.warning
                    }
                }
            }
        }
    }
}
