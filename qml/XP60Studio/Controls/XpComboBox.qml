import QtQuick
import QtQuick.Templates as T
import XP60Studio

// Drop-down selector styled for the dark shell. Works with C++ list models
// (set textRole) or plain string lists.
T.ComboBox {
    id: control

    implicitWidth: 220
    implicitHeight: Metrics.controlHeight
    leftPadding: Metrics.spacingMd
    rightPadding: Metrics.spacingMd + indicatorItem.width
    font.pointSize: Typography.bodySize
    hoverEnabled: true

    Accessible.role: Accessible.ComboBox

    delegate: T.ItemDelegate {
        id: delegate
        required property var model
        required property int index
        width: ListView.view ? ListView.view.width : control.width
        height: Metrics.controlHeight
        leftPadding: Metrics.spacingMd
        rightPadding: Metrics.spacingMd
        highlighted: control.highlightedIndex === index
        hoverEnabled: true
        contentItem: XpLabel {
            text: control.textRole ? (Array.isArray(control.model) ? delegate.model.modelData[control.textRole] : delegate.model[control.textRole])
                                   : delegate.model.modelData !== undefined ? delegate.model.modelData : delegate.model.display
            color: delegate.highlighted ? Theme.textPrimary : Theme.textSecondary
        }
        background: Rectangle {
            color: delegate.highlighted ? Theme.selection : (delegate.hovered ? Theme.surfaceHover : "transparent")
            radius: Metrics.radiusSm
        }
    }

    indicator: Item {
        id: indicatorItem
        x: control.mirrored ? control.leftPadding : control.width - width - Metrics.spacingSm
        width: 20
        height: parent.height
        XpLabel {
            anchors.centerIn: parent
            text: "⌄"
            role: "heading"
            color: control.enabled ? Theme.textSecondary : Theme.textDisabled
        }
    }

    contentItem: XpLabel {
        leftPadding: 0
        rightPadding: control.indicator.width + control.spacing
        text: control.displayText
        color: control.enabled ? Theme.textPrimary : Theme.textDisabled
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: Metrics.radiusSm
        color: !control.enabled ? Theme.surface : (control.pressed ? Theme.surfacePressed : (control.hovered ? Theme.surfaceHover : Theme.surfaceSunken))
        border.width: Metrics.borderWidth
        border.color: control.visualFocus || control.popup.visible ? Theme.focusRing : Theme.borderStrong
        Behavior on color { ColorAnimation { duration: Motion.durationFast } }
    }

    popup: T.Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 2 * padding, 320)
        padding: 4
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            T.ScrollIndicator.vertical: T.ScrollIndicator {}
        }
        background: Rectangle {
            radius: Metrics.radiusSm
            color: Theme.surfaceRaised
            border.width: Metrics.borderWidth
            border.color: Theme.borderStrong
        }
    }
}
