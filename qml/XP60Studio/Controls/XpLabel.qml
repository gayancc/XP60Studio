import QtQuick
import XP60Studio

// Text with the XP60Studio type scale applied.
// role: "display" | "title" | "heading" | "body" | "caption" | "overline" | "mono"
Text {
    id: root

    property string role: "body"
    property bool secondary: false
    property bool muted: false

    color: muted ? Theme.textMuted : (secondary ? Theme.textSecondary : Theme.textPrimary)
    font.family: role === "mono" ? Typography.monoFamily : Typography.family
    font.pointSize: {
        switch (role) {
        case "display": return Typography.displaySize
        case "title": return Typography.titleSize
        case "heading": return Typography.headingSize
        case "caption": return Typography.captionSize
        case "overline": return Typography.overlineSize
        case "mono": return Typography.monoSize
        }
        return Typography.bodySize
    }
    font.weight: {
        switch (role) {
        case "display":
        case "title": return Typography.weightBold
        case "heading":
        case "overline": return Typography.weightMedium
        }
        return Typography.weightRegular
    }
    font.capitalization: role === "overline" ? Font.AllUppercase : Font.MixedCase
    font.letterSpacing: role === "overline" ? Typography.overlineSpacing : 0
    elide: Text.ElideRight
    verticalAlignment: Text.AlignVCenter
}
