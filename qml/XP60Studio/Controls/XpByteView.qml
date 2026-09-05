import QtQuick
import QtQuick.Layouts
import XP60Studio

// Raw protocol data: SysEx bytes, addresses, device IDs.
//
// This is technical data, not form content, and it should not look like
// either. It sits on its own darker surface, in the monospace face, grouped so
// the eye can count bytes, and it can be copied — reading a checksum off the
// screen and retyping it is how transcription errors get into a bug report.
//
// The view never interprets the bytes. Whatever the protocol layer produced is
// what is shown.
Rectangle {
    id: root

    // Space-separated hex bytes, as produced by the protocol layer.
    property string bytes: ""
    property string label: ""
    // "in" | "out" | "" — colours the direction marker only.
    property string direction: ""
    property string timestamp: ""
    // Bytes per group. Roland SysEx reads naturally in fours.
    property int groupSize: 4
    property int maximumLines: 0 // 0 = unlimited

    readonly property color directionColor: direction === "in" ? Theme.midiIn
                                         : direction === "out" ? Theme.midiOut
                                         : Theme.dataDim

    // Regroups the byte string so every group is `groupSize` bytes wide, with
    // a wider gap between groups than between bytes.
    readonly property string grouped: {
        var parts = bytes.trim().split(/\s+/).filter(function(b) { return b.length > 0 })
        if (parts.length === 0)
            return ""
        var out = []
        for (var i = 0; i < parts.length; i += groupSize)
            out.push(parts.slice(i, i + groupSize).join(" "))
        return out.join("   ")
    }

    color: Theme.dataBackground
    radius: Metrics.radiusSm
    border.width: Metrics.borderWidth
    border.color: Theme.dataBorder
    implicitHeight: column.implicitHeight + 2 * Metrics.spacingSm
    implicitWidth: column.implicitWidth + 2 * Metrics.spacingSm

    ColumnLayout {
        id: column
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: Metrics.spacingSm }
        spacing: Metrics.spacingXs

        RowLayout {
            visible: root.label.length > 0 || root.direction.length > 0 || root.timestamp.length > 0
            Layout.fillWidth: true
            spacing: Metrics.spacingSm

            XpIcon {
                visible: root.direction.length > 0
                name: root.direction === "in" ? "midi-in" : "midi-out"
                color: root.directionColor
                implicitWidth: Metrics.iconSizeSm
                implicitHeight: Metrics.iconSizeSm
            }

            XpLabel {
                visible: root.direction.length > 0
                text: root.direction === "in" ? "In" : "Out"
                role: "label"
                color: root.directionColor
            }

            XpLabel {
                text: root.label
                role: "label"
                color: Theme.dataDim
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            XpLabel {
                visible: root.timestamp.length > 0
                text: root.timestamp
                role: "mono"
                color: Theme.dataDim
            }

            XpButton {
                variant: "quiet"
                compact: true
                iconOnly: true
                iconName: "copy"
                Accessible.name: qsTr("Copy bytes")
                onClicked: byteText.copyToClipboard()
            }
        }

        // Selectable so a byte range can be copied on its own, not only the
        // whole line.
        TextEdit {
            id: byteText
            text: root.grouped
            readOnly: true
            selectByMouse: true
            font.family: Typography.monoFamily
            font.pixelSize: Typography.dataSize
            color: Theme.dataText
            selectionColor: Theme.selection
            selectedTextColor: Theme.textPrimary
            wrapMode: TextEdit.WrapAnywhere
            Layout.fillWidth: true
            Layout.maximumHeight: root.maximumLines > 0
                                  ? root.maximumLines * (Typography.dataSize + 4)
                                  : implicitHeight
            clip: true

            function copyToClipboard() {
                selectAll()
                copy()
                deselect()
            }
        }
    }
}
