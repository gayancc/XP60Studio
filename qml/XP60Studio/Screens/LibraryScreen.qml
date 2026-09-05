import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Dialogs
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Phase 5 — the Library.
//
// A librarian's question is "which of my sounds is this, and where did it come
// from?", so the row leads with the name, then the visual state a scan can pick
// up (favourite, rating, category, tags) and the provenance that answers "where
// from". Filter chips carry the vocabulary the library actually contains — no
// invented categories — and the count line says how much of the library the
// current filters are hiding.
//
// The list is virtualized: the model fetches a page only when a row is asked
// for, so this stays responsive with thousands of patches.
FocusScope {
    id: root

    required property LibraryListModel library
    // Optional: without it the screen is read-and-organise only, which is what
    // it was before import and export existed and what the screenshot harness
    // still gets.
    property var transfer: null
    readonly property bool wide: width >= 1040
    readonly property var selection: library.selected

    onVisibleChanged: if (visible) search.forceActiveFocus()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.screenPadding
        spacing: Metrics.spacingMd

        // ---------------------------------------------------------------
        // Identity and scope
        // ---------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingSm

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                XpLabel { text: qsTr("LIBRARY"); role: "overline"; color: Theme.accentText }
                XpLabel {
                    text: qsTr("Every patch you have collected")
                    role: "title"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }

            XpButton {
                objectName: "libraryImport"
                visible: root.transfer !== null
                enabled: root.transfer !== null && !root.transfer.busy
                text: qsTr("Import")
                iconName: "import"
                compact: true
                variant: "ghost"
                onClicked: importDialog.open()
            }

            XpButton {
                objectName: "libraryExport"
                visible: root.transfer !== null
                // Exports what the filters currently show. Nothing visible
                // means nothing to write, so the action is off rather than
                // producing an empty file.
                enabled: root.transfer !== null && !root.transfer.busy && root.library.count > 0
                text: root.library.filtered
                      ? qsTr("Export %n shown", "", root.library.count)
                      : qsTr("Export all")
                iconName: "export"
                compact: true
                variant: "ghost"
                onClicked: exportDialog.open()
            }

            StatusPill {
                objectName: "libraryCount"
                // "128 patches" when nothing is filtered; "12 of 128" when it is,
                // so a filter can never quietly hide the rest of the library.
                text: root.library.filtered
                      ? qsTr("%1 of %2").arg(root.library.count).arg(root.library.libraryTotal)
                      : qsTr("%n patch(es)", "", root.library.libraryTotal)
                tone: root.library.filtered ? "info" : "neutral"
                showDot: false
            }
        }

        // ---------------------------------------------------------------
        // Search, sort and the filters in force
        // ---------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingSm

            XpTextField {
                id: search
                objectName: "librarySearch"
                Layout.fillWidth: true
                placeholderText: qsTr("Search patch names")
                text: root.library.searchText
                onTextEdited: root.library.searchText = text
            }

            XpComboBox {
                objectName: "librarySort"
                Layout.preferredWidth: 180
                model: [
                    qsTr("Name A–Z"),
                    qsTr("Name Z–A"),
                    qsTr("Newest first"),
                    qsTr("Oldest first"),
                    qsTr("Highest rated"),
                    qsTr("Source slot")
                ]
                currentIndex: root.library.sortOrder
                onActivated: root.library.sortOrder = currentIndex
            }
        }

        LibraryTransferCard {
            objectName: "libraryTransferCard"
            visible: root.transfer !== null && (root.transfer.busy || root.transfer.hasResult)
            transfer: root.transfer !== null ? root.transfer : null
            Layout.fillWidth: true
        }

        Flow {
            Layout.fillWidth: true
            spacing: Metrics.spacingXs

            XpButton {
                objectName: "libraryFavouritesFilter"
                text: qsTr("Favourites")
                compact: true
                variant: root.library.favouritesOnly ? "primary" : "ghost"
                onClicked: root.library.favouritesOnly = !root.library.favouritesOnly
            }

            XpButton {
                objectName: "libraryRatedFilter"
                // One tap for "the ones I actually rated well"; tapping again
                // clears it, so the chip is its own off switch.
                text: qsTr("4★ and up")
                compact: true
                variant: root.library.minimumRating >= 4 ? "primary" : "ghost"
                onClicked: root.library.minimumRating = root.library.minimumRating >= 4 ? 0 : 4
            }

            Repeater {
                model: root.library.categoriesInUse
                delegate: XpButton {
                    required property string modelData
                    text: modelData
                    compact: true
                    variant: root.library.category === modelData ? "primary" : "ghost"
                    onClicked: root.library.category = root.library.category === modelData ? "" : modelData
                }
            }

            Repeater {
                model: root.library.tagsInUse
                delegate: XpButton {
                    required property string modelData
                    text: "#" + modelData
                    compact: true
                    variant: root.library.tags.indexOf(modelData) >= 0 ? "primary" : "ghost"
                    onClicked: {
                        const current = root.library.tags.slice()
                        const at = current.indexOf(modelData)
                        if (at >= 0)
                            current.splice(at, 1)
                        else
                            current.push(modelData)
                        root.library.tags = current
                    }
                }
            }

            XpButton {
                objectName: "libraryClearFilters"
                text: qsTr("Clear filters")
                compact: true
                variant: "ghost"
                visible: root.library.filtered
                onClicked: root.library.clearFilters()
            }
        }

        // ---------------------------------------------------------------
        // Results, and the inspector for the selected patch
        // ---------------------------------------------------------------
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Narrow windows put the details under the results rather than
            // squeezing both into unreadable columns.
            columns: root.wide ? 2 : 1
            columnSpacing: Metrics.spacingMd
            rowSpacing: Metrics.spacingMd

            XpCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                // Stretch factors rather than a fraction of parent.width: the
                // layout sets this item's width, so reading the parent's width
                // back to compute it makes the two chase each other (Qt Quick
                // Layouts reports a recursive rearrange and gives up).
                Layout.horizontalStretchFactor: 3

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Metrics.spacingSm
                    spacing: Metrics.spacingXs

                    XpEmptyState {
                        objectName: "libraryEmptyState"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.library.count === 0
                        title: root.library.libraryTotal === 0
                               ? qsTr("The library is empty")
                               : qsTr("No patch matches these filters")
                        // Never offer an action the phase does not implement.
                        message: root.library.libraryTotal === 0
                                 ? qsTr("Import a .syx file to start collecting patches.")
                                 : qsTr("Clear a filter to see more of your %n patch(es).", "", root.library.libraryTotal)
                    }

                    ListView {
                        id: results
                        objectName: "libraryResults"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.library.count > 0
                        clip: true
                        model: root.library
                        // The model pages on demand, so the buffer decides how
                        // far ahead it reads.
                        cacheBuffer: 400
                        spacing: 2
                        currentIndex: root.library.selectedRow
                        keyNavigationEnabled: true
                        focus: true
                        QQC.ScrollBar.vertical: XpScrollBar {}

                        delegate: LibraryResultRow {
                            width: ListView.view.width
                            selected: ListView.isCurrentItem
                            onClicked: root.library.selectRow(index)
                            onFavouriteToggled: root.library.setFavourite(index, !favourite)
                            onRatingPicked: function (value) { root.library.setRating(index, value) }
                        }

                        onCurrentIndexChanged: root.library.selectRow(currentIndex)
                    }
                }
            }

            LibraryDetailsPanel {
                objectName: "libraryDetails"
                Layout.fillWidth: true
                Layout.horizontalStretchFactor: 2
                Layout.fillHeight: root.wide
                Layout.preferredHeight: root.wide ? -1 : 260
                library: root.library
            }
        }
    }

    // ------------------------------------------------------------------
    // File choosers
    //
    // The library is the one place that reads and writes the user's own
    // files, so both dialogs name the format plainly and neither picks a
    // destination on the user's behalf.
    // ------------------------------------------------------------------
    FileDialog {
        id: importDialog
        objectName: "libraryImportDialog"
        title: qsTr("Import SysEx banks")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Roland SysEx (*.syx)"), qsTr("All files (*)")]
        onAccepted: if (root.transfer) root.transfer.importFiles(selectedFiles)
    }

    FileDialog {
        id: exportDialog
        objectName: "libraryExportDialog"
        title: qsTr("Export to SysEx")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "syx"
        nameFilters: [qsTr("Roland SysEx (*.syx)")]
        onAccepted: {
            if (!root.transfer)
                return
            // Every patch the current filters match, not just the rows already
            // fetched, so the file holds what the screen says it holds.
            root.transfer.exportIds(root.library.filteredIds(), selectedFile, 0)
        }
    }

}
