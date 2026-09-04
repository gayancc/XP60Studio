import QtQuick
import XP60Studio

// Piano keyboard showing a note range, with draggable edges.
// Note numbers are MIDI 0..127 (C-1 .. G9), matching Roland's key range.
Item {
    id: root

    property int lowerNote: 0
    property int upperNote: 127
    property int firstNote: 0
    property int lastNote: 127
    property color accentColor: Theme.accent
    property bool interactive: true
    signal rangeEdited(int lower, int upper)

    implicitHeight: 54

    readonly property int noteCount: lastNote - firstNote + 1
    readonly property real noteWidth: width / Math.max(1, noteCount)
    function isBlack(n) {
        var p = ((n % 12) + 12) % 12
        return p === 1 || p === 3 || p === 6 || p === 8 || p === 10
    }
    function noteAt(x) {
        return Math.max(firstNote, Math.min(lastNote, firstNote + Math.floor(x / Math.max(1, noteWidth))))
    }

    Rectangle {
        anchors.fill: parent
        radius: Metrics.radiusSm
        color: Theme.surfaceSunken
        border.width: 1
        border.color: Theme.borderSubtle
        clip: true

        // White keys first, black keys over them: 128 equal slices would read
        // as a barcode rather than a keyboard.
        Item {
            id: keys
            anchors.fill: parent
            anchors.margins: 1

            readonly property int whiteCount: {
                var n = 0
                for (var i = root.firstNote; i <= root.lastNote; ++i)
                    if (!root.isBlack(i)) n++
                return Math.max(1, n)
            }
            readonly property real whiteWidth: width / whiteCount
            // Index of a note among the white keys to its left.
            function whiteIndex(note) {
                var n = 0
                for (var i = root.firstNote; i < note; ++i)
                    if (!root.isBlack(i)) n++
                return n
            }

            Repeater {
                model: root.noteCount
                delegate: Rectangle {
                    required property int index
                    readonly property int note: root.firstNote + index
                    readonly property bool inRange: note >= root.lowerNote && note <= root.upperNote
                    visible: !root.isBlack(note)
                    x: keys.whiteIndex(note) * keys.whiteWidth
                    width: keys.whiteWidth
                    height: parent.height
                    color: inRange ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.55)
                                   : "#C8D0DC"
                    // Across the full 0..127 range a white key is a few pixels
                    // wide; a border on each one would leave only border.
                    border.width: keys.whiteWidth >= 5 ? 1 : 0
                    border.color: "#0A0E16"
                }
            }

            Repeater {
                model: root.noteCount
                delegate: Rectangle {
                    required property int index
                    readonly property int note: root.firstNote + index
                    readonly property bool inRange: note >= root.lowerNote && note <= root.upperNote
                    visible: root.isBlack(note)
                    // Sits astride the boundary between its two white neighbours.
                    width: Math.max(2, keys.whiteWidth * 0.62)
                    x: keys.whiteIndex(note) * keys.whiteWidth - width / 2
                    height: parent.height * 0.62
                    color: inRange ? Qt.darker(root.accentColor, 1.6) : "#0A0E16"
                    border.width: keys.whiteWidth >= 5 ? 1 : 0
                    border.color: "#05070C"
                }
            }
        }
    }

    // Range bar under the keys, with grab handles at each end.
    Rectangle {
        id: bar
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 6
        radius: 3
        color: Theme.surfaceSunken

        Rectangle {
            x: (root.lowerNote - root.firstNote) * root.noteWidth
            width: Math.max(4, (root.upperNote - root.lowerNote + 1) * root.noteWidth)
            height: parent.height
            radius: 3
            color: root.accentColor
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.interactive
        property int grabbed: -1 // 0 lower, 1 upper
        cursorShape: root.interactive ? Qt.SizeHorCursor : Qt.ArrowCursor
        onPressed: function(mouse) {
            var n = root.noteAt(mouse.x)
            grabbed = Math.abs(n - root.lowerNote) <= Math.abs(n - root.upperNote) ? 0 : 1
            apply(n)
        }
        onPositionChanged: function(mouse) { if (pressed) apply(root.noteAt(mouse.x)) }
        function apply(n) {
            if (grabbed === 0)
                root.rangeEdited(Math.min(n, root.upperNote), root.upperNote)
            else
                root.rangeEdited(root.lowerNote, Math.max(n, root.lowerNote))
        }
    }
}
