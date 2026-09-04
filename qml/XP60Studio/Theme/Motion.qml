pragma Singleton
import QtQuick

// Animation durations. Set reducedMotion to disable non-essential motion.
QtObject {
    property bool reducedMotion: false

    readonly property int durationFast: reducedMotion ? 0 : 90
    readonly property int durationNormal: reducedMotion ? 0 : 160
    readonly property int durationSlow: reducedMotion ? 0 : 260

    readonly property int easingStandard: Easing.OutCubic
}
