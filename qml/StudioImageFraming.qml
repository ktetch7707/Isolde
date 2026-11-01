import QtQuick
import QtQuick.Controls

Item {
    id:root
    required property var appearance
    property var componentItem:null
    property var componentAppearance:appearance
    property var targetGeometry:({})
    property var artworkResolver:null
    property url source
    property var original:({})
    property var draft:({})
    property int control:0
    property string message:""
    readonly property real unit:Math.min(width/1280,height/720)
    readonly property var labels:["Focal point · D-pad or drag target","Image framing","Crop width: "+((draft.imageCrop || imageSize).width || 0)+" px","Crop height: "+((draft.imageCrop || imageSize).height || 0)+" px","Zoom: "+Number(draft.artZoom || 1).toFixed(1),"Lock proportions","Use full artwork","Done","Cancel"]
    readonly property bool sourceReady:probe.status===Image.Ready
    readonly property var imageSize:({width:probe.sourceSize.width,height:probe.sourceSize.height})
    signal previewed(var value)
    signal accepted()
    signal canceled()
    function begin(){draft=Object.assign({artFit:"cover",artFocalX:.5,artFocalY:.5,artZoom:1,artAspectLocked:true},original);control=0;message=""}
    function change(value){if(!sourceReady){message=probe.status===Image.Loading?"Image is loading. Try again in a moment.":"Choose a readable image in Content → Artwork. Back keeps the image unchanged.";return}draft=Object.assign({},draft,value);previewed(value)}
    function focal(x,y){if(x<0 || x>1 || y<0 || y>1){message="The focal point must stay inside the image.";return}message="";change({artFocalX:Math.round(x*100)/100,artFocalY:Math.round(y*100)/100})}
    function resizeCrop(axis,direction){
        const w=imageSize.width,h=imageSize.height;if(!w || !h){message="Choose a readable image first.";return}
        const old=draft.imageCrop || {x:0,y:0,width:w,height:h},step=Math.max(1,Math.round((axis==="width"?w:h)/20)),next=Object.assign({},old)
        next[axis]+=direction*step
        if(draft.artAspectLocked){if(axis==="width")next.height=Math.round(next.width*old.height/old.width);else next.width=Math.round(next.height*old.width/old.height)}
        next.x=Math.round((w-next.width)*(draft.artFocalX===undefined?.5:draft.artFocalX));next.y=Math.round((h-next.height)*(draft.artFocalY===undefined?.5:draft.artFocalY))
        if(next.width<1 || next.height<1 || next.x<0 || next.y<0 || next.x+next.width>w || next.y+next.height>h){message="Crop exceeds the image. Reduce its size or use full artwork.";return}
        message="";change({imageCrop:next})
    }
    function adjust(direction){
        if(control===1)change({artFit:direction>0?"contain":"cover"})
        else if(control===2 || control===3)resizeCrop(control===2?"width":"height",direction)
        else if(control===4){const z=Math.round(((draft.artZoom || 1)+direction*.1)*10)/10;if(z>=1 && z<=8)change({artZoom:z})}
        else if(control===5)change({artAspectLocked:!draft.artAspectLocked})
    }
    function activate(){if(control===0){control=1;return}if(control<6){adjust(1);return}if(control===6){change({imageCrop:null,artFocalX:.5,artFocalY:.5,artZoom:1});return}if(control===7)accepted();else canceled()}
    function handleCommand(command){
        if(command===inputManager.back){canceled();return}
        if(command===inputManager.confirm){activate();return}
        if(control===0){
            let x=draft.artFocalX===undefined?.5:draft.artFocalX,y=draft.artFocalY===undefined?.5:draft.artFocalY
            if(command===inputManager.navigateLeft)x-=.05;else if(command===inputManager.navigateRight)x+=.05;else if(command===inputManager.navigateUp)y-=.05;else if(command===inputManager.navigateDown)y+=.05
            focal(x,y);return
        }
        if(command===inputManager.navigateLeft || command===inputManager.navigateRight)adjust(command===inputManager.navigateRight?1:-1)
        else if(command===inputManager.navigateUp)control=Math.max(0,control-1)
        else if(command===inputManager.navigateDown)control=Math.min(labels.length-1,control+1)
    }
    Component.onCompleted:begin()
    Rectangle {anchors.fill:parent;color:root.appearance.surfaceColor;opacity:.96}
    Image {id:probe;source:root.source;visible:false}
    Column {
        x:32*root.unit;y:24*root.unit;width:parent.width-64*root.unit;spacing:12*root.unit
        Text {text:"Frame image · live draft";font.pixelSize:28*root.unit;font.bold:true;color:root.appearance.textColor}
        Row {
            spacing:24*root.unit
            Text {width:root.width*.43;text:"Source · crop and focal point";color:root.appearance.mutedColor;font.pixelSize:17*root.unit}
            Text {width:root.width*.43;text:"Live result";color:root.appearance.mutedColor;font.pixelSize:17*root.unit}
        }
        Row {
            spacing:24*root.unit
            Item {
                id:originalSurface;width:root.width*.43;height:root.height*.40
                Image {id:full;anchors.fill:parent;source:root.source;fillMode:Image.PreserveAspectFit}
                readonly property real renderedWidth:Math.min(width,height*(root.imageSize.width/Math.max(1,root.imageSize.height)))
                readonly property real renderedHeight:Math.min(height,width*(root.imageSize.height/Math.max(1,root.imageSize.width)))
                Rectangle {
                    x:(parent.width-parent.renderedWidth)/2+(root.draft.imageCrop || {}).x*parent.renderedWidth/Math.max(1,root.imageSize.width) || (parent.width-parent.renderedWidth)/2
                    y:(parent.height-parent.renderedHeight)/2+(root.draft.imageCrop || {}).y*parent.renderedHeight/Math.max(1,root.imageSize.height) || (parent.height-parent.renderedHeight)/2
                    width:(root.draft.imageCrop || root.imageSize).width*parent.renderedWidth/Math.max(1,root.imageSize.width)
                    height:(root.draft.imageCrop || root.imageSize).height*parent.renderedHeight/Math.max(1,root.imageSize.height)
                    color:"transparent";border.width:2;border.color:root.appearance.textColor
                }
                Rectangle {width:24*root.unit;height:width;radius:width/2;color:"transparent";border.width:3;border.color:root.appearance.focusColor;x:(parent.width-parent.renderedWidth)/2+parent.renderedWidth*root.draft.artFocalX-width/2;y:(parent.height-parent.renderedHeight)/2+parent.renderedHeight*root.draft.artFocalY-height/2}
                MouseArea {objectName:"studioFocalTarget";anchors.fill:parent;onPressed:event=>move(event);onPositionChanged:event=>{if(pressed)move(event)};function move(event){root.control=0;root.focal((event.x-(parent.width-parent.renderedWidth)/2)/parent.renderedWidth,(event.y-(parent.height-parent.renderedHeight)/2)/parent.renderedHeight)}}
            }
            Item {
                id:resultSurface;width:originalSurface.width;height:originalSurface.height;clip:true
                readonly property real targetWidth:root.targetGeometry.width || root.imageSize.width || width
                readonly property real targetHeight:root.targetGeometry.height || root.imageSize.height || height
                readonly property real factor:Math.min(width/targetWidth,height/targetHeight)
                Loader {
                    id:renderedResult;anchors.centerIn:parent;width:parent.targetWidth*parent.factor;height:parent.targetHeight*parent.factor
                    sourceComponent:root.componentItem?componentResult:plainResult
                }
                Component {id:plainResult;ArtworkImage {objectName:"studioFramingPreview";source:root.source;crop:root.draft.imageCrop || null;fit:root.draft.artFit;focalX:root.draft.artFocalX;focalY:root.draft.artFocalY;zoom:root.draft.artZoom;radius:root.original.radius || 0}}
                Component {id:componentResult;Item {
                    HomeCard {
                        objectName:"studioFramingCard";width:resultSurface.targetWidth;height:resultSurface.targetHeight;scale:resultSurface.factor;transformOrigin:Item.TopLeft
                        item:Object.assign({},root.componentItem,{logoSource:root.source,presentation:root.draft});appearance:root.componentAppearance
                        unit:root.targetGeometry.unit || 1;artworkResolver:root.artworkResolver;artworkMotionAllowed:false;enabled:false
                        opacity:root.componentItem.opacity===undefined?1:root.componentItem.opacity
                    }
                }}
            }
        }
        Text {width:parent.width;text:root.message || "Confirm leaves the focal target for Fit, crop, zoom and Done. Back restores the entire framing draft.";wrapMode:Text.Wrap;font.pixelSize:17*root.unit;color:root.appearance.textColor}
        Grid {
            columns:3;spacing:8*root.unit
            Repeater {model:root.labels.length;delegate:Rectangle {
                id:framingControl
                required property int index
                readonly property string caption:root.labels[index]
                width:(root.width-80*root.unit)/3;height:43*root.unit;radius:8*root.unit;color:root.control===index?root.appearance.selectedColor:"transparent";border.width:root.control===index?2:1;border.color:root.control===index?root.appearance.focusColor:root.appearance.mutedColor
                Text {visible:parent.index!==1 && parent.index!==5;anchors.fill:parent;anchors.margins:8*root.unit;text:parent.caption;elide:Text.ElideRight;font.pixelSize:16*root.unit;color:root.appearance.textColor}
                Row {
                    visible:parent.index===1;anchors.fill:parent;anchors.margins:3*root.unit;spacing:4*root.unit
                    Repeater {model:["Fill","Fit"];delegate:Rectangle {
                        required property string modelData;required property int index
                        objectName:framingControl.index===1?(index===0?"studioFramingFill":"studioFramingFit"):""
                        width:(parent.width-4*root.unit)/2;height:parent.height;radius:6*root.unit
                        color:root.draft.artFit===(index===0?"cover":"contain")?root.appearance.selectedColor:"transparent"
                        border.width:root.draft.artFit===(index===0?"cover":"contain")?2:1;border.color:root.appearance.focusColor
                        Text {anchors.centerIn:parent;text:parent.modelData;color:root.appearance.textColor;font.pixelSize:17*root.unit}
                        MouseArea {anchors.fill:parent;onClicked:{root.control=1;root.change({artFit:parent.index===0?"cover":"contain"})}}
                    }}
                }
                Switch {
                    objectName:framingControl.index===5?"studioFramingAspectLock":"";visible:parent.index===5;anchors.fill:parent
                    checked:!!root.draft.artAspectLocked;text:"Lock proportions"
                    onToggled:{root.control=5;root.change({artAspectLocked:checked})}
                    contentItem:Text {text:parent.text;color:root.appearance.textColor;font.pixelSize:17*root.unit;verticalAlignment:Text.AlignVCenter;leftPadding:parent.indicator.width+10*root.unit}
                    indicator:Rectangle {implicitWidth:52*root.unit;implicitHeight:26*root.unit;x:8*root.unit;y:(parent.height-height)/2;radius:height/2;color:parent.checked?root.appearance.focusColor:root.appearance.surfaceColor;border.width:1;border.color:root.appearance.textColor
                        Rectangle {width:20*root.unit;height:width;radius:width/2;x:parent.parent.checked?parent.width-width-3*root.unit:3*root.unit;y:3*root.unit;color:root.appearance.textColor}
                    }
                }
                MouseArea {anchors.fill:parent;enabled:parent.index!==1 && parent.index!==5;onClicked:{root.control=parent.index;root.activate()}}
            }}
        }
    }
}
