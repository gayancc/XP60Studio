pragma Singleton
import QtQuick

// Colour tokens sampled from docs/design/xp60studio-ui-master-mockup.jpg.
// Every XP60Studio component reads from here; screens never hard-code colours.
QtObject {
    // Surfaces ---------------------------------------------------------------
    readonly property color windowBackground: "#05080F"
    readonly property color railBackground: "#080C15"
    readonly property color headerBackground: "#080C15"
    readonly property color surface: "#0D131E"
    readonly property color surfaceRaised: "#111927"
    readonly property color surfaceSunken: "#070B12"
    readonly property color surfaceHover: "#141D2C"
    readonly property color surfacePressed: "#0B111B"
    readonly property color selection: "#15305E"

    // Borders ----------------------------------------------------------------
    readonly property color border: "#1B2534"
    readonly property color borderStrong: "#273246"
    readonly property color borderSubtle: "#121A27"

    // Text -------------------------------------------------------------------
    readonly property color textPrimary: "#E6EDF7"
    readonly property color textSecondary: "#96A3B8"
    readonly property color textMuted: "#63708A"
    readonly property color textDisabled: "#3E4A5F"
    readonly property color textOnAccent: "#FFFFFF"

    // Accent -----------------------------------------------------------------
    readonly property color accent: "#3B82F6"
    readonly property color accentHover: "#5B96FF"
    readonly property color accentPressed: "#2F6AD1"
    readonly property color accentSoft: "#12213B"
    readonly property color accentText: "#7FB0FF"

    // Semantic ---------------------------------------------------------------
    readonly property color success: "#22C55E"
    readonly property color successSoft: "#0F2A1F"
    readonly property color warning: "#F5A524"
    readonly property color warningSoft: "#2B2010"
    readonly property color error: "#EF4444"
    readonly property color errorSoft: "#2D1216"
    readonly property color info: "#38BDF8"
    readonly property color infoSoft: "#0F2431"
    readonly property color neutral: "#63708A"
    readonly property color neutralSoft: "#131B28"

    // Connection / live ------------------------------------------------------
    readonly property color live: "#2ED47A"
    readonly property color liveSoft: "#0D2A1B"
    readonly property color offline: "#63708A"
    readonly property color connecting: "#F5A524"

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

    // Misc -------------------------------------------------------------------
    readonly property color focusRing: "#5B96FF"
    readonly property color scrollBar: "#273246"
    readonly property color scrollBarHover: "#354362"
}
