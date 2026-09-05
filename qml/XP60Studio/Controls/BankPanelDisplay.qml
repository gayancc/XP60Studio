import QtQuick
import QtQuick.Layouts
import XP60Studio

// The instrument's information display.
//
// A workstation tells you where you are before it tells you anything else, and
// it does it on a recessed panel that is visibly a different kind of surface
// from the buttons around it. So: a sunken well, a hairline inner edge, mono
// type for the identities, and the same three lines the hardware would show —
// the group, the location, and the name of what is there.
//
// The location is written the way the panel is operated (A · BANK 3 · 5) with
// the linear number beside it in a quieter weight. The physical identity is
// primary; 001-128 is supporting information, never the headline.
Rectangle {
    id: root

    required property var builder
    // Overridden during a drag so the display previews the drop rather than
    // continuing to describe the selection the pointer has already left.
    property var preview: null

    readonly property bool previewing: preview !== null && preview !== undefined
                                       && preview.panelLabel !== undefined
    readonly property string shownPanel: previewing ? preview.panelLabel : builder.panelLabel
    readonly property string shownLinear: previewing ? preview.linearLabel : builder.linearLabel
    readonly property string shownName: previewing
                                        ? (preview.patchName || "")
                                        : builder.currentPatchName
    readonly property string shownState: previewing ? preview.action : builder.currentState

    implicitHeight: content.implicitHeight + 2 * Metrics.spacingMd
    radius: Metrics.radiusMd
    color: Theme.dataBackground
    border.width: 1
    border.color: previewing ? Theme.accent : Theme.dataBorder

    Behavior on border.color {
        enabled: !Motion.reducedMotion
        ColorAnimation { duration: Motion.durationFast }
    }

    // The inner bevel that makes the panel read as inset rather than as
    // another card.
    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Metrics.radiusMd - 1
        color: "transparent"
        border.width: 1
        border.color: Qt.rgba(0, 0, 0, 0.5)
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Metrics.spacingMd
        spacing: Metrics.spacingXs

        // Group and state -----------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingSm

            XpLabel {
                text: qsTr("USER")
                role: "overline"
                color: Theme.accentText
            }
            XpLabel {
                text: qsTr("TARGET XP BANK")
                role: "overline"
                muted: true
            }
            Item { Layout.fillWidth: true }

            StatusPill {
                objectName: "bankDisplayState"
                text: root.shownState
                tone: {
                    switch (root.shownState) {
                    case "ASSIGNED": return "success"
                    case "PLACE": return "success"
                    case "MOVE": return "success"
                    case "REPLACE": return "warning"
                    case "SWAP": return "info"
                    case "MISSING": return "error"
                    }
                    return "neutral"
                }
                showDot: root.shownState !== "EMPTY"
            }
        }

        // Location ------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: Metrics.spacingMd

            // The panel identity, at display size. This is the line a player
            // reads across the room.
            XpLabel {
                objectName: "bankDisplayPanelLabel"
                text: root.shownPanel
                role: "mono"
                font.pixelSize: 34
                font.weight: Typography.weightBold
                color: root.previewing ? Theme.accentText : Theme.textPrimary
                Layout.alignment: Qt.AlignBottom
            }

            ColumnLayout {
                Layout.alignment: Qt.AlignBottom
                Layout.bottomMargin: 3
                spacing: 0

                XpLabel {
                    objectName: "bankDisplaySpokenLabel"
                    text: root.previewing
                          ? qsTr("SUBGROUP %1 · BANK %2 · NUMBER %3")
                            .arg(root.shownPanel.charAt(0))
                            .arg(root.shownPanel.charAt(1))
                            .arg(root.shownPanel.charAt(2))
                          : root.builder.spokenLabel
                    role: "caption"
                    color: Theme.dataText
                }
                XpLabel {
                    objectName: "bankDisplayLinearLabel"
                    text: qsTr("PATCH %1").arg(root.shownLinear)
                    role: "mono"
                    color: Theme.dataDim
                }
            }

            Item { Layout.fillWidth: true }

            // Occupancy, as the bank's own vital sign.
            ColumnLayout {
                Layout.alignment: Qt.AlignBottom
                spacing: 0
                XpLabel {
                    text: qsTr("%1 / %2").arg(root.builder.occupiedCount).arg(root.builder.slotCount)
                    role: "mono"
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignRight
                    Layout.fillWidth: true
                    color: Theme.textSecondary
                }
                XpLabel {
                    text: qsTr("FILLED")
                    role: "overline"
                    muted: true
                    horizontalAlignment: Text.AlignRight
                    Layout.fillWidth: true
                }
            }
        }

        // Name and provenance -------------------------------------------
        XpLabel {
            objectName: "bankDisplayPatchName"
            Layout.fillWidth: true
            text: root.shownName.length > 0
                  ? root.shownName
                  : (root.previewing ? qsTr("—") : qsTr("Empty destination"))
            role: "title"
            font.pixelSize: 20
            color: root.shownName.length > 0 ? Theme.textPrimary : Theme.textDisabled
            elide: Text.ElideRight
        }

        XpLabel {
            Layout.fillWidth: true
            visible: text.length > 0
            text: {
                if (root.previewing) {
                    if (root.shownState === "REPLACE")
                        return qsTr("Replaces %1").arg(root.preview.occupant || qsTr("what is there"))
                    if (root.shownState === "SWAP")
                        return qsTr("Swaps with %1 at %2").arg(root.preview.occupant || "").arg(root.shownPanel)
                    if (root.shownState === "MOVE")
                        return qsTr("Moves from %1").arg(root.preview.from || "")
                    return qsTr("Drop to place it here")
                }
                if (root.builder.currentState === "MISSING")
                    return qsTr("This Patch is no longer in the library. Nothing was deleted from the bank.")
                if (!root.builder.currentOccupied)
                    return qsTr("Drag a Patch from the source library onto this destination.")
                var from = root.builder.currentSourceName
                var slot = root.builder.currentSourceSlot
                return slot.length > 0 ? qsTr("from %1 · %2").arg(from).arg(slot) : qsTr("from %1").arg(from)
            }
            role: "caption"
            color: root.previewing ? Theme.accentText : Theme.textMuted
            elide: Text.ElideRight
        }
    }
}
