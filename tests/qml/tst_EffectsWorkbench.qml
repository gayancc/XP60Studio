import QtQuick
import QtTest
import XP60Studio

TestCase {
    id: tests
    name: "EffectsWorkbench"
    when: windowShown
    width: 1200; height: 1000
    visible: true
    Component { id: workbench; EffectsWorkbench { editor: testEditor; width: 1150 } }
    Component { id: route; EffectRouteSelector { editor: testEditor; parameterId: "tone.output_assign"; title: "Tone"; width: 420 } }
    Component { id: graph; EffectParameterGraph { editor: testEditor; width: 600; height: 230; xParameter: "common.chorus_rate"; yParameter: "common.chorus_depth" } }
    Component { id: control; EffectParameterControl { editor: testEditor; parameterId: "common.chorus_rate"; width: 150; height: 180 } }
    function value(id) { return testEditor.effectValues[id].value }
    function init() {
        testHarness.reloadPatch()
        testEditor.disarmWrite()
        testEditor.disclosure = 1
        testEditor.section = 4
        testEditor.selectedTone = 1
        testEditor.effectPage = 0
    }
    function test_all_algorithms_have_identity_topology_and_safe_stages() {
        var algorithms = testEditor.effectAlgorithms
        compare(algorithms.length, 40)
        for (var i = 0; i < 40; ++i) {
            compare(algorithms[i].value, i)
            compare(algorithms[i].topology, i < 25 ? "SINGLE" : i < 37 ? "SERIES" : "PARALLEL")
            compare(algorithms[i].stages.length, i < 25 ? 1 : 2)
            verify(!algorithms[i].bound)
            verify(algorithms[i].stages[0].controls.length > 0)
            testEditor.editEffect("common.efx_type", i)
            compare(value("common.efx_type"), i)
        }
        compare(algorithms[25].stages[0].name, "OVERDRIVE")
        compare(algorithms[25].stages[1].name, "CHORUS")
        compare(algorithms[39].name, "CHORUS/FLANGER")
    }
    function test_unverified_and_invalid_writes_have_no_history() {
        var before = testEditor.differenceSummary
        testEditor.editEffect("common.efx_parameter_1", 45)
        testEditor.editEffect("common.efx_output_assign", 2)
        testEditor.editEffect("tone.output_assign", 3)
        testEditor.editEffect("common.chorus_level", 128)
        testEditor.editEffect("common.patch_level", 50)
        compare(testEditor.differenceSummary, before)
        verify(!testEditor.canUndo)
        verify(testEditor.effectValues["common.efx_parameter_1"] === undefined)
    }
    function test_gesture_undo_redo_and_original_marker() {
        var originalRate = value("common.chorus_rate"), originalDepth = value("common.chorus_depth")
        testEditor.beginEffectGesture()
        for (var i = 10; i < 90; ++i) {
            testEditor.editEffect("common.chorus_rate", i)
            testEditor.editEffect("common.chorus_depth", 100 - i)
        }
        testEditor.endEffectGesture()
        compare(value("common.chorus_rate"), 89)
        compare(testEditor.effectValues["common.chorus_rate"].original, originalRate)
        testEditor.undo()
        compare(value("common.chorus_rate"), originalRate)
        compare(value("common.chorus_depth"), originalDepth)
        verify(!testEditor.canUndo)
        testEditor.redo()
        compare(value("common.chorus_rate"), 89)
        compare(value("common.chorus_depth"), 11)
        testEditor.comparing = true
        compare(value("common.chorus_rate"), originalRate)
        testEditor.editEffect("common.chorus_rate", 1)
        testEditor.comparing = false
        compare(value("common.chorus_rate"), 89)
    }
    function test_real_knob_gesture_and_keyboard() {
        var c = createTemporaryObject(control, tests)
        var knob = findChild(c, "effectKnob-common.chorus_rate")
        var original = value("common.chorus_rate")
        mousePress(knob, 34, 34)
        mouseMove(knob, 34, 10, 20)
        mouseMove(knob, 34, -15, 20)
        mouseRelease(knob, 34, -15)
        verify(value("common.chorus_rate") !== original)
        testEditor.undo()
        compare(value("common.chorus_rate"), original)
        verify(!testEditor.canUndo)
        knob.forceActiveFocus()
        keyClick(Qt.Key_Right, Qt.ShiftModifier)
        compare(value("common.chorus_rate"), Math.min(127, original + 1))
    }
    function test_real_graph_drag_is_one_edit() {
        var g = createTemporaryObject(graph, tests)
        var point = findChild(g, "effectGraphPoint")
        var originalRate = value("common.chorus_rate"), originalDepth = value("common.chorus_depth")
        var destination = g.mapToItem(point, 350, 50)
        mousePress(point, 7, 7)
        mouseMove(point, destination.x, destination.y, 30)
        mouseRelease(point, 7, 7)
        verify(value("common.chorus_rate") !== originalRate)
        testEditor.undo()
        compare(value("common.chorus_rate"), originalRate)
        compare(value("common.chorus_depth"), originalDepth)
        verify(!testEditor.canUndo)
    }
    function test_paired_output_owner_and_graph_synchronization() {
        testEditor.setToneSetting("common.structure_type_1_2", 1)
        // Use Expert's verified Structure identity, not a guessed byte offset.
        var owner = testEditor.routing.outputTone
        compare(owner, 2)
        testEditor.editEffect("tone.output_assign", 2)
        compare(value("tone.output_assign"), 2)
        compare(testEditor.routing.edges.length, 1)
        compare(testEditor.routing.edges[0].to, "direct")
        var g = createTemporaryObject(graph, tests)
        g.adjust(0.25, 0.75)
        compare(value("common.chorus_rate"), 32)
        compare(value("common.chorus_depth"), 95)
        testEditor.editEffect("common.chorus_rate", 127)
        compare(g.nx, 1)
    }
    function test_route_click_drag_and_escape() {
        var r = createTemporaryObject(route, tests)
        var direct = findChild(r, "routeTarget-tone.output_assign-2")
        mouseClick(direct)
        compare(value("tone.output_assign"), 2)
        var socket = findChild(r, "routeSocket-tone.output_assign")
        var target = findChild(r, "routeTarget-tone.output_assign-1")
        var p = target.mapToItem(socket, target.width / 2, target.height / 2)
        mousePress(socket, 16, 16)
        mouseMove(socket, p.x, p.y, 30)
        compare(r.candidate, 1)
        keyClick(Qt.Key_Escape)
        mouseRelease(socket, p.x, p.y)
        compare(value("tone.output_assign"), 2)
        mousePress(socket, 16, 16)
        mouseMove(socket, p.x, p.y, 30)
        mouseRelease(socket, p.x, p.y)
        compare(value("tone.output_assign"), 1)
    }
    function test_pages_palette_and_compare() {
        var w = createTemporaryObject(workbench, tests)
        for (var i = 0; i < 5; ++i) { testEditor.effectPage = i; compare(w.page, i) }
        testEditor.effectPage = 1
        for (i = 0; i < 40; ++i) {
            testEditor.editEffect("common.efx_type", i)
            compare(w.algorithmIndex, i)
        }
        w.browserOpen = true
        var algorithm = findChild(w, "algorithm-39")
        verify(algorithm)
        algorithm.clicked()
        compare(value("common.efx_type"), 39)
        verify(!w.browserOpen)
        testEditor.comparing = true
        verify(!algorithm.enabled)
    }
}
