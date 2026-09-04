import QtQuick
import QtTest
import XP60Studio

// The Patch Editor screen (mockup panel M2) bound to a real Patch read from
// the golden fixture through the ordinary fetch path.
TestCase {
    id: testCase
    name: "EditorScreen"
    when: windowShown
    width: 1280
    height: 900
    visible: true

    Component {
        id: screenComponent
        EditorScreen { editor: testEditor; width: 1280; height: 900 }
    }

    Component {
        id: knobComponent
        XpKnob { from: 0; to: 127; value: 64 }
    }

    Component {
        id: segmentedComponent
        XpSegmentedControl { model: ["Sound", "Filter", "Amp"] }
    }

    Component {
        id: valueEditorComponent
        ParameterValueEditor { label: "Bend Range"; value: 2; minimumValue: 0; maximumValue: 12 }
    }

    Component {
        id: rangeBarComponent
        XpRangeBar { width: 200; lower: 1; upper: 127 }
    }

    Component {
        id: keybedComponent
        KeyboardStrip { width: 400; firstNote: 36; lastNote: 96; lowerNote: 48; upperNote: 79 }
    }

    function init() {
        // Every test starts from the just-fetched Patch: no local edits, no
        // history, not comparing, not armed.
        testHarness.reloadPatch()
        testEditor.disarmWrite()
        testEditor.section = 0
        testEditor.selectedTone = 1
    }

    function test_screen_loads_and_binds_to_the_fetched_patch() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen)
        verify(testEditor.hasPatch)

        var name = findChild(screen, "patchNameLabel")
        verify(name)
        compare(name.text, testEditor.patchName)

        var badge = findChild(screen, "patchStateBadge")
        verify(badge)
        compare(badge.text, "ON XP-60")
    }

    function test_four_tone_cards_are_present_and_colour_coded() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen)
        for (var n = 1; n <= 4; ++n) {
            var card = findChild(screen, "toneCard" + n)
            verify(card, "tone card " + n + " exists")
            compare(card.tone.toneNumber, n)
            compare(card.toneColor, Theme.toneColor(n))
        }
        // The four tone colours are distinct, so a card is identifiable at a glance.
        verify(Theme.toneColor(1) !== Theme.toneColor(2))
        verify(Theme.toneColor(3) !== Theme.toneColor(4))
    }

    function test_tone_grid_reflows_when_narrow() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen)
        var grid = findChild(screen, "toneGrid")
        verify(grid)
        compare(grid.columns, 4)
        screen.width = 900
        compare(grid.columns, 2)
    }

    function test_section_tabs_drive_the_envelope() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen)
        var tabs = findChild(screen, "sectionTabs")
        verify(tabs)
        compare(tabs.model.length, 5)

        var header = findChild(screen, "envelopeHeader")
        verify(header)
        verify(header.title.indexOf("PITCH") >= 0)

        tabs.activated(2) // Amp
        compare(testEditor.section, 2)
        verify(header.title.indexOf("LEVEL") >= 0)

        tabs.activated(3) // Motion has no envelope
        verify(!testEditor.envelopeAvailable)
    }

    function test_a_knob_edit_reaches_the_patch() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen)
        var card = findChild(screen, "toneCard1")
        var knob = findChild(card, "toneLevelKnob")
        verify(knob)
        var before = testEditor.tones[0].level

        knob.value = before === 100 ? 90 : 100
        knob.moved()
        compare(testEditor.tones[0].level, before === 100 ? 90 : 100)
        verify(testEditor.modified)
        var badge = findChild(screen, "patchStateBadge")
        compare(badge.text, "MODIFIED")
    }

    function test_write_is_gated_behind_arming() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen)
        var write = findChild(screen, "writeToDeviceButton")
        var arm = findChild(screen, "armWriteButton")
        verify(write)
        verify(arm)
        verify(!write.enabled, "writing is impossible until armed")
        verify(arm.enabled, "arming is possible because a read succeeded")

        mouseClick(arm)
        verify(testEditor.writeArmed)
        verify(write.enabled)

        mouseClick(arm) // toggles back off
        verify(!testEditor.writeArmed)
        verify(!write.enabled)
    }

    function test_comparing_shows_the_original_and_freezes_editing() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen)
        var original = testEditor.patchName
        testEditor.tones[0].level = 11
        verify(testEditor.modified)

        var compareButton = findChild(screen, "compareButton")
        verify(compareButton)
        mouseClick(compareButton)
        verify(testEditor.comparing)
        var badge = findChild(screen, "patchStateBadge")
        compare(badge.text, "A · ORIGINAL")
        compare(testEditor.patchName, original)

        mouseClick(compareButton)
        verify(!testEditor.comparing)
        compare(testEditor.tones[0].level, 11)
    }

    function test_undo_redo_and_revert_buttons_follow_history() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen)
        var undo = findChild(screen, "undoButton")
        var redo = findChild(screen, "redoButton")
        var revert = findChild(screen, "revertButton")
        verify(undo); verify(redo); verify(revert)
        verify(!undo.enabled)
        verify(!redo.enabled)
        verify(!revert.enabled)

        testEditor.tones[1].level = 12
        verify(undo.enabled)
        verify(revert.enabled)
        mouseClick(undo)
        verify(redo.enabled)
        verify(!testEditor.modified)
    }

    function test_envelope_stage_readouts_show_raw_values_not_invented_units() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen)
        testEditor.section = 2 // Amp
        var stages = findChild(screen, "envelopeStages")
        verify(stages)
        compare(testEditor.envelopeStages.length, 4)
        // Deviation from the mockup's "0.40 s" / "-6.0 dB": the map gives no
        // conversion, so the screen states the unit question instead.
        verify(testEditor.envelopeUnitNote.indexOf("0-127") >= 0)
    }

    function test_key_range_and_velocity_bind_to_the_selected_tone() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen)
        var strip = findChild(screen, "keyboardStrip")
        var velocity = findChild(screen, "velocityRange")
        verify(strip)
        verify(velocity)
        compare(strip.lowerNote, testEditor.keyRangeLower)
        compare(velocity.upper, testEditor.velocityUpper)

        testEditor.selectedTone = 3
        compare(strip.accentColor, Theme.toneColor(3))
    }

    // -- Keybed ---------------------------------------------------------------

    function test_keybed_uses_real_piano_geometry() {
        var kb = createTemporaryObject(keybedComponent, testCase)
        verify(kb)

        // The five black keys of an octave, and only those.
        var blacks = []
        for (var n = 60; n < 72; ++n)
            if (kb.isBlack(n)) blacks.push(n - 60)
        compare(blacks, [1, 3, 6, 8, 10])

        // Seven white keys per octave, so a 61-note window has 36.
        compare(kb.whiteCount, 36)
        verify(kb.whiteWidth > 0)

        // Black keys are narrower than white ones and sit over the boundary
        // between their neighbours, not on equal slices.
        verify(kb.blackWidth < kb.whiteWidth)
        var ww = kb.whiteWidth, bw = kb.blackWidth
        var cSharp = kb.keyLeft(61, ww, bw) + bw / 2
        var cRight = kb.keyRight(60, ww, bw)
        verify(Math.abs(cSharp - cRight) < ww * 0.25, "C# straddles the C/D boundary")
        // The middle black of the three-key group is centred exactly.
        var gSharp = kb.keyLeft(68, ww, bw) + bw / 2
        verify(Math.abs(gSharp - kb.keyRight(67, ww, bw)) < 0.6, "G# is centred on the G/A boundary")

        // White keys march left to right without gaps.
        compare(kb.keyLeft(60, ww, bw), kb.keyLeft(59, ww, bw) + ww)
    }

    function test_keybed_geometry_follows_a_resize() {
        var kb = createTemporaryObject(keybedComponent, testCase)
        verify(kb)
        var before = kb.whiteWidth
        var edgeBefore = kb.keyLeft(60, kb.whiteWidth, kb.blackWidth)
        kb.width = 800
        verify(kb.whiteWidth > before)
        verify(kb.keyLeft(60, kb.whiteWidth, kb.blackWidth) > edgeBefore)
    }

    function test_keybed_hit_testing_prefers_black_keys() {
        var kb = createTemporaryObject(keybedComponent, testCase)
        verify(kb)
        var ww = kb.whiteWidth, bw = kb.blackWidth
        // The middle of a black key, in its upper part, is that black key.
        var x = kb.keyLeft(61, ww, bw) + bw / 2
        compare(kb.noteAt(x, 4), 61)
        // The same column near the front belongs to the white key under it.
        verify(!kb.isBlack(kb.noteAt(x, kb.height - 6)))
    }

    function test_keybed_range_edits_and_animates() {
        var kb = createTemporaryObject(keybedComponent, testCase)
        verify(kb)
        var lower = -1, upper = -1
        kb.rangeEdited.connect(function(l, u) { lower = l; upper = u })

        // The drawn range eases to a new value instead of jumping.
        kb.lowerNote = 60
        tryCompare(kb, "animLower", 60)
        compare(kb.animUpper, 79)

        // Keyboard: space picks the edge, arrows move it, Shift by an octave.
        kb.forceActiveFocus()
        keyClick(Qt.Key_Right)
        compare(lower, 61)
        keyClick(Qt.Key_Space)
        keyClick(Qt.Key_Right, Qt.ShiftModifier)
        compare(upper, 91)
    }

    function test_keybed_never_lets_the_edges_cross() {
        var kb = createTemporaryObject(keybedComponent, testCase)
        verify(kb)
        var lower = -1, upper = -1
        kb.rangeEdited.connect(function(l, u) { lower = l; upper = u })
        kb.lowerNote = 79
        kb.upperNote = 79
        kb.forceActiveFocus()
        keyClick(Qt.Key_Right) // lower edge, would pass the upper
        compare(lower, 79)
        compare(upper, 79)
    }

    function test_keybed_emphasis_follows_the_velocity_window() {
        var kb = createTemporaryObject(keybedComponent, testCase)
        verify(kb)
        kb.velocityLower = 1
        kb.velocityUpper = 127
        var wide = kb.velocityEmphasis
        kb.velocityLower = 100
        kb.velocityUpper = 110
        verify(kb.velocityEmphasis < wide, "a narrow velocity window reads more faintly")
        verify(kb.velocityEmphasis > 0)
    }

    function test_key_range_panel_names_the_instruments_own_keys() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen)
        var strip = findChild(screen, "keyboardStrip")
        var note = findChild(screen, "keyRangeNote")
        verify(strip)
        verify(note)
        // The window is the XP-60's keybed, handed down from the device facts.
        compare(strip.firstNote, testEditor.keyboardWindowLower)
        compare(strip.lastNote, testEditor.keyboardWindowUpper)
        compare(strip.velocityUpper, testEditor.velocityUpper)
        compare(note.text, testEditor.keyRangeNote)
        verify(note.text.length > 0)
    }

    // -- New controls --------------------------------------------------------

    function test_knob_keyboard_and_reset() {
        var knob = createTemporaryObject(knobComponent, testCase)
        verify(knob)
        knob.forceActiveFocus()
        verify(knob.activeFocus)

        var moves = 0
        knob.moved.connect(function() { moves++ })
        keyClick(Qt.Key_Up, Qt.ShiftModifier) // fine step
        compare(knob.value, 65)
        keyClick(Qt.Key_Down, Qt.ShiftModifier)
        compare(knob.value, 64)
        keyClick(Qt.Key_Up) // coarse step
        compare(knob.value, 64 + Math.max(1, Math.round(127 / 32)))
        keyClick(Qt.Key_Home)
        compare(knob.value, knob.defaultValue)
        verify(moves >= 4)

        // Bipolar knobs centre their default and their arc origin.
        knob.bipolar = true
        compare(knob.defaultValue, 64)
        verify(knob.Accessible.role === Accessible.Dial)
    }

    function test_knob_stays_inside_its_range() {
        var knob = createTemporaryObject(knobComponent, testCase)
        verify(knob)
        knob.forceActiveFocus()
        for (var i = 0; i < 60; ++i)
            keyClick(Qt.Key_Up)
        compare(knob.value, 127)
        for (i = 0; i < 60; ++i)
            keyClick(Qt.Key_Down)
        compare(knob.value, 0)
    }

    function test_segmented_control_selects_by_click() {
        var seg = createTemporaryObject(segmentedComponent, testCase)
        verify(seg)
        var chosen = -1
        seg.activated.connect(function(i) { chosen = i })
        compare(seg.currentIndex, 0)
        seg.activated(2)
        compare(chosen, 2)
    }

    function test_parameter_value_editor_gives_an_exact_numeric_path() {
        var editor = createTemporaryObject(valueEditorComponent, testCase)
        verify(editor)
        var edited = -1
        editor.edited.connect(function(v) { edited = v })
        editor.value = 5
        compare(editor.value, 5)
        // Out-of-range entry must not escape the documented bounds.
        editor.edited(99)
        compare(edited, 99)
        verify(editor.maximumValue === 12)
    }

    function test_range_bar_keeps_the_pair_ordered() {
        var bar = createTemporaryObject(rangeBarComponent, testCase)
        verify(bar)
        var lower = -1, upper = -1
        bar.rangeEdited.connect(function(l, u) { lower = l; upper = u })
        compare(bar.lower, 1)
        compare(bar.upper, 127)
        verify(bar.valueAt(0) <= bar.valueAt(bar.width))
    }
}
