pragma Singleton
import QtQuick

// Type scale. Sizes are in points so they follow the platform DPI scaling.
QtObject {
    // Offscreen renderers can otherwise select the first installed font
    // (for example Agency FB on Windows), changing every control's metrics.
    readonly property var familyCandidates: ["Segoe UI", "SF Pro Text", "Helvetica Neue", "Noto Sans", "DejaVu Sans", "Arial"]
    readonly property string family: {
        var installed = Qt.fontFamilies()
        for (var i = 0; i < familyCandidates.length; ++i) {
            if (installed.indexOf(familyCandidates[i]) >= 0)
                return familyCandidates[i]
        }
        return Qt.application.font.family
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

    readonly property int displaySize: 26
    readonly property int titleSize: 18
    readonly property int headingSize: 14
    readonly property int bodySize: 12
    readonly property int captionSize: 11
    readonly property int overlineSize: 10
    readonly property int monoSize: 11

    readonly property int weightRegular: Font.Normal
    readonly property int weightMedium: Font.DemiBold
    readonly property int weightBold: Font.Bold

    readonly property real overlineSpacing: 1.2
}
