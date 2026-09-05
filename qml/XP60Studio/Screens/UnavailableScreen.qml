import QtQuick
import XP60Studio

// Shown if navigation ever lands on a destination whose phase has not
// started. Reachable only through the disabled rail entries.
Item {
    property string screenTitle: ""
    property string availability: ""

    XpEmptyState {
        anchors.fill: parent
        iconName: "dashboard"
        title: screenTitle + " is not part of Phase 1"
        message: "This destination is implemented in " + availability + " once its backing domain layer is trustworthy."
    }
}
