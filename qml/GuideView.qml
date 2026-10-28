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
    property string pendingFocusChannelId:""
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
    property string numberConflictId:""
    property var numberSwapRevisions:({})
    property var numberSwapReview:({})
    property bool metadataSaving:false
    property var moveIds:[]
    property var moveRevisions:({})
    property bool moveSaving:false
    property bool sourceSaving:false
    property int editRevision:0
    property bool regenerationSaving:false
    property bool repairSaving:false
    property int repairChoice:-1
    property string channelQuery:""
    property string sourceFilter:"all"
    property string groupFilter:""
    property bool favoritesOnly:false
    property string navigationError:""
    readonly property real fontSize:(presentation.labelSize || options.labelSize)*(appearance.typographyScale || 1)
    readonly property real gap:presentation.cardGap===undefined?options.gap:presentation.cardGap
    readonly property real radius:presentation.radius===undefined?options.radius:presentation.radius
    readonly property bool overlay:(presentation.guideSurface || options.surface)==="overlay"
    readonly property bool inOverlay:overlay && !mode
    readonly property real overlayWidthFraction:(presentation.guideOverlayWidth===undefined?options.overlayWidth:presentation.guideOverlayWidth)/100
    readonly property real overlayHeightFraction:(presentation.guideOverlayHeight===undefined?options.overlayHeight:presentation.guideOverlayHeight)/100
    readonly property string overlayPosition:presentation.guideOverlayPosition || options.overlayPosition || ""
    readonly property real overlayHorizontal:overlayPosition.endsWith("left")?0:overlayPosition.endsWith("right")?1:0.5
    readonly property real overlayVertical:overlayPosition.startsWith("top")?0:overlayPosition.startsWith("bottom")?1:0.5
    readonly property int rowCount:inOverlay?(presentation.guideOverlayRows || options.overlayRows):(presentation.guideRows || options.rows)
    readonly property int minutes:presentation.guideMinutes || options.minutes
    readonly property real channelWidth:presentation.guideChannelWidth || options.channelWidth
    readonly property bool labels:presentation.guideLabels===undefined?options.labels:presentation.guideLabels
    readonly property bool sourceBadges:presentation.guideSourceBadges===undefined?options.sourceBadges:presentation.guideSourceBadges
    readonly property var channels:controller?controller.channels.filter(c=>matchesChannel(c)):[]
    readonly property var visibleRows:controller?controller.rows.filter(c=>matchesChannel(c)):[]
    readonly property var groupNames:collectGroups()
    readonly property var selectedChannel:channels[selectedIndex] || null
    readonly property var selectedRow:controller?controller.rows.find(c=>selectedChannel && c.id===selectedChannel.id):null
    readonly property var selectedProgram:selectedRow?(selectedRow.programs || []).find(p=>p.displayStartUtc<=anchorUtc && p.displayEndUtc>anchorUtc):null
    readonly property var managedChannel:controller?controller.channels.find(c=>c.id===managedId):null
    readonly property var conflictingChannel:controller?controller.channels.find(c=>c.id===numberConflictId):null
    readonly property bool previewCurrent:controller && controller.previewReady && JSON.stringify(controller.previewDefinition)===JSON.stringify(draft)
    readonly property var createFields:["Name: "+(draft.name || ""),"Channel number: "+(draft.number || ""),"Media: "+mediaKind,"Edit matching rules","Sequence: "+(draft.sequence || ""),"Clock: "+(draft.clock || ""),"Preview matches",previewCurrent?(editRevision?"Save rules for unscheduled future slots":"Save channel"):"Preview these settings before saving","Schedule horizon, history and timezone","Cancel"]
    readonly property var menuRows:buildMenu()
    readonly property var repairItems:buildRepairItems()
    visible:open && !suppressed
    signal closed(string origin)
    function show(origin,style){showFocused(origin,style,"")}
    function showFocused(origin,style,focusChannelId){
        originId=origin || "";presentation=style || {};open=true;mode="";headerFocused=true;headerIndex=0
        nowUtc=Date.now()/1000;anchorUtc=nowUtc;fromUtc=Math.floor(nowUtc/1800)*1800;navigationError=""
        pendingFocusChannelId=focusChannelId || "";applyPendingFocus()
        loadWindow()
    }
    function applyPendingFocus(){
        if(!open || !controller || !pendingFocusChannelId || !controller.ready || controller.busy)return false
        const focusChannelId=pendingFocusChannelId;pendingFocusChannelId=""
        if(controller.channels.some(c=>c.id===focusChannelId && !c.hidden) && !channels.some(c=>c.id===focusChannelId)){
                channelQuery="";sourceFilter="all";groupFilter="";favoritesOnly=false
        }
        const index=channels.findIndex(c=>c.id===focusChannelId)
        selectedIndex=index>=0?index:0;selectedChannelId=selectedChannel?selectedChannel.id:""
        navigationError=index>=0?"":"The requested channel is no longer visible. Showing the nearest available channel."
        headerFocused=index<0
        return true
    }
    function close(){open=false;mode="";pendingFocusChannelId="";closed(originId)}
    function toggleSurface(){presentation=Object.assign({},presentation,{guideSurface:overlay?"full":"overlay"})}
    function matchesChannel(c){return !c.hidden && (!favoritesOnly || c.favorite) && (!groupFilter || (c.groups || []).indexOf(groupFilter)>=0) && (sourceFilter==="all" || (sourceFilter==="pvr")===(c.owner==="kodi-pvr")) && (!channelQuery || (String(c.number)+" "+c.name).toLocaleLowerCase().indexOf(channelQuery.toLocaleLowerCase())>=0)}
    function collectGroups(){let names=[];if(controller)for(const c of controller.channels)for(const group of c.groups || [])if(names.indexOf(group)<0)names.push(group);return names.sort()}
    function loadWindow(){if(controller){const all=controller.channels.filter(c=>!c.hidden);const index=selectedChannel?all.findIndex(c=>c.id===selectedChannel.id):0;controller.setWindow(Math.floor(fromUtc),Math.floor(fromUtc+minutes*60),Math.floor(Math.max(0,index)/50)*50)}}
    function applyFilter(){selectedIndex=0;selectedChannelId=selectedChannel?selectedChannel.id:"";headerFocused=!channels.length;loadWindow()}
    function pickChannel(index){selectedIndex=Math.max(0,Math.min(channels.length-1,index));selectedChannelId=selectedChannel?selectedChannel.id:"";if(controller && !controller.rows.some(c=>c.id===selectedChannelId))loadWindow()}
    function moveTimeline(delta){
        const programs=selectedRow?selectedRow.programs || []:[];const slot=selectedProgram
        anchorUtc=slot?(delta>0?slot.displayEndUtc:slot.displayStartUtc-1):anchorUtc+delta*1800
        if(anchorUtc<fromUtc || anchorUtc>=fromUtc+minutes*60){fromUtc=Math.floor(anchorUtc/1800)*1800;loadWindow()}
    }
    function timeLabel(value){return new Date(value*1000).toLocaleTimeString(Qt.locale(),"hh:mm")}
    function buildMenu(){
        if(mode==="add")return ["Create local channel","Add source","Back"]
        if(mode==="schedule")return ["Future horizon: "+(draft.horizonDays===undefined?7:draft.horizonDays)+" days","Retained history: "+(draft.retentionDays===undefined?2:draft.retentionDays)+" days","Timezone: "+draft.timezone,"Back to channel draft"]
        if(mode==="browse")return ["Find channel: "+(channelQuery || "All"),"Favorites: "+(favoritesOnly?"Only favorites":"All channels"),"Source: "+sourceFilter,"Jump to local date and time…","Previous day","Next day","Clear filters","Group: "+(groupFilter || "All groups"),"Back"]
        if(mode==="groupFilter")return ["All groups"].concat(groupNames).concat(["Back"])
        if(mode==="sources")return options.sources.map(s=>s.name+(s.kind==="kodi-pvr"?" · Preview channel import":" · App destination")).concat(["Back"])
        if(mode==="sourcePreview")return controller.sourcePreviewReady?controller.sourcePreview.map(e=>"Source "+e.definition.source.number+" → "+e.definition.number+" · "+e.definition.name+" · "+e.programs.length+" listings").concat(["Import reviewed lineup","Cancel"]):["Reading channels and listings…","Cancel"]
        if(mode==="manage")return (controller?controller.channels:[]).map(c=>c.number+" · "+c.name+(c.hidden?" · Hidden":"")+(c.favorite?" · Favorite":"")).concat(["Back"])
        if(mode==="move")return moveIds.map(id=>{const c=controller.channels.find(c=>c.id===id);return c?c.number+" · "+c.name:"Removed channel"})
        if(mode==="context"){const base=[managedChannel && managedChannel.favorite?"Remove favorite":"Favorite",managedChannel && managedChannel.hidden?"Show channel":"Hide channel","Move up","Move down","Rename","Channel number","Delete channel…"];const local=managedChannel && managedChannel.owner!=="kodi-pvr";return base.concat(local?["Edit rules and sequencing","Regenerate future schedule…","Duplicate local channel"]:[]).concat(["Description","Channel groups"]).concat(local?["Repair selected program…"]:[]).concat(["Back"])}
        if(mode==="repair")return repairItems.map(item=>item.label)
        if(mode==="repairConfirm")return ["Cancel",repairChoice<0?"Mark only this slot unavailable":"Apply reviewed replacement"]
        if(mode==="regenerate")return ["Cancel",controller.regenerationReview.slotCount?"Replace reviewed future schedule":"No reviewed replacement boundary available"]
        if(mode==="numberConflict")return ["Choose another number",conflictingChannel?"Confirm swap with "+conflictingChannel.name:"Channel no longer available","Cancel"]
        if(mode==="delete")return ["Cancel","Remove guide channel and cached schedule"]
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
        else if(id==="browse"){mode="browse";menuIndex=0}
        else close()
    }
    function buildRepairItems(){
        const review=controller?controller.repairReview:{};if(!review.slotId)return [{label:"Back",action:"back"}]
        const choices=(review.alternatives || []).map((item,index)=>({label:item.title+" · "+item.duration+" seconds",action:"choose",choice:index}))
        if(review.offset>0)choices.push({label:"Previous replacement page",action:"previous"})
        if(review.offset+review.alternatives.length<review.total)choices.push({label:"Next replacement page",action:"next"})
        return choices.concat([{label:"Mark only this slot unavailable…",action:"unavailable"},{label:"Back",action:"back"}])
    }
    function repairDescription(){
        const review=controller.repairReview;if(!review.program)return "Reading the selected program…"
        let text=review.program.title+" · "+new Date(review.program.displayStartUtc*1000).toLocaleString()+" – "+timeLabel(review.program.displayEndUtc)+"\n"
        if(mode==="repairConfirm")text+=(repairChoice<0?"Mark this slot unavailable.":"Replace with "+review.alternatives[repairChoice].title+".")+" Its ID and time interval stay unchanged; other programs are untouched."
        else text+=review.total+" matching-duration replacements · "+review.incompatibleCount+" different-duration items excluded. Choose a replacement or leave this slot unavailable; nothing is saved until confirmation."
        return text
    }
    function panelTitle(){
        if(mode==="context")return managedChannel?managedChannel.name:"Channel options"
        if(mode==="create")return editRevision?"Edit local channel · Committed slots stay unchanged until explicit regeneration":"Create local channel · Unsaved draft"
        const titles={add:"Add channel",sources:"Sources · App destinations do not supply channel listings",sourcePreview:"Review source channel mapping",manage:"Manage channels",move:"Move preview · D-pad reorders · Confirm applies · Back cancels",regenerate:"Regenerate future schedule",repair:"Repair selected program",repairConfirm:"Review program repair",numberConflict:"Channel number already in use",metadataReview:"Review channel changes",details:"Program details",browse:"Find channels and time",groupFilter:"Choose channel group",schedule:"Schedule policy · Unsaved draft",delete:"Remove only this guide channel and cached schedule? Media and Kodi stay unchanged.",discard:"Discard these unsaved channel edits?"}
        return titles[mode] || "Live TV options"
    }
    function invokeCreate(delta){
        if(controller.busy && !controller.backgroundBusy)return
        let value=JSON.parse(JSON.stringify(draft));delta=delta || 1
        if(fieldIndex===0 || fieldIndex===1){editField=fieldIndex===0?"name":"number";keyboard.value=String(draft[editField]);keyboard.selectedIndex=0;keyboardReturn="create";mode="keyboard"}
        else if(fieldIndex===2){const kinds=["episode","movie","mixed","musicvideo","song","all"];mediaKind=kinds[(kinds.indexOf(mediaKind)+delta+kinds.length)%kinds.length];value.mediaKinds=mediaKind==="mixed"?["movie","episode"]:mediaKind==="all"?["movie","episode","musicvideo","song"]:[mediaKind];draft=value}
        else if(fieldIndex===3){rules.begin(draft.rule);mode="rules"}
        else if(fieldIndex===4){const values=["seeded-shuffle","episode-order","rotation","next-unwatched"];value.sequence=values[(values.indexOf(value.sequence)+delta+values.length)%values.length];draft=value}
        else if(fieldIndex===5){value.clock=value.clock==="real-time"?"pause-when-off":"real-time";draft=value}
        else if(fieldIndex===6)controller.previewLocal(draft,mediaKind)
        else if(fieldIndex===7){if(editRevision)controller.updateLocal(draft,editRevision);else controller.createLocal(draft)}
        else if(fieldIndex===8){navigationError="";mode="schedule";menuIndex=0}
        else {mode="discard";menuIndex=0}
    }
    function editSchedule(delta){
        if(menuIndex===3){mode="create";fieldIndex=8;return}
        editField=["horizonDays","retentionDays","timezone"][menuIndex]
        const current=draft[editField]===undefined?(menuIndex===0?7:2):draft[editField]
        if(delta && menuIndex<2){const value=Number(current)+delta;const minimum=menuIndex===0?1:0,maximum=menuIndex===0?31:7;if(value<minimum || value>maximum){navigationError="Choose "+minimum+"–"+maximum+" days.";return}let copy=JSON.parse(JSON.stringify(draft));copy[editField]=value;draft=copy;navigationError=""}
        else{keyboard.value=String(current);keyboard.selectedIndex=0;keyboardReturn="schedule";mode="keyboard"}
    }
    function invokeMenu(){
        if(controller.busy && !controller.backgroundBusy)return
        if(mode==="schedule")editSchedule(0)
        else if(mode==="browse"){
            navigationError=""
            if(menuIndex===0){editField="query";keyboard.value=channelQuery;keyboard.selectedIndex=0;keyboardReturn="browse";mode="keyboard"}
            else if(menuIndex===1){favoritesOnly=!favoritesOnly;applyFilter()}
            else if(menuIndex===2){const sources=["all","local","pvr"];sourceFilter=sources[(sources.indexOf(sourceFilter)+1)%sources.length];applyFilter()}
            else if(menuIndex===3){editField="jump";const d=new Date(anchorUtc*1000);keyboard.value=d.getFullYear()+"-"+String(d.getMonth()+1).padStart(2,"0")+"-"+String(d.getDate()).padStart(2,"0")+" "+String(d.getHours()).padStart(2,"0")+":"+String(d.getMinutes()).padStart(2,"0");keyboard.selectedIndex=0;keyboardReturn="browse";mode="keyboard"}
            else if(menuIndex===4 || menuIndex===5){jumpTo(anchorUtc+(menuIndex===4?-86400:86400));mode=""}
            else if(menuIndex===6){channelQuery="";sourceFilter="all";groupFilter="";favoritesOnly=false;applyFilter()}
            else if(menuIndex===7){mode="groupFilter";menuIndex=0}
            else mode=""
        }else if(mode==="groupFilter"){
            if(menuIndex<=groupNames.length){groupFilter=menuIndex===0?"":groupNames[menuIndex-1];applyFilter()}
            mode="browse";menuIndex=7
        }else if(mode==="add"){
            if(menuIndex===0){editRevision=0;draft=controller.newDefinition();mediaKind="episode";fieldIndex=0;mode="create"}
            else if(menuIndex===1){mode="sources";menuIndex=0}else mode=""
        }else if(mode==="sources"){
            if(menuIndex<options.sources.length){const source=options.sources[menuIndex];if(source.kind==="kodi-pvr"){if(controller.previewPvr()){mode="sourcePreview";menuIndex=0}}else dispatcher.dispatch(source.action,originId)}
            else mode=""
        }else if(mode==="sourcePreview"){
            if(!controller.sourcePreviewReady){controller.cancel();mode="sources";menuIndex=0}
            else if(menuIndex<controller.sourcePreview.length){editField="sourceNumber";keyboard.value=String(controller.sourcePreview[menuIndex].definition.number);keyboard.selectedIndex=0;keyboardReturn="sourcePreview";mode="keyboard"}
            else if(menuIndex===controller.sourcePreview.length)sourceSaving=controller.importPvr()
            else{controller.cancel();mode="sources";menuIndex=0}
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
            else if(menuIndex===6){mode="delete";menuIndex=0}
            else if(menuIndex===7 && c.owner!=="kodi-pvr"){editRevision=c.revision;draft=controller.editableDefinition(c.id);mediaKind=kindFor(draft);fieldIndex=3;mode="create"}
            else if(menuIndex===8 && c.owner!=="kodi-pvr"){if(controller.previewRegeneration(c.id)){mode="regenerate";menuIndex=0}}
            else if(menuIndex===9 && c.owner!=="kodi-pvr")controller.previewDuplicate(c.id,c.revision)
            else if(menuIndex===(c.owner==="kodi-pvr"?7:10) || menuIndex===(c.owner==="kodi-pvr"?8:11)){editField=menuIndex===(c.owner==="kodi-pvr"?7:10)?"description":"groups";keyboard.value=editField==="groups"?(c.groups || []).join(", "):(c.description || "");keyboard.selectedIndex=0;keyboardReturn="metadataReview";mode="keyboard"}
            else if(menuIndex===12 && c.owner!=="kodi-pvr"){
                navigationError=""
                if(!selectedProgram || selectedProgram.channelId!==managedId){navigationError="Select a program on this channel in the guide, then open Menu to repair it.";return}
                if(controller.previewRepair(managedId,selectedProgram.id,c.revision)){mode="repair";menuIndex=0}
            }
            else{mode="manage";menuIndex=0}
        }else if(mode==="repair"){
            const choice=repairItems[menuIndex];if(!choice)return
            if(choice.action==="choose" || choice.action==="unavailable"){repairChoice=choice.action==="unavailable"?-1:choice.choice;mode="repairConfirm";menuIndex=0}
            else if(choice.action==="previous" || choice.action==="next"){const review=controller.repairReview;if(controller.previewRepair(review.channelId,review.slotId,review.revision,review.offset+(choice.action==="next"?50:-50)))menuIndex=0}
            else{mode="context";menuIndex=12}
        }else if(mode==="repairConfirm"){
            if(menuIndex===1)repairSaving=controller.repair(repairChoice)
            else{mode="repair";menuIndex=0}
        }else if(mode==="regenerate"){
            if(menuIndex===1 && controller.regenerationReview.slotCount)regenerationSaving=controller.regenerate()
            else{mode="context";menuIndex=8}
        }else if(mode==="delete"){
            if(menuIndex===1 && managedChannel)controller.remove(managedChannel.id,managedChannel.revision)
            mode="manage";menuIndex=0
        }else if(mode==="discard"){
            if(menuIndex===0)mode="create";else{controller.cancel();mode=""}
        }else if(mode==="metadataReview"){
            if(menuIndex===0 && managedChannel){
                const conflict=editField==="number"?controller.channels.find(c=>c.id!==managedId && c.number===metadataDraft.number):null
                if(conflict){numberConflictId=conflict.id;let revisions={};revisions[managedId]=managedChannel.revision;revisions[conflict.id]=conflict.revision;numberSwapRevisions=revisions;numberSwapReview={firstName:managedChannel.name,firstNumber:managedChannel.number,secondName:conflict.name,secondNumber:conflict.number};mode="numberConflict";menuIndex=0}
                else metadataSaving=controller.setMetadata(managedChannel.id,metadataDraft,managedChannel.revision)
            }else mode="context"
        }else if(mode==="numberConflict"){
            if(menuIndex===0){keyboard.value=String(metadataDraft.number);keyboard.selectedIndex=0;keyboardReturn="metadataReview";mode="keyboard"}
            else if(menuIndex===1 && managedChannel && conflictingChannel)metadataSaving=controller.swapNumbers(managedId,numberConflictId,numberSwapRevisions)
            else{mode="context";menuIndex=5}
        }else if(mode==="details"){
            if(menuIndex===1 && selectedProgram && selectedProgram.displayStartUtc<=nowUtc && selectedProgram.displayEndUtc>nowUtc)watch()
            else if(menuIndex===2)mode=""
        }
    }
    function kindFor(value){const kinds=value.mediaKinds || [];return kinds.length===1?kinds[0]:kinds.length===2 && kinds.indexOf("movie")>=0 && kinds.indexOf("episode")>=0?"mixed":kinds.length===4?"all":"custom"}
    function previewMove(delta){
        let ids=moveIds.slice();const at=ids.indexOf(managedId),to=Math.max(0,Math.min(ids.length-1,at+delta))
        if(at>=0){ids.splice(at,1);ids.splice(to,0,managedId);moveIds=ids;menuIndex=to}
    }
    function chooseMenu(index){if(mode==="move")previewMove(index-moveIds.indexOf(managedId));else if(mode==="create"){fieldIndex=index;invokeCreate(1)}else{menuIndex=index;invokeMenu()}}
    function watch(){
        nowUtc=Date.now()/1000
        if(selectedChannel && (selectedProgram?selectedProgram.displayStartUtc<=nowUtc && selectedProgram.displayEndUtc>nowUtc:Math.abs(anchorUtc-nowUtc)<60))dispatcher.dispatch({type:"tune-channel",channelId:selectedChannel.id},originId)
        mode=""
    }
    function jumpTo(value){const now=Date.now()/1000;if(!Number.isFinite(value) || value<now-7*86400 || value>now+30*86400){navigationError="Choose a time within the previous 7 or next 30 days.";return false}anchorUtc=value;fromUtc=Math.floor(value/1800)*1800;loadWindow();return true}
    function acceptNavigation(text){
        if(editField==="query"){if(text.length>160){navigationError="Use at most 160 characters.";return}channelQuery=text.trim();applyFilter()}
        else{const match=/^(\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2})$/.exec(text);if(!match){navigationError="Enter local time as YYYY-MM-DD HH:MM.";return}const parts=match.slice(1).map(Number);const d=new Date(parts[0],parts[1]-1,parts[2],parts[3],parts[4]);if(d.getFullYear()!==parts[0] || d.getMonth()!==parts[1]-1 || d.getDate()!==parts[2] || d.getHours()!==parts[3] || d.getMinutes()!==parts[4]){navigationError="That local date or time does not exist.";return}if(!jumpTo(d.getTime()/1000))return}
        navigationError=""
    }
    function handleCommand(command){
        if(mode==="keyboard"){keyboard.handleCommand(command);return}
        if(mode==="rules"){rules.handleCommand(command);return}
        if(command===inputManager.back){
            if(controller.busy && !controller.backgroundBusy){controller.cancel();return}
            if(mode==="move"){moveIds=[];mode="context";menuIndex=2}
            else if(mode==="repairConfirm"){mode="repair";menuIndex=0}
            else if(mode==="repair"){mode="context";menuIndex=12}
            else if(mode==="schedule"){mode="create";fieldIndex=8}
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
            else if(mode==="schedule" && menuIndex<2 && (command===inputManager.navigateLeft || command===inputManager.navigateRight))editSchedule(command===inputManager.navigateLeft?-1:1)
            else if(command===inputManager.confirm)invokeMenu()
            return
        }
        if(command===inputManager.menu){if(headerFocused){toggleSurface();return}if(selectedChannel){managedId=selectedChannel.id;mode="context"}else mode="add";menuIndex=0;return}
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
        function onDuplicatePrepared(value){root.editRevision=0;root.draft=value;root.mediaKind=root.kindFor(value);root.fieldIndex=0;root.mode="create"}
        function onChannelCreated(id){root.mode="";root.headerFocused=false;root.nowUtc=Date.now()/1000;root.anchorUtc=root.nowUtc;root.fromUtc=Math.floor(root.nowUtc/1800)*1800;root.pickChannel(root.channels.findIndex(c=>c.id===id));root.loadWindow()}
        function onChanged(){
            if(root.repairSaving && !root.controller.busy){root.repairSaving=false;if(!root.controller.errorString){root.mode="context";root.menuIndex=12}}
            if(root.regenerationSaving && !root.controller.busy){root.regenerationSaving=false;if(!root.controller.errorString){root.mode="context";root.menuIndex=8}}
            if(root.sourceSaving && !root.controller.busy){root.sourceSaving=false;if(!root.controller.errorString){root.mode="";root.loadWindow()}}
            if(root.metadataSaving && !root.controller.busy){root.metadataSaving=false;if(!root.controller.errorString)root.mode="manage"}
            if(root.moveSaving && !root.controller.busy){root.moveSaving=false;if(!root.controller.errorString){root.moveIds=[];root.mode="context";root.menuIndex=2}}
            if(root.selectedChannelId){const index=root.channels.findIndex(c=>c.id===root.selectedChannelId);if(index>=0)root.selectedIndex=index;else if(root.selectedIndex>=root.channels.length)root.selectedIndex=Math.max(0,root.channels.length-1)}
            if(root.applyPendingFocus())root.loadWindow()
        }
    }
    Timer {interval:30000;repeat:true;running:root.open;onTriggered:{root.nowUtc=Date.now()/1000;if(!root.controller.busy)root.loadWindow()}}
    Rectangle {anchors.fill:parent;radius:root.inOverlay?root.radius:0;color:root.appearance.backgroundStart || root.appearance.surfaceColor;opacity:root.presentation.surfaceOpacity===undefined?1:root.presentation.surfaceOpacity}
    Column {
        anchors.fill:parent;anchors.margins:root.gap*2;spacing:root.gap
        Text {width:parent.width;textFormat:Text.PlainText;text:"Live TV · "+root.timeLabel(root.nowUtc)+(root.headerFocused?" · Menu: "+(root.overlay?"Full guide":"Overlay"):"");font.pixelSize:root.fontSize*1.4;color:root.appearance.textColor;elide:Text.ElideRight}
        Row {width:parent.width;height:root.fontSize*2.2;spacing:root.gap
            Repeater {model:root.options.toolbar
                delegate:Rectangle {required property var modelData;required property int index;width:(parent.width-root.gap*(root.options.toolbar.length-1))/root.options.toolbar.length;height:parent.height;radius:root.radius;color:root.appearance.surfaceColor;border.width:!root.mode && root.headerFocused && root.headerIndex===index?2:0;border.color:root.appearance.focusColor
                    objectName:"guideHeader-"+modelData.id
                    Accessible.role:Accessible.Button;Accessible.name:modelData.label;Accessible.focusable:true;Accessible.focused:root.visible && !root.mode && root.headerFocused && root.headerIndex===index
                    Accessible.onPressAction:{root.headerIndex=index;root.headerFocused=true;root.invokeHeader()}
                    Text {anchors.fill:parent;anchors.margins:root.gap;textFormat:Text.PlainText;text:modelData.label;color:root.appearance.textColor;font.pixelSize:root.fontSize;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter}
                    MouseArea {anchors.fill:parent;onClicked:{root.headerIndex=index;root.headerFocused=true;root.invokeHeader()}}
                }
            }
        }
        Text {width:parent.width;textFormat:Text.PlainText;text:root.selectedProgram?(root.selectedProgram.title+" · "+root.timeLabel(root.selectedProgram.displayStartUtc)+"–"+root.timeLabel(root.selectedProgram.displayEndUtc)):"Select a channel · Add creates local channels or connects sources";color:root.appearance.textColor;font.pixelSize:root.fontSize;elide:Text.ElideRight}
        Item {width:parent.width;height:root.fontSize*1.4
            Repeater {model:4;delegate:Text {required property int index;x:root.channelWidth+(parent.width-root.channelWidth)*index/4;text:root.timeLabel(root.fromUtc+root.minutes*60*index/4);color:root.appearance.mutedColor;font.pixelSize:root.fontSize*.8}}
        }
        ListView {id:timeline;width:parent.width;height:Math.max(root.fontSize*3,parent.height-root.fontSize*12);clip:true;model:root.visibleRows;currentIndex:root.visibleRows.findIndex(c=>root.selectedChannel && c.id===root.selectedChannel.id);highlightMoveDuration:0;highlightRangeMode:ListView.ApplyRange
            delegate:Item {required property var modelData;required property int index;width:timeline.width;height:timeline.height/root.rowCount
                objectName:"guideChannel-"+modelData.id
                Accessible.role:Accessible.Button;Accessible.name:modelData.number+" · "+modelData.name;Accessible.description:modelData.sourceName+" · "+(modelData.current.title || "No current listings");Accessible.focusable:true;Accessible.focused:root.visible && !root.mode && !root.headerFocused && !root.selectedProgram && root.selectedChannel && root.selectedChannel.id===modelData.id
                Accessible.onPressAction:{root.headerFocused=false;root.pickChannel(root.channels.findIndex(c=>c.id===modelData.id));root.handleCommand(inputManager.confirm)}
                Rectangle {anchors.fill:parent;anchors.bottomMargin:root.gap;color:"transparent";radius:root.radius;border.width:!root.headerFocused && !root.mode && root.selectedChannel && root.selectedChannel.id===modelData.id?2:0;border.color:root.appearance.focusColor}
                Text {x:root.gap;y:root.gap;width:root.channelWidth-root.gap*2;height:parent.height-root.gap*2;textFormat:Text.PlainText;text:modelData.number+" · "+modelData.name+(root.sourceBadges?"\n"+modelData.sourceName:"");color:root.appearance.textColor;font.pixelSize:root.fontSize;elide:Text.ElideRight;wrapMode:Text.WordWrap}
                Item {x:root.channelWidth;width:parent.width-root.channelWidth;height:parent.height;clip:true
                    property string stationName:modelData.name
                    property string sourceName:modelData.sourceName
                    Text {anchors.fill:parent;anchors.margins:root.gap;visible:!modelData.programs.length;textFormat:Text.PlainText;text:"No listings in this time window";color:root.appearance.mutedColor;font.pixelSize:root.fontSize}
                    Repeater {model:modelData.programs
                        delegate:Rectangle {required property var modelData;x:(modelData.displayStartUtc-root.fromUtc)/(root.minutes*60)*parent.width;width:Math.max(1,(modelData.displayEndUtc-modelData.displayStartUtc)/(root.minutes*60)*parent.width-root.gap/2);height:parent.height-root.gap;radius:root.radius;color:root.appearance.surfaceColor;border.color:root.appearance.focusColor;border.width:!root.headerFocused && !root.mode && root.selectedProgram && root.selectedProgram.id===modelData.id?2:0
                            objectName:"guideProgram-"+modelData.id
                            Accessible.role:Accessible.Button;Accessible.name:modelData.title+" · "+root.timeLabel(modelData.displayStartUtc)+"–"+root.timeLabel(modelData.displayEndUtc);Accessible.description:parent.stationName+" · "+parent.sourceName+(modelData.status!=="ready"?" · Unavailable":"");Accessible.focusable:true;Accessible.focused:root.visible && !root.mode && !root.headerFocused && root.selectedProgram && root.selectedProgram.id===modelData.id
                            Accessible.onPressAction:{root.headerFocused=false;root.pickChannel(root.channels.findIndex(c=>c.id===modelData.channelId));root.anchorUtc=modelData.displayStartUtc;root.handleCommand(inputManager.confirm)}
                            Text {anchors.fill:parent;anchors.margins:root.gap;visible:root.labels;textFormat:Text.PlainText;text:modelData.title+(modelData.status!=="ready"?" · Unavailable":"");color:root.appearance.textColor;font.pixelSize:root.fontSize;elide:Text.ElideRight;wrapMode:Text.WordWrap}
                            MouseArea {anchors.fill:parent;onClicked:{root.headerFocused=false;root.pickChannel(root.channels.findIndex(c=>c.id===modelData.channelId));root.anchorUtc=modelData.displayStartUtc;root.handleCommand(inputManager.confirm)}}
                        }
                    }
                    Rectangle {x:(root.nowUtc-root.fromUtc)/(root.minutes*60)*parent.width;visible:x>=0 && x<parent.width;width:2;height:parent.height;color:root.appearance.focusColor}
                }
            }
            Text {anchors.centerIn:parent;width:parent.width;textFormat:Text.PlainText;horizontalAlignment:Text.AlignHCenter;wrapMode:Text.WordWrap;visible:!root.channels.length;text:root.controller.ready?(root.channelQuery || root.favoritesOnly || root.sourceFilter!=="all"?"No channels match. Use Find / time to clear filters.":"No visible channels. Add a local channel or use Manage to restore hidden channels."):root.controller.errorString || "Opening saved guide…";color:root.appearance.textColor;font.pixelSize:root.fontSize}
        }
        Text {width:parent.width;textFormat:Text.PlainText;text:root.controller.errorString || root.controller.busy?root.controller.errorString || "Working · Back cancels safely":(root.selectedRow && root.selectedRow.listingsTruncated?"Listing limit reached; use a shorter time window. · ":"")+root.controller.status+" · D-pad browse · Menu options · Back returns";font.pixelSize:root.fontSize*.8;color:root.appearance.mutedColor;elide:Text.ElideRight}
    }
    Rectangle {anchors.fill:parent;anchors.margins:root.gap;visible:!!root.mode;color:root.appearance.surfaceColor;radius:root.radius
        Column {anchors.fill:parent;anchors.margins:root.gap*2;spacing:root.gap;visible:root.mode!=="keyboard" && root.mode!=="rules"
            Text {width:parent.width;textFormat:Text.PlainText;text:root.panelTitle();wrapMode:Text.WordWrap;color:root.appearance.textColor;font.pixelSize:root.fontSize*1.2}
            Text {width:parent.width;visible:root.mode==="regenerate" || root.mode==="numberConflict";textFormat:Text.PlainText;wrapMode:Text.WordWrap;color:root.appearance.textColor;font.pixelSize:root.fontSize
                text:root.mode==="regenerate"?(root.controller.regenerationReview.slotCount?"Replace "+root.controller.regenerationReview.slotCount+" future slots starting "+new Date(root.controller.regenerationReview.displayFromUtc*1000).toLocaleString()+". The airing program and earlier slots stay unchanged.":"Reviewing the replacement boundary…"):root.numberSwapReview.firstName+": "+root.numberSwapReview.firstNumber+" → "+root.numberSwapReview.secondNumber+"\n"+root.numberSwapReview.secondName+": "+root.numberSwapReview.secondNumber+" → "+root.numberSwapReview.firstNumber+"\nConfirm swaps both numbers together. Back changes neither."
            }
            Text {width:parent.width;visible:root.mode==="repair" || root.mode==="repairConfirm";textFormat:Text.PlainText;wrapMode:Text.WordWrap;color:root.appearance.textColor;font.pixelSize:root.fontSize;text:visible?root.repairDescription():""}
            ListView {id:menu;width:parent.width;height:Math.max(1,parent.height-y-feedback.height-root.gap);clip:true;model:root.mode==="create"?root.createFields:root.menuRows;currentIndex:root.mode==="create"?root.fieldIndex:root.menuIndex;highlightRangeMode:ListView.ApplyRange;highlightMoveDuration:0
                delegate:Rectangle {required property int index;required property string modelData;width:menu.width;height:root.fontSize*2.3;radius:root.radius;color:root.appearance.surfaceColor;border.width:menu.currentIndex===index?2:0;border.color:root.appearance.focusColor
                    objectName:"guideMenu-"+index
                    Accessible.role:Accessible.Button;Accessible.name:modelData;Accessible.focusable:true;Accessible.focused:root.visible && !!root.mode && menu.currentIndex===index
                    Accessible.onPressAction:root.chooseMenu(index)
                    Text {anchors.fill:parent;anchors.margins:root.gap;textFormat:Text.PlainText;text:modelData;color:root.appearance.textColor;font.pixelSize:root.fontSize;elide:Text.ElideRight}
                    MouseArea {anchors.fill:parent;onClicked:root.chooseMenu(index)}
                }
            }
            Text {id:feedback;width:parent.width;textFormat:Text.PlainText;wrapMode:Text.WordWrap;maximumLineCount:3;elide:Text.ElideRight;text:root.navigationError || root.controller.errorString || (root.controller.busy?"Working · Back cancels safely":root.mode==="schedule"?"Left/Right adjusts days; Confirm enters a value. Use a named timezone, such as America/Los_Angeles. Save keeps committed future slots; regeneration is separate.":root.mode==="sources" && root.menuIndex<root.options.sources.length?root.options.sources[root.menuIndex].description:root.controller.status+(root.mode==="create" && root.controller.previewReady?"\n"+(root.previewCurrent?"Current preview: ":"Settings changed — preview again: ")+root.controller.previewItems.map(i=>i.title).join(" · "):""));color:root.appearance.mutedColor;font.pixelSize:root.fontSize*.85}
        }
    }
    GuideRuleEditor {id:rules;anchors.fill:parent;anchors.margins:root.gap*2;visible:root.mode==="rules";appearance:root.appearance;options:root.options
        onCanceled:root.mode="create"
        onAccepted:rule=>{let value=JSON.parse(JSON.stringify(root.draft));value.rule=rule;root.draft=value;root.mode="create"}
    }
    OnScreenKeyboard {id:keyboard;anchors.centerIn:parent;width:Math.min(parent.width-root.gap*4,850);height:Math.min(parent.height,440);visible:root.mode==="keyboard";appearance:root.appearance
        onCanceled:root.mode=root.keyboardReturn==="create"?"create":root.keyboardReturn==="sourcePreview"?"sourcePreview":root.keyboardReturn==="browse"?"browse":root.keyboardReturn==="schedule"?"schedule":"context"
        onAccepted:text=>{
            const value=root.editField==="number" || root.editField==="sourceNumber"?Number(text):text
            if(root.keyboardReturn==="schedule"){
                let copy=JSON.parse(JSON.stringify(root.draft))
                if(root.editField!=="timezone"){const number=Number(text),minimum=root.editField==="horizonDays"?1:0,maximum=root.editField==="horizonDays"?31:7;if(!/^\d{1,2}$/.test(text.trim()) || !Number.isInteger(number) || number<minimum || number>maximum){root.navigationError="Choose a whole number from "+minimum+" to "+maximum+" days.";root.mode="schedule";return}copy[root.editField]=number}
                else copy.timezone=text.trim()
                root.draft=copy;root.navigationError="";root.mode="schedule"
            }
            else if(root.keyboardReturn==="browse"){root.acceptNavigation(text);root.mode="browse"}
            else if(root.keyboardReturn==="create"){let copy=JSON.parse(JSON.stringify(root.draft));copy[root.editField]=value;root.draft=copy;root.mode="create"}
            else if(root.keyboardReturn==="sourcePreview"){root.controller.setSourceNumber(root.menuIndex,value);root.mode="sourcePreview"}
            else{let copy={};copy[root.editField]=root.editField==="groups"?(text.trim()?text.split(",").map(s=>s.trim()):[]):value;root.metadataDraft=copy;root.mode="metadataReview";root.menuIndex=0}
        }
    }
}
