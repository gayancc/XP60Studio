import QtQuick
import QtQuick.Controls as QQC
import XP60Studio

// One of the eight destinations the current SUBGROUP + BANK exposes.
//
// A tile is deliberately *not* a card. It is the lower half of the physical
// NUMBER button above it: same column, same width, a continuous stem joining
// the two, and the same lit/unlit language. Selecting NUMBER 5 lights the
// button and the tile together, so the player sees one control, not a button
// and a list row that happen to correspond.
//
// An empty destination is drawn as a bare recess with its position engraved in
// it, not as a card containing the word "Empty". That is the difference
// between a panel with a free slot and a form with a blank field.
Item {
    id: root

    // One entry of BankBuilderViewModel.visibleDestinations.
    required property var destination
    property bool current: false
    property bool dropCandidate: false
    property bool dropActive: false
    // "PLACE" | "REPLACE" | "MOVE" | "SWAP" — what a drop here would do.
    property string dropAction: ""
    // Dimmed while it is the source of the drag in progress.
    property bool dragging: false
    // Set briefly after a successful placement.
    property bool flashing: false

    signal clicked()
    signal auditioned()
    signal dragStarted(real x, real y)
    signal dragMoved(real x, real y)
    signal dragReleased()
    signal dragCancelled()

    readonly property bool occupied: destination.occupied === true
    readonly property bool missing: destination.missing === true

    implicitHeight: 84
    opacity: dragging ? 0.35 : 1
    Behavior on opacity {
        enabled: !Motion.reducedMotion
        NumberAnimation { duration: Motion.durationFast }
    }

    Accessible.role: Accessible.Button
    Accessible.name: root.occupied
                     ? qsTr("%1, %2, patch %3").arg(destination.panelLabel)
                       .arg(destination.patchName).arg(destination.linearLabel)
                     : qsTr("%1, empty, patch %2").arg(destination.panelLabel).arg(destination.linearLabel)

    // The stem joining this tile to its NUMBER button. Short, and lit with the
    // same colour, because that connection is the whole point of the layout.
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        width: 2
        height: 6
        color: root.dropActive ? Theme.success
                               : (root.current ? Theme.accent : Theme.borderSubtle)
        Behavior on color {
            enabled: !Motion.reducedMotion
            ColorAnimation { duration: Motion.durationFast }
        }
    }

    Rectangle {
        id: body
        anchors.fill: parent
        anchors.topMargin: 6
        radius: Metrics.radiusSm
        color: {
            if (root.dropActive) return Theme.successSoft
            if (root.current) return Theme.accentSoft
            if (root.occupied) return Theme.surfaceRaised
            return Theme.surfaceSunken
        }
        border.width: root.current || root.dropActive ? 2 : 1
        border.color: {
            if (root.dropActive) return Theme.success
            if (root.dropCandidate) return Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.55)
            if (root.current) return Theme.accent
            if (root.missing) return Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.5)
            return root.occupied ? Theme.border : Theme.borderSubtle
        }

        Behavior on color {
            enabled: !Motion.reducedMotion
            ColorAnimation { duration: Motion.durationFast }
        }
        Behavior on border.color {
            enabled: !Motion.reducedMotion
            ColorAnimation { duration: Motion.durationFast }
        }

        // A filled destination sits slightly proud; an empty one is a hollow.
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top }
            anchors.margins: 2
            height: 1
            color: root.occupied ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.35)
        }

        // Placement confirmation: one pulse of the accent, then gone. Motion
        // that says "it landed there", not decoration.
        Rectangle {
            id: flash
            anchors.fill: parent
            radius: parent.radius
            color: Theme.success
            opacity: 0
            SequentialAnimation on opacity {
                id: flashAnimation
                running: false
                NumberAnimation { to: 0.30; duration: Motion.durationFast }
                NumberAnimation { to: 0.0; duration: Motion.durationSlow }
            }
        }

        Item {
            anchors.fill: parent
            anchors.margins: Metrics.spacingSm

            // The NUMBER, engraved. Big, quiet, and always in the same corner
            // so the eight read as a numbered row at a glance.
            XpLabel {
                id: numberLabel
                anchors { left: parent.left; top: parent.top }
                text: root.destination.number
                role: "mono"
                font.pixelSize: 15
                font.weight: Typography.weightBold
                color: root.current ? Theme.accentText
                                    : (root.occupied ? Theme.textSecondary : Theme.textDisabled)
            }

            // Occupancy LED, mirroring the NUMBER button's.
            Rectangle {
                anchors { right: parent.right; top: parent.top; topMargin: 4 }
                width: 6
                height: 6
                radius: 3
                visible: root.occupied
                color: root.missing ? Theme.error : Theme.live
            }

            XpLabel {
                id: nameLabel
                // Centred in the space between the engraved NUMBER and the
                // identity line, so a tall tile does not leave the name
                // stranded at the bottom of an empty box.
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    verticalCenterOffset: 3
                }
                text: root.occupied ? root.destination.patchName : qsTr("EMPTY")
                role: root.occupied ? "value" : "overline"
                font.weight: root.occupied ? Typography.weightMedium : Typography.weightRegular
                color: root.missing ? Theme.error
                                    : (root.occupied ? Theme.textPrimary : Theme.textDisabled)
                elide: Text.ElideRight
            }

            // Both identities, always together: the panel label leads, the
            // linear number follows in the dimmer weight.
            Row {
                id: identity
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                spacing: 5

                XpLabel {
                    text: root.destination.panelLabel
                    role: "mono"
                    font.weight: Typography.weightMedium
                    color: root.current ? Theme.accentText : Theme.textSecondary
                }
                XpLabel {
                    text: "·"
                    role: "mono"
                    color: Theme.textDisabled
                }
                XpLabel {
                    text: root.destination.linearLabel
                    role: "mono"
                    color: Theme.textMuted
                }
            }
        }

        // A truncated Patch name is still readable on hover: an eight-across
        // row cannot fit every XP-60 name, and guessing at "TEREBIN..." is not
        // a reasonable thing to ask of anybody.
        QQC.ToolTip.visible: handle.containsMouse && root.occupied && nameLabel.truncated
        QQC.ToolTip.delay: 500
        QQC.ToolTip.text: root.destination.patchName + "  ·  " + root.destination.panelLabel
                          + "  ·  " + qsTr("PATCH %1").arg(root.destination.linearLabel)

        // What a drop here would do, said before it happens.
        Rectangle {
            anchors.centerIn: parent
            visible: root.dropActive && root.dropAction.length > 0
            width: actionLabel.implicitWidth + 2 * Metrics.spacingSm
            height: 20
            radius: Metrics.radiusSm
            color: root.dropAction === "REPLACE" ? Theme.warning : Theme.success
            XpLabel {
                id: actionLabel
                anchors.centerIn: parent
                text: root.dropAction
                role: "overline"
                color: Theme.textOnAccent
                font.weight: Typography.weightBold
            }
        }
    }

    // Interaction: click selects, drag lifts the Patch out to somewhere else.
    // The threshold matters — without it a click on an occupied destination
    // starts a one-pixel drag and the selection never happens.
    MouseArea {
        id: handle
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true
        cursorShape: root.occupied ? (pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor) : Qt.ArrowCursor
        property point origin
        property bool lifted: false
        readonly property int threshold: 6

        onPressed: function (mouse) {
            origin = Qt.point(mouse.x, mouse.y)
            lifted = false
            root.clicked()
        }
        onPositionChanged: function (mouse) {
            if (!pressed || !root.occupied) return
            if (!lifted) {
                if (Math.abs(mouse.x - origin.x) < threshold && Math.abs(mouse.y - origin.y) < threshold)
                    return
                lifted = true
                var start = mapToItem(null, mouse.x, mouse.y)
                root.dragStarted(start.x, start.y)
            }
            var p = mapToItem(null, mouse.x, mouse.y)
            root.dragMoved(p.x, p.y)
        }
        onReleased: {
            if (lifted) root.dragReleased()
            lifted = false
        }
        onCanceled: {
            if (lifted) root.dragCancelled()
            lifted = false
        }
        onDoubleClicked: if (root.occupied) root.auditioned()
    }

    onFlashingChanged: if (flashing && !Motion.reducedMotion) flashAnimation.restart()
}
