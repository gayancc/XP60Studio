import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Dialogs
import QtQuick.Layouts
import XP60Studio
import XP60Studio.Presentation

// Bank Builder — the Virtual XP-60 patch panel.
//
// The screen is two halves that are deliberately different kinds of object.
// On the left is the SOURCE LIBRARY: a flat list of Patches, on a plain
// surface, that supplies. On the right is the TARGET XP BANK: an instrument
// panel with SUBGROUP, BANK and NUMBER, that receives. A musician builds a
// bank by carrying a Patch from one to the other, and the panel is operated
// exactly as the instrument is — pick A or B, pick a BANK, and the eight
// NUMBER destinations that selection exposes are what you can see and drop on.
//
// The panel never asks anyone to think in 001-128. Every destination carries
// both identities, with the panel label leading and the linear number quiet
// beside it, and the mapping between them is computed once in
// xpmodel::Xp60BankLocation, never here.
//
// ── Why the drag is written this way ─────────────────────────────────────
// A drag is one pointer gesture owned by the item it started on, tracked in
// this screen's coordinate space, and hit-tested against the real geometry of
// the buttons and tiles. There are no DropAreas: nested DropAreas under a
// moving item is exactly what made an earlier canvas drag flicker between
// targets. Three rules keep it steady:
//
//   * a lift needs a few pixels of movement, so a click on a filled
//     destination still selects it rather than starting a one-pixel drag;
//   * an acquired destination is sticky — it is only given up once the
//     pointer is clearly outside it, so a shaky hand near an edge does not
//     flicker the drop in and out;
//   * hovering BANK or SUBGROUP while dragging changes the visible eight only
//     after a deliberate dwell, so passing over BANK 5 on the way to NUMBER 3
//     never moves the panel out from under the drop.
FocusScope {
    id: root

    required property BankBuilderViewModel builder
    required property LibraryListModel library
    // Optional: the shared import/export view model. Without it the screen
    // arranges and saves banks exactly as before but cannot read or write a
    // `.syx`, which is what the screenshot harness gets.
    property var transfer: null

    readonly property bool wide: width >= 1180

    // ── Drag state ───────────────────────────────────────────────────────
    property bool dragActive: false
    property var dragPatchId: 0
    property string dragPatchName: ""
    // >= 0 when the drag started at a destination inside the bank.
    property int dragFromSlot: -1
    property int dragFromRow: -1
    property point dragPoint: Qt.point(0, 0)
    property int dropSlot: -1
    property var dropPreview: null
    // Set for one beat after a placement lands, so the destination confirms it.
    property int flashSlot: -1
    // Dwell targets while dragging.
    property int hoverBank: -1
    property int hoverSubgroup: -1
    // True for the length of the refusal shake after a drop that landed on
    // nothing, so the ghost is still on screen to perform it.
    property bool refusing: false
    // Held across endDrag so the refusing ghost still says what was carried.
    property string refusedName: ""

    // How far outside an acquired destination the pointer must go before it is
    // released, and how long a BANK/SUBGROUP must be dwelt on before the panel
    // follows. Both are the difference between a surface that feels deliberate
    // and one that feels twitchy.
    readonly property int stickyPad: 22
    readonly property int dwellMs: 420

    onVisibleChanged: if (visible) root.forceActiveFocus()

    // ── Geometry helpers ─────────────────────────────────────────────────
    function rectOf(item) {
        if (!item || !item.visible)
            return null
        var p = item.mapToItem(root, 0, 0)
        return Qt.rect(p.x, p.y, item.width, item.height)
    }
    function within(r, x, y, pad) {
        return r !== null && x >= r.x - pad && x <= r.x + r.width + pad
                && y >= r.y - pad && y <= r.y + r.height + pad
    }
    // Which of the eight visible destinations the pointer is over, by tile or
    // by the NUMBER button above it. Both are the same destination: that is
    // the point of putting them in one column.
    function destinationAt(x, y) {
        for (var i = 0; i < 8; ++i) {
            if (within(rectOf(tiles.itemAt(i)), x, y, 0) || within(rectOf(numbers.itemAt(i)), x, y, 0))
                return root.builder.slotIndexFor(root.builder.subgroup, root.builder.bank, i + 1)
        }
        return -1
    }

    // ── Drag lifecycle ───────────────────────────────────────────────────
    function beginDragFromLibrary(row, patchId, patchName, wx, wy) {
        root.dragFromSlot = -1
        root.dragFromRow = row
        root.dragPatchId = patchId
        root.dragPatchName = patchName
        root.dragActive = true
        updateDrag(wx, wy)
    }
    function beginDragFromSlot(slotIndex, wx, wy) {
        var destination = root.builder.destinationAt(slotIndex)
        if (!destination || destination.occupied !== true)
            return
        root.dragFromSlot = slotIndex
        root.dragFromRow = -1
        root.dragPatchId = destination.patchId
        root.dragPatchName = destination.patchName
        root.dragActive = true
        updateDrag(wx, wy)
    }

    function updateDrag(wx, wy) {
        if (!root.dragActive)
            return
        var p = root.mapFromItem(null, wx, wy)
        root.dragPoint = p

        // 1. Destinations first: they always win over a navigation button.
        var hit = destinationAt(p.x, p.y)
        if (hit >= 0) {
            setDropSlot(hit)
            clearDwell()
            return
        }
        // 2. Sticky: keep the acquired destination until the pointer is
        //    clearly away from both its tile and its NUMBER button.
        if (root.dropSlot >= 0) {
            var index = root.dropSlot % 8
            var tileRect = rectOf(tiles.itemAt(index))
            var buttonRect = rectOf(numbers.itemAt(index))
            if (within(tileRect, p.x, p.y, root.stickyPad) || within(buttonRect, p.x, p.y, root.stickyPad)) {
                clearDwell()
                return
            }
            setDropSlot(-1)
        }
        // 3. Navigation by dwell.
        updateDwell(p.x, p.y)
    }

    function setDropSlot(slotIndex) {
        if (root.dropSlot === slotIndex)
            return
        root.dropSlot = slotIndex
        root.dropPreview = slotIndex >= 0
                ? root.builder.dropPreview(slotIndex, root.dragPatchId, root.dragFromSlot)
                : null
    }

    function updateDwell(x, y) {
        var bank = -1
        var subgroup = -1
        var i
        for (i = 0; i < 8; ++i) {
            if (within(rectOf(banks.itemAt(i)), x, y, 0)) {
                bank = i + 1
                subgroup = root.builder.subgroup
                break
            }
        }
        if (bank < 0) {
            for (i = 0; i < 2; ++i) {
                if (within(rectOf(subgroups.itemAt(i)), x, y, 0)) {
                    subgroup = i
                    bank = root.builder.bank
                    break
                }
            }
        }
        if (bank < 0) {
            // The overview map is a navigation control too, so a drag can be
            // carried to any of the sixteen banks without letting go.
            for (var s = 0; s < 2 && bank < 0; ++s) {
                for (var b = 1; b <= 8; ++b) {
                    if (within(rectOf(overview.blockAt(s, b)), x, y, 0)) {
                        subgroup = s
                        bank = b
                        break
                    }
                }
            }
        }

        if (bank < 0 || (bank === root.builder.bank && subgroup === root.builder.subgroup)) {
            clearDwell()
            return
        }
        if (root.hoverBank === bank && root.hoverSubgroup === subgroup)
            return
        root.hoverBank = bank
        root.hoverSubgroup = subgroup
        dwell.restart()
    }
    function clearDwell() {
        dwell.stop()
        root.hoverBank = -1
        root.hoverSubgroup = -1
    }

    // Stages a drag without a pointer, so a test or a documentation capture can
    // put the surface into a mid-drag state exactly as a real gesture would.
    // `fromSlot` >= 0 stages a move inside the bank; -1 stages a drag from the
    // source library.
    function stageDrag(patchId, patchName, slotIndex, fromSlot) {
        root.dragFromSlot = fromSlot === undefined ? -1 : fromSlot
        root.dragFromRow = root.dragFromSlot >= 0 ? -1 : 0
        if (root.dragFromSlot >= 0) {
            // A move inside the bank carries what is at the source, exactly as
            // beginDragFromSlot does, so a staged move can never show a Patch
            // the destination does not hold.
            var origin = root.builder.destinationAt(root.dragFromSlot)
            root.dragPatchId = origin.patchId
            root.dragPatchName = origin.patchName
        } else {
            root.dragPatchId = patchId
            root.dragPatchName = patchName
        }
        root.dragActive = true
        var tile = tiles.itemAt(slotIndex % 8)
        if (tile) {
            var centre = tile.mapToItem(root, tile.width / 2, tile.height / 2)
            root.dragPoint = Qt.point(centre.x, centre.y)
        }
        setDropSlot(slotIndex)
    }

    function commitDrag() {
        if (!root.dragActive)
            return
        var target = root.dropSlot
        var placed = false
        if (target >= 0) {
            placed = root.dragFromSlot >= 0
                    ? root.builder.moveSlot(root.dragFromSlot, target)
                    : root.builder.placePatch(target, root.dragPatchId)
        }
        if (placed) {
            root.flashSlot = target
            flashReset.restart()
        } else if (!Motion.reducedMotion) {
            // Nothing was dropped anywhere useful. The ghost has to outlive
            // the gesture to refuse visibly, so it keeps its content and its
            // position for the length of the shake instead of blinking out.
            root.refusing = true
            refusal.restart()
        }
        endDrag()
    }
    function endDrag() {
        root.refusedName = root.dragPatchName
        root.dragActive = false
        root.dragFromSlot = -1
        root.dragFromRow = -1
        root.dragPatchId = 0
        root.dragPatchName = ""
        setDropSlot(-1)
        clearDwell()
    }

    Timer {
        id: dwell
        interval: root.dwellMs
        onTriggered: {
            if (!root.dragActive || root.hoverBank < 0)
                return
            root.builder.selectSubgroup(root.hoverSubgroup)
            root.builder.selectBank(root.hoverBank)
            // The visible eight have changed underneath the pointer, so the
            // acquired destination is no longer meaningful.
            setDropSlot(-1)
            clearDwell()
        }
    }
    Timer {
        id: flashReset
        interval: 600
        onTriggered: root.flashSlot = -1
    }

    // ── Keyboard ─────────────────────────────────────────────────────────
    // The same panel, without the mouse. Arrow keys walk NUMBER and BANK, A
    // and B switch subgroup, Delete empties the selected destination.
    Keys.onPressed: function (event) {
        var handled = true
        if (event.matches(StandardKey.Undo)) {
            root.builder.undo()
        } else if (event.matches(StandardKey.Redo)) {
            root.builder.redo()
        } else if (event.key === Qt.Key_Left) {
            root.builder.selectNumber(root.builder.number - 1)
        } else if (event.key === Qt.Key_Right) {
            root.builder.selectNumber(root.builder.number + 1)
        } else if (event.key === Qt.Key_Up) {
            root.builder.selectBank(root.builder.bank - 1)
        } else if (event.key === Qt.Key_Down) {
            root.builder.selectBank(root.builder.bank + 1)
        } else if (event.key === Qt.Key_A) {
            root.builder.selectSubgroup(0)
        } else if (event.key === Qt.Key_B) {
            root.builder.selectSubgroup(1)
        } else if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
            root.builder.clearSlot(root.builder.currentSlotIndex)
        } else if (event.key >= Qt.Key_1 && event.key <= Qt.Key_8) {
            root.builder.selectNumber(event.key - Qt.Key_0)
        } else {
            handled = false
        }
        event.accepted = handled
    }

    // ── Layout ───────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.screenMargin(root.width)
        spacing: Metrics.spacingMd

        // Identity and bank-level actions ---------------------------------
        BankBuilderHeader {
            objectName: "bankBuilderHeader"
            Layout.fillWidth: true
            builder: root.builder
            transfer: root.transfer
            savedBanksOpen: banksDrawer.visible
            onExportRequested: exportDialog.open()
            onSavedBanksToggled: banksDrawer.visible = !banksDrawer.visible
            onNewBankRequested: root.builder.occupiedCount > 0 || root.builder.modified
                                ? discardConfirm.open()
                                : root.builder.newEmptyBank()
            onSaveAsRequested: {
                saveName.text = root.builder.bankName
                saveDialog.open()
                saveName.forceActiveFocus()
            }
        }

        // What the last import or export did. Kept until dismissed rather
        // than shown in a toast: an import can report duplicates, partial
        // Patches and rejected messages, and those are worth reading slowly.
        LibraryTransferCard {
            objectName: "bankTransferCard"
            Layout.fillWidth: true
            visible: root.transfer !== null && (root.transfer.busy || root.transfer.hasResult)
            transfer: root.transfer !== null ? root.transfer : null
        }

        // Source | Panel ---------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Metrics.spacingMd

            XpCard {
                Layout.preferredWidth: root.wide ? 320 : 260
                Layout.fillHeight: true
                padding: Metrics.spacingMd

                BankSourcePanel {
                    id: sourcePanel
                    objectName: "bankSourcePanel"
                    anchors.fill: parent
                    library: root.library
                    draggingRow: root.dragFromRow
                    canImport: root.transfer !== null
                    importBusy: root.transfer !== null && root.transfer.busy
                    onImportRequested: importDialog.open()
                    onFillRequested: function (digest) {
                        // The view model decides what can be arranged and says
                        // what it left out; the screen only asks.
                        root.builder.fillFromSource(digest)
                    }
                    onPatchDragStarted: function (row, patchId, patchName, x, y) {
                        root.beginDragFromLibrary(row, patchId, patchName, x, y)
                    }
                    onPatchDragMoved: function (x, y) { root.updateDrag(x, y) }
                    onPatchDragReleased: root.commitDrag()
                    onPatchDragCancelled: root.endDrag()
                }
            }

            // The instrument panel ----------------------------------------
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Metrics.spacingSm

                BankPanelDisplay {
                    objectName: "bankPanelDisplay"
                    Layout.fillWidth: true
                    builder: root.builder
                    preview: root.dropPreview
                }

                // What just happened, and the audition the hardware allows.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingSm

                    StatusPill {
                        objectName: "bankLastAction"
                        visible: root.builder.lastAction.length > 0
                        text: root.builder.lastAction
                        tone: root.builder.lastActionTone
                    }
                    Item { Layout.fillWidth: true }
                    XpButton {
                        objectName: "bankAudition"
                        text: qsTr("Audition")
                        iconName: "activity"
                        compact: true
                        variant: "ghost"
                        enabled: root.builder.canAudition
                        QQC.ToolTip.visible: hovered
                        QQC.ToolTip.delay: 400
                        QQC.ToolTip.text: root.builder.auditionMessage
                        onClicked: root.builder.auditionCurrent()
                    }
                    XpButton {
                        objectName: "bankClearSlot"
                        text: qsTr("Clear destination")
                        compact: true
                        variant: "ghost"
                        enabled: root.builder.currentOccupied
                        onClicked: root.builder.clearSlot(root.builder.currentSlotIndex)
                    }
                }

                // The panel surface. One raised plate carrying every control,
                // so SUBGROUP, BANK, NUMBER and the eight destinations read as
                // one instrument rather than four cards.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Metrics.radiusMd
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.borderStrong

                    // A hairline highlight along the top edge: the plate is
                    // lit from above, like the buttons standing on it.
                    Rectangle {
                        anchors { left: parent.left; right: parent.right; top: parent.top }
                        anchors.margins: 8
                        height: 1
                        color: Qt.rgba(1, 1, 1, 0.05)
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Metrics.spacingMd
                        spacing: Metrics.spacingXs

                        // SUBGROUP -------------------------------------------
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.minimumHeight: 34
                            Layout.preferredHeight: 34
                            Layout.maximumHeight: 34
                            spacing: Metrics.spacingSm

                            XpLabel {
                                Layout.preferredWidth: 68
                                text: qsTr("SUBGROUP")
                                role: "overline"
                                secondary: true
                            }
                            Repeater {
                                id: subgroups
                                model: 2
                                delegate: XpHardwareButton {
                                    required property int index
                                    objectName: "subgroup" + (index === 0 ? "A" : "B")
                                    text: index === 0 ? "A" : "B"
                                    caption: index === 0 ? "001–064" : "065–128"
                                    large: true
                                    selected: root.builder.subgroup === index
                                    fill: (index === 0 ? root.builder.subgroupOccupancyA
                                                       : root.builder.subgroupOccupancyB) / 64
                                    indicator: (index === 0 ? root.builder.subgroupOccupancyA
                                                            : root.builder.subgroupOccupancyB) > 0
                                    dropCandidate: root.dragActive && root.builder.subgroup !== index
                                    dropActive: root.hoverSubgroup === index && root.hoverBank >= 0
                                    onClicked: root.builder.selectSubgroup(index)
                                }
                            }
                            Item { Layout.fillWidth: true }
                            XpLabel {
                                visible: root.dragActive || root.refusing
                                text: qsTr("Hold over a BANK to move the panel there")
                                role: "caption"
                                color: Theme.accentText
                            }
                        }

                        // BANK -----------------------------------------------
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.minimumHeight: 34
                            Layout.preferredHeight: 34
                            Layout.maximumHeight: 34
                            spacing: Metrics.spacingSm

                            XpLabel {
                                Layout.preferredWidth: 68
                                text: qsTr("BANK")
                                role: "overline"
                                secondary: true
                            }
                            Repeater {
                                id: banks
                                model: 8
                                delegate: XpHardwareButton {
                                    required property int index
                                    objectName: "bankButton" + (index + 1)
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 34
                                    text: (index + 1).toString()
                                    caption: {
                                        var occupied = root.builder.bankOccupancy[index]
                                        return occupied > 0 ? occupied + "/8" : ""
                                    }
                                    // Occupancy remains visible through the
                                    // key LED and Bank Map. Avoid squeezing a
                                    // second text line into this shallow cap.
                                    showCaption: false
                                    selected: root.builder.bank === index + 1
                                    fill: (root.builder.bankOccupancy[index] || 0) / 8
                                    indicator: (root.builder.bankOccupancy[index] || 0) > 0
                                    dropCandidate: root.dragActive && root.builder.bank !== index + 1
                                    dropActive: root.hoverBank === index + 1
                                                && root.hoverSubgroup === root.builder.subgroup
                                    onClicked: root.builder.selectBank(index + 1)
                                }
                            }
                        }

                        // NUMBER ---------------------------------------------
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.minimumHeight: 34
                            Layout.preferredHeight: 34
                            Layout.maximumHeight: 34
                            spacing: Metrics.spacingSm

                            XpLabel {
                                Layout.preferredWidth: 68
                                text: qsTr("NUMBER")
                                role: "overline"
                                secondary: true
                            }
                            Repeater {
                                id: numbers
                                model: 8
                                delegate: XpHardwareButton {
                                    required property int index
                                    readonly property int slotIndex:
                                        root.builder.slotIndexFor(root.builder.subgroup, root.builder.bank, index + 1)
                                    objectName: "numberButton" + (index + 1)
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 34
                                    text: (index + 1).toString()
                                    selected: root.builder.number === index + 1
                                    indicator: root.builder.numberOccupancy[index] === true
                                    fill: root.builder.numberOccupancy[index] === true ? 1 : 0
                                    dropCandidate: root.dragActive
                                    dropActive: root.dropSlot === slotIndex
                                    onClicked: root.builder.selectNumber(index + 1)
                                }
                            }
                        }

                        // The eight destinations -----------------------------
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.minimumHeight: 112
                            Layout.preferredHeight: 144
                            Layout.maximumHeight: 144
                            spacing: Metrics.spacingSm

                            // Empty column keeping the tiles under the NUMBER
                            // buttons: the physical alignment is the thing that
                            // makes button and destination read as one control.
                            Item { Layout.preferredWidth: 68 }

                            Repeater {
                                id: tiles
                                model: root.builder.visibleDestinations
                                delegate: BankDestinationTile {
                                    required property var modelData
                                    required property int index
                                    objectName: "destination" + modelData.panelLabel
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.minimumHeight: 112
                                    Layout.maximumHeight: 144
                                    destination: modelData
                                    current: modelData.current === true
                                    dropCandidate: root.dragActive
                                    dropActive: root.dropSlot === modelData.slotIndex
                                    dropAction: root.dropPreview && root.dropSlot === modelData.slotIndex
                                                ? root.dropPreview.action : ""
                                    dragging: root.dragFromSlot === modelData.slotIndex
                                    flashing: root.flashSlot === modelData.slotIndex
                                    onClicked: root.builder.selectSlot(modelData.slotIndex)
                                    onAuditioned: root.builder.auditionSlot(modelData.slotIndex)
                                    onDragStarted: function (x, y) {
                                        root.beginDragFromSlot(modelData.slotIndex, x, y)
                                    }
                                    onDragMoved: function (x, y) { root.updateDrag(x, y) }
                                    onDragReleased: root.commitDrag()
                                    onDragCancelled: root.endDrag()
                                }
                            }
                        }

                        XpDivider { Layout.fillWidth: true }

                        // The whole bank, as a map ---------------------------
                        BankOverviewMap {
                            id: overview
                            objectName: "bankOverview"
                            Layout.fillWidth: true
                            Layout.preferredHeight: implicitHeight
                            builder: root.builder
                            hoverBank: root.hoverBank
                            hoverSubgroup: root.hoverSubgroup
                            onBankPicked: function (subgroup, bank) {
                                root.builder.selectSubgroup(subgroup)
                                root.builder.selectBank(bank)
                            }
                        }
                    }
                }
            }
        }
    }

    // ── The drag ghost ───────────────────────────────────────────────────
    // One item, following the pointer, saying what is being carried and where
    // it would land. It is above everything and takes no input.
    Item {
        anchors.fill: parent
        z: 100
        visible: root.dragActive

        Rectangle {
            id: ghost
            objectName: "bankDragGhost"
            x: Math.max(4, Math.min(root.width - width - 4, root.dragPoint.x + 16))
            y: Math.max(4, Math.min(root.height - height - 4, root.dragPoint.y - height / 2))
            width: ghostContent.implicitWidth + 2 * Metrics.spacingSm
            height: ghostContent.implicitHeight + 2 * Metrics.spacingXs
            radius: Metrics.radiusSm
            color: Theme.surfaceRaised
            border.width: 1
            border.color: root.refusing
                          ? Theme.warning
                          : (root.dropSlot >= 0
                             ? (root.dropPreview && root.dropPreview.action === "REPLACE"
                                ? Theme.warning : Theme.success)
                             : Theme.borderStrong)
            opacity: 0.98

            // A refused drop shakes rather than disappearing, so a miss is
            // visible instead of silent.
            SequentialAnimation {
                id: refusal
                running: false
                NumberAnimation { target: ghost; property: "rotation"; to: -4; duration: 60 }
                NumberAnimation { target: ghost; property: "rotation"; to: 4; duration: 60 }
                NumberAnimation { target: ghost; property: "rotation"; to: 0; duration: 60 }
                ScriptAction { script: root.refusing = false }
            }

            ColumnLayout {
                id: ghostContent
                anchors.centerIn: parent
                spacing: 1

                XpLabel {
                    text: root.dragActive ? root.dragPatchName : root.refusedName
                    role: "value"
                    font.weight: Typography.weightMedium
                    Layout.maximumWidth: 220
                    elide: Text.ElideRight
                }
                RowLayout {
                    spacing: Metrics.spacingXs
                    visible: root.dropSlot >= 0 && root.dropPreview !== null
                    XpLabel {
                        text: root.dropPreview ? root.dropPreview.action : ""
                        role: "overline"
                        color: root.dropPreview && root.dropPreview.action === "REPLACE"
                               ? Theme.warning : Theme.success
                    }
                    XpLabel {
                        text: root.dropPreview ? root.dropPreview.panelLabel : ""
                        role: "mono"
                        font.weight: Typography.weightBold
                    }
                    XpLabel {
                        text: root.dropPreview ? qsTr("PATCH %1").arg(root.dropPreview.linearLabel) : ""
                        role: "mono"
                        muted: true
                    }
                }
                XpLabel {
                    visible: root.dropSlot < 0
                    text: root.refusing ? qsTr("Not a destination — nothing was changed")
                                        : qsTr("Drop on a NUMBER destination")
                    role: "caption"
                    color: root.refusing ? Theme.warning : Theme.textMuted
                }
            }
        }
    }

    // ── Saved banks ──────────────────────────────────────────────────────
    XpCard {
        id: banksDrawer
        objectName: "bankSavedDrawer"
        visible: false
        z: 90
        raised: true
        width: 340
        anchors {
            right: parent.right
            rightMargin: Metrics.screenMargin(root.width)
            top: parent.top
            topMargin: 64
        }
        height: Math.min(root.height - 120, savedColumn.implicitHeight + 2 * Metrics.cardPadding)

        BankSavedList {
            id: savedColumn
            anchors.fill: parent
            builder: root.builder
            onBankOpened: banksDrawer.visible = false
            onCloseRequested: banksDrawer.visible = false
        }
    }

    // ── Small dialogs ────────────────────────────────────────────────────
    QQC.Dialog {
        id: saveDialog
        objectName: "bankSaveDialog"
        anchors.centerIn: parent
        modal: true
        title: qsTr("Save as new bank")
        standardButtons: QQC.Dialog.Save | QQC.Dialog.Cancel
        onAccepted: root.builder.saveAsNewBank(saveName.text)

        background: Rectangle {
            color: Theme.surface
            radius: Metrics.radiusMd
            border.width: 1
            border.color: Theme.borderStrong
        }

        ColumnLayout {
            spacing: Metrics.spacingSm
            XpLabel {
                text: qsTr("All 128 destinations are stored exactly as they are arranged, "
                           + "empty positions included. No Patch is copied or moved.")
                role: "caption"
                muted: true
                wrapMode: Text.WordWrap
                Layout.preferredWidth: 340
            }
            XpTextField {
                id: saveName
                objectName: "bankSaveName"
                Layout.fillWidth: true
                placeholderText: qsTr("Bank name")
                onAccepted: saveDialog.accept()
            }
        }
    }

    QQC.Dialog {
        id: discardConfirm
        objectName: "bankDiscardDialog"
        anchors.centerIn: parent
        modal: true
        title: qsTr("Start a new empty bank?")
        standardButtons: QQC.Dialog.Yes | QQC.Dialog.Cancel
        onAccepted: root.builder.newEmptyBank()

        background: Rectangle {
            color: Theme.surface
            radius: Metrics.radiusMd
            border.width: 1
            border.color: Theme.borderStrong
        }

        XpLabel {
            width: 340
            text: root.builder.modified
                  ? qsTr("This arrangement has unsaved changes. Starting a new bank clears all 128 "
                         + "destinations. Every Patch stays in the library.")
                  : qsTr("This clears all 128 destinations. Every Patch stays in the library.")
            role: "body"
            wrapMode: Text.WordWrap
        }
    }

    // ── Files ────────────────────────────────────────────────────────────
    // Importing here fills the source library this screen builds from; it is
    // the same import the Library screen runs, so a bank imported from either
    // place is one source bank in both.
    FileDialog {
        id: importDialog
        objectName: "bankImportDialog"
        title: qsTr("Import SysEx banks")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Roland SysEx (*.syx)"), qsTr("All files (*)")]
        onAccepted: if (root.transfer) root.transfer.importFiles(selectedFiles)
    }

    // Exporting writes the arrangement, not the library: each Patch is
    // addressed to the User slot it occupies here, and the empty destinations
    // contribute nothing at all rather than a blank Patch that would erase
    // whatever the instrument holds there.
    FileDialog {
        id: exportDialog
        objectName: "bankExportDialog"
        title: qsTr("Export this bank to SysEx")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "syx"
        nameFilters: [qsTr("Roland SysEx (*.syx)")]
        onAccepted: {
            if (!root.transfer)
                return
            root.transfer.exportBankArrangement(root.builder.arrangementIds(), selectedFile)
        }
    }
}
