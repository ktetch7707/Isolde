import QtQuick
import QtQuick.Controls

// A view over a validated session proposal. No persistence or geometry solver.
Item {
    id:root
    property var value:({})
    property var appearance:({})
    property var selectionRect:null
    property string message:""
    property bool valid:true
    property int choice:0
    property bool collapsed:false
    property var gridDimensions:({columns:1,rows:1})
    readonly property real unit:appearance.typographyScale || 1
    readonly property real gap:appearance.spacing || 20
    readonly property var modes:["grid","row","column"]
    readonly property var modeNames:["Grid","Row","Column"]
    readonly property int doneIndex:value.mode==="row"?7:5
    signal proposed(var value)
    signal accepted()
    signal canceled()

    function adjust(index,direction) {
        const next=Object.assign({},value)
        if(index===0){const target=modes.indexOf(value.mode)+direction;if(target<0 || target>=modes.length)return;setMode(modes[target]);return}
        else if(index===5){const target=Number(value.visibleColumns || value.columns)+direction;if(target<1 || target>value.columns)return;next.visibleColumns=target}
        else if(index===6){next.wrap=!value.wrap}
        else{const key=["","columns","rows","gap","padding"][index],minimum=index<3?1:0,maximum=index===1?16:index===2?10:100;const target=Number(next[key])+direction;if(target<minimum || target>maximum)return;next[key]=target}
        if(index===1)gridDimensions=Object.assign({},gridDimensions,{columns:next.columns})
        if(index===2)gridDimensions=Object.assign({},gridDimensions,{rows:next.rows})
        proposed(next)
    }
    function setMode(mode) {
        const next=Object.assign({},value,{mode:mode,columns:mode==="column"?1:gridDimensions.columns,rows:mode==="row"?1:gridDimensions.rows})
        if(mode!=="row")delete next.visibleColumns
        proposed(next)
    }
    function setValue(key,number){const next=Object.assign({},value);next[key]=number;proposed(next)}
    function handleCommand(command) {
        if(command===inputManager.back){canceled();return}
        if(command===inputManager.menu){collapsed=!collapsed;return}
        if(collapsed){if(command===inputManager.confirm)collapsed=false;return}
        if(command===inputManager.navigateUp)choice=Math.max(0,choice-1)
        else if(command===inputManager.navigateDown)choice=Math.min(doneIndex+1,choice+1)
        else if(choice<doneIndex && (command===inputManager.navigateLeft || command===inputManager.navigateRight))adjust(choice,command===inputManager.navigateRight?1:-1)
        else if(command===inputManager.confirm){if(choice===doneIndex)accepted();else if(choice===doneIndex+1)canceled();else if(choice===6)adjust(choice,1)}
        if((choice===1 && value.mode==="column") || (choice===2 && value.mode==="row"))choice+=command===inputManager.navigateUp?-1:1
    }
    Rectangle {
        id:panel;objectName:"studioChildLayoutPanel"
        width:Math.min(root.width-2*root.gap,440*root.unit)
        height:root.collapsed?48*root.unit:Math.min(root.height-2*root.gap,body.implicitHeight+2*root.gap)
        x:root.selectionRect && root.selectionRect.x<root.width/2?root.width-width-root.gap:root.gap
        y:root.collapsed?root.gap:Math.max(root.gap,(root.height-height)/2)
        color:root.appearance.surfaceColor;border.color:root.appearance.focusColor;border.width:2;radius:root.gap
        Flickable {
            id:scroll;anchors.fill:parent;anchors.margins:root.gap
            visible:!root.collapsed
            contentHeight:body.implicitHeight;clip:true;boundsBehavior:Flickable.StopAtBounds
            function reveal(item){if(!item)return;const p=item.mapToItem(body,0,0);if(p.y<contentY)contentY=p.y;else if(p.y+item.height>contentY+height)contentY=p.y+item.height-height}
            Column {
                id:body;width:scroll.width;spacing:12*root.unit
                Item {
                    width:parent.width;height:36*root.unit
                    Text {anchors.left:parent.left;anchors.verticalCenter:parent.verticalCenter;text:"Group layout";color:root.appearance.textColor;font.pixelSize:24*root.unit}
                    Text {anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;text:"Hide controls";color:root.appearance.mutedColor;font.pixelSize:14*root.unit
                        MouseArea {anchors.fill:parent;anchors.margins:-6*root.unit;onClicked:root.collapsed=true}
                    }
                }
                Row {
                    id:modeRow;width:parent.width;spacing:6*root.unit
                    Repeater {
                        model:root.modeNames
                        delegate:Rectangle {
                            required property int index;required property string modelData
                            objectName:"studioChildMode-"+root.modes[index]
                            width:(body.width-12*root.unit)/3;height:44*root.unit;radius:6*root.unit
                            color:root.value.mode===root.modes[index]?root.appearance.selectedColor:"transparent"
                            border.width:root.choice===0 && root.value.mode===root.modes[index]?2:1;border.color:root.choice===0 && root.value.mode===root.modes[index]?root.appearance.focusColor:root.appearance.mutedColor
                            Text {anchors.centerIn:parent;text:modelData;color:root.appearance.textColor;font.pixelSize:16*root.unit}
                            Accessible.role:Accessible.RadioButton;Accessible.name:modelData;Accessible.checked:root.value.mode===root.modes[index]
                            MouseArea {anchors.fill:parent;onClicked:{root.choice=0;root.setMode(root.modes[index])}}
                        }
                    }
                }
                Item {
                    width:parent.width;height:64*root.unit
                    readonly property int columns:Math.max(1,root.value.columns || 1)
                    readonly property int rows:Math.max(1,root.value.rows || 1)
                    Repeater {
                        model:parent.columns*parent.rows
                        delegate:Rectangle {
                            required property int index
                            width:Math.max(1,(parent.width-4*(parent.columns-1))/parent.columns);height:Math.max(1,(parent.height-4*(parent.rows-1))/parent.rows)
                            x:(index%parent.columns)*(width+4);y:Math.floor(index/parent.columns)*(height+4)
                            color:"transparent";border.width:1;border.color:root.appearance.mutedColor;radius:2*root.unit
                        }
                    }
                }
                Repeater {
                    id:dimensions;model:["Columns","Rows"]
                    delegate:Row {
                        id:dimension
                        required property int index;required property string modelData
                        readonly property string field:index===0?"columns":index===1?"rows":"visibleColumns"
                        readonly property int controlIndex:index===2?5:index+1
                        visible:index===2?root.value.mode==="row":root.value.mode==="grid" || (index===0?root.value.mode==="row":root.value.mode==="column")
                        width:body.width;height:44*root.unit;spacing:8*root.unit
                        Text {width:parent.width-150*root.unit;height:parent.height;verticalAlignment:Text.AlignVCenter;text:dimension.modelData;color:root.appearance.textColor;font.pixelSize:18*root.unit}
                        Repeater {
                            model:["−",String(dimension.index===2?(root.value.visibleColumns || root.value.columns):root.value[dimension.field]),"＋"]
                            delegate:Rectangle {
                                required property int index;required property string modelData
                                objectName:"studioChild-"+dimension.field+"-"+index
                                readonly property bool available:index===1 || (index===0?root.value[dimension.field]>1:root.value[dimension.field]<(dimension.index===0?16:10))
                                width:42*root.unit;height:44*root.unit;radius:6*root.unit
                                color:root.choice===dimension.controlIndex?root.appearance.selectedColor:"transparent"
                                border.width:root.choice===dimension.controlIndex?1:0;border.color:root.appearance.focusColor
                                Text {anchors.centerIn:parent;text:modelData;color:parent.available?root.appearance.textColor:root.appearance.mutedColor;font.pixelSize:18*root.unit}
                                Accessible.role:index===1?Accessible.StaticText:Accessible.Button;Accessible.name:index===1?dimension.modelData+" "+modelData:(index===0?"Decrease ":"Increase ")+dimension.modelData
                                MouseArea {anchors.fill:parent;enabled:index!==1 && parent.available;onClicked:{root.choice=dimension.controlIndex;root.adjust(root.choice,index===0?-1:1)}}
                            }
                        }
                    }
                }
                Repeater {
                    id:spacingRows;model:["Item gap","Inside padding"]
                    delegate:Column {
                        id:scalar
                        required property int index;required property string modelData
                        readonly property string field:index===0?"gap":"padding"
                        width:body.width;spacing:2*root.unit
                        Text {text:scalar.modelData+" · "+root.value[scalar.field];color:root.appearance.textColor;font.pixelSize:18*root.unit}
                        Slider {
                            id:slider;objectName:"studioChild-"+scalar.field
                            width:parent.width;height:36*root.unit;from:0;to:100;stepSize:1;value:root.value[scalar.field] || 0
                            Accessible.name:scalar.modelData
                            onMoved:{root.choice=scalar.index+3;root.setValue(scalar.field,Math.round(value))}
                            background:Rectangle {x:slider.leftPadding;y:slider.topPadding+slider.availableHeight/2-height/2;width:slider.availableWidth;height:4*root.unit;radius:height/2;color:root.appearance.mutedColor}
                            handle:Rectangle {x:slider.leftPadding+slider.visualPosition*(slider.availableWidth-width);y:slider.topPadding+slider.availableHeight/2-height/2;width:20*root.unit;height:width;radius:width/2;color:root.appearance.focusColor;border.width:root.choice===scalar.index+3?2:0;border.color:root.appearance.textColor}
                        }
                    }
                }
                Row {
                    id:capacityRow;visible:root.value.mode==="row"
                    width:body.width;height:44*root.unit;spacing:8*root.unit
                    Text {width:parent.width-150*root.unit;height:parent.height;verticalAlignment:Text.AlignVCenter;text:"Visible cards";color:root.appearance.textColor;font.pixelSize:18*root.unit}
                    Repeater {
                        model:["−",String(root.value.visibleColumns || root.value.columns),"＋"]
                        delegate:Rectangle {
                            required property int index;required property string modelData
                            objectName:"studioChild-visibleColumns-"+index
                            readonly property bool available:index===1 || (index===0?(root.value.visibleColumns || root.value.columns)>1:(root.value.visibleColumns || root.value.columns)<root.value.columns)
                            width:42*root.unit;height:44*root.unit;radius:6*root.unit;color:root.choice===5?root.appearance.selectedColor:"transparent"
                            border.width:root.choice===5?1:0;border.color:root.appearance.focusColor
                            Text {anchors.centerIn:parent;text:modelData;color:parent.available?root.appearance.textColor:root.appearance.mutedColor;font.pixelSize:18*root.unit}
                            Accessible.role:index===1?Accessible.StaticText:Accessible.Button;Accessible.name:index===1?"Visible cards "+modelData:(index===0?"Decrease":"Increase")+" visible cards"
                            MouseArea {anchors.fill:parent;enabled:index!==1 && parent.available;onClicked:{root.choice=5;root.adjust(5,index===0?-1:1)}}
                        }
                    }
                }
                CheckBox {
                    id:wrapToggle;objectName:"studioChildWrap"
                    visible:root.value.mode==="row";width:parent.width;height:44*root.unit
                    text:"Wrap navigation";checked:!!root.value.wrap
                    Accessible.name:text
                    onClicked:{root.choice=6;root.setValue("wrap",checked)}
                    contentItem:Text {leftPadding:44*root.unit;text:wrapToggle.text;verticalAlignment:Text.AlignVCenter;color:root.appearance.textColor;font.pixelSize:18*root.unit}
                    indicator:Rectangle {x:0;y:(wrapToggle.height-height)/2;width:28*root.unit;height:width;radius:5*root.unit;color:wrapToggle.checked?root.appearance.focusColor:"transparent";border.width:2;border.color:root.choice===6?root.appearance.focusColor:root.appearance.mutedColor;Text {anchors.centerIn:parent;text:wrapToggle.checked?"✓":"";color:root.appearance.surfaceColor;font.pixelSize:20*root.unit}}
                }
                Text {width:parent.width;visible:text.length>0;text:root.message;wrapMode:Text.Wrap;color:root.appearance.mutedColor;font.pixelSize:16*root.unit}
                Row {
                    id:actions;width:parent.width;spacing:8*root.unit
                    Repeater {
                        model:["Done","Cancel"]
                        delegate:Rectangle {
                            required property int index;required property string modelData
                            objectName:index===0?"studioChildDone":"studioChildCancel"
                            width:(body.width-8*root.unit)/2;height:44*root.unit;radius:6*root.unit;color:root.choice===index+root.doneIndex?root.appearance.selectedColor:"transparent"
                            border.width:root.choice===index+root.doneIndex?2:0;border.color:root.appearance.focusColor
                            Text {anchors.centerIn:parent;text:modelData;color:index===0 && !root.valid?root.appearance.mutedColor:root.appearance.textColor;font.pixelSize:18*root.unit}
                            Accessible.role:Accessible.Button;Accessible.name:modelData
                            MouseArea {anchors.fill:parent;onClicked:{root.choice=index+root.doneIndex;if(index===0)root.accepted();else root.canceled()}}
                        }
                    }
                }
            }
        }
        Text {visible:root.collapsed;anchors.centerIn:parent;text:"Group layout · Show controls";color:root.appearance.textColor;font.pixelSize:18*root.unit}
        MouseArea {anchors.fill:parent;visible:root.collapsed;onClicked:root.collapsed=false}
    }
    onVisibleChanged:if(visible){choice=0;collapsed=false;gridDimensions={columns:value.columns || 1,rows:value.rows || 1}}
    onChoiceChanged:Qt.callLater(()=>scroll.reveal(choice===0?modeRow:choice<3?dimensions.itemAt(choice-1):choice<5?spacingRows.itemAt(choice-3):choice===5 && value.mode==="row"?capacityRow:choice===6 && value.mode==="row"?wrapToggle:actions))
}
