pragma Singleton
import QtQuick

// Colour tokens. Every XP60Studio component reads from here; screens never
// hard-code colours.
//
// The surface ladder is the load-bearing part. Grouping is expressed by moving
// a step up or down the ladder, not by drawing another border: two nested
// bordered rectangles 16 px apart read as a mistake, whereas a raised surface
// inside a panel reads as a group. The steps are therefore spaced far enough
// apart to be legible on a dim display, which the earlier palette was not
// (`surface` and `surfaceRaised` differed by about 4%).
QtObject {
    // Surfaces ---------------------------------------------------------------
    readonly property color windowBackground: "#06090F"   // application ground
    readonly property color railBackground: "#0A0F19"     // navigation
    readonly property color headerBackground: "#0A0F19"
    readonly property color surface: "#101825"            // panel
    readonly property color surfaceRaised: "#18222F"      // group inside a panel
    readonly property color surfaceSunken: "#0A0E16"      // inset field / data well
    readonly property color surfaceHover: "#1E2836"
    readonly property color surfacePressed: "#0D141F"
    readonly property color selection: "#1B3A6E"

    // Borders ----------------------------------------------------------------
    readonly property color border: "#202C3D"
    readonly property color borderStrong: "#2E3B50"
    readonly property color borderSubtle: "#172131"

    // Text -------------------------------------------------------------------
    readonly property color textPrimary: "#E8EEF8"
    readonly property color textSecondary: "#9DAAC0"
    readonly property color textMuted: "#6D7A93"
    readonly property color textDisabled: "#465268"
    readonly property color textOnAccent: "#FFFFFF"

    // Accent -----------------------------------------------------------------
    // Reserved for the current navigation item, the primary action, focus and
    // the selected Tone. Not for decorative headings.
    readonly property color accent: "#3B82F6"
    readonly property color accentHover: "#5B96FF"
    readonly property color accentPressed: "#2F6AD1"
    readonly property color accentSoft: "#152742"
    readonly property color accentText: "#7FB0FF"

    // Semantic ---------------------------------------------------------------
    readonly property color success: "#22C55E"
    readonly property color successSoft: "#112E22"
    readonly property color warning: "#F5A524"
    readonly property color warningSoft: "#2E2312"
    readonly property color error: "#EF4444"
    readonly property color errorSoft: "#301418"
    readonly property color info: "#38BDF8"
    readonly property color infoSoft: "#112733"
    readonly property color neutral: "#6D7A93"
    readonly property color neutralSoft: "#18202D"

    // Connection / live ------------------------------------------------------
    readonly property color live: "#2ED47A"
    readonly property color liveSoft: "#0F2E1E"
    readonly property color offline: "#6D7A93"
    readonly property color connecting: "#F5A524"

    // Editing state ----------------------------------------------------------
    // A parameter the user moved but has not sent, versus the value the XP-60
    // is known to hold. The editor has to be able to show both at once.
    readonly property color localEdit: "#F5A524"
    readonly property color hardwareValue: "#6D7A93"

    // Raw protocol data ------------------------------------------------------
    // SysEx bytes, addresses and device IDs are technical data, not form
    // content, and are presented on their own surface.
    readonly property color dataBackground: "#080C13"
    readonly property color dataBorder: "#1A2330"
    readonly property color dataText: "#B8C6DC"
    readonly property color dataDim: "#5C6980"
    readonly property color midiIn: "#38BDF8"
    readonly property color midiOut: "#A855F7"

    // Tone identity (stable across the whole application) --------------------
    readonly property color tone1: "#3B82F6"
    readonly property color tone2: "#14B8A6"
    readonly property color tone3: "#F59E0B"
    readonly property color tone4: "#A855F7"

    function toneColor(index) {
        switch (index) {
        case 1: return tone1
        case 2: return tone2
        case 3: return tone3
        case 4: return tone4
        }
        return textSecondary
    }

    // Semantic tone lookup used by StatusPill / XpMetricTile -----------------
    function toneForeground(tone) {
        switch (tone) {
        case "success": return success
        case "warning": return warning
        case "error": return error
        case "info": return info
        case "accent": return accentText
        case "live": return live
        }
        return textSecondary
    }

    function toneBackground(tone) {
        switch (tone) {
        case "success": return successSoft
        case "warning": return warningSoft
        case "error": return errorSoft
        case "info": return infoSoft
        case "accent": return accentSoft
        case "live": return liveSoft
        }
        return neutralSoft
    }

    // A tone's border, at the one opacity used everywhere a tinted container
    // needs an edge. Sites should not re-derive this with their own alpha.
    function toneBorder(tone) {
        var fg = toneForeground(tone)
        return Qt.rgba(fg.r, fg.g, fg.b, 0.35)
    }

    // Misc -------------------------------------------------------------------
    readonly property color focusRing: "#5B96FF"
    readonly property color scrollBar: "#2E3B50"
    readonly property color scrollBarHover: "#3D4C68"
}
