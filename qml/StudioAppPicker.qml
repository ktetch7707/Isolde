import QtQuick
import QtQuick.Controls

Item {
    id: root
    objectName:"studioAppPicker"
    required property var apps
    required property var appearance
    required property var settings
    property var selection:[]
    property string contentMode:"selected"
    property bool selectedView:false
    property bool toolsFocused:true
    property int control:5
    property int index:0
    property string focusedId:""
    property string movingId:""
    property var orderBaseline:[]
    property string message:""
    readonly property real unit:Math.min(width/1280,height/720)*(appearance.typographyScale || 1)
    readonly property var controls:["Use all installed","Select all","Clear","Order selected","Remove focused","Done","Cancel"]
    readonly property var entries:selectedView?selection.map(id=>apps.find(a=>a.action.appId===id) || {name:"Unavailable app",missing:true,action:{appId:id}}):apps
    onAppsChanged:if(contentMode==="all" && !movingId)selection=apps.length<=128?apps.map(a=>a.action.appId):[]
    onIndexChanged:if(entries[index])focusedId=entries[index].action.appId
    onEntriesChanged:{const retained=entries.findIndex(a=>a.action.appId===focusedId);index=retained>=0?retained:Math.max(0,Math.min(index,entries.length-1))}
    signal accepted(string mode,var ids)
    signal canceled()
    function begin(ids,mode){contentMode=mode || "selected";selection=contentMode==="all" && apps.length<=128?apps.map(a=>a.action.appId):(ids || []).slice();selectedView=false;toolsFocused=true;control=5;index=0;focusedId="";movingId="";orderBaseline=[];message=""}
    function toggle(id){const next=selection.slice(),at=next.indexOf(id);if(at>=0)next.splice(at,1);else if(next.length<128)next.push(id);else{message="Choose up to 128 apps.";return}contentMode="selected";selection=next;message=""}
    function activateTool(){
        if(control===0){if(apps.length>128){message="More than 128 apps are available. Choose a smaller selection.";return}accepted("all",[]);return}
        if(control===1){if(apps.length>128){message="More than 128 apps are available. Choose a smaller selection.";return}contentMode="selected";selection=apps.map(a=>a.action.appId);message=""}
        else if(control===2){contentMode="selected";selection=[];movingId="";message=""}
        else if(control===3){contentMode="selected";selectedView=!selectedView;toolsFocused=entries.length===0;index=0;message=selectedView?"Select an app to move it in this order.":""}
        else if(control===4){const id=focusedId;if(selection.indexOf(id)>=0){contentMode="selected";selection=selection.filter(value=>value!==id);message="Removed from this collection."}else message="Focus an app in the selected order first."}
        else if(control===5){if(contentMode==="all" && apps.length>128){message="Choose up to 128 apps.";return}accepted(contentMode,contentMode==="all"?[]:selection.slice())}
        else canceled()
    }
    function activateItem(){
        const entry=entries[index];if(!entry)return
        if(selectedView){if(movingId){movingId="";orderBaseline=[];message="Order updated. Done accepts the collection."}else{movingId=entry.action.appId;orderBaseline=selection.slice();message="Moving "+entry.name+". Confirm keeps this order; Back restores it."}}
        else toggle(entry.action.appId)
    }
    function moveTo(target){
        const at=selection.indexOf(movingId)
        if(at<0 || target<0 || target>=selection.length){message="Collection boundary.";return}
        const next=selection.slice();next.splice(at,1);next.splice(target,0,movingId);selection=next;index=target
    }
    function handleCommand(command){
        if(command===inputManager.back){if(movingId){selection=orderBaseline.slice();movingId="";orderBaseline=[];message="Order restored."}else canceled();return}
        if(movingId){
            if(command===inputManager.confirm){activateItem();return}
            const delta=command===inputManager.navigateLeft?-1:command===inputManager.navigateRight?1:command===inputManager.navigateUp?-settings.columns:command===inputManager.navigateDown?settings.columns:0
            if(!delta)return
            moveTo(selection.indexOf(movingId)+delta);return
        }
        if(toolsFocused){
            if(command===inputManager.confirm)activateTool()
            else if(command===inputManager.navigateLeft)control=Math.max(0,control-1)
            else if(command===inputManager.navigateRight)control=Math.min(controls.length-1,control+1)
            else if(command===inputManager.navigateDown && entries.length){toolsFocused=false;index=Math.min(index,entries.length-1)}
            return
        }
        if(command===inputManager.confirm){activateItem();return}
        if(command===inputManager.menu){toolsFocused=true;control=3;return}
        if(command===inputManager.navigateUp && index<settings.columns){toolsFocused=true;return}
        const delta=command===inputManager.navigateLeft?-1:command===inputManager.navigateRight?1:command===inputManager.navigateUp?-settings.columns:command===inputManager.navigateDown?settings.columns:0
        index=Math.max(0,Math.min(entries.length-1,index+delta))
    }
    Rectangle {
        anchors.fill:parent;anchors.margins:root.settings.margin*root.unit
        color:root.appearance.surfaceColor;radius:root.settings.radius*root.unit;border.width:2;border.color:root.appearance.focusColor
        Column {
            anchors.fill:parent;anchors.margins:root.settings.margin*root.unit;spacing:root.settings.gap*root.unit
            Text {width:parent.width;text:root.contentMode==="all"?"All installed apps":(root.selectedView?"Selected order":"Choose apps")+" · "+root.selection.length+" selected";textFormat:Text.PlainText;color:root.appearance.textColor;font.family:root.appearance.fontFamily || "DejaVu Sans";font.pixelSize:root.settings.labelSize*root.unit*1.3}
            Row {
                width:parent.width;spacing:root.settings.gap*root.unit/2
                Repeater {model:root.controls;delegate:Rectangle {
                    required property int index;required property string modelData
                    objectName:"studioAppPickerTool-"+index
                    width:(parent.width-6*parent.spacing)/7;height:56*root.unit;radius:root.settings.radius*root.unit
                    color:root.toolsFocused && root.control===index?root.appearance.selectedColor:"transparent"
                    border.width:root.toolsFocused && root.control===index?2:1;border.color:root.appearance.focusColor
                    Text {anchors.fill:parent;anchors.margins:4*root.unit;text:parent.index===3 && root.selectedView?"Choose apps":parent.modelData;wrapMode:Text.Wrap;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;color:root.appearance.textColor;font.pixelSize:root.settings.labelSize*root.unit}
                    Accessible.role:Accessible.Button;Accessible.name:index===3 && root.selectedView?"Choose apps":modelData
                    MouseArea {anchors.fill:parent;enabled:!root.movingId;onClicked:{root.toolsFocused=true;root.control=index;root.activateTool()}}
                }}
            }
            Text {width:parent.width;text:root.message || (root.entries.length?"":"No apps in this selection.");textFormat:Text.PlainText;color:root.appearance.mutedColor;font.pixelSize:root.settings.labelSize*root.unit;elide:Text.ElideRight}
            GridView {
                id:grid;objectName:"studioAppPickerGrid";width:parent.width;height:parent.height-y;clip:true
                cellWidth:width/root.settings.columns;cellHeight:root.settings.cardHeight*root.unit
                model:root.entries;currentIndex:root.index;highlightMoveDuration:0;highlightRangeMode:GridView.ApplyRange
                delegate:Rectangle {
                    required property int index;required property var modelData
                    objectName:"studioAppChoice-"+modelData.action.appId
                    width:grid.cellWidth-root.settings.gap*root.unit;height:grid.cellHeight-root.settings.gap*root.unit;radius:root.settings.radius*root.unit
                    color:!root.toolsFocused && root.index===index?root.appearance.selectedColor:"transparent"
                    border.width:root.movingId===modelData.action.appId?3:!root.toolsFocused && root.index===index?2:1;border.color:root.appearance.focusColor
                    readonly property bool chosen:root.selection.indexOf(modelData.action.appId)>=0
                    Image {anchors.horizontalCenter:parent.horizontalCenter;y:root.settings.gap*root.unit;width:root.settings.logoSize*root.unit;height:width;source:modelData.logoSource || "";fillMode:Image.PreserveAspectFit;asynchronous:true}
                    Text {anchors.top:parent.top;anchors.right:parent.right;anchors.margins:root.settings.gap*root.unit;text:parent.chosen?"✓":"□";color:root.appearance.textColor;font.pixelSize:root.settings.labelSize*root.unit}
                    Text {anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.margins:root.settings.gap*root.unit;text:(modelData.missing?"Unavailable · ":"")+modelData.name;textFormat:Text.PlainText;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight;color:root.appearance.textColor;font.family:root.appearance.fontFamily || "DejaVu Sans";font.pixelSize:root.settings.labelSize*root.unit}
                    Accessible.role:Accessible.CheckBox;Accessible.name:modelData.name;Accessible.checked:chosen
                    MouseArea {anchors.fill:parent;onClicked:{
                        // Moving replaces the model and can destroy this delegate.
                        // Keep the picker alive in this handler's local scope.
                        const picker=root,target=index
                        picker.toolsFocused=false
                        if(picker.movingId)picker.moveTo(target);else picker.index=target
                        picker.activateItem()
                    }}
                }
            }
        }
    }
}
