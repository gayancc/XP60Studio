import QtQuick
import QtQuick.Templates as T
import XP60Studio

// Branded button. variant: "primary" | "secondary" | "ghost" | "danger"
T.Button {
    id: control

    property string variant: "secondary"
    property string glyph: ""
    property string iconName: ""
    property bool compact: false

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: compact ? Metrics.controlHeightSm : Metrics.controlHeight
    leftPadding: compact ? Metrics.spacingSm : Metrics.spacingMd
    rightPadding: compact ? Metrics.spacingSm : Metrics.spacingMd
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.Button
    Accessible.name: control.text

    readonly property color fillColor: {
        if (!enabled) return variant === "ghost" ? "transparent" : Theme.surfaceSunken
        if (variant === "primary") return pressed ? Theme.accentPressed : (hovered ? Theme.accentHover : Theme.accent)
        if (variant === "danger") return pressed ? Qt.darker(Theme.error, 1.3) : (hovered ? Theme.error : Theme.errorSoft)
        if (variant === "ghost") return pressed ? Theme.surfacePressed : (hovered ? Theme.surfaceHover : "transparent")
        return pressed ? Theme.surfacePressed : (hovered ? Theme.surfaceHover : Theme.surfaceRaised)
    }
    readonly property color outlineColor: {
        if (!enabled) return Theme.borderSubtle
        if (visualFocus) return Theme.focusRing
        if (variant === "primary") return "transparent"
        if (variant === "danger") return Theme.error
        if (variant === "ghost") return hovered ? Theme.border : "transparent"
        return Theme.borderStrong
    }
    readonly property color labelColor: {
        if (!enabled) return Theme.textDisabled
        if (variant === "primary") return Theme.textOnAccent
        if (variant === "danger") return (hovered || pressed) ? Theme.textOnAccent : Theme.error
        return Theme.textPrimary
    }

    background: Rectangle {
        radius: Metrics.radiusSm
        color: control.fillColor
        border.width: Metrics.borderWidth
        border.color: control.outlineColor
        Behavior on color { ColorAnimation { duration: Motion.durationFast } }
    }

    contentItem: Row {
        spacing: (control.iconName.length > 0 || control.glyph.length > 0) && control.text.length > 0 ? Metrics.spacingSm : 0
        XpIcon {
            visible: control.iconName.length > 0
            name: control.iconName
            color: control.labelColor
            anchors.verticalCenter: parent.verticalCenter
        }
        XpLabel {
            visible: control.glyph.length > 0
            text: control.glyph
            color: control.labelColor
            anchors.verticalCenter: parent.verticalCenter
        }
        XpLabel {
            text: control.text
            visible: control.text.length > 0
            color: control.labelColor
            font.weight: Typography.weightMedium
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
