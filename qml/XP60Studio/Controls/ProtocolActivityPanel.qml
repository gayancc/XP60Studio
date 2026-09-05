import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// The MIDI monitor.
//
// This used to be a card whose collapsed state was a bordered rectangle
// containing one sentence — "Latest activity available — expand for IN/OUT
// detail and raw SysEx" — which told the user nothing and looked like an
// unfinished placeholder. It is a monitor now: a fixed-column log, always
// showing its most recent traffic, with the raw bytes one click away.
//
// Columns are fixed-width and monospace so a byte count in one row lines up
// with the byte count in the next; that is the whole point of a monitor, and
// it is what a wrapping caption cannot do. IN and OUT differ by icon as well
// as colour.
XpCard {
    id: root

    required property DevicesViewModel devices
    // "All" | "IN" | "OUT"
    property string directionFilter: "All"

    // Fixed columns, shared by the header row and every log row.
    readonly property int timeColumn: 64
    readonly property int directionColumn: 52
    readonly property int checksumColumn: 88
    // Every row ends with a disclosure chevron. The header has to reserve the
    // same width or its last column label sits 26 px right of its own values.
    readonly property int disclosureColumn: Metrics.iconSizeSm

    implicitHeight: Math.max(220, column.implicitHeight + 2 * Metrics.cardPadding)

    ColumnLayout {
        id: column
        anchors { fill: parent; margins: Metrics.cardPadding }
        spacing: Metrics.spacingSm

        XpPanelHeader {
            title: qsTr("Protocol activity")
            iconName: "activity"
            XpLabel {
                text: root.devices.log.count === 1 ? qsTr("1 event")
                                                   : qsTr("%1 events").arg(root.devices.log.count)
                role: "caption"
                muted: true
            }
            XpButton {
                text: qsTr("Clear")
                compact: true
                variant: "ghost"
                enabled: root.devices.log.count > 0
                onClicked: root.devices.clearLog()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingXs

            // A segmented filter, not three separate buttons with gaps.
            XpSegmentedControl {
                model: [qsTr("All"), qsTr("In"), qsTr("Out")]
                uppercase: false
                currentIndex: root.directionFilter === "IN" ? 1 : root.directionFilter === "OUT" ? 2 : 0
                compact: true
                onActivated: function(index) {
                    root.directionFilter = index === 1 ? "IN" : index === 2 ? "OUT" : "All"
                }
            }

            Item { Layout.fillWidth: true }

            XpLabel {
                text: qsTr("%1 · model %2").arg(root.devices.backendName).arg(root.devices.modelIdText)
                role: "caption"
                muted: true
                elide: Text.ElideRight
            }
        }

        // Column headers. Uppercase and small, the one place table headers
        // belong in the type ramp.
        RowLayout {
            visible: root.devices.log.count > 0
            Layout.fillWidth: true
            Layout.leftMargin: Metrics.spacingSm
            Layout.rightMargin: Metrics.spacingSm
            spacing: Metrics.spacingSm
            XpLabel { text: qsTr("Time"); role: "overline"; muted: true; Layout.preferredWidth: root.timeColumn }
            XpLabel { text: qsTr("Dir"); role: "overline"; muted: true; Layout.preferredWidth: root.directionColumn }
            XpLabel { text: qsTr("Message"); role: "overline"; muted: true; Layout.fillWidth: true }
            XpLabel { text: qsTr("Checksum"); role: "overline"; muted: true; Layout.preferredWidth: root.checksumColumn; horizontalAlignment: Text.AlignRight }
            Item { Layout.preferredWidth: root.disclosureColumn }
        }

        XpDivider { visible: root.devices.log.count > 0; Layout.fillWidth: true; color: Theme.borderSubtle }

        XpEmptyState {
            visible: root.devices.log.count === 0
            iconName: "activity"
            title: qsTr("No MIDI traffic yet")
            message: qsTr("Every message to and from the XP-60 is listed here with its direction, checksum result and raw SysEx.")
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        ListView {
            id: logList
            objectName: "protocolLog"
            visible: root.devices.log.count > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 120
            Layout.topMargin: Metrics.spacingXs
            clip: true
            model: root.devices.log
            spacing: 1
            QQC.ScrollBar.vertical: XpScrollBar {}
            // Follow the tail, which is what a monitor is for, but only while
            // the user is already at the tail — otherwise reading history is
            // interrupted every time a message arrives.
            property bool atTail: true
            onMovementEnded: atTail = atYEnd
            onCountChanged: Qt.callLater(function() {
                if (logList && logList.atTail)
                    logList.positionViewAtEnd()
            })

            delegate: Rectangle {
                id: logRow
                required property var model
                property bool rowExpanded: false

                width: ListView.view.width
                visible: root.directionFilter === "All" || model.direction === root.directionFilter
                height: visible ? rowColumn.implicitHeight + 2 * Metrics.spacingXs : 0
                color: logRowHover.hovered || rowExpanded ? Theme.surfaceHover : "transparent"

                readonly property color directionColor: model.direction === "IN" ? Theme.midiIn
                                                      : model.direction === "OUT" ? Theme.midiOut
                                                      : Theme.textMuted
                readonly property color textColor: model.severity === "Error" ? Theme.error
                                                 : model.severity === "Warning" ? Theme.warning
                                                 : Theme.textPrimary

                Accessible.role: Accessible.Button
                Accessible.name: model.direction + " " + model.summary

                HoverHandler { id: logRowHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: logRow.rowExpanded = !logRow.rowExpanded }

                ColumnLayout {
                    id: rowColumn
                    anchors {
                        left: parent.left; right: parent.right; top: parent.top
                        leftMargin: Metrics.spacingSm
                        rightMargin: Metrics.spacingSm
                        topMargin: Metrics.spacingXs
                    }
                    spacing: Metrics.spacingXs

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Metrics.spacingSm

                        XpLabel {
                            text: logRow.model.timeText
                            role: "mono"
                            color: Theme.dataDim
                            Layout.preferredWidth: root.timeColumn
                        }

                        // Direction by icon and word, never colour alone.
                        RowLayout {
                            Layout.preferredWidth: root.directionColumn
                            spacing: Metrics.spacingXs
                            XpIcon {
                                name: logRow.model.direction === "IN" ? "chevron-down" : "chevron-up"
                                color: logRow.directionColor
                                implicitWidth: Metrics.iconSizeSm
                                implicitHeight: Metrics.iconSizeSm
                            }
                            XpLabel {
                                text: logRow.model.direction === "IN" ? qsTr("In") : qsTr("Out")
                                role: "label"
                                color: logRow.directionColor
                            }
                        }

                        XpIcon {
                            visible: logRow.model.severity === "Error" || logRow.model.severity === "Warning"
                            name: "alert"
                            color: logRow.textColor
                            implicitWidth: Metrics.iconSizeSm
                            implicitHeight: Metrics.iconSizeSm
                        }

                        XpLabel {
                            text: logRow.model.summary
                            role: "caption"
                            color: logRow.textColor
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        XpLabel {
                            text: logRow.model.checksum === "-" ? ""
                                : logRow.model.checksum === "OK" ? qsTr("OK") : qsTr("Failed")
                            role: "mono"
                            color: logRow.model.checksum === "OK" ? Theme.success
                                 : logRow.model.checksum === "-" ? Theme.dataDim : Theme.error
                            horizontalAlignment: Text.AlignRight
                            Layout.preferredWidth: root.checksumColumn
                        }

                        XpIcon {
                            name: logRow.rowExpanded ? "chevron-up" : "chevron-down"
                            color: Theme.textMuted
                            implicitWidth: Metrics.iconSizeSm
                            implicitHeight: Metrics.iconSizeSm
                        }
                    }

                    XpLabel {
                        visible: logRow.rowExpanded && logRow.model.detail.length > 0
                        text: logRow.model.detail
                        role: "caption"
                        secondary: true
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        Layout.leftMargin: root.timeColumn + Metrics.spacingSm
                    }

                    // The raw bytes, on the technical-data surface, grouped and
                    // copyable.
                    XpByteView {
                        visible: logRow.rowExpanded && logRow.model.rawHex.length > 0
                        bytes: logRow.model.rawHex
                        direction: logRow.model.direction === "IN" ? "in" : "out"
                        timestamp: logRow.model.timeText
                        label: logRow.model.kind
                        Layout.fillWidth: true
                        Layout.leftMargin: root.timeColumn + Metrics.spacingSm
                        Layout.bottomMargin: Metrics.spacingXs
                    }
                }
            }
        }
    }
}
