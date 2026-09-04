import QtQuick
import QtQuick.Layouts
import XP60Studio

XpTextField {
    id: root
    objectName: "waveSearch"
    placeholderText: qsTr("Search name, bank or number…")
    Accessible.name: qsTr("Search waveforms")
}
