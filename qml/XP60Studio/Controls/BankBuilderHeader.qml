import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio

// The Bank Builder's identity line and its bank-level actions.
//
// Everything here is about the bank *as a whole* — what it is called, whether
// it has unsaved changes, walking the history, and starting, opening or
// storing one. Nothing here touches a destination: that is the panel's job,
// and keeping the two apart is what stops the screen turning into one long
// toolbar of unrelated verbs.
//
// The state pills are exclusive by construction: a bank is MODIFIED, or SAVED,
// or neither (a fresh unsaved one). MISSING is additive, because a bank can be
// saved *and* reference Patches that have since been deleted.
RowLayout {
    id: root

    required property var builder
    property bool savedBanksOpen: false
    // Optional: without it the screen arranges banks but cannot write one to a
    // file, which is what the screenshot harness gets.
    property var transfer: null

    signal savedBanksToggled()
    signal newBankRequested()
    signal saveAsRequested()
    signal exportRequested()
    signal writeToUserRequested()
    signal fetchBankRequested()

    spacing: Metrics.spacingSm

    ColumnLayout {
        spacing: 2
        XpLabel { text: qsTr("BANK BUILDER"); role: "overline"; color: Theme.accentText }

        RowLayout {
            spacing: Metrics.spacingSm

            XpTextField {
                objectName: "bankName"
                Layout.preferredWidth: 240
                text: root.builder.bankName
                placeholderText: qsTr("Name this bank")
                onEditingFinished: root.builder.bankName = text
            }
            StatusPill {
                objectName: "bankModified"
                visible: root.builder.modified
                text: qsTr("MODIFIED")
                tone: "warning"
            }
            StatusPill {
                objectName: "bankSaved"
                visible: !root.builder.modified && root.builder.savedBefore
                text: qsTr("SAVED")
                tone: "success"
            }
            // Reported, never acted on. A bank holding one sound twice may be
            // exactly what was meant.
            StatusPill {
                objectName: "bankDuplicates"
                visible: root.builder.duplicateCount > 0
                text: qsTr("%n duplicate(s)", "", root.builder.duplicateCount)
                tone: "info"
            }
            // What this bank would want from the instrument's expansion slots.
            // Reported wherever the bank is, and read before a write: "these
            // will have nothing to sound" is worth knowing before 128
            // destinations are committed to USER memory.
            StatusPill {
                objectName: "bankNeedsBoards"
                visible: root.builder.expansionSummary.needsBoard > 0
                text: root.builder.expansionSummary.undecided
                      ? qsTr("%n may need a board", "", root.builder.expansionSummary.needsBoard)
                      : qsTr("%n need a board", "", root.builder.expansionSummary.needsBoard)
                tone: root.builder.expansionSummary.undecided ? "warning" : "error"
                HoverHandler { id: needsBoardHover }
                QQC.ToolTip.visible: needsBoardHover.hovered
                QQC.ToolTip.delay: 400
                QQC.ToolTip.text: root.builder.expansionSummary.summary
            }
            StatusPill {
                objectName: "bankMissing"
                visible: root.builder.missingCount > 0
                text: qsTr("%n missing patch(es)", "", root.builder.missingCount)
                tone: "error"
            }
        }
    }

    Item { Layout.fillWidth: true }

    // History. The tooltip names the edit, so "Undo" is never a guess.
    XpButton {
        objectName: "bankUndo"
        iconName: "undo"
        iconOnly: true
        compact: true
        variant: "ghost"
        enabled: root.builder.canUndo
        QQC.ToolTip.visible: hovered && root.builder.canUndo
        QQC.ToolTip.delay: 400
        QQC.ToolTip.text: qsTr("Undo %1").arg(root.builder.undoLabel)
        onClicked: root.builder.undo()
    }
    XpButton {
        objectName: "bankRedo"
        iconName: "redo"
        iconOnly: true
        compact: true
        variant: "ghost"
        enabled: root.builder.canRedo
        QQC.ToolTip.visible: hovered && root.builder.canRedo
        QQC.ToolTip.delay: 400
        QQC.ToolTip.text: qsTr("Redo %1").arg(root.builder.redoLabel)
        onClicked: root.builder.redo()
    }

    XpButton {
        objectName: "bankOpen"
        text: qsTr("Saved banks")
        iconName: "library"
        compact: true
        variant: root.savedBanksOpen ? "primary" : "ghost"
        onClicked: root.savedBanksToggled()
    }
    XpButton {
        objectName: "bankNew"
        text: qsTr("New empty bank")
        iconName: "plus"
        compact: true
        variant: "ghost"
        onClicked: root.newBankRequested()
    }
    XpButton {
        objectName: "bankSave"
        text: qsTr("Save")
        compact: true
        variant: "ghost"
        // Only meaningful once there is a stored bank to write over.
        visible: root.builder.savedBefore
        enabled: root.builder.modified
        onClicked: root.builder.saveBank()
    }
    // Writes the arrangement to a `.syx`, each Patch addressed to the
    // destination it occupies. An empty bank has nothing to write, so the
    // action is off rather than producing a file with no Patches in it.
    XpButton {
        objectName: "bankExport"
        text: qsTr("Export bank")
        iconName: "export"
        compact: true
        variant: "ghost"
        visible: root.transfer !== null
        enabled: root.transfer !== null && !root.transfer.busy && root.builder.occupiedCount > 0
        QQC.ToolTip.visible: hovered
        QQC.ToolTip.delay: 400
        QQC.ToolTip.text: root.builder.occupiedCount > 0
                          ? qsTr("Write %n destination(s) to a .syx file, addressed to the User slots they occupy here", "", root.builder.occupiedCount)
                          : qsTr("This bank has no Patches in it yet")
        onClicked: root.exportRequested()
    }

    // Read-only, and the thing to do before the destructive one beside it:
    // take what is on the keyboard into the library first.
    XpButton {
        objectName: "bankFetchFromDevice"
        text: qsTr("Read XP-60 bank")
        iconName: "midi-in"
        compact: true
        variant: "ghost"
        visible: root.builder.canFetchBank || root.builder.bankFetchBusy
        enabled: root.builder.canFetchBank
        QQC.ToolTip.visible: hovered
        QQC.ToolTip.delay: 400
        QQC.ToolTip.text: qsTr("Reads all 128 Patches out of the XP-60's USER memory into the library and arranges them here at the slots they came from. Read-only: nothing is written to the instrument. It takes a couple of minutes.")
        onClicked: root.fetchBankRequested()
    }

    // The destructive one. Deliberately last, deliberately two presses (arm,
    // then write), and deliberately named for the memory it overwrites: the
    // Owner's Manual reserves "write" for exactly this operation, and the
    // XP-60's own front panel makes it a separate, destination-chosen,
    // confirmed act (p.46). This mirrors that.
    XpButton {
        objectName: "bankArmUserWrite"
        text: root.builder.userWriteArmed ? qsTr("Armed") : qsTr("Arm")
        compact: true
        variant: root.builder.userWriteArmed ? "danger" : "ghost"
        visible: root.builder.userWriteTotal > 0 || root.builder.canArmUserWrite || root.builder.userWriteArmed
        enabled: root.builder.userWriteArmed || root.builder.canArmUserWrite
        QQC.ToolTip.visible: hovered
        QQC.ToolTip.delay: 400
        QQC.ToolTip.text: qsTr("Arms the write to the XP-60's permanent USER memory. One arming permits one write.")
        onClicked: root.builder.userWriteArmed ? root.builder.disarmUserWrite() : root.builder.armUserWrite()
    }
    XpButton {
        objectName: "bankWriteToUser"
        text: qsTr("Write to XP-60 USER")
        iconName: "midi-out"
        compact: true
        variant: "danger"
        visible: root.builder.userWriteArmed || root.builder.userWriteBusy
        enabled: root.builder.canWriteToUserMemory
        QQC.ToolTip.visible: hovered
        QQC.ToolTip.delay: 400
        QQC.ToolTip.text: root.builder.userWritePlan
        onClicked: root.writeToUserRequested()
    }

    XpButton {
        objectName: "bankSaveAs"
        text: qsTr("Save as new bank")
        iconName: "export"
        compact: true
        variant: "primary"
        onClicked: root.saveAsRequested()
    }
}
