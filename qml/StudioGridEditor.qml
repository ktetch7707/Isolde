import QtQuick
import QtQuick.Controls

// Page-grid controls issue proposals; the shared session owns validation/history.
Item {
    id:root
    property var value:({})
    required property real sectionHeight
    property var modes:[]
    property var fields:[]
    property var metrics:({})
    property var appearance:({})
    property var selectionRect:null
    property string message:""
    property bool valid:true
    property int choice:6
    property bool collapsed:false
    readonly property real unit:appearance.typographyScale || 1
    readonly property real spacing:appearance.spacing || 20
    readonly property var focusOrder:[0,1,2,3,4].concat(value.mode==="sections"?[5]:[]).concat([9,6,7,8])
    signal proposed(var grid, real sectionHeight)
    signal focused(int index)
    signal activated(int index)
    signal canceled()

    function field(index){return fields[index-1] || ({key:"",min:0,max:0,step:1})}
    function setField(index,number){
        const spec=field(index)
        if(!Number.isFinite(number) || number<spec.min || number>spec.max)return
        const next=Object.assign({},value)
        if(spec.key==="sectionHeight")proposed(next,number)
        else{next[spec.key]=number;proposed(next,sectionHeight)}
    }
    function chooseMode(index){if(!modes[index])return;focused(0);proposed(Object.assign({},value,{mode:modes[index].id}),sectionHeight)}
    function handleCommand(command){
        if(command===inputManager.back){canceled();return}
        if(command===inputManager.menu){collapsed=!collapsed;return}
        if(collapsed){if(command===inputManager.confirm)collapsed=false;return}
        const direction=command===inputManager.navigateRight?1:command===inputManager.navigateLeft?-1:0
        if(choice===9 && (direction || command===inputManager.confirm)){proposed(Object.assign({},value,{overlap:direction?direction>0:!value.overlap}),sectionHeight);return}
        if(direction && choice===0){const index=modes.findIndex(m=>m.id===value.mode)+direction;chooseMode(index);return}
        if(direction && choice>0 && choice<6){const spec=field(choice);setField(choice,(spec.key==="sectionHeight"?sectionHeight:Number(value[spec.key]))+direction*spec.step);return}
        if(direction && choice>=6){focused(Math.max(6,Math.min(8,choice+direction)));return}
        if(command===inputManager.navigateDown || command===inputManager.navigateUp){
            const step=command===inputManager.navigateDown?1:-1
            const index=focusOrder.indexOf(choice)
            focused(focusOrder[Math.max(0,Math.min(focusOrder.length-1,index+step))]);return
        }
        if(command===inputManager.confirm){if(choice===0)chooseMode((modes.findIndex(m=>m.id===value.mode)+1)%modes.length);else if(choice>=6)activated(choice)}
    }
    Rectangle {
        id:panel;objectName:"studioGridPanel"
        width:Math.min(root.width-2*root.spacing,460*root.unit)
        height:root.collapsed?48*root.unit:Math.min(root.height-2*root.spacing,body.implicitHeight+2*root.spacing)
        x:root.selectionRect && root.selectionRect.x<root.width/2?root.width-width-root.spacing:root.spacing
        y:root.collapsed?root.spacing:Math.max(root.spacing,(root.height-height)/2)
        color:root.appearance.surfaceColor;border.width:2;border.color:root.appearance.focusColor;radius:root.spacing
        Flickable {
            id:scroll;anchors.fill:parent;anchors.margins:root.spacing;visible:!root.collapsed
            clip:true;contentHeight:body.implicitHeight;boundsBehavior:Flickable.StopAtBounds
            ScrollBar.vertical:ScrollBar {
                policy:ScrollBar.AsNeeded;width:4*root.unit
                contentItem:Rectangle {implicitWidth:4*root.unit;radius:width/2;color:root.appearance.focusColor}
            }
            function reveal(item){if(!item)return;const point=item.mapToItem(body,0,0);if(point.y<contentY)contentY=point.y;else if(point.y+item.height>contentY+height)contentY=point.y+item.height-height}
            Column {
                id:body;width:scroll.width-8*root.unit;spacing:12*root.unit
                Item {
                    width:parent.width;height:36*root.unit
                    Text {anchors.left:parent.left;anchors.verticalCenter:parent.verticalCenter;text:"Page layout";color:root.appearance.textColor;font.pixelSize:24*root.unit}
                    Text {anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;text:"Hide controls";color:root.appearance.mutedColor;font.pixelSize:14*root.unit
                        MouseArea {anchors.fill:parent;anchors.margins:-6*root.unit;onClicked:root.collapsed=true}
                    }
                }
                Row {
                    id:modeRow;width:parent.width;spacing:8*root.unit
                    Repeater {
                        model:root.modes
                        delegate:Rectangle {
                            required property int index;required property var modelData
                            objectName:"studioGridMode-"+modelData.id
                            width:(body.width-8*root.unit*(root.modes.length-1))/Math.max(1,root.modes.length);height:64*root.unit;radius:8*root.unit
                            color:root.value.mode===modelData.id?root.appearance.selectedColor:"transparent";border.width:root.choice===0 && root.value.mode===modelData.id?2:1
                            border.color:root.choice===0 && root.value.mode===modelData.id?root.appearance.focusColor:root.appearance.mutedColor
                            Text {anchors.fill:parent;anchors.margins:8*root.unit;text:modelData.name;wrapMode:Text.Wrap;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;color:root.appearance.textColor;font.pixelSize:18*root.unit}
                            Accessible.role:Accessible.RadioButton;Accessible.name:modelData.name;Accessible.checked:root.value.mode===modelData.id
                            MouseArea {anchors.fill:parent;onClicked:root.chooseMode(index)}
                        }
                    }
                }
                Item {
                    id:diagram;objectName:"studioGridDiagram";width:parent.width;height:76*root.unit
                    readonly property real sourceWidth:root.metrics.valid?2*root.metrics.margin+root.metrics.columns*root.metrics.cellW+(root.metrics.columns-1)*root.metrics.gap:1
                    readonly property real sourceHeight:root.metrics.valid?2*root.metrics.margin+root.metrics.rows*root.metrics.cellH+(root.metrics.rows-1)*root.metrics.gap:1
                    readonly property real ratio:Math.min(width/sourceWidth,height/sourceHeight)
                    Rectangle {
                        id:diagramBounds;anchors.centerIn:parent;width:diagram.sourceWidth*diagram.ratio;height:diagram.sourceHeight*diagram.ratio
                        visible:root.valid && !!root.metrics.valid;color:root.appearance.selectedColor;border.width:1;border.color:root.appearance.mutedColor
                        Repeater {
                            model:root.valid && root.metrics.valid?root.metrics.columns*root.metrics.rows:0
                            delegate:Rectangle {
                                required property int index
                                x:(root.metrics.margin+(index%root.metrics.columns)*(root.metrics.cellW+root.metrics.gap))*diagram.ratio
                                y:(root.metrics.margin+Math.floor(index/root.metrics.columns)*(root.metrics.cellH+root.metrics.gap))*diagram.ratio
                                width:root.metrics.cellW*diagram.ratio;height:root.metrics.cellH*diagram.ratio
                                color:root.appearance.surfaceColor;border.width:1;border.color:root.appearance.mutedColor
                            }
                        }
                    }
                    Text {visible:!root.valid || !root.metrics.valid;anchors.centerIn:parent;text:"Preview unavailable";color:root.appearance.mutedColor;font.pixelSize:16*root.unit}
                }
                Repeater {
                    id:dimensions;model:["Columns","Rows"]
                    delegate:Row {
                        id:dimension;required property int index;required property string modelData
                        readonly property var spec:root.field(index+1)
                        width:body.width;height:44*root.unit;spacing:8*root.unit
                        Text {width:parent.width-150*root.unit;height:parent.height;verticalAlignment:Text.AlignVCenter;text:dimension.modelData;color:root.appearance.textColor;font.pixelSize:18*root.unit}
                        Repeater {
                            model:["−",String(root.value[dimension.spec.key]),"＋"]
                            delegate:Rectangle {
                                required property int index;required property string modelData
                                objectName:"studioGrid-"+dimension.spec.key+"-"+index
                                readonly property bool available:index===1 || (index===0?root.value[dimension.spec.key]>dimension.spec.min:root.value[dimension.spec.key]<dimension.spec.max)
                                width:42*root.unit;height:44*root.unit;radius:6*root.unit;color:root.choice===dimension.index+1?root.appearance.selectedColor:"transparent"
                                border.width:root.choice===dimension.index+1?1:0;border.color:root.appearance.focusColor
                                Text {anchors.centerIn:parent;text:modelData;color:parent.available?root.appearance.textColor:root.appearance.mutedColor;font.pixelSize:18*root.unit}
                                Accessible.role:index===1?Accessible.StaticText:Accessible.Button;Accessible.name:index===1?dimension.modelData+" "+modelData:(index===0?"Decrease ":"Increase ")+dimension.modelData
                                MouseArea {anchors.fill:parent;enabled:index!==1 && parent.available;onClicked:{root.focused(dimension.index+1);root.setField(dimension.index+1,Number(root.value[dimension.spec.key])+(index===0?-1:1))}}
                            }
                        }
                    }
                }
                Repeater {
                    id:scalars;model:["Item gap","Safe margin","Section height"]
                    delegate:Column {
                        id:scalar;required property int index;required property string modelData
                        readonly property var spec:root.field(index+3)
                        readonly property real number:index===2?root.sectionHeight:Number(root.value[spec.key] || 0)
                        visible:index<2 || root.value.mode==="sections";width:body.width;spacing:2*root.unit
                        Text {text:scalar.modelData+" · "+scalar.number;color:root.appearance.textColor;font.pixelSize:18*root.unit}
                        Slider {
                            id:slider;objectName:"studioGrid-"+scalar.spec.key
                            width:parent.width;height:32*root.unit;from:scalar.spec.min;to:scalar.spec.max;stepSize:scalar.spec.step;value:scalar.number
                            Accessible.name:scalar.modelData
                            onMoved:{root.focused(scalar.index+3);root.setField(scalar.index+3,value)}
                            background:Rectangle {x:slider.leftPadding;y:slider.topPadding+slider.availableHeight/2-height/2;width:slider.availableWidth;height:4*root.unit;radius:height/2;color:root.appearance.mutedColor}
                            handle:Rectangle {x:slider.leftPadding+slider.visualPosition*(slider.availableWidth-width);y:slider.topPadding+slider.availableHeight/2-height/2;width:20*root.unit;height:width;radius:width/2;color:root.appearance.focusColor;border.width:root.choice===scalar.index+3?2:0;border.color:root.appearance.textColor}
                        }
                        Item {
                            width:parent.width;height:16*root.unit
                            Text {anchors.left:parent.left;text:scalar.spec.min;color:root.appearance.mutedColor;font.pixelSize:13*root.unit}
                            Text {anchors.right:parent.right;text:scalar.spec.max;color:root.appearance.mutedColor;font.pixelSize:13*root.unit}
                        }
                    }
                }
                CheckBox {
                    id:overlapToggle;objectName:"studioGridOverlap"
                    width:parent.width;height:44*root.unit;checkable:false
                    text:"Allow overlapping layers";checked:!!root.value.overlap
                    Accessible.name:text
                    onClicked:{root.focused(9);root.proposed(Object.assign({},root.value,{overlap:!root.value.overlap}),root.sectionHeight)}
                    background:Rectangle {color:root.choice===9?root.appearance.selectedColor:"transparent";radius:6*root.unit;border.width:root.choice===9?2:0;border.color:root.appearance.focusColor}
                    contentItem:Text {leftPadding:44*root.unit;text:overlapToggle.text;verticalAlignment:Text.AlignVCenter;color:root.appearance.textColor;font.pixelSize:18*root.unit;wrapMode:Text.Wrap}
                    indicator:Rectangle {x:6*root.unit;y:(overlapToggle.height-height)/2;width:28*root.unit;height:width;radius:5*root.unit;color:overlapToggle.checked?root.appearance.focusColor:"transparent";border.width:2;border.color:root.choice===9?root.appearance.focusColor:root.appearance.mutedColor;Text {anchors.centerIn:parent;text:overlapToggle.checked?"✓":"";color:root.appearance.surfaceColor;font.pixelSize:20*root.unit}}
                }
                Text {width:parent.width;visible:text.length>0;text:root.message;wrapMode:Text.Wrap;color:root.appearance.mutedColor;font.pixelSize:16*root.unit}
                Row {
                    id:actions;width:parent.width;spacing:8*root.unit
                    Repeater {
                        model:["Review layout","Cancel"]
                        delegate:Rectangle {
                            required property int index;required property string modelData
                            objectName:index===0?"studioGridPreview":"studioGridCancel"
                            width:(body.width-8*root.unit)/2;height:44*root.unit;radius:6*root.unit;color:root.choice===index+6?root.appearance.selectedColor:"transparent"
                            border.width:root.choice===index+6?2:0;border.color:root.appearance.focusColor
                            Text {anchors.centerIn:parent;text:modelData;color:root.appearance.textColor;font.pixelSize:18*root.unit}
                            Accessible.role:Accessible.Button;Accessible.name:modelData
                            MouseArea {anchors.fill:parent;onClicked:{root.focused(index+6);root.activated(index+6)}}
                        }
                    }
                }
                Rectangle {
                    id:exact;width:parent.width;height:36*root.unit;radius:6*root.unit;color:root.choice===8?root.appearance.selectedColor:"transparent"
                    border.width:root.choice===8?2:0;border.color:root.appearance.focusColor
                    Text {anchors.centerIn:parent;text:"Exact values…";color:root.appearance.mutedColor;font.pixelSize:16*root.unit}
                    Accessible.role:Accessible.Button;Accessible.name:"Exact layout values"
                    MouseArea {anchors.fill:parent;onClicked:{root.focused(8);root.activated(8)}}
                }
            }
        }
        Text {visible:root.collapsed;anchors.centerIn:parent;text:"Page layout · Show controls";color:root.appearance.textColor;font.pixelSize:18*root.unit}
        MouseArea {visible:root.collapsed;anchors.fill:parent;onClicked:root.collapsed=false}
    }
    onVisibleChanged:if(visible){collapsed=false;scroll.contentY=0}
    onChoiceChanged:Qt.callLater(()=>scroll.reveal(choice===0?modeRow:choice<3?dimensions.itemAt(choice-1):choice<6?scalars.itemAt(choice-3):choice===9?overlapToggle:choice===8?exact:actions))
}
