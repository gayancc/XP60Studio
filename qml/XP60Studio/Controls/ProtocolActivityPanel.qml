import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Protocol activity as a diagnostics tool: severity, direction, expandable raw.
XpCard {
    id: root

    required property DevicesViewModel devices
    property bool expanded: false
    property string directionFilter: "All" // All | IN | OUT | SYS

    implicitHeight: column.implicitHeight + 2 * Metrics.cardPadding

    ColumnLayout {
        id: column
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: root.expanded ? parent.bottom : undefined }
        height: root.expanded ? parent.height - 2 * Metrics.cardPadding : undefined
        spacing: Metrics.spacingSm

        XpPanelHeader {
            title: "Protocol activity"
            XpLabel {
                text: root.devices.log.count + " events"
                role: "caption"
                muted: true
            }
            XpButton {
                text: root.expanded ? "Collapse" : "Expand"
                compact: true
                variant: "ghost"
                onClicked: root.expanded = !root.expanded
            }
            XpButton {
                text: "Clear"
                compact: true
                variant: "ghost"
                onClicked: root.devices.clearLog()
            }
        }

        // Compact latest-event strip when collapsed
        Rectangle {
            visible: !root.expanded
            Layout.fillWidth: true
            implicitHeight: 40
            radius: Metrics.radiusSm
            color: Theme.surfaceSunken
            border.width: 1
            border.color: Theme.borderSubtle
            XpLabel {
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: Metrics.spacingMd }
                text: root.devices.log.count === 0
                      ? "No protocol events yet"
                      : "Latest activity available — expand for IN/OUT detail and raw SysEx"
                role: "caption"
                secondary: true
                elide: Text.ElideRight
            }
        }

        RowLayout {
            visible: root.expanded
            Layout.fillWidth: true
            spacing: Metrics.spacingXs
            Repeater {
                model: ["All", "IN", "OUT"]
                delegate: XpButton {
                    text: modelData
                    compact: true
                    variant: root.directionFilter === modelData ? "primary" : "ghost"
                    onClicked: root.directionFilter = modelData
                }
            }
            Item { Layout.fillWidth: true }
            XpLabel {
                text: "Backend " + root.devices.backendName + " · Model " + root.devices.modelIdText
                role: "caption"
                muted: true
            }
        }

        ListView {
            id: logList
            objectName: "protocolLog"
            visible: root.expanded
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitHeight: 320
            clip: true
            model: root.devices.log
            spacing: 2
            QQC.ScrollBar.vertical: XpScrollBar {}
            onCountChanged: Qt.callLater(function() { if (logList) logList.positionViewAtEnd() })

            delegate: Rectangle {
                id: logRow
                required property var model
                property bool rowExpanded: false
                width: ListView.view.width
                visible: root.directionFilter === "All" || model.direction === root.directionFilter
                height: visible ? (rowColumn.implicitHeight + Metrics.spacingSm) : 0
                radius: Metrics.radiusSm
                color: logRowHover.hovered || rowExpanded ? Theme.surfaceHover : "transparent"

                readonly property color directionColor: model.direction === "IN" ? Theme.tone2
                                                      : model.direction === "OUT" ? Theme.tone1 : Theme.textMuted
                readonly property color textColor: model.severity === "Error" ? Theme.error
                                                 : model.severity === "Warning" ? Theme.warning : Theme.textPrimary

                HoverHandler { id: logRowHover }
                TapHandler { onTapped: logRow.rowExpanded = !logRow.rowExpanded }

                ColumnLayout {
                    id: rowColumn
                    anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: Metrics.spacingSm; rightMargin: Metrics.spacingSm; topMargin: Metrics.spacingXs }
                    spacing: 2
                    RowLayout {
                        spacing: Metrics.spacingSm
                        Layout.fillWidth: true
                        XpLabel { text: logRow.model.timeText; role: "mono"; muted: true }
                        Rectangle {
                            implicitWidth: 34
                            implicitHeight: 16
                            radius: 3
                            color: Qt.rgba(logRow.directionColor.r, logRow.directionColor.g, logRow.directionColor.b, 0.18)
                            XpLabel { anchors.centerIn: parent; text: logRow.model.direction; role: "overline"; color: logRow.directionColor }
                        }
                        StatusPill {
                            visible: logRow.model.severity === "Error" || logRow.model.severity === "Warning"
                            text: logRow.model.severity
                            tone: logRow.model.severity === "Error" ? "error" : "warning"
                            showDot: false
                        }
                        XpLabel {
                            text: logRow.model.summary
                            role: "caption"
                            color: logRow.textColor
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        StatusPill {
                            visible: logRow.model.checksum !== "-"
                            text: logRow.model.checksum === "OK" ? "Checksum OK" : "Checksum fail"
                            tone: logRow.model.checksum === "OK" ? "success" : "error"
                            showDot: false
                        }
                    }
                    XpLabel {
                        visible: logRow.rowExpanded && logRow.model.detail.length > 0
                        text: logRow.model.detail
                        role: "caption"
                        secondary: true
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    XpLabel {
                        visible: logRow.rowExpanded && logRow.model.rawHex.length > 0
                        text: logRow.model.rawHex
                        role: "mono"
                        muted: true
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }
}
