import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import XP60Studio

// The Living Effects Canvas.
//
// Replaces the routing diagram with a surface a musician reads rather than
// decodes. Three ideas do the work:
//
//   a spine. SOURCE -> EFX -> MIX OUT runs dead straight across the middle at
//   full weight. Chorus and Reverb sit above it as a send tier and return
//   below their own lane; a dry bypass and any alternate output run under it.
//   Where a stage sits tells you its role before you read a word.
//
//   semantic rails. Main, send, return and direct are four visual classes, not
//   seven equal cables. A send takes its weight from how much is being sent, a
//   silent path is a faint dashed trace. Turning a send up can never make it
//   outweigh the spine.
//
//   named routes. Every value is a chip that says what it governs -- CHORUS
//   SEND 127 -- and is the control for it. Nothing floats unexplained.
//
// Focus and isolation stop the musician having to hold the whole topology in
// mind: click a processor and it becomes the hero with its controls beside it;
// click a route and only that path stays lit.
//
// The C++ model remains the authority. Topology, levels, parameter ranges and
// which destinations are legal all come from `editor.routing` and
// `editor.effectValues`; this file positions, weights and animates them.
Item {
    id: root

    required property var editor

    // "" in Overview, otherwise "efx" | "chorus" | "reverb" | "mix" | "source".
    property string focusedNode: ""
    // parameterId of the route being isolated, "" for none.
    property string isolatedRoute: ""

    readonly property var routing: editor.routing
    readonly property var edges: routing.edges || []

    // Topology and state are deliberately separated.
    //
    // The model rebuilds the whole edge list on every patch change, so binding
    // a Repeater straight to it recreates every delegate each time a value
    // moves -- which destroys the chip the user is mid-drag on and drops the
    // gesture. `railModel` therefore changes only when the *shape* of the
    // routing changes, and delegates read live level and open state through
    // edgeState(). Same information, stable item lifetimes.
    property var railModel: []
    readonly property string topologySignature: {
        var e = root.edges, parts = []
        for (var i = 0; i < e.length; ++i)
            parts.push(e[i].from + ">" + e[i].to + ":" + (e[i].parameterId || ""))
        return parts.join("|")
    }
    onTopologySignatureChanged: root.rebuildRailModel()
    Component.onCompleted: root.rebuildRailModel()

    function rebuildRailModel() {
        var e = root.edges, next = []
        for (var i = 0; i < e.length; ++i)
            next.push({ "from": e[i].from, "to": e[i].to, "parameterId": e[i].parameterId || "" })
        root.railModel = next
    }
    // Live state for one edge of the stable model.
    function edgeState(from, to) {
        var e = root.edges
        for (var i = 0; i < e.length; ++i)
            if (e[i].from === from && e[i].to === to)
                return e[i]
        return null
    }
    function edgeOpen(from, to) {
        var live = edgeState(from, to)
        return live ? live.open : false
    }
    // Hit test for a canvas point, exposed here so it has one home and can be
    // exercised without reaching into the internals.
    function nodeAtPoint(px, py) {
        return routingLayer.nodeAt(px, py)
    }
    readonly property bool auditioning: editor.liveAudition
    readonly property color toneTint: Theme.toneColor(editor.selectedTone)

    readonly property bool showDirect: edges.some(function(e) { return e.to === "direct" || e.to === "unknown" })

    // The canvas is the workspace in the Effects section and in Expert; in the
    // other sections it is a strip that says where the sound goes without
    // taking room from the mixer and envelope below it. Same topology, same
    // lanes, less of it.
    readonly property bool detailed: editor.section === 4 || editor.disclosure === 2

    implicitHeight: stage.implicitHeight + inspector.implicitHeight
                    + (root.detailed ? header.implicitHeight + 2 * Metrics.spacingSm : 0)

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Effects canvas for %1").arg(routing.source || "")
    Accessible.description: routing.description || ""

    function clearFocus() {
        root.focusedNode = ""
        root.isolatedRoute = ""
    }

    // ── Which stage owns an edge, for focus and isolation ─────────────────
    function edgeTouches(edge, node) {
        return edge.from === node || edge.to === node
    }
    function railClass(edge) {
        if (edge.to === "direct" || edge.to === "unknown") return "direct"
        if (edge.from === "source" && edge.to === "efx") return "main"
        if (edge.from === "efx" && edge.to === "mix") return "main"
        if (edge.to === "chorus" || edge.to === "reverb") return "send"
        return "return"
    }
    function railTint(edge) {
        switch (railClass(edge)) {
        case "main": return root.toneTint
        case "send": return edge.to === "chorus" ? Theme.tone2 : Theme.tone3
        case "direct": return edge.to === "unknown" ? Theme.warning : Theme.accent
        default: return edge.from === "chorus" ? Theme.tone2
               : edge.from === "reverb" ? Theme.tone3 : Theme.textMuted
        }
    }
    // A short, unambiguous name for the value on this rail.
    function railCaption(edge) {
        if (edge.to === "chorus") return qsTr("CHORUS SEND")
        if (edge.to === "reverb") return qsTr("REVERB SEND")
        if (edge.from === "efx" && edge.to === "mix") return qsTr("EFX OUT")
        if (edge.from === "chorus") return qsTr("CHORUS RETURN")
        if (edge.from === "reverb") return qsTr("REVERB RETURN")
        if (edge.from === "source" && edge.to === "mix") return qsTr("DRY")
        if (edge.from === "source" && edge.to === "efx") return qsTr("TO EFX")
        if (edge.to === "direct") return qsTr("DIRECT OUT")
        if (edge.to === "unknown") return qsTr("UNMAPPED OUT")
        return qsTr("TONE LEVEL")
    }
    function levelFraction(edge) {
        var v = parseInt(edge.level)
        return isNaN(v) ? 0.6 : Math.max(0, Math.min(1, v / 127))
    }
    function nodeActive(id) {
        if (id === "source") return true
        for (var i = 0; i < edges.length; ++i)
            if (edges[i].open && edgeTouches(edges[i], id)) return true
        return false
    }
    // Focus dims everything not adjacent to the hero, so orientation survives.
    function nodeDimmed(id) {
        if (root.isolatedRoute !== "") {
            for (var i = 0; i < edges.length; ++i)
                if (edges[i].parameterId === root.isolatedRoute)
                    return !edgeTouches(edges[i], id)
            return true
        }
        if (root.focusedNode === "") return false
        if (root.focusedNode === id) return false
        for (var j = 0; j < edges.length; ++j)
            if (edges[j].parameterId !== undefined && edgeTouches(edges[j], root.focusedNode)
                && edgeTouches(edges[j], id)) return false
        return true
    }
    function edgeDimmed(edge) {
        if (root.isolatedRoute !== "") return edge.parameterId !== root.isolatedRoute
        if (root.focusedNode === "") return false
        return !edgeTouches(edge, root.focusedNode)
    }

    // ── Direct routing manipulation ───────────────────────────────────────
    // A stage may only be re-routed where the XP-60 actually offers a choice,
    // and the choices come from the model, never from this file. The parameter
    // enumerations are MIX/DIR for EFX, MIX/REVERB/MIX+REV for Chorus and
    // MIX/EFX/DIR for a Tone; the model has already dropped the undocumented
    // OUTPUT-2 value, so it can never be offered as a destination here.
    property string dragFrom: ""        // node id the drag started on
    property string dragParameter: ""   // parameter it would write
    property point dragPoint: Qt.point(0, 0)
    property string dragTarget: ""      // node id currently under the pointer
    readonly property bool routingDrag: dragFrom !== ""

    function outputParameterFor(node) {
        switch (node) {
        case "source": return "tone.output_assign"
        case "efx": return "common.efx_output_assign"
        case "chorus": return "common.chorus_output"
        default: return ""
        }
    }
    // Which canvas node a choice label denotes. Combined destinations such as
    // MIX+REV name two stages at once, so they are not drop targets: those stay
    // with the enumerated control in the inspector rather than being guessed at
    // from where a pointer happened to land.
    function nodeForChoice(label) {
        switch (label) {
        case "MIX": return "mix"
        case "EFX": return "efx"
        case "DIR": return "direct"
        case "REVERB": return "reverb"
        default: return ""
        }
    }
    function choiceIndexForNode(parameterId, node) {
        var p = root.editor.effectValues[parameterId]
        if (!p || !p.choices) return -1
        for (var i = 0; i < p.choices.length; ++i)
            if (nodeForChoice(p.choices[i]) === node) return i
        return -1
    }
    function isValidDestination(node) {
        return root.routingDrag && node !== root.dragFrom
               && choiceIndexForNode(root.dragParameter, node) >= 0
    }
    // The XP consequence of the drop, in the instrument's own terms.
    function dropConsequence() {
        var p = root.editor.effectValues[root.dragParameter]
        if (!p || root.dragTarget === "") return ""
        var index = choiceIndexForNode(root.dragParameter, root.dragTarget)
        if (index < 0) return ""
        return qsTr("%1 → %2").arg(p.name).arg(p.choices[index])
    }
    function cancelRouting() {
        root.dragFrom = ""
        root.dragParameter = ""
        root.dragTarget = ""
    }
    function commitRouting() {
        var index = choiceIndexForNode(root.dragParameter, root.dragTarget)
        if (index >= 0) {
            root.editor.beginEffectGesture()
            root.editor.editEffect(root.dragParameter, index)
            root.editor.endEffectGesture()
        }
        cancelRouting()
    }

    // ── Micro-visualisation inputs ────────────────────────────────────────
    // EFX is keyed to the algorithm's identity only. Which byte carries which
    // EFX control is verified for 3 of 40 algorithms, so nothing here reads
    // common.efx_parameter_*; doing so would draw a picture of a guess.
    function efxKind() {
        var n = (root.editor.mfxText || "").toUpperCase()
        if (n.indexOf("CHORUS") >= 0 && n.indexOf("FLANGER") < 0) return "chorus"
        if (n.indexOf("FLANGER") >= 0) return "flanger"
        if (n.indexOf("DELAY") >= 0) return "delay"
        if (n.indexOf("PHASER") >= 0) return "phaser"
        if (n.indexOf("ROTARY") >= 0) return "rotary"
        if (n.indexOf("COMPRESSOR") >= 0 || n.indexOf("LIMITER") >= 0) return "compressor"
        if (n.indexOf("OVERDRIVE") >= 0 || n.indexOf("OVER-DRIVE") >= 0 || n.indexOf("DISTORTION") >= 0) return "drive"
        if (n.indexOf("EQ") >= 0 || n.indexOf("SPECTRUM") >= 0 || n.indexOf("ENHANCER") >= 0) return "eq"
        if (n.indexOf("WAH") >= 0) return "filter"
        if (n.indexOf("PITCH") >= 0) return "pitch"
        if (n.indexOf("REVERB") >= 0) return "reverb"
        return "generic"
    }
    function paramFraction(id, fallback) {
        var p = root.editor.effectValues[id]
        if (!p || p.maximum === p.minimum) return fallback
        return (p.value - p.minimum) / (p.maximum - p.minimum)
    }
    // Reverb type decides the drawing: a delay type is not a room.
    function reverbKind() {
        var t = (root.editor.reverbText || "").toUpperCase()
        return t.indexOf("DLY") >= 0 || t.indexOf("DELAY") >= 0 ? "delay" : "reverb"
    }

    // ── Header ────────────────────────────────────────────────────────────
    RowLayout {
        id: header
        visible: root.detailed
        anchors { left: parent.left; right: parent.right; top: parent.top }
        spacing: Metrics.spacingSm

        XpLabel { text: qsTr("SIGNAL FLOW"); role: "overline"; secondary: true }
        XpLabel {
            text: root.routing.source || ""
            role: "caption"
            color: root.toneTint
        }
        Item { Layout.fillWidth: true }
        StatusPill {
            objectName: "canvasAuditionPill"
            visible: root.auditioning
            text: qsTr("AUDITION")
            tone: "live"
            pulsing: true
        }
        XpButton {
            objectName: "canvasBack"
            visible: root.focusedNode !== "" || root.isolatedRoute !== ""
            text: qsTr("Back to overview")
            variant: "ghost"
            compact: true
            onClicked: root.clearFocus()
        }
        XpLabel {
            text: root.editor.comparing ? qsTr("A · ORIGINAL") : qsTr("B · CURRENT")
            role: "caption"; secondary: true
        }
    }

    // ── Stage ─────────────────────────────────────────────────────────────
    Item {
        id: stage
        anchors {
            left: parent.left; right: parent.right
            top: root.detailed ? header.bottom : parent.top
            topMargin: root.detailed ? Metrics.spacingSm : 0
        }
        // Derived from the tiers below, never guessed. A constant height here
        // is what let the alternate output overlap the spine when the routing
        // grew a row.
        implicitHeight: (root.showDirect ? tierAlt + nodeH : laneDry) + (root.detailed ? 10 : 6)
        height: implicitHeight

        // Clicking bare canvas returns to Overview.
        MouseArea {
            anchors.fill: parent
            z: -1
            onClicked: root.clearFocus()
        }

        readonly property real nodeW: Math.max(112, Math.min(158, (width - 4 * Metrics.spacingLg) / 5))
        readonly property real nodeH: root.detailed ? 74 : 46
        readonly property real colStep: (width - nodeW) / 4
        // EFX is the hero of the chain, so it is physically bigger than the
        // stages that feed it. Hierarchy through size, not just colour.
        readonly property real efxW: nodeW * (root.detailed ? 1.2 : 1.0)
        readonly property real efxH: nodeH * (root.detailed ? 1.12 : 1.0)

        // Every rail class owns a horizontal channel, and no channel ever
        // crosses a node body. Reading top to bottom: returns, the send tier,
        // the two send channels, the spine, then the dry bypass and any
        // alternate output. Chips sit on their own channel, so a value can
        // never land on top of a processor or another value.
        readonly property real laneReturn: root.detailed ? 12 : 6
        readonly property real tierSend: root.detailed ? 28 : 0
        readonly property real laneSendSource: tierSend + nodeH + (root.detailed ? 22 : 8)
        readonly property real laneSendEfx: laneSendSource + (root.detailed ? 26 : 8)
        readonly property real tierMain: laneSendEfx + (root.detailed ? 24 : 10)
        readonly property real laneDry: tierMain + Math.max(nodeH, efxH) + (root.detailed ? 18 : 8)
        readonly property real laneDirect: laneDry + (root.detailed ? 20 : 8)
        readonly property real tierAlt: laneDirect + (root.detailed ? 14 : 6)
        // The spine's centreline: every main rail is dead straight along it.
        readonly property real spineY: tierMain + Math.max(nodeH, efxH) / 2

        function nodeItem(id) {
            switch (id) {
            case "source": return sourceNode
            case "efx": return efxNode
            case "chorus": return chorusNode
            case "reverb": return reverbNode
            case "mix": return mixNode
            default: return directNode
            }
        }

        // Orthogonal waypoints per rail class. Each class has its own lane, so
        // paths run in predictable channels instead of crossing the canvas.
        function railPoints(edge, index) {
            var a = nodeItem(edge.from), b = nodeItem(edge.to)
            var cls = root.railClass(edge)

            // The spine, dead straight along one centreline.
            if (cls === "main")
                return [Qt.point(a.x + a.width, spineY), Qt.point(b.x, spineY)]

            // Chorus into Reverb runs inside the send tier.
            if (edge.from === "chorus" && edge.to === "reverb")
                return [Qt.point(a.x + a.width, a.y + a.height / 2), Qt.point(b.x, b.y + b.height / 2)]

            // Sends rise from the spine into the send tier. Source and EFX get
            // separate channels and separate entry points, so two sends to the
            // same processor never share a line or a chip position.
            if (cls === "send") {
                var fromSource = edge.from === "source"
                var sx = a.x + a.width * (fromSource ? 0.62 : 0.42)
                var ex = b.x + b.width * (fromSource ? 0.34 : 0.66)
                var lane = fromSource ? laneSendSource : laneSendEfx
                return [Qt.point(sx, a.y), Qt.point(sx, lane), Qt.point(ex, lane), Qt.point(ex, b.y + b.height)]
            }

            if (cls === "direct") {
                var dx = a.x + a.width * 0.5
                return [Qt.point(dx, a.y + a.height), Qt.point(dx, laneDirect),
                        Qt.point(b.x + b.width * 0.5, laneDirect), Qt.point(b.x + b.width * 0.5, b.y)]
            }

            // The dry bypass runs under the spine and enters Mix from below.
            if (edge.from === "source" && edge.to === "mix") {
                var yx = a.x + a.width * 0.34
                var mx = b.x + b.width * 0.34
                return [Qt.point(yx, a.y + a.height), Qt.point(yx, laneDry),
                        Qt.point(mx, laneDry), Qt.point(mx, b.y + b.height)]
            }

            // Returns leave the top of their processor, cross above the send
            // tier and drop into Mix. That channel is otherwise empty, so a
            // return never has to thread between the sends.
            var rx = a.x + a.width * (edge.from === "chorus" ? 0.26 : 0.74)
            var rmx = b.x + b.width * (edge.from === "chorus" ? 0.4 : 0.66)
            return [Qt.point(rx, a.y), Qt.point(rx, laneReturn),
                    Qt.point(rmx, laneReturn), Qt.point(rmx, b.y)]
        }

        // Where the chip sits: on the straightest run of its own lane.
        function chipPoint(pts) {
            if (!pts || pts.length < 2)
                return Qt.point(0, 0)
            if (pts.length === 2)
                return Qt.point((pts[0].x + pts[1].x) / 2, pts[0].y)
            return Qt.point((pts[1].x + pts[2].x) / 2, pts[1].y)
        }

        // Two chips on one lane must never sit on top of each other: a value
        // hidden behind another value is the exact failure this redesign set
        // out to remove. Chips sharing a lane are separated left to right, in
        // lane order, and only as far as they actually need.
        function chipX(edge, index, chipWidth) {
            var here = chipPoint(railPoints(edge, index))
            var x = here.x - chipWidth / 2
            var edges = root.railModel
            for (var i = 0; i < index; ++i) {
                var other = edges[i]
                if (!other.parameterId)
                    continue
                var there = chipPoint(railPoints(other, i))
                // Same lane, within a chip width of each other.
                if (Math.abs(there.y - here.y) > 6)
                    continue
                var otherX = chipX(other, i, chipWidth)
                if (Math.abs(otherX - x) < chipWidth + Metrics.spacingXs)
                    x = otherX + chipWidth + Metrics.spacingXs
            }
            return Math.max(0, Math.min(width - chipWidth, x))
        }

        Repeater {
            id: rails
            model: root.railModel
            delegate: EffectSignalRail {
                required property var modelData
                required property int index
                // Live level and open state, without the delegate itself being
                // rebuilt when they change.
                readonly property var live: root.edgeState(modelData.from, modelData.to)
                anchors.fill: parent
                objectName: "rail-" + modelData.from + "-" + modelData.to
                points: stage.railPoints(modelData, index)
                rail: root.railClass(modelData)
                tint: root.railTint(modelData)
                open: live ? live.open : false
                strength: live ? root.levelFraction(live) : 0
                isolated: root.isolatedRoute !== "" && modelData.parameterId === root.isolatedRoute
                dimmed: root.edgeDimmed(modelData)
                flowing: root.auditioning && open
            }
        }

        // ── Nodes ────────────────────────────────────────────────────────
        EffectProcessorNode {
            compact: !root.detailed
            id: sourceNode
            objectName: "canvasSource"
            x: 0; y: stage.spineY - stage.nodeH / 2
            width: stage.nodeW; height: stage.nodeH
            role: "source"
            title: qsTr("SOURCE")
            detail: root.routing.structure || ""
            kind: "source"
            accentColor: root.toneTint
            badgeText: root.routing.source || ""
            active: true
            focused: root.focusedNode === "source"
            dimmed: root.nodeDimmed("source")
            onActivated: root.focusedNode = root.focusedNode === "source" ? "" : "source"
        }

        EffectProcessorNode {
            compact: !root.detailed
            id: efxNode
            objectName: "canvasEfx"
            x: stage.colStep; y: stage.spineY - stage.efxH / 2
            width: stage.efxW; height: stage.efxH
            role: "processor"
            title: qsTr("MFX / EFX")
            detail: root.editor.mfxText
            kind: root.efxKind()
            accentColor: Theme.tone4
            active: root.nodeActive("efx")
            focused: root.focusedNode === "efx"
            dimmed: root.nodeDimmed("efx")
            modified: (root.editor.effectValues["common.efx_type"] || {}).modified || false
            badgeText: (root.editor.effectValues["common.efx_output_assign"] || {}).display
                       ? qsTr("OUT · %1").arg(root.editor.effectValues["common.efx_output_assign"].display) : ""
            onActivated: root.focusedNode = root.focusedNode === "efx" ? "" : "efx"
        }

        EffectProcessorNode {
            compact: !root.detailed
            id: chorusNode
            objectName: "canvasChorus"
            x: 2 * stage.colStep; y: stage.tierSend
            width: stage.nodeW; height: stage.nodeH
            role: "processor"
            title: qsTr("CHORUS")
            detail: root.editor.chorusText
            kind: "chorus"
            accentColor: Theme.tone2
            vizAmount: root.paramFraction("common.chorus_depth", 0.5)
            vizSecondary: root.paramFraction("common.chorus_rate", 0.5)
            active: root.nodeActive("chorus")
            focused: root.focusedNode === "chorus"
            dimmed: root.nodeDimmed("chorus")
            modified: (root.editor.effectValues["common.chorus_level"] || {}).modified || false
            onActivated: root.focusedNode = root.focusedNode === "chorus" ? "" : "chorus"
        }

        EffectProcessorNode {
            compact: !root.detailed
            id: reverbNode
            objectName: "canvasReverb"
            x: 3 * stage.colStep; y: stage.tierSend
            width: stage.nodeW; height: stage.nodeH
            role: "processor"
            title: qsTr("REVERB")
            detail: root.editor.reverbText
            kind: root.reverbKind()
            accentColor: Theme.tone3
            vizAmount: root.paramFraction("common.reverb_time", 0.5)
            active: root.nodeActive("reverb")
            focused: root.focusedNode === "reverb"
            dimmed: root.nodeDimmed("reverb")
            modified: (root.editor.effectValues["common.reverb_level"] || {}).modified || false
            onActivated: root.focusedNode = root.focusedNode === "reverb" ? "" : "reverb"
        }

        EffectProcessorNode {
            compact: !root.detailed
            id: mixNode
            objectName: "canvasMix"
            x: 4 * stage.colStep; y: stage.spineY - stage.nodeH / 2
            width: stage.nodeW; height: stage.nodeH
            role: "destination"
            title: qsTr("MIX OUT")
            detail: root.editor.outputText
            kind: "output"
            accentColor: Theme.accent
            vizAmount: root.paramFraction("common.efx_mix_out_send_level", 0.8)
            active: root.nodeActive("mix")
            focused: root.focusedNode === "mix"
            dimmed: root.nodeDimmed("mix")
            badgeText: qsTr("DESTINATION")
            onActivated: root.focusedNode = root.focusedNode === "mix" ? "" : "mix"
        }

        EffectProcessorNode {
            compact: !root.detailed
            id: directNode
            objectName: "canvasDirect"
            // Shown as a ghost target while a routing drag is looking for a
            // destination, so DIR can be chosen even before anything uses it.
            visible: root.showDirect || root.isValidDestination("direct")
            opacity: root.showDirect ? 1.0 : 0.5
            x: 4 * stage.colStep; y: stage.tierAlt
            width: stage.nodeW; height: stage.nodeH
            role: "destination"
            title: root.routing.unknown ? qsTr("UNKNOWN") : qsTr("DIRECT OUT")
            detail: root.routing.unknown ? qsTr("Unmapped output") : qsTr("Bypasses sends")
            kind: "output"
            accentColor: root.routing.unknown ? Theme.warning : Theme.accent
            vizAmount: 1
            active: root.nodeActive("direct")
            dimmed: root.nodeDimmed("direct")
            interactive: false
        }

        // ── Routing handles ──────────────────────────────────────────────
        // Only appear on a focused stage that actually has a destination
        // choice, and only where the XP-60 offers one.
        Item {
            id: routingLayer
            anchors.fill: parent
            z: 30

            // A few pixels of tolerance around each stage. Dropping is a
            // gesture, not a precision task, and demanding the pointer be
            // strictly inside the rectangle is what makes a drop feel like it
            // failed for no reason.
            readonly property int dropPad: 12
            function nodeAt(px, py) {
                var ids = ["mix", "efx", "reverb", "direct", "chorus", "source"]
                for (var i = 0; i < ids.length; ++i) {
                    var n = stage.nodeItem(ids[i])
                    if (!n || !n.visible) continue
                    if (px >= n.x - dropPad && px <= n.x + n.width + dropPad
                        && py >= n.y - dropPad && py <= n.y + n.height + dropPad)
                        return ids[i]
                }
                return ""
            }

            // Destination rings. Valid targets light; everything else is left
            // alone rather than being marked invalid, so the canvas does not
            // flash with warnings during an ordinary drag.
            Repeater {
                model: ["mix", "efx", "reverb", "direct"]
                delegate: Rectangle {
                    required property string modelData
                    readonly property var target: stage.nodeItem(modelData)
                    visible: root.isValidDestination(modelData) && target.visible
                    x: target.x - 4; y: target.y - 4
                    width: target.width + 8; height: target.height + 8
                    radius: Metrics.radiusMd + 4
                    color: "transparent"
                    border.width: root.dragTarget === modelData ? 3 : 2
                    border.color: root.dragTarget === modelData ? Theme.success : Theme.accent
                    opacity: root.dragTarget === modelData ? 1.0 : 0.55
                }
            }

            // The line being dragged. A rotated rectangle rather than a
            // Canvas: repainting a Canvas on every mouse move is what made the
            // drag feel like it was catching, and this is one transform.
            Rectangle {
                id: dragLine
                visible: root.routingDrag
                readonly property var origin: root.routingDrag ? stage.nodeItem(root.dragFrom) : null
                readonly property real ox: origin ? origin.x + origin.width : 0
                readonly property real oy: origin ? origin.y + origin.height / 2 : 0
                x: ox
                y: oy - height / 2
                width: origin ? Math.hypot(root.dragPoint.x - ox, root.dragPoint.y - oy) : 0
                height: 2
                radius: 1
                color: root.dragTarget !== "" ? Theme.success : Theme.accent
                antialiasing: true
                transformOrigin: Item.Left
                rotation: origin ? Math.atan2(root.dragPoint.y - oy, root.dragPoint.x - ox) * 180 / Math.PI : 0
            }

            // The grab handle. Deliberately larger than it looks: a small dot
            // on a node edge is a frustrating target, so the visible ring is
            // 18 px while the grab area is the shell-wide minimum hit target.
            Rectangle {
                id: handle
                objectName: "routingHandle"
                readonly property string owner: root.focusedNode
                readonly property string parameterId: root.outputParameterFor(owner)
                readonly property var ownerNode: owner !== "" ? stage.nodeItem(owner) : null
                visible: root.detailed && ownerNode !== null && parameterId !== ""
                         && (root.editor.effectValues[parameterId] || null) !== null
                         && !root.editor.comparing
                width: 18; height: 18; radius: 9
                x: ownerNode ? ownerNode.x + ownerNode.width - width / 2 : 0
                y: ownerNode ? ownerNode.y + ownerNode.height / 2 - height / 2 : 0
                color: root.routingDrag || grab.containsMouse ? Theme.accent : Theme.surfaceRaised
                border.width: 2
                border.color: Theme.accent
                scale: root.routingDrag ? 1.25 : (grab.containsMouse ? 1.12 : 1.0)

                Behavior on scale {
                    enabled: !Motion.reducedMotion
                    NumberAnimation { duration: Motion.durationFast; easing.type: Motion.easingStandard }
                }
                Behavior on color {
                    enabled: !Motion.reducedMotion
                    ColorAnimation { duration: Motion.durationFast }
                }

                // An arrow, so the handle reads as "drag me somewhere" rather
                // than as another status dot.
                XpIcon {
                    anchors.centerIn: parent
                    name: "arrow-right"
                    width: 12; height: 12
                    color: root.routingDrag || grab.containsMouse ? Theme.textOnAccent : Theme.accent
                }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Change destination")

                MouseArea {
                    id: grab
                    anchors.centerIn: parent
                    width: Metrics.hitTarget
                    height: Metrics.hitTarget
                    hoverEnabled: true
                    cursorShape: root.routingDrag ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    // Same reason as the chips: the editor ScrollView would
                    // otherwise take the gesture and scroll the page instead.
                    preventStealing: true

                    function updateTarget(mouse) {
                        var p = mapToItem(routingLayer, mouse.x, mouse.y)
                        root.dragPoint = p
                        var hit = routingLayer.nodeAt(p.x, p.y)
                        if (root.isValidDestination(hit)) {
                            root.dragTarget = hit
                            return
                        }
                        if (root.dragTarget === "")
                            return
                        // Sticky: a target is only given up once the pointer is
                        // clearly away from it, so a shaky hand near an edge
                        // does not flicker the drop in and out.
                        var t = stage.nodeItem(root.dragTarget)
                        var pad = 24
                        if (p.x < t.x - pad || p.x > t.x + t.width + pad
                            || p.y < t.y - pad || p.y > t.y + t.height + pad)
                            root.dragTarget = ""
                    }

                    onPressed: function(mouse) {
                        root.dragFrom = handle.owner
                        root.dragParameter = handle.parameterId
                        root.dragTarget = ""
                        updateTarget(mouse)
                    }
                    onPositionChanged: function(mouse) {
                        if (root.routingDrag)
                            updateTarget(mouse)
                    }
                    onReleased: if (root.routingDrag) root.commitRouting()
                    onCanceled: root.cancelRouting()
                }
            }

            // What the drop will do, said in the instrument's own words before
            // it happens.
            Rectangle {
                visible: root.routingDrag
                x: Math.max(0, Math.min(stage.width - width, root.dragPoint.x + 14))
                y: Math.max(0, root.dragPoint.y - height - 8)
                width: preview.implicitWidth + 2 * Metrics.spacingSm
                height: preview.implicitHeight + Metrics.spacingXs
                radius: Metrics.radiusSm
                color: Theme.surfaceRaised
                border.width: 1
                border.color: root.dragTarget !== "" ? Theme.success : Theme.borderStrong
                XpLabel {
                    id: preview
                    objectName: "routingPreview"
                    anchors.centerIn: parent
                    role: "caption"
                    text: root.dragTarget !== "" ? root.dropConsequence()
                                                 : qsTr("Drag to a destination")
                    color: root.dragTarget !== "" ? Theme.success : Theme.textSecondary
                }
            }
        }

        // ── Route chips ──────────────────────────────────────────────────
        // One per addressable route, sitting on its own lane. These replace the
        // bare numbers that used to float beside the cables.
        Repeater {
            model: root.railModel
            delegate: EffectRouteChip {
                required property var modelData
                required property int index
                visible: root.detailed && (modelData.parameterId || "") !== ""
                objectName: "chip-" + modelData.from + "-" + modelData.to
                editor: root.editor
                parameterId: modelData.parameterId || ""
                caption: root.railCaption(modelData)
                tint: root.railTint(modelData)
                open: root.edgeOpen(modelData.from, modelData.to)
                isolated: root.isolatedRoute !== "" && modelData.parameterId === root.isolatedRoute
                dimmed: root.edgeDimmed(modelData)
                x: stage.chipX(modelData, index, width)
                y: stage.chipPoint(stage.railPoints(modelData, index)).y - height / 2
                // A chip being adjusted rises above its neighbours so the value
                // bubble is never clipped by the chip next to it.
                z: adjusting || isolated ? 20 : 10
                onIsolateRequested: root.isolatedRoute =
                    root.isolatedRoute === modelData.parameterId ? "" : modelData.parameterId
                onExactEntryRequested: {
                    root.isolatedRoute = modelData.parameterId
                    exactPopup.parameterId = modelData.parameterId
                    exactPopup.open()
                }
            }
        }
    }

    // ── Contextual inspector ──────────────────────────────────────────────
    // Appears beside the canvas rather than replacing it, so the topology stays
    // on screen while a stage or a route is being adjusted.
    Rectangle {
        id: inspector
        anchors { left: parent.left; right: parent.right; top: stage.bottom; topMargin: Metrics.spacingSm }
        visible: root.focusedNode !== "" || root.isolatedRoute !== ""
        implicitHeight: visible ? inspectorContent.implicitHeight + 2 * Metrics.cardPadding : 0
        height: implicitHeight
        radius: Metrics.radiusMd
        color: Theme.surfaceRaised
        border.width: 1
        border.color: Theme.borderSubtle

        Behavior on implicitHeight {
            enabled: !Motion.reducedMotion
            NumberAnimation { duration: Motion.durationNormal; easing.type: Motion.easingStandard }
        }

        ColumnLayout {
            id: inspectorContent
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: Metrics.cardPadding }
            spacing: Metrics.spacingSm

            // Route isolation: what this number means, and the control for it.
            ColumnLayout {
                visible: root.isolatedRoute !== ""
                Layout.fillWidth: true
                spacing: Metrics.spacingXs
                XpLabel {
                    objectName: "isolatedRouteTitle"
                    text: (root.editor.effectValues[root.isolatedRoute] || {}).label || ""
                    role: "body"
                    font.weight: Typography.weightMedium
                }
                XpLabel {
                    objectName: "isolatedRouteExplanation"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    role: "caption"
                    secondary: true
                    text: {
                        var p = root.editor.effectValues[root.isolatedRoute]
                        if (!p) return ""
                        return qsTr("%1 · drag the chip or this control to adjust · 0 silences this path")
                            .arg(p.name)
                    }
                }
                EffectParameterControl {
                    objectName: "isolatedRouteControl"
                    editor: root.editor
                    parameterId: root.isolatedRoute
                    highlighted: true
                }
            }

            // Processor focus: the controls that stage actually owns.
            ColumnLayout {
                visible: root.isolatedRoute === "" && root.focusedNode !== ""
                Layout.fillWidth: true
                spacing: Metrics.spacingSm

                XpLabel {
                    objectName: "focusTitle"
                    text: root.focusedNode === "efx" ? root.editor.mfxText
                        : root.focusedNode === "chorus" ? qsTr("Chorus · %1").arg(root.editor.chorusText)
                        : root.focusedNode === "reverb" ? qsTr("Reverb · %1").arg(root.editor.reverbText)
                        : root.focusedNode === "mix" ? qsTr("Mix output")
                        : root.routing.source || ""
                    role: "body"
                    font.weight: Typography.weightMedium
                }

                // EFX parameter slots are established for 3 of 40 algorithms,
                // so the focused EFX offers identity and routing -- which are
                // verified -- and points at Expert for the raw bytes rather
                // than labelling them with control names it cannot justify.
                XpLabel {
                    visible: root.focusedNode === "efx"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    role: "caption"
                    color: Theme.warning
                    text: qsTr("Algorithm parameter mapping is unverified for this algorithm. Use Expert for raw EFX bytes.")
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingMd
                    Repeater {
                        model: {
                            switch (root.focusedNode) {
                            case "efx": return ["common.efx_output_assign", "common.efx_mix_out_send_level",
                                                "common.efx_chorus_send_level", "common.efx_reverb_send_level"]
                            case "chorus": return ["common.chorus_level", "common.chorus_rate", "common.chorus_depth",
                                                   "common.chorus_pre_delay", "common.chorus_feedback",
                                                   "common.chorus_output"]
                            case "reverb": return ["common.reverb_type", "common.reverb_level", "common.reverb_time",
                                                   "common.reverb_hf_damp"]
                            case "mix": return ["tone.mix_efx_send_level", "tone.output_assign"]
                            case "source": return ["tone.mix_efx_send_level", "tone.chorus_send_level",
                                                   "tone.reverb_send_level"]
                            default: return []
                            }
                        }
                        delegate: Loader {
                            required property string modelData
                            active: (root.editor.effectValues[modelData] || null) !== null
                            sourceComponent: ((root.editor.effectValues[modelData] || {}).choices || []).length > 0
                                             ? choiceComponent : sliderComponent
                            property string pid: modelData
                        }
                    }
                }
            }
        }
    }

    Component {
        id: sliderComponent
        EffectParameterControl {
            editor: root.editor
            parameterId: parent.pid
            exact: root.editor.disclosure === 2
        }
    }
    Component {
        id: choiceComponent
        EffectChoiceControl {
            editor: root.editor
            parameterId: parent.pid
        }
    }

    // Exact numeric entry, for when a gesture is not precise enough.
    QQC.Popup {
        id: exactPopup
        property string parameterId: ""
        x: Math.max(0, root.width / 2 - width / 2)
        y: Math.max(0, root.height / 2 - height / 2)
        width: 210
        padding: Metrics.cardPadding
        modal: true
        dim: false
        background: Rectangle {
            color: Theme.surfaceRaised
            radius: Metrics.radiusMd
            border.color: Theme.borderStrong
            border.width: 1
        }
        contentItem: EffectParameterControl {
            objectName: "exactRouteControl"
            editor: root.editor
            parameterId: exactPopup.parameterId
            exact: true
            highlighted: true
        }
        onClosed: root.editor.endEffectGesture()
    }
}
