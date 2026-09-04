import QtQuick
import QtQuick.Layouts
import XP60Studio

// Label plus exact numeric entry, the companion every continuous control
// needs so a value can always be typed rather than only dragged.
RowLayout {
    id: root

    property string label: ""
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

    spacing: Metrics.spacingSm

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

    XpTextField {
        id: field
        objectName: "valueField"
        Layout.preferredWidth: 68
        Layout.minimumWidth: 56
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
        Accessible.name: root.label
    }
}
