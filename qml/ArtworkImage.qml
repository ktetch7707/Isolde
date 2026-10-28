import QtQuick
import QtQuick.Window

Canvas {
    id: root
    smooth: true
    antialiasing: true
    property url source
    property var animationResolver:null
    property string managedAsset:""
    property bool motionEnabled:false
    property bool reducedMotion:false
    property var animation:({})
    property int animationFrame:0
    property int animationLoop:0
    property bool animationFinished:false
    property bool animationAttempted:false
    property string animationError:""
    readonly property string animationOwner:"art-"+Math.random().toString(36).slice(2)
    property var leasedResolver:null
    property url loadedSource
    readonly property url displaySource:(animation.frames || [])[animationFrame] || source
    property var crop: null
    property real radius: 0
    property string fit:"cover"
    property real focalX:0.5
    property real focalY:0.5
    property real zoom:1
    property real outlineWidth:0
    property color outlineColor:"transparent"
    readonly property int status: probe.status
    onSourceChanged:clearAnimation()
    onManagedAssetChanged:clearAnimation()
    onAnimationResolverChanged:clearAnimation()
    onMotionEnabledChanged:clearAnimation()
    onReducedMotionChanged:clearAnimation()
    onDisplaySourceChanged:{if(loadedSource.toString())unloadImage(loadedSource);loadedSource=displaySource;if(displaySource.toString())loadImage(displaySource);requestPaint()}
    Component.onDestruction:clearAnimation()
    function clearAnimation(){animation=({});animationFrame=0;animationLoop=0;animationFinished=false;animationAttempted=false;animationError="";if(leasedResolver)leasedResolver.releaseArtworkAnimation(animationOwner);leasedResolver=null}
    function onScreen(){
        const window=root.Window.window;if(!window || !window.active || !visible || opacity===0)return false
        const a=mapToItem(null,0,0),b=mapToItem(null,width,height)
        let left=Math.max(0,a.x),top=Math.max(0,a.y),right=Math.min(window.width,b.x),bottom=Math.min(window.height,b.y),ancestor=parent
        while(ancestor){if(!ancestor.visible || ancestor.opacity===0)return false;if(ancestor.clip){const c=ancestor.mapToItem(null,0,0),d=ancestor.mapToItem(null,ancestor.width,ancestor.height);left=Math.max(left,c.x);top=Math.max(top,c.y);right=Math.min(right,d.x);bottom=Math.min(bottom,d.y)}ancestor=ancestor.parent}
        return right>left && bottom>top
    }
    function advanceAnimation(){
        if(!motionEnabled || reducedMotion){if(leasedResolver)clearAnimation();return}
        if(!onScreen()){if(leasedResolver || animationAttempted)clearAnimation();return}
        if(!animationAttempted){
            animationAttempted=true
            if(!animationResolver || !animationResolver.artworkAnimation)return
            const result=animationResolver.artworkAnimation(managedAsset,animationOwner)
            if(result.error){animationError=result.error;return}
            leasedResolver=animationResolver;animation=result;animationFrame=0;return
        }
        if(animationFinished)return
        if((animation.frames || []).length>1){
            if(animationFrame+1===animation.frames.length){if(animation.loops>=0 && animationLoop>=animation.loops){animationFinished=true;return}animationLoop++}
            animationFrame=(animationFrame+1)%animation.frames.length
        }
    }
    Timer {interval:(root.animation.delays || [])[root.animationFrame] || 125;repeat:true;running:root.visible && root.motionEnabled && !root.reducedMotion && root.managedAsset.endsWith(".gif");onTriggered:root.advanceAnimation()}
    onCropChanged: requestPaint()
    onRadiusChanged: requestPaint()
    onFitChanged:requestPaint()
    onFocalXChanged:requestPaint()
    onFocalYChanged:requestPaint()
    onZoomChanged:requestPaint()
    onOutlineWidthChanged:requestPaint()
    onOutlineColorChanged:requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onImageLoaded: requestPaint()
    onVisibleChanged:if(visible)requestPaint();else clearAnimation()
    Image { id: probe; source: root.source; visible: false; asynchronous: true; onStatusChanged: root.requestPaint() }
    onPaint: {
        const c = getContext("2d")
        if (!displaySource.toString()){c.reset();return}
        if (!isImageLoaded(displaySource) || probe.status !== Image.Ready || width <= 0 || height <= 0) return
        c.reset()
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
        if(outlineWidth>0){
            for(let ox=-1;ox<=1;ox++)for(let oy=-1;oy<=1;oy++)if(ox || oy)c.drawImage(displaySource,x,y,w,h,dx+ox*outlineWidth,dy+oy*outlineWidth,dw,dh)
            c.globalCompositeOperation="source-in";c.fillStyle=outlineColor;c.fillRect(0,0,width,height);c.globalCompositeOperation="source-over"
        }
        c.drawImage(displaySource,x,y,w,h,dx,dy,dw,dh); c.restore()
    }
}
