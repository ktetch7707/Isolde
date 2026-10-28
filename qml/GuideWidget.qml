import QtQuick

Item {
    id:root
    required property var controller
    required property var dispatcher
    required property var options
    property string originId:""
    property var style:({})
    property var appearance:({})
    property bool engaged:false
    property bool readOnly:false
    property int selectedIndex:-1
    property string selectedChannelId:""
    readonly property var rows:controller?controller.compactRows.slice(0,Math.min(5,style.libraryLimit || 5)):[]
    readonly property real labelSize:(style.labelSize || options.labelSize)*(appearance.typographyScale || 1)
    readonly property real roundness:style.radius===undefined?options.radius:style.radius
    readonly property bool sourceBadges:style.guideSourceBadges===undefined?options.sourceBadges:style.guideSourceBadges
    signal engage()
    onRowsChanged:if(selectedChannelId){const at=rows.findIndex(c=>c.id===selectedChannelId);selectedIndex=at>=0?at:-1;if(at<0)selectedChannelId=""}
    function select(index){selectedIndex=index;selectedChannelId=rows[index]?rows[index].id:""}
    function progressFor(current){const duration=current.endUtc-current.startUtc;const elapsed=current.clockUtc===undefined?current.offsetSeconds:current.clockUtc-current.startUtc;return duration>0?Math.max(0,Math.min(1,elapsed/duration)):0}
    function programLabel(current){return current.title || (current.launchKind==="kodi_pvr"?"No listings · Watch live":"No available scheduled program")}
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
        if(command===inputManager.confirm){if(selectedIndex<0)openGuide();else if(dispatcher)dispatcher.dispatch({type:"tune-channel",channelId:rows[selectedIndex].id},originId);return true}
        return false
    }
    Rectangle {anchors.fill:parent;color:root.style.surfaceColor || root.appearance.surfaceColor;opacity:root.style.surfaceOpacity===undefined?1:root.style.surfaceOpacity;radius:root.roundness}
    Column {
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
                width:parent.width;height:Math.max(root.labelSize*2.2,(root.height-root.labelSize*3.5)/Math.max(1,root.rows.length)-root.labelSize/3)
                Accessible.role:Accessible.Button
                Accessible.name:modelData.number+" · "+modelData.name
                Accessible.description:(modelData.sourceName || "")+" · "+root.programLabel(modelData.current)
                Accessible.focusable:!root.readOnly
                Accessible.focused:root.visible && root.engaged && root.selectedIndex===index
                Accessible.onPressAction:{root.engage();root.select(index);if(!root.readOnly && root.dispatcher)root.dispatcher.dispatch({type:"tune-channel",channelId:modelData.id},root.originId)}
                color:root.appearance.surfaceColor;radius:root.roundness;border.color:root.appearance.focusColor;border.width:root.engaged && root.selectedIndex===index?(root.style.focusWidth || 2):0
                Text {objectName:"guideWidgetLabel-"+modelData.id;anchors.fill:parent;anchors.margins:root.labelSize/3;textFormat:Text.PlainText;color:root.appearance.textColor;font.pixelSize:root.labelSize;elide:Text.ElideRight;text:modelData.number+" · "+modelData.name+(root.sourceBadges && modelData.sourceName?" · "+modelData.sourceName:"")+"\n"+root.programLabel(modelData.current)}
                Rectangle {anchors.left:parent.left;anchors.bottom:parent.bottom;height:Math.max(2,root.labelSize/7);width:parent.width*root.progressFor(modelData.current);color:root.appearance.focusColor}
                MouseArea {anchors.fill:parent;onClicked:{root.engage();root.select(index);if(!root.readOnly && root.dispatcher)root.dispatcher.dispatch({type:"tune-channel",channelId:modelData.id},root.originId)}}
            }
        }
        Text {visible:!root.rows.length;width:parent.width;wrapMode:Text.WordWrap;textFormat:Text.PlainText;text:root.controller && root.controller.errorString?root.controller.errorString:"Create a local channel or add a source in Full guide.";color:root.appearance.mutedColor;font.pixelSize:root.labelSize}
    }
}
