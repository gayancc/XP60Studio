import QtQuick
import QtQuick.Layouts
import XP60Studio

// Label plus exact numeric entry, the companion every continuous control
// needs so a value can always be typed rather than only dragged.
RowLayout {
    id: root

    property string label: ""
    property string accessibleName: label
    property int value: 0
    property int minimumValue: 0
    property int maximumValue: 127
    property string displayText: ""
    property bool editable: true
    // Parameter names come verbatim from the Roland table and are often long,
    // so the caller sizes the label column and the name wraps rather than
    // being abbreviated into something the manual never says.
    property int labelWidth: 64
    signal edited(int value)

    spacing: Metrics.spacingXs

    WheelHandler {
        enabled: root.editable
        target: root
        orientation: Qt.Vertical
        onWheel: function(event) {
            var delta = event.angleDelta.y > 0 ? 1 : -1
            var next = Math.max(root.minimumValue, Math.min(root.maximumValue, root.value + delta))
            if (next !== root.value) root.edited(next)
        }
    }

    XpLabel {
        visible: root.label.length > 0
        text: root.label
        role: "caption"
        secondary: true
        Layout.preferredWidth: root.labelWidth
        Layout.maximumWidth: root.labelWidth
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
    }

    Rectangle {
        Layout.preferredWidth: 18
        Layout.preferredHeight: Metrics.controlHeightSm
        radius: Metrics.radiusSm
        color: decArea.pressed ? Theme.surfacePressed : (decArea.containsMouse ? Theme.surfaceHover : Theme.surfaceRaised)
        border.width: 1
        border.color: Theme.borderSubtle
        visible: root.editable
        XpLabel { anchors.centerIn: parent; text: "−"; role: "caption"; color: Theme.textSecondary }
        MouseArea {
            id: decArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                var next = Math.max(root.minimumValue, root.value - 1)
                if (next !== root.value) root.edited(next)
            }
        }
    }

    XpTextField {
        id: field
        objectName: "valueField"
        Layout.preferredWidth: 60
        Layout.minimumWidth: 48
        Layout.fillWidth: true
        mono: true
        enabled: root.editable
        horizontalAlignment: Text.AlignHCenter
        // Show the interpreted text while idle, the raw number while editing.
        text: activeFocus ? String(root.value)
                          : (root.displayText.length > 0 ? root.displayText : String(root.value))
        validator: IntValidator { bottom: root.minimumValue; top: root.maximumValue }
        onEditingFinished: {
            var v = parseInt(text)
            if (!isNaN(v) && v >= root.minimumValue && v <= root.maximumValue && v !== root.value)
                root.edited(v)
            focus = false
        }
        Keys.onUpPressed: {
            var next = Math.min(root.maximumValue, root.value + 1)
            if (next !== root.value) root.edited(next)
        }
        Keys.onDownPressed: {
            var next = Math.max(root.minimumValue, root.value - 1)
            if (next !== root.value) root.edited(next)
        }
        Accessible.name: root.accessibleName
    }

    Rectangle {
        Layout.preferredWidth: 18
        Layout.preferredHeight: Metrics.controlHeightSm
        radius: Metrics.radiusSm
        color: incArea.pressed ? Theme.surfacePressed : (incArea.containsMouse ? Theme.surfaceHover : Theme.surfaceRaised)
        border.width: 1
        border.color: Theme.borderSubtle
        visible: root.editable
        XpLabel { anchors.centerIn: parent; text: "+"; role: "caption"; color: Theme.textSecondary }
        MouseArea {
            id: incArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                var next = Math.min(root.maximumValue, root.value + 1)
                if (next !== root.value) root.edited(next)
            }
        }
    }
}
