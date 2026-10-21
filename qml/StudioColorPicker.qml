import QtQuick
import QtQuick.Controls

Item {
    id: root
    objectName: "studioColorPicker"
    required property var appearance
    property var settings: ({})
    property var themeColors: []
    property var recentColors: []
    property var selectionRect: null
    property string roleName: "Color"
    property color original: "white"
    property color sample: original
    property real hue: 0
    property real saturation: 0
    property real brightness: 1
    property int category: 1
    property int control: 0
    property int swatch: 0
    property bool adjustingWheel: false
    property bool resetChoice: false
    property string error: ""
    property string contrastText: ""
    property bool previewQueued: false
    property string pendingValue: ""
    property bool pendingReset: false
    readonly property var categories: ["Theme", "Presets", "Recent", "Custom"]
    readonly property var swatches: category===0?themeColors:category===1?(settings.presets || []):recentColors
    readonly property real unit: Math.min(width/1280,height/720)
    readonly property real panelWidth: Math.min(width-32*unit,540*unit)
    signal preview(string value, bool reset)
    signal accepted(string value, bool reset)
    signal canceled()
    signal exactRequested(string value)

    function queuePreview(value,reset) {
        pendingValue=value;pendingReset=reset
        if(!previewQueued){previewQueued=true;Qt.callLater(flushPreview)}
    }
    function flushPreview() {
        if(!previewQueued)return
        previewQueued=false;preview(pendingValue,pendingReset)
    }
    function cancel() {previewQueued=false;canceled()}

    function hex(value) {
        function channel(n) {return Math.round(n*255).toString(16).padStart(2,"0")}
        return "#"+channel(value.r)+channel(value.g)+channel(value.b)
    }
    function begin(value) {
        original=value;sample=value
        hue=Math.max(0,sample.hsvHue);saturation=sample.hsvSaturation;brightness=sample.hsvValue
        category=1;control=0;swatch=0;adjustingWheel=false;resetChoice=false;error="";previewQueued=false
    }
    function choose(value) {
        sample=value;hue=Math.max(0,sample.hsvHue);saturation=sample.hsvSaturation;brightness=sample.hsvValue
        resetChoice=false;error="";queuePreview(hex(sample),false)
    }
    function updateHsv(h,s,v) {
        hue=(h%1+1)%1;saturation=Math.max(0,Math.min(1,s));brightness=Math.max(0,Math.min(1,v))
        sample=Qt.hsva(hue,saturation,brightness,1);resetChoice=false;error="";queuePreview(hex(sample),false)
    }
    function wheelAt(x,y) {
        const dx=x-wheel.width/2,dy=y-wheel.height/2,r=wheel.width/2-4
        // Screen coordinates are integer pixels. Stable one-degree/one-percent
        // steps prevent a half-pixel offset from changing an axis color and
        // let pointer and remote reach identical authored values.
        updateHsv(Math.round(Math.atan2(dy,dx)*180/Math.PI)/360,Math.round(Math.sqrt(dx*dx+dy*dy)/r*100)/100,brightness)
    }
    function handleCommand(command) {
        if(command===inputManager.back){if(adjustingWheel){adjustingWheel=false;return}cancel();return}
        const dx=command===inputManager.navigateRight?1:command===inputManager.navigateLeft?-1:0
        const dy=command===inputManager.navigateDown?1:command===inputManager.navigateUp?-1:0
        if(adjustingWheel){if(command===inputManager.confirm){adjustingWheel=false;return}if(dx || dy)updateHsv(hue+dx/72,saturation-dy*.05,brightness);return}
        if(control===0 && dx){category=Math.max(0,Math.min(3,category+dx));swatch=0;return}
        if(control===1 && category<3){
            if(dx){swatch=Math.max(0,Math.min(swatches.length-1,swatch+dx));return}
            if(dy && ((dy>0 && swatch+4<swatches.length) || (dy<0 && swatch>=4))){swatch+=4*dy;return}
        }
        if(control===2 && dx){updateHsv(hue,saturation,brightness+dx*.05);return}
        if(dy){control=Math.max(0,Math.min(6,control+dy));return}
        if(command!==inputManager.confirm)return
        if(control===1){if(category===3)adjustingWheel=true;else if(swatches[swatch])choose(swatches[swatch].value)}
        else if(control===3){resetChoice=true;queuePreview("",true)}
        else if(control===4){flushPreview();accepted(hex(sample),resetChoice)}
        else if(control===5)cancel()
        else if(control===6)exactRequested(hex(sample))
    }

    Rectangle {
        id: panel
        objectName: "studioColorPanel"
        width:root.panelWidth;height:body.implicitHeight+32*root.unit
        x:root.selectionRect && root.selectionRect.x-width-24*root.unit>=0?root.selectionRect.x-width-16*root.unit:parent.width-width-24*root.unit
        y:Math.max(16*root.unit,(parent.height-height)/2)
        radius:16*root.unit;color:root.appearance.surfaceColor;border.width:2;border.color:root.appearance.focusColor
        Column {
            id:body;x:16*root.unit;y:16*root.unit;width:parent.width-32*root.unit;spacing:10*root.unit
            Text {text:root.roleName;color:root.appearance.textColor;font.pixelSize:24*root.unit;font.bold:true}
            Row {
                width:parent.width;spacing:12*root.unit
                Repeater {
                    model:[{name:"Before",value:root.original},{name:root.resetChoice?"Theme":"After",value:root.sample}]
                    delegate:Rectangle {
                        required property var modelData
                        width:(body.width-12*root.unit)/2;height:52*root.unit;radius:8*root.unit;color:modelData.value
                        border.width:1;border.color:root.appearance.mutedColor
                        Text {anchors.centerIn:parent;text:modelData.name;color:parent.color.r*.299+parent.color.g*.587+parent.color.b*.114>.55?"#111111":"#ffffff";font.pixelSize:17*root.unit}
                    }
                }
            }
            Row {
                spacing:6*root.unit
                Repeater {
                    model:root.categories
                    delegate:Button {
                        required property string modelData;required property int index
                        objectName:"studioColorCategory-"+index
                        width:(body.width-18*root.unit)/4;height:38*root.unit;text:modelData
                        onClicked:{root.category=index;root.control=0;root.swatch=0}
                        background:Rectangle {radius:7*root.unit;color:root.category===index?root.appearance.selectedColor:root.appearance.surfaceColor;border.width:root.control===0 && root.category===index?2:1;border.color:root.appearance.focusColor}
                        contentItem:Text {text:parent.text;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;color:root.appearance.textColor;font.pixelSize:16*root.unit}
                    }
                }
            }
            Item {
                width:parent.width;height:200*root.unit
                Grid {
                    visible:root.category<3;width:parent.width;columns:4;spacing:8*root.unit
                    Repeater {
                        model:root.swatches
                        delegate:Button {
                            required property var modelData;required property int index
                            objectName:"studioColorSwatch-"+index
                            width:(body.width-24*root.unit)/4;height:88*root.unit
                            onClicked:{root.control=1;root.swatch=index;root.choose(modelData.value)}
                            background:Rectangle {radius:8*root.unit;color:root.appearance.surfaceColor;border.width:root.control===1 && root.swatch===index?3:1;border.color:root.appearance.focusColor}
                            contentItem:Column {
                                spacing:4*root.unit
                                Rectangle {width:parent.width;height:42*root.unit;radius:5*root.unit;color:modelData.value}
                                Text {width:parent.width;text:modelData.name;elide:Text.ElideRight;horizontalAlignment:Text.AlignHCenter;color:root.appearance.textColor;font.pixelSize:14*root.unit}
                            }
                        }
                    }
                }
                Text {visible:root.category===2 && !root.swatches.length;anchors.centerIn:parent;text:"Your accepted colors appear here";color:root.appearance.mutedColor;font.pixelSize:16*root.unit}
                Canvas {
                    id:wheel;objectName:"studioColorWheel";visible:root.category===3
                    Accessible.role:Accessible.Slider;Accessible.name:"Hue and saturation"
                    anchors.centerIn:parent;width:196*root.unit;height:width
                    onPaint:{
                        const ctx=getContext("2d"),cx=width/2,cy=height/2,r=cx-4
                        ctx.clearRect(0,0,width,height)
                        for(let a=0;a<360;++a){ctx.beginPath();ctx.moveTo(cx,cy);ctx.arc(cx,cy,r,a*Math.PI/180,(a+1.5)*Math.PI/180);ctx.closePath();ctx.fillStyle="hsl("+a+",100%,50%)";ctx.fill()}
                        const wash=ctx.createRadialGradient(cx,cy,0,cx,cy,r);wash.addColorStop(0,"white");wash.addColorStop(1,"rgba(255,255,255,0)");ctx.fillStyle=wash;ctx.beginPath();ctx.arc(cx,cy,r,0,Math.PI*2);ctx.fill()
                    }
                    Rectangle {
                        x:wheel.width/2+Math.cos(root.hue*2*Math.PI)*root.saturation*(wheel.width/2-4)-width/2
                        y:wheel.height/2+Math.sin(root.hue*2*Math.PI)*root.saturation*(wheel.height/2-4)-height/2
                        width:14*root.unit;height:width;radius:width/2;color:"transparent";border.width:3;border.color:"#111111"
                        Rectangle {anchors.fill:parent;anchors.margins:3;radius:width/2;color:root.sample;border.width:1;border.color:"white"}
                    }
                    Rectangle {anchors.fill:parent;radius:width/2;color:"transparent";border.width:root.control===1?(root.adjustingWheel?4:2):0;border.color:root.appearance.focusColor}
                    MouseArea {anchors.fill:parent;onPressed:mouse=>{root.control=1;root.wheelAt(mouse.x,mouse.y)};onPositionChanged:mouse=>{if(pressed)root.wheelAt(mouse.x,mouse.y)}}
                }
            }
            Text {text:"Brightness · "+Math.round(root.brightness*100)+"%";color:root.appearance.textColor;font.pixelSize:17*root.unit}
            Slider {
                id:valueSlider;objectName:"studioColorBrightness";width:parent.width;height:30*root.unit
                Accessible.name:"Brightness"
                from:0;to:1;stepSize:.01;value:root.brightness
                onMoved:{root.control=2;root.updateHsv(root.hue,root.saturation,value)}
                background:Rectangle {x:valueSlider.leftPadding;y:(valueSlider.height-height)/2;width:valueSlider.availableWidth;height:6*root.unit;radius:height/2;gradient:Gradient {orientation:Gradient.Horizontal;GradientStop {position:0;color:"black"}GradientStop {position:1;color:Qt.hsva(root.hue,root.saturation,1,1)}}}
                handle:Rectangle {x:valueSlider.leftPadding+valueSlider.visualPosition*(valueSlider.availableWidth-width);y:(valueSlider.height-height)/2;width:18*root.unit;height:width;radius:width/2;color:root.appearance.focusColor;border.width:root.control===2?3:1;border.color:root.appearance.textColor}
            }
            Text {text:root.error || root.contrastText;width:parent.width;elide:Text.ElideRight;color:root.appearance.mutedColor;font.pixelSize:14*root.unit}
            Row {
                spacing:8*root.unit
                Repeater {
                    model:["Reset to theme","Use color","Cancel"]
                    delegate:Button {
                        required property string modelData;required property int index
                        objectName:"studioColorAction-"+index;width:(body.width-16*root.unit)/3;height:42*root.unit;text:modelData
                        onClicked:{root.control=index+3;root.handleCommand(inputManager.confirm)}
                        background:Rectangle {radius:7*root.unit;color:root.appearance.selectedColor;border.width:root.control===index+3?2:0;border.color:root.appearance.focusColor}
                        contentItem:Text {text:parent.text;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;color:root.appearance.textColor;font.pixelSize:15*root.unit}
                    }
                }
            }
            Button {
                text:"Exact color…";width:parent.width;height:32*root.unit
                onClicked:{root.control=6;root.exactRequested(root.hex(root.sample))}
                background:Rectangle {color:"transparent";radius:5*root.unit;border.width:root.control===6?2:0;border.color:root.appearance.focusColor}
                contentItem:Text {text:parent.text;color:root.appearance.textColor;font.pixelSize:15*root.unit;verticalAlignment:Text.AlignVCenter}
            }
        }
    }
}
