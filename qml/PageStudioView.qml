import QtQuick

Item {
    id: studio
    objectName: "pageStudio"
    property var layoutModel
    property var session: null
    property bool open: false
    property string pageId: ""
    property string scopeId:""
    property bool scopeOverview:false
    property real sectionHeightDraft:0
    property string selectedId: ""
    property string mode: "canvas"
    property bool toolbarFocus: true
    property int tool: 0
    property int menuIndex:0
    property int choice: 0
    property string message: ""
    property string addingType: ""
    property string resizeAxis:"both"
    property string resizeAnchor:"top-left"
    property string gestureMode:"move"
    property string originScopeId:""
    property string collisionStrategy:"shift"
    property string deletionReplacement:""
    property bool deletionRemaps:false
    property var duplicateBefore:[]
    property var pageFocus:({})
    property var candidate: ({x:0,y:0,width:1,height:1})
    property var origin: ({})
    property var proposal: ({})
    property var gridDraft: ({})
    property string keyboardAction: ""
    property string keyboardReturnMode: "context"
    property var palette: draftModel?draftModel.studioAppearance:layoutModel ? layoutModel.studioAppearance : ({})
    readonly property var draftModel: session ? (mode==="conversionPreview" && session.proposalModel ? session.proposalModel : session.model) : null
    readonly property var page: draftModel ? (pageId ? draftModel.pageModel(pageId) : draftModel) : null
    readonly property var items: page ? page.items : []
    readonly property var pageGrid:page?page.grid:({})
    readonly property bool sectionMode:gridMode && pageGrid.mode==="sections"
    readonly property var selectedSection:page?(page.sections || []).find(s=>(s.id || "")===scopeId):null
    readonly property var grid:sectionMode && selectedSection?Object.assign({},pageGrid,(selectedSection.presentation || {}).grid || {}):pageGrid
    readonly property var scopedItems:sectionMode && !scopeOverview?items.filter(i=>(i.categoryId || "")===scopeId):items
    readonly property var gridModes:layoutModel && layoutModel.studioOptions?layoutModel.studioOptions.gridModes || []:[]
    readonly property bool gridMode: {
        if(!page || !page.composition.enabled || !page.composition.grid)return false
        for(const item of items)if(!item.placement)return false
        if(page.composition.grid.mode===undefined)for(const section of page.sections || []) {
            const style=section.presentation || {};if(style.variant==="library" || style.trailingAction)return false
        }
        return true
    }
    readonly property var tools: ["Add", "Pages", "Layers", "Undo", "Redo", "Grid", "Fit", "Preview", "Apply", "Scope"]
    readonly property var contextActions: ["Move", "Resize", "Rename", "Duplicate", "Remove", "Text", "Surface opacity", "Corners", "Position and size","Hide / show","Lock / unlock",gridMode?"Reading order before…":"Move before…",gridMode?"Reading order after…":"Move after…","Move to section…"]
    readonly property var names: ["Clock", "Text"]
    readonly property var current: items.find(i=>i.id===selectedId) || null
    readonly property bool manipulating:["move","resize","collision","collisionDetails","geometry"].indexOf(mode)>=0
    function componentName(item) {
        if(!item)return addingType?addingType.charAt(0).toUpperCase()+addingType.slice(1):"Component"
        return item.name || (item.widgetType?item.widgetType.charAt(0).toUpperCase()+item.widgetType.slice(1):item.id)
    }
    readonly property string operationSummary:componentName(addingType?null:current)+" · row "+(candidate.y+1)+", column "+(candidate.x+1)+" · "+candidate.width+" × "+candidate.height+" cells"
    readonly property string operationTitle:({flowMove:"Move in this layout",moveOptions:"Move and size",anchors:"Resize anchor",destination:"Choose a section",orderBefore:gridMode?"Reading order before":"Move before",orderAfter:gridMode?"Reading order after":"Move after",collision:"Resolve overlap",collisionDetails:"Affected components",geometry:"Position and size"})[mode] || ""
    readonly property string footerText: {
        if(mode==="move" || mode==="resize")return breadcrumb+" · "+(mode==="move"?"Moving · Arrows position · Confirm place · Back cancel · Menu more":"Sizing "+resizeAxis+" · "+resizeAnchor+" anchor · Left/Right adjust · Confirm accept · Back cancel")+"\n"+operationSummary+(message?" · "+message:"")
        if(mode==="collision")return breadcrumb+" · Resolve overlap · Choose a proposal or Cancel · Back cancels"
        if(mode==="collisionDetails")return breadcrumb+" · Proposed positions only · Back returns to the choices"
        return breadcrumb+" · Edit mode · Confirm select · Menu actions · Back pages"
    }
    readonly property var relativeItems:current?items.filter(i=>i.id!==selectedId && (i.categoryId || "")===(current.categoryId || "")):[]
    readonly property var destinationSections:sectionMode?page.sections.filter(s=>(s.presentation || {}).variant!=="library"):[]
    readonly property var placementOptions:sectionMode?({categoryId:scopeId}):({})
    function collisionProposal(strategy) {
        if((mode!=="collision" && mode!=="collisionDetails") || !session)return ({})
        return addingType?session.additionProposal(addingType,candidate,strategy,pageId,sectionMode?scopeId:"")
                         :session.placementProposal(selectedId,candidate,strategy,pageId,placementOptions)
    }
    readonly property var shiftPreview:collisionProposal("shift")
    readonly property var swapPreview:collisionProposal("swap")
    readonly property var collisionPreview: {
        if((mode!=="collision" && mode!=="collisionDetails") || (mode==="collision" && choice===2) || !session)return ({})
        return collisionStrategy==="shift"?shiftPreview:swapPreview
    }
    readonly property real textScale: palette.typographyScale || 1
    readonly property real spacing: palette.spacing || 20
    readonly property var pageEntries:draftModel?draftModel.studioPages:[]
    readonly property var removalDestinations:pageEntries.filter(p=>p.id!==pageId)
    readonly property string breadcrumb:((pageEntries.find(p=>p.id===pageId)||{}).name || "Page")+(sectionMode?" / "+(scopeOverview?"All sections":selectedSection?selectedSection.name:"Uncategorized"):"")
    readonly property var menuEntries:[{label:"＋ Add page",action:"new"}].concat(pageEntries.map(p=>({label:(p.id===draftModel.homePageId?"⌂ ":"")+p.name,action:"page",id:p.id}))).concat([
        {label:"＋ Add component",action:"add"},{label:"Page appearance",action:"appearance"},{label:"More tools",action:"more"},{label:"Review & Apply",action:"apply"},{label:"Edit page →",action:"edit"}])
    function activateMenu(index) {
        menuIndex=index;const entry=menuEntries[index];if(!entry)return
        choice=0
        if(entry.action==="new")beginKeyboard("newPage","")
        else if(entry.action==="page"){pageFocus[pageId]=selectedId;pageId=entry.id;selectedId=pageFocus[pageId] || "";mode="pageContext"}
        else if(entry.action==="add")chooseTool(0)
        else if(entry.action==="appearance")mode="pageAppearance"
        else if(entry.action==="more")mode="more"
        else if(entry.action==="apply")mode="apply"
        else {toolbarFocus=false;mode="canvas";if(!current && scopedItems.length)selectedId=scopedItems[0].id}
    }
    signal closed()
    visible: open
    z: 100
    onSelectedIdChanged:if(current && sectionMode)scopeId=current.categoryId || ""
    onChoiceChanged:if(mode==="collision" && choice<2)collisionStrategy=choice===0?"shift":"swap"
    onScopeIdChanged:if(open && sectionMode && !scopeOverview)Qt.callLater(()=>{if(open && sectionMode)preview.revealScope(scopeId)})

    function show(id, component) {
        session = layoutModel.beginStudio()
        pageId = id || (!session.model.rootPageVisible?session.model.homePageId:""); selectedId = component || "";pageFocus=({})
        mode = "canvas"; toolbarFocus = true; tool = 0;menuIndex=0; message = "Draft · Apply saves your interface"
        scopeOverview=false;scopeId=current?current.categoryId || "":page && page.sections.length?page.sections[0].id || "":""
        open = true
    }
    function close() { if(session) session.invalidate(); open=false; closed() }
    function command(op,args) {
        if (!session || !session.execute(op,args || {},["newPage","renamePage","duplicatePage","reorderPage","removePage","setHomePage","studioAppearance"].indexOf(op)>=0 ? "" : pageId)) {
            message = session ? session.errorString : "The edit session is closed."
            return false
        }
        message = "Draft updated. Nothing applied yet."
        return true
    }
    function position(item) { return item && item.placement ? Object.assign({},item.placement) : ({x:0,y:0,width:1,height:1}) }
    function beginGesture(resize) {
        if (!current) { message="Choose a component in Layers first."; return }
        if(current.locked){message="This component is locked. Choose Edit component → Lock / unlock first.";return}
        if(!gridMode){if(resize)beginGrid();else{mode="flowMove";choice=0;toolbarFocus=false;message="This page uses an ordered layout. Move before/after preserves it; grid conversion is an explicit preview."}return}
        addingType=""; origin=position(current); candidate=Object.assign({},origin)
        originScopeId=current.categoryId || "";scopeId=originScopeId;resizeAxis="both";resizeAnchor="top-left"
        gestureMode=resize?"resize":"move"
        message=""
        mode=resize ? "resize" : "move"; toolbarFocus=false
    }
    function cancelGesture() {
        if(!addingType && current){scopeId=originScopeId;candidate=Object.assign({},origin);Qt.callLater(()=>preview.revealPlacement(origin,originScopeId))}
        addingType="";mode="canvas";message="Canceled · no draft or history changes."
    }
    function beginRelative(before) {
        if(!current || addingType){message="Place the new component before changing its reading order.";return}
        if(current.locked){message="Unlock this component before changing its order.";return}
        scopeId=current.categoryId || "";candidate=position(current)
        mode=before?"orderBefore":"orderAfter";choice=0
        message=gridMode?"Change reading order. Grid positions remain as authored.":"Choose the neighboring component in this section."
    }
    function beginGrid() {
        if(!gridModes.length){message="Packaged Studio defaults are missing or invalid.";return}
        gridDraft=Object.assign({},layoutModel.studioOptions.defaultGrid,pageGrid)
        sectionHeightDraft=page && page.composition.sectionHeight!==undefined?page.composition.sectionHeight:layoutModel.studioOptions.sectionHeight
        mode="convert";choice=6
    }
    function gridField(index) {
        if(index===1)return {key:"columns",min:4,max:16,integer:true,step:1}
        if(index===2)return {key:"rows",min:3,max:10,integer:true,step:1}
        if(index===3)return {key:"gap",min:0,max:100,integer:false,step:1}
        if(index===4)return {key:"margin",min:0,max:100,integer:false,step:1}
        return {key:"sectionHeight",min:60,max:800,integer:false,step:20}
    }
    function acceptCandidate(strategy) {
        if (addingType) {
            proposal=session.additionProposal(addingType,candidate,strategy || "cancel",pageId,sectionMode?scopeId:"")
            if(!proposal.valid){message=proposal.reason;const previous=mode;mode="collision";if(previous!=="collision"){collisionStrategy="shift";choice=0}return}
            if(command("batch",{commands:proposal.commands})){selectedId=proposal.id;addingType="";mode="canvas"}
            return
        }
        proposal=session.placementProposal(selectedId,candidate,strategy || "cancel",pageId,placementOptions)
        if (!proposal.valid) { message=proposal.reason; const previous=mode; mode="collision"; if(previous!=="collision"){collisionStrategy="shift";choice=0} return }
        if (command("place",{placements:proposal.placements})) mode="canvas"
    }
    function beginKeyboard(action, value) { keyboardReturnMode=mode; keyboardAction=action; keyboard.value=value || ""; keyboard.selectedIndex=0; mode="keyboard" }
    function chooseTool(index) {
        tool=index; choice=0
        if(index===0) { if(!gridMode) beginGrid(); else mode="add" }
        else if(index===1) mode="pages"
        else if(index===2) mode="layers"
        else if(index===3) { if(!session.undo()) message=session.errorString }
        else if(index===4) { if(!session.redo()) message=session.errorString }
        else if(index===5) beginGrid()
        else if(index===6) { camera.zoom=1; message="Canvas fitted to the current device aspect." }
        else if(index===7) { preview.restoreFocus(selectedId); mode="preview"; message="Draft preview: external actions are intercepted. Back returns to editing." }
        else if(index===8) mode="apply"
        else if(index===9){if(sectionMode)mode="scopes";else message="Section scopes are available in scrolling section layouts."}
    }
    function context(index) {
        choice=index
        if(index===0) beginGesture(false)
        else if(index===1) beginGesture(true)
        else if(index===2) beginKeyboard("rename",current ? current.name || current.id : "")
        else if(index===3) { if(command("duplicate",{id:selectedId})) {selectedId=items[items.length-1].id;mode="canvas"} }
        else if(index===4) { mode="remove";choice=0 }
        else if(index===5) beginKeyboard("text",current && current.presentation ? current.presentation.subtitle : "")
        else if(index===6) beginKeyboard("opacity",current && current.presentation && current.presentation.surfaceOpacity!==undefined ? String(current.presentation.surfaceOpacity) : "1")
        else if(index===7) beginKeyboard("corners",current && current.presentation ? String(current.presentation.radius || 0) : "0")
        else if(index===8) {beginGesture(false);if(mode==="move"){mode="geometry";choice=0}}
        else if(index===9){if(command("state",{id:selectedId,key:"hidden",value:!current.hidden})){mode="canvas";toolbarFocus=false}}
        else if(index===10){if(command("state",{id:selectedId,key:"locked",value:!current.locked})){mode="canvas";toolbarFocus=false}}
        else if(index===11 || index===12)beginRelative(index===11)
        else if(index===13){beginGesture(false);if(mode==="move"){mode="destination";choice=0}}
    }
    function dialogLabels() {
        if(mode==="pageContext")return ["Edit page","Appearance","Rename page","Duplicate page",pageId===draftModel.homePageId?"Current Home page":"Set as Home","Move page up","Move page down",pageId===draftModel.homePageId?"Delete unavailable · this is Home":"Delete page…"]
        if(mode==="setHome")return ["Set as Home in draft","Cancel"]
        if(mode==="deletePage")return ["Cancel","Delete if there are no incoming links"].concat(removalDestinations.map(p=>"Redirect incoming links to "+p.name))
        if(mode==="confirmDeletePage")return ["Delete page in draft","Cancel"]
        if(mode==="pageAppearance")return ["Background start","Background end","Component surface","Text color","Focus color"]
        if(mode==="editorAppearance")return ["Surface color","Selected surface","Text color","Supporting text","Focus color","Reset Studio colors"]
        if(mode==="more")return ["Undo","Redo","Layers","Grid and sections","Try interaction","Section scope","Studio appearance"]
        if(mode==="context") return ["Move","Width","Height","Edit component"]
        if(mode==="flowMove")return ["Move before…","Move after…","Convert to a grid…","Cancel"]
        if(mode==="moveOptions")return ["Position and size",gridMode?"Reading order before…":"Move before…",gridMode?"Reading order after…":"Move after…",sectionMode?"Move to section…":"Sections unavailable in this layout","Resize anchor…","Return to "+gestureMode]
        if(mode==="anchors")return ["Top left","Top right","Bottom left","Bottom right"]
        if(mode==="destination")return destinationSections.map(s=>s.name+(s.id===originScopeId?" · Current section":""))
        if(mode==="orderBefore" || mode==="orderAfter")return relativeItems.length?relativeItems.map(i=>i.name || i.widgetType || i.id):["No other components in this section · Back returns"]
        if(mode==="properties")return contextActions.slice(2)
        if(mode==="layers") return items.map(i=>(i.name || i.widgetType || i.id)+(i.hidden?" · Hidden":"")+(i.locked?" · Locked":""))
        if(mode==="pages") return ["Home"].concat(draftModel.pages.map(p=>p.name)).concat(["Create blank page"])
        if(mode==="add") return names
        if(mode==="collision") return ["Shift others"+(shiftPreview.valid?"":" · Unavailable"),"Swap"+(swapPreview.valid?"":" · Unavailable"),"Cancel","Review affected items…"]
        if(mode==="collisionDetails")return (collisionPreview.placements || []).map(change=>{
            const item=items.find(i=>i.id===change.id),section=page.sections.find(s=>(s.id || "")===(change.categoryId || ""))
            return (item?item.name || item.widgetType || item.id:change.id)+" → "+(sectionMode && section?section.name+" · ":"")+"row "+(change.placement.y+1)+", column "+(change.placement.x+1)+", "+change.placement.width+" × "+change.placement.height
        })
        if(mode==="geometry") return ["Column (cell): "+(candidate.x+1), "Row (cell): "+(candidate.y+1), "Width (cells): "+candidate.width, "Height (cells): "+candidate.height, "Accept placement", "Cancel"]
        if(mode==="scopes")return ["Whole page overview"].concat((page.sections || []).map(s=>s.name))
        if(mode==="convert") return ["Layout: "+((gridModes.find(m=>m.id===gridDraft.mode)||{}).name || "Unavailable"),"Columns: "+gridDraft.columns,"Rows: "+gridDraft.rows,"Gap: "+gridDraft.gap,"Margin: "+gridDraft.margin,"Section height: "+sectionHeightDraft,"Preview arrangement","Cancel"]
        if(mode==="conversionPreview") return ["Use this arrangement in draft", "Cancel"]
        if(mode==="dirty") return ["Keep editing", "Discard draft…", "Review & Apply"]
        if(mode==="discard") return ["Keep editing", "Discard all draft pages"]
        if(mode==="remove") return ["Cancel", "Remove selected component"]
        if(mode==="apply") return ["Apply all draft changes", "Keep editing"]
        if(mode==="history") return ["Keep history and cancel edit", "Release history; keep draft"]
        return []
    }
    function activateChoice() {
        if(mode==="pageContext") {
            if(choice===0){mode="canvas";toolbarFocus=false;if(!current && scopedItems.length)selectedId=scopedItems[0].id}
            else if(choice===1){mode="pageAppearance";choice=0}
            else if(choice===2)beginKeyboard("renamePage",(pageEntries.find(p=>p.id===pageId)||{}).name)
            else if(choice===3){duplicateBefore=pageEntries.map(p=>p.id);beginKeyboard("duplicatePage",(pageEntries.find(p=>p.id===pageId)||{}).name+" copy")}
            else if(choice===4){mode="setHome";choice=0;message="Home navigation and Settings colors will follow this page after Apply."}
            else if(choice===5 || choice===6)command("reorderPage",{id:pageId,offset:choice===5?-1:1})
            else if(pageId===draftModel.homePageId)message="Set another page as Home before deleting this page."
            else {mode="deletePage";choice=0;message="Choose whether incoming links must be absent or redirect them to an existing page. The page and its contents are removed together; Undo restores them."}
            return
        }
        if(mode==="setHome"){if(choice===0)command("setHomePage",{id:pageId});mode="canvas";toolbarFocus=true;return}
        if(mode==="deletePage") {
            if(choice===0){mode="pageContext";choice=7;return}
            deletionRemaps=choice>1;deletionReplacement=deletionRemaps?removalDestinations[choice-2].id:""
            message="Delete "+(pageEntries.find(p=>p.id===pageId)||{}).name+" and all its components"+(deletionRemaps?", redirecting incoming links to "+(removalDestinations.find(p=>p.id===deletionReplacement)||{}).name:" only if no incoming links remain")+". This is one undoable draft change; Apply saves it."
            mode="confirmDeletePage";choice=0;return
        }
        if(mode==="confirmDeletePage") {
            if(choice===1){mode="deletePage";choice=0;return}
            const args={id:pageId};if(deletionRemaps)args.replacementPageId=deletionReplacement
            if(command("removePage",args)){pageId=draftModel.homePageId;selectedId=pageFocus[pageId] || "";mode="canvas";toolbarFocus=true;menuIndex=0}
            return
        }
        if(mode==="pageAppearance"){const key=["backgroundStart","backgroundEnd","surfaceColor","textColor","focusColor"][choice];beginKeyboard("pageColor:"+key,String(page.appearance[key] || ""));return}
        if(mode==="more"){if(choice===6){mode="editorAppearance";choice=0;return}const index=[3,4,2,5,7,9][choice];mode="canvas";chooseTool(index);return}
        if(mode==="editorAppearance") {
            if(choice===5){command("studioAppearance",{value:{}});return}
            const key=["surfaceColor","selectedColor","textColor","mutedColor","focusColor"][choice];beginKeyboard("studioColor:"+key,String(palette[key]));return
        }
        if(mode==="context") {
            if(choice===0)beginGesture(false)
            else if(choice===1 || choice===2){const axis=choice===1?"width":"height";beginGesture(true);resizeAxis=axis}
            else {mode="properties";choice=0}
            return
        }
        if(mode==="properties"){context(choice+2);return}
        if(mode==="flowMove"){if(choice<2)beginRelative(choice===0);else if(choice===2)beginGrid();else mode="canvas";return}
        if(mode==="scopes") {
            scopeOverview=choice===0
            if(choice>0){scopeId=page.sections[choice-1].id || "";const item=items.find(i=>(i.categoryId || "")===scopeId);selectedId=item?item.id:""}
            mode="canvas";toolbarFocus=false;Qt.callLater(()=>preview.revealScope(scopeId));return
        }
        if(mode==="layers") {if(items[choice]) selectedId=items[choice].id;mode="canvas";toolbarFocus=false;return}
        if(mode==="pages") {
            if(choice===draftModel.pages.length+1) {beginKeyboard("newPage","");return}
            pageId=choice===0 ? "" : draftModel.pages[choice-1].id;selectedId="";mode="canvas";return
        }
        if(mode==="add") {addingType=choice===0 ? "clock" : "text";origin={};originScopeId=scopeId;gestureMode="move";candidate={x:0,y:0,width:1,height:1};mode="move";toolbarFocus=false;return}
        if(mode==="collision") {if(choice===2)cancelGesture();else if(choice===3){if(collisionPreview.valid){mode="collisionDetails";choice=0;message="Proposed positions only · Back returns to Shift / Swap / Cancel. Nothing has changed."}else message=collisionPreview.reason}else if(collisionPreview.valid)acceptCandidate(choice===0 ? "shift":"swap");else message=collisionPreview.reason;return}
        if(mode==="collisionDetails")return
        if(mode==="moveOptions") {
            if(choice===0){mode="geometry";choice=0}
            else if(choice===1 || choice===2)beginRelative(choice===1)
            else if(choice===3){if(sectionMode){mode="destination";choice=0}else message="Choose a scrolling section layout to move between sections."}
            else if(choice===4){if(gestureMode==="resize"){mode="anchors";choice=0}else message="Choose Width or Height to change the resize anchor."}
            else mode=gestureMode
            return
        }
        if(mode==="anchors"){resizeAnchor=["top-left","top-right","bottom-left","bottom-right"][choice];mode="resize";message="";return}
        if(mode==="destination") {
            if(!destinationSections[choice])return
            scopeId=destinationSections[choice].id || "";scopeOverview=false;candidate=Object.assign({},candidate,{x:0,y:0});mode="move"
            message="Move to "+destinationSections[choice].name+" · choose a cell, then Confirm. Back restores the original section."
            Qt.callLater(()=>preview.revealScope(scopeId));return
        }
        if(mode==="orderBefore" || mode==="orderAfter") {
            if(relativeItems[choice] && command("orderRelative",{id:selectedId,relativeId:relativeItems[choice].id,before:mode==="orderBefore"})){mode="canvas";toolbarFocus=false}
            return
        }
        if(mode==="geometry") {
            if(choice===5)cancelGesture()
            else if(choice===4)acceptCandidate()
            else beginKeyboard("geometry"+choice,String(choice===0?candidate.x+1:choice===1?candidate.y+1:choice===2?candidate.width:candidate.height))
            return
        }
        if(mode==="convert") {
            if(choice===7)mode="canvas"
            else if(choice===0){const index=gridModes.findIndex(m=>m.id===gridDraft.mode);gridDraft=Object.assign({},gridDraft,{mode:gridModes[(index+1)%gridModes.length].id})}
            else if(choice<6){const field=gridField(choice);beginKeyboard("grid:"+field.key,String(field.key==="sectionHeight"?sectionHeightDraft:gridDraft[field.key]))}
            else if(session.previewCommand("convertGrid",{value:gridDraft,sectionHeight:sectionHeightDraft},pageId)){mode="conversionPreview";choice=0;message="Preview the complete grid arrangement. Section layouts retain named sections, library widgets and section actions. Apply remains a separate step."}else message=session.errorString
            return
        }
        if(mode==="conversionPreview") {if(choice===0)session.acceptPreview();else session.cancelPreview();mode="canvas";return}
        if(mode==="dirty") {mode=choice===0 ? "canvas" : choice===1 ? "discard":"apply";choice=0;return}
        if(mode==="discard") {if(choice===1)close();else mode="canvas";return}
        if(mode==="remove") {if(choice===1 && command("remove",{id:selectedId}))selectedId=items.length ? items[0].id:"";mode="canvas";return}
        if(mode==="apply") {if(choice===1)mode="canvas";else if(preview.layoutError)message=preview.layoutError;else if(session.apply(selectedId)){mode="canvas";message="Applied successfully. All pages saved."}else message=session.errorString;return}
        if(mode==="history") {if(choice===1)session.continueWithNewHistoryBaseline();mode="canvas"}
    }
    function handleCommand(c) {
        if (!open) return
        if (!session || !session.active) {close();return}
        if(mode==="keyboard") {keyboard.handleCommand(c);return}
        if(mode==="preview") {if(c===inputManager.back)mode="canvas";else preview.handleCommand(c);return}
        if(c===inputManager.back) {
            if(mode==="collisionDetails"){mode="collision";choice=collisionStrategy==="shift"?0:1;return}
            if(["move","resize","collision","geometry","moveOptions","destination","anchors"].indexOf(mode)>=0){cancelGesture();return}
            if(mode!=="canvas") {if(mode==="conversionPreview")session.cancelPreview();mode="canvas";addingType="";return}
            if(!toolbarFocus){toolbarFocus=true;return}
            if(session.dirty){mode="dirty";choice=0}else close()
            return
        }
        if(mode==="move" || mode==="resize") {
            if(c===inputManager.confirm){acceptCandidate();return}
            if(c===inputManager.menu){gestureMode=mode;mode="moveOptions";choice=0;return}
            const dx=c===inputManager.navigateRight?1:c===inputManager.navigateLeft?-1:0
            const dy=c===inputManager.navigateDown?1:c===inputManager.navigateUp?-1:0
            let p=Object.assign({},candidate)
            if(mode==="move"){p.x+=dx;p.y+=dy}
            else if(resizeAxis==="width")p.width+=dx
            else if(resizeAxis==="height")p.height+=dx
            else{p.width+=dx;p.height+=dy}
            if(mode==="resize") {
                if(resizeAnchor.indexOf("right")>=0)p.x-=p.width-candidate.width
                if(resizeAnchor.indexOf("bottom")>=0)p.y-=p.height-candidate.height
            }
            if(p.x<0 || p.y<0 || p.width<1 || p.height<1 || p.x+p.width>grid.columns || p.y+p.height>grid.rows){message="Grid boundary: choose a position or size inside the grid.";return}
            candidate=p;preview.revealPlacement(p,scopeId);message="";return
        }
        if(mode!=="canvas") {
            const labels=dialogLabels()
            if(mode==="convert" && choice<6 && (c===inputManager.navigateLeft || c===inputManager.navigateRight)) {
                const direction=c===inputManager.navigateRight?1:-1
                if(choice===0){const index=gridModes.findIndex(m=>m.id===gridDraft.mode);const next=index+direction;if(next>=0 && next<gridModes.length)gridDraft=Object.assign({},gridDraft,{mode:gridModes[next].id});return}
                const field=gridField(choice),current=field.key==="sectionHeight"?sectionHeightDraft:gridDraft[field.key],number=current+direction*field.step
                if(number<field.min || number>field.max){message="Choose "+field.min+"–"+field.max+" for "+field.key+".";return}
                if(field.key==="sectionHeight")sectionHeightDraft=number
                else {let next=Object.assign({},gridDraft);next[field.key]=number;gridDraft=next}return
            }
            if(c===inputManager.navigateDown || c===inputManager.navigateRight)choice=Math.min(labels.length-1,choice+1)
            else if(c===inputManager.navigateUp || c===inputManager.navigateLeft)choice=Math.max(0,choice-1)
            else if(c===inputManager.confirm)activateChoice()
            return
        }
        if(c===inputManager.menu){if(current){mode="context";choice=0}else toolbarFocus=true;return}
        if(c===inputManager.search){chooseTool(0);return}
        if(toolbarFocus) {
            if(c===inputManager.navigateUp)menuIndex=Math.max(0,menuIndex-1)
            else if(c===inputManager.navigateDown)menuIndex=Math.min(menuEntries.length-1,menuIndex+1)
            else if(c===inputManager.navigateRight){toolbarFocus=false;if(!current && scopedItems.length)selectedId=scopedItems[0].id}
            else if(c===inputManager.confirm)activateMenu(menuIndex)
        } else {
            if(c===inputManager.confirm){if(current){mode="context";choice=0}else chooseTool(0);return}
            const index=items.findIndex(i=>i.id===selectedId)
            if(gridMode && current && current.placement) {
                const p=current.placement, cx=p.x+p.width/2, cy=p.y+p.height/2
                const dx=c===inputManager.navigateRight?1:c===inputManager.navigateLeft?-1:0
                const dy=c===inputManager.navigateDown?1:c===inputManager.navigateUp?-1:0
                let best=null, score=Infinity
                for(const item of scopedItems) {
                    if(item.id===selectedId || !item.placement || item.hidden)continue
                    const q=item.placement, x=q.x+q.width/2-cx, y=q.y+q.height/2-cy
                    if((dx && x*dx<=0) || (dy && y*dy<=0))continue
                    const distance=(dx?Math.abs(x):Math.abs(y))*1000+(dx?Math.abs(y):Math.abs(x))
                    if(distance<score){score=distance;best=item}
                }
                if(best)selectedId=best.id
                else if(dy<0)toolbarFocus=true
                else message="Canvas boundary. Menu opens editing tools."
            } else if(c===inputManager.navigateUp)toolbarFocus=true
            else if(items.length)selectedId=items[Math.max(0,Math.min(items.length-1,index+(c===inputManager.navigateLeft?-1:1)))].id
        }
    }

    Rectangle {anchors.fill:parent;color:palette.surfaceColor || layoutModel.backgroundEnd}
    Column {
        id:editorColumn
        z:1
        anchors.fill:parent;anchors.margins:studio.spacing;spacing:studio.spacing/2
        Rectangle {
            id:pageRail;objectName:"studioPageMenu"
            width:Math.min(studio.width-studio.spacing*2,studio.layoutModel.studioOptions.menuWidth*studio.textScale)
            height:editorColumn.height
            visible:studio.toolbarFocus && studio.mode==="canvas"
            color:studio.palette.surfaceColor;radius:studio.spacing/2
            Column {
                anchors.fill:parent;anchors.margins:studio.spacing;spacing:studio.spacing/2
                Text {text:"Page Studio";color:studio.palette.textColor;font.pixelSize:26*studio.textScale}
                Text {id:menuStatus;width:parent.width;text:session && session.errorString?"Edit refused: "+session.errorString:session && session.dirty?"Draft · "+session.changeCount+" changes":"Your interface";wrapMode:Text.Wrap;color:studio.palette.textColor;font.pixelSize:16*studio.textScale}
                ListView {
                    width:parent.width;height:Math.max(100,pageRail.height-150*studio.textScale-menuStatus.implicitHeight);clip:true
                    model:studio.menuEntries;currentIndex:studio.menuIndex;highlightRangeMode:ListView.ApplyRange
                    delegate:Rectangle {
                        required property int index;required property var modelData
                        width:ListView.view.width;height:48*studio.textScale;radius:studio.spacing/3
                        color:studio.menuIndex===index?studio.palette.selectedColor:"transparent"
                        border.width:studio.menuIndex===index?2:0;border.color:studio.palette.focusColor
                        Text {anchors.fill:parent;anchors.margins:studio.spacing/2;verticalAlignment:Text.AlignVCenter;text:modelData.label;color:studio.palette.textColor;font.pixelSize:18*studio.textScale;elide:Text.ElideRight}
                        MouseArea {anchors.fill:parent;onClicked:studio.activateMenu(index)}
                    }
                }
            }
        }
        Item {
            id:canvasArea;parent:studio;anchors.fill:parent;clip:true
            Item {
                id:camera;property real zoom:1
                width:studio.width;height:studio.height
                scale:Math.min(canvasArea.width/width,canvasArea.height/height)*zoom
                transformOrigin:Item.TopLeft
                x:(canvasArea.width-width*scale)/2;y:(canvasArea.height-height*scale)/2
                HomeScreen {
                    id:preview;objectName:"studioPreview"
                    x:draftNavigation.contentLeft;y:draftNavigation.contentTop;width:parent.width-x;height:parent.height-y
                    items:studio.page?studio.page.items.filter(i=>!i.hidden || (studio.mode!=="preview" && i.id===studio.selectedId)):[]
                    sections:studio.page?studio.page.sections.map(s=>Object.assign({},s,{items:(s.items || []).filter(i=>!i.hidden || (studio.mode!=="preview" && i.id===studio.selectedId))})):[]
                    appearance:studio.page?studio.page.appearance:studio.palette;composition:studio.page?studio.page.composition:({})
                    grid:studio.pageGrid;placements:studio.page?studio.page.placements:[]
                    editMode:studio.mode!=="preview";scopeView:studio.sectionMode && !studio.scopeOverview;editScopeId:studio.scopeId
                    editSelection:studio.gridMode?"":studio.selectedId
                    suppressedComponentIds:(studio.mode==="collision" || studio.mode==="collisionDetails") && studio.collisionPreview.valid?studio.collisionPreview.placements.map(p=>p.id):studio.manipulating && !studio.addingType && studio.current?[studio.selectedId]:[]
                    onEditSelected:componentId=>{if(studio.mode!=="canvas")return;studio.selectedId=componentId;studio.toolbarFocus=false;studio.mode="context";studio.choice=0}
                    brandVisible:studio.page?studio.page.brandVisible:false;tileRoundness:studio.page?studio.page.tileRoundness:0
                    backgroundStart:studio.page?studio.page.backgroundStart:studio.layoutModel.backgroundStart
                    backgroundEnd:studio.page?studio.page.backgroundEnd:studio.layoutModel.backgroundEnd
                    wallpaperSource:studio.page?studio.page.wallpaperSource:""
                    internetAvailable:false;localNetworkAvailable:false;hasContextualSources:false
                    onActionActivated:(action,originId)=>studio.message="Preview only: would invoke "+action.type
                    onSettingsRequested:section=>studio.message="Return to editing to change settings."
                    onSearchRequested:studio.message="Search is unavailable in draft preview."
                }
                PageNavigation {
                    id:draftNavigation;anchors.fill:parent;enabledNavigation:studio.pageId!==""
                    composition:studio.page?studio.page.composition:({});appearance:studio.page?studio.page.appearance:({})
                    pages:studio.pageEntries;activeId:studio.pageId
                    selectedPage:Math.max(0,studio.pageEntries.findIndex(p=>p.id===studio.pageId))
                    onChosen:index=>{studio.message="Page navigation · use the Pages menu to choose an editing page."}
                }
                Item {
                    x:preview.x;y:preview.y;width:preview.width;height:preview.height;visible:studio.mode!=="preview"
                    Rectangle {
                        visible:!studio.gridMode && !!preview.editRect && !studio.toolbarFocus
                        x:preview.editRect?preview.editRect.x:0;y:preview.editRect?preview.editRect.y:0
                        width:preview.editRect?preview.editRect.width:0;height:preview.editRect?preview.editRect.height:0
                        color:"transparent";border.width:7;border.color:studio.palette.surfaceColor
                        Rectangle {anchors.fill:parent;anchors.margins:2;color:"transparent";border.width:3;border.color:studio.palette.focusColor}
                        Text {anchors.bottom:parent.top;width:parent.width;text:studio.current?studio.current.name || studio.current.widgetType || studio.current.id:"";color:studio.palette.textColor;style:Text.Outline;styleColor:studio.palette.surfaceColor;elide:Text.ElideRight;font.pixelSize:16*studio.textScale}
                    }
                    Repeater {
                        model:studio.gridMode && (studio.mode==="move" || studio.mode==="resize" || studio.mode==="geometry") ? studio.grid.columns*studio.grid.rows : 0
                        delegate:Rectangle {
                            required property int index
                            readonly property var rect:preview.placementRectFor({x:index%studio.grid.columns,y:Math.floor(index/studio.grid.columns),width:1,height:1},studio.scopeId)
                            x:rect.x;y:rect.y;width:rect.width;height:rect.height;color:"transparent";border.width:1;border.color:studio.palette.mutedColor;opacity:0.35
                        }
                    }
                    Repeater {
                        model:studio.gridMode ? studio.scopedItems.filter(i=>!!i.placement && (!i.hidden || i.id===studio.selectedId)):[]
                        delegate:Rectangle {
                            required property var modelData
                            readonly property var rect:preview.placementRectFor(modelData.placement,modelData.categoryId || studio.scopeId)
                            x:rect.x;y:rect.y;width:rect.width;height:rect.height;color:"transparent";border.width:modelData.id===studio.selectedId && !studio.toolbarFocus && !studio.manipulating?7:0;border.color:studio.palette.surfaceColor
                            Rectangle {anchors.fill:parent;anchors.margins:2;visible:modelData.id===studio.selectedId && !studio.toolbarFocus && !studio.manipulating;color:"transparent";border.width:3;border.color:studio.palette.focusColor}
                            Text {visible:modelData.id===studio.selectedId && !studio.toolbarFocus && !studio.manipulating;anchors.bottom:parent.top;width:Math.min(studio.width-2*studio.spacing,Math.max(parent.width,200*studio.textScale));text:studio.componentName(modelData)+(modelData.hidden?" · Hidden":"")+(modelData.locked?" · Locked":"");color:studio.palette.textColor;style:Text.Outline;styleColor:studio.palette.surfaceColor;font.pixelSize:16*studio.textScale;elide:Text.ElideRight}
                            MouseArea {
                                objectName:"studioHit-"+modelData.id
                                anchors.fill:parent
                                property point start
                                property bool dragging:false
                                onPressed:mouse=>{if(studio.mode!=="canvas"){mouse.accepted=false;return}studio.selectedId=modelData.id;studio.toolbarFocus=false;dragging=false;start=Qt.point(mouse.x,mouse.y)}
                                onPositionChanged:mouse=>{
                                    if(!pressed)return
                                    if(!dragging){if(Math.abs(mouse.x-start.x)+Math.abs(mouse.y-start.y)<Qt.styleHints.startDragDistance)return;if(studio.current && studio.current.locked){studio.message="Unlock this component before moving it.";return}studio.beginGesture(false);dragging=true}
                                    let p=Object.assign({},studio.origin)
                                    p.x+=Math.round((mouse.x-start.x)/(preview.gridMetricsFor(studio.scopeId).cellW+preview.gridMetricsFor(studio.scopeId).gap))
                                    p.y+=Math.round((mouse.y-start.y)/(preview.gridMetricsFor(studio.scopeId).cellH+preview.gridMetricsFor(studio.scopeId).gap))
                                    if(p.x>=0 && p.y>=0 && p.x+p.width<=studio.grid.columns && p.y+p.height<=studio.grid.rows)studio.candidate=p
                                }
                                onReleased:{if(dragging)studio.acceptCandidate();else{studio.mode="context";studio.choice=0}}
                                onCanceled:studio.mode="canvas"
                            }
                        }
                    }
                    Rectangle {
                        objectName:"studioOrigin"
                        visible:!studio.addingType && !!studio.current && (studio.mode==="move" || studio.mode==="resize" || studio.mode==="collision" || studio.mode==="geometry")
                        readonly property var rect:studio.origin.x!==undefined?preview.placementRectFor(studio.origin,studio.originScopeId):({x:0,y:0,width:0,height:0})
                        x:rect.x;y:rect.y;width:rect.width;height:rect.height;color:"transparent";border.width:2;border.color:studio.palette.mutedColor
                    }
                    Rectangle {
                        objectName:"studioGhost"
                        visible:studio.mode==="move" || studio.mode==="resize" || studio.mode==="geometry" || (studio.mode==="collision" && !studio.collisionPreview.valid)
                        readonly property var rect:preview.placementRectFor(studio.candidate,studio.scopeId)
                        x:rect.x;y:rect.y;width:rect.width;height:rect.height;color:"transparent";border.width:4;border.color:studio.palette.focusColor
                        readonly property var sourceSection:studio.page?(studio.page.sections || []).find(s=>(s.id || "")===studio.scopeId):null
                        HomeCard {objectName:"studioGhostCard";anchors.fill:parent;enabled:false;unit:preview.componentUnit;outsideColor:studio.page?studio.page.backgroundEnd:studio.layoutModel.backgroundEnd;item:Object.assign({},studio.addingType?{kind:"widget",widgetType:studio.addingType,name:studio.addingType}:studio.current || {},{presentation:Object.assign({},parent.sourceSection?parent.sourceSection.presentation || {}:{},studio.current && !studio.addingType?studio.current.presentation || {}:{})});appearance:studio.page?studio.page.appearance:studio.palette;selected:true;opacity:studio.current && !studio.addingType && studio.current.opacity!==undefined?studio.current.opacity:1}
                        Text {anchors.bottom:parent.top;width:Math.min(studio.width-2*studio.spacing,Math.max(parent.width,200*studio.textScale));x:Math.max(studio.spacing-preview.x-parent.x,Math.min(0,studio.width-preview.x-parent.x-width-studio.spacing));text:(studio.mode==="resize"?"Sizing "+studio.resizeAxis:"Moving")+" · "+studio.componentName(studio.addingType?null:studio.current);color:studio.palette.textColor;style:Text.Outline;styleColor:studio.palette.surfaceColor;font.pixelSize:18*studio.textScale;elide:Text.ElideRight;Accessible.role:Accessible.StaticText;Accessible.name:studio.operationSummary}
                    }
                    Repeater {
                        model:studio.collisionPreview.valid ? studio.collisionPreview.placements : []
                        delegate:Rectangle {
                            required property var modelData
                            required property int index
                            readonly property var previewItem:studio.items.find(i=>i.id===modelData.id) || {id:modelData.id,kind:"widget",widgetType:studio.addingType,name:studio.addingType}
                            objectName:"studioProposal-"+modelData.id
                            readonly property var section:studio.page.sections.find(s=>(s.id || "")===(modelData.categoryId || ""))
                            readonly property var rect:preview.placementRectFor(modelData.placement,modelData.categoryId!==undefined?modelData.categoryId:studio.scopeId)
                            x:rect.x;y:rect.y;width:rect.width;height:rect.height;color:"transparent"
                            HomeCard {objectName:"studioProposedCard-"+parent.modelData.id;anchors.fill:parent;enabled:false;unit:preview.componentUnit;outsideColor:studio.page.backgroundEnd;item:Object.assign({},parent.previewItem,{presentation:Object.assign({},parent.section?parent.section.presentation || {}:{},parent.previewItem.presentation || {})});appearance:studio.page.appearance;opacity:parent.previewItem.opacity===undefined?1:parent.previewItem.opacity}
                            Rectangle {anchors.fill:parent;color:"transparent";border.width:3;border.color:studio.palette.focusColor}
                            Text {anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:7;text:(index+1)+" · "+studio.componentName(parent.previewItem);color:studio.palette.textColor;style:Text.Outline;styleColor:studio.palette.surfaceColor;font.pixelSize:15*studio.textScale;elide:Text.ElideRight}
                        }
                    }
                }
            }
            Text {anchors.centerIn:parent;visible:!studio.items.length && studio.mode!=="preview";text:"Empty page · Confirm to add a component";color:studio.palette.textColor;font.pixelSize:26*studio.textScale}
        }
        Text {parent:studio;anchors.bottom:parent.bottom;anchors.left:parent.left;anchors.margins:studio.spacing;z:2;width:parent.width-2*studio.spacing;visible:!studio.toolbarFocus && studio.mode!=="preview";text:studio.footerText;color:studio.palette.textColor;style:Text.Outline;styleColor:studio.palette.surfaceColor;wrapMode:Text.Wrap;font.pixelSize:17*studio.textScale}
    }
    Rectangle {
        visible:studio.mode!=="canvas" && studio.mode!=="move" && studio.mode!=="resize" && studio.mode!=="preview"
        z:3
        width:Math.min(parent.width-2*studio.spacing,560*studio.textScale);height:Math.min(parent.height-2*studio.spacing,dialogColumn.implicitHeight+2*studio.spacing)
        readonly property var localSelection:studio.gridMode && studio.current && studio.current.placement?preview.placementRectFor(studio.current.placement,studio.scopeId):preview.editRect
        readonly property var selectionRect:localSelection?Object.assign({},localSelection,{x:preview.x+localSelection.x,y:preview.y+localSelection.y}):null
        x:selectionRect && (studio.mode==="context" || studio.mode==="properties")?(selectionRect.x+selectionRect.width+width+2*studio.spacing<=studio.width?selectionRect.x+selectionRect.width+studio.spacing:Math.max(studio.spacing,selectionRect.x-width-studio.spacing)):(parent.width-width)/2
        y:selectionRect && (studio.mode==="context" || studio.mode==="properties")?Math.max(studio.spacing,Math.min(parent.height-height-studio.spacing,selectionRect.y)):(parent.height-height)/2
        color:studio.palette.surfaceColor;border.width:3;border.color:studio.palette.focusColor;radius:studio.spacing
        Column {
            id:dialogColumn;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:studio.spacing;spacing:studio.spacing/2
            Text {id:dialogHeading;width:parent.width;text:studio.operationTitle?studio.operationTitle:studio.mode==="context" || studio.mode==="properties"?(studio.current?studio.current.name || studio.current.widgetType || "Component":"Component"):studio.mode==="pageContext"?(studio.pageEntries.find(p=>p.id===studio.pageId)||{}).name:studio.mode==="pageAppearance"?"Page appearance":studio.mode==="more"?"More tools":studio.mode.toUpperCase();color:studio.palette.textColor;font.pixelSize:24*studio.textScale;elide:Text.ElideRight}
            Text {id:dialogMessage;width:parent.width;text:studio.mode==="apply" ? "Save the complete interface draft: "+(session?session.changeCount:0)+" operations across Home and pages." : studio.mode==="remove" ? "Remove "+studio.selectedId+"? This stays undoable until Apply." : studio.mode==="collision" && studio.choice!==2 ? (studio.collisionPreview.valid ? "Preview: "+studio.collisionPreview.placements.length+" affected components. Confirm commits one undoable draft operation." : "Unavailable: "+studio.collisionPreview.reason) : studio.message;color:studio.palette.mutedColor;wrapMode:Text.Wrap;font.pixelSize:18*studio.textScale}
            ListView {
                width:parent.width;height:Math.min(studio.dialogLabels().length*42*studio.textScale,Math.max(42*studio.textScale,studio.height-dialogHeading.implicitHeight-dialogMessage.implicitHeight-6*studio.spacing))
                clip:true;currentIndex:studio.choice;highlightRangeMode:ListView.ApplyRange
                model:studio.mode==="keyboard" ? []:studio.dialogLabels()
                delegate:Rectangle {
                    required property int index;required property string modelData
                    readonly property bool available:studio.mode!=="collision" || index>1 || (index===0?!!studio.shiftPreview.valid:!!studio.swapPreview.valid)
                    width:dialogColumn.width;height:42*studio.textScale;color:studio.choice===index?studio.palette.selectedColor:"transparent";border.width:studio.choice===index?2:0;border.color:studio.palette.focusColor
                    Text {anchors.fill:parent;anchors.margins:8;text:modelData;color:parent.available?studio.palette.textColor:studio.palette.mutedColor;font.pixelSize:18*studio.textScale;elide:Text.ElideRight}
                    Accessible.role:Accessible.Button;Accessible.name:modelData;Accessible.description:available?"":"Unavailable. Choose another proposal or Cancel."
                    MouseArea {anchors.fill:parent;onClicked:{studio.choice=index;studio.activateChoice()}}
                }
            }
        }
    }
    OnScreenKeyboard {
        id:keyboard;objectName:"studioKeyboard";visible:studio.mode==="keyboard";anchors.fill:parent;z:10;appearance:studio.palette;hint:studio.message
        onCanceled:studio.mode=studio.keyboardReturnMode
        onAccepted:text=>{
            if(studio.keyboardAction.indexOf("studioColor:")===0) {
                const key=studio.keyboardAction.slice(12),value=Object.assign({},studio.palette,{[key]:text})
                if(studio.command("studioAppearance",{value:value}))studio.mode="editorAppearance"
                return
            }
            if(studio.keyboardAction==="duplicatePage") {
                if(studio.command("duplicatePage",{id:studio.pageId,name:text})){
                    studio.pageId=studio.pageEntries.find(p=>studio.duplicateBefore.indexOf(p.id)<0).id
                    studio.selectedId="";studio.mode="pageContext";studio.choice=0
                }
                return
            }
            if(studio.keyboardAction==="renamePage") {
                if(studio.command("renamePage",{id:studio.pageId,name:text}))studio.mode="pageContext"
                return
            }
            if(studio.keyboardAction.indexOf("pageColor:")===0) {
                const key=studio.keyboardAction.slice(10),value=Object.assign({},studio.page.appearance,{[key]:text})
                if(studio.command("appearance",{value:value}))studio.mode="pageAppearance"
                return
            }
            if(studio.keyboardAction.indexOf("grid:")===0) {
                const key=studio.keyboardAction.slice(5),field=studio.gridField(studio.choice),number=Number(text)
                if(!text.trim() || !Number.isFinite(number) || (field.integer && !Number.isInteger(number)) || number<field.min || number>field.max){studio.message="Enter a value between "+field.min+" and "+field.max+".";return}
                if(key==="sectionHeight")studio.sectionHeightDraft=number
                else studio.gridDraft=Object.assign({},studio.gridDraft,{[key]:number})
                studio.mode="convert";return
            }
            if(studio.keyboardAction.indexOf("geometry")===0) {
                const field=Number(studio.keyboardAction.slice(8)), number=Number(text)
                if(!Number.isInteger(number) || number<1){studio.message="Enter a positive whole number.";return}
                let p=Object.assign({},studio.candidate)
                if(field===0)p.x=number-1
                else if(field===1)p.y=number-1
                else if(field===2)p.width=number
                else p.height=number
                if(p.x+p.width>studio.grid.columns || p.y+p.height>studio.grid.rows){studio.message="The placement exceeds the grid.";return}
                studio.candidate=p;studio.mode="geometry";return
            }
            if(studio.keyboardAction==="rename")studio.command("rename",{id:studio.selectedId,name:text})
            else if(studio.keyboardAction==="newPage"){
                if(!studio.command("newPage",{name:text}))return
                studio.pageId=studio.draftModel.pages[studio.draftModel.pages.length-1].id
                studio.selectedId="";studio.toolbarFocus=false
            }
            else {
                let p=Object.assign({},studio.current?studio.current.presentation || {}:{})
                if(studio.keyboardAction==="text"){p.variant="text";p.subtitle=text}
                else if(studio.keyboardAction==="opacity")p.surfaceOpacity=Number(text)
                else if(studio.keyboardAction==="corners")p.radius=Number(text)
                studio.command("presentation",{id:studio.selectedId,value:p})
            }
            studio.mode="canvas"
        }
    }
    Connections {
        target:studio.session
        function onChanged(){
            if(studio.session && studio.session.errorString.indexOf("History budget")===0){studio.mode="history";studio.choice=0}
            Qt.callLater(()=>{
                if(!studio.open || !studio.session || !studio.session.active)return
                if(!studio.pageEntries.some(p=>p.id===studio.pageId)){
                    studio.pageId=studio.draftModel.homePageId;studio.selectedId=studio.pageFocus[studio.pageId] || ""
                    studio.toolbarFocus=true;studio.mode="canvas"
                }
                if(studio.selectedId && !studio.current)studio.selectedId=studio.scopedItems.length?studio.scopedItems[0].id:""
                if(studio.mode==="canvas" && studio.sectionMode && studio.current && !studio.scopeOverview){studio.scopeId=studio.current.categoryId || "";preview.revealPlacement(studio.current.placement,studio.scopeId)}
            })
        }
    }
    Text {
        anchors.top:parent.top;anchors.horizontalCenter:parent.horizontalCenter;anchors.topMargin:studio.spacing
        visible:studio.mode==="preview";z:120
        text:"Draft preview · Back returns to Studio · "+studio.message
        color:studio.palette.textColor;style:Text.Outline;styleColor:studio.palette.surfaceColor
        font.pixelSize:18*studio.textScale;width:parent.width-2*studio.spacing;wrapMode:Text.Wrap
    }
    states: State {
        name:"fullPreview";when:studio.mode==="preview"
        ParentChange {target:camera;parent:studio;x:0;y:0}
        PropertyChanges {target:camera;scale:1}
        PropertyChanges {target:editorColumn;visible:false}
    }
}
