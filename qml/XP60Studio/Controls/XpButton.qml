import QtQuick
import QtQuick.Templates as T
import XP60Studio

// Branded button.
//
// variant: "primary"   filled accent — the one action a panel most wants
//          "secondary" raised, outlined — the default
//          "ghost"     low emphasis, but still outlined at rest
//          "quiet"     text plus a chevron, for disclosure only
//          "danger"    destructive
//
// `ghost` used to be fully transparent at rest, with no fill and no border,
// which is why "Connection options", "Scan", "Expand", "Clear" and
// "Show raw parameters" all rendered as bare text that nothing marked as
// clickable — while the non-interactive "Read only" badge beside them was the
// brightest thing in the row. A low-emphasis button is still a button, so
// `ghost` now carries a resting outline. `quiet` is the deliberately
// text-shaped variant, and it requires an icon: the chevron is the affordance.
T.Button {
    id: control

    property string variant: "secondary"
    property string glyph: ""
    property string iconName: ""
    property bool compact: false
    // Renders as a square target around a single icon. Used for the editor's
    // undo/redo and for row actions.
    property bool iconOnly: false

    readonly property int rowHeight: compact ? Metrics.controlHeightSm : Metrics.controlHeight

    implicitWidth: iconOnly
                   ? Math.max(Metrics.hitTarget, rowHeight)
                   : Math.max(implicitBackgroundWidth + leftInset + rightInset,
                              implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: iconOnly ? Math.max(Metrics.hitTarget, rowHeight) : rowHeight
    leftPadding: iconOnly ? 0 : (compact ? Metrics.spacingSm : Metrics.spacingMd)
    rightPadding: leftPadding
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.Button
    Accessible.name: control.text

    // Qt Quick Templates activates buttons with Space, but Return/Enter was
    // ignored in the Windows UIA path. Navigation controls already accept
    // both; keep the shared button equally predictable for keyboard users.
    Keys.onReturnPressed: control.clicked()
    Keys.onEnterPressed: control.clicked()

    readonly property bool quiet: variant === "quiet"

    readonly property color fillColor: {
        if (!enabled) return (quiet || variant === "ghost") ? "transparent" : Theme.surfaceSunken
        if (variant === "primary") return pressed ? Theme.accentPressed : (hovered ? Theme.accentHover : Theme.accent)
        if (variant === "danger") return pressed ? Qt.darker(Theme.error, 1.3) : (hovered ? Theme.error : Theme.errorSoft)
        if (quiet) return pressed ? Theme.surfacePressed : (hovered ? Theme.surfaceHover : "transparent")
        if (variant === "ghost") return pressed ? Theme.surfacePressed : (hovered ? Theme.surfaceHover : "transparent")
        return pressed ? Theme.surfacePressed : (hovered ? Theme.surfaceHover : Theme.surfaceRaised)
    }
    // The border is always 1 px wide and only its colour changes, so a button
    // never changes size between states and never nudges its neighbours.
    readonly property color outlineColor: {
        if (visualFocus) return Theme.focusRing
        if (!enabled) return quiet ? "transparent" : Theme.borderSubtle
        if (variant === "primary") return pressed ? Theme.accentPressed : Theme.accent
        if (variant === "danger") return Theme.error
        if (quiet) return hovered ? Theme.border : "transparent"
        if (variant === "ghost") return hovered ? Theme.borderStrong : Theme.borderSubtle
        return Theme.borderStrong
    }
    readonly property color labelColor: {
        if (!enabled) return Theme.textDisabled
        if (variant === "primary") return Theme.textOnAccent
        if (variant === "danger") return (hovered || pressed) ? Theme.textOnAccent : Theme.error
        if (quiet) return hovered ? Theme.textPrimary : Theme.textSecondary
        return Theme.textPrimary
    }

    background: Rectangle {
        radius: Metrics.radiusSm
        color: control.fillColor
        border.width: Metrics.borderWidth
        border.color: control.outlineColor
        Behavior on color {
            enabled: !Motion.reducedMotion
            ColorAnimation { duration: Motion.durationFast }
        }
        Behavior on border.color {
            enabled: !Motion.reducedMotion
            ColorAnimation { duration: Motion.durationFast }
        }
    }

    // The row is centred inside the content rect rather than left-aligned in
    // it, so an icon-only button's glyph sits in the middle of its 32 px
    // target instead of against the left edge.
    contentItem: Item {
        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight
        Row {
            id: contentRow
            anchors.centerIn: parent
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
}
