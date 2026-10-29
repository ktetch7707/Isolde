import QtQuick

Item {
    id:root
    required property var controller
    required property var dispatcher
    required property var options
    property string originId:""
    property var style:({})
    property var appearance:({})
    property var provider:null
    property bool engaged:false
    property bool readOnly:false
    property int selectedIndex:-1
    property string selectedChannelId:""
    readonly property string variant:style.guideVariant || "mini"
    readonly property bool horizontal:variant==="on-now" || style.libraryLayout==="row"
    readonly property var rows:buildRows()
    readonly property real labelSize:(style.labelSize || options.labelSize)*(appearance.typographyScale || 1)
    readonly property real roundness:style.radius===undefined?options.radius:style.radius
    readonly property bool sourceBadges:style.guideSourceBadges===undefined?options.sourceBadges:style.guideSourceBadges
    signal engage()
    function buildRows(){
        if(!controller)return []
        const ids=(style.guideChannels || "").split(",").map(s=>s.trim()).filter(s=>s)
        let result=controller.compactRows.filter(c=>(!ids.length || ids.indexOf(c.id)>=0) && (!style.guideGroup || (c.groups || []).indexOf(style.guideGroup)>=0)
            && (style.guideFilter!=="favorites" && variant!=="favorites" || c.favorite) && (style.guideFilter!=="local" || !c.owner) && (style.guideFilter!=="connected" || !!c.owner))
        if(variant==="upcoming")result=result.filter(c=>(c.upcoming || []).length).map(c=>Object.assign({},c,{current:c.upcoming[0],future:true}))
        if(variant==="now-playing" || variant==="on-now")result=result.filter(c=>!!c.current.title)
        return result.slice(0,variant==="preview"?1:Math.min(5,style.libraryLimit || 5))
    }
    function activate(index){if(readOnly || !dispatcher)return;select(index);if(rows[index].future)openGuide();else dispatcher.dispatch({type:"tune-channel",channelId:rows[index].id},originId)}
    onRowsChanged:if(selectedChannelId){const at=rows.findIndex(c=>c.id===selectedChannelId);selectedIndex=at>=0?at:-1;if(at<0)selectedChannelId=""}
    function select(index){selectedIndex=index;selectedChannelId=rows[index]?rows[index].id:""}
    function progressFor(current){const duration=current.endUtc-current.startUtc;const elapsed=current.clockUtc===undefined?current.offsetSeconds:current.clockUtc-current.startUtc;return duration>0?Math.max(0,Math.min(1,elapsed/duration)):0}
    function programLabel(current){return current.title?(current.title+(current.status==="unavailable"?" · Unavailable":"")):(current.launchKind==="kodi_pvr"?"No listings · Watch live":"No available scheduled program")}
    function openGuide(surface){
        if(readOnly || !dispatcher)return
        const action={type:"open-guide",presentation:Object.assign({},style,surface?{guideSurface:surface}:{})}
        if(selectedChannelId)action.focusChannelId=selectedChannelId
        dispatcher.dispatch(action,originId)
    }
    function handleCommand(command){
        if(readOnly)return false
        if(command===inputManager.back)return false
        if(command===inputManager.menu){openGuide("overlay");return true}
        if(command===inputManager.navigateUp){if(selectedIndex<0)return false;select(selectedIndex-1);return true}
        if(command===inputManager.navigateDown){if(selectedIndex+1<rows.length)select(selectedIndex+1);return true}
        if(horizontal && (command===inputManager.navigateLeft || command===inputManager.navigateRight)){const index=selectedIndex+(command===inputManager.navigateLeft?-1:1);if(index<0 || index>=rows.length)return false;select(index);return true}
        if(command===inputManager.confirm){if(selectedIndex<0)openGuide();else activate(selectedIndex);return true}
        return false
    }
    Rectangle {anchors.fill:parent;color:root.style.surfaceColor || root.appearance.surfaceColor;opacity:root.style.surfaceOpacity===undefined?1:root.style.surfaceOpacity;radius:root.roundness}
    Flow {
        anchors.fill:parent;anchors.margins:root.labelSize/2;spacing:root.labelSize/3
        Rectangle {
            objectName:"guideWidgetHeader"
            width:parent.width;height:root.labelSize*2.4;color:root.appearance.selectedColor;radius:root.roundness
            Accessible.role:Accessible.Button
            Accessible.name:root.style.label || "TV Guide · Full guide / Add channel"
            Accessible.description:"Confirm opens the configured guide view. Menu opens the expanded overlay."
            Accessible.focusable:!root.readOnly
            Accessible.focused:root.visible && root.engaged && root.selectedIndex<0
            Accessible.onPressAction:{root.engage();root.select(-1);root.openGuide()}
            border.width:root.engaged && root.selectedIndex<0?(root.style.focusWidth || 2):0;border.color:root.appearance.focusColor
            Text {anchors.fill:parent;anchors.margins:root.labelSize/3;verticalAlignment:Text.AlignVCenter;textFormat:Text.PlainText;text:root.style.label || "TV Guide · Full guide / Add channel";font.pixelSize:root.labelSize;color:root.appearance.textColor;elide:Text.ElideRight}
            MouseArea {anchors.fill:parent;onClicked:{root.engage();root.select(-1);root.openGuide()}}
        }
        Repeater {
            model:root.rows
            delegate:Rectangle {
                required property var modelData;required property int index
                objectName:"guideWidgetChannel-"+modelData.id
                width:root.horizontal?(parent.width-root.labelSize/3*(root.rows.length-1))/Math.max(1,root.rows.length):parent.width;height:Math.max(root.labelSize*2.2,(root.height-root.labelSize*3.5)/Math.max(1,root.horizontal?1:root.rows.length)-root.labelSize/3)
                Accessible.role:Accessible.Button
                Accessible.name:modelData.number+" · "+modelData.name
                Accessible.description:(modelData.sourceName || "")+" · "+root.programLabel(modelData.current)
                Accessible.focusable:!root.readOnly
                Accessible.focused:root.visible && root.engaged && root.selectedIndex===index
                Accessible.onPressAction:{root.engage();root.activate(index)}
                color:root.appearance.surfaceColor;radius:root.roundness;border.color:root.appearance.focusColor;border.width:root.engaged && root.selectedIndex===index?(root.style.focusWidth || 2):0
                readonly property var art:root.provider && !root.readOnly && root.style.artHeight!==0 && (root.variant==="preview" || root.horizontal) && ["movie","episode","musicvideo","song"].indexOf(modelData.current.mediaType)>=0?root.provider.artwork(modelData.current.mediaType,modelData.current.mediaId):null
                Image {anchors.fill:parent;source:parent.art?parent.art.source:"";fillMode:root.style.artFit==="contain"?Image.PreserveAspectFit:Image.PreserveAspectCrop;opacity:root.style.surfaceOpacity===undefined?1:root.style.surfaceOpacity;asynchronous:true}
                Rectangle {anchors.bottom:parent.bottom;width:parent.width;height:Math.min(parent.height,root.labelSize*3);visible:!!parent.art;color:root.style.labelSurfaceColor || root.appearance.surfaceColor;opacity:root.style.panelOpacity===undefined?1:root.style.panelOpacity;radius:root.roundness}
                Text {objectName:"guideWidgetLabel-"+modelData.id;anchors.bottom:parent.bottom;width:parent.width-root.labelSize*2/3;height:Math.max(0,(parent.art?Math.min(parent.height,root.labelSize*3):parent.height)-root.labelSize*2/3);anchors.left:parent.left;anchors.margins:root.labelSize/3;textFormat:Text.PlainText;color:root.appearance.textColor;font.pixelSize:root.labelSize;elide:Text.ElideRight;text:modelData.number+" · "+modelData.name+(root.sourceBadges && modelData.sourceName?" · "+modelData.sourceName:"")+"\n"+root.programLabel(modelData.current)+(root.variant==="guide" && (modelData.upcoming || []).length?"\nNext: "+modelData.upcoming[0].title:"")}
                Rectangle {anchors.left:parent.left;anchors.bottom:parent.bottom;height:Math.max(2,root.labelSize/7);width:parent.width*root.progressFor(modelData.current);color:root.appearance.focusColor}
                MouseArea {anchors.fill:parent;onClicked:{root.engage();root.activate(index)}}
            }
        }
        Text {visible:!root.rows.length;width:parent.width;wrapMode:Text.WordWrap;textFormat:Text.PlainText;text:root.controller && root.controller.errorString?root.controller.errorString:"Create a local channel or add a source in Full guide.";color:root.appearance.mutedColor;font.pixelSize:root.labelSize}
    }
}
