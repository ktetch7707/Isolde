import QtQuick

Item {
    id:root
    objectName:"guideView"
    required property var controller
    required property var dispatcher
    required property var options
    required property var appearance
    property var colorSettings:({})
    property var stationStyle:({})
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
    property string ruleReturn:"create"
    property string policyRuleKey:""
    property int blockIndex:0
    property var blockDraft:({})
    property string packagePath:""
    property var feedDraft:({instance:"",playlist:"",xmltv:"",url:"",name:""})
    property int sourceMappingIndex:0
    property int remapItem:0
    property int remapPage:0
    property bool feedSource:false
    property string sourceManagedId:""
    readonly property var importedSources:controller.channels.filter(c=>c.owner).reduce((rows,c)=>{const id=c.source.instance;if(!rows.some(s=>s.id===id))rows.push({id:id,enabled:c.sourceEnabled!==false,refreshedUtc:c.refreshedUtc,available:c.sourceAvailable});return rows},[])
    property bool packageSchedule:false
    property var metadataDraft:({})
    property string numberConflictId:""
    property var numberSwapRevisions:({})
    property var numberSwapReview:({})
    property bool metadataSaving:false
    property var moveIds:[]
    property var moveRevisions:({})
    property bool moveSaving:false
    property bool sourceSaving:false
    property int sourceOriginIndex:0
    property int editRevision:0
    property bool regenerationSaving:false
    property bool repairSaving:false
    property int repairChoice:-1
    property int clockDeltaSeconds:0
    property bool clockSaving:false
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
    readonly property var createFields:["Name: "+(draft.name || ""),"Channel number: "+(draft.number || ""),"Media: "+mediaKind,"Edit matching rules","Sequence: "+(draft.sequence || ""),"Clock: "+(draft.clock || "")+(draft.clockOffsetSeconds?" · Offset "+draft.clockOffsetSeconds/60+" minutes":""),"Preview matches",previewCurrent?(editRevision?"Save rules for unscheduled future slots":"Save channel"):"Preview these settings before saving","Schedule horizon, history and timezone","Cancel"].concat(controller.importReview.length?["Review imported media mappings"]:[]).concat(["Choose saved rule template…","Save draft as rule template…"])
    readonly property var menuRows:buildMenu()
    readonly property var repairItems:buildRepairItems()
    visible:open && !suppressed
    signal closed(string origin)
    onControllerChanged:{open=false;mode="";draft={};blockDraft={};stationStyle={};feedDraft={instance:"",playlist:"",xmltv:"",url:"",name:""};packagePath="";packageSchedule=false;metadataDraft={};managedId="";sourceManagedId="";selectedChannelId="";pendingFocusChannelId="";originId="";selectedIndex=0;menuIndex=0;fieldIndex=0;editRevision=0;channelQuery="";sourceFilter="all";groupFilter="";favoritesOnly=false;moveIds=[];moveRevisions={};sourceSaving=false;metadataSaving=false;clockSaving=false;repairSaving=false;regenerationSaving=false;moveSaving=false;navigationError=""}
    function show(origin,style){showFocused(origin,style,"")}
    function showFocused(origin,style,focusChannelId){
        controller.beginGuidePaint()
        originId=origin || "";presentation=style || {};open=true;mode="";headerFocused=true;headerIndex=0
        nowUtc=Date.now()/1000;anchorUtc=nowUtc;fromUtc=Math.floor(nowUtc/1800)*1800;navigationError=""
        pendingFocusChannelId=focusChannelId || "";applyPendingFocus()
        if(controller.clockWarning){mode="deviceClock";menuIndex=0}
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
    function matchesChannel(c){return !c.hidden && (!favoritesOnly || c.favorite) && (!groupFilter || (c.groups || []).indexOf(groupFilter)>=0) && (sourceFilter==="all" || (sourceFilter==="pvr")===(!!c.owner)) && (!channelQuery || (String(c.number)+" "+c.name).toLocaleLowerCase().indexOf(channelQuery.toLocaleLowerCase())>=0)}
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
        if(mode==="deviceClock")return ["Keep browsing cached listings","Use the reviewed current device clock for tuning"]
        if(mode==="add")return ["Create local channel","Add source","Back"]
        if(mode==="schedule")return ["Future horizon: "+(draft.horizonDays===undefined?7:draft.horizonDays)+" days","Retained history: "+(draft.retentionDays===undefined?2:draft.retentionDays)+" days","Timezone: "+draft.timezone,"Back to channel draft","Calendar blocks, cooldown, filler and bumpers"]
        if(mode==="policy")return ["Repetition cooldown: "+((draft.schedule || {}).cooldownSeconds || 0)/60+" minutes","Filler rules"+((draft.schedule || {}).fillerRule?" · Set":" · Off"),"Bumper rules"+((draft.schedule || {}).bumperRule?" · Set":" · Off"),"Remove filler rules","Remove bumper rules"].concat(((draft.schedule || {}).blocks || []).map(b=>"Edit block: "+b.name)).concat(["Add calendar block","Back to schedule"])
        if(mode==="block")return ["Name: "+blockDraft.name,"Weekdays: "+blockDraft.days.map(d=>["Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"][d-1]).join(", "),"Start time: "+civilTime(blockDraft.startMinute),"End time: "+civilTime(blockDraft.endMinute),"First date: "+(blockDraft.fromDate || "Any"),"Last date: "+(blockDraft.toDate || "Any"),"Edit block matching rules","Use block in draft","Remove block","Cancel block changes"]
        if(mode==="blockDays")return ["Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"].map((name,index)=>(blockDraft.days.indexOf(index+1)>=0?"✓ ":"○ ")+name).concat(["Back to block"])
        if(mode==="clock")return ["Earlier by 15 minutes","Later by 15 minutes","Enter adjustment: "+clockDeltaSeconds/60+" minutes","Return to wall-clock position","Review adjustment: "+clockDeltaSeconds/60+" minutes","Cancel"]
        if(mode==="clockReview")return ["Cancel",controller.clockReview.channelId?"Apply reviewed clock adjustment":"No valid clock adjustment"]
        if(mode==="browse")return ["Find channel: "+(channelQuery || "All"),"Favorites: "+(favoritesOnly?"Only favorites":"All channels"),"Source: "+sourceFilter,"Jump to local date and time…","Previous day","Next day","Clear filters","Group: "+(groupFilter || "All groups"),"Back"]
        if(mode==="groupFilter")return ["All groups"].concat(groupNames).concat(["Back"])
        if(mode==="sources")return options.sources.map(s=>s.name+(s.kind?" · Preview channel import":s.action.type==="settings"?" · Setup destination":" · App destination")).concat(["Manage imported sources","Back"])
        if(mode==="sourceManage")return importedSources.map(s=>s.id+" · "+(s.enabled?"Enabled":"OFF")+" · "+(s.available?"Cached source available":"Source missing")+" · "+new Date(s.refreshedUtc*1000).toLocaleString()).concat(["Back"])
        if(mode==="sourceEnable")return ["Cancel", "Enable source · Tuning connects only on request", "Disable source · Keep cached listings; refuse tuning"]
        if(mode==="diagnostics"){const d=controller.guideDiagnostics;return ["Cached channels: "+(d.channelCount || 0),"Cached programs: "+(d.slotCount || 0),"Unavailable programs: "+(d.unavailableCount || 0),"Last guide first paint: "+(controller.firstPaintMs<0?"Not measured":controller.firstPaintMs+" ms"),controller.scheduleStatus || "No schedule maintenance result this session"].concat((d.unavailable || []).map(i=>i.channelId+" · "+i.title+" · "+new Date(i.startUtc*1000).toLocaleString())).concat(["Back"])}
        if(mode==="feed")return ["Source ID: "+(feedDraft.instance || "Choose a stable unique name"),"M3U file: "+(feedDraft.playlist || "Absolute local path"),"XMLTV file: "+(feedDraft.xmltv || "Optional · no listings"),"Direct stream URL: "+(feedDraft.url || "Optional · replaces M3U file"),"Direct station name: "+(feedDraft.name || "Required with direct URL"),"Preview complete source · No changes until Import","Cancel"]
        if(mode==="sourceMapping")return ["Station number: "+controller.sourcePreview[sourceMappingIndex].definition.number,"XMLTV channel ID: "+(controller.sourcePreview[sourceMappingIndex].definition.source.epgId || "No listings"),"Back to lineup review"]
        if(mode==="sourcePreview")return controller.sourcePreviewReady?controller.sourcePreview.map(e=>"Source "+e.definition.source.number+" → "+e.definition.number+" · "+e.definition.name+" · "+e.programs.length+" listings").concat(["Import reviewed lineup","Cancel"]):[controller.busy?"Reading channels and listings…":"Retry source preview","Cancel"]
        if(mode==="manage")return (controller?controller.channels:[]).map(c=>c.number+" · "+c.name+(c.hidden?" · Hidden":"")+(c.favorite?" · Favorite":"")).concat(["Import local channel package","Guide diagnostics / missing programs","Back"])
        if(mode==="import" || mode==="export")return ["Package file: "+(packagePath || "Absolute .json package or .xsp playlist path"),(mode==="export"?"Include":"Restore")+" cached schedule snapshot: "+(packageSchedule?"Yes":"No"),packagePath.toLowerCase().endsWith(".xsp")?(mode==="export"?"Export matching rules only · Use JSON to preserve the complete channel":"Import matching rules into a new channel draft"):(mode==="export"?"Save new package · Existing files are preserved":"Open as unsaved channel · Preview current Kodi matches before Save"),"Cancel"]
        if(mode==="move")return moveIds.map(id=>{const c=controller.channels.find(c=>c.id===id);return c?c.number+" · "+c.name:"Removed channel"})
        if(mode==="context"){const base=[managedChannel && managedChannel.favorite?"Remove favorite":"Favorite",managedChannel && managedChannel.hidden?"Show channel":"Hide channel","Move up","Move down","Rename","Channel number","Delete channel…"];const local=managedChannel && !managedChannel.owner;return base.concat(local?["Edit rules and sequencing","Regenerate future schedule…","Duplicate local channel"]:[]).concat(["Description","Channel groups"]).concat(local?["Repair selected program…","Adjust broadcast clock…","Export local channel…"]:[]).concat(["Station logo…","Station appearance…","Remove station logo","Back"])}
        if(mode==="stationStyle")return ["Station color…","Text color…","Art fit: "+(stationStyle.artFit || "Inherit"),"Text size: "+(stationStyle.labelSize===undefined?"Inherit":stationStyle.labelSize),"Opacity: "+(stationStyle.surfaceOpacity===undefined?"Inherit":stationStyle.surfaceOpacity),"Corners: "+(stationStyle.radius===undefined?"Inherit":stationStyle.radius),"Spacing: "+(stationStyle.cardGap===undefined?"Inherit":stationStyle.cardGap),"Focus width: "+(stationStyle.focusWidth===undefined?"Inherit":stationStyle.focusWidth),"Label position: "+(stationStyle.labelPosition || "Inherit"),"Save appearance","Reset draft to inherited appearance","Cancel"]
        if(mode==="remap")return controller.importReview.slice(remapPage,remapPage+50).map(i=>i.title+" · "+i.mappingLabel).concat(["Previous page","Next page","Back to draft"])
        if(mode==="templates")return controller.ruleTemplates.map(t=>t.name).concat(["Back to unsaved draft"])
        if(mode==="remapChoices")return controller.importChoices(remapItem,remapPage).map(i=>i.title+" · "+i.duration+" seconds"+(i.fits?"":" · Duration differs")).concat(["Previous page","Next page","Cancel replacement"])
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
        if(id==="now"){if(controller.clockWarning){mode="deviceClock";menuIndex=0;return}nowUtc=Date.now()/1000;anchorUtc=nowUtc;fromUtc=Math.floor(nowUtc/1800)*1800;loadWindow()}
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
        return choices.concat([{label:"Mark only this slot unavailable…",action:"unavailable"},{label:review.fitInterval?"Show matching-duration replacements":"Include shorter replacements with an unavailable remainder…",action:"fit"},{label:"Back",action:"back"}])
    }
    function repairDescription(){
        const review=controller.repairReview;if(!review.program)return "Reading the selected program…"
        let text=review.program.title+" · "+new Date(review.program.displayStartUtc*1000).toLocaleString()+" – "+timeLabel(review.program.displayEndUtc)+"\n"
        if(mode==="repairConfirm")text+=(repairChoice<0?"Mark this slot unavailable.":"Replace with "+review.alternatives[repairChoice].title+".")+" Other programs keep their IDs and times. A shorter replacement keeps this ID, then shows unavailable time until the original end."
        else text+=review.total+" replacements · "+(review.fitInterval?"Shorter programs leave an unavailable remainder; longer programs are excluded.":review.incompatibleCount+" different-duration items excluded.")+" Nothing is saved until confirmation."
        return text
    }
    function panelTitle(){
        if(mode==="clock")return "Adjust broadcast clock · Unsaved choice"
        if(mode==="clockReview")return "Review clock position · Saved programs stay unchanged"
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
        else if(fieldIndex===3){ruleReturn="create";rules.begin(draft.rule);mode="rules"}
        else if(fieldIndex===4){const values=["seeded-shuffle","episode-order","rotation","next-unwatched"];value.sequence=values[(values.indexOf(value.sequence)+delta+values.length)%values.length];draft=value}
        else if(fieldIndex===5){value.clock=value.clock==="real-time"?"pause-when-off":"real-time";if(value.clock==="pause-when-off")delete value.clockOffsetSeconds;draft=value}
        else if(fieldIndex===6)controller.previewLocal(draft,mediaKind)
        else if(fieldIndex===7){if(editRevision)controller.updateLocal(draft,editRevision);else controller.createLocal(draft)}
        else if(fieldIndex===8){navigationError="";mode="schedule";menuIndex=0}
        else if(fieldIndex===10 && controller.importReview.length){remapPage=0;mode="remap";menuIndex=0}
        else if(fieldIndex===(controller.importReview.length?11:10)){controller.refreshRuleTemplates();mode="templates";menuIndex=0}
        else if(fieldIndex===(controller.importReview.length?12:11)){keyboard.value=draft.name || "";keyboard.selectedIndex=0;keyboardReturn="templateName";mode="keyboard"}
        else {mode="discard";menuIndex=0}
    }
    function editSchedule(delta){
        if(menuIndex===4){mode="policy";menuIndex=0;return}
        if(menuIndex===3){mode="create";fieldIndex=8;return}
        editField=["horizonDays","retentionDays","timezone"][menuIndex]
        const current=draft[editField]===undefined?(menuIndex===0?7:2):draft[editField]
        if(delta && menuIndex<2){const value=Number(current)+delta;const minimum=menuIndex===0?1:0,maximum=menuIndex===0?31:7;if(value<minimum || value>maximum){navigationError="Choose "+minimum+"–"+maximum+" days.";return}let copy=JSON.parse(JSON.stringify(draft));copy[editField]=value;draft=copy;navigationError=""}
        else{keyboard.value=String(current);keyboard.selectedIndex=0;keyboardReturn="schedule";mode="keyboard"}
    }
    function policyCopy(){let value=JSON.parse(JSON.stringify(draft));value.schedule=value.schedule || {};return value}
    function policyKeyboard(key,value,where){editField=key;keyboard.value=String(value);keyboard.selectedIndex=0;keyboardReturn=where;mode="keyboard"}
    function editPolicy(){
        let value=policyCopy(),p=value.schedule,blocks=p.blocks || []
        if(menuIndex===0){policyKeyboard("cooldownSeconds",(p.cooldownSeconds || 0)/60,"policy");return}
        if(menuIndex===1 || menuIndex===2){policyRuleKey=menuIndex===1?"fillerRule":"bumperRule";ruleReturn="policy";rules.begin(p[policyRuleKey] || draft.rule);mode="rules";return}
        if(menuIndex===3 || menuIndex===4){delete p[menuIndex===3?"fillerRule":"bumperRule"];draft=value;return}
        if(menuIndex===6+blocks.length){mode="schedule";menuIndex=4;return}
        blockIndex=menuIndex-5
        if(blockIndex===blocks.length && blocks.length>=32){navigationError="Choose at most 32 calendar blocks.";return}
        blockDraft=JSON.parse(JSON.stringify(blocks[blockIndex] || {name:"New block",days:[1,2,3,4,5,6,7],startMinute:0,endMinute:1440,rule:draft.rule}))
        mode="block";menuIndex=0
    }
    function editBlock(){
        const keys=["name","days","startMinute","endMinute","fromDate","toDate"]
        if(menuIndex===1){mode="blockDays";menuIndex=0;return}
        if(menuIndex<6){const key=keys[menuIndex];policyKeyboard(key,key==="startMinute" || key==="endMinute"?civilTime(blockDraft[key]):(blockDraft[key]===undefined?"":blockDraft[key]),"block");return}
        if(menuIndex===6){ruleReturn="block";rules.begin(blockDraft.rule);mode="rules";return}
        if(menuIndex===7 || menuIndex===8){let value=policyCopy();value.schedule.blocks=value.schedule.blocks || [];if(menuIndex===7)value.schedule.blocks[blockIndex]=JSON.parse(JSON.stringify(blockDraft));else if(blockIndex<value.schedule.blocks.length)value.schedule.blocks.splice(blockIndex,1);draft=value}
        mode="policy";menuIndex=0
    }
    function civilTime(minute){return String(Math.floor(minute/60)).padStart(2,"0")+":"+String(minute%60).padStart(2,"0")}
    function stationContrast(){
        if(!appearance || !appearance.textColor || !appearance.surfaceColor)return ""
        function luminance(value){const c=Qt.darker(value,1);function linear(v){return v<=.04045?v/12.92:Math.pow((v+.055)/1.055,2.4)}return .2126*linear(c.r)+.7152*linear(c.g)+.0722*linear(c.b)}
        const a=luminance(editField==="labelColor"?stationColor.sample:stationStyle.labelColor || appearance.textColor),b=luminance(editField==="surfaceColor"?stationColor.sample:stationStyle.surfaceColor || appearance.surfaceColor),ratio=(Math.max(a,b)+.05)/(Math.min(a,b)+.05)
        return (ratio>=4.5?"Text contrast ":"Low text contrast · ")+ratio.toFixed(1)+":1 · solid-surface estimate"
    }
    function acceptPolicyText(text){
        navigationError=""
        if(keyboardReturn==="policy"){
            const minutes=Number(text);if(!/^\d{1,5}$/.test(text.trim()) || !Number.isInteger(minutes) || minutes<0 || minutes>10080){navigationError="Choose 0–10080 whole minutes.";return}
            let value=policyCopy();value.schedule.cooldownSeconds=minutes*60;draft=value;return
        }
        let block=JSON.parse(JSON.stringify(blockDraft))
        if(editField==="days"){
            const parts=text.split(",").map(s=>Number(s.trim()));if(!/^[1-7](\s*,\s*[1-7])*$/.test(text.trim()) || new Set(parts).size!==parts.length){navigationError="Enter unique weekdays 1–7 separated by commas.";return}block.days=parts
        }else if(editField==="startMinute" || editField==="endMinute"){
            const parts=text.trim().match(/^(\d{2}):(\d{2})$/),maximum=editField==="endMinute"?1440:1439;const n=parts?Number(parts[1])*60+Number(parts[2]):-1;if(!parts || Number(parts[2])>59 || n<0 || n>maximum || (editField==="endMinute" && n===0)){navigationError="Enter HH:MM in 24-hour time; 24:00 is allowed only for an end time.";return}block[editField]=n
        }else if(editField==="fromDate" || editField==="toDate"){
            if(text.trim() && !/^\d{4}-\d{2}-\d{2}$/.test(text.trim())){navigationError="Enter YYYY-MM-DD or leave empty for any date.";return}if(text.trim())block[editField]=text.trim();else delete block[editField]
        }else block[editField]=text.trim()
        blockDraft=block
    }
    function setClockDelta(value){
        if(!Number.isInteger(value) || Math.abs(value)>31*86400){navigationError="Choose an adjustment within 31 days.";return false}
        clockDeltaSeconds=value;navigationError="";return true
    }
    function clockDescription(){
        const review=controller.clockReview
        if(mode==="clockReview" && review.channelId)return "Adjust "+review.deltaSeconds/60+" minutes. Saved program IDs and times stay unchanged.\nBroadcast position (UTC): "+new Date(review.fromClockUtc*1000).toISOString()+" → "+new Date(review.toClockUtc*1000).toISOString()+"\n"+review.title+(review.status!=="ready"?" · Unavailable":"")
        const c=managedChannel
        return "Current policy: "+(c && c.clock==="real-time"?"Real time · Offset "+(c.clockOffsetSeconds || 0)/60+" minutes":"Pause when off")+".\nChoose earlier/later or enter signed minutes, then review before applying. The target must be inside the cached schedule."
    }
    function invokeMenu(){
        if(mode==="blockDays"){if(menuIndex<7){let copy=JSON.parse(JSON.stringify(blockDraft)),day=menuIndex+1,index=copy.days.indexOf(day);if(index<0)copy.days.push(day);else copy.days.splice(index,1);copy.days.sort((a,b)=>a-b);blockDraft=copy}else{mode="block";menuIndex=1}return}
        if(mode==="deviceClock"){if(menuIndex===1)controller.acceptDeviceClock();mode="";return}
        if(controller.busy && !controller.backgroundBusy)return
        if(mode==="import" || mode==="export"){
            if(menuIndex===0)policyKeyboard("packagePath",packagePath,mode)
            else if(menuIndex===1)packageSchedule=!packageSchedule
            else if(menuIndex===2){if(mode==="export")controller.exportChannel(managedId,packagePath,packageSchedule);else controller.previewImport(packagePath,packageSchedule)}
            else{mode="manage";menuIndex=0}
        }else if(mode==="clock"){
            if(menuIndex<2)setClockDelta(clockDeltaSeconds+(menuIndex===0?-900:900))
            else if(menuIndex===2){editField="clockMinutes";keyboard.value=String(clockDeltaSeconds/60);keyboard.selectedIndex=0;keyboardReturn="clock";mode="keyboard"}
            else if(menuIndex===3 && managedChannel){const wall=Math.floor(Date.now()/1000),c=managedChannel;const position=c.clock==="real-time"?wall+(c.clockOffsetSeconds || 0):c.clockUtc+(c.active?Math.max(0,wall-c.anchorUtc):0);setClockDelta(wall-position)}
            else if(menuIndex===4 && managedChannel){if(controller.previewClockAdjustment(managedId,clockDeltaSeconds,managedChannel.revision)){mode="clockReview";menuIndex=0}}
            else{controller.cancel();mode="context";menuIndex=13}
        }else if(mode==="clockReview"){
            if(menuIndex===1 && controller.clockReview.channelId)clockSaving=controller.adjustClock()
            else{controller.cancel();mode="clock";menuIndex=4}
        }else if(mode==="policy")editPolicy()
        else if(mode==="block")editBlock()
        else if(mode==="schedule")editSchedule(0)
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
            if(menuIndex<options.sources.length){const source=options.sources[menuIndex];sourceOriginIndex=menuIndex;feedSource=source.kind==="guide-feed";if(feedSource){mode="feed";menuIndex=0}else if(source.kind==="kodi-pvr"){if(controller.previewPvr()){mode="sourcePreview";menuIndex=0}}else if(dispatcher.dispatch(source.action,originId) && source.action.type==="settings")close()}
            else if(menuIndex===options.sources.length){mode="sourceManage";menuIndex=0}
            else mode=""
        }else if(mode==="feed"){
            if(menuIndex<5){editField=["instance","playlist","xmltv","url","name"][menuIndex];keyboard.value=feedDraft[editField];keyboard.selectedIndex=0;keyboardReturn="feed";mode="keyboard"}
            else if(menuIndex===5){if(controller.previewFeed(feedDraft.instance,feedDraft.playlist,feedDraft.xmltv,feedDraft.url,feedDraft.name)){mode="sourcePreview";menuIndex=0}}
            else{mode="sources";menuIndex=sourceOriginIndex}
        }else if(mode==="sourceMapping"){
            if(menuIndex<2){editField=menuIndex===0?"sourceNumber":"sourceEpg";const entry=controller.sourcePreview[sourceMappingIndex];keyboard.value=String(menuIndex===0?entry.definition.number:entry.definition.source.epgId);keyboard.selectedIndex=0;keyboardReturn="sourceMapping";mode="keyboard"}
            else{mode="sourcePreview";menuIndex=sourceMappingIndex}
        }else if(mode==="sourcePreview"){
            if(!controller.sourcePreviewReady){if(menuIndex===0 && !controller.busy){if(feedSource){mode="feed";menuIndex=5}else controller.previewPvr()}else{controller.cancel();mode="sources";menuIndex=sourceOriginIndex}}
            else if(menuIndex<controller.sourcePreview.length){if(feedSource){sourceMappingIndex=menuIndex;mode="sourceMapping";menuIndex=0}else{editField="sourceNumber";keyboard.value=String(controller.sourcePreview[menuIndex].definition.number);keyboard.selectedIndex=0;keyboardReturn="sourcePreview";mode="keyboard"}}
            else if(menuIndex===controller.sourcePreview.length)sourceSaving=controller.importPvr()
            else{controller.cancel();mode="sources";menuIndex=sourceOriginIndex}
        }else if(mode==="sourceManage"){
            if(menuIndex<importedSources.length){sourceManagedId=importedSources[menuIndex].id;mode="sourceEnable";menuIndex=0}else{mode="sources";menuIndex=options.sources.length}
        }else if(mode==="sourceEnable"){
            if(menuIndex>0)controller.setSourceEnabled(sourceManagedId,menuIndex===1);mode="sourceManage";menuIndex=0
        }else if(mode==="diagnostics"){
            mode="manage";menuIndex=controller.channels.length+1
        }else if(mode==="manage"){
            if(menuIndex<controller.channels.length){managedId=controller.channels[menuIndex].id;mode="context";menuIndex=0}
            else if(menuIndex===controller.channels.length){packagePath="";packageSchedule=false;mode="import";menuIndex=0}
            else if(menuIndex===controller.channels.length+1){controller.readDiagnostics();mode="diagnostics";menuIndex=0}
            else mode=""
        }else if(mode==="context"){
            const c=managedChannel;if(!c){mode="manage";menuIndex=0;return}
            const extra=c.owner?9:15
            if(menuIndex===extra){editField="stationLogo";keyboard.value="";keyboard.selectedIndex=0;keyboardReturn="stationLogo";mode="keyboard";return}
            if(menuIndex===extra+1){stationStyle=Object.assign({},c.presentation || {});mode="stationStyle";menuIndex=0;return}
            if(menuIndex===extra+2){metadataSaving=controller.setStationLogo(c.id,"",c.revision);return}
            if(menuIndex===0)controller.setPreferences(c.id,{favorite:!c.favorite},c.revision)
            else if(menuIndex===1)controller.setPreferences(c.id,{hidden:!c.hidden},c.revision)
            else if(menuIndex===2 || menuIndex===3){
                const direction=menuIndex===2?-1:1
                moveIds=controller.channels.map(x=>x.id);let revisions={};for(const item of controller.channels)revisions[item.id]=item.revision
                moveRevisions=revisions;mode="move";previewMove(direction)
            }else if(menuIndex===4 || menuIndex===5){editField=menuIndex===4?"name":"number";keyboard.value=String(c[editField]);keyboard.selectedIndex=0;keyboardReturn="metadataReview";mode="keyboard"}
            else if(menuIndex===6){mode="delete";menuIndex=0}
            else if(menuIndex===7 && !c.owner){editRevision=c.revision;draft=controller.editableDefinition(c.id);mediaKind=kindFor(draft);fieldIndex=3;mode="create"}
            else if(menuIndex===8 && !c.owner){if(controller.previewRegeneration(c.id)){mode="regenerate";menuIndex=0}}
            else if(menuIndex===9 && !c.owner)controller.previewDuplicate(c.id,c.revision)
            else if(menuIndex===13 && !c.owner){clockDeltaSeconds=0;navigationError="";mode="clock";menuIndex=0}
            else if(menuIndex===14 && !c.owner){packagePath="";packageSchedule=false;mode="export";menuIndex=0}
            else if(menuIndex===(!!c.owner?7:10) || menuIndex===(!!c.owner?8:11)){editField=menuIndex===(!!c.owner?7:10)?"description":"groups";keyboard.value=editField==="groups"?(c.groups || []).join(", "):(c.description || "");keyboard.selectedIndex=0;keyboardReturn="metadataReview";mode="keyboard"}
            else if(menuIndex===12 && !c.owner){
                navigationError=""
                if(!selectedProgram || selectedProgram.channelId!==managedId){navigationError="Select a program on this channel in the guide, then open Menu to repair it.";return}
                if(controller.previewRepair(managedId,selectedProgram.id,c.revision)){mode="repair";menuIndex=0}
            }
            else{mode="manage";menuIndex=0}
        }else if(mode==="templates"){
            if(menuIndex<controller.ruleTemplates.length)controller.useRuleTemplate(menuIndex)
            else mode="create"
        }else if(mode==="remap" || mode==="remapChoices"){
            const choices=mode==="remapChoices"?controller.importChoices(remapItem,remapPage):controller.importReview.slice(remapPage,remapPage+50)
            if(menuIndex<choices.length){if(mode==="remap"){remapItem=remapPage+menuIndex;remapPage=0;mode="remapChoices";menuIndex=0}else if(controller.mapImportedItem(remapItem,choices[menuIndex].choice)){remapPage=Math.floor(remapItem/50)*50;mode="remap";menuIndex=remapItem-remapPage}}
            else if(menuIndex===choices.length){remapPage=Math.max(0,remapPage-50);menuIndex=0}
            else if(menuIndex===choices.length+1){const total=mode==="remap"?controller.importReview.length:controller.previewCount;if(remapPage+50<total){remapPage+=50;menuIndex=0}}
            else if(mode==="remapChoices"){remapPage=Math.floor(remapItem/50)*50;mode="remap";menuIndex=remapItem-remapPage}else{mode="create";fieldIndex=10}
        }else if(mode==="stationStyle"){
            let copy=Object.assign({},stationStyle)
            if(menuIndex<2){editField=menuIndex===0?"surfaceColor":"labelColor";stationColor.begin(copy[editField] || (menuIndex===0?appearance.surfaceColor:appearance.textColor));mode="stationColor"}
            else if(menuIndex===9)metadataSaving=controller.setMetadata(managedId,{presentation:stationStyle},managedChannel.revision)
            else if(menuIndex===10)stationStyle={}
            else if(menuIndex===11){mode="context";menuIndex=managedChannel.owner?10:16}
            else{const keys=["artFit","labelSize","surfaceOpacity","radius","cardGap","focusWidth","labelPosition"],values=[["contain","cover"],[16,20,24,28,32,40],[0,0.25,0.5,0.75,1],[0,4,8,16,24,40],[0,4,8,12,16,24],[1,2,3,4,6,8],["inside","hidden"]];const key=keys[menuIndex-2],choices=values[menuIndex-2];copy[key]=choices[(choices.indexOf(copy[key])+1)%choices.length];stationStyle=copy}
        }else if(mode==="repair"){
            const choice=repairItems[menuIndex];if(!choice)return
            if(choice.action==="choose" || choice.action==="unavailable"){repairChoice=choice.action==="unavailable"?-1:choice.choice;mode="repairConfirm";menuIndex=0}
            else if(choice.action==="previous" || choice.action==="next"){const review=controller.repairReview;if(controller.previewRepair(review.channelId,review.slotId,review.revision,review.offset+(choice.action==="next"?50:-50),!!review.fitInterval))menuIndex=0}
            else if(choice.action==="fit"){const review=controller.repairReview;if(controller.previewRepair(review.channelId,review.slotId,review.revision,0,!review.fitInterval))menuIndex=0}
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
        if(mode==="stationColor"){stationColor.handleCommand(command);return}
        if(mode==="keyboard"){keyboard.handleCommand(command);return}
        if(mode==="rules"){rules.handleCommand(command);return}
        if(command===inputManager.back){
            if(controller.busy && !controller.backgroundBusy){controller.cancel();return}
            if(mode==="move"){moveIds=[];mode="context";menuIndex=2}
            else if(mode==="clockReview"){controller.cancel();mode="clock";menuIndex=4}
            else if(mode==="clock"){controller.cancel();mode="context";menuIndex=13}
            else if(mode==="sourcePreview"){controller.cancel();mode="sources";menuIndex=sourceOriginIndex}
            else if(mode==="feed"){mode="sources";menuIndex=sourceOriginIndex}
            else if(mode==="sourceMapping"){mode="sourcePreview";menuIndex=sourceMappingIndex}
            else if(mode==="sourceEnable"){mode="sourceManage";menuIndex=0}
            else if(mode==="sourceManage"){mode="sources";menuIndex=options.sources.length}
            else if(mode==="diagnostics"){mode="manage";menuIndex=controller.channels.length+1}
            else if(mode==="stationStyle"){mode="context";menuIndex=managedChannel.owner?10:16}
            else if(mode==="remapChoices"){remapPage=Math.floor(remapItem/50)*50;mode="remap";menuIndex=remapItem-remapPage}
            else if(mode==="remap"){mode="create";fieldIndex=10}
            else if(mode==="templates"){mode="create";fieldIndex=controller.importReview.length?11:10}
            else if(mode==="repairConfirm"){mode="repair";menuIndex=0}
            else if(mode==="repair"){mode="context";menuIndex=12}
            else if(mode==="block"){mode="policy";menuIndex=0}
            else if(mode==="blockDays"){mode="block";menuIndex=1}
            else if(mode==="policy"){mode="schedule";menuIndex=4}
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
        function onChannelCreated(id){root.mode="";root.nowUtc=Date.now()/1000;root.anchorUtc=root.nowUtc;root.fromUtc=Math.floor(root.nowUtc/1800)*1800;root.pendingFocusChannelId=id;root.applyPendingFocus();root.loadWindow()}
        function onChanged(){
            if(root.clockSaving && !root.controller.busy){root.clockSaving=false;if(!root.controller.errorString){root.mode="context";root.menuIndex=13;root.loadWindow()}}
            if(root.repairSaving && !root.controller.busy){root.repairSaving=false;if(!root.controller.errorString){root.mode="context";root.menuIndex=12}}
            if(root.regenerationSaving && !root.controller.busy){root.regenerationSaving=false;if(!root.controller.errorString){root.mode="context";root.menuIndex=8}}
            if(root.sourceSaving && !root.controller.busy){root.sourceSaving=false;if(!root.controller.errorString){root.mode="";root.loadWindow()}}
            if(root.metadataSaving && !root.controller.busy){root.metadataSaving=false;if(!root.controller.errorString){root.mode="manage";root.menuIndex=Math.max(0,root.controller.channels.findIndex(c=>c.id===root.managedId))}}
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
                Rectangle {z:1;anchors.fill:parent;anchors.bottomMargin:root.gap;color:"transparent";radius:root.radius;border.width:!root.headerFocused && !root.mode && root.selectedChannel && root.selectedChannel.id===modelData.id?((modelData.presentation || {}).focusWidth || 2):0;border.color:(modelData.presentation || {}).focusColor || root.appearance.focusColor}
                Rectangle {x:0;y:0;width:root.channelWidth;height:parent.height-root.gap;color:(modelData.presentation || {}).surfaceColor || "transparent";opacity:(modelData.presentation || {}).surfaceOpacity===undefined?1:modelData.presentation.surfaceOpacity;radius:(modelData.presentation || {}).radius===undefined?root.radius:modelData.presentation.radius}
                Column {x:(modelData.presentation || {}).cardGap===undefined?root.gap:modelData.presentation.cardGap;y:x;width:root.channelWidth-x*2
                    Image {width:parent.width;height:modelData.logo?Math.min(root.fontSize*1.5,parent.parent.height/3):0;source:modelData.logo?root.controller.stationLogo(modelData.logo):"";fillMode:(modelData.presentation || {}).artFit==="cover"?Image.PreserveAspectCrop:Image.PreserveAspectFit;asynchronous:true}
                    Text {width:parent.width;textFormat:Text.PlainText;text:modelData.number+" · "+modelData.name;color:(modelData.presentation || {}).labelColor || root.appearance.textColor;font.pixelSize:(modelData.presentation || {}).labelSize || root.fontSize;elide:Text.ElideRight;visible:(modelData.presentation || {}).labelPosition!=="hidden"}
                    Text {objectName:"guideSource-"+modelData.id;width:parent.width;visible:root.sourceBadges;textFormat:Text.PlainText;text:modelData.sourceName;color:root.appearance.textColor;font.pixelSize:root.fontSize*.85;elide:Text.ElideRight}
                }
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
            ListView {id:menu;objectName:"guideMenu";width:parent.width;height:Math.max(1,parent.height-y-feedback.height-root.gap);clip:true;model:root.mode==="create"?root.createFields:root.menuRows;highlightRangeMode:ListView.ApplyRange;highlightMoveDuration:0
                readonly property int selectedRowIndex:root.mode==="create"?root.fieldIndex:root.menuIndex
                function restoreSelection(){currentIndex=selectedRowIndex;positionViewAtIndex(selectedRowIndex,ListView.Contain)}
                onSelectedRowIndexChanged:Qt.callLater(restoreSelection)
                onModelChanged:Qt.callLater(restoreSelection)
                Component.onCompleted:Qt.callLater(restoreSelection)
                delegate:Rectangle {required property int index;required property string modelData;width:menu.width;height:root.fontSize*2.3;radius:root.radius;color:root.appearance.surfaceColor;border.width:menu.selectedRowIndex===index?2:0;border.color:root.appearance.focusColor
                    objectName:"guideMenu-"+index
                    Accessible.role:Accessible.Button;Accessible.name:modelData;Accessible.focusable:true;Accessible.focused:root.visible && !!root.mode && menu.selectedRowIndex===index
                    Accessible.onPressAction:root.chooseMenu(index)
                    Text {anchors.fill:parent;anchors.margins:root.gap;textFormat:Text.PlainText;text:modelData;color:root.appearance.textColor;font.pixelSize:root.fontSize;elide:Text.ElideRight}
                    MouseArea {anchors.fill:parent;onClicked:root.chooseMenu(index)}
                }
            }
            Text {id:feedback;width:parent.width;textFormat:Text.PlainText;wrapMode:Text.WordWrap;maximumLineCount:3;elide:Text.ElideRight;text:root.navigationError || root.controller.errorString || (root.mode==="deviceClock"?root.controller.clockWarning:"") || (root.controller.busy?"Working · Back cancels safely":root.mode==="clock" || root.mode==="clockReview"?root.clockDescription():root.mode==="schedule"?"Left/Right adjusts days; Confirm enters a value. Use a named timezone, such as America/Los_Angeles. Save keeps committed future slots; regeneration is separate.":root.mode==="sources" && root.menuIndex<root.options.sources.length?root.options.sources[root.menuIndex].description:root.controller.status+(root.mode==="create" && root.controller.previewReady?"\n"+(root.previewCurrent?"Current preview: ":"Settings changed — preview again: ")+root.controller.previewItems.map(i=>i.title).join(" · "):""));color:root.appearance.mutedColor;font.pixelSize:root.fontSize*.85}
        }
    }
    GuideRuleEditor {id:rules;anchors.fill:parent;anchors.margins:root.gap*2;visible:root.mode==="rules";appearance:root.appearance;options:root.options
        onCanceled:root.mode=root.ruleReturn
        onAccepted:rule=>{
            if(root.ruleReturn==="block"){let b=JSON.parse(JSON.stringify(root.blockDraft));b.rule=rule;root.blockDraft=b;root.mode="block";return}
            let value=JSON.parse(JSON.stringify(root.draft));if(root.ruleReturn==="policy"){value.schedule=value.schedule || {};value.schedule[root.policyRuleKey]=rule}else value.rule=rule;root.draft=value;root.mode=root.ruleReturn
        }
    }
    OnScreenKeyboard {id:keyboard;anchors.centerIn:parent;width:Math.min(parent.width-root.gap*4,850);height:Math.min(parent.height,440);visible:root.mode==="keyboard";appearance:root.appearance
        onCanceled:root.mode=(["feed","sourceMapping","policy","block","import","export"].indexOf(root.keyboardReturn)>=0)?root.keyboardReturn:root.keyboardReturn==="create"?"create":root.keyboardReturn==="sourcePreview"?"sourcePreview":root.keyboardReturn==="browse"?"browse":root.keyboardReturn==="schedule"?"schedule":root.keyboardReturn==="clock"?"clock":"context"
        onAccepted:text=>{
            const value=root.editField==="number" || root.editField==="sourceNumber"?Number(text):text
            if(root.keyboardReturn==="templateName"){
                root.controller.saveRuleTemplate(text.trim(),root.draft);root.mode="create"
            }else if(root.keyboardReturn==="stationLogo"){
                if(root.managedChannel)root.metadataSaving=root.controller.setStationLogo(root.managedId,text.trim(),root.managedChannel.revision);root.mode="context"
            }else if(root.keyboardReturn==="feed"){
                root.feedDraft=Object.assign({},root.feedDraft,{[root.editField]:text.trim()});root.mode="feed"
            }else if(root.keyboardReturn==="sourceMapping"){
                if(root.editField==="sourceNumber")root.controller.setSourceNumber(root.sourceMappingIndex,Number(text))
                else root.controller.setSourceEpg(root.sourceMappingIndex,text.trim())
                root.mode="sourceMapping"
            }else if(root.keyboardReturn==="import" || root.keyboardReturn==="export"){
                root.packagePath=text.trim();root.mode=root.keyboardReturn
            }else if(root.keyboardReturn==="policy" || root.keyboardReturn==="block"){
                root.acceptPolicyText(text);root.mode=root.keyboardReturn
            }else if(root.keyboardReturn==="clock"){
                if(!/^-?\d{1,5}$/.test(text.trim()))root.navigationError="Enter a whole number of signed minutes within 31 days."
                else root.setClockDelta(Number(text)*60)
                root.mode="clock"
            }else if(root.keyboardReturn==="schedule"){
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
    StudioColorPicker {id:stationColor;anchors.fill:parent;visible:root.mode==="stationColor";appearance:root.appearance;settings:root.colorSettings
        contrastText:root.stationContrast()
        themeColors:Object.keys(root.appearance).filter(k=>k.endsWith("Color")).map(k=>({name:k.replace("Color",""),value:root.appearance[k]}))
        onAccepted:(value,reset)=>{let copy=Object.assign({},root.stationStyle);if(reset)delete copy[root.editField];else copy[root.editField]=value;root.stationStyle=copy;root.mode="stationStyle"}
        onCanceled:root.mode="stationStyle"
    }
}
