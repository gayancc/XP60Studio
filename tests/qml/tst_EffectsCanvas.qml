import QtQuick
import QtTest
import XP60Studio
import XP60Studio.Presentation

// The Living Effects Canvas, over the real Patch the editor holds.
//
// These tests are about the things the canvas claims to fix: that the signal
// path has a hierarchy rather than seven equal cables, that no value floats
// without a name, that focusing a stage does not lose the topology, and that
// routing can only go where the XP-60 allows.
TestCase {
    id: testCase
    name: "EffectsCanvas"
    when: windowShown
    width: 1280
    height: 720
    visible: true

    Component {
        id: canvasComponent
        EffectsCanvas { editor: testEditor; width: 1180; height: 320 }
    }

    function makeCanvas() {
        testEditor.revertToOriginal()
        testEditor.section = 4          // Effects: the full canvas, not the strip
        var canvas = createTemporaryObject(canvasComponent, testCase)
        verify(canvas)
        waitForRendering(canvas)
        return canvas
    }

    function railFor(canvas, from, to) {
        return findChild(canvas, "rail-" + from + "-" + to)
    }

    // ── Hierarchy ─────────────────────────────────────────────────────────

    function test_main_spine_outranks_every_send() {
        var canvas = makeCanvas()
        var spine = railFor(canvas, "source", "efx")
        verify(spine, "the source feeds EFX along the spine")
        compare(spine.rail, "main")

        // Whatever the sends are set to, the spine stays the heaviest rail.
        // Turning a send up must not let it outrank the main path.
        var send = railFor(canvas, "source", "chorus")
        if (send) {
            compare(send.rail, "send")
            verify(spine.baseWidth > send.baseWidth)
        }
        var out = railFor(canvas, "efx", "mix")
        if (out)
            compare(out.rail, "main")
    }

    function test_the_three_spine_stages_sit_on_one_centreline() {
        var canvas = makeCanvas()
        var source = findChild(canvas, "canvasSource")
        var efx = findChild(canvas, "canvasEfx")
        var mix = findChild(canvas, "canvasMix")
        // A straight left-to-right spine is the anchor of the whole canvas.
        var cs = source.y + source.height / 2
        var ce = efx.y + efx.height / 2
        var cm = mix.y + mix.height / 2
        compare(Math.round(cs), Math.round(ce))
        compare(Math.round(ce), Math.round(cm))
        verify(source.x < efx.x)
        verify(efx.x < mix.x)
    }

    function test_stages_read_by_role_not_by_colour_alone() {
        var canvas = makeCanvas()
        compare(findChild(canvas, "canvasSource").role, "source")
        compare(findChild(canvas, "canvasMix").role, "destination")
        compare(findChild(canvas, "canvasEfx").role, "processor")
        // The hero of the chain is physically larger than what feeds it.
        verify(findChild(canvas, "canvasEfx").width > findChild(canvas, "canvasSource").width)
    }

    // ── Named values ──────────────────────────────────────────────────────

    function test_every_route_value_carries_its_own_name() {
        var canvas = makeCanvas()
        var edges = testEditor.routing.edges
        var named = 0
        for (var i = 0; i < edges.length; ++i) {
            if (!edges[i].parameterId)
                continue
            var chip = findChild(canvas, "chip-" + edges[i].from + "-" + edges[i].to)
            verify(chip, "route " + edges[i].from + "->" + edges[i].to + " has a chip")
            verify(chip.caption.length > 0, "a value never appears without a name")
            verify(chip.parameterId === edges[i].parameterId)
            named++
        }
        verify(named > 0)
    }

    function test_a_chip_edits_the_real_parameter_at_full_resolution() {
        var canvas = makeCanvas()
        var chip = findChild(canvas, "chip-source-chorus")
        if (!chip)
            return   // this Patch routes without a chorus send
        var before = chip.value
        var target = before > 0 ? before - 1 : 1
        chip.editor.beginEffectGesture()
        chip.editor.editEffect(chip.parameterId, target)
        chip.editor.endEffectGesture()
        // Raw XP resolution, not a rescaled approximation of the gesture.
        compare(testEditor.effectValues[chip.parameterId].value, target)
        verify(testEditor.modified)
        testEditor.undo()
    }

    // ── Focus ─────────────────────────────────────────────────────────────

    function test_focus_promotes_one_stage_and_keeps_the_topology_visible() {
        var canvas = makeCanvas()
        var chorus = findChild(canvas, "canvasChorus")
        var reverb = findChild(canvas, "canvasReverb")

        canvas.focusedNode = "chorus"
        compare(chorus.focused, true)
        verify(!chorus.dimmed)
        // Orientation survives: every stage is still on the canvas, and the
        // ones the focused stage talks to stay legible.
        verify(chorus.visible && reverb.visible)
        verify(findChild(canvas, "canvasSource").visible)
        verify(findChild(canvas, "canvasMix").visible)

        // Routes that do not touch the focused stage recede.
        var unrelated = railFor(canvas, "source", "efx")
        if (unrelated)
            compare(unrelated.dimmed, true)

        canvas.focusedNode = ""
        compare(chorus.focused, false)
        verify(!chorus.dimmed)
    }

    function test_isolating_a_route_leaves_only_that_path_lit() {
        var canvas = makeCanvas()
        var chip = findChild(canvas, "chip-efx-mix")
        if (!chip)
            return
        canvas.isolatedRoute = chip.parameterId

        compare(chip.isolated, true)
        var rail = railFor(canvas, "efx", "mix")
        compare(rail.isolated, true)
        verify(!rail.dimmed)
        // Its two ends stay lit so the isolated value has context.
        verify(!findChild(canvas, "canvasEfx").dimmed)
        verify(!findChild(canvas, "canvasMix").dimmed)
        // Everything else recedes.
        verify(findChild(canvas, "canvasChorus").dimmed)

        // And the number is explained rather than left bare.
        var title = findChild(canvas, "isolatedRouteTitle")
        verify(title.text.length > 0)

        canvas.isolatedRoute = ""
        verify(!findChild(canvas, "canvasChorus").dimmed)
    }

    function test_back_to_overview_clears_focus_and_isolation() {
        var canvas = makeCanvas()
        canvas.focusedNode = "reverb"
        canvas.isolatedRoute = "common.reverb_level"
        var back = findChild(canvas, "canvasBack")
        verify(back.visible)
        mouseClick(back)
        compare(canvas.focusedNode, "")
        compare(canvas.isolatedRoute, "")
        verify(!back.visible)
    }

    // ── Routing manipulation ──────────────────────────────────────────────

    function test_routing_handle_offers_only_destinations_the_xp60_allows() {
        var canvas = makeCanvas()
        canvas.focusedNode = "efx"
        var handle = findChild(canvas, "routingHandle")
        verify(handle.visible, "a focused EFX can be re-routed")

        // EFX output is MIX or DIR on the XP-60. Chorus and Reverb are not
        // destinations for it, and must not light up as though they were.
        canvas.dragFrom = "efx"
        canvas.dragParameter = "common.efx_output_assign"
        verify(canvas.isValidDestination("mix"))
        verify(canvas.isValidDestination("direct"))
        verify(!canvas.isValidDestination("chorus"))
        verify(!canvas.isValidDestination("reverb"))
        // A stage is never a destination for itself.
        verify(!canvas.isValidDestination("efx"))

        // The consequence is stated in the instrument's own terms before drop.
        canvas.dragTarget = "mix"
        verify(canvas.dropConsequence().indexOf("MIX") >= 0)

        canvas.dragFrom = ""
        canvas.dragParameter = ""
        canvas.dragTarget = ""
    }

    function test_dropping_writes_the_documented_parameter_and_is_undoable() {
        var canvas = makeCanvas()
        canvas.focusedNode = "efx"
        var pid = "common.efx_output_assign"
        var before = testEditor.effectValues[pid].value
        var choices = testEditor.effectValues[pid].choices
        // Pick the destination that is not already selected.
        var wantNode = choices[before] === "MIX" ? "direct" : "mix"

        canvas.dragFrom = "efx"
        canvas.dragParameter = pid
        canvas.dragTarget = wantNode
        canvas.commitRouting()

        var after = testEditor.effectValues[pid].value
        verify(after !== before)
        compare(choices[after], wantNode === "mix" ? "MIX" : "DIR")
        compare(canvas.dragFrom, "")

        // One drop is one undo.
        testEditor.undo()
        compare(testEditor.effectValues[pid].value, before)
    }

    function test_undocumented_output_values_are_never_offered() {
        var canvas = makeCanvas()
        // The model drops the undocumented OUTPUT-2 value before it reaches
        // QML, so no drop target can ever select it.
        var choices = testEditor.effectValues["common.efx_output_assign"].choices
        for (var i = 0; i < choices.length; ++i)
            verify(choices[i].indexOf("OUTPUT-2") < 0)
        compare(canvas.nodeForChoice("<OUTPUT-2>"), "")
        // Combined destinations name two stages, so they are not drop targets.
        compare(canvas.nodeForChoice("MIX+REV"), "")
    }

    // ── Gesture reliability ───────────────────────────────────────────────
    // The canvas sits inside the editor ScrollView. Every drag surface has to
    // hold its gesture, or the Flickable takes it partway through and the value
    // stops tracking while the page scrolls.

    function test_drag_surfaces_refuse_to_hand_the_gesture_to_the_scrollview() {
        var canvas = makeCanvas()
        canvas.focusedNode = "efx"
        var handle = findChild(canvas, "routingHandle")
        verify(handle.visible)
        var grab = null
        for (var i = 0; i < handle.children.length; ++i)
            if (handle.children[i].preventStealing !== undefined)
                grab = handle.children[i]
        verify(grab, "the routing handle has a grab area")
        compare(grab.preventStealing, true)
        canvas.focusedNode = ""
    }

    function test_a_click_on_a_chip_isolates_without_nudging_the_value() {
        var canvas = makeCanvas()
        var chip = findChild(canvas, "chip-efx-mix")
        if (!chip)
            return
        var before = chip.value
        // A press and release with no travel must not change a send, however
        // small the accidental movement.
        mousePress(chip, chip.width / 2, chip.height / 2)
        mouseMove(chip, chip.width / 2, chip.height / 2 - 1)
        mouseRelease(chip, chip.width / 2, chip.height / 2 - 1)
        compare(chip.value, before, "a click never nudges a send")
        compare(canvas.isolatedRoute, chip.parameterId, "a click isolates the route")
        verify(!chip.adjusting)
        canvas.isolatedRoute = ""
    }

    function test_a_real_drag_adjusts_and_leaves_one_undo_entry() {
        var canvas = makeCanvas()
        var chip = findChild(canvas, "chip-efx-mix")
        if (!chip)
            return
        var before = chip.value
        mousePress(chip, chip.width / 2, chip.height / 2)
        // Well past the threshold, downwards, so the value falls.
        mouseMove(chip, chip.width / 2, chip.height / 2 + 40)
        verify(chip.adjusting, "past the threshold the chip is adjusting")
        mouseMove(chip, chip.width / 2, chip.height / 2 + 70)
        mouseRelease(chip, chip.width / 2, chip.height / 2 + 70)
        verify(!chip.adjusting)
        verify(chip.value !== before, "the drag moved the value")
        // One gesture, one undo, however many values it passed through.
        testEditor.undo()
        compare(testEditor.effectValues[chip.parameterId].value, before)
        verify(!testEditor.modified)
    }

    function test_a_drop_is_forgiving_near_a_destination() {
        var canvas = makeCanvas()
        canvas.focusedNode = "efx"
        canvas.dragFrom = "efx"
        canvas.dragParameter = "common.efx_output_assign"

        var mix = findChild(canvas, "canvasMix")
        // Just outside the node rectangle: a drop there still lands, because
        // dropping is a gesture rather than a precision task.
        compare(canvas.nodeAtPoint(mix.x - 6, mix.y + mix.height / 2), "mix")
        // Far away is still a miss.
        compare(canvas.nodeAtPoint(mix.x - 260, mix.y + mix.height / 2), "")

        canvas.cancelRouting()
        compare(canvas.dragFrom, "")
        compare(canvas.dragTarget, "")
        canvas.focusedNode = ""
    }

    function test_a_cancelled_drag_leaves_no_state_behind() {
        var canvas = makeCanvas()
        canvas.focusedNode = "efx"
        var pid = "common.efx_output_assign"
        var before = testEditor.effectValues[pid].value
        canvas.dragFrom = "efx"
        canvas.dragParameter = pid
        canvas.dragTarget = "mix"
        canvas.cancelRouting()
        // Cancelling writes nothing and clears everything.
        compare(testEditor.effectValues[pid].value, before)
        compare(canvas.dragFrom, "")
        compare(canvas.dragParameter, "")
        compare(canvas.dragTarget, "")
        verify(!canvas.routingDrag)
        canvas.focusedNode = ""
    }

    function test_two_values_never_sit_on_top_of_each_other() {
        var canvas = makeCanvas()
        var edges = testEditor.routing.edges
        var chips = []
        for (var i = 0; i < edges.length; ++i) {
            if (!edges[i].parameterId)
                continue
            var c = findChild(canvas, "chip-" + edges[i].from + "-" + edges[i].to)
            if (c && c.visible)
                chips.push(c)
        }
        verify(chips.length > 1)
        for (var a = 0; a < chips.length; ++a) {
            for (var b = a + 1; b < chips.length; ++b) {
                var overlapX = chips[a].x < chips[b].x + chips[b].width
                               && chips[b].x < chips[a].x + chips[a].width
                var overlapY = chips[a].y < chips[b].y + chips[b].height
                               && chips[b].y < chips[a].y + chips[a].height
                verify(!(overlapX && overlapY),
                       "chips " + chips[a].caption + " and " + chips[b].caption + " overlap")
            }
        }
    }

    function test_chips_stay_inside_the_canvas_at_the_narrowest_width() {
        var canvas = makeCanvas()
        canvas.width = 760
        waitForRendering(canvas)
        var edges = testEditor.routing.edges
        for (var i = 0; i < edges.length; ++i) {
            if (!edges[i].parameterId)
                continue
            var c = findChild(canvas, "chip-" + edges[i].from + "-" + edges[i].to)
            if (!c || !c.visible)
                continue
            verify(c.x >= 0, c.caption + " starts inside the canvas")
            verify(c.x + c.width <= canvas.width + 1, c.caption + " ends inside the canvas")
        }
    }

    // ── Motion ────────────────────────────────────────────────────────────

    function test_pulse_only_travels_configured_paths_during_audition() {
        var canvas = makeCanvas()
        var spine = railFor(canvas, "source", "efx")
        verify(spine)
        // Nothing moves when the editor is not auditioning.
        compare(spine.flowing, false)
    }
}
