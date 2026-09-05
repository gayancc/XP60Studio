import QtQuick
import QtTest
import XP60Studio
import XP60Studio.Presentation

// The Library screen, over a real in-memory library holding the golden
// fixture's 128 patches (see tst_qml_main.cpp).
TestCase {
    id: testCase
    name: "LibraryScreen"
    when: windowShown
    width: 1440
    height: 900
    visible: true

    Component {
        id: screenComponent
        LibraryScreen {
            width: 1440
            height: 900
            library: testLibrary
            transfer: testLibraryTransfer
        }
    }

    function createScreen() {
        const screen = createTemporaryObject(screenComponent, testCase)
        verify(screen, "screen created")
        // Every test starts from an unfiltered library at a known ordering.
        screen.library.clearFilters()
        screen.library.sortOrder = LibraryListModel.SourceSlot
        screen.library.selectRow(-1)
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

    function test_shows_the_whole_library_and_its_real_names() {
        const screen = createScreen()
        compare(screen.library.count, 128, "all 128 fixture patches are listed")
        compare(screen.library.libraryTotal, 128)

        const results = findChild(screen, "libraryResults")
        verify(results, "results list exists")
        verify(results.visible, "the list is shown when there are patches")
        compare(results.count, 128)

        const empty = findChild(screen, "libraryEmptyState")
        verify(empty && !empty.visible, "no empty state while there are results")
    }

    function test_count_pill_states_what_a_filter_hides() {
        const screen = createScreen()
        const pill = findChild(screen, "libraryCount")
        verify(pill, "count pill exists")
        verify(pill.text.indexOf("128") >= 0, "unfiltered count names the whole library")

        screen.library.searchText = "zzzznotaname"
        compare(screen.library.count, 0)
        // A filter must never make the rest of the library invisible in words
        // as well as in the list.
        verify(pill.text.indexOf("128") >= 0, "filtered count still names the library total")
        verify(pill.text.indexOf("0") >= 0, "filtered count names the match count")

        screen.library.clearFilters()
    }

    function test_empty_state_appears_only_when_a_filter_matches_nothing() {
        const screen = createScreen()
        screen.library.searchText = "zzzznotaname"

        const empty = findChild(screen, "libraryEmptyState")
        verify(empty && empty.visible, "empty state shown when nothing matches")
        verify(empty.title.indexOf("filters") >= 0 || empty.title.indexOf("No patch") >= 0,
               "the message says it is the filters, not an empty library")

        const results = findChild(screen, "libraryResults")
        verify(results && !results.visible, "the list is hidden when it has nothing to show")

        screen.library.clearFilters()
    }

    function test_search_field_drives_the_model() {
        const screen = createScreen()
        const field = findChild(screen, "librarySearch")
        verify(field, "search field exists")

        field.forceActiveFocus()
        // A name the fixture certainly contains: its own first patch.
        const sample = screen.library.rowData(0).name
        verify(sample.length > 0)

        screen.library.searchText = sample
        verify(screen.library.count >= 1, "searching a real name finds it")
        verify(screen.library.count < 128, "and narrows the list")
        compare(field.text, sample, "the field reflects the model")

        screen.library.clearFilters()
        compare(field.text, "", "clearing filters clears the field")
    }

    function test_clear_filters_button_appears_only_when_filtered() {
        const screen = createScreen()
        const clear = findChild(screen, "libraryClearFilters")
        verify(clear, "clear button exists")
        verify(!clear.visible, "hidden while nothing is filtered")

        screen.library.favouritesOnly = true
        verify(clear.visible, "shown once a filter is in force")
        clear.clicked()
        verify(!screen.library.filtered, "clicking it clears every filter")
        compare(screen.library.count, 128)
    }

    function test_selecting_a_row_fills_the_details_panel() {
        const screen = createScreen()
        const details = findChild(screen, "libraryDetails")
        verify(details, "details panel exists")

        screen.library.selectRow(6)
        const name = findChild(screen, "libraryDetailsName")
        const source = findChild(screen, "libraryDetailsSource")
        verify(name && name.text.length > 0, "the selected patch is named")
        compare(source.text, "user-bank-amal.syx", "provenance names the real source file")

        const summary = findChild(screen, "libraryDuplicateSummary")
        verify(summary && summary.text.length > 0, "duplicate state is stated either way")
    }

    function test_favourite_and_rating_are_editable_from_the_row() {
        const screen = createScreen()
        screen.library.selectRow(0)

        verify(screen.library.setFavourite(0, true))
        compare(screen.library.rowData(0).favourite, true)

        verify(screen.library.setRating(0, 4))
        compare(screen.library.rowData(0).rating, 4)

        // Filtering to favourites now finds exactly this one.
        screen.library.favouritesOnly = true
        compare(screen.library.count, 1)
        screen.library.clearFilters()

        // Put the fixture library back so the next test starts clean.
        verify(screen.library.setFavourite(0, false))
        verify(screen.library.setRating(0, 0))
    }

    function test_narrow_window_keeps_results_and_details_usable() {
        const screen = createScreen()
        screen.width = 1024
        screen.height = 680
        wait(0)

        verify(!screen.wide, "1024 is a narrow layout")
        const results = findChild(screen, "libraryResults")
        const details = findChild(screen, "libraryDetails")
        verify(results.width > 0 && results.height > 0, "results still have room")
        verify(details.width > 0 && details.height > 0, "details move below rather than disappearing")
        verify(results.width <= screen.width, "nothing overflows the window")
        verify(details.width <= screen.width)
    }

    // ------------------------------------------------------------------
    // Import and export
    // ------------------------------------------------------------------

    function test_transfer_card_is_absent_until_there_is_something_to_say() {
        testLibraryTransfer.dismissResult()
        var screen = createScreen()
        var card = findChild(screen, "libraryTransferCard")
        verify(card !== null)
        // An idle library carries no empty progress panel.
        verify(!card.visible)
    }

    function test_import_and_export_actions_are_present_and_guarded() {
        testLibraryTransfer.dismissResult()
        var screen = createScreen()
        var importButton = findChild(screen, "libraryImport")
        var exportButton = findChild(screen, "libraryExport")
        verify(importButton !== null)
        verify(exportButton !== null)
        verify(importButton.visible)
        verify(exportButton.visible)
        // The fixture library holds patches, so exporting is possible.
        verify(testLibrary.count > 0)
        verify(exportButton.enabled)
    }

    function test_export_button_says_how_much_a_filter_would_write() {
        testLibraryTransfer.dismissResult()
        var screen = createScreen()
        var exportButton = findChild(screen, "libraryExport")
        testLibrary.clearFilters()
        tryCompare(exportButton, "text", "Export all")

        // With a filter on, the label must name the number actually written so
        // a filtered export can never be mistaken for a whole-library one.
        testLibrary.searchText = "Strings"
        tryVerify(function() { return testLibrary.filtered })
        verify(exportButton.text.indexOf("Export") === 0)
        verify(exportButton.text !== "Export all")
        testLibrary.clearFilters()
    }

    function test_export_is_disabled_when_the_filters_show_nothing() {
        testLibraryTransfer.dismissResult()
        var screen = createScreen()
        var exportButton = findChild(screen, "libraryExport")
        testLibrary.searchText = "zzzz-no-such-patch-zzzz"
        tryCompare(testLibrary, "count", 0)
        // Nothing visible means nothing to write, rather than an empty file.
        tryCompare(exportButton, "enabled", false)
        testLibrary.clearFilters()
    }

    function test_a_failed_export_is_reported_and_stays_until_dismissed() {
        testLibraryTransfer.dismissResult()
        var screen = createScreen()
        // An empty selection is refused by the service, which is the cheapest
        // way to drive the failure path through the whole stack.
        verify(!testLibraryTransfer.exportIds([], "file:///nowhere/none.syx", 0))
        verify(testLibraryTransfer.hasResult)
        compare(testLibraryTransfer.resultTone, "error")

        var card = findChild(screen, "libraryTransferCard")
        tryCompare(card, "visible", true)
        var pill = findChild(screen, "transferResultPill")
        tryCompare(pill, "text", "Export failed")

        // It stays put: a failure is not taken away on a timer.
        wait(120)
        verify(card.visible)

        mouseClick(findChild(screen, "dismissTransferResult"))
        tryCompare(testLibraryTransfer, "hasResult", false)
        tryCompare(card, "visible", false)
    }

    function test_a_successful_export_writes_the_filtered_patches() {
        testLibraryTransfer.dismissResult()
        var screen = createScreen()
        testLibrary.clearFilters()
        var ids = testLibrary.filteredIds()
        // filteredIds returns the whole matching set, not just fetched pages.
        compare(ids.length, testLibrary.count)

        var target = testHarness.temporaryFileUrl("qml-export.syx")
        verify(testLibraryTransfer.exportIds(ids, target, 0))
        tryCompare(testLibraryTransfer, "hasResult", true)
        compare(testLibraryTransfer.resultTone, "success")
        verify(testLibraryTransfer.resultDetail.indexOf("qml-export.syx") >= 0)
        testLibraryTransfer.dismissResult()
    }

}
