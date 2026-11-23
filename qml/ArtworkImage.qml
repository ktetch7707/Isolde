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
    property color tintColor:"transparent"
    property real contentInset:0
    property real contentScale:1
    property real glowWidth:0
    property real glowOpacity:0.65
    property real reflectionHeight:0
    property real reflectionOpacity:0
    property bool featherBackdrop:false
    property bool sampleBackdropColor:false
    property var backdropColor:null
    readonly property int status: probe.status
    onSourceChanged:clearAnimation()
    onManagedAssetChanged:clearAnimation()
    onAnimationResolverChanged:clearAnimation()
    onMotionEnabledChanged:clearAnimation()
    onReducedMotionChanged:clearAnimation()
    onDisplaySourceChanged:{backdropColor=null;if(loadedSource.toString())unloadImage(loadedSource);loadedSource=displaySource;if(displaySource.toString())loadImage(displaySource);requestPaint()}
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
    onTintColorChanged:requestPaint()
    onContentInsetChanged:requestPaint()
    onContentScaleChanged:requestPaint()
    onGlowWidthChanged:requestPaint()
    onGlowOpacityChanged:requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onReflectionHeightChanged: requestPaint()
    onReflectionOpacityChanged: requestPaint()
    onFeatherBackdropChanged: requestPaint()
    onSampleBackdropColorChanged: requestPaint()
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
        const artHeight=Math.max(1,height-reflectionHeight)
        const innerWidth=Math.max(1,width-2*contentInset),innerHeight=Math.max(1,artHeight-2*contentInset),ratio=innerWidth/innerHeight
        let dx=contentInset,dy=contentInset,dw=innerWidth,dh=innerHeight
        if(fit==="contain") {const factor=Math.min(innerWidth/w,innerHeight/h)*contentScale;dw=w*factor;dh=h*factor;dx=contentInset+(innerWidth-dw)*focalX;dy=contentInset+(innerHeight-dh)*focalY}
        else {
            if (w/h > ratio) { const cut = h*ratio; x += (w-cut)*focalX; w = cut }
            else { const cut = w/ratio; y += (h-cut)*focalY; h = cut }
            const zw=w/zoom,zh=h/zoom;x+=(w-zw)*focalX;y+=(h-zh)*focalY;w=zw;h=zh
        }
        c.save(); c.beginPath(); c.roundedRect(0,0,width,artHeight,Math.min(radius,width/2),Math.min(radius,artHeight/2)); c.clip()
        // Qt's raster Canvas rounds a source crop below one pixel to an empty
        // QImage. Transform the whole image instead, with the same output clip.
        // The transform avoids preparing an oversized intermediate bitmap.
        function draw(ox,oy){
            if(w>=1 && h>=1){c.drawImage(displaySource,x,y,w,h,dx+ox,dy+oy,dw,dh);return}
            c.save();c.beginPath();c.rect(dx+ox,dy+oy,dw,dh);c.clip()
            c.translate(dx+ox,dy+oy);c.scale(dw/w,dh/h);c.drawImage(displaySource,-x,-y);c.restore()
        }
        if(glowWidth>0){
            c.shadowBlur=glowWidth;c.shadowColor=Qt.rgba(outlineColor.r,outlineColor.g,outlineColor.b,glowOpacity);c.shadowOffsetX=0;c.shadowOffsetY=0;draw(0,0)
            c.shadowBlur=0;c.shadowColor="transparent"
        }
        if(outlineWidth>0){
            for(let ox=-1;ox<=1;ox++)for(let oy=-1;oy<=1;oy++)if(ox || oy)draw(ox*outlineWidth,oy*outlineWidth)
            c.globalCompositeOperation="source-in";c.fillStyle=outlineColor;c.fillRect(0,0,width,height);c.globalCompositeOperation="source-over"
        }
        // Recolour the foreground alpha independently of the focus outline.
        if(tintColor.a>0){
            const outline=outlineWidth>0 || glowWidth>0?c.getImageData(0,0,width,height):null
            c.clearRect(0,0,width,height);draw(0,0)
            const foreground=c.getImageData(0,0,width,height),pixels=foreground.data
            for(let i=0;i<pixels.length;i+=4){
                const alpha=pixels[i+3]/255*tintColor.a
                const behind=outline?outline.data[i+3]/255*(1-alpha):0,total=alpha+behind
                pixels[i]=total?(tintColor.r*255*alpha+(outline?outline.data[i]:0)*behind)/total:0
                pixels[i+1]=total?(tintColor.g*255*alpha+(outline?outline.data[i+1]:0)*behind)/total:0
                pixels[i+2]=total?(tintColor.b*255*alpha+(outline?outline.data[i+2]:0)*behind)/total:0
                pixels[i+3]=total*255
            }
            // Qt's Context2D requires the explicit dirty rectangle as well.
            c.putImageData(foreground,0,0,0,0,foreground.width,foreground.height)
        } else draw(0,0)
        c.restore()
        if(sampleBackdropColor){
            // Read the sample region once so a full-page canvas does not repeatedly flush to the CPU.
            const left=Math.floor(width*.2),top=Math.floor(artHeight*.3)
            const right=Math.floor(width*.8),bottom=Math.floor(artHeight*.7)
            const region=c.getImageData(left,top,right-left+1,bottom-top+1)
            const pixels=region.data
            let red=0,green=0,blue=0,weight=0
            for(const x of [.2,.35,.5,.65,.8])for(const y of [.3,.5,.7]){
                const offset=((Math.floor(artHeight*y)-top)*region.width+Math.floor(width*x)-left)*4
                const contribution=(pixels[offset+3] || 0)/255
                red+=(pixels[offset] || 0)*contribution;green+=(pixels[offset+1] || 0)*contribution;blue+=(pixels[offset+2] || 0)*contribution
                weight+=contribution
            }
            backdropColor=weight?Qt.rgba(red/(255*weight),green/(255*weight),blue/(255*weight),1):null
        }
        if(featherBackdrop){
            // Fade into the page through transparency so the underlying theme or wallpaper shows through.
            const side=c.createLinearGradient(0,0,width,0)
            side.addColorStop(0,"black");side.addColorStop(.55,"transparent");side.addColorStop(.88,"transparent");side.addColorStop(1,"black")
            const edge=c.createLinearGradient(0,0,0,height)
            edge.addColorStop(0,"black");edge.addColorStop(.17,"transparent");edge.addColorStop(.63,"transparent");edge.addColorStop(1,"black")
            c.globalCompositeOperation="destination-out"
            c.fillStyle=side;c.fillRect(0,0,width,height)
            c.fillStyle=edge;c.fillRect(0,0,width,height)
            c.globalCompositeOperation="source-over"
        }
        if(reflectionHeight>0 && reflectionOpacity>0){
            // Mirror just the painted bottom strip so the crop and corners stay identical.
            // This also works on Qt Quick's software renderer.
            const top=Math.ceil(artHeight),rows=Math.min(Math.floor(height)-top,Math.floor(artHeight))
            const columns=Math.floor(width)
            if(rows<1 || columns<1)return
            const source=c.getImageData(0,top-rows,columns,rows)
            const reflected=c.createImageData(columns,rows)
            const originalPixels=source.data, pixels=reflected.data
            for(let y=0;y<rows;++y){
                const alpha=reflectionOpacity*(1-y/rows)
                for(let x=0;x<columns;++x){
                    const from=((rows-1-y)*columns+x)*4, to=(y*columns+x)*4
                    pixels[to]=originalPixels[from]
                    pixels[to+1]=originalPixels[from+1]
                    pixels[to+2]=originalPixels[from+2]
                    pixels[to+3]=Math.round(originalPixels[from+3]*alpha)
                }
            }
            c.putImageData(reflected,0,top,0,0,columns,rows)
        }
    }
}
