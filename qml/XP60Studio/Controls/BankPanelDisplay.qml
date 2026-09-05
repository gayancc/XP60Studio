import QtQuick
import QtQuick.Layouts
import XP60Studio

// A live Bank Builder projection rendered as an XP-60-inspired backlit LCD.
// There is deliberately no destination state in this component: every normal
// value is read directly from BankBuilderViewModel. `preview` is transient drag
// feedback supplied by that same model and disappears when the gesture ends.
Item {
    id: root

    required property var builder
    property var preview: null

    readonly property bool previewing: preview !== null && preview !== undefined
                                       && preview.panelLabel !== undefined
    readonly property string shownPanel: previewing ? preview.panelLabel : builder.panelLabel
    readonly property string shownLinear: previewing ? preview.linearLabel : builder.linearLabel
    readonly property string shownName: previewing ? (preview.patchName || "") : builder.currentPatchName
    readonly property string shownState: previewing ? preview.action : builder.currentState
    readonly property string shownSubgroup: previewing ? shownPanel.charAt(0) : builder.subgroupLabel
    readonly property string shownBank: previewing ? shownPanel.charAt(1) : builder.bank.toString()
    readonly property string shownNumber: previewing ? shownPanel.charAt(2) : builder.number.toString()
    readonly property string lcdRegularFamily: lcdRegular.status === FontLoader.Ready
                                                ? lcdRegular.name : Typography.monoFamily
    readonly property string lcdBoldFamily: lcdBold.status === FontLoader.Ready
                                             ? lcdBold.name : lcdRegularFamily

    implicitHeight: 116

    FontLoader {
        id: lcdRegular
        source: Qt.resolvedUrl("../Assets/Fonts/Silkscreen-Regular.ttf")
    }
    FontLoader {
        id: lcdBold
        source: Qt.resolvedUrl("../Assets/Fonts/Silkscreen-Bold.ttf")
    }

    // Heavy outer surround and inset edge: the LCD is mounted in the panel,
    // not painted on top of it.
    Rectangle {
        anchors.fill: parent
        radius: 6
        color: Theme.lcdBezel
        border.width: 1
        border.color: root.previewing ? Theme.success : Theme.borderStrong

        Behavior on border.color {
            enabled: !Motion.reducedMotion
            ColorAnimation { duration: Motion.durationFast }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 3
            radius: 4
            color: Theme.lcdFrame
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.07)

            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                anchors.leftMargin: 3
                anchors.rightMargin: 3
                anchors.bottomMargin: 2
                height: 2
                color: Qt.rgba(0, 0, 0, 0.55)
            }
        }
    }

    Rectangle {
        id: lcd
        anchors.fill: parent
        anchors.margins: 8
        radius: 2
        border.width: 1
        border.color: Qt.rgba(0.45, 0.72, 1, 0.85)
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.lcdBacklightTop }
            GradientStop { position: 0.24; color: Theme.lcdBacklight }
            GradientStop { position: 1.0; color: Theme.lcdBacklightDeep }
        }

        // A narrow edge glow and quiet scan lines give the blue field the
        // optical depth of a backlit display without obscuring information.
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top }
            anchors.margins: 1
            height: 1
            color: Qt.rgba(0.75, 0.9, 1, 0.48)
        }
        Repeater {
            model: 8
            delegate: Rectangle {
                required property int index
                x: 2
                y: 10 + index * 11
                width: lcd.width - 4
                height: 1
                color: Qt.rgba(0, 0, 0.25, 0.055)
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 9
            anchors.rightMargin: 9
            anchors.topMargin: 5
            anchors.bottomMargin: 5
            spacing: 2

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 14
                spacing: Metrics.spacingSm

                XpLabel {
                    text: qsTr("BANK BUILDER")
                    role: "mono"
                    font.family: root.lcdBoldFamily
                    font.pixelSize: 8
                    font.weight: Typography.weightBold
                    font.hintingPreference: Font.PreferFullHinting
                    font.kerning: false
                    color: Theme.lcdText
                }
                XpLabel {
                    text: qsTr("USER TARGET")
                    role: "mono"
                    font.family: root.lcdRegularFamily
                    font.pixelSize: 8
                    font.hintingPreference: Font.PreferFullHinting
                    font.kerning: false
                    color: Theme.lcdTextDim
                }
                Item { Layout.fillWidth: true }
                XpLabel {
                    objectName: "bankDisplayState"
                    text: root.shownState
                    role: "mono"
                    font.family: root.lcdBoldFamily
                    font.pixelSize: 8
                    font.weight: Typography.weightBold
                    font.hintingPreference: Font.PreferFullHinting
                    font.kerning: false
                    color: root.shownState === "MISSING" || root.shownState === "REPLACE"
                           ? "#FFF0B8" : Theme.lcdText
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.lcdText
                opacity: 0.82
            }

            // The hardware's main line: group, current location and the Patch
            // occupying it. The outlined value window makes the destination
            // unmistakable without inflating the selector keys.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                color: Qt.rgba(0.01, 0.03, 0.18, 0.13)
                border.width: 1
                border.color: Qt.rgba(0.82, 0.93, 1, root.previewing ? 0.98 : 0.72)

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 7
                    anchors.rightMargin: 7
                    spacing: 7

                    XpLabel {
                        text: "*USER:"
                        role: "mono"
                        font.family: root.lcdBoldFamily
                        font.pixelSize: 16
                        font.weight: Typography.weightBold
                        font.hintingPreference: Font.PreferFullHinting
                        font.kerning: false
                        color: Theme.lcdText
                    }

                    Rectangle {
                        Layout.preferredWidth: 56
                        Layout.preferredHeight: 27
                        color: Qt.rgba(0.01, 0.03, 0.16, 0.18)
                        border.width: 1
                        border.color: Theme.lcdSelection

                        XpLabel {
                            objectName: "bankDisplayPanelLabel"
                            anchors.centerIn: parent
                            text: root.shownPanel
                            role: "mono"
                            font.family: root.lcdBoldFamily
                            font.pixelSize: 16
                            font.weight: Typography.weightBold
                            font.hintingPreference: Font.PreferFullHinting
                            font.kerning: false
                            color: Theme.lcdSelection
                        }
                    }

                    XpLabel {
                        objectName: "bankDisplayPatchName"
                        Layout.fillWidth: true
                        text: root.shownName.length > 0 ? root.shownName : qsTr("-- EMPTY DESTINATION --")
                        role: "mono"
                        font.family: root.lcdBoldFamily
                        font.pixelSize: 16
                        font.weight: Typography.weightBold
                        font.hintingPreference: Font.PreferFullHinting
                        font.kerning: false
                        color: root.shownName.length > 0 ? Theme.lcdText : Theme.lcdTextDim
                        elide: Text.ElideRight
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 15
                spacing: Metrics.spacingLg

                XpLabel {
                    objectName: "bankDisplaySpokenLabel"
                    text: qsTr("SUB %1   BANK %2   NUMBER %3")
                          .arg(root.shownSubgroup).arg(root.shownBank).arg(root.shownNumber)
                    role: "mono"
                    font.family: root.lcdRegularFamily
                    font.pixelSize: 8
                    font.weight: Typography.weightMedium
                    font.hintingPreference: Font.PreferFullHinting
                    font.kerning: false
                    color: Theme.lcdText
                }
                XpLabel {
                    objectName: "bankDisplayLinearLabel"
                    text: qsTr("PATCH %1").arg(root.shownLinear)
                    role: "mono"
                    font.family: root.lcdBoldFamily
                    font.pixelSize: 8
                    font.weight: Typography.weightBold
                    font.hintingPreference: Font.PreferFullHinting
                    font.kerning: false
                    color: Theme.lcdSelection
                }
                Item { Layout.fillWidth: true }
                XpLabel {
                    text: qsTr("%1/128 FILLED").arg(root.builder.occupiedCount)
                    role: "mono"
                    font.family: root.lcdRegularFamily
                    font.pixelSize: 8
                    font.hintingPreference: Font.PreferFullHinting
                    font.kerning: false
                    color: Theme.lcdTextDim
                }
            }

            XpLabel {
                Layout.fillWidth: true
                Layout.preferredHeight: 13
                text: {
                    if (root.previewing) {
                        if (root.shownState === "REPLACE")
                            return qsTr("REPLACE: %1").arg(root.preview.occupant || qsTr("occupied destination"))
                        if (root.shownState === "SWAP")
                            return qsTr("SWAP WITH: %1").arg(root.preview.occupant || "")
                        if (root.shownState === "MOVE")
                            return qsTr("MOVE FROM: %1").arg(root.preview.from || "")
                        return qsTr("DROP TO ASSIGN")
                    }
                    if (root.builder.currentState === "MISSING")
                        return qsTr("SOURCE MISSING - DESTINATION PRESERVED")
                    if (!root.builder.currentOccupied)
                        return qsTr("READY - SELECT OR DROP A PATCH")
                    var from = root.builder.currentSourceName
                    var slot = root.builder.currentSourceSlot
                    return slot.length > 0 ? qsTr("FROM %1  /  %2").arg(from).arg(slot)
                                           : qsTr("FROM %1").arg(from)
                }
                role: "mono"
                font.family: root.lcdRegularFamily
                font.pixelSize: 8
                font.hintingPreference: Font.PreferFullHinting
                font.kerning: false
                color: Theme.lcdTextDim
                elide: Text.ElideRight
            }
        }
    }
}
