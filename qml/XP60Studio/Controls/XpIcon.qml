import QtQuick
import XP60Studio

// Original line icons on a 24-unit grid. No font glyphs, platform emoji or
// external assets, so every icon in the application shares one stroke weight,
// one cap/join style and one optical size.
//
// Text characters used as icons — a chevron, an arrow, a disclosure triangle —
// are the thing this component exists to replace: they carry the text font's
// stroke weight and its own vertical centring, so they never line up with the
// icons beside them.
Canvas {
    id: root
    property string name: ""
    property color color: Theme.textSecondary
    // Informational icons take their meaning from the label beside them.
    // Interactive ones are wrapped by a control that supplies the target size
    // and the accessible name.
    implicitWidth: name.length > 0 ? Metrics.iconSize : 0
    implicitHeight: name.length > 0 ? Metrics.iconSize : 0
    Accessible.ignored: true // The owning control supplies its accessible name.
    onNameChanged: requestPaint()
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        var c = getContext("2d")
        c.reset()
        c.scale(width / 24, height / 24)
        c.strokeStyle = color; c.lineWidth = 1.6
        c.lineCap = "round"; c.lineJoin = "round"
        function line(points) {
            c.beginPath(); c.moveTo(points[0], points[1])
            for (var i = 2; i < points.length; i += 2) c.lineTo(points[i], points[i + 1])
            c.stroke()
        }
        function box(x,y,w,h) { c.strokeRect(x,y,w,h) }
        function circle(x,y,r) { c.beginPath(); c.arc(x,y,r,0,Math.PI*2); c.stroke() }
        function disc(x,y,r) { c.beginPath(); c.arc(x,y,r,0,Math.PI*2); c.fillStyle = color; c.fill() }
        // Five-pointed star on the 24-unit grid, outlined or filled. Filled is
        // the only icon that paints a solid shape: a favourite and a rating
        // have to read as on/off from across the screen, which an outline of
        // this size does not.
        function star(filled) {
            c.beginPath()
            for (var k = 0; k < 10; ++k) {
                var r = (k % 2 === 0) ? 9 : 3.8
                var a = -Math.PI / 2 + k * Math.PI / 5
                var px = 12 + r * Math.cos(a)
                var py = 12 + r * Math.sin(a)
                if (k === 0) c.moveTo(px, py); else c.lineTo(px, py)
            }
            c.closePath()
            if (filled) { c.fillStyle = color; c.fill() } else { c.stroke() }
        }
        switch (name) {
        // Navigation ----------------------------------------------------------
        case "dashboard": box(3,3,7,7); box(14,3,7,7); box(3,14,7,7); box(14,14,7,7); break
        case "library": box(3,4,4,16); box(10,4,4,16); line([17,4,21,19]); break
        case "editor": line([4,20,5,15,16,4,20,8,9,19,4,20]); line([14,6,18,10]); break
        case "banks": box(3,4,18,16); line([3,9,21,9]); line([9,9,9,20]); line([15,9,15,20]); line([3,14,21,14]); break
        case "performance": line([9,17,9,5,19,3,19,15]); circle(6,18,3); circle(16,16,3); break
        case "compare": line([3,8,21,8,17,4]); line([21,16,3,16,7,20]); break
        case "devices": box(3,5,18,14); circle(8,12,2); line([14,9,18,9]); line([14,13,18,13]); line([7,22,7,19]); line([17,22,17,19]); break
        case "settings": circle(12,12,4); circle(12,12,8); for(var j=0;j<8;++j) { var a2=j*Math.PI/4; line([12+8*Math.cos(a2),12+8*Math.sin(a2),12+10*Math.cos(a2),12+10*Math.sin(a2)]) } break

        // Direction and disclosure -------------------------------------------
        // Drawn as strokes on the same grid as everything else, so a chevron
        // in a combo box and a chevron in a tree row are the same shape.
        case "chevron-down": line([6,10,12,16,18,10]); break
        case "chevron-up": line([6,15,12,9,18,15]); break
        case "chevron-right": line([10,6,16,12,10,18]); break
        case "chevron-left": line([14,6,8,12,14,18]); break
        case "arrow-right": line([4,12,20,12]); line([15,7,20,12,15,17]); break
        case "arrow-left": line([20,12,4,12]); line([9,7,4,12,9,17]); break

        // Actions -------------------------------------------------------------
        case "plus": line([12,5,12,19]); line([5,12,19,12]); break
        case "minus": line([5,12,19,12]); break
        case "close": line([6,6,18,18]); line([18,6,6,18]); break
        case "check": line([5,13,10,18,19,6]); break
        case "search": circle(10.5,10.5,6.5); line([15.5,15.5,20,20]); break
        case "copy": box(4,4,12,12); line([8,20,20,20]); line([20,8,20,20]); break
        case "refresh":
            c.beginPath(); c.arc(12,12,8,-Math.PI*0.35,Math.PI*1.15); c.stroke()
            line([17,3,18.5,8.5,13,9.5]); break
        case "undo": line([8,4,3,9,8,14]); line([3,9,14,9]); c.beginPath();c.arc(14,14,5,-Math.PI/2,Math.PI/2);c.stroke();line([14,19,10,19]); break
        case "redo": line([16,4,21,9,16,14]);line([21,9,10,9]);c.beginPath();c.arc(10,14,5,-Math.PI/2,-3*Math.PI/2,true);c.stroke();line([10,19,14,19]);break
        case "power": c.beginPath();c.arc(12,13,8,-Math.PI/3,4*Math.PI/3);c.stroke();line([12,2,12,12]);break
        // Import and export share a tray and differ only in the arrow's
        // direction, so the pair reads as one idea at a glance rather than as
        // two unrelated glyphs.
        case "import": line([12,3,12,15]); line([7,10,12,15,17,10]); line([4,17,4,21,20,21,20,17]); break
        case "export": line([12,15,12,3]); line([7,8,12,3,17,8]); line([4,17,4,21,20,21,20,17]); break

        // Status --------------------------------------------------------------
        case "alert": line([12,4,21,20,3,20,12,4]); line([12,10,12,15]); disc(12,17.6,0.9); break
        case "info": circle(12,12,8); line([12,11,12,16]); disc(12,8.4,0.9); break

        // Domain --------------------------------------------------------------
        case "envelope": line([3,19,7,5,11,11,17,12,21,19]);line([3,3,3,21,22,21]);break
        case "keyboard": box(2,5,20,14);line([6,5,6,19]);line([10,5,10,19]);line([14,5,14,19]);line([18,5,18,19]);break
        // Direction is carried by the arrow's shape, so IN and OUT stay
        // distinguishable at 14 px and without colour. A circled arrow, which
        // these were, turns to mush at log-row size.
        case "midi-in": line([12,4,12,18]); line([6,12,12,18,18,12]); line([4,21,20,21]); break
        case "midi-out": line([12,20,12,6]); line([6,12,12,6,18,12]); line([4,3,20,3]); break
        case "activity": line([2,12,7,12,10,5,14,19,17,12,22,12]); break
        case "star": star(false); break
        case "star-filled": star(true); break
        }
    }
}
