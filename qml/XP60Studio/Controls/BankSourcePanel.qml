import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// SOURCE LIBRARY — where the Patches come from.
//
// This is the supply side, and it is kept visibly a different kind of thing
// from the instrument panel beside it: a flat list on a plain surface, with no
// buttons that look like hardware. Nothing here is a destination, and nothing
// on the panel is a source, so "which side am I looking at" is never a
// question the musician has to stop and answer.
//
// A "source bank" is one import: everything that arrived from one .syx file or
// one device read, grouped by the digest its provenance recorded. Any source
// can supply any destination — the piano bank, an old backup and an imported
// XP-50 bank can all feed the same new User bank — so the picker narrows the
// list without ever implying a correspondence between a source and the target.
//
// The list is the shared virtualized LibraryListModel: thousands of Patches
// scroll here at the cost of a few indexed queries, and no Patch is decoded
// until something actually asks for it.
Item {
    id: root

    required property LibraryListModel library
    // Set while this panel is supplying the drag, so the row that was picked
    // up can be dimmed.
    property int draggingRow: -1

    signal patchDragStarted(int row, var patchId, string patchName, real x, real y)
    signal patchDragMoved(real x, real y)
    signal patchDragReleased()
    signal patchDragCancelled()

    ColumnLayout {
        anchors.fill: parent
        spacing: Metrics.spacingSm

        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingSm
            XpLabel { text: qsTr("SOURCE LIBRARY"); role: "overline"; color: Theme.textSecondary }
            Item { Layout.fillWidth: true }
            StatusPill {
                objectName: "bankSourceCount"
                text: root.library.filtered
                      ? qsTr("%1 of %2").arg(root.library.count).arg(root.library.libraryTotal)
                      : qsTr("%n patch(es)", "", root.library.libraryTotal)
                tone: root.library.filtered ? "info" : "neutral"
                showDot: false
            }
        }

        XpLabel {
            Layout.fillWidth: true
            text: qsTr("Drag any Patch onto a destination on the panel. The Patch stays here.")
            role: "caption"
            muted: true
            wrapMode: Text.WordWrap
        }

        // Which source bank is open ---------------------------------------
        XpLabel { text: qsTr("SOURCE BANK"); role: "overline"; muted: true }

        Flow {
            Layout.fillWidth: true
            spacing: Metrics.spacingXs

            XpButton {
                objectName: "bankSourceAll"
                text: qsTr("All sources")
                compact: true
                variant: root.library.sourceDigest === "" ? "primary" : "ghost"
                onClicked: root.library.sourceDigest = ""
            }

            Repeater {
                model: root.library.sourcesInUse
                delegate: XpButton {
                    required property var modelData
                    text: qsTr("%1 · %2").arg(modelData.name).arg(modelData.patchCount)
                    compact: true
                    variant: root.library.sourceDigest === modelData.digest ? "primary" : "ghost"
                    onClicked: root.library.sourceDigest =
                               root.library.sourceDigest === modelData.digest ? "" : modelData.digest
                }
            }
        }

        XpTextField {
            id: search
            objectName: "bankSourceSearch"
            Layout.fillWidth: true
            placeholderText: qsTr("Search patch names")
            text: root.library.searchText
            onTextEdited: root.library.searchText = text
        }

        // The patches themselves ------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Metrics.radiusSm
            color: Theme.surfaceSunken
            border.width: 1
            border.color: Theme.borderSubtle
            clip: true

            XpEmptyState {
                anchors.fill: parent
                visible: root.library.count === 0
                title: root.library.libraryTotal === 0
                       ? qsTr("The library is empty")
                       : qsTr("No Patch matches this source or search")
                message: root.library.libraryTotal === 0
                         ? qsTr("Import a .syx file on the Library screen to start collecting Patches.")
                         : qsTr("Choose another source bank, or clear the search.")
            }

            ListView {
                id: list
                objectName: "bankSourceList"
                anchors.fill: parent
                anchors.margins: 2
                visible: root.library.count > 0
                clip: true
                model: root.library
                cacheBuffer: 400
                spacing: 1
                QQC.ScrollBar.vertical: XpScrollBar {}

                delegate: Rectangle {
                    id: row
                    required property int index
                    required property string name
                    required property var entryId
                    required property string sourceName
                    required property string slotLabel

                    width: ListView.view.width
                    height: 40
                    radius: Metrics.radiusSm
                    color: grab.containsMouse ? Theme.surfaceHover : "transparent"
                    opacity: root.draggingRow === index ? 0.35 : 1

                    Behavior on color {
                        enabled: !Motion.reducedMotion
                        ColorAnimation { duration: Motion.durationFast }
                    }
                    Behavior on opacity {
                        enabled: !Motion.reducedMotion
                        NumberAnimation { duration: Motion.durationFast }
                    }

                    // The grip. A row you can pick up should look like one,
                    // and it appears on hover rather than adding permanent
                    // furniture to every row.
                    Column {
                        anchors { left: parent.left; leftMargin: 6; verticalCenter: parent.verticalCenter }
                        spacing: 3
                        opacity: grab.containsMouse ? 1 : 0.25
                        Repeater {
                            model: 3
                            delegate: Rectangle {
                                width: 8
                                height: 1
                                color: Theme.textMuted
                            }
                        }
                        Behavior on opacity {
                            enabled: !Motion.reducedMotion
                            NumberAnimation { duration: Motion.durationFast }
                        }
                    }

                    ColumnLayout {
                        anchors {
                            left: parent.left; leftMargin: 22
                            right: parent.right; rightMargin: Metrics.spacingSm
                            verticalCenter: parent.verticalCenter
                        }
                        spacing: 0

                        XpLabel {
                            Layout.fillWidth: true
                            text: row.name
                            role: "value"
                            elide: Text.ElideRight
                        }
                        XpLabel {
                            Layout.fillWidth: true
                            text: row.slotLabel.length > 0
                                  ? qsTr("%1 · %2").arg(row.sourceName).arg(row.slotLabel)
                                  : row.sourceName
                            role: "caption"
                            muted: true
                            elide: Text.ElideRight
                        }
                    }

                    QQC.ToolTip.visible: grab.containsMouse && row.name.length > 18
                    QQC.ToolTip.delay: 600
                    QQC.ToolTip.text: row.name

                    MouseArea {
                        id: grab
                        anchors.fill: parent
                        hoverEnabled: true
                        preventStealing: true
                        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor

                        property point origin
                        property bool lifted: false
                        readonly property int threshold: 6

                        onPressed: function (mouse) {
                            origin = Qt.point(mouse.x, mouse.y)
                            lifted = false
                            root.library.selectRow(row.index)
                        }
                        onPositionChanged: function (mouse) {
                            if (!pressed) return
                            if (!lifted) {
                                if (Math.abs(mouse.x - origin.x) < threshold
                                    && Math.abs(mouse.y - origin.y) < threshold)
                                    return
                                lifted = true
                                var start = mapToItem(null, mouse.x, mouse.y)
                                root.patchDragStarted(row.index, row.entryId, row.name, start.x, start.y)
                            }
                            var p = mapToItem(null, mouse.x, mouse.y)
                            root.patchDragMoved(p.x, p.y)
                        }
                        onReleased: {
                            if (lifted) root.patchDragReleased()
                            lifted = false
                        }
                        onCanceled: {
                            if (lifted) root.patchDragCancelled()
                            lifted = false
                        }
                    }
                }
            }
        }
    }
}
