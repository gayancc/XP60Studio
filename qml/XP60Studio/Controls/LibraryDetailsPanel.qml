import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// The inspector for the selected library patch.
//
// It answers the two questions the list cannot: where exactly did this come
// from, and are there other copies of it. Provenance is shown as recorded —
// nothing is inferred, and a patch with no User bank slot shows the address it
// actually lived at rather than a slot it never had.
XpCard {
    id: root

    required property LibraryListModel library
    readonly property var entry: library.selected
    readonly property bool hasSelection: Object.keys(entry).length > 0

    XpEmptyState {
        anchors.fill: parent
        visible: !root.hasSelection
        title: qsTr("No patch selected")
        message: qsTr("Choose a patch to see where it came from.")
    }

    QQC.ScrollView {
        anchors.fill: parent
        visible: root.hasSelection
        clip: true
        QQC.ScrollBar.vertical: XpScrollBar {}

        ColumnLayout {
            width: root.width - 2 * Metrics.spacingSm
            spacing: Metrics.spacingSm

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                XpLabel { text: qsTr("SELECTED PATCH"); role: "overline"; color: Theme.accentText }
                XpLabel {
                    objectName: "libraryDetailsName"
                    text: root.entry.name ?? ""
                    role: "heading"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }

            XpDivider { Layout.fillWidth: true }

            // ---------------------------------------------------------------
            // Provenance, exactly as recorded at import
            // ---------------------------------------------------------------
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Metrics.spacingSm
                rowSpacing: Metrics.spacingXs

                XpLabel { text: qsTr("Origin"); role: "caption"; secondary: true }
                XpLabel { text: root.entry.originLabel ?? ""; Layout.fillWidth: true; elide: Text.ElideRight }

                XpLabel { text: qsTr("Source"); role: "caption"; secondary: true }
                XpLabel {
                    objectName: "libraryDetailsSource"
                    text: root.entry.sourceName ?? ""
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                }

                XpLabel { text: qsTr("Slot"); role: "caption"; secondary: true }
                XpLabel { text: root.entry.slotLabel ?? ""; Layout.fillWidth: true }

                XpLabel { text: qsTr("Preserved bytes"); role: "caption"; secondary: true }
                XpLabel {
                    // The original SysEx is kept verbatim; saying how much of
                    // it there is makes that concrete rather than a claim.
                    text: qsTr("%1 bytes of original SysEx").arg(root.entry.sysExBytes ?? 0)
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }

            XpDivider { Layout.fillWidth: true }

            // ---------------------------------------------------------------
            // The user's own metadata
            // ---------------------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingSm

                XpButton {
                    objectName: "libraryDetailsFavourite"
                    text: root.entry.favourite ? qsTr("Favourite") : qsTr("Add to favourites")
                    compact: true
                    variant: root.entry.favourite ? "primary" : "ghost"
                    onClicked: root.library.setFavourite(root.library.selectedRow, !root.entry.favourite)
                }

                Item { Layout.fillWidth: true }

                Repeater {
                    model: 5
                    delegate: XpIcon {
                        required property int index
                        name: (root.entry.rating ?? 0) > index ? "star-filled" : "star"
                        color: (root.entry.rating ?? 0) > index ? Theme.accentText : Theme.textMuted
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Rate %n star(s)", "", index + 1)
                        TapHandler {
                            onTapped: root.library.setRating(root.library.selectedRow,
                                                             (root.entry.rating ?? 0) === index + 1 ? 0 : index + 1)
                        }
                    }
                }
            }

            XpTextField {
                objectName: "libraryDetailsCategory"
                Layout.fillWidth: true
                placeholderText: qsTr("Your category, e.g. Pad")
                text: root.entry.category ?? ""
                onEditingFinished: root.library.setCategoryOf(root.library.selectedRow, text)
            }

            Flow {
                Layout.fillWidth: true
                spacing: Metrics.spacingXs
                Repeater {
                    model: root.entry.tags ?? []
                    delegate: XpButton {
                        required property string modelData
                        text: "#" + modelData + "  ×"
                        compact: true
                        variant: "ghost"
                        onClicked: root.library.removeTag(root.library.selectedRow, modelData)
                    }
                }
            }

            XpTextField {
                objectName: "libraryDetailsAddTag"
                Layout.fillWidth: true
                placeholderText: qsTr("Add a tag and press Enter")
                onAccepted: {
                    if (root.library.addTag(root.library.selectedRow, text))
                        text = ""
                }
            }

            XpDivider { Layout.fillWidth: true }

            // ---------------------------------------------------------------
            // Other copies of the same sound
            // ---------------------------------------------------------------
            ColumnLayout {
                id: duplicates
                Layout.fillWidth: true
                spacing: Metrics.spacingXs

                property var matches: []
                function refresh() {
                    matches = root.hasSelection ? root.library.duplicatesOf(root.library.selectedRow) : []
                }

                Connections {
                    target: root.library
                    function onSelectionChanged() { duplicates.refresh() }
                    function onFilterChanged() { duplicates.refresh() }
                }
                Component.onCompleted: refresh()

                XpLabel {
                    objectName: "libraryDuplicateSummary"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    role: "caption"
                    secondary: duplicates.matches.length === 0
                    // A fingerprint match is a candidate confirmed against the
                    // parameters themselves, so this can be stated plainly —
                    // but nothing is merged or removed on its own.
                    text: duplicates.matches.length === 0
                          ? qsTr("No other copy of this patch in the library.")
                          : qsTr("%n other patch(es) hold identical parameters.", "", duplicates.matches.length)
                }

                Repeater {
                    model: duplicates.matches
                    delegate: XpLabel {
                        required property var modelData
                        Layout.fillWidth: true
                        role: "caption"
                        secondary: true
                        elide: Text.ElideRight
                        text: modelData.slotLabel + " · " + modelData.sourceName
                    }
                }
            }
        }
    }
}
