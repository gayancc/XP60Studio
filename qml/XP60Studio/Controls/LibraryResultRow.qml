import QtQuick
import QtQuick.Layouts
import XP60Studio

// One patch in the Library list.
//
// Scanning a library is a visual task, so the row is built to be read at a
// glance: the name leads, the favourite and rating are direct-manipulation
// controls rather than fields, and the provenance line answers "where did this
// come from" without the reader opening anything. Everything here is metadata —
// no Patch parameter is touched from this row.
Rectangle {
    id: root

    required property int index
    required property string name
    required property string slotLabel
    required property string sourceName
    required property bool favourite
    required property int rating
    required property string category
    required property var tags

    property bool selected: false

    signal clicked()
    signal favouriteToggled()
    signal ratingPicked(int value)

    height: 52
    radius: Metrics.radiusSm
    color: selected ? Theme.selection : (hover.hovered ? Theme.surfaceHover : "transparent")
    border.width: selected ? 1 : 0
    border.color: Theme.focusRing

    Accessible.role: Accessible.ListItem
    Accessible.name: root.name + ", " + root.slotLabel + ", " + root.sourceName
    Accessible.selected: selected

    HoverHandler { id: hover }
    TapHandler { onTapped: root.clicked() }

    RowLayout {
        anchors { fill: parent; leftMargin: Metrics.spacingSm; rightMargin: Metrics.spacingSm }
        spacing: Metrics.spacingSm

        // Favourite is one tap, and reads as on/off without a label.
        XpIcon {
            objectName: "libraryRowFavourite"
            name: root.favourite ? "star-filled" : "star"
            color: root.favourite ? Theme.accentText : Theme.textMuted
            opacity: root.favourite || hover.hovered ? 1 : 0.45
            Accessible.role: Accessible.CheckBox
            Accessible.name: qsTr("Favourite")
            Accessible.checked: root.favourite
            TapHandler { onTapped: root.favouriteToggled() }
            Behavior on opacity { NumberAnimation { duration: Motion.durationFast } }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1

            XpLabel {
                text: root.name
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.weight: root.selected ? Typography.weightMedium : Typography.weightRegular
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingXs
                // Where it came from, in the order a librarian asks: the slot
                // it occupied, then the file or device it arrived in.
                XpLabel { text: root.slotLabel; role: "caption"; secondary: true }
                XpLabel {
                    text: root.sourceName
                    role: "caption"
                    secondary: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
        }

        // The user's own classification, shown only when they set it.
        StatusPill {
            visible: root.category.length > 0
            text: root.category
            tone: "neutral"
            showDot: false
        }

        XpLabel {
            visible: root.tags.length > 0
            text: root.tags.length === 1 ? "#" + root.tags[0] : qsTr("%n tags", "", root.tags.length)
            role: "caption"
            secondary: true
        }

        // Five stars, each directly clickable. Clicking the current rating
        // clears it, so a mis-tap is undone with the same gesture.
        RowLayout {
            objectName: "libraryRowRating"
            spacing: 1
            Repeater {
                model: 5
                delegate: XpIcon {
                    required property int index
                    name: root.rating > index ? "star-filled" : "star"
                    color: root.rating > index ? Theme.accentText : Theme.textMuted
                    opacity: root.rating > index ? 1 : (hover.hovered ? 0.5 : 0.2)
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Rate %n star(s)", "", index + 1)
                    TapHandler {
                        onTapped: root.ratingPicked(root.rating === index + 1 ? 0 : index + 1)
                    }
                    Behavior on opacity { NumberAnimation { duration: Motion.durationFast } }
                }
            }
        }
    }
}
