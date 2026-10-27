// StudioInspector.qml — remote-operable property editor (D-pad friendly). Views: fields, choices, artwork, crop, layout, apps, states, colorWheel.
import QtQuick
import QtQuick.Controls

Item {
    id:root
    objectName:"studioInspector"
    required property var session
    required property var page
    required property var options
    property var provider:null
    readonly property var playlistClient:view==="playlistSource" && provider && provider.playlistCatalog ? provider.playlistCatalog() : null
    required property var appearance
    property string pageId:""
    property var selectedIds:[]
    property bool pageMode:false
    property bool editorMode:false
    property string structureType:""
    property string structureId:""
    property var selectionRect:null
    property string view:"tabs"
    property int tab:0
    property int selected:0
    property bool advanced:false
    property string editingState:"base"
    property bool controlsShown:false
    property bool hiddenControls:false
    property string message:"Choose a group of properties. Changes affect the draft; Apply saves them."
    property var activeField:({})
    property var choices:[]
    property var assets:[]
    property var appSelection:[]
    readonly property var appPicker:appPickerLoader.item
    property var layoutDraft:({})
    property var cropDraft:({})
    property var pendingAction:({})
    property string keyboardKind:""
    property var recentColors:[]
    property var pickerAppearance:({})
    property var pickerThemeColors:[]
    readonly property var colorPicker:colorLoader.item
    property bool colorTransactionActive:false
    property bool scalarTransactionActive:false
    readonly property bool previewActive:colorTransactionActive || scalarTransactionActive
    property real scalarDraft:0
    property real scalarOriginal:0
    property bool scalarPending:false
    property bool scalarResetChoice:false
    property int scalarControl:0
    property int scalarPreset:0
    property var scalarAppearance:({})
    readonly property real scalarUnit:Math.min(width/1280,height/720)*(scalarAppearance.typographyScale || 1)
    readonly property int scalarActionBase:(activeField.presets || []).length?2:1
    onSessionChanged:{recentColors=[];colorTransactionActive=false;scalarTransactionActive=false;scalarPending=false;if(colorPicker)colorPicker.previewQueued=false;if(colorLoader)colorLoader.active=false}
    onVisibleChanged:if(!visible){if(colorTransactionActive)cancelColor();if(scalarTransactionActive)cancelScalar()}
    property string returnView:"fields"
    property int returnSelection:0
    property int keyboardSelection:0
    readonly property var tabs:editorMode?["Colors"]:pageMode?["Colors","Background","Layout","More","Header"]:structureType==="navigation"?["Content","Actions","More"]:structureType==="section"?["Content","Appearance","Layout","Actions","More"]:["Content","Appearance","Layout","Actions","Accessibility","More"]
    readonly property var targets:!page?[]:structureType?((structureType==="section"?page.categories:page.composition.navigation) || []).filter(i=>i.id===structureId).map(i=>Object.assign({},i,{componentType:structureType})):(page.items || []).filter(i=>selectedIds.indexOf(i.id)>=0)
    readonly property var current:targets[0] || null
    readonly property var actions:session?session.availableActions:[]
    readonly property var allApps:actions.filter(a=>a.action && a.action.type==="launch-app" && (a.appType==="web" || a.appType==="desktop" || a.appType==="android"))
    readonly property var apps:allApps.filter(a=>!current || !current.appCollection || !current.appCollection.section || a.appSection===current.appCollection.section)
    readonly property var fields:((options.inspector || {})[editorMode?"studioFields":pageMode?"pageFields":"fields"] || []).filter(f=>f.tab===tabs[tab] && (advanced || !f.advanced) && (!f.needsNamedPage || !!pageId) && (!f.needsComposition || page && page.composition.enabled) && (structureType!=="navigation" || f.key==="name" || f.key==="action") && (editingState==="base" || tabs[tab]!=="Appearance" || f.scope==="presentation" && ["surfaceColor","surfaceOpacity","labelColor","iconColor","focusColor","borderColor","borderWidth","focusWidth","radius","labelSize","labelBold","labelSurfaceColor"].indexOf(f.key)>=0) && (editorMode || pageMode || targets.length && targets.every(i=>(!f.types || f.types.indexOf(i.componentType)>=0) && (!f.excludeTypes || f.excludeTypes.indexOf(i.componentType)<0) && (!f.families || f.families.indexOf(i.family)>=0) && (!f.excludeFamilies || f.excludeFamilies.indexOf(i.family)<0))))
    readonly property real unit:(appearance.typographyScale || 1)*Math.min(width/1280,height/720)
    readonly property real panelWidth:Math.min(width-48*unit,540*unit)
    readonly property real panelHeight:Math.min(height-48*unit,650*unit)
    readonly property bool roomRight:selectionRect && selectionRect.x+selectionRect.width+panelWidth+48*unit<=width
    readonly property bool roomLeft:selectionRect && selectionRect.x-panelWidth-48*unit>=0
    readonly property bool collapseNeeded:!pageMode && !!selectionRect && !roomRight && !roomLeft
    readonly property bool collapsed:(hiddenControls || collapseNeeded && !controlsShown) && view!=="keyboard"
    readonly property var rawPage:{const doc=session?session.document:{};return pageId?((doc.pages || []).find(p=>p.id===pageId) || {}).layout || {}:doc}
    signal closed()
    signal operation(string name)
    signal samplesChanged(bool enabled)
    function begin(asPage){editorMode=false;pageMode=asPage;structureType="";structureId="";view="tabs";tab=0;selected=0;advanced=false;editingState="base";controlsShown=false;hiddenControls=false;message="Choose a property group. Changes affect the draft; Apply saves them."}
    function local(item,f){
        if(editorMode)return ((session?session.document:{}).studioAppearance || {})[f.key]
        if(pageMode){if(f.scope==="theme")return (rawPage.theme || {})[f.key];if(f.scope==="background")return ((rawPage.composition || {}).background || {})[f.key];if(f.scope==="headerStyle")return ((rawPage.composition || {}).header || {})[f.key];return (rawPage.composition || {})[f.key]}
        if(f.scope==="presentation"){const p=item.presentation || {};return editingState==="base" || f.tab!=="Appearance"?p[f.key]:((p.states || {})[editingState] || {})[f.key]}
        if(f.scope==="appCollection")return (item.appCollection || {})[f.key]
        return f.scope==="accessibility"?(item.accessibility || {})[f.key]:item[f.key]
    }
    function effective(item,f){
        item=item || {}
        if(!page)return f.fallback!==undefined?f.fallback:""
        const value=local(item,f);if(value!==undefined)return value
        if(editorMode)return (options.appearance || {})[f.key]
        if(pageMode){if(f.scope==="theme")return page.appearance[f.key];if(f.scope==="background")return (options.backgroundDefaults || {})[f.key];if(f.scope==="headerStyle"){const defaults=(options.headerDefaults || {})[page.composition.headerMode==="identity"?"identity":"navigation"] || {};if(defaults[f.key]!==undefined)return defaults[f.key];if(f.key==="textColor")return page.appearance.textColor;if(f.key==="focusColor")return page.appearance.focusColor;if(f.key==="backgroundColor")return page.appearance.surfaceColor}}
        else if(f.scope==="presentation"){
            const p=item.presentation || {};if(editingState!=="base" && p[f.key]!==undefined)return p[f.key]
            const section=!item.parentId?(page.categories || []).find(s=>s.id===item.categoryId):null
            if(section && section.presentation && section.presentation[f.key]!==undefined)return section.presentation[f.key]
            if(f.key==="fillEndColor")return effective(item,{scope:"presentation",key:"surfaceColor",fallback:page.appearance.surfaceColor})
            const key=f.key==="labelColor"?"textColor":f.key==="iconColor"?"textColor":f.key
            if(page.appearance[key]!==undefined)return page.appearance[key]
        }
        if(f.fallback!==undefined)return f.fallback
        if(f.type==="toggle")return false
        return ""
    }
    function mixed(f){if(pageMode || targets.length<2)return false;const value=JSON.stringify(effective(targets[0],f));return targets.some(i=>JSON.stringify(effective(i,f))!==value)}
    function summary(f){
        if(mixed(f))return "Mixed"
        const value=effective(current,f)
        if(f.type==="visibility")return current && current.hidden?"Hidden":"Visible"
        if(f.type==="lock")return current && current.locked?"Locked":"Unlocked"
        if(f.type==="action"){if(!value || !value.type)return "None";const named=actions.find(a=>Object.keys(a.action).every(k=>a.action[k]===value[k]));return named?named.name:"Unavailable link · choose a replacement"}
        if(f.type==="artwork")return value?"Managed image":"None"
        if(["geometry","layout","children","apps","order","crop","grid","header","typography"].indexOf(f.type)>=0)return "Open…"
        if(f.type==="toggle" || f.type==="scrolling")return value?"On":"Off"
        if(f.type==="choice"){const named=(f.choices || []).find(entry=>entry && typeof entry==="object" && JSON.stringify(entry.value)===JSON.stringify(value));if(named)return named.name}
        if(f.type==="color")return local(current || {},f)===undefined?"Theme":"Custom"
        if(f.control==="slider")return Number((Number(value)*(f.displayScale || 1)).toFixed(2))+String(f.displayUnit || "")+(local(current || {},f)===undefined?" · Theme":"")
        const members=pageMode?[{}]:targets,overridden=members.filter(i=>local(i,f)!==undefined).length
        return String(value===undefined?"":value)+(overridden===0?" · Inherited":overridden===members.length?" · Override":" · Mixed overrides")
    }
    function run(op,args){
        const success=previewActive?session.previewCommand(op,args,pageId):session.execute(op,args,pageId)
        if(!success){message=session.errorString;if(colorTransactionActive)colorPicker.error=message;return false}
        message=previewActive?"":"Draft updated · nothing applied.";return true
    }
    function scalarText(value){return Number((value*(activeField.displayScale || 1)).toFixed(2))+String(activeField.displayUnit || "")}
    function beginScalar(){
        scalarOriginal=Number(effective(current,activeField));scalarDraft=scalarOriginal
        scalarAppearance=Object.assign({},appearance);scalarPending=false;scalarResetChoice=false;scalarControl=0;scalarPreset=0;scalarTransactionActive=true;view="scalar";message=""
    }
    function changeScalar(value,reset){
        if(!reset && (!Number.isFinite(value) || value<activeField.min || value>activeField.max)){message="Choose a value within the slider range.";return false}
        if(!reset)scalarDraft=value
        scalarResetChoice=reset;message=""
        if(!scalarPending){scalarPending=true;Qt.callLater(flushScalar)}
        return true
    }
    function flushScalar(){
        if(!scalarPending || !scalarTransactionActive)return
        scalarPending=false
        if(set(activeField,scalarDraft,scalarResetChoice) && scalarResetChoice)Qt.callLater(()=>{
            if(!scalarTransactionActive || !scalarResetChoice)return
            if(pageMode && activeField.scope==="theme")scalarDraft=Number(page.appearance[activeField.key])
            else if(pageMode && activeField.scope==="background")scalarDraft=Number((page.composition.background || {})[activeField.key]===undefined?(options.backgroundDefaults || {})[activeField.key]:(page.composition.background || {})[activeField.key])
            else scalarDraft=Number(effective(current,activeField))
        })
    }
    function cancelScalar(){scalarPending=false;if(session)session.cancelPreview();scalarTransactionActive=false;view="fields";selected=returnSelection;message=""}
    function acceptScalar(){flushScalar();if(message)return;if(session.proposalModel && !session.acceptPreview()){message=session.errorString;return}scalarTransactionActive=false;view="fields";selected=returnSelection;message="Draft updated · nothing applied."}
    function scalarCommand(command){
        if(command===inputManager.back){cancelScalar();return}
        const direction=command===inputManager.navigateRight?1:command===inputManager.navigateLeft?-1:0
        if(scalarControl===0 && direction){const next=Math.round((scalarDraft+direction*activeField.step)*10000)/10000;if(next>=activeField.min && next<=activeField.max)changeScalar(next,false);return}
        if(scalarControl===1 && scalarActionBase===2 && direction){scalarPreset=Math.max(0,Math.min(activeField.presets.length-1,scalarPreset+direction));return}
        if(direction && scalarControl>=scalarActionBase && scalarControl<=scalarActionBase+2){scalarControl=Math.max(scalarActionBase,Math.min(scalarActionBase+2,scalarControl+direction));return}
        if(command===inputManager.navigateDown)scalarControl=Math.min(scalarActionBase+3,scalarControl+1)
        else if(command===inputManager.navigateUp)scalarControl=Math.max(0,scalarControl-1)
        else if(command===inputManager.confirm){
            if(scalarControl===1 && scalarActionBase===2)changeScalar(activeField.presets[scalarPreset].value,false)
            else if(scalarControl===scalarActionBase)changeScalar(scalarDraft,true)
            else if(scalarControl===scalarActionBase+1)acceptScalar()
            else if(scalarControl===scalarActionBase+2)cancelScalar()
            else if(scalarControl===scalarActionBase+3)beginKeyboard("scalarNumber",scalarDraft*(activeField.displayScale || 1))
        }
    }
    function beginColor(){
        pickerAppearance=Object.assign({},appearance)
        pickerThemeColors=[{name:"Background",value:page.appearance.backgroundStart},{name:"Surface",value:page.appearance.surfaceColor},{name:"Text",value:page.appearance.textColor},{name:"Focus",value:page.appearance.focusColor},{name:"Selected",value:page.appearance.selectedColor},{name:"Muted",value:page.appearance.mutedColor}].filter(entry=>!!entry.value)
        colorTransactionActive=true;colorLoader.active=true;view="colorWheel";selected=0
        colorPicker.begin(effective(current,activeField) || page.appearance.textColor)
    }
    function cancelColor(){if(colorPicker)colorPicker.previewQueued=false;if(session)session.cancelPreview();colorTransactionActive=false;view="fields";selected=returnSelection;message="";colorLoader.active=false}
    function acceptColor(value,reset){
        if(colorPicker.error)return
        if(session.proposalModel && !session.acceptPreview()){colorPicker.error=session.errorString;return}
        if(!reset && value!==colorPicker.hex(colorPicker.original))recentColors=[{value:value}].concat(recentColors.filter(c=>c.value!==value)).slice(0,8).map((entry,index)=>({name:"Recent "+(index+1),value:entry.value}))
        colorTransactionActive=false;view="fields";selected=returnSelection;message="Draft updated · nothing applied.";colorLoader.active=false
    }
    function set(f,value,reset){
        if(editorMode){const appearance=Object.assign({},(session.document || {}).studioAppearance || {});if(reset)delete appearance[f.key];else appearance[f.key]=value;return run("studioAppearance",{value:appearance})}
        if(f.scope==="presentation" && f.key==="libraryView" && !pageMode && (value==="playlist" || (current.presentation || {}).libraryView==="playlist")){
            if(structureType==="section")return run("section",{kind:"presentation",id:structureId,value:Object.assign({},current.presentation || {},{libraryView:reset?"all":value,libraryValue:""})})
            return run("batch",{commands:targets.map(item=>({operation:"patch",arguments:{id:item.id,value:{presentation:{libraryView:reset?"all":value,libraryValue:""}}},pageId:pageId}))})
        }
        if(f.scope==="appCollection")return run("batch",{commands:targets.map(item=>{
            const collection=Object.assign({},item.appCollection || {mode:"all",ids:[]});
            if(reset || value===undefined || value===null || value==="")delete collection[f.key];else collection[f.key]=value;
            return {operation:"patch",arguments:{id:item.id,value:{appCollection:collection}},pageId:pageId}
        })})
        if(structureType==="navigation")return run("navigation",{kind:"patch",id:structureId,value:{[f.key]:reset?null:value}})
        if(structureType==="section"){
            if(f.key==="name")return run("section",{kind:"rename",id:structureId,name:value})
            if(f.scope!=="presentation"){message="Choose a supported section property.";return false}
            const style=Object.assign({},current.presentation || {})
            if(editingState!=="base" && f.tab==="Appearance"){
                const states=Object.assign({},style.states || {}),state=Object.assign({},states[editingState] || {});if(reset)delete state[f.key];else state[f.key]=value;if(Object.keys(state).length)states[editingState]=state;else delete states[editingState];if(Object.keys(states).length)style.states=states;else delete style.states
            }else if(reset || f.key==="trailingAction" && value && !value.type)delete style[f.key];else style[f.key]=value
            return run("section",{kind:"presentation",id:structureId,value:style})
        }
        if(pageMode){let scope=f.scope,key=f.key,change=value
            if(scope==="headerStyle"){scope="composition";key="header";change=Object.assign({},(rawPage.composition || {}).header || {});if(reset)delete change[f.key];else change[f.key]=value}
            if(scope==="background"){scope="composition";key="background";change=Object.assign({},options.backgroundDefaults || {},page.composition.background || {});if(reset)delete change[f.key];else change[f.key]=value}
            if(run("pagePatch",{scope:scope==="theme"?"theme":"composition",value:{[key]:reset && f.scope!=="headerStyle" && (f.scope!=="background" || f.key==="mode")?null:change}}))return true
            return false
        }
        let patch={}
        if(f.scope==="presentation" && editingState!=="base" && f.tab==="Appearance"){
            const commands=targets.map(item=>{const states=Object.assign({},(item.presentation || {}).states || {}),state=Object.assign({},states[editingState] || {});if(reset)delete state[f.key];else state[f.key]=value;if(Object.keys(state).length)states[editingState]=state;else delete states[editingState];return {operation:"patch",arguments:{id:item.id,value:{presentation:{states:Object.keys(states).length?states:null}}},pageId:pageId}})
            return run("batch",{commands:commands})
        }
        if(f.scope==="presentation" || f.scope==="accessibility")patch[f.scope]={[f.key]:reset?null:value};else patch[f.key]=reset?null:value
        return run("patch",{ids:selectedIds,value:patch})
    }
    function reset(f){if(["geometry","layout","apps","children","order","crop","grid","header"].indexOf(f.type)>=0){message="This operation has explicit controls; Back returns.";return}set(f,null,true)}
    function adjust(f,direction){
        if(f.type==="number"){const value=Number(effective(current,f)),number=Math.round((value+direction*f.step)*1000)/1000;if(!Number.isFinite(number) || number<f.min || number>f.max){message="Choose "+f.min+"–"+f.max+" for "+f.label+".";return}set(f,number,false)}
        else if(f.type==="toggle")set(f,!effective(current,f),false)
    }
    function openChoices(entries){choices=entries;view="choices";selected=0;if(!entries.length)message="No configured choices are available. Add the corresponding page, tab or source first. Back returns without changes."}
    function beginKeyboard(kind,value){keyboardKind=kind;returnView=view;keyboardSelection=selected;keyboard.value=String(value===undefined?"":value);keyboard.selectedIndex=0;view="keyboard"}
    function pickArtwork(source){const relative=session.stageArtwork(source);if(!relative){message=session.errorString;return}if(set(activeField,relative,false)){view="fields";selected=returnSelection;message="Artwork staged privately. Apply publishes it; cancel/discard leaves the applied profile unchanged."}}
    function activateField(f){
        if(targets.length>1 && ["geometry","layout","children","apps","order","grid","replaceComponent"].indexOf(f.type)>=0){message="Choose one component for this structural operation. Shared appearance and links can edit all selected items.";return}
        activeField=f;returnSelection=selected
        if(f.type==="replaceComponent"){openChoices((options.replacementChoices || []).map(entry=>({name:entry.name,value:entry.type,kind:"replacement"})));return}
        if(f.type==="number" && f.control==="slider"){beginScalar();return}
        if(f.type==="number" || f.type==="text"){beginKeyboard(f.type,effective(current,f));return}
        if(f.type==="toggle"){adjust(f,1);return}
        if(f.type==="choice"){openChoices(f.choices.map(value=>value && typeof value==="object"?value:({name:value,value:value})));return}
        if(f.type==="color"){beginColor();return}
        if(f.type==="action"){view="actionPicker";return}
        if(f.type==="typography"){openChoices((options.inspector.typePresets || []).map(p=>({name:p.name,value:p.value,kind:"typography"})));return}
        if(f.type==="artwork"){assets=session.artworkChoices();view="artwork";selected=0;return}
        if(f.type==="crop"){view="crop";selected=0;return}
        if(f.type==="repair"){openChoices(session.sourceChoices(current.id,pageId).map(c=>({name:c.name || c.label || c.id,value:c.id,kind:"repair"})));return}
        if(f.type==="section"){openChoices([{name:"Uncategorized",value:""}].concat((page.categories || []).map(c=>({name:c.name,value:c.id}))));return}
        if(f.type==="source"){
            if((current.presentation || {}).libraryView==="playlist"){view="playlistSource";selected=0;message="Choose a video playlist saved in Kodi. Refresh retries unavailable Kodi; Back keeps the draft unchanged.";return}
            const collections=actions.filter(a=>a.action.type==="open-collection");openChoices([{name:"Kodi playlists…",kind:"playlist"}].concat(collections.map(a=>({name:a.name,value:a.action.collectionId}))).concat([{name:"Enter search / genre / tag text…",kind:"text"}]));return
        }
        if(f.type==="header"){openChoices((options.headerChoices || []).map(entry=>({name:entry.name,value:entry.value})));return}
        if(f.type==="appGroup"){
            const groups=Array.from(new Set(allApps.map(a=>a.appSection).filter(s=>!!s && s.length<=80))).sort();
            const currentGroup=(current.appCollection || {}).section;if(currentGroup && groups.indexOf(currentGroup)<0)groups.unshift(currentGroup);
            openChoices([{name:"All app groups",value:""}].concat(groups.map(s=>({name:s,value:s}))));
            message="Filters this collection. Selected app membership is kept.";return
        }
        if(f.type==="tab"){openChoices((page.composition.navigation || []).map(t=>({name:t.name,value:t.id})));return}
        if(f.type==="layout"){operation("groupLayout");return}
        if(f.type==="apps"){const ids=new Set(selectedIds);appSelection=current.appCollection?(current.appCollection.ids || []).slice():(page.items || []).filter(i=>ids.has(i.parentId) && i.appId).map(i=>i.appId);view="apps";selected=0;message="Choosing apps replaces this bar's contents as one undoable change.";return}
        if(f.type==="visibility"){run("batch",{commands:targets.map(i=>({operation:"state",arguments:{id:i.id,key:"hidden",value:!current.hidden},pageId:pageId}))});return}
        if(f.type==="lock"){run("batch",{commands:targets.map(i=>({operation:"state",arguments:{id:i.id,key:"locked",value:!current.locked},pageId:pageId}))});return}
        operation(f.type)
    }
    function labels(){
        if(view==="tabs")return tabs
        if(view==="fields"){
            if(tabs[tab]==="More")return pageMode?["Navigation tabs","Sections","Movies destination","Shows destination","Close controls"]:structureType?["Move earlier","Move later","Remove…",...(structureType==="navigation"?["Visible sections…"]:[]),"Close controls"]:["Duplicate","Remove…","Move before…","Move after…","Move to section…","Move to group…","Select / deselect for grouping","Edit group contents","Show design samples for empty media","Close controls"]
            return fields.map(f=>f.label+": "+summary(f)).concat([advanced?"Hide advanced controls":"Show advanced controls",...(tabs[tab]==="Appearance"?["Appearance state: "+editingState]:[]),"Back to property groups"])
        }
        if(view==="choices")return choices.map(c=>c.name)
        if(view==="playlistSource")return ["Refresh Kodi playlists"].concat(playlistClient?(playlistClient.items || []).map(i=>i.title || i.playlistName):[])
if(view==="colorWheel")return ["Back"]
        if(view==="artwork")return ["Import local PNG / JPEG…","Clear image"].concat(assets.map(a=>a.name+" · "+a.width+" × "+a.height))
        if(view==="crop")return ["Fit: "+effective(current,{scope:pageMode?"background":"presentation",key:"artFit",fallback:"cover"}),"Focal X: "+effective(current,{scope:pageMode?"background":"presentation",key:"artFocalX",fallback:.5}),"Focal Y: "+effective(current,{scope:pageMode?"background":"presentation",key:"artFocalY",fallback:.5}),"Zoom: "+effective(current,{scope:pageMode?"background":"presentation",key:"artZoom",fallback:1}),"Pixel crop…","Use full artwork","Back"]
        if(view==="pixelCrop")return ["Left: "+cropDraft.x,"Top: "+cropDraft.y,"Width: "+cropDraft.width,"Height: "+cropDraft.height,"Use this crop","Cancel"]
        if(view==="layout")return ["Layout: "+layoutDraft.mode,"Columns: "+layoutDraft.columns,"Rows: "+layoutDraft.rows,"Gap: "+layoutDraft.gap,"Padding: "+layoutDraft.padding,"Visible columns: "+(layoutDraft.visibleColumns || layoutDraft.columns),"Wrap navigation: "+(layoutDraft.wrap?"On":"Off"),"Use this arrangement","Cancel"]
        if(view==="apps")return ["All installed apps","Use "+appSelection.length+" selected apps"].concat(apps.map(a=>(appSelection.indexOf(a.action.appId)>=0?"✓ ":"□ ")+a.name))
        if(view==="states")return ["base","normal","focused","selected","pressed","disabled","unavailable"]
        return []
    }
    function activate(){
        if(view==="playlistSource"){
            if(!playlistClient){message="Kodi unavailable. Back returns without changes.";return}
            if(selected===0){provider.refresh(playlistClient);message=playlistClient.status;return}
            const item=(playlistClient.items || [])[selected-1];if(!item)return
            const source={libraryView:"playlist",libraryValue:item.playlistName}
            const saved=structureType==="section"?run("section",{kind:"presentation",id:structureId,value:Object.assign({},current.presentation || {},source)}):run("batch",{commands:targets.map(target=>({operation:"patch",arguments:{id:target.id,value:{presentation:source}},pageId:pageId}))})
            if(saved){view="fields";selected=returnSelection}return
        }
        if(view==="tabs"){tab=selected;selected=0;view="fields";return}
        if(view==="fields"){
            if(tabs[tab]==="More"){
                const operations=pageMode?["navigation","sections","moviePage","showsPage","close"]:structureType?["structureEarlier","structureLater","structureRemove",...(structureType==="navigation"?["navSections"]:[]),"close"]:["duplicate","remove","orderBefore","orderAfter","destination","groupDestination","selectGroup","contents","samples","close"]
                if(operations[selected]==="close")closed();else if(operations[selected]==="samples"){samplesChanged(true);message="Design samples are editor-only and never saved as media or progress."}else operation(operations[selected]);return
            }
            if(selected===fields.length){advanced=!advanced;selected=0;return}
            if(tabs[tab]==="Appearance" && selected===fields.length+1){view="states";selected=0;return}
            if(selected>=fields.length){view="tabs";selected=tab;return}
            activateField(fields[selected]);return
        }
        if(view==="states"){editingState=labels()[selected];view="fields";selected=0;return}
if(view==="colorWheel"){view="fields";selected=returnSelection;return}
        if(view==="choices"){
            const entry=choices[selected];if(!entry)return
            if(entry.kind==="replacement"){operation("replace:"+entry.value);return}
            if(entry.kind==="playlist"){view="playlistSource";selected=0;message="Choose a video playlist saved in Kodi. Back keeps the source unchanged.";return}
            if(entry.needsMedia){media.libraryKind=entry.needsMedia;media.selected=0;view="mediaAction";return}
            if(entry.kind==="repair" || activeField.type==="section"){if(run(entry.kind==="repair"?"repairSource":"assignSection",Object.assign({id:current.id},entry.kind==="repair"?{sourceId:entry.value}:{categoryId:entry.value}))){view="fields";selected=returnSelection}return}
            if(entry.kind==="custom" || entry.kind==="text"){beginKeyboard("text","");return}
            if(entry.kind==="reset"){if(set(activeField,null,true)){view="fields";selected=returnSelection}return}
            if(entry.kind==="typography"){if(structureType==="section"?run("section",{kind:"presentation",id:structureId,value:Object.assign({},current.presentation || {},entry.value)}):run("patch",{ids:selectedIds,value:{presentation:entry.value}})){view="fields";selected=returnSelection}return}
            if(entry.needsTarget){pendingAction=entry.value;beginKeyboard("target","");return}
            if(set(activeField,entry.value,false)){view="fields";selected=returnSelection}return
        }
        if(view==="artwork"){if(selected===0)beginKeyboard("import","");else if(selected===1){if(set(activeField,null,true)){view="fields";selected=returnSelection}}else pickArtwork(assets[selected-2].source);return}
        if(view==="crop"){
            if(selected<4){const f=cropField(selected);if(selected===0)set(f,effective(current,f)==="cover"?"contain":"cover",false);else beginKeyboard("cropNumber",effective(current,f));return}
            if(selected===4){if(assetProbe.status!==Image.Ready){message="Choose a readable image before cropping.";return}cropDraft=Object.assign({x:0,y:0,width:assetProbe.sourceSize.width,height:assetProbe.sourceSize.height},effective(current,{scope:pageMode?"background":"presentation",key:"imageCrop"}) || {});view="pixelCrop";selected=0;return}
            if(selected===5){set({scope:pageMode?"background":"presentation",key:"imageCrop",type:"crop"},null,true);return}view="fields";selected=returnSelection;return
        }
        if(view==="pixelCrop"){
            if(selected===5){view="crop";selected=4;return}
            if(selected<4){beginKeyboard("pixel",cropDraft[["x","y","width","height"][selected]]);return}
            if(cropDraft.x+cropDraft.width>assetProbe.sourceSize.width || cropDraft.y+cropDraft.height>assetProbe.sourceSize.height){message="The crop exceeds the image bounds. Adjust it or Cancel.";return}
            if(set({scope:pageMode?"background":"presentation",key:"imageCrop",type:"crop"},cropDraft,false)){view="crop";selected=4}return
        }
        if(view==="layout"){
            if(selected===8){view="fields";selected=returnSelection;return}
            if(selected===7){if(run("childLayout",{id:current.id,value:layoutDraft})){view="fields";selected=returnSelection}return}
            adjustLayout(1);return
        }
        if(view==="apps"){
            if(selected<=1){if(run("setApps",{id:current.id,mode:selected===0?"all":"selected",apps:appSelection})){view="fields";selected=returnSelection}return}
            const id=apps[selected-2].action.appId,next=appSelection.slice(),index=next.indexOf(id);if(index>=0)next.splice(index,1);else if(next.length<128)next.push(id);else{message="Choose up to 128 apps.";return}appSelection=next
        }
    }
    function cropField(index){return {scope:pageMode?"background":"presentation",key:["artFit","artFocalX","artFocalY","artZoom"][index],fallback:index===0?"cover":index===3?1:.5,min:index===3?1:0,max:index===3?8:1,step:index===3?.1:.05,type:index===0?"choice":"number"}}
    function adjustLayout(direction){
        let next=Object.assign({},layoutDraft)
        if(selected===0){const modes=["grid","row","column"],index=modes.indexOf(next.mode)+direction;if(index>=0 && index<modes.length)next.mode=modes[index];if(next.mode!=="row"){delete next.visibleColumns;delete next.wrap}}
        else if(selected===6)next.wrap=!next.wrap
        else if(selected<6){const key=["","columns","rows","gap","padding","visibleColumns"][selected],n=(next[key]===undefined?next.columns:next[key])+direction,min=selected<3 || selected===5?1:0,max=selected===1 || selected===5?(next.mode==="row"?128:16):selected===2?10:100;if(n<min || n>max){message="Choose "+min+"–"+max+".";return}next[key]=n}
        layoutDraft=next
    }
    function chooseAction(entry){
        if(entry.needsMedia){media.libraryKind=entry.needsMedia;media.selected=0;view="mediaAction";return}
        if(entry.needsTarget){pendingAction=entry.action;beginKeyboard("target","");return}
        if(set(activeField,entry.action || {},false)){view="fields";selected=returnSelection}
    }
    function handleCommand(command){
        if(view==="actionPicker"){if(actionLoader.item)actionLoader.item.handleCommand(command);return}
        if(view==="apps"){if(appPicker)appPicker.handleCommand(command);return}
        if(view==="scalar"){scalarCommand(command);return}
        if(view==="colorWheel"){colorPicker.handleCommand(command);return}
        if(view==="mediaAction"){media.handleCommand(command);return}
        if(view==="keyboard"){keyboard.handleCommand(command);return}
        if(collapsed){if(command===inputManager.back)closed();else if(command===inputManager.confirm){controlsShown=true;hiddenControls=false;}return}
        if(command===inputManager.back){if(view==="tabs"){closed();return}if(view==="fields"){view="tabs";selected=tab}else if(view==="pixelCrop"){view="crop";selected=4}else{view="fields";selected=returnSelection}return}
        if(command===inputManager.search){hiddenControls=!hiddenControls;return}
        if(command===inputManager.menu && view==="fields" && fields[selected]){reset(fields[selected]);return}
        if(command===inputManager.confirm){activate();return}
        const direction=command===inputManager.navigateRight?1:command===inputManager.navigateLeft?-1:0
        if(direction && view==="fields" && fields[selected] && fields[selected].control==="slider"){activeField=fields[selected];returnSelection=selected;beginScalar();scalarCommand(command);return}
        if(direction && view==="fields" && fields[selected] && ["number","toggle"].indexOf(fields[selected].type)>=0){adjust(fields[selected],direction);return}
        if(direction && view==="crop" && selected>0 && selected<4){adjust(cropField(selected),direction);return}
        if(direction && view==="layout" && selected<7){adjustLayout(direction);return}
        if(direction && view==="pixelCrop" && selected<4){const key=["x","y","width","height"][selected],n=cropDraft[key]+direction,min=selected<2?0:1;if(n<min || n>16384){message="Crop values must stay within image bounds.";return}cropDraft=Object.assign({},cropDraft,{[key]:n});return}
        if(command===inputManager.navigateDown || command===inputManager.navigateRight)selected=Math.min(labels().length-1,selected+1)
        else if(command===inputManager.navigateUp || command===inputManager.navigateLeft)selected=Math.max(0,selected-1)
    }
    function contrast(){
        if(!page)return ""
        function luminance(value){const c=Qt.darker(value,1);function linear(v){return v<=.04045?v/12.92:Math.pow((v+.055)/1.055,2.4)}return .2126*linear(c.r)+.7152*linear(c.g)+.0722*linear(c.b)}
        const style=!pageMode && current?current.presentation || {}:{},a=luminance(style.labelColor || page.appearance.textColor)
        const backgrounds=pageMode && (activeField.key==="backgroundStart" || activeField.key==="backgroundEnd" || activeField.key==="textColor")?[page.appearance.backgroundStart,page.appearance.backgroundEnd]:[style.surfaceColor || page.appearance.surfaceColor]
        const ratio=Math.min(...backgrounds.map(value=>{const b=luminance(value);return (Math.max(a,b)+.05)/(Math.min(a,b)+.05)}))
        return (ratio>=4.5?"Text contrast ":"Low text contrast · ")+ratio.toFixed(1)+":1 · solid-surface estimate"
    }
    Loader {
        id:colorLoader;anchors.fill:parent;active:false;visible:root.view==="colorWheel";z:10
        sourceComponent:Component { StudioColorPicker {
        appearance:Object.keys(root.pickerAppearance).length?root.pickerAppearance:root.appearance;settings:root.options.colorPicker || ({})
        roleName:root.activeField.label || "Color";selectionRect:root.pageMode?null:root.selectionRect
        themeColors:root.pickerThemeColors;recentColors:root.recentColors
        contrastText:root.contrast()
        onPreview:(value,reset)=>{
            if(root.set(root.activeField,value,reset) && reset)Qt.callLater(()=>{
                if(root.view!=="colorWheel" || !root.session.proposalModel)return
                const model=root.pageId?root.session.proposalModel.pageModel(root.pageId):root.session.proposalModel
                colorPicker.sample=root.pageMode && root.activeField.scope==="theme"?model.appearance[root.activeField.key]:root.effective(root.current,root.activeField)
            })
        }
        onAccepted:(value,reset)=>root.acceptColor(value,reset)
        onCanceled:root.cancelColor()
        onExactRequested:value=>root.beginKeyboard("color",value)
        } }
    }
    Image {id:assetProbe;visible:false;source:root.pageMode && root.page?root.page.wallpaperSource:root.current?root.current.logoSource || "":""}
    Loader {
        anchors.fill:parent;active:root.scalarTransactionActive;visible:root.view==="scalar";z:10
        sourceComponent:Component { Item { Rectangle {
            objectName:"studioScalarPanel"
            readonly property real uiScale:Math.min(root.width/1280,root.height/720)
            width:Math.min(root.width-32*uiScale,500*uiScale);height:scalarBody.implicitHeight+36*uiScale
            x:root.roomLeft?root.selectionRect.x-width-24*uiScale:root.roomRight?root.selectionRect.x+root.selectionRect.width+24*uiScale:root.width-width-24*uiScale
            y:Math.max(16*uiScale,(root.height-height)/2)
            color:root.scalarAppearance.surfaceColor;radius:16*uiScale;border.width:2;border.color:root.scalarAppearance.focusColor
            Column {
                id:scalarBody;x:18*parent.uiScale;y:18*parent.uiScale;width:parent.width-36*parent.uiScale;spacing:14*parent.uiScale
                Text {width:parent.width;text:root.activeField.label;elide:Text.ElideRight;color:root.scalarAppearance.textColor;font.pixelSize:24*root.scalarUnit;font.bold:true}
                Text {text:"Before "+root.scalarText(root.scalarOriginal)+"   →   "+(root.scalarResetChoice?"Theme · ":"")+root.scalarText(root.scalarDraft);color:root.scalarAppearance.textColor;font.pixelSize:22*root.scalarUnit}
                Slider {
                    id:scalarSlider;objectName:"studioScalarSlider";width:parent.width;height:38*root.scalarUnit
                    from:root.activeField.min || 0;to:root.activeField.max || 1;stepSize:root.activeField.step || 1;snapMode:Slider.SnapAlways;value:root.scalarDraft
                    Accessible.name:root.activeField.label
                    onMoved:{root.scalarControl=0;root.changeScalar(Number(value.toFixed(4)),false)}
                    background:Rectangle {x:scalarSlider.leftPadding;y:(scalarSlider.height-height)/2;width:scalarSlider.availableWidth;height:6*root.scalarUnit;radius:height/2;color:root.scalarAppearance.mutedColor;Rectangle {width:parent.width*scalarSlider.visualPosition;height:parent.height;radius:parent.radius;color:root.scalarAppearance.focusColor}}
                    handle:Rectangle {x:scalarSlider.leftPadding+scalarSlider.visualPosition*(scalarSlider.availableWidth-width);y:(scalarSlider.height-height)/2;width:22*root.scalarUnit;height:width;radius:width/2;color:root.scalarAppearance.focusColor;border.width:root.scalarControl===0?3:1;border.color:root.scalarAppearance.textColor}
                }
                Row {
                    width:parent.width
                    Text {width:parent.width/2;text:root.scalarText(root.activeField.min || 0);color:root.scalarAppearance.mutedColor;font.pixelSize:15*root.scalarUnit}
                    Text {width:parent.width/2;horizontalAlignment:Text.AlignRight;text:root.scalarText(root.activeField.max || 1);color:root.scalarAppearance.mutedColor;font.pixelSize:15*root.scalarUnit}
                }
                Row {
                    visible:!!root.activeField.presets;spacing:8*root.scalarUnit
                    Repeater {
                        model:root.activeField.presets || []
                        delegate:Button {
                            required property var modelData;required property int index
                            width:(scalarBody.width-16*root.scalarUnit)/3;height:44*root.scalarUnit;text:modelData.name
                            onClicked:{root.scalarControl=1;root.scalarPreset=index;root.changeScalar(modelData.value,false)}
                            background:Rectangle {radius:Math.min(14*root.scalarUnit,parent.modelData.value*root.scalarUnit/4);color:root.scalarAppearance.selectedColor;border.width:root.scalarControl===1 && root.scalarPreset===parent.index?3:root.scalarDraft===parent.modelData.value?2:1;border.color:root.scalarAppearance.focusColor}
                            contentItem:Text {text:parent.text;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;color:root.scalarAppearance.textColor;font.pixelSize:16*root.scalarUnit}
                        }
                    }
                }
                Text {visible:!!root.message;width:parent.width;text:root.message;wrapMode:Text.Wrap;color:root.scalarAppearance.textColor;font.pixelSize:16*root.scalarUnit}
                Row {
                    spacing:8*root.scalarUnit
                    Repeater {
                        model:["Reset to theme","Done","Cancel"]
                        delegate:Button {
                            required property string modelData;required property int index
                            objectName:"studioScalarAction-"+index;width:(scalarBody.width-16*root.scalarUnit)/3;height:44*root.scalarUnit;text:modelData
                            onClicked:{root.scalarControl=index+root.scalarActionBase;root.scalarCommand(inputManager.confirm)}
                            background:Rectangle {radius:7*root.scalarUnit;color:root.scalarAppearance.selectedColor;border.width:root.scalarControl===parent.index+root.scalarActionBase?2:0;border.color:root.scalarAppearance.focusColor}
                            contentItem:Text {text:parent.text;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;color:root.scalarAppearance.textColor;font.pixelSize:15*root.scalarUnit}
                        }
                    }
                }
                Button {
                    objectName:"studioScalarExact";width:parent.width;height:36*root.scalarUnit;text:"Exact value…"
                    onClicked:{root.scalarControl=root.scalarActionBase+3;root.scalarCommand(inputManager.confirm)}
                    background:Rectangle {color:"transparent";radius:5*root.scalarUnit;border.width:root.scalarControl===root.scalarActionBase+3?2:0;border.color:root.scalarAppearance.focusColor}
                    contentItem:Text {text:parent.text;color:root.scalarAppearance.textColor;font.pixelSize:16*root.scalarUnit;verticalAlignment:Text.AlignVCenter}
                }
            }
        } } }
    }
    Rectangle {
        visible:root.collapsed && !root.previewActive && root.view!=="apps" && root.view!=="actionPicker";anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.margins:24*root.unit
        width:Math.min(parent.width-48,440*root.unit);height:64*root.unit;radius:12*root.unit;color:root.appearance.surfaceColor;border.width:2;border.color:root.appearance.focusColor
        Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";anchors.fill:parent;anchors.margins:12*root.unit;text:"Show controls · selection stays full size";color:root.appearance.textColor;font.pixelSize:19*root.unit;wrapMode:Text.Wrap}
        MouseArea {anchors.fill:parent;onClicked:{root.controlsShown=true;root.hiddenControls=false}}
    }
    Rectangle {
        id:panel;visible:!root.collapsed && root.view!=="actionPicker" && root.view!=="apps" && root.view!=="keyboard" && root.view!=="colorWheel" && root.view!=="scalar";width:root.panelWidth;height:root.panelHeight
        x:root.roomRight?root.selectionRect.x+root.selectionRect.width+24*root.unit:root.roomLeft?root.selectionRect.x-width-24*root.unit:(parent.width-width)/2
        y:Math.max(24*root.unit,Math.min(parent.height-height-24*root.unit,root.selectionRect?root.selectionRect.y:(parent.height-height)/2))
        radius:18*root.unit;color:root.appearance.surfaceColor;border.width:2;border.color:root.appearance.focusColor
        Column {
            anchors.fill:parent;anchors.margins:22*root.unit;spacing:10*root.unit
            Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";id:titleText;width:parent.width;text:root.pageMode?"Page appearance":root.targets.length>1?root.targets.length+" selected components":root.current?root.current.name || "Component":"Component";color:root.appearance.textColor;font.pixelSize:26*root.unit;elide:Text.ElideRight}
            Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";id:detailText;width:parent.width;text:root.view==="fields"?root.tabs[root.tab]+" · "+root.contrast():root.view==="tabs"?"Choose what to edit":root.view;color:root.appearance.mutedColor;font.pixelSize:17*root.unit;wrapMode:Text.Wrap}
            Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";id:statusText;width:parent.width;text:root.view==="playlistSource"?(root.playlistClient?root.playlistClient.status:"Kodi unavailable · Back returns without changes"):root.current && root.current.unavailableReason?root.current.unavailableReason+" · Change its source or On select link.":root.message;color:root.appearance.mutedColor;font.pixelSize:16*root.unit;wrapMode:Text.Wrap;maximumLineCount:3;elide:Text.ElideRight}
            ListView {
                id:list;objectName:"studioInspectorFields";width:parent.width;height:Math.max(48*root.unit,parent.height-titleText.height-detailText.height-statusText.height-footerText.height-4*parent.spacing);clip:true;spacing:5*root.unit;currentIndex:root.selected;highlightRangeMode:ListView.ApplyRange;highlightMoveDuration:0
                model:root.labels()
                // A model reset can reset ListView currentIndex to zero; restore the remote field origin.
                onModelChanged:Qt.callLater(()=>{list.currentIndex=Qt.binding(()=>root.selected);list.positionViewAtIndex(root.selected,ListView.Contain)})
                delegate:Rectangle {
                    required property int index;required property string modelData
                    readonly property var rowField:root.view==="fields" && root.tabs[root.tab]!=="More"?root.fields[index]:null
                    width:list.width;height:48*root.unit;radius:8*root.unit;color:index===root.selected?root.appearance.selectedColor:"transparent";border.width:index===root.selected?2:0;border.color:root.appearance.focusColor
                    Text {visible:!scrollCheckbox.visible && !propertySwitch.visible;textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";anchors.fill:parent;anchors.margins:10*root.unit;anchors.rightMargin:colorSample.visible?48*root.unit:10*root.unit;text:modelData;color:root.appearance.textColor;font.pixelSize:18*root.unit;elide:Text.ElideRight}
                    Rectangle {id:colorSample;visible:!!parent.rowField && parent.rowField.type==="color";anchors.right:parent.right;anchors.rightMargin:12*root.unit;anchors.verticalCenter:parent.verticalCenter;width:26*root.unit;height:width;radius:5*root.unit;color:visible?root.effective(root.current,parent.rowField):"transparent";border.width:1;border.color:root.appearance.textColor}
                    CheckBox {
                        id:scrollCheckbox;objectName:"studioScrollingCheckbox";anchors.fill:parent;visible:!!parent.rowField && parent.rowField.type==="scrolling"
                        checkable:false;checked:visible && root.effective(root.current,parent.rowField)===true
                        text:visible?parent.rowField.label:"";font.pixelSize:18*root.unit
                        indicator:Rectangle {x:8*root.unit;anchors.verticalCenter:parent.verticalCenter;width:24*root.unit;height:width;radius:4*root.unit;color:scrollCheckbox.checked?root.appearance.focusColor:root.appearance.surfaceColor;border.width:2;border.color:root.appearance.focusColor;Text {anchors.centerIn:parent;text:scrollCheckbox.checked?"✓":"";color:root.appearance.surfaceColor}}
                        contentItem:Text {text:scrollCheckbox.text;leftPadding:44*root.unit;verticalAlignment:Text.AlignVCenter;color:root.appearance.textColor;font:scrollCheckbox.font;elide:Text.ElideRight}
                        onClicked:{root.selected=index;root.activateField(parent.rowField)}
                    }
                    Switch {
                        id:propertySwitch
                        objectName:"studioSwitch-"+(parent.rowField?parent.rowField.key:"")
                        anchors.fill:parent;visible:!!parent.rowField && parent.rowField.type==="toggle"
                        checkable:true;checked:visible && root.effective(root.current,parent.rowField)===true
                        text:visible?parent.rowField.label:"";Accessible.name:text;Accessible.checkable:true;Accessible.checked:checked
                        indicator:Rectangle {
                            x:propertySwitch.width-width-12*root.unit;anchors.verticalCenter:parent.verticalCenter
                            width:48*root.unit;height:26*root.unit;radius:height/2
                            color:propertySwitch.checked?root.appearance.focusColor:root.appearance.surfaceColor
                            border.width:1;border.color:root.appearance.mutedColor
                            Rectangle {width:18*root.unit;height:width;radius:width/2;y:4*root.unit;x:(propertySwitch.checked?26:4)*root.unit;color:propertySwitch.checked?root.appearance.surfaceColor:root.appearance.textColor}
                        }
                        contentItem:Text {text:propertySwitch.text;leftPadding:10*root.unit;rightPadding:72*root.unit;verticalAlignment:Text.AlignVCenter;color:root.appearance.textColor;font.pixelSize:18*root.unit;font.family:root.appearance.fontFamily || "DejaVu Sans";elide:Text.ElideRight}
                        onClicked:{root.selected=index;root.activateField(parent.rowField);checked=Qt.binding(()=>propertySwitch.visible && root.effective(root.current,propertySwitch.parent.rowField)===true)}
                    }
                    Accessible.role:Accessible.Button;Accessible.name:modelData;Accessible.ignored:scrollCheckbox.visible || propertySwitch.visible
                    MouseArea {anchors.fill:parent;enabled:!scrollCheckbox.visible && !propertySwitch.visible;onClicked:{root.selected=index;root.activate()}}
                }
            }
            Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";id:footerText;width:parent.width;text:root.collapseNeeded?"Confirm edits · Back returns":"Adjust · Menu resets · Back returns";color:root.appearance.mutedColor;font.pixelSize:14*root.unit;wrapMode:Text.Wrap}
        }
        Rectangle {visible:true;anchors.right:parent.right;anchors.top:parent.top;width:140*root.unit;height:34*root.unit;color:root.appearance.selectedColor;Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";anchors.centerIn:parent;text:"Hide controls";color:root.appearance.textColor;font.pixelSize:14*root.unit}MouseArea {anchors.fill:parent;onClicked:root.hiddenControls=true}}

    }
    Loader {
        id:actionLoader;anchors.fill:parent;visible:root.view==="actionPicker";active:root.visible && root.activeField.type==="action" && ["actionPicker","mediaAction","keyboard"].indexOf(root.view)>=0
        sourceComponent:Component {StudioActionPicker {
            anchors.fill:parent;actions:[{name:"None · clear link",action:{}}].concat(root.actions,[{name:"Play a Kodi movie…",needsMedia:"movies"},{name:"Open a Kodi show…",needsMedia:"shows"}]);appearance:root.appearance;categories:root.options.actionCategories || []
            onCanceled:{root.view="fields";root.selected=root.returnSelection}
            onPicked:entry=>root.chooseAction(entry)
        }}
    }
    StudioMediaPicker {id:media;collections:root.actions.filter(a=>a.action && a.action.type==="open-collection");anchors.fill:parent;visible:root.view==="mediaAction";provider:root.provider;appearance:root.appearance;onCanceled:root.view="actionPicker";onPicked:action=>{if(root.set(root.activeField,action,false)){root.view="fields";root.selected=root.returnSelection}}}
    Loader {
        id:appPickerLoader;anchors.fill:parent;active:root.visible && root.view==="apps"
        onLoaded:item.begin(root.appSelection,root.current && root.current.appCollection?root.current.appCollection.mode || "selected":"selected")
        sourceComponent:Component {StudioAppPicker {
            anchors.fill:parent;apps:root.apps;settings:root.options.appPicker || ({});appearance:root.appearance
            onCanceled:{root.view="fields";root.selected=root.returnSelection}
            onAccepted:(mode,ids)=>{if(root.run("setApps",{id:root.current.id,mode:mode,apps:ids})){root.appSelection=ids.slice();root.view="fields";root.selected=root.returnSelection}else message=root.message}
        }}
    }
    OnScreenKeyboard {
        id:keyboard;objectName:"inspectorKeyboard";anchors.fill:parent;visible:root.view==="keyboard";appearance:root.appearance;hint:root.message
        onCanceled:{root.view=root.returnView;root.selected=root.keyboardSelection;if(root.keyboardKind==="scalarNumber")root.message=""}
        onAccepted:text=>{
            if(root.keyboardKind==="scalarNumber"){
                const value=Number(text)/(root.activeField.displayScale || 1)
                if(!text.trim() || !Number.isFinite(value) || value<root.activeField.min || value>root.activeField.max || root.activeField.step>=1 && !Number.isInteger(value)){root.message="Choose a value within the slider range.";return}
                root.view="scalar";root.changeScalar(value,false);return
            }
            if(root.keyboardKind==="color"){
                if(!/^#[0-9a-fA-F]{6}$/.test(text)){root.message="Use # followed by six hexadecimal digits.";return}
                root.view="colorWheel";colorPicker.choose(text);return
            }
            if(root.keyboardKind==="import"){root.pickArtwork(text);return}
            if(root.keyboardKind==="pixel"){const n=Number(text),key=["x","y","width","height"][root.keyboardSelection];if(!Number.isInteger(n) || n<(root.keyboardSelection<2?0:1) || n>16384){root.message="Enter a whole pixel value within the image bounds.";return}root.cropDraft=Object.assign({},root.cropDraft,{[key]:n});root.view="pixelCrop";root.selected=root.keyboardSelection;return}
            let f=root.activeField,value=text
            if(root.keyboardKind==="number" || root.keyboardKind==="cropNumber"){
                if(root.keyboardKind==="cropNumber")f=root.cropField(root.keyboardSelection)
                value=Number(text);if(!text.trim() || !Number.isFinite(value) || value<f.min || value>f.max || f.step>=1 && !Number.isInteger(value)){root.message="Enter "+f.min+"–"+f.max+" for "+f.label+".";return}
            }
            if(root.keyboardKind==="target")value=Object.assign({},root.pendingAction,{target:text})
            if(root.set(f,value,false)){root.view=root.keyboardKind==="cropNumber"?"crop":"fields";root.selected=root.keyboardKind==="cropNumber"?root.keyboardSelection:root.returnSelection}
        }
    }
}
