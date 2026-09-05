import QtQuick
import QtQuick.Layouts
import XP60Studio

// A group *inside* a panel, without another border.
//
// This is the answer to card-inside-card-inside-card. When a panel needed to
// set some content apart, the only choice was another bordered rounded
// rectangle, which put two parallel 1 px borders 16 px apart and made the
// nesting look like a mistake. Here the grouping is carried by a surface step
// and by the heading's own weight; `tone` tints the surface when the group is
// carrying a state (armed to send, a warning, a verified result), and only
// then does it take an edge.
//
// Use `XpCard` for a panel. Use `XpSection` for a group within one. Children
// are laid out in a column and may use `Layout.*`.
Rectangle {
    id: root

    default property alias contentData: layout.data
    property string title: ""
    // "" (plain raised surface) | "neutral" | "success" | "warning" | "error"
    // | "info" | "accent" | "live"
    property string tone: ""
    // A data well — inset rather than raised. For fields and raw protocol
    // content, which should read as recessed into the panel.
    property bool sunken: false
    property int padding: Metrics.spacingMd
    property alias spacing: layout.spacing

    readonly property bool tinted: tone.length > 0 && tone !== "neutral"

    color: tinted ? Theme.toneBackground(tone)
         : sunken ? Theme.surfaceSunken : Theme.surfaceRaised
    radius: Metrics.radiusSm
    // Only a tinted (state-carrying) group takes an edge. A plain group is
    // separated by its surface alone.
    border.width: tinted ? Metrics.borderWidth : 0
    border.color: tinted ? Theme.toneBorder(tone) : "transparent"

    implicitWidth: layout.implicitWidth + 2 * padding
    implicitHeight: layout.implicitHeight + 2 * padding

    ColumnLayout {
        id: layout
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: root.padding }
        spacing: Metrics.spacingSm

        XpLabel {
            visible: root.title.length > 0
            text: root.title
            role: "overline"
            color: root.tinted ? Theme.toneForeground(root.tone) : Theme.textSecondary
            Layout.fillWidth: true
        }
    }
}
