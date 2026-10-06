import QtQuick
import XP60Studio

// Shown if navigation ever lands on a destination whose phase has not
// started. Reachable only through the disabled rail entries.
Item {
    property string screenTitle: ""
    property string availability: ""

    XpEmptyState {
        anchors.fill: parent
        iconName: "settings"
        title: qsTr("%1 is not available yet").arg(screenTitle)
        message: qsTr("This destination is implemented in %1, once its backing domain layer is trustworthy.").arg(availability)
    }
}
