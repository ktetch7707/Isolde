import QtQuick

Canvas {
    id: root
    smooth: true
    antialiasing: true
    property url source
    property var crop: null
    property real radius: 0
    property string fit:"cover"
    property real focalX:0.5
    property real focalY:0.5
    property real zoom:1
    readonly property int status: probe.status
    onSourceChanged: { if (source.toString()) loadImage(source); requestPaint() }
    onCropChanged: requestPaint()
    onRadiusChanged: requestPaint()
    onFitChanged:requestPaint()
    onFocalXChanged:requestPaint()
    onFocalYChanged:requestPaint()
    onZoomChanged:requestPaint()
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
        let dx=0,dy=0,dw=width,dh=height
        if(fit==="contain") {const factor=Math.min(width/w,height/h);dw=w*factor;dh=h*factor;dx=(width-dw)*focalX;dy=(height-dh)*focalY}
        else {
            if (w/h > ratio) { const cut = h*ratio; x += (w-cut)*focalX; w = cut }
            else { const cut = w/ratio; y += (h-cut)*focalY; h = cut }
            const zw=w/zoom,zh=h/zoom;x+=(w-zw)*focalX;y+=(h-zh)*focalY;w=zw;h=zh
        }
        c.save(); c.beginPath(); c.roundedRect(0,0,width,height,Math.min(radius,width/2),Math.min(radius,height/2)); c.clip()
        c.drawImage(source,x,y,w,h,dx,dy,dw,dh); c.restore()
    }
}
