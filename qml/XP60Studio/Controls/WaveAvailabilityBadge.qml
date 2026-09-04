import QtQuick
import XP60Studio

// Availability badge for catalog rows. Internal ROM = Available; Expansion = not verified.
StatusPill {
    id: root
    property string availability: "available" // available | missing_catalog | unknown

    text: availability === "available" ? qsTr("Available")
        : availability === "missing_catalog" ? qsTr("Catalog unavailable")
        : qsTr("Unknown")
    tone: availability === "available" ? "success"
        : availability === "missing_catalog" ? "warning"
        : "neutral"
    showDot: true
}
