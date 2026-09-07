pragma Singleton
import QtQuick

// Type ramp.
//
// Sizes are device-independent **pixels**, not points. Every control height,
// pill height and row height in Metrics is a pixel constant, so the type ramp
// has to use the same unit or the two drift apart: a 12 pt body renders at
// 16 px on a 96 dpi display, which is a quarter larger than the containers
// were sized for, and it drifts again when the user changes the OS font size
// without changing the display scale. Pixel sizes still scale with the
// display's device pixel ratio, so 125%/150%/175% OS scaling enlarges text and
// geometry together — which is the behaviour a dense desktop editor needs.
QtObject {
    // Keep the application face independent from fonts loaded by individual
    // components. In particular, loading Silkscreen for the Bank Builder LCD
    // must never allow it to become Qt.application.font and leak into the
    // shell. Naming the native UI family also keeps metrics deterministic in
    // headless captures, whose font database may initially be sparse.
    readonly property string family: {
        if (Qt.platform.os === "windows")
            return "Segoe UI"
        if (Qt.platform.os === "osx" || Qt.platform.os === "macos")
            return "SF Pro Text"
        if (Qt.platform.os === "ios")
            return "SF Pro Text"
        if (Qt.platform.os === "android")
            return "Roboto"
        return "Noto Sans"
    }

    // First installed monospace face from the preferred list; "monospace"
    // lets the platform pick its default when none of them is present.
    readonly property var monoCandidates: ["JetBrains Mono", "SF Mono", "Menlo", "Consolas", "Cascadia Mono", "DejaVu Sans Mono", "Liberation Mono"]
    readonly property string monoFamily: {
        var installed = Qt.fontFamilies()
        for (var i = 0; i < monoCandidates.length; ++i) {
            if (installed.indexOf(monoCandidates[i]) >= 0)
                return monoCandidates[i]
        }
        return "monospace"
    }

    // The ramp. Each size has one job; a screen that needs a size not on this
    // list needs a different role, not a one-off number.
    readonly property int displaySize: 22     // screen title, one per screen
    readonly property int titleSize: 17       // patch name, hero value
    readonly property int headingSize: 14     // panel heading
    readonly property int subheadingSize: 13  // module heading inside a panel
    readonly property int bodySize: 13        // default
    readonly property int valueSize: 13       // parameter value
    readonly property int labelSize: 12       // parameter label — title case
    readonly property int captionSize: 12     // helper text, metadata
    readonly property int overlineSize: 11    // section overline — upper case
    readonly property int monoSize: 12        // numeric value, address
    readonly property int dataSize: 12        // raw SysEx bytes

    readonly property int weightRegular: Font.Normal
    readonly property int weightMedium: Font.DemiBold
    readonly property int weightBold: Font.Bold

    // Letter spacing is only ever applied to `overline`. It is a signal that a
    // line is a section marker, not a substitute for hierarchy anywhere else.
    readonly property real overlineSpacing: 0.6
}
