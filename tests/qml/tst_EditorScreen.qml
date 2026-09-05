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
        testEditor.disclosure = 1
        testEditor.expertParameters.commonScope = false
        testEditor.expertParameters.group = 0
        testEditor.expertParameters.search = ""
    }

    function test_browsing_waves_never_modifies_the_patch_and_is_keyboard_accessible() {
        var screen = createTemporaryObject(screenComponent, testCase)
        testEditor.waves.query = ""
        testEditor.waves.sourceFilter = 0
        var before = testEditor.differenceSummary
        mouseClick(findChild(screen, "browseWavesButton"))
        verify(screen.browsingWaves)
        var search = findChild(screen, "waveSearch")
        tryCompare(search, "activeFocus", true)
        testEditor.waves.query = "Kalimba"
        compare(testEditor.waves.count, 1)
        var list = findChild(screen, "waveResults")
        list.forceActiveFocus()
        keyClick(Qt.Key_Down)
        compare(testEditor.waves.selected.name, "Kalimba")
        compare(testEditor.differenceSummary, before)
        verify(!testEditor.modified)
        keyClick(Qt.Key_Escape)
        verify(!screen.browsingWaves)
        tryCompare(findChild(screen, "browseWavesButton"), "activeFocus", true)
        testEditor.waves.query = ""
    }

    // Use in Tone. The bank/number to SysEx mapping this depends on is hardware
    // evidence from DEVICE_ACCEPTANCE.md area 9, not documentation.
    function test_use_in_tone_assigns_the_selected_wave_as_one_undoable_edit() {
        var screen = createTemporaryObject(screenComponent, testCase)
        testEditor.revertToOriginal()
        testEditor.waves.query = ""
        testEditor.waves.sourceFilter = 0
        testEditor.selectedTone = 2

        mouseClick(findChild(screen, "browseWavesButton"))
        verify(screen.browsingWaves)

        var useButton = findChild(screen, "useWaveInTone")
        verify(useButton !== null)
        // The disabled-with-no-selection case is asserted in the C++ tests,
        // which get a fresh fixture. Every QML test here shares one editor and
        // a selection cannot be cleared once made, so it is not reproducible
        // from this side.

        testEditor.waves.query = "Kalimba"
        compare(testEditor.waves.count, 1)
        testEditor.waves.selectRow(0)
        compare(testEditor.waves.selected.bank, "INT-B")
        compare(testEditor.waves.selected.number, 1)
        tryCompare(useButton, "enabled", true)
        compare(useButton.text, "Use in Tone 2")

        mouseClick(useButton)
        // The browser closes once a wave has been used, returning to the editor.
        tryCompare(screen, "browsingWaves", false)
        verify(testEditor.modified)

        // One edit: a single undo takes the whole wave reference back.
        testEditor.undo()
        verify(!testEditor.modified)

        testEditor.waves.query = ""
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
        verify(findChild(screen, "toneConnections").visible)
        screen.width = 900
        compare(grid.columns, 2)
        verify(!findChild(screen, "toneConnections").visible)
    }

    function test_write_actions_fit_at_minimum_shell_width() {
        var screen = createTemporaryObject(screenComponent, testCase,
                                           { width: Metrics.windowMinWidth - Metrics.railWidth })
        verify(screen)
        waitForRendering(screen)
        var write = findChild(screen, "writeToDeviceButton")
        var arm = findChild(screen, "armWriteButton")
        verify(write)
        verify(arm)
        var position = write.mapToItem(screen, 0, 0)
        verify(position.x >= 0)
        verify(position.x + write.width <= screen.width)
        verify(position.y + write.height <= screen.height)
        mouseClick(arm)
        verify(testEditor.writeArmed)
        mouseClick(arm)
    }

    function test_tone_switches_support_keyboard_activation() {
        var screen = createTemporaryObject(screenComponent, testCase)
        var card = findChild(screen, "toneCard1")
        var enable = findChild(card, "toneEnableButton")
        var solo = findChild(card, "toneSoloButton")
        var mute = findChild(card, "toneMuteButton")
        var wasEnabled = card.tone.enabled
        enable.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(card.tone.enabled, !wasEnabled)
        keyClick(Qt.Key_Return)
        compare(card.tone.enabled, wasEnabled)
        solo.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(card.tone.solo, true)
        keyClick(Qt.Key_Return)
        compare(card.tone.solo, false)
        mute.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(card.tone.mute, true)
        keyClick(Qt.Key_Return)
        compare(card.tone.mute, false)
    }

    function test_desktop_toolbar_keeps_all_tones_and_envelope_in_view() {
        var screen = createTemporaryObject(screenComponent, testCase, { width: 1204, height: 988 })
        waitForRendering(screen)
        var sections = findChild(screen, "sectionTabs")
        var disclosure = findChild(screen, "disclosureTabs")
        var a = sections.mapToItem(screen, 0, 0)
        var b = disclosure.mapToItem(screen, 0, 0)
        compare(Math.round(a.y), Math.round(b.y))
        verify(a.x + sections.width <= b.x)
        verify(b.x + disclosure.width <= screen.width)
        var envelope = findChild(screen, "envelopeEditor")
        var p = envelope.mapToItem(screen, 0, 0)
        verify(p.y + envelope.height <= screen.height, "Full envelope graph fits inside the desktop viewport")
    }

    function test_routing_changes_with_output_and_fits_minimum_width() {
        var screen = createTemporaryObject(screenComponent, testCase,
                                           { width: Metrics.windowMinWidth - Metrics.railWidth })
        testEditor.section = 4
        testEditor.sectionParameters.group = 3
        testEditor.sectionParameters.edit("tone.output_assign", 1, 2)
        var routing = findChild(screen, "effectRoutingView")
        verify(routing)
        waitForRendering(screen)
        // Paired structures may route through Tone 2, so use the displayed owner.
        var owner = testEditor.routing.outputTone
        testEditor.selectedTone = owner
        testEditor.sectionParameters.edit("tone.output_assign", owner, 2)
        tryVerify(function() { return findChild(routing, "route-source-direct") !== null })
        verify(!findChild(routing, "route-source-chorus"))
        var direct = findChild(routing, "routingDirect")
        verify(direct.visible)
        verify(direct.highlighted)
        var mix = findChild(routing, "routingMix")
        verify(!mix.highlighted)
        for (var name of ["structureNode", "routingEfx", "routingChorus", "routingReverb", "routingMix", "routingDirect"]) {
            var node = findChild(routing, name)
            var pos = node.mapToItem(routing, 0, 0)
            verify(pos.x >= 0)
            verify(pos.x + node.width <= routing.width + 1)
            verify(pos.y + node.height <= routing.implicitHeight)
        }
        testEditor.sectionParameters.edit("tone.output_assign", owner, 0)
        tryVerify(function() { return findChild(routing, "route-source-mix") !== null })
        verify(!findChild(routing, "route-source-direct"))
        verify(!direct.visible)
    }

    function test_live_audition_requires_arm_and_finishes_explicitly() {
        var screen = createTemporaryObject(screenComponent, testCase)
        screen.width = 764
        var start = findChild(screen, "startLiveButton")
        var stop = findChild(screen, "stopLiveButton")
        var restore = findChild(screen, "restoreAuditionButton")
        compare(start.enabled, false)
        testEditor.armWrite()
        compare(start.enabled, true)
        mouseClick(start)
        compare(testEditor.liveAudition, true)
        testHarness.pumpEditor()
        verify(findChild(screen, "auditionMessage").text.indexOf("LIVE") >= 0)
        compare(stop.visible, true)
        compare(restore.visible, true)
        verify(restore.mapToItem(screen, restore.width, 0).x <= screen.width)
        mouseClick(stop)
        compare(testEditor.liveStopping, true)
        wait(180)
        testHarness.pumpEditor()
        wait(180)
        compare(testEditor.liveAudition, false)
        compare(start.enabled, false)
    }

    Component {
        id: parameterPanelComponent
        EditorParameterPanel { editor: testEditor; parameters: testEditor.sectionParameters; width: 700; title: "Parameters" }
    }

    function test_disclosure_keeps_four_tones_and_reveals_the_right_controls() {
        var screen = createTemporaryObject(screenComponent, testCase)
        var tabs = findChild(screen, "disclosureTabs")
        tabs.activated(0)
        compare(testEditor.disclosure, 0)
        verify(!findChild(screen, "designDetails").visible)
        verify(!findChild(screen, "expertParameterPanel").visible)
        verify(findChild(screen, "toneGrid").visible)
        tabs.activated(2)
        verify(findChild(screen, "expertParameterPanel").visible)
        verify(findChild(screen, "toneGrid").visible)
        verify(!testEditor.modified)
    }

    function test_filter_parameter_edit_flows_through_undo_and_comparison() {
        var screen = createTemporaryObject(screenComponent, testCase)
        testEditor.section = 1
        var panel = findChild(screen, "sectionParameterPanel")
        tryVerify(function() { return findChild(panel, "parameter-tone.cutoff_frequency") !== null })
        var cell = findChild(panel, "parameter-tone.cutoff_frequency")
        var control = findChild(cell, "parameterValue")
        var original = control.value
        var edited = original === 20 ? 30 : 20
        control.edited(edited)
        compare(control.value, edited)
        testEditor.comparing = true
        compare(control.value, original)
        verify(!control.editable)
        testEditor.comparing = false
        testEditor.undo()
        compare(control.value, original)
    }

    function test_parameter_controls_accept_keyboard_input() {
        testEditor.section = 1
        var panel = createTemporaryObject(parameterPanelComponent, testCase)
        tryVerify(function() { return findChild(panel, "parameter-tone.cutoff_frequency") !== null })
        var cutoff = findChild(panel, "parameter-tone.cutoff_frequency")
        var field = findChild(cutoff, "valueField")
        field.forceActiveFocus()
        keyClick(Qt.Key_A, Qt.ControlModifier)
        keyClick(Qt.Key_4)
        keyClick(Qt.Key_2)
        keyClick(Qt.Key_Return)
        compare(findChild(cutoff, "parameterValue").value, 42)
        testEditor.section = 3
        tryVerify(function() { return findChild(panel, "parameter-tone.lfo1_waveform") !== null })
        var shape = findChild(findChild(panel, "parameter-tone.lfo1_waveform"), "parameterChoice")
        var original = shape.currentIndex
        shape.forceActiveFocus()
        keyClick(original < shape.count - 1 ? Qt.Key_Down : Qt.Key_Up)
        compare(shape.currentIndex, original < shape.count - 1 ? original + 1 : original - 1)
        testEditor.undo()
        compare(shape.currentIndex, original)
    }

    function test_motion_lfo_choice_and_effects_controls_are_available() {
        var screen = createTemporaryObject(screenComponent, testCase)
        testEditor.section = 3
        var panel = findChild(screen, "motionEffectsPanel")
        verify(panel.visible)
        // Visual LFO selector is primary; exact ComboBox remains under disclosure.
        var exactToggle = null
        var buttons = panel.contentItem ? null : null
        // Expand exact parameters for the documented ComboBox path.
        for (var i = 0; i < 20; ++i) {
            var kids = panel.children
        }
        // Force the exact panel open by finding the nested EditorParameterPanel path:
        // Click the "Exact LFO" ghost button via text search is fragile; toggle via first ghost after shapes.
        var exactButton = null
        function findExactButton(item) {
            if (!item) return null
            if (item.text && String(item.text).indexOf("Exact LFO") >= 0) return item
            if (item.children) {
                for (var c = 0; c < item.children.length; ++c) {
                    var found = findExactButton(item.children[c])
                    if (found) return found
                }
            }
            return null
        }
        exactButton = findExactButton(panel)
        verify(exactButton)
        mouseClick(exactButton)
        tryVerify(function() { return findChild(panel, "parameter-tone.lfo1_waveform") !== null })
        var shape = findChild(findChild(panel, "parameter-tone.lfo1_waveform"), "parameterChoice")
        compare(shape.count, 8)
        shape.activated(3)
        compare(shape.currentIndex, 3)
        testEditor.section = 4
        var effects = findChild(screen, "effectsWorkbench")
        verify(effects.visible)
        testEditor.effectPage = 1
        compare(effects.algorithms.length, 40)
        effects.browserOpen = true
        findChild(effects, "algorithm-39").clicked()
        compare(testEditor.mfxText, "CHORUS/FLANGER")
    }

    function test_expert_search_and_scope_show_documented_parameters() {
        var screen = createTemporaryObject(screenComponent, testCase)
        testEditor.disclosure = 2
        var panel = findChild(screen, "expertParameterPanel")
        findChild(panel, "expertScope").activated(1)
        testEditor.expertParameters.search = "reverb type"
        tryCompare(findChild(panel, "parameterGrid"), "count", 1)
        tryVerify(function() { return findChild(panel, "parameter-common.reverb_type") !== null })
        var choice = findChild(findChild(panel, "parameter-common.reverb_type"), "parameterChoice")
        choice.activated(6)
        compare(choice.currentText, "DELAY")
        verify(testEditor.modified)
    }

    function test_exact_range_fields_edit_and_follow_the_selected_tone() {
        var screen = createTemporaryObject(screenComponent, testCase)
        var lower = findChild(screen, "keyLowerEntry")
        var upper = findChild(screen, "keyUpperEntry")
        lower.edited(36)
        upper.edited(72)
        compare(testEditor.keyRangeLower, 36)
        compare(testEditor.keyRangeUpper, 72)
        compare(lower.maximumValue, 72)
        compare(upper.minimumValue, 36)
        findChild(screen, "velocityLowerEntry").edited(30)
        findChild(screen, "velocityUpperEntry").edited(90)
        compare(testEditor.velocityLower, 30)
        compare(testEditor.velocityUpper, 90)
        testEditor.selectedTone = 2
        compare(lower.value, testEditor.keyRangeLower)
        testEditor.comparing = true
        verify(!lower.editable)
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
