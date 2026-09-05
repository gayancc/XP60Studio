import QtQuick
import QtQuick.Layouts
import XP60Studio

// Import / export state for the Library screen.
//
// Three states in one card, because they are the same conversation at
// different moments: nothing happening, something running, something finished.
// The card only appears once there is something to say — an idle library
// should not carry a permanently empty progress panel.
//
// The finished state stays until dismissed. Importing a bank can report
// duplicates, partial Patches and rejected messages, and those are exactly the
// things a librarian needs to read at their own pace. A toast would take them
// away mid-sentence.
Rectangle {
    id: root

    required property var transfer
    // Collapsed by default: the headline answers "did it work", the list
    // answers "what happened to each file", and most of the time the first is
    // enough.
    property bool detailsExpanded: false

    readonly property bool running: transfer.busy
    readonly property bool showing: running || transfer.hasResult

    visible: showing
    Layout.fillWidth: true
    implicitHeight: showing ? content.implicitHeight + 2 * Metrics.cardPadding : 0

    radius: Metrics.radiusSm
    color: Theme.surfaceRaised
    border.width: 1
    border.color: {
        if (root.running) return Theme.accent
        switch (root.transfer.resultTone) {
        case "error": return Theme.error
        case "warning": return Theme.warning
        case "success": return Theme.success
        default: return Theme.borderSubtle
        }
    }

    Behavior on implicitHeight {
        enabled: !Motion.reducedMotion
        NumberAnimation { duration: Motion.durationNormal; easing.type: Motion.easingStandard }
    }

    ColumnLayout {
        id: content
        anchors {
            left: parent.left; right: parent.right; top: parent.top
            margins: Metrics.cardPadding
        }
        spacing: Metrics.spacingSm

        // -----------------------------------------------------------------
        // Running
        // -----------------------------------------------------------------
        ColumnLayout {
            visible: root.running
            Layout.fillWidth: true
            spacing: Metrics.spacingXs

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingSm
                XpLabel {
                    objectName: "transferActivity"
                    text: root.transfer.activity
                    role: "body"
                    font.weight: Typography.weightMedium
                }
                Item { Layout.fillWidth: true }
                XpLabel {
                    // "2 of 7 files" is the honest unit: a file either lands or
                    // it does not, so a byte percentage would overstate what is
                    // actually known.
                    visible: root.transfer.filesTotal > 0
                    text: qsTr("%1 of %2 files").arg(root.transfer.filesCompleted).arg(root.transfer.filesTotal)
                    role: "caption"
                    secondary: true
                }
                XpButton {
                    objectName: "cancelImport"
                    text: qsTr("Cancel")
                    variant: "ghost"
                    compact: true
                    onClicked: root.transfer.cancelImport()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 4
                radius: 2
                color: Theme.surfaceSunken
                Rectangle {
                    width: parent.width * root.transfer.progress
                    height: parent.height
                    radius: 2
                    color: Theme.accent
                    Behavior on width {
                        enabled: !Motion.reducedMotion
                        NumberAnimation { duration: Motion.durationNormal; easing.type: Motion.easingStandard }
                    }
                }
            }

            XpLabel {
                visible: root.transfer.currentFile.length > 0
                text: root.transfer.currentFile
                role: "mono"
                secondary: true
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
            XpLabel {
                text: qsTr("Cancelling stops before the next file. Files already imported stay imported.")
                role: "caption"
                muted: true
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        // -----------------------------------------------------------------
        // Finished
        // -----------------------------------------------------------------
        ColumnLayout {
            visible: !root.running && root.transfer.hasResult
            Layout.fillWidth: true
            spacing: Metrics.spacingXs

            RowLayout {
                Layout.fillWidth: true
                spacing: Metrics.spacingSm
                StatusPill {
                    objectName: "transferResultPill"
                    text: root.transfer.resultHeadline
                    tone: root.transfer.resultTone
                    showDot: false
                }
                Item { Layout.fillWidth: true }
                XpButton {
                    objectName: "dismissTransferResult"
                    text: qsTr("Dismiss")
                    variant: "ghost"
                    compact: true
                    onClicked: root.transfer.dismissResult()
                }
            }

            XpLabel {
                objectName: "transferResultDetail"
                text: root.transfer.resultDetail
                role: "body"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            // Duplicates are counted and kept, never dropped. Saying so where
            // the count appears stops it reading as "skipped".
            XpLabel {
                visible: root.transfer.hasDuplicates
                text: qsTr("Patches already in your library were imported anyway. Nothing was discarded — review them under duplicates.")
                role: "caption"
                color: Theme.warning
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            XpButton {
                objectName: "toggleTransferDetails"
                visible: root.transfer.resultFiles.length > 0
                text: root.detailsExpanded
                      ? qsTr("Hide per-file detail")
                      : qsTr("Show per-file detail (%1)").arg(root.transfer.resultFiles.length)
                variant: "ghost"
                compact: true
                onClicked: root.detailsExpanded = !root.detailsExpanded
            }

            ColumnLayout {
                visible: root.detailsExpanded
                Layout.fillWidth: true
                spacing: 2
                Repeater {
                    model: root.transfer.resultFiles
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: Metrics.spacingSm
                        XpIcon {
                            name: modelData.ok ? "check" : "alert"
                            color: modelData.ok ? Theme.success : Theme.error
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            XpLabel {
                                text: modelData.name
                                role: "mono"
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                            XpLabel {
                                text: modelData.description
                                role: "caption"
                                secondary: true
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }
        }
    }
}
