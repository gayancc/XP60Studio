import QtQuick
import XP60Studio

// Text with the XP60Studio type ramp applied.
//
// role: "display" | "title" | "heading" | "subheading" | "body" | "label"
//     | "value" | "caption" | "overline" | "mono" | "data"
//
// `overline` is the only uppercase, letter-spaced role, and it marks a section.
// A parameter label is `label` (title case, quiet); a parameter value is
// `value`. Choosing `overline` for a parameter label is what turned the editor
// into a wall of shouting abbreviations.
Text {
    id: root

    property string role: "body"
    property bool secondary: false
    property bool muted: false

    readonly property bool isMono: role === "mono" || role === "data"

    color: muted ? Theme.textMuted : (secondary ? Theme.textSecondary : Theme.textPrimary)
    font.family: isMono ? Typography.monoFamily : Typography.family
    font.pixelSize: {
        switch (role) {
        case "display": return Typography.displaySize
        case "title": return Typography.titleSize
        case "heading": return Typography.headingSize
        case "subheading": return Typography.subheadingSize
        case "label": return Typography.labelSize
        case "value": return Typography.valueSize
        case "caption": return Typography.captionSize
        case "overline": return Typography.overlineSize
        case "mono": return Typography.monoSize
        case "data": return Typography.dataSize
        }
        return Typography.bodySize
    }
    font.weight: {
        switch (role) {
        case "display":
        case "title": return Typography.weightBold
        case "heading":
        case "subheading":
        case "value":
        case "overline": return Typography.weightMedium
        }
        return Typography.weightRegular
    }
    font.capitalization: role === "overline" ? Font.AllUppercase : Font.MixedCase
    font.letterSpacing: role === "overline" ? Typography.overlineSpacing : 0
    // Raw protocol data is read column-by-column, so its glyphs must not be
    // hinted into different advances between rows.
    font.kerning: !isMono
    elide: Text.ElideRight
    verticalAlignment: Text.AlignVCenter
}
