import QtQuick
import XP60Studio

// Original 24-unit line icons. No font glyphs, platform emoji or external assets.
Canvas {
    id: root
    property string name: ""
    property color color: Theme.textSecondary
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
        switch (name) {
        case "dashboard": box(3,3,7,7); box(14,3,7,7); box(3,14,7,7); box(14,14,7,7); break
        case "library": box(3,4,4,16); box(10,4,4,16); line([17,4,21,19]); break
        case "editor": line([4,20,5,15,16,4,20,8,9,19,4,20]); line([14,6,18,10]); break
        case "banks": box(3,4,18,16); line([3,9,21,9]); line([9,9,9,20]); line([15,9,15,20]); line([3,14,21,14]); break
        case "performance": line([9,17,9,5,19,3,19,15]); circle(6,18,3); circle(16,16,3); break
        case "compare": line([3,8,21,8,17,4]); line([21,16,3,16,7,20]); break
        case "devices": box(3,5,18,14); circle(8,12,2); line([14,9,18,9]); line([14,13,18,13]); line([7,22,7,19]); line([17,22,17,19]); break
        case "settings": circle(12,12,4); circle(12,12,8); for(var j=0;j<8;++j) { var a=j*Math.PI/4; line([12+8*Math.cos(a),12+8*Math.sin(a),12+10*Math.cos(a),12+10*Math.sin(a)]) } break
        case "undo": line([8,4,3,9,8,14]); line([3,9,14,9]); c.beginPath();c.arc(14,14,5,-Math.PI/2,Math.PI/2);c.stroke();line([14,19,10,19]); break
        case "redo": line([16,4,21,9,16,14]);line([21,9,10,9]);c.beginPath();c.arc(10,14,5,-Math.PI/2,-3*Math.PI/2,true);c.stroke();line([10,19,14,19]);break
        case "power": c.beginPath();c.arc(12,13,8,-Math.PI/3,4*Math.PI/3);c.stroke();line([12,2,12,12]);break
        case "envelope": line([3,19,7,5,11,11,17,12,21,19]);line([3,3,3,21,22,21]);break
        case "keyboard": box(2,5,20,14);line([6,5,6,19]);line([10,5,10,19]);line([14,5,14,19]);line([18,5,18,19]);break
        }
    }
}
