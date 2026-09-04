import QtQuick
import QtQuick.Layouts
import XP60Studio

// Selected Tone/Structure pair and its parallel effect sends. C++ supplies
// the topology; QML supplies only node positions, arrow geometry and styling.
Item {
    id: root
    required property var editor
    // Keep the envelope/ranges close to the mixer; Effects and Expert expose
    // the larger routing view with exact send readouts.
    readonly property bool detailed: editor.section === 4 || editor.disclosure === 2
    readonly property var routing: editor.routing
    readonly property bool showAlternateOutput: (routing.edges || []).some(function(edge) {
        return edge.to === "direct" || edge.to === "unknown"
    })
    readonly property real sourceCenterX: sourceNode.x + sourceNode.width / 2
    readonly property real nodeWidth: Math.min(150, (width - 4 * Metrics.controlHeightLg) / 5)
    readonly property real step: (width - nodeWidth) / 4
    readonly property real rowHeight: detailed ? Metrics.controlHeightSm * 2 : Metrics.controlHeightLg
    readonly property real mainY: rowHeight + (detailed ? Metrics.spacingXl : Metrics.spacingMd) + Metrics.spacingLg
    readonly property real directY: mainY + rowHeight + Metrics.spacingXl + Metrics.spacingMd
    implicitHeight: diagram.y + diagram.height + (detailed ? Metrics.spacingXs + legend.implicitHeight : 0)

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Configured routing for %1").arg(routing.source || "")
    Accessible.description: routing.description || ""

    RowLayout {
        id: heading
        visible: root.detailed
        width: parent.width
        XpLabel { text: qsTr("SIGNAL FLOW"); role: "overline"; secondary: true }
        XpLabel {
            Layout.fillWidth: true
            text: routing.source || ""
            role: "caption"
            color: Theme.toneColor(root.editor.selectedTone)
        }
        XpLabel {
            text: root.editor.comparing ? qsTr("A · ORIGINAL") : qsTr("B · CURRENT")
            role: "caption"; secondary: true
        }
    }
    Item {
        id: diagram
        y: root.detailed ? heading.height + Metrics.spacingSm : 0
        width: parent.width
        height: root.showAlternateOutput ? root.directY + root.rowHeight
                : root.mainY + root.rowHeight + Metrics.spacingXl + Metrics.spacingXs

        function node(id) {
            switch (id) {
            case "source": return sourceNode
            case "efx": return efxNode
            case "chorus": return chorusNode
            case "reverb": return reverbNode
            case "mix": return mixNode
            default: return directNode
            }
        }
        function active(id) {
            var edges = root.routing.edges || []
            for (var i = 0; i < edges.length; ++i)
                if (edges[i].open && (edges[i].from === id || edges[i].to === id)) return true
            return false
        }
        function path(edge) {
            var a = node(edge.from), b = node(edge.to)
            var start = Qt.point(a.x + a.width, a.y + a.height / 2)
            var end = Qt.point(b.x, b.y + b.height / 2)
            if (edge.from === "source" && edge.to === "efx") return [start, end]
            if (edge.from === "chorus" && edge.to === "reverb") return [start, end]
            if (edge.to === "chorus" || edge.to === "reverb") {
                var top = edge.from === "source"
                start = Qt.point(a.x + a.width * (edge.to === "chorus" ? 0.3 : 0.7), top ? a.y : a.y + a.height)
                end = Qt.point(b.x + b.width * (top ? 0.35 : 0.65), top ? b.y : b.y + b.height)
                var lane = top ? Metrics.spacingMd : root.mainY + root.rowHeight + Metrics.spacingMd
                if (edge.to === "reverb") lane += top ? -Metrics.spacingSm : Metrics.spacingSm
                return [start, Qt.point(start.x, lane), Qt.point(end.x, lane), end]
            }
            if (edge.to === "mix") {
                var lower = edge.from === "source" || edge.from === "efx"
                var laneY = edge.from === "source" ? root.mainY + root.rowHeight + Metrics.spacingXl
                          : edge.from === "efx" ? root.mainY + root.rowHeight + Metrics.spacingSm
                          : edge.from === "chorus" ? root.rowHeight + Metrics.spacingLg + Metrics.spacingSm : root.rowHeight + 2 * Metrics.spacingLg
                start = Qt.point(a.x + a.width * 0.85, a.y + a.height)
                end = Qt.point(b.x + b.width * (edge.from === "source" || edge.from === "chorus" ? 0.3 : 0.7),
                               lower ? b.y + b.height : b.y)
                return [start, Qt.point(start.x, laneY), Qt.point(end.x, laneY), end]
            }
            start = Qt.point(a.x + a.width / 2, a.y + a.height)
            return [start, Qt.point(start.x, end.y), end]
        }
        Repeater {
            model: root.routing.edges || []
            delegate: SignalFlowConnector {
                required property var modelData
                anchors.fill: parent
                objectName: "route-" + modelData.from + "-" + modelData.to
                points: diagram.path(modelData)
                open: modelData.open
                lineColor: modelData.to === "unknown" ? Theme.warning
                         : modelData.from === "source" ? Theme.toneColor(root.editor.selectedTone)
                         : modelData.from === "efx" ? Theme.tone4
                         : modelData.from === "chorus" ? Theme.tone2 : Theme.tone3
                label: root.detailed ? modelData.level : ""
                labelPosition: points.length === 2
                    ? Qt.point((points[0].x + points[1].x) / 2, points[0].y)
                    : Qt.point((points[1].x + points[2].x) / 2, points[1].y)
            }
        }
        SignalFlowNode {
            id: sourceNode
            objectName: "structureNode"
            y: root.mainY
            width: root.nodeWidth; height: root.rowHeight
            title: qsTr("STRUCTURE"); detail: root.routing.structure || ""
            detailLines: root.detailed ? 2 : 1
            highlighted: true; accentColor: Theme.toneColor(root.editor.selectedTone)
        }
        SignalFlowNode {
            id: efxNode
            objectName: "routingEfx"
            x: root.step; y: root.mainY
            width: root.nodeWidth; height: root.rowHeight
            title: qsTr("MFX / EFX"); detail: root.editor.mfxText
            detailLines: root.detailed ? 2 : 1
            highlighted: diagram.active("efx"); accentColor: Theme.tone4
        }
        SignalFlowNode {
            id: chorusNode
            objectName: "routingChorus"
            x: 2 * root.step
            y: Metrics.spacingLg
            width: root.nodeWidth; height: root.rowHeight
            title: qsTr("CHORUS"); detail: root.editor.chorusText
            detailLines: root.detailed ? 2 : 1
            highlighted: diagram.active("chorus"); accentColor: Theme.tone2
        }
        SignalFlowNode {
            id: reverbNode
            objectName: "routingReverb"
            x: 3 * root.step
            y: Metrics.spacingLg
            width: root.nodeWidth; height: root.rowHeight
            title: qsTr("REVERB"); detail: root.editor.reverbText
            detailLines: root.detailed ? 2 : 1
            highlighted: diagram.active("reverb"); accentColor: Theme.tone3
        }
        SignalFlowNode {
            id: mixNode
            objectName: "routingMix"
            x: 4 * root.step; y: root.mainY
            width: root.nodeWidth; height: root.rowHeight
            title: qsTr("MIX OUT"); detail: root.editor.outputText
            detailLines: root.detailed ? 2 : 1
            highlighted: diagram.active("mix")
        }
        SignalFlowNode {
            id: directNode
            visible: root.showAlternateOutput
            objectName: "routingDirect"
            x: 4 * root.step; y: root.directY
            width: root.nodeWidth; height: root.rowHeight
            title: root.routing.unknown ? qsTr("UNKNOWN") : qsTr("DIRECT OUT")
            detail: root.routing.unknown ? qsTr("Unmapped output") : qsTr("Bypass sends")
            detailLines: root.detailed ? 2 : 1
            highlighted: root.routing.unknown || diagram.active("direct")
            accentColor: root.routing.unknown ? Theme.warning : Theme.accent
        }
    }
    XpLabel {
        id: legend
        visible: root.detailed
        y: diagram.y + diagram.height + Metrics.spacingXs
        width: parent.width
        text: qsTr("Configured sends · XP values · dashed = zero path. Tone and system effect switches are not shown.")
        role: "caption"; secondary: true; wrapMode: Text.WordWrap
    }
}
