import QtQuick

Canvas {
    id: root
    smooth: true
    antialiasing: true
    property url source
    property var crop: null
    property real radius: 0
    readonly property int status: probe.status
    onSourceChanged: { if (source.toString()) loadImage(source); requestPaint() }
    onCropChanged: requestPaint()
    onRadiusChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onImageLoaded: requestPaint()
    Image { id: probe; source: root.source; visible: false; asynchronous: true; onStatusChanged: root.requestPaint() }
    onPaint: {
        const c = getContext("2d"); c.reset()
        if (!source.toString() || !isImageLoaded(source) || probe.status !== Image.Ready || width <= 0 || height <= 0) return
        let x = crop ? crop.x : 0, y = crop ? crop.y : 0
        let w = crop ? crop.width : probe.sourceSize.width, h = crop ? crop.height : probe.sourceSize.height
        const ratio = width/height
        if (w/h > ratio) { const cut = h*ratio; x += (w-cut)/2; w = cut }
        else { const cut = w/ratio; y += (h-cut)/2; h = cut }
        c.save(); c.beginPath(); c.roundedRect(0,0,width,height,Math.min(radius,width/2),Math.min(radius,height/2)); c.clip()
        c.drawImage(source,x,y,w,h,0,0,width,height); c.restore()
    }
}
