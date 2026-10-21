import QtQuick

Canvas {
    id: root
    smooth: true
    antialiasing: true
    property string name: "display"
    property color ink: "#EEE8EF"
    onNameChanged: requestPaint()
    onInkChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const c = getContext("2d"); c.reset(); c.scale(width / 32, height / 32)
        c.strokeStyle = ink; c.lineWidth = 2; c.lineJoin = "round"; c.lineCap = "round"
        c.beginPath()
        if (name === "arrow-right") {
            c.moveTo(4,16);c.lineTo(27,16);c.moveTo(19,8);c.lineTo(27,16);c.lineTo(19,24)
        } else if(name === "image") {
            c.roundedRect(3,3,26,26,3,3);c.moveTo(8,24);c.lineTo(14,15);c.lineTo(19,21);c.lineTo(23,16);c.lineTo(28,24);c.moveTo(13,10);c.arc(10,10,3,0,Math.PI*2)
        } else if(name === "carousel") {
            c.roundedRect(10,6,12,20,2,2);c.moveTo(5,8);c.lineTo(2,8);c.lineTo(2,24);c.lineTo(5,24);c.moveTo(27,8);c.lineTo(30,8);c.lineTo(30,24);c.lineTo(27,24)
        } else if (name === "gamepad") {
            c.moveTo(9,8); c.lineTo(23,8); c.quadraticCurveTo(27,8,28,13); c.lineTo(30,24)
            c.quadraticCurveTo(30,29,26,26); c.lineTo(21,21); c.lineTo(11,21); c.lineTo(6,26)
            c.quadraticCurveTo(2,29,2,24); c.lineTo(4,13); c.quadraticCurveTo(5,8,9,8)
            c.moveTo(8,14); c.lineTo(14,14); c.moveTo(11,11); c.lineTo(11,17)
            c.moveTo(22,12); c.lineTo(22,13); c.moveTo(25,16); c.lineTo(25,17)
        } else if (name === "hdmi") {
            c.moveTo(3,12); c.lineTo(29,12); c.lineTo(29,18); c.lineTo(25,22); c.lineTo(7,22); c.lineTo(3,18); c.closePath()
            c.moveTo(8,16); c.lineTo(24,16)
        } else if (name === "sparkle") {
            const g = c.createLinearGradient(0,0,32,32); g.addColorStop(0,"#B89AE0"); g.addColorStop(0.5,"#F4B6D5"); g.addColorStop(1,"#FFDDA3")
            c.fillStyle = g; c.moveTo(16,0); c.quadraticCurveTo(17,14,32,16); c.quadraticCurveTo(18,17,16,32); c.quadraticCurveTo(14,18,0,16); c.quadraticCurveTo(14,14,16,0); c.fill(); return
        } else if (name === "music") {
            c.moveTo(12,23); c.lineTo(12,7); c.lineTo(26,4); c.lineTo(26,21); c.moveTo(12,11); c.lineTo(26,8)
            c.moveTo(12,23); c.ellipse(4,22,8,5); c.moveTo(26,21); c.ellipse(18,20,8,5)
        } else if (name === "film") {
            c.arc(16,16,13,0,Math.PI*2); c.moveTo(16,7); c.ellipse(13,5,6,6); c.moveTo(25,13); c.ellipse(21,10,6,6)
            c.moveTo(23,24); c.ellipse(19,21,6,6); c.moveTo(10,25); c.ellipse(5,20,6,6); c.moveTo(7,15); c.ellipse(3,10,6,6)
        } else if (name === "grid") {
            c.fillStyle = ink
            for (let y=6;y<=26;y+=10) for (let x=6;x<=26;x+=10) { c.beginPath(); c.arc(x,y,2.5,0,Math.PI*2); c.fill() }
            return
        } else if (name === "search") {
            c.arc(13,13,9,0,Math.PI*2); c.moveTo(20,20); c.lineTo(29,29)
        } else if (name === "settings") {
            for (let i=0;i<=24;i++) { const a=i*Math.PI/12, r=i%3===0 ? 14 : 11; const x=16+Math.cos(a)*r,y=16+Math.sin(a)*r; if(i===0)c.moveTo(x,y);else c.lineTo(x,y) }
            c.closePath(); c.moveTo(21,16); c.arc(16,16,5,0,Math.PI*2)
        } else if (name === "heart") {
            c.fillStyle = ink; c.moveTo(16,28); c.bezierCurveTo(-7,14,3,-3,16,8); c.bezierCurveTo(29,-3,39,14,16,28); c.fill(); return
        } else if (name === "clock") {
            c.arc(16,16,13,0,Math.PI*2); c.moveTo(16,7); c.lineTo(16,16); c.lineTo(22,19)
        } else {
            c.roundedRect(3,8,26,18,3,3)
            if (name === "tv") { c.moveTo(11,2); c.lineTo(16,7); c.lineTo(21,2) }
            else { c.moveTo(16,26); c.lineTo(16,30); c.moveTo(11,30); c.lineTo(21,30) }
        }
        c.stroke()
    }
}
