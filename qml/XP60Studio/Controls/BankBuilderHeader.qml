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

    XpButton {
        objectName: "bankSaveAs"
        text: qsTr("Save as new bank")
        iconName: "export"
        compact: true
        variant: "primary"
        onClicked: root.saveAsRequested()
    }
}
