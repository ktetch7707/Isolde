import QtQuick

Item {
    id:root
    objectName:"guideView"
    required property var controller
    required property var dispatcher
    required property var options
    required property var appearance
    property bool open:false
    property bool suppressed:false
    property string originId:""
    property var presentation:({})
    property string mode:""
    property int headerIndex:0
    property bool headerFocused:true
    property int selectedIndex:0
    property string selectedChannelId:""
    property real nowUtc:Date.now()/1000
    property real anchorUtc:nowUtc
    property real fromUtc:Math.floor(nowUtc/1800)*1800
    property int menuIndex:0
    property int fieldIndex:0
    property var draft:({})
    property string mediaKind:"episode"
    property string managedId:""
    property string editField:""
    property string keyboardReturn:""
    property var metadataDraft:({})
    property bool metadataSaving:false
    property var moveIds:[]
    property var moveRevisions:({})
    property bool moveSaving:false
    readonly property real fontSize:(presentation.labelSize || options.labelSize)*(appearance.typographyScale || 1)
    readonly property real gap:presentation.cardGap===undefined?options.gap:presentation.cardGap
    readonly property real radius:presentation.radius===undefined?options.radius:presentation.radius
    readonly property int rowCount:presentation.guideRows || options.rows
    readonly property int minutes:presentation.guideMinutes || options.minutes
    readonly property real channelWidth:presentation.guideChannelWidth || options.channelWidth
    readonly property bool labels:presentation.guideLabels===undefined?options.labels:presentation.guideLabels
    readonly property bool sourceBadges:presentation.guideSourceBadges===undefined?options.sourceBadges:presentation.guideSourceBadges
    readonly property var channels:controller?controller.channels.filter(c=>!c.hidden):[]
    readonly property var selectedChannel:channels[selectedIndex] || null
    readonly property var selectedRow:controller?controller.rows.find(c=>selectedChannel && c.id===selectedChannel.id):null
    readonly property var selectedProgram:selectedRow?(selectedRow.programs || []).find(p=>p.displayStartUtc<=anchorUtc && p.displayEndUtc>anchorUtc):null
    readonly property var managedChannel:controller?controller.channels.find(c=>c.id===managedId):null
    readonly property bool previewCurrent:controller && controller.previewReady && JSON.stringify(controller.previewDefinition)===JSON.stringify(draft)
    readonly property var createFields:["Name: "+(draft.name || ""),"Channel number: "+(draft.number || ""),"Media: "+mediaKind,"Edit matching rules","Sequence: "+(draft.sequence || ""),"Clock: "+(draft.clock || ""),"Preview matches",previewCurrent?"Save channel":"Preview these settings before saving","Cancel"]
    readonly property var menuRows:buildMenu()
    visible:open && !suppressed
    signal closed(string origin)
    function show(origin,style){originId=origin || "";presentation=style || {};open=true;mode="";headerFocused=true;headerIndex=0;nowUtc=Date.now()/1000;anchorUtc=nowUtc;fromUtc=Math.floor(nowUtc/1800)*1800;loadWindow()}
    function close(){open=false;mode="";closed(originId)}
    function loadWindow(){if(controller)controller.setWindow(Math.floor(fromUtc),Math.floor(fromUtc+minutes*60),Math.floor(selectedIndex/50)*50)}
    function pickChannel(index){selectedIndex=Math.max(0,Math.min(channels.length-1,index));selectedChannelId=selectedChannel?selectedChannel.id:"";if(controller && !controller.rows.some(c=>c.id===selectedChannelId))loadWindow()}
    function moveTimeline(delta){
        const programs=selectedRow?selectedRow.programs || []:[];const slot=selectedProgram
        anchorUtc=slot?(delta>0?slot.displayEndUtc:slot.displayStartUtc-1):anchorUtc+delta*1800
        if(anchorUtc<fromUtc || anchorUtc>=fromUtc+minutes*60){fromUtc=Math.floor(anchorUtc/1800)*1800;loadWindow()}
    }
    function timeLabel(value){return new Date(value*1000).toLocaleTimeString(Qt.locale(),"hh:mm")}
    function buildMenu(){
        if(mode==="add")return ["Create local channel","Add source","Back"]
        if(mode==="sources")return options.sources.map(s=>s.name+" · App destination").concat(["Back"])
        if(mode==="manage")return (controller?controller.channels:[]).map(c=>c.number+" · "+c.name+(c.hidden?" · Hidden":"")+(c.favorite?" · Favorite":"")).concat(["Back"])
        if(mode==="move")return moveIds.map(id=>{const c=controller.channels.find(c=>c.id===id);return c?c.number+" · "+c.name:"Removed channel"})
        if(mode==="context")return [managedChannel && managedChannel.favorite?"Remove favorite":"Favorite",managedChannel && managedChannel.hidden?"Show channel":"Hide channel","Move up","Move down","Rename","Channel number","Delete channel…","Back"]
        if(mode==="delete")return ["Cancel","Delete local channel and its schedule"]
        if(mode==="discard")return ["Keep editing","Discard unsaved channel"]
        if(mode==="metadataReview")return ["Save "+editField+": "+String(metadataDraft[editField]),"Cancel"]
        if(mode==="details")return [selectedProgram?selectedProgram.title:"No listings available",selectedProgram && selectedProgram.displayStartUtc<=nowUtc && selectedProgram.displayEndUtc>nowUtc?"Watch live":"This program is not currently airing","Back"]
        return []
    }
    function invokeHeader(){
        if(controller.busy && !controller.backgroundBusy)return
        const id=options.toolbar[headerIndex].id
        if(id==="now"){nowUtc=Date.now()/1000;anchorUtc=nowUtc;fromUtc=Math.floor(nowUtc/1800)*1800;loadWindow()}
        else if(id==="add"){mode="add";menuIndex=0}
        else if(id==="manage"){mode="manage";menuIndex=0}
        else if(id==="sources"){mode="sources";menuIndex=0}
        else close()
    }
    function invokeCreate(delta){
        if(controller.busy && !controller.backgroundBusy)return
        let value=JSON.parse(JSON.stringify(draft));delta=delta || 1
        if(fieldIndex===0 || fieldIndex===1){editField=fieldIndex===0?"name":"number";keyboard.value=String(draft[editField]);keyboard.selectedIndex=0;keyboardReturn="create";mode="keyboard"}
        else if(fieldIndex===2){const kinds=["episode","movie","mixed","musicvideo","song"];mediaKind=kinds[(kinds.indexOf(mediaKind)+delta+kinds.length)%kinds.length];value.mediaKinds=mediaKind==="mixed"?["movie","episode"]:[mediaKind];draft=value}
        else if(fieldIndex===3){rules.begin(draft.rule);mode="rules"}
        else if(fieldIndex===4){const values=["seeded-shuffle","episode-order","rotation"];value.sequence=values[(values.indexOf(value.sequence)+delta+values.length)%values.length];draft=value}
        else if(fieldIndex===5){value.clock=value.clock==="real-time"?"pause-when-off":"real-time";draft=value}
        else if(fieldIndex===6)controller.previewLocal(draft,mediaKind)
        else if(fieldIndex===7)controller.createLocal(draft)
        else {mode="discard";menuIndex=0}
    }
    function invokeMenu(){
        if(controller.busy && !controller.backgroundBusy)return
        if(mode==="add"){
            if(menuIndex===0){draft=controller.newDefinition();mediaKind="episode";fieldIndex=0;mode="create"}
            else if(menuIndex===1){mode="sources";menuIndex=0}else mode=""
        }else if(mode==="sources"){
            if(menuIndex<options.sources.length)dispatcher.dispatch(options.sources[menuIndex].action,originId)
            else mode=""
        }else if(mode==="manage"){
            if(menuIndex<controller.channels.length){managedId=controller.channels[menuIndex].id;mode="context";menuIndex=0}else mode=""
        }else if(mode==="context"){
            const c=managedChannel;if(!c){mode="manage";menuIndex=0;return}
            if(menuIndex===0)controller.setPreferences(c.id,{favorite:!c.favorite},c.revision)
            else if(menuIndex===1)controller.setPreferences(c.id,{hidden:!c.hidden},c.revision)
            else if(menuIndex===2 || menuIndex===3){
                const direction=menuIndex===2?-1:1
                moveIds=controller.channels.map(x=>x.id);let revisions={};for(const item of controller.channels)revisions[item.id]=item.revision
                moveRevisions=revisions;mode="move";previewMove(direction)
            }else if(menuIndex===4 || menuIndex===5){editField=menuIndex===4?"name":"number";keyboard.value=String(c[editField]);keyboard.selectedIndex=0;keyboardReturn="metadataReview";mode="keyboard"}
            else if(menuIndex===6){mode="delete";menuIndex=0}else{mode="manage";menuIndex=0}
        }else if(mode==="delete"){
            if(menuIndex===1 && managedChannel)controller.remove(managedChannel.id,managedChannel.revision)
            mode="manage";menuIndex=0
        }else if(mode==="discard"){
            if(menuIndex===0)mode="create";else{controller.cancel();mode=""}
        }else if(mode==="metadataReview"){
            if(menuIndex===0 && managedChannel){metadataSaving=controller.setMetadata(managedChannel.id,metadataDraft,managedChannel.revision)}else mode="context"
        }else if(mode==="details"){
            if(menuIndex===1 && selectedProgram && selectedProgram.displayStartUtc<=nowUtc && selectedProgram.displayEndUtc>nowUtc)watch()
            else if(menuIndex===2)mode=""
        }
    }
    function previewMove(delta){
        let ids=moveIds.slice();const at=ids.indexOf(managedId),to=Math.max(0,Math.min(ids.length-1,at+delta))
        if(at>=0){ids.splice(at,1);ids.splice(to,0,managedId);moveIds=ids;menuIndex=to}
    }
    function watch(){
        nowUtc=Date.now()/1000
        if(selectedChannel && (!selectedProgram || selectedProgram.displayStartUtc<=nowUtc && selectedProgram.displayEndUtc>nowUtc))dispatcher.dispatch({type:"tune-channel",channelId:selectedChannel.id},originId)
        mode=""
    }
    function handleCommand(command){
        if(mode==="keyboard"){keyboard.handleCommand(command);return}
        if(mode==="rules"){rules.handleCommand(command);return}
        if(command===inputManager.back){
            if(controller.busy && !controller.backgroundBusy){controller.cancel();return}
            if(mode==="move"){moveIds=[];mode="context";menuIndex=2}
            else if(mode==="create"){mode="discard";menuIndex=0}else if(mode==="context" || mode==="delete" || mode==="metadataReview"){mode="manage";menuIndex=0}else if(mode)mode="";else close()
            return
        }
        if(controller.busy && !controller.backgroundBusy)return
        if(mode==="move"){
            if(command===inputManager.navigateUp)previewMove(-1)
            else if(command===inputManager.navigateDown)previewMove(1)
            else if(command===inputManager.confirm)moveSaving=controller.reorder(moveIds,moveRevisions)
            return
        }
        if(mode==="create"){
            if(command===inputManager.navigateUp)fieldIndex=Math.max(0,fieldIndex-1)
            else if(command===inputManager.navigateDown)fieldIndex=Math.min(createFields.length-1,fieldIndex+1)
            else if(command===inputManager.confirm)invokeCreate(1)
            else if(command===inputManager.navigateLeft || command===inputManager.navigateRight){if(fieldIndex===2 || fieldIndex===4 || fieldIndex===5)invokeCreate(command===inputManager.navigateLeft?-1:1)}
            return
        }
        if(mode){
            if(command===inputManager.navigateUp)menuIndex=Math.max(0,menuIndex-1)
            else if(command===inputManager.navigateDown)menuIndex=Math.min(menuRows.length-1,menuIndex+1)
            else if(command===inputManager.confirm)invokeMenu()
            return
        }
        if(command===inputManager.menu){if(selectedChannel){managedId=selectedChannel.id;mode="context"}else mode="add";menuIndex=0;return}
        if(headerFocused){
            if(command===inputManager.navigateLeft)headerIndex=Math.max(0,headerIndex-1)
            else if(command===inputManager.navigateRight)headerIndex=Math.min(options.toolbar.length-1,headerIndex+1)
            else if(command===inputManager.navigateDown && channels.length){headerFocused=false;pickChannel(selectedIndex)}
            else if(command===inputManager.confirm)invokeHeader()
            return
        }
        if(command===inputManager.navigateUp){if(selectedIndex===0)headerFocused=true;else pickChannel(selectedIndex-1)}
        else if(command===inputManager.navigateDown)pickChannel(selectedIndex+1)
        else if(command===inputManager.navigateLeft)moveTimeline(-1)
        else if(command===inputManager.navigateRight)moveTimeline(1)
        else if(command===inputManager.confirm){nowUtc=Date.now()/1000;if(selectedProgram && !(selectedProgram.displayStartUtc<=nowUtc && selectedProgram.displayEndUtc>nowUtc)){mode="details";menuIndex=0}else watch()}
    }
    Connections {target:root.controller
        function onChannelCreated(id){root.mode="";root.headerFocused=false;root.nowUtc=Date.now()/1000;root.anchorUtc=root.nowUtc;root.fromUtc=Math.floor(root.nowUtc/1800)*1800;root.pickChannel(root.channels.findIndex(c=>c.id===id));root.loadWindow()}
        function onChanged(){
            if(root.metadataSaving && !root.controller.busy){root.metadataSaving=false;if(!root.controller.errorString)root.mode="manage"}
            if(root.moveSaving && !root.controller.busy){root.moveSaving=false;if(!root.controller.errorString){root.moveIds=[];root.mode="context";root.menuIndex=2}}
            if(root.selectedChannelId){const index=root.channels.findIndex(c=>c.id===root.selectedChannelId);if(index>=0)root.selectedIndex=index;else if(root.selectedIndex>=root.channels.length)root.selectedIndex=Math.max(0,root.channels.length-1)}
        }
    }
    Timer {interval:30000;repeat:true;running:root.open;onTriggered:{root.nowUtc=Date.now()/1000;if(!root.controller.busy)root.loadWindow()}}
    Rectangle {anchors.fill:parent;color:root.appearance.backgroundStart || root.appearance.surfaceColor}
    Column {
        anchors.fill:parent;anchors.margins:root.gap*2;spacing:root.gap
        Text {width:parent.width;textFormat:Text.PlainText;text:"Live TV · "+root.timeLabel(root.nowUtc);font.pixelSize:root.fontSize*1.4;color:root.appearance.textColor}
        Row {width:parent.width;height:root.fontSize*2.2;spacing:root.gap
            Repeater {model:root.options.toolbar
                delegate:Rectangle {required property var modelData;required property int index;width:(parent.width-root.gap*(root.options.toolbar.length-1))/root.options.toolbar.length;height:parent.height;radius:root.radius;color:root.appearance.surfaceColor;border.width:!root.mode && root.headerFocused && root.headerIndex===index?2:0;border.color:root.appearance.focusColor
                    Text {anchors.fill:parent;anchors.margins:root.gap;textFormat:Text.PlainText;text:modelData.label;color:root.appearance.textColor;font.pixelSize:root.fontSize;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter}
                    MouseArea {anchors.fill:parent;onClicked:{root.headerIndex=index;root.headerFocused=true;root.invokeHeader()}}
                }
            }
        }
        Text {width:parent.width;textFormat:Text.PlainText;text:root.selectedProgram?(root.selectedProgram.title+" · "+root.timeLabel(root.selectedProgram.displayStartUtc)+"–"+root.timeLabel(root.selectedProgram.displayEndUtc)):"Select a channel · Add creates local channels or connects sources";color:root.appearance.textColor;font.pixelSize:root.fontSize;elide:Text.ElideRight}
        Item {width:parent.width;height:root.fontSize*1.4
            Repeater {model:4;delegate:Text {required property int index;x:root.channelWidth+(parent.width-root.channelWidth)*index/4;text:root.timeLabel(root.fromUtc+root.minutes*60*index/4);color:root.appearance.mutedColor;font.pixelSize:root.fontSize*.8}}
        }
        ListView {id:timeline;width:parent.width;height:Math.max(root.fontSize*3,parent.height-root.fontSize*12);clip:true;model:root.controller.rows;currentIndex:root.controller.rows.findIndex(c=>root.selectedChannel && c.id===root.selectedChannel.id);highlightMoveDuration:0;highlightRangeMode:ListView.ApplyRange
            delegate:Item {required property var modelData;required property int index;width:timeline.width;height:timeline.height/root.rowCount
                Rectangle {anchors.fill:parent;anchors.bottomMargin:root.gap;color:"transparent";radius:root.radius;border.width:!root.headerFocused && !root.mode && root.selectedChannel && root.selectedChannel.id===modelData.id?2:0;border.color:root.appearance.focusColor}
                Text {x:root.gap;y:root.gap;width:root.channelWidth-root.gap*2;height:parent.height-root.gap*2;textFormat:Text.PlainText;text:modelData.number+" · "+modelData.name+(root.sourceBadges?"\n"+modelData.sourceName:"");color:root.appearance.textColor;font.pixelSize:root.fontSize;elide:Text.ElideRight;wrapMode:Text.WordWrap}
                Item {x:root.channelWidth;width:parent.width-root.channelWidth;height:parent.height;clip:true
                    Text {anchors.fill:parent;anchors.margins:root.gap;visible:!modelData.programs.length;textFormat:Text.PlainText;text:"No listings in this time window";color:root.appearance.mutedColor;font.pixelSize:root.fontSize}
                    Repeater {model:modelData.programs
                        delegate:Rectangle {required property var modelData;x:(modelData.displayStartUtc-root.fromUtc)/(root.minutes*60)*parent.width;width:Math.max(1,(modelData.displayEndUtc-modelData.displayStartUtc)/(root.minutes*60)*parent.width-root.gap/2);height:parent.height-root.gap;radius:root.radius;color:root.appearance.surfaceColor;border.color:root.appearance.focusColor;border.width:!root.headerFocused && !root.mode && root.selectedProgram && root.selectedProgram.id===modelData.id?2:0
                            Text {anchors.fill:parent;anchors.margins:root.gap;visible:root.labels;textFormat:Text.PlainText;text:modelData.title+(modelData.status!=="ready"?" · Unavailable":"");color:root.appearance.textColor;font.pixelSize:root.fontSize;elide:Text.ElideRight;wrapMode:Text.WordWrap}
                            MouseArea {anchors.fill:parent;onClicked:{root.headerFocused=false;root.pickChannel(root.channels.findIndex(c=>c.id===modelData.channelId));root.anchorUtc=modelData.displayStartUtc;root.handleCommand(inputManager.confirm)}}
                        }
                    }
                    Rectangle {x:(root.nowUtc-root.fromUtc)/(root.minutes*60)*parent.width;visible:x>=0 && x<parent.width;width:2;height:parent.height;color:root.appearance.focusColor}
                }
            }
            Text {anchors.centerIn:parent;width:parent.width;textFormat:Text.PlainText;horizontalAlignment:Text.AlignHCenter;wrapMode:Text.WordWrap;visible:!root.channels.length;text:root.controller.ready?"No visible channels. Add a local channel or use Manage to restore hidden channels.":root.controller.errorString || "Opening saved guide…";color:root.appearance.textColor;font.pixelSize:root.fontSize}
        }
        Text {width:parent.width;textFormat:Text.PlainText;text:root.controller.errorString || root.controller.busy?root.controller.errorString || "Working · Back cancels safely":root.controller.status+" · D-pad browse · Menu options · Back returns";font.pixelSize:root.fontSize*.8;color:root.appearance.mutedColor;elide:Text.ElideRight}
    }
    Rectangle {anchors.fill:parent;anchors.margins:root.gap;visible:!!root.mode;color:root.appearance.surfaceColor;radius:root.radius
        Column {anchors.fill:parent;anchors.margins:root.gap*2;spacing:root.gap;visible:root.mode!=="keyboard" && root.mode!=="rules"
            Text {width:parent.width;textFormat:Text.PlainText;text:root.mode==="move"?"Move preview · D-pad reorders · Confirm applies · Back cancels":root.mode==="create"?"Create local channel · Unsaved draft":root.mode==="sources"?"Sources · App destinations do not supply channel listings":root.mode==="delete"?"Remove only this local definition and schedule? Media and Kodi stay unchanged.":root.mode==="discard"?"Discard this unsaved channel?":root.managedChannel && root.mode==="context"?root.managedChannel.name:root.mode;wrapMode:Text.WordWrap;color:root.appearance.textColor;font.pixelSize:root.fontSize*1.2}
            ListView {id:menu;width:parent.width;height:Math.max(1,parent.height-root.fontSize*6);clip:true;model:root.mode==="create"?root.createFields:root.menuRows;currentIndex:root.mode==="create"?root.fieldIndex:root.menuIndex;highlightRangeMode:ListView.ApplyRange;highlightMoveDuration:0
                delegate:Rectangle {required property int index;required property string modelData;width:menu.width;height:root.fontSize*2.3;radius:root.radius;color:root.appearance.surfaceColor;border.width:menu.currentIndex===index?2:0;border.color:root.appearance.focusColor
                    Text {anchors.fill:parent;anchors.margins:root.gap;textFormat:Text.PlainText;text:modelData;color:root.appearance.textColor;font.pixelSize:root.fontSize;elide:Text.ElideRight}
                    MouseArea {anchors.fill:parent;onClicked:{if(root.mode==="move")root.previewMove(index-root.moveIds.indexOf(root.managedId));else if(root.mode==="create"){root.fieldIndex=index;root.invokeCreate(1)}else{root.menuIndex=index;root.invokeMenu()}}}
                }
            }
            Text {width:parent.width;textFormat:Text.PlainText;wrapMode:Text.WordWrap;maximumLineCount:3;elide:Text.ElideRight;text:root.controller.errorString || (root.controller.busy?"Working · Back cancels safely":root.mode==="sources" && root.menuIndex<root.options.sources.length?root.options.sources[root.menuIndex].description:root.controller.status+(root.mode==="create" && root.controller.previewReady?"\n"+(root.previewCurrent?"Current preview: ":"Settings changed — preview again: ")+root.controller.previewItems.map(i=>i.title).join(" · "):""));color:root.appearance.mutedColor;font.pixelSize:root.fontSize*.85}
        }
    }
    GuideRuleEditor {id:rules;anchors.fill:parent;anchors.margins:root.gap*2;visible:root.mode==="rules";appearance:root.appearance;options:root.options
        onCanceled:root.mode="create"
        onAccepted:rule=>{let value=JSON.parse(JSON.stringify(root.draft));value.rule=rule;root.draft=value;root.mode="create"}
    }
    OnScreenKeyboard {id:keyboard;anchors.centerIn:parent;width:Math.min(parent.width-root.gap*4,850);height:Math.min(parent.height,440);visible:root.mode==="keyboard";appearance:root.appearance
        onCanceled:root.mode=root.keyboardReturn==="create"?"create":"context"
        onAccepted:text=>{
            const value=root.editField==="number"?Number(text):text
            if(root.keyboardReturn==="create"){let copy=JSON.parse(JSON.stringify(root.draft));copy[root.editField]=value;root.draft=copy;root.mode="create"}
            else{let copy={};copy[root.editField]=value;root.metadataDraft=copy;root.mode="metadataReview";root.menuIndex=0}
        }
    }
}
