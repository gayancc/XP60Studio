import QtQuick
import QtTest
import XP60Studio
import XP60Studio.Presentation

// The Virtual XP-60 patch panel, over a real in-memory library holding the
// golden fixture's 128 patches (see tst_qml_main.cpp).
//
// What is checked here is the part a musician actually operates: that pressing
// SUBGROUP / BANK / NUMBER moves the panel to the destination those controls
// name, that the eight visible destinations are the right eight, that a drag
// says what it will do before it does it, and that the physical identity and
// the linear 001-128 identity never disagree.
TestCase {
    id: testCase
    name: "BankBuilderScreen"
    when: windowShown
    width: 1440
    height: 900
    visible: true

    Component {
        id: screenComponent
        BankBuilderScreen {
            width: 1440
            height: 900
            builder: testBankBuilder
            library: testBankLibrary
        }
    }

    function createScreen() {
        const screen = createTemporaryObject(screenComponent, testCase)
        verify(screen, "screen created")
        screen.library.clearFilters()
        screen.library.sortOrder = LibraryListModel.SourceSlot
        // Every test starts from an empty bank at A · BANK 1 · 1.
        screen.builder.newEmptyBank("Test Bank")
        return screen
    }

    function findChild(root, objectName) {
        if (root.objectName === objectName)
            return root
        for (let i = 0; i < root.children.length; ++i) {
            const found = findChild(root.children[i], objectName)
            if (found)
                return found
        }
        return null
    }

    // The first library id, so a test can place a real Patch.
    function firstPatch(screen) {
        const row = screen.library.rowData(0)
        verify(row.id !== undefined, "the library has a row to drag")
        return row
    }

    function verifySelectionSurfaces(screen, subgroup, bank, number, panel, linear) {
        compare(screen.builder.subgroup, subgroup)
        compare(screen.builder.bank, bank)
        compare(screen.builder.number, number)
        compare(findChild(screen, "bankDisplayPanelLabel").text, panel)
        compare(findChild(screen, "bankDisplayLinearLabel").text, "PATCH " + linear)
        verify(findChild(screen, subgroup === 0 ? "subgroupA" : "subgroupB").selected)
        verify(findChild(screen, "bankButton" + bank).selected)
        verify(findChild(screen, "numberButton" + number).selected)
        verify(findChild(screen, "destination" + panel).current)
        verify(screen.builder.overview[subgroup * 8 + bank - 1].current)
    }

    // ── The panel ────────────────────────────────────────────────────────

    function test_panel_selection_names_the_destination() {
        const screen = createScreen()
        const builder = screen.builder

        compare(builder.panelLabel, "A11")
        compare(builder.linearLabel, "001")
        compare(builder.currentSlotIndex, 0)

        // The example the product brief uses: A, BANK 3, NUMBER 5 is Patch 021.
        builder.selectSubgroup(0)
        builder.selectBank(3)
        builder.selectNumber(5)
        compare(builder.panelLabel, "A35")
        compare(builder.linearLabel, "021")
        compare(builder.currentSlotIndex, 20)
        compare(builder.spokenLabel, "A · BANK 3 · 5")

        // Subgroup B starts at 065 and ends at 128.
        builder.selectSubgroup(1)
        builder.selectBank(1)
        builder.selectNumber(1)
        compare(builder.panelLabel, "B11")
        compare(builder.linearLabel, "065")
        builder.selectBank(8)
        builder.selectNumber(8)
        compare(builder.panelLabel, "B88")
        compare(builder.linearLabel, "128")
    }

    function test_the_display_shows_both_identities() {
        const screen = createScreen()
        screen.builder.selectBank(3)
        screen.builder.selectNumber(5)

        compare(findChild(screen, "bankDisplayPanelLabel").text, "A35")
        compare(findChild(screen, "bankDisplayLinearLabel").text, "PATCH 021")
        compare(findChild(screen, "bankDisplayState").text, "EMPTY")
    }

    function test_lcd_selectors_slots_and_map_share_one_live_destination() {
        const screen = createScreen()

        // Hardware selectors drive the LCD, visible destinations and map.
        findChild(screen, "subgroupB").clicked()
        findChild(screen, "bankButton3").clicked()
        findChild(screen, "numberButton4").clicked()
        verifySelectionSurfaces(screen, 1, 3, 4, "B34", "084")

        // Assigning into that destination changes the same live LCD; there is
        // no display-only copy of the Patch name or state.
        const first = screen.library.rowData(0)
        verify(screen.builder.placePatchAtCurrent(first.id))
        compare(findChild(screen, "bankDisplayPatchName").text, first.name)
        compare(findChild(screen, "bankDisplayState").text, "ASSIGNED")

        // A destination tile is another route into the same selection.
        findChild(screen, "destinationB35").clicked()
        verifySelectionSurfaces(screen, 1, 3, 5, "B35", "085")

        // The overview moves subgroup and bank while preserving NUMBER.
        findChild(screen, "bankOverview").bankPicked(0, 2)
        verifySelectionSurfaces(screen, 0, 2, 5, "A25", "013")

        // A swap selects its target and the LCD immediately follows the Patch
        // now occupying that target; clearing it immediately returns to EMPTY.
        const second = screen.library.rowData(1)
        verify(screen.builder.placePatch(12, first.id))   // A25
        verify(screen.builder.placePatch(13, second.id))  // A26
        verify(screen.builder.moveSlot(12, 13))
        verifySelectionSurfaces(screen, 0, 2, 6, "A26", "014")
        compare(findChild(screen, "bankDisplayPatchName").text, first.name)
        verify(screen.builder.clearSlot(screen.builder.currentSlotIndex))
        compare(findChild(screen, "bankDisplayPatchName").text, "-- EMPTY DESTINATION --")
        compare(findChild(screen, "bankDisplayState").text, "EMPTY")
    }

    function test_the_visible_eight_belong_to_the_selected_bank() {
        const screen = createScreen()
        screen.builder.selectSubgroup(1)
        screen.builder.selectBank(3)

        const visible = screen.builder.visibleDestinations
        compare(visible.length, 8)
        compare(visible[0].panelLabel, "B31")
        compare(visible[0].linearLabel, "081")
        compare(visible[7].panelLabel, "B38")
        compare(visible[7].linearLabel, "088")
        // And the tiles on screen are those eight.
        verify(findChild(screen, "destinationB31"), "B31 is on screen")
        verify(findChild(screen, "destinationB38"), "B38 is on screen")
        verify(!findChild(screen, "destinationB41"), "the next bank is not")
    }

    function test_pressing_a_number_button_selects_that_destination() {
        const screen = createScreen()
        screen.builder.selectBank(4)

        const button = findChild(screen, "numberButton6")
        verify(button, "NUMBER 6 exists")
        button.clicked()
        compare(screen.builder.number, 6)
        compare(screen.builder.panelLabel, "A46")
        compare(screen.builder.linearLabel, "030")
    }

    function test_selector_keys_keep_hardware_proportions() {
        const screen = createScreen()
        const bankOne = findChild(screen, "bankButton1")
        const bankEight = findChild(screen, "bankButton8")
        const numberOne = findChild(screen, "numberButton1")

        verify(bankOne.width > bankOne.height * 2,
               "BANK keys are broad, low-profile hardware switches")
        verify(Math.abs(bankOne.width - bankEight.width) <= 1,
               "equal Layout cells may differ by one physical pixel after rounding")
        verify(Math.abs(bankOne.width - numberOne.width) <= 1,
               "equal Layout cells may differ by one physical pixel after rounding")
        verify(findChild(screen, "subgroupA").width > 80,
               "SUBGROUP has room for its range caption")
    }

    function test_destination_strip_stays_compact() {
        const screen = createScreen()
        verify(waitForRendering(screen), "compact strip completed layout")
        const destination = findChild(screen, "destinationA11")

        verify(destination.height >= 112,
               "destination remains a usable drag and drop target")
        verify(destination.height <= 144,
               "destination strip does not expand into tall cards")
    }

    function test_minimum_width_keeps_all_destinations_reachable_and_moves_source_to_overlay() {
        const screen = createScreen()
        screen.width = 800
        screen.height = 620
        verify(waitForRendering(screen), "minimum-width bank completed layout")

        const last = findChild(screen, "destinationA18")
        const lastPosition = last.mapToItem(screen, 0, 0)
        let geometry = ""
        let item = last
        for (let depth = 0; item !== null && depth < 7; ++depth) {
            geometry += " " + (item.objectName || item.toString())
                      + "[x=" + item.x + ",w=" + item.width + "]"
            item = item.parent
        }
        verify(lastPosition.x + last.width <= screen.width,
               "all eight destination columns stay inside the compact viewport "
               + "(right=" + (lastPosition.x + last.width)
               + ", viewport=" + screen.width + ";" + geometry + ")")

        const sourceButton = findChild(screen, "compactSourceButton")
        verify(sourceButton.visible)
        sourceButton.clicked()
        verify(findChild(screen, "compactSourceOverlay").visible,
               "the full source workflow remains available as an overlay")
    }

    // ── Placement ────────────────────────────────────────────────────────

    function test_placing_a_patch_leaves_the_source_alone() {
        const screen = createScreen()
        const before = screen.library.libraryTotal
        const row = firstPatch(screen)

        verify(screen.builder.placePatch(20, row.id), "the Patch is placed at A35")
        compare(screen.builder.occupiedCount, 1)
        compare(screen.builder.panelLabel, "A35")
        compare(screen.builder.currentPatchName, row.name)
        compare(screen.builder.currentState, "ASSIGNED")
        // The library is untouched: a bank is an arrangement, not a copy.
        compare(screen.library.libraryTotal, before)

        // The same Patch can fill a second destination.
        verify(screen.builder.placePatch(100, row.id))
        compare(screen.builder.occupiedCount, 2)
        compare(screen.library.libraryTotal, before)
    }

    function test_a_drag_says_what_it_will_do_before_it_does_it() {
        const screen = createScreen()
        const row = firstPatch(screen)
        screen.builder.selectBank(3)

        // Carrying a Patch over an empty destination.
        screen.stageDrag(row.id, row.name, 20, -1)
        verify(screen.dragActive)
        compare(screen.dropSlot, 20)
        compare(screen.dropPreview.action, "PLACE")
        compare(screen.dropPreview.panelLabel, "A35")
        compare(screen.dropPreview.linearLabel, "021")
        // The display previews the destination rather than the selection.
        compare(findChild(screen, "bankDisplayPanelLabel").text, "A35")
        compare(findChild(screen, "bankDisplayState").text, "PLACE")

        screen.commitDrag()
        verify(!screen.dragActive)
        compare(screen.builder.occupiedCount, 1)
        compare(screen.builder.destinationAt(20).patchName, row.name)
    }

    function test_dropping_on_an_occupied_destination_announces_replace() {
        const screen = createScreen()
        const first = screen.library.rowData(0)
        const second = screen.library.rowData(1)
        verify(screen.builder.placePatch(20, first.id))

        screen.builder.selectBank(3)
        screen.stageDrag(second.id, second.name, 20, -1)
        compare(screen.dropPreview.action, "REPLACE")
        compare(screen.dropPreview.occupant, first.name)
        compare(findChild(screen, "bankDisplayState").text, "REPLACE")

        screen.commitDrag()
        // Still one destination filled, now holding the second Patch.
        compare(screen.builder.occupiedCount, 1)
        compare(screen.builder.destinationAt(20).patchName, second.name)
    }

    function test_a_drop_that_lands_nowhere_changes_nothing() {
        const screen = createScreen()
        const row = firstPatch(screen)

        screen.stageDrag(row.id, row.name, 20, -1)
        screen.setDropSlot(-1)
        screen.commitDrag()
        compare(screen.builder.occupiedCount, 0)
        verify(!screen.dragActive)
    }

    // ── Rearranging inside the bank ──────────────────────────────────────

    function test_moving_and_swapping_inside_the_bank() {
        const screen = createScreen()
        const first = screen.library.rowData(0)
        const second = screen.library.rowData(1)
        verify(screen.builder.placePatch(0, first.id))   // A11
        verify(screen.builder.placePatch(9, second.id))  // A22

        // Into an empty destination: a move.
        screen.builder.selectSubgroup(0)
        screen.builder.selectBank(1)
        screen.stageDrag(first.id, first.name, 7, 0)     // A11 -> A18
        compare(screen.dropPreview.action, "MOVE")
        screen.commitDrag()
        compare(screen.builder.destinationAt(0).occupied, false)
        compare(screen.builder.destinationAt(7).patchName, first.name)
        compare(screen.builder.occupiedCount, 2)

        // Onto an occupied one: a swap, and nothing is lost.
        screen.builder.selectBank(2)
        screen.stageDrag(second.id, second.name, 9, 7)
        compare(screen.dropPreview.action, "SWAP")
        screen.commitDrag()
        compare(screen.builder.destinationAt(9).patchName, first.name)
        compare(screen.builder.destinationAt(7).patchName, second.name)
        compare(screen.builder.occupiedCount, 2)
    }

    function test_undo_and_redo_walk_the_arrangement_back() {
        const screen = createScreen()
        const row = firstPatch(screen)
        verify(screen.builder.placePatch(20, row.id))
        verify(screen.builder.canUndo)
        compare(screen.builder.undoLabel, "Place " + row.name + " at A35")

        verify(screen.builder.undo())
        compare(screen.builder.occupiedCount, 0)
        verify(screen.builder.canRedo)
        verify(screen.builder.redo())
        compare(screen.builder.occupiedCount, 1)
        compare(screen.builder.destinationAt(20).patchName, row.name)
    }

    function test_clearing_a_destination_keeps_the_patch_in_the_library() {
        const screen = createScreen()
        const before = screen.library.libraryTotal
        const row = firstPatch(screen)
        verify(screen.builder.placePatch(20, row.id))

        verify(screen.builder.clearSlot(20))
        compare(screen.builder.occupiedCount, 0)
        compare(screen.builder.currentState, "EMPTY")
        compare(screen.library.libraryTotal, before)
    }

    // ── The overview ─────────────────────────────────────────────────────

    function test_the_overview_maps_all_sixteen_banks_and_navigates() {
        const screen = createScreen()
        const overview = screen.builder.overview
        compare(overview.length, 16)
        compare(overview[0].label, "A1")
        compare(overview[0].first, "001")
        compare(overview[0].last, "008")
        compare(overview[8].label, "B1")
        compare(overview[8].first, "065")
        compare(overview[15].label, "B8")
        compare(overview[15].last, "128")

        const map = findChild(screen, "bankOverview")
        verify(map, "the bank map is on screen")
        map.bankPicked(1, 5)
        compare(screen.builder.subgroup, 1)
        compare(screen.builder.bank, 5)
        compare(screen.builder.visibleDestinations[0].linearLabel, "097")
    }

    function test_occupancy_is_reported_per_bank_and_per_subgroup() {
        const screen = createScreen()
        for (let i = 0; i < 8; ++i)
            verify(screen.builder.placePatch(16 + i, screen.library.rowData(i).id)) // A3x

        compare(screen.builder.subgroupOccupancyA, 8)
        compare(screen.builder.subgroupOccupancyB, 0)
        compare(screen.builder.bankOccupancy[2], 8)
        compare(screen.builder.bankOccupancy[0], 0)
        compare(screen.builder.overview[2].occupied, 8)
    }

    // ── Saving ───────────────────────────────────────────────────────────

    function test_save_as_new_bank_preserves_all_128_positions() {
        const screen = createScreen()
        const first = screen.library.rowData(0)
        const last = screen.library.rowData(3)
        verify(screen.builder.placePatch(0, first.id))    // A11 = 001
        verify(screen.builder.placePatch(127, last.id))   // B88 = 128
        verify(screen.builder.modified)

        const name = "Live Band " + Date.now()
        verify(screen.builder.saveAsNewBank(name), screen.builder.lastAction)
        verify(!screen.builder.modified, "saving clears the modified state")
        verify(screen.builder.savedBefore)

        let saved = null
        for (const bank of screen.builder.savedBanks) {
            if (bank.name === name)
                saved = bank
        }
        verify(saved, "the saved bank is listed")
        compare(saved.occupied, 2)

        // Start again, then reopen it: the two Patches are at exactly the
        // destinations they were placed at, and everything else is empty.
        screen.builder.newEmptyBank("Scratch")
        compare(screen.builder.occupiedCount, 0)
        verify(screen.builder.loadBank(saved.id))
        compare(screen.builder.bankName, name)
        compare(screen.builder.occupiedCount, 2)
        compare(screen.builder.destinationAt(0).patchName, first.name)
        compare(screen.builder.destinationAt(0).panelLabel, "A11")
        compare(screen.builder.destinationAt(0).linearLabel, "001")
        compare(screen.builder.destinationAt(127).patchName, last.name)
        compare(screen.builder.destinationAt(127).panelLabel, "B88")
        compare(screen.builder.destinationAt(127).linearLabel, "128")
        compare(screen.builder.arrangementIds().length, 128)

        verify(screen.builder.deleteBank(saved.id))
    }

    function test_new_empty_bank_starts_with_128_empty_destinations() {
        const screen = createScreen()
        verify(screen.builder.placePatch(20, firstPatch(screen).id))
        screen.builder.newEmptyBank("Fresh")

        compare(screen.builder.bankName, "Fresh")
        compare(screen.builder.occupiedCount, 0)
        compare(screen.builder.emptyCount, 128)
        compare(screen.builder.slotCount, 128)
        compare(screen.builder.panelLabel, "A11")
        verify(!screen.builder.modified)
        verify(!screen.builder.canUndo)
    }

    // ── Source library ───────────────────────────────────────────────────

    function test_a_source_bank_can_be_opened_on_its_own() {
        const screen = createScreen()
        const sources = screen.library.sourcesInUse
        compare(sources.length, 1, "the fixture is one source")
        compare(sources[0].patchCount, 128)

        screen.library.sourceDigest = sources[0].digest
        compare(screen.library.count, 128)
        verify(screen.library.filtered)

        screen.library.sourceDigest = "not-a-real-digest"
        compare(screen.library.count, 0, "an unknown source matches nothing")
        screen.library.clearFilters()
        compare(screen.library.count, 128)
    }

    // Arranging a bank from an import is the alternative to carrying 128
    // Patches across by hand. The panel offers it only with one source open,
    // because "fill from all sources" has no arrangement to reproduce.
    function test_arranging_a_bank_from_the_open_source() {
        const screen = createScreen()
        const fill = findChild(screen, "bankSourceFill")
        verify(fill, "the fill action exists")
        verify(!fill.visible, "not offered while all sources are shown")

        const sources = screen.library.sourcesInUse
        screen.library.sourceDigest = sources[0].digest
        verify(fill.visible)

        fill.clicked()
        // The fixture is a whole User bank read from USER:001..128, so it
        // comes back arranged exactly as it arrived.
        compare(screen.builder.occupiedCount, 128)
        compare(screen.builder.destinationAt(0).sourceSlot, "USER:001")
        compare(screen.builder.destinationAt(127).sourceSlot, "USER:128")
        compare(screen.builder.destinationAt(0).panelLabel, "A11")
        verify(screen.builder.modified)

        // One undo step, not 128.
        verify(screen.builder.undo())
        compare(screen.builder.occupiedCount, 0)
        verify(!screen.builder.canUndo)
    }

    // A destination the source claims is filled with what the source says
    // belongs there. Nothing is lost either way: the Patch that was standing
    // in the way is still in the library, and one undo puts it back.
    function test_arranging_from_a_source_replaces_the_destinations_it_claims() {
        const screen = createScreen()
        const other = screen.library.rowData(5)
        verify(other.id !== undefined)
        verify(screen.builder.placePatch(0, other.id))
        compare(screen.builder.patchIdAt(0), other.id)

        screen.library.sourceDigest = screen.library.sourcesInUse[0].digest
        findChild(screen, "bankSourceFill").clicked()
        compare(screen.builder.occupiedCount, 128)
        compare(screen.builder.destinationAt(0).sourceSlot, "USER:001")
        verify(screen.builder.patchIdAt(0) !== other.id)

        verify(screen.builder.undo())
        compare(screen.builder.occupiedCount, 1)
        compare(screen.builder.patchIdAt(0), other.id)
    }

    // The export action is about the bank as a whole, so it is off until the
    // bank has something in it. Without a transfer view model the screen is
    // arrange-and-save only, exactly as it was before files were involved.
    function test_export_is_offered_only_for_a_bank_with_patches_in_it() {
        const screen = createScreen()
        const exportButton = findChild(screen, "bankExport")
        verify(exportButton, "the export action exists")
        verify(!exportButton.visible, "hidden without a transfer view model")

        compare(screen.transfer, null)
        compare(screen.builder.occupiedCount, 0)
        compare(screen.builder.arrangementIds().length, 128)

        verify(screen.builder.placePatch(20, firstPatch(screen).id))
        // The arrangement an export would be handed: the ids in slot order,
        // empty destinations as 0.
        const ids = screen.builder.arrangementIds()
        compare(ids.length, 128)
        compare(ids[0], 0)
        verify(ids[20] > 0)
    }
}
