import QtQuick
import QtTest
import XP60Studio

// Smoke tests for the reusable foundation controls.
TestCase {
    id: testCase
    name: "Controls"
    when: windowShown
    width: 400
    height: 300
    visible: true

    Component {
        id: partStripComponent
        PerformancePartStrip {
            width: 96
            height: 360
            part: ({
                partNumber: 1, midiChannel: 1, isRhythmPart: false,
                patchNumber: 12, patchGroupLabel: "USER", receives: true,
                level: 100, pan: 64, panText: "C",
                chorusSend: 64, reverbSend: 32, voiceReserve: 0,
                keyLowerNote: "C2", keyUpperNote: "C7",
                keyLowerRaw: 36, keyUpperRaw: 96
            })
        }
    }

    Component {
        id: musicalKnobComponent
        MusicalParamKnob {
            label: "Random Pitch Depth"
            from: 0; to: 126; value: 63; bipolar: true; labelWidth: 140
        }
    }

    Component {
        id: buttonComponent
        XpButton { text: "Connect"; variant: "primary" }
    }

    Component {
        id: pillComponent
        StatusPill { text: "Connected"; tone: "live" }
    }

    Component {
        id: fieldComponent
        XpTextField { mono: true; placeholderText: "03 00 00 00" }
    }

    Component {
        id: comboComponent
        XpComboBox { model: ["One", "Two", "Three"] }
    }

    Component {
        id: metricComponent
        XpMetricTile { label: "Timeouts"; value: "3"; tone: "warning" }
    }

    Component {
        id: cardComponent
        XpCard { width: 200; XpLabel { text: "Inside" } }
    }

    Component {
        id: labelComponent
        XpLabel { text: "Application label" }
    }

    function test_theme_tokens_are_defined() {
        verify(Theme.accent.toString().length > 0)
        verify(Theme.tone1 !== Theme.tone2)
        verify(Theme.tone3 !== Theme.tone4)
        compare(Theme.toneColor(1), Theme.tone1)
        compare(Theme.toneColor(4), Theme.tone4)
        compare(Theme.toneForeground("success"), Theme.success)
        compare(Theme.toneBackground("error"), Theme.errorSoft)
        verify(Metrics.controlHeight > 0)
        verify(Typography.bodySize > 0)
        verify(Typography.family.toLowerCase().indexOf("silkscreen") < 0,
               "the LCD face must never become the application default")
        verify(Motion.durationNormal >= 0)
    }

    function test_regular_labels_never_inherit_the_lcd_face() {
        var label = createTemporaryObject(labelComponent, testCase)
        verify(label)
        compare(label.font.family, Typography.family)
        verify(label.font.family.toLowerCase().indexOf("silkscreen") < 0)
    }

    function test_button_click_and_states() {
        var button = createTemporaryObject(buttonComponent, testCase)
        verify(button)
        var clicks = 0
        button.clicked.connect(function() { clicks++ })
        mouseClick(button)
        compare(clicks, 1)
        compare(button.labelColor, Theme.textOnAccent)
        button.enabled = false
        mouseClick(button)
        compare(clicks, 1)
        compare(button.labelColor, Theme.textDisabled)
        button.enabled = true
        button.variant = "secondary"
        compare(button.labelColor, Theme.textPrimary)
        button.variant = "danger"
        compare(button.outlineColor, Theme.error)
    }

    function test_button_keyboard_activation() {
        var button = createTemporaryObject(buttonComponent, testCase)
        verify(button)
        button.forceActiveFocus()
        verify(button.activeFocus)
        var clicks = 0
        button.clicked.connect(function() { clicks++ })
        keyClick(Qt.Key_Space)
        compare(clicks, 1)
        keyClick(Qt.Key_Return)
        compare(clicks, 2)
        keyClick(Qt.Key_Enter)
        compare(clicks, 3)
    }

    function test_status_pill_conveys_tone_by_text_and_colour() {
        var pill = createTemporaryObject(pillComponent, testCase)
        verify(pill)
        compare(pill.text, "Connected")
        compare(pill.foreground, Theme.live)
        pill.tone = "error"
        compare(pill.foreground, Theme.error)
        verify(pill.implicitWidth > 0)
        verify(pill.Accessible.name === "Connected")
    }

    function test_text_field_invalid_border() {
        var field = createTemporaryObject(fieldComponent, testCase)
        verify(field)
        compare(field.background.border.color, Theme.borderStrong)
        field.invalid = true
        // The border colour is animated (Motion.durationFast); wait for it.
        tryCompare(field.background.border, "color", Theme.error)
        field.forceActiveFocus()
        keyClick(Qt.Key_0)
        keyClick(Qt.Key_3)
        compare(field.text, "03")
    }

    function test_combo_box_with_string_model() {
        var combo = createTemporaryObject(comboComponent, testCase)
        verify(combo)
        compare(combo.count, 3)
        compare(combo.currentIndex, 0)
        compare(combo.displayText, "One")
        combo.currentIndex = 2
        compare(combo.displayText, "Three")
    }

    function test_metric_tile_and_card() {
        var tile = createTemporaryObject(metricComponent, testCase)
        verify(tile)
        compare(tile.toneColor, Theme.warning)
        verify(tile.implicitHeight > 0)
        var card = createTemporaryObject(cardComponent, testCase)
        verify(card)
        compare(card.color, Theme.surface)
        verify(card.implicitHeight > 0)
    }

    // A send is a continuous amount, so the strip has to show how much, not
    // only print the number (Directive Alpha).
    function test_part_strip_shows_sends_as_meters_and_flags_no_voice_reserve() {
        var strip = createTemporaryObject(partStripComponent, testCase)
        verify(strip)
        verify(strip.showSends)
        var reserve = findChild(strip, "partVoiceReserve1")
        verify(reserve)
        compare(reserve.text, "0")
        // Voice Reserve 0 means the Part can be starved, and the strip says so
        // in colour rather than only in the inspector.
        compare(reserve.color, Theme.warning)
        verify(findChild(strip, "partLevel1"))
        verify(findChild(strip, "partPan1"))
    }

    // Documented Roland names are long; a knob-width caption elided them into
    // nonsense ("Random...").
    function test_musical_knob_caption_survives_a_long_documented_name() {
        var knob = createTemporaryObject(musicalKnobComponent, testCase)
        verify(knob)
        var caption = null
        for (var i = 0; i < knob.children.length; ++i) {
            if (knob.children[i].text === "Random Pitch Depth") caption = knob.children[i]
        }
        verify(caption, "knob caption not found")
        verify(!caption.truncated, "documented parameter name was elided")
        verify(findChild(knob, "musicalKnob").bipolar)
    }
}
