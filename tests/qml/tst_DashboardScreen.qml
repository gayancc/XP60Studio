import QtQuick
import QtTest
import XP60Studio
import XP60Studio.Presentation

// The Dashboard / Command Center (mockup panel M1).
//
// These tests are mostly about restraint. The roadmap forbids fake Dashboard
// actions and AGENTS.md forbids inventing hardware facts, so what the screen
// must *not* claim matters as much as what it shows.
TestCase {
    id: testCase
    name: "DashboardScreen"
    when: windowShown
    width: 1280
    height: 800
    visible: true

    Component {
        id: screenComponent
        DashboardScreen {
            width: 1280
            height: 800
            shell: testShell
            editor: testEditor
            dashboard: testDashboard
            devices: testDevices
        }
    }

    function makeScreen() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen)
        waitForRendering(screen)
        return screen
    }

    // ── The current Patch ─────────────────────────────────────────────────

    function test_hero_shows_the_patch_the_instrument_is_holding() {
        var screen = makeScreen()
        compare(findChild(screen, "dashboardPatchName").text, testEditor.patchName)
        var where = findChild(screen, "dashboardPatchLocation")
        verify(where.text.indexOf(testEditor.locationText) >= 0)
    }

    function test_all_four_tones_are_shown_including_silent_ones() {
        var screen = makeScreen()
        // A Patch has four Tones. Hiding the silent ones would misrepresent
        // its shape, so every one is present whatever its state.
        for (var i = 1; i <= 4; ++i) {
            var card = findChild(screen, "dashboardTone" + i)
            verify(card, "Tone " + i + " is on the Dashboard")
            compare(card.toneNumber, i)
        }
    }

    function test_a_tone_level_is_shown_as_the_xp_value_not_as_decibels() {
        var screen = makeScreen()
        var card = findChild(screen, "dashboardTone1")
        var value = findChild(card, "toneLevelValue")
        // The manual gives no raw-to-dB conversion, so a dB figure here would
        // be invented. The XP value is what the instrument actually stores.
        verify(value.text.indexOf("dB") < 0, "no decibel figure is claimed")
        compare(value.text, testEditor.tones[0].levelText)
    }

    function test_clicking_a_tone_opens_the_editor_focused_on_it() {
        var screen = makeScreen()
        testShell.currentScreen = "dashboard"
        mouseClick(findChild(screen, "dashboardTone3"))
        compare(testEditor.selectedTone, 3)
        compare(testShell.currentScreen, "editor")
        testShell.currentScreen = "dashboard"
    }

    // ── Quick actions ─────────────────────────────────────────────────────

    function test_every_quick_action_goes_somewhere_that_exists() {
        var screen = makeScreen()
        var names = ["quickEditPatch", "quickCompare", "quickBrowseWaves", "quickDevices"]
        for (var i = 0; i < names.length; ++i) {
            var button = findChild(screen, names[i])
            verify(button, names[i] + " is present")
            verify(button.enabled, names[i] + " is usable, not a placeholder")
        }
    }

    function test_edit_patch_navigates_to_the_editor() {
        var screen = makeScreen()
        testShell.currentScreen = "dashboard"
        mouseClick(findChild(screen, "quickEditPatch"))
        compare(testShell.currentScreen, "editor")
        testShell.currentScreen = "dashboard"
    }

    function test_compare_opens_the_editor_already_comparing() {
        var screen = makeScreen()
        testShell.currentScreen = "dashboard"
        testEditor.comparing = false
        mouseClick(findChild(screen, "quickCompare"))
        compare(testEditor.comparing, true)
        compare(testShell.currentScreen, "editor")
        testEditor.comparing = false
        testShell.currentScreen = "dashboard"
    }

    // ── What the Dashboard refuses to claim ───────────────────────────────

    function test_the_library_card_counts_what_exists_and_admits_what_it_cannot() {
        var screen = makeScreen()
        var card = findChild(screen, "libraryCard")
        verify(card.available)
        // Patch and wave counts are real. Which expansion boards are fitted is
        // not established, so the card shows a dash rather than a number.
        compare(card.metrics[0].value, testDashboard.libraryPatchCount)
        compare(card.metrics[1].value, testDashboard.waveCount)
        compare(card.metrics[2].value, "—")
        compare(testDashboard.expansionCountKnown, false)
    }

    function test_the_bank_card_opens_the_implemented_builder_without_inventing_counts() {
        var screen = makeScreen()
        var card = findChild(screen, "banksCard")
        compare(testDashboard.banksAvailable, true)
        verify(card.available)
        verify(card.interactive)
        compare(card.metrics.length, 0)
        card.activated()
        compare(testShell.currentScreen, "banks")
    }

    function test_the_device_card_reports_the_real_connection() {
        var screen = makeScreen()
        var card = findChild(screen, "deviceCard")
        verify(card.available)
        compare(card.headline, testShell.connectionLabel)
        verify(card.interactive)
    }

    function test_the_wave_count_is_the_catalog_and_not_a_round_number() {
        // 448 is what the generated catalog holds; the mockup's figures are
        // illustration.
        compare(testDashboard.waveCount, 448)
    }
}
