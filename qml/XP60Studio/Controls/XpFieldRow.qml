import QtQuick
import QtQuick.Layouts
import XP60Studio

// One label-plus-control row, with a shared label column.
//
// Connection options had `From XP-60` and `To XP-60` inside a two-column
// `GridLayout` while `Speed` sat in a sibling `RowLayout`, so the Speed field
// started 44 px to the left of the other two in what looks like one form. A
// `GridLayout` only aligns rows that are in the same grid, which is easy to
// break as a screen grows. Rows built from this component share `labelWidth`
// instead, so they line up whatever their parents are.
//
// Place these in a `ColumnLayout` and give every row the same `labelWidth`.
RowLayout {
    id: root

    property string label: ""
    property int labelWidth: 96
    // Helper text under the control, for the one field that needs it.
    property string hint: ""
    default property alias controlData: controlHolder.data

    spacing: Metrics.spacingMd
    Layout.fillWidth: true

    XpLabel {
        text: root.label
        role: "label"
        secondary: true
        // Fixed width is what makes the column align; the label is allowed to
        // elide rather than push the control out of the column.
        Layout.preferredWidth: root.labelWidth
        Layout.minimumWidth: root.labelWidth
        Layout.maximumWidth: root.labelWidth
        // Top-aligned so a wrapping control keeps the label on the first line.
        Layout.alignment: Qt.AlignTop
        Layout.topMargin: Math.max(0, (Metrics.controlHeight - implicitHeight) / 2)
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Metrics.spacingXs

        // A row, so a field can carry a trailing button or status pill; a
        // single control just uses `Layout.fillWidth`.
        RowLayout {
            id: controlHolder
            Layout.fillWidth: true
            spacing: Metrics.spacingSm
        }

        XpLabel {
            visible: root.hint.length > 0
            text: root.hint
            role: "caption"
            muted: true
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
