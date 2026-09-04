import QtQuick
import QtQuick.Layouts
import XP60Studio

// Semantic operation progress row; hex/payload details expand on demand.
Rectangle {
    id: root

    required property var operation
    property bool detailsExpanded: false

    radius: Metrics.radiusSm
    color: Theme.surfaceRaised
    border.width: 1
    border.color: Theme.borderSubtle
    implicitHeight: col.implicitHeight + 2 * Metrics.spacingMd

    readonly property string semanticState: {
        var s = String(operation.stateLabel || operation.stateName || "")
        var lower = s.toLowerCase()
        if (lower.indexOf("cancel") >= 0) return "Cancelled"
        if (lower.indexOf("timeout") >= 0 || lower.indexOf("timed") >= 0) return "Timed Out"
        if (lower.indexOf("fail") >= 0 || lower.indexOf("error") >= 0 || lower.indexOf("invalid") >= 0)
            return "Failed Validation"
        if (operation.isSuccess) return "Completed"
        if (!operation.isTerminal && operation.receivedBytes > 0) return "Receiving"
        if (!operation.isTerminal) return "Awaiting Data"
        return s.length ? s : "Request Sent"
    }

    ColumnLayout {
        id: col
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: Metrics.spacingMd }
        spacing: Metrics.spacingXs

        RowLayout {
            spacing: Metrics.spacingSm
            Layout.fillWidth: true
            XpLabel {
                text: root.semanticState
                role: "body"
                font.weight: Typography.weightMedium
                Layout.fillWidth: true
            }
            StatusPill {
                text: root.operation.stateLabel
                tone: root.operation.isSuccess ? "success" : (root.operation.isTerminal ? "error" : "warning")
                pulsing: !root.operation.isTerminal
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 4
            radius: 2
            color: Theme.surfaceSunken
            Rectangle {
                width: parent.width * root.operation.progress
                height: parent.height
                radius: 2
                color: root.operation.isSuccess ? Theme.success : (root.operation.isTerminal ? Theme.error : Theme.accent)
                Behavior on width {
                    enabled: !Motion.reducedMotion
                    NumberAnimation { duration: Motion.durationNormal; easing.type: Motion.easingStandard }
                }
            }
        }

        XpLabel {
            text: root.operation.receivedBytes + " / " + root.operation.expectedBytes + " bytes received"
            role: "caption"
            secondary: true
        }

        XpLabel {
            visible: root.operation.failureReason.length > 0
            text: root.operation.failureReason
            role: "caption"
            color: Theme.error
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        XpButton {
            text: root.detailsExpanded ? "Hide details" : "Show details"
            variant: "ghost"
            compact: true
            onClicked: root.detailsExpanded = !root.detailsExpanded
        }

        ColumnLayout {
            visible: root.detailsExpanded
            Layout.fillWidth: true
            spacing: 2
            XpLabel {
                text: "#" + root.operation.requestId + "  " + root.operation.addressHex + " · " + root.operation.sizeHex
                role: "mono"
                secondary: true
                Layout.fillWidth: true
            }
            XpLabel {
                visible: root.operation.chunkCount > 0
                text: root.operation.chunkCount + " chunk(s)"
                role: "caption"
                muted: true
            }
            XpLabel {
                visible: root.operation.dataHex.length > 0
                text: root.operation.dataHex
                role: "mono"
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 4
                Layout.fillWidth: true
            }
            XpLabel {
                visible: root.operation.isSuccess && root.operation.dataText.length > 0
                text: "Text: " + root.operation.dataText
                role: "mono"
                secondary: true
                Layout.fillWidth: true
            }
            XpLabel {
                visible: root.operation.notes.length > 0
                text: root.operation.notes
                role: "caption"
                color: Theme.warning
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }
}
