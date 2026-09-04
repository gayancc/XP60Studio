import QtQuick
import QtQuick.Layouts
import XP60Studio

// Structured verification mismatch surface (keeps full report text).
Rectangle {
    id: root

    property string report: ""

    objectName: "mismatchPanel"
    visible: report.length > 0
    radius: Metrics.radiusSm
    color: Theme.errorSoft
    border.width: 1
    border.color: Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.45)
    implicitHeight: visible ? col.implicitHeight + 2 * Metrics.spacingMd : 0

    ColumnLayout {
        id: col
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: Metrics.spacingMd }
        spacing: Metrics.spacingSm
        RowLayout {
            Layout.fillWidth: true
            StatusPill { text: "Verification mismatch"; tone: "error" }
            Item { Layout.fillWidth: true }
        }
        XpLabel {
            text: "Read-back did not match what was sent. Details below are kept parameter-by-parameter."
            role: "caption"
            color: Theme.error
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        XpLabel {
            text: root.report
            role: "mono"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
