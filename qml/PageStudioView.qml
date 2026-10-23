// PageStudioView.qml — remote-first page editor: canvas, context menu, dialogs, preview/apply.
import QtQuick
import QtQuick.Controls

Item {
    id: studio
    objectName: "pageStudio"
    property var layoutModel
    property var provider:null
    property var session: null
    property bool open: false
    property string pageId: ""
    property string scopeId:""
    property string parentScopeId:""
    property string originParentId:""
    property var expandedGroups:({})
    property var multiSelection:[]
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
    property var addingConfiguration:({})
    property bool inspectorReturn:false
    property bool samplePreview:false
    property string structureType:""
    property string structureId:""
    property var navSections:[]
    property string resizeAxis:"both"
    property string resizeAnchor:"top-left"
    property string gestureMode:"move"
    property string originScopeId:""
    property bool originScopeOverview:true
    property string collisionStrategy:"shift"
    property string deletionReplacement:""
    property bool deletionRemaps:false
    property var duplicateBefore:[]
    property var pageFocus:({})
    property var candidate: ({x:0,y:0,width:1,height:1})
    property var flowIds: []
    property int flowOriginIndex: 0
    property int flowTargetIndex: 0
    property var flowTargets: []
    property string flowContainerId:""
    property string flowAxis:"x"
    property bool flowPreviewValid:true
    property string deletedSelection: ""
    property bool sizeControlsVisible: true
    property bool pendingScrollEnable:false
    property bool scrollingDraft:false
    property var scrollingViewport:({})
    property var origin: ({})
    property var gestureViewport: ({})
    property var proposal: ({})
    property var gridDraft: ({})
    property var gridBaselineComposition:({})
    property bool gridWasPlaced:false
    property bool gridPreviewValid:true
    property int gridPreviewSerial:0
    readonly property bool gridEditing:["convert","conversionPreview","gridExact"].indexOf(mode)>=0 || mode==="keyboard" && keyboardAction.indexOf("grid:")===0
    readonly property bool gridGeometryPending:gridEditing && !!session && !!session.proposalModel && !preview.editorGeometryReady
    readonly property string gridGeometryError:gridEditing && session && session.proposalModel && preview.editorGeometryReady?preview.layoutError:""
    onGridGeometryErrorChanged: {
        if(!gridGeometryError)return
        const serial=gridPreviewSerial
        Qt.callLater(()=>{
            if(serial!==gridPreviewSerial || !gridEditing || !session || !session.proposalModel || !preview.editorGeometryReady || !preview.layoutError)return
            message=preview.layoutError;gridPreviewValid=false;session.cancelPreview()
        })
    }
    property var childLayoutDraft:({})
    property bool childLayoutValid:true
    property bool childLayoutFromInspector:false
    property bool arrangementValid:false
    property int arrangementReturnChoice:0
    readonly property var arrangementModes:["left","center-x","right","top","center-y","bottom","distribute-x","distribute-y"]
    readonly property var arrangementNames:["Left","Horizontal center","Right","Top","Vertical center","Bottom","Horizontal spacing","Vertical spacing"]
    property string keyboardAction: ""
    property string keyboardReturnMode: "context"
    property var palette: draftModel?draftModel.studioAppearance:layoutModel ? layoutModel.studioAppearance : ({})
    readonly property var draftModel: session ? ((gridEditing || mode==="flowMove" || mode==="scrolling" || mode==="arrange" || mode==="childLayout" || mode==="inspector" && inspector.previewActive) && session.proposalModel ? session.proposalModel : session.model) : null
    readonly property var page: draftModel ? (pageId ? draftModel.pageModel(pageId) : draftModel) : null
    readonly property var items: page ? page.items : []
    readonly property var pageGrid:page?page.grid:({})
    readonly property bool scrollingEnabled:!sectionMode && (pendingScrollEnable || !!(page && page.composition.verticalScrolling))
    readonly property int placementRows:scrollingEnabled && !parentScopeId?layoutModel.studioOptions.maxScrollableRows:grid.rows || 1
    readonly property bool sectionMode:gridMode && pageGrid.mode==="sections"
    readonly property var selectedSection:page?(page.sections || []).find(s=>(s.id || "")===scopeId):null
    readonly property var parentGroup:items.find(i=>i.id===parentScopeId) || null
    readonly property var grid:parentGroup?Object.assign({},parentGroup.presentation.layout,{margin:parentGroup.presentation.layout.padding}):sectionMode && selectedSection?Object.assign({},pageGrid,(selectedSection.presentation || {}).grid || {}):pageGrid
    readonly property var scopedItems:items.filter(i=>(i.parentId || "")===parentScopeId && (!sectionMode || scopeOverview || parentScopeId || (i.categoryId || "")===scopeId))
    // Keep pointer delegates alive while the separate proposal model reflows.
    readonly property var pointerItems: {
        if(!session)return []
        const model=pageId?session.model.pageModel(pageId):session.model
        return model?model.items.filter(i=>(i.parentId || "")===parentScopeId && (!sectionMode || scopeOverview || parentScopeId || (i.categoryId || "")===scopeId)):[]
    }
    readonly property var layerRows: {
        const rows=[]
        function append(parent,depth){for(const item of items.filter(i=>(i.parentId || "")===parent)){rows.push({item:item,depth:depth});if(item.componentType==="container" && expandedGroups[item.id])append(item.id,depth+1)}}
        append("",0);return rows
    }
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
    readonly property var contextActions: ["Move", "Resize", "Rename", "Duplicate", "Remove", "Text", "Surface opacity", "Corners", "Position and size","Hide / show","Lock / unlock",gridMode?"Reading order before…":"Move before…",gridMode?"Reading order after…":"Move after…","Move to section…","Edit group contents","Ungroup","Move to group…","Select / deselect for grouping","Group internal layout…"]
    readonly property var names: ["Clock", "Text", "Group", "Input slots (4 × 2)"]
    readonly property var current: items.find(i=>i.id===selectedId) || null
    readonly property var addPrototype:addingType && session?session.componentPrototype(addingType,addingConfiguration):({})
    readonly property bool manipulating:["move","flowMove","resize","size","collision","collisionDetails","geometry"].indexOf(mode)>=0
    function componentName(item) {
        if(!item)return addingType?addingType.charAt(0).toUpperCase()+addingType.slice(1):"Component"
        return item.name || (item.widgetType?item.widgetType.charAt(0).toUpperCase()+item.widgetType.slice(1):item.id)
    }
    function editorReveals(item){if(!item.hidden)return true;if(mode==="preview")return false;let id=selectedId;for(let depth=0;id && depth<=4;++depth){if(id===item.id)return true;const node=items.find(i=>i.id===id);id=node?node.parentId || "":""}return false}
    readonly property string operationSummary:componentName(addingType?null:current)+" · row "+(candidate.y+1)+", column "+(candidate.x+1)+" · "+candidate.width+" × "+candidate.height+" cells"
    readonly property string operationTitle:({noRoom:"No room",flowMove:"Move in this layout",moveOptions:"Move and size",anchors:"Resize anchor",destination:"Choose a section",groupDestination:"Move into a group",orderBefore:gridMode?"Reading order before":"Move before",orderAfter:gridMode?"Reading order after":"Move after",collision:"Resolve overlap",collisionDetails:"Affected components",geometry:"Position and size",conversionPreview:"Review page layout",gridExact:"Exact layout values"})[mode] || ""
    readonly property string footerText: {
        if(mode==="flowMove")return message
        if(mode==="move" || mode==="resize")return (addingType?"Adding ":mode==="move"?(grid.overlap?"Moving layer · ":"Moving "):"Resizing ")+componentName(addingType?null:current)+(message?" · "+message:mode==="move" && collisionPreview.valid===false?" · "+collisionPreview.reason:"")
        if(mode==="collision")return breadcrumb+" · Resolve overlap · Choose a proposal or Cancel · Back cancels"
        if(mode==="collisionDetails")return breadcrumb+" · Proposed positions only · Back returns to the choices"
        return breadcrumb+(current?" · "+componentName(current):"")+(message?" · "+message:"")
    }
    readonly property var relativeItems:current?items.filter(i=>i.id!==selectedId && (i.parentId || "")===(current.parentId || "") && (i.categoryId || "")===(current.categoryId || "")):[]
    readonly property var destinationSections:sectionMode?page.sections.filter(s=>(s.presentation || {}).variant!=="library"):[]
    function groupDestinationAllowed(group) {
        if(group.componentType!=="container" || group.appCollection)return false
        let node=group
        for(let depth=0;node && depth<=4;++depth){
            if(node.id===selectedId || node.locked || node.hidden)return false
            if(!node.parentId)return true
            node=items.find(item=>item.id===node.parentId)
        }
        return false
    }
    readonly property var destinationGroups:[{id:"",name:sectionMode?"Current section":"Page"}].concat(items.filter(group=>groupDestinationAllowed(group)))
    function destinationGroupLabel(group) {
        if(!group.id)return "Move to "+group.name
        const path=[group.name]
        let root=group
        for(let depth=0;root.parentId && depth<4;++depth){
            const parent=items.find(item=>item.id===root.parentId)
            if(!parent)break
            path.unshift(componentName(parent));root=parent
        }
        if(sectionMode){const section=page.sections.find(item=>item.id===(root.categoryId || ""));if(section)path.unshift(section.name)}
        return "Move into "+path.join(" / ")
    }
    function chooseGroupDestination(index) {
        const group=destinationGroups[index]
        if(!group)return
        parentScopeId=group.id
        if(group.id && sectionMode){
            let root=group
            for(let depth=0;root.parentId && depth<4;++depth)root=items.find(item=>item.id===root.parentId) || root
            scopeId=root.categoryId || "";scopeOverview=false
        }
        candidate=Object.assign({},candidate,{x:0,y:0});mode="move"
        message=destinationGroupLabel(group)+" · Preview"
        Qt.callLater(()=>preview.revealPlacement(candidate,scopeId,parentScopeId))
    }
    readonly property var placementOptions:parentScopeId?({parentId:parentScopeId,categoryId:""}):sectionMode?({parentId:"",categoryId:scopeId}):({parentId:""})
    function collisionProposal(strategy) {
        if(strategy==="swap" && mode!=="collision" && mode!=="collisionDetails")return ({})
        if((mode!=="collision" && mode!=="collisionDetails" && mode!=="move" && mode!=="resize" && mode!=="size") || !session)return ({})
        return addingType?session.additionProposal(addingType,candidate,strategy,pageId,parentScopeId?"":sectionMode?scopeId:"",parentScopeId,addingConfiguration,pendingScrollEnable)
                         :session.placementProposal(selectedId,candidate,strategy,pageId,placementOptions)
    }
    readonly property var shiftPreview:collisionProposal("shift")
    readonly property var swapPreview:collisionProposal("swap")
    readonly property var collisionPreview: {
        if(mode==="move" || mode==="resize" || mode==="size")return shiftPreview
        if((mode!=="collision" && mode!=="collisionDetails") || (mode==="collision" && choice===2) || !session)return ({})
        return collisionStrategy==="shift"?shiftPreview:swapPreview
    }
    readonly property real textScale: palette.typographyScale || 1
    readonly property real spacing: palette.spacing || 20
    readonly property var pageEntries:draftModel?draftModel.studioPages:[]
    readonly property var removalDestinations:pageEntries.filter(p=>p.id!==pageId)
    readonly property string breadcrumb:((pageEntries.find(p=>p.id===pageId)||{}).name || "Page")+(sectionMode?" / "+(scopeOverview?"All sections":selectedSection?selectedSection.name:"Uncategorized"):"")
        +groupBreadcrumb()
    function groupBreadcrumb(){let result=[],id=parentScopeId;for(let depth=0;id && depth<4;++depth){const group=items.find(i=>i.id===id);if(!group)break;result.unshift(group.name || group.id);id=group.parentId || ""}return result.length?" / "+result.join(" / "):""}
    readonly property var menuEntries:[{label:"＋ Add page",action:"new"}].concat(pageEntries.map(p=>({label:(p.id===draftModel.homePageId?"⌂ ":"")+p.name,action:"page",id:p.id}))).concat([
        {label:"＋ Add component",action:"add"},{label:"Page appearance",action:"appearance"},{label:"More tools",action:"more"},{label:"Review & Apply",action:"apply"},{label:"Edit page →",action:"edit"}])
    function activateMenu(index) {
        menuIndex=index;const entry=menuEntries[index];if(!entry)return
        choice=0
        if(entry.action==="new")beginKeyboard("newPage","")
        else if(entry.action==="page"){pageFocus[pageId]=selectedId;pageId=entry.id;selectedId=pageFocus[pageId] || "";mode="pageContext"}
        else if(entry.action==="add")chooseTool(0)
        else if(entry.action==="appearance")openInspector(true)
        else if(entry.action==="more")mode="more"
        else if(entry.action==="apply")mode="apply"
        else {toolbarFocus=false;mode="canvas";if(!current && scopedItems.length)selectedId=scopedItems[0].id}
    }
    signal closed()
    signal skinsRequested()
    visible: open
    z: 100
    onCurrentChanged:if(current && !manipulating){parentScopeId=current.parentId || "";if(sectionMode){let root=current;for(let depth=0;root.parentId && depth<4;++depth)root=items.find(i=>i.id===root.parentId) || root;scopeId=root.categoryId || ""}Qt.callLater(()=>{if(current && current.placement && gridMode && !manipulating)preview.revealPlacement(current.placement,scopeId,current.parentId || "")})}
    onChoiceChanged:if(mode==="collision" && choice<2)collisionStrategy=choice===0?"shift":"swap"
    onScopeIdChanged:if(open && sectionMode && !scopeOverview)Qt.callLater(()=>{if(open && sectionMode)preview.revealScope(scopeId)})

    function show(id, component) {
        session = layoutModel.beginStudio()
        pageId = id || (!session.model.rootPageVisible?session.model.homePageId:""); selectedId = component || "";pageFocus=({})
        mode = "canvas"; toolbarFocus = true; tool = 0;menuIndex=0; message = "";deletedSelection=""
        scopeOverview=true;scopeId=current?current.categoryId || "":page && page.sections.length?page.sections[0].id || "":""
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
    function openInspector(asPage){mode="inspector";toolbarFocus=false;inspector.begin(asPage)}
    function openStructureInspector(type,id){openInspector(false);inspector.structureType=type;inspector.structureId=id;structureType=type;structureId=id}
    function projected(item){return mode==="inspector" && inspector.editingState!=="base" && (inspector.structureType==="section"?item.categoryId===inspector.structureId:inspector.selectedIds.indexOf(item.id)>=0)?Object.assign({},item,{designState:inspector.editingState}):item}
    function inspectorOperation(name){
        inspectorReturn=false
        if(name==="groupLayout"){beginChildLayout(true);return}
        if(name==="scrolling"){
            if(!gridMode || sectionMode){inspector.message="This layout uses automatic sizing or scrolling sections. Use Grid to preview a different layout.";return}
            scrollingViewport=preview.editorViewport();scrollingDraft=!!page.composition.verticalScrolling;mode="scrolling";choice=0;message="";return
        }
        if(name==="structureEarlier" || name==="structureLater") {command(inspector.structureType==="section"?"section":"navigation",{kind:"move",id:inspector.structureId,offset:name==="structureEarlier"?-1:1});return}
        if(name==="structureRemove"){structureType=inspector.structureType;structureId=inspector.structureId;mode="structureRemove";choice=0;message=structureType==="section"?"Remove this section and move its components to Uncategorized? Placements must still fit; Undo restores the entire change.":"Remove this navigation tab? If it starts the page, the first remaining tab becomes the start. Undo restores it.";return}
        if(name==="navSections"){structureId=inspector.structureId;const tab=(page.composition.navigation || []).find(t=>t.id===structureId);navSections=tab && tab.sections?tab.sections.slice():(page.categories || []).map(s=>s.id);mode="navSections";choice=0;return}
        if(name==="geometry"){
            if(!gridMode){message="Size follows this page's automatic layout.";inspector.message=message;return}
            beginGesture(true);if(mode==="resize"){mode="size";choice=0;sizeControlsVisible=true;inspectorReturn=true}return
        }
        if(name==="grid"){beginGrid();inspectorReturn=true;return}
        if(name==="children"){if(current && current.componentType==="container"){const parent=current.id;context(14);parentScopeId=parent;chooseTool(0)}return}
        if(name==="order"){mode="navigationOrder";choice=0;inspectorReturn=true;return}
        if(name==="navigation" && !page.composition.enabled){beginGrid();inspectorReturn=true;message="Preview an explicit composed layout before adding navigation tabs.";return}
        if(name==="navigation" || name==="sections" || name==="moviePage" || name==="showsPage"){mode=name;choice=0;return}
        const index=({duplicate:3,remove:4,orderBefore:11,orderAfter:12,destination:13,contents:14,groupDestination:16,selectGroup:17})[name]
        if(index!==undefined){context(index);if(mode!=="canvas")inspectorReturn=true}
    }
    function position(item) { return item && item.placement ? Object.assign({},item.placement) : ({x:0,y:0,width:1,height:1}) }
    function beginGesture(resize){beginGestureInScope(resize,false)}
    function beginGestureInScope(resize,forceGrid) {
        if (!current) { message="Choose a component in Layers first."; return }
        if(current.locked){message="This component is locked. Choose Edit component → Lock / unlock first.";return}
        gestureViewport=preview.editorViewport()
        originScopeOverview=scopeOverview
        originParentId=current.parentId || "";originScopeId=scopeId
        origin=position(current);candidate=Object.assign({},origin)
        const layout=parentGroup?(parentGroup.presentation.layout || {}):({})
        const ordered=!!parentGroup && (layout.mode==="row" || layout.mode==="column")
        if(!gridMode || !resize && ordered && !forceGrid){
            if(resize){message="Size is determined by this page's layout.";return}
            flowContainerId=ordered?parentScopeId:"";flowAxis=layout.mode==="column"?"y":"x";flowPreviewValid=true
            let siblings=items.filter(i=>(i.parentId || "")===(current.parentId || "") && (i.categoryId || "")===(current.categoryId || ""))
            if(ordered)siblings=siblings.slice().sort((a,b)=>a.placement[flowAxis]-b.placement[flowAxis] || a.id.localeCompare(b.id))
            flowIds=siblings.map(i=>i.id)
            flowOriginIndex=flowIds.indexOf(selectedId);flowTargetIndex=flowOriginIndex
            // Freeze target geometry before reordering. Moving neighbors must
            // not change which insertion target lies under a stationary mouse.
            flowTargets=flowIds.map(id=>({id:id,rect:preview.editorRectFor(id),placement:position(items.find(item=>item.id===id))}))
            mode="flowMove";toolbarFocus=false;message="Moving "+componentName(current);return
        }
        addingType="";pendingScrollEnable=false; origin=position(current); candidate=Object.assign({},origin)
        originParentId=current.parentId || "";originScopeId=scopeId;resizeAxis="both";resizeAnchor="top-left"
        gestureMode=resize?"resize":"move"
        message=""
        mode=resize ? "resize" : "move"; toolbarFocus=false
    }
    function cancelGesture() {
        if(mode==="flowMove")session.cancelPreview()
        parentScopeId=originParentId;scopeId=originScopeId;scopeOverview=originScopeOverview
        if(!addingType && current)candidate=Object.assign({},origin)
        addingType="";pendingScrollEnable=false;mode=inspectorReturn?"inspector":"canvas";inspectorReturn=false;message="Canceled · no draft or history changes."
        const viewport=gestureViewport
        // Let proposal delegates and queued selection reveals settle first.
        Qt.callLater(()=>Qt.callLater(()=>preview.restoreEditorViewport(viewport)))
    }
    function previewFlowTarget(target) {
        if(mode!=="flowMove" || target<0 || target>=flowIds.length || target===flowTargetIndex)return
        if(target===flowOriginIndex){session.cancelPreview();flowTargetIndex=target;flowPreviewValid=true;message="";return}
        flowPreviewValid=session.previewCommand(flowContainerId?"insertRelative":"orderRelative",{id:selectedId,relativeId:flowIds[target],before:target<flowOriginIndex},pageId)
        flowTargetIndex=target
        if(flowPreviewValid){message="Moving "+componentName(current);if(flowContainerId)Qt.callLater(()=>{if(current)preview.revealPlacement(current.placement,scopeId,flowContainerId)})}
        else{message=session.errorString;session.cancelPreview()}
    }
    function flowTargetRect(index){const target=flowTargets[index];return target?(flowContainerId?preview.placementRectFor(target.placement,scopeId,flowContainerId):target.rect):null}
    function beginArrangement() {
        arrangementReturnChoice=choice;gestureViewport=preview.editorViewport()
        session.cancelPreview();arrangementValid=false;mode="arrange";choice=0;message=""
    }
    function previewArrangement(index) {
        if(index<0 || index>=arrangementModes.length)return
        choice=index;session.cancelPreview();arrangementValid=false
        arrangementValid=session.previewCommand("arrange",{ids:multiSelection,mode:arrangementModes[index]},pageId)
        message=arrangementValid?arrangementNames[index]+" · Preview":session.errorString
    }
    function finishArrangement(accept) {
        if(accept){
            if(!arrangementValid){message="Choose a valid arrangement first.";return}
            if(!session.acceptPreview()){arrangementValid=false;message=session.errorString;return}
            mode="canvas";toolbarFocus=false;message="Arrangement updated in draft."
        }else{
            session.cancelPreview();mode="layers";choice=arrangementReturnChoice;message=""
            const viewport=gestureViewport;Qt.callLater(()=>Qt.callLater(()=>preview.restoreEditorViewport(viewport)))
        }
    }
    function beginChildLayout(fromInspector) {
        if(!current || current.componentType!=="container"){message="Choose a group first.";return}
        if(current.locked){message="Unlock this group before arranging it.";return}
        childLayoutFromInspector=!!fromInspector;gestureViewport=preview.editorViewport();session.cancelPreview()
        childLayoutDraft=Object.assign({},current.presentation.layout);childLayoutValid=true;mode="childLayout";choice=0;message=""
    }
    function previewChildLayout(value) {
        childLayoutDraft=value;session.cancelPreview()
        childLayoutValid=session.previewCommand("childLayout",{id:selectedId,value:value},pageId)
        message=childLayoutValid?"":session.errorString
    }
    function finishChildLayout(accept) {
        if(accept){
            if(!childLayoutValid){message="Choose a valid layout first.";return}
            if(preview.layoutError){message=preview.layoutError;return}
            if(session.proposalModel && !session.acceptPreview()){childLayoutValid=false;message=session.errorString;return}
            message="Group layout updated in draft."
        }else{session.cancelPreview();message="";const viewport=gestureViewport;Qt.callLater(()=>Qt.callLater(()=>preview.restoreEditorViewport(viewport)))}
        mode=childLayoutFromInspector?"inspector":"canvas";toolbarFocus=false
    }
    function beginRelative(before) {
        if(!current || addingType){message="Place the new component before changing its reading order.";return}
        if(current.locked){message="Unlock this component before changing its order.";return}
        parentScopeId=current.parentId || "";candidate=position(current)
        mode=before?"orderBefore":"orderAfter";choice=0
        message=gridMode?"Change reading order. Grid positions remain as authored.":"Choose the neighboring component in this section."
    }
    function beginGrid() {
        if(!gridModes.length){message="Packaged Studio defaults are missing or invalid.";return}
        inspectorReturn=false;gridWasPlaced=gridMode;gridBaselineComposition=Object.assign({},page.composition);gestureViewport=preview.editorViewport()
        ++gridPreviewSerial;session.cancelPreview();gridPreviewValid=gridWasPlaced;message=""
        gridDraft=Object.assign({},layoutModel.studioOptions.defaultGrid,pageGrid)
        sectionHeightDraft=page && page.composition.sectionHeight!==undefined?page.composition.sectionHeight:layoutModel.studioOptions.sectionHeight
        mode="convert";choice=6
    }
    function previewGrid(value,height) {
        ++gridPreviewSerial;gridDraft=Object.assign({},value);sectionHeightDraft=height;session.cancelPreview()
        const preserve=gridWasPlaced && value.mode===((gridBaselineComposition.grid || {}).mode || "screen")
        const composition=Object.assign({},gridBaselineComposition,{grid:value})
        if(value.mode==="sections")composition.sectionHeight=height
        gridPreviewValid=preserve?session.previewCommand("composition",{value:composition},pageId):session.previewCommand("convertGrid",{value:value,sectionHeight:height},pageId)
        message=gridPreviewValid?"":session.errorString
        if(gridPreviewValid && preview.editorGeometryReady && preview.layoutError){message=preview.layoutError;gridPreviewValid=false;session.cancelPreview()}
        return gridPreviewValid
    }
    function cancelGrid() {
        ++gridPreviewSerial;session.cancelPreview();mode=inspectorReturn?"inspector":"canvas";inspectorReturn=false;message=""
        const viewport=gestureViewport;Qt.callLater(()=>Qt.callLater(()=>preview.restoreEditorViewport(viewport)))
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
            proposal=session.additionProposal(addingType,candidate,strategy || "shift",pageId,parentScopeId?"":sectionMode?scopeId:"",parentScopeId,addingConfiguration,pendingScrollEnable)
            if(!proposal.valid){message=proposal.reason;if(mode!=="size"){mode="noRoom";choice=0}return}
            if(command("batch",{commands:proposal.commands})){selectedId=proposal.id;addingType="";pendingScrollEnable=false;mode="canvas"}
            return
        }
        proposal=session.placementProposal(selectedId,candidate,strategy || (["move","resize","size"].indexOf(mode)>=0?"shift":"cancel"),pageId,placementOptions)
        if(!proposal.valid && (["move","resize","size"].indexOf(mode)>=0 && !strategy)){message=proposal.reason;return}
        if (!proposal.valid) { message=proposal.reason; const previous=mode; mode="collision"; if(previous!=="collision"){collisionStrategy="shift";choice=0} return }
        if (command("place",{placements:proposal.placements})){mode=inspectorReturn?"inspector":"canvas";inspectorReturn=false}
    }
    function setDimension(axis,value) {
        if(mode!=="size" || (axis!=="width" && axis!=="height"))return
        const limit=axis==="width"?grid.columns:grid.rows
        if(value<1 || value>limit || Math.floor(value)!==value)return
        let next=Object.assign({},candidate);next[axis]=value
        // Preview edge anchoring visibly, so the full page span is selectable
        // even when the component did not start in the first column/row.
        next.x=Math.min(origin.x,grid.columns-next.width)
        next.y=Math.min(origin.y,placementRows-next.height)
        candidate=next;preview.revealPlacement(next,scopeId,parentScopeId);message=""
    }
    function beginKeyboard(action, value) { keyboardReturnMode=mode; keyboardAction=action; keyboard.value=value || ""; keyboard.selectedIndex=0; mode="keyboard" }
    function startAddition(type,configuration) {
        pendingScrollEnable=false;addingType=type;addingConfiguration=configuration || ({})
        const prototype=session.componentPrototype(type,addingConfiguration)
        if(!prototype.root){message=session.errorString;return}
        gestureViewport=preview.editorViewport();originScopeId=scopeId;originParentId=parentScopeId;originScopeOverview=scopeOverview
        candidate=Object.assign({},prototype.root.placement,{x:0,y:0});origin=Object.assign({},candidate)
        gestureMode="move";toolbarFocus=false;mode="move";message=""
        suggestAdditionPosition()
    }
    function suggestAdditionPosition() {
        const occupied=new Set();let bottom=grid.rows
        for(const item of scopedItems){const p=item.placement;if(!p)continue;bottom=Math.max(bottom,p.y+p.height);for(let y=p.y;y<p.y+p.height;++y)for(let x=p.x;x<p.x+p.width;++x)occupied.add(y*grid.columns+x)}
        const rows=Math.min(placementRows,bottom+candidate.height)
        for(let y=0;y+candidate.height<=rows;++y)for(let x=0;x+candidate.width<=grid.columns;++x){
            let free=true
            for(let dy=0;dy<candidate.height && free;++dy)for(let dx=0;dx<candidate.width;++dx)if(occupied.has((y+dy)*grid.columns+x+dx)){free=false;break}
            if(free){candidate=Object.assign({},candidate,{x:x,y:y});origin=Object.assign({},candidate);Qt.callLater(()=>preview.revealPlacement(candidate,scopeId,parentScopeId));return}
        }
        message="No room for this component.";mode="noRoom";choice=0
    }
    function previewScrolling(enabled) {
        if(!session.previewCommand("scrolling",{enabled:enabled},pageId)){message=session.errorString;return}
        scrollingDraft=enabled;message=""
    }
    function chooseTool(index) {
        tool=index; choice=0
        if(index===0) { if(!gridMode) beginGrid(); else {mode="catalog";catalog.begin()} }
        else if(index===1) mode="pages"
        else if(index===2) mode="layers"
        else if(index===3) { if(!session.undo()) message=session.errorString;else if(deletedSelection && items.some(i=>i.id===deletedSelection)){selectedId=deletedSelection;deletedSelection=""} }
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
        else if(index===1){mode="inspector";inspectorReturn=false;return}
        else if(index===2) beginKeyboard("rename",current?current.name:"")
        else if(index===3) {const identities=items.map(i=>i.id),parent=current.parentId || "";if(command("duplicate",{id:selectedId})){const copy=items.find(i=>identities.indexOf(i.id)<0 && (i.parentId || "")===parent);selectedId=copy?copy.id:"";mode="canvas"}}
        else if(index===4) { mode="remove";choice=0 }
        else if(index===5) beginKeyboard("text",current && current.presentation ? current.presentation.subtitle : "")
        else if(index===6) beginKeyboard("opacity",current && current.presentation && current.presentation.surfaceOpacity!==undefined ? String(current.presentation.surfaceOpacity) : "1")
        else if(index===7) beginKeyboard("corners",current && current.presentation ? String(current.presentation.radius || 0) : "0")
        else if(index===8) {beginGestureInScope(false,true);if(mode==="move"){mode="geometry";choice=0}}
        else if(index===9){if(command("state",{id:selectedId,key:"hidden",value:!current.hidden})){mode="canvas";toolbarFocus=false}}
        else if(index===10){if(command("state",{id:selectedId,key:"locked",value:!current.locked})){mode="canvas";toolbarFocus=false}}
        else if(index===11 || index===12)beginRelative(index===11)
        else if(index===13){beginGestureInScope(false,true);if(mode==="move"){mode="destination";choice=0}}
        else if(index===14){if(current.appCollection){openInspector(false);inspector.controlsShown=true;inspector.activateField({type:"apps"});return}if(current.componentType!=="container"){message="Choose a group to edit its contents.";return}parentScopeId=current.id;const children=items.filter(i=>(i.parentId || "")===parentScopeId);selectedId=children.length?children[0].id:"";mode="canvas";toolbarFocus=false}
        else if(index===15){if(command("ungroup",{id:selectedId})){selectedId="";mode="canvas";toolbarFocus=false}}
        else if(index===16){beginGestureInScope(false,true);if(mode==="move"){mode="groupDestination";choice=0}}
        else if(index===18)beginChildLayout()
        else if(index===17){let selection=multiSelection.slice();const index=selection.indexOf(selectedId);if(index>=0)selection.splice(index,1);else if(selection.length<32)selection.push(selectedId);else{message="Select at most 32 components.";return}multiSelection=selection;mode="layers";choice=0;message="Choose Group selected at the end of Layers. Right expands; Left collapses."}
    }
    function dialogLabels() {
        if(mode==="noRoom")return [!scrollingEnabled && !parentScopeId && !sectionMode?"Enable vertical scrolling":"Vertical scrolling unavailable here","Choose another position","Change size",sectionMode?"Choose another section":"Choose another group","Cancel"]
if(mode==="canvas" && !selectedId)return ["Add component","Page appearance","Templates"]
        if(mode==="navigation")return ["＋ Add navigation tab"].concat((page.composition.navigation || []).map(t=>t.name))
        if(mode==="sections")return ["＋ Add section"].concat((page.categories || []).map(s=>s.name))
        if(mode==="structureRemove")return ["Cancel","Remove in draft"]
        if(mode==="remove")return ["Cancel","Remove in draft"]
        if(mode==="moviePage" || mode==="showsPage")return ["Use classic library"].concat(pageEntries.filter(p=>!!p.id).map(p=>p.name))
        if(mode==="navSections")return ["Show all sections","Use selected sections"].concat((page.categories || []).map(s=>(navSections.indexOf(s.id)>=0?"✓ ":"□ ")+s.name))
        if(mode==="pageContext")return ["Edit page","Appearance","Rename page","Duplicate page",pageId===draftModel.homePageId?"Current Home page":"Set as Home","Move page up","Move page down",pageId===draftModel.homePageId?"Delete unavailable · this is Home":"Delete page…"]
        if(mode==="setHome")return ["Set as Home in draft","Cancel"]
        if(mode==="deletePage")return ["Cancel","Delete if there are no incoming links"].concat(removalDestinations.map(p=>"Redirect incoming links to "+p.name))
        if(mode==="confirmDeletePage")return ["Delete page in draft","Cancel"]
        if(mode==="pageAppearance")return ["Background start","Background end","Component surface","Text color","Focus color"]
        if(mode==="editorAppearance")return ["Surface color","Selected surface","Text color","Supporting text","Focus color","Reset Studio colors"]
        if(mode==="more")return ["Undo","Redo","Layers","Grid and sections","Try interaction","Section scope","Studio appearance","Canvas view","Saved skins","Restore defaults"]
        if(mode==="restoreDefaults")return ["Cancel","Restore Home layout; keep other pages"]
        if(mode==="context") return (current && (current.componentType==="container" || current.componentType==="carousel")) ? ["Move","Edit component","Duplicate","Delete…","Edit contents"] : ["Move","Edit component","Duplicate","Delete…"]
        if(mode==="flowMove")return ["Move before…","Move after…","Convert to a grid…","Cancel"]
        if(mode==="moveOptions")return ["Position and size",gridMode?"Reading order before…":"Move before…",gridMode?"Reading order after…":"Move after…",sectionMode?"Move to section…":"Sections unavailable in this layout","Resize anchor…","Move into group…","Return to "+gestureMode]
        if(mode==="anchors")return ["Top left","Top right","Bottom left","Bottom right"]
        if(mode==="destination")return destinationSections.map(s=>s.name+(s.id===originScopeId?" · Current section":""))
        if(mode==="groupDestination")return destinationGroups.map(g=>destinationGroupLabel(g)+(g.id===originParentId?" · Current":""))
        if(mode==="orderBefore" || mode==="orderAfter")return relativeItems.length?relativeItems.map(i=>i.name || i.widgetType || i.id):["No other components in this section · Back returns"]
        if(mode==="properties")return contextActions.slice(2)
        if(mode==="layers") return layerRows.map(row=>"  ".repeat(row.depth)+(row.item.componentType==="container"?(expandedGroups[row.item.id]?"▾ ":"▸ "):"  ")+(multiSelection.indexOf(row.item.id)>=0?"✓ ":"")+(row.item.name || row.item.widgetType || row.item.id)+(row.item.hidden?" · Hidden":"")+(row.item.locked?" · Locked":"")).concat(multiSelection.length>=2?["Group "+multiSelection.length+" selected components","Align / distribute selected…"]:[]).concat(["Inspect reading order / directional focus"])
        if(mode==="view")return ["Zoom in · "+Math.round(camera.zoom*100)+"%","Zoom out","Pan left","Pan right","Pan up","Pan down","Fit / reset view"]
        if(mode==="childLayout")return ["Layout: "+childLayoutDraft.mode,"Columns: "+childLayoutDraft.columns,"Rows: "+childLayoutDraft.rows,"Gap: "+childLayoutDraft.gap,"Padding: "+childLayoutDraft.padding,"Arrange children in draft","Cancel"]
        if(mode==="arrange")return ["Align left","Center horizontally","Align right","Align top","Center vertically","Align bottom","Distribute horizontally","Distribute vertically"]
        if(mode==="navigationOrder")return scopedItems.map((item,index)=>(index+1)+" · "+componentName(item)+(item.placement?" · row "+(item.placement.y+1)+", column "+(item.placement.x+1):""))
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
        if(mode==="gridExact")return ["Columns: "+gridDraft.columns,"Rows: "+gridDraft.rows,"Item gap: "+gridDraft.gap,"Safe margin: "+gridDraft.margin].concat(gridDraft.mode==="sections"?["Section height: "+sectionHeightDraft]:[]).concat(["Back to layout"])
        if(mode==="conversionPreview") return ["Use this arrangement in draft", "Cancel"]
        if(mode==="dirty") return ["Keep editing", "Discard draft…", "Review & Apply"]
        if(mode==="discard") return ["Keep editing", "Discard all draft pages"]
        if(mode==="remove") return ["Cancel", "Delete component"]
        if(mode==="apply") return ["Apply all draft changes", "Keep editing"]
        if(mode==="history") return ["Keep history and cancel edit", "Release history; keep draft"]
        return []
    }
    function activateChoice() {
        if(mode==="noRoom"){
            if(choice===0){if(!scrollingEnabled && !parentScopeId && !sectionMode){pendingScrollEnable=true;mode="move";suggestAdditionPosition()}else message="Choose a position, a different size or another container."}
            else if(choice===1){mode="move";message=""}
            else if(choice===2){mode="size";choice=0;sizeControlsVisible=true}
            else if(choice===3){mode=sectionMode?"destination":"groupDestination";choice=0}
            else cancelGesture()
            return
        }
        if(mode==="navigation"){if(choice===0)beginKeyboard("navAdd","");else{const tab=(page.composition.navigation || [])[choice-1];if(tab)openStructureInspector("navigation",tab.id)}return}
        if(mode==="sections"){if(choice===0)beginKeyboard("sectionAdd","");else{const section=(page.categories || [])[choice-1];if(section)openStructureInspector("section",section.id)}return}
        if(mode==="structureRemove"){if(choice===1 && command(structureType==="section"?"section":"navigation",{kind:"remove",id:structureId})){mode=structureType==="section"?"sections":"navigation";choice=0}else if(choice===0)mode="inspector";return}
        if(mode==="moviePage" || mode==="showsPage"){const destinations=pageEntries.filter(p=>!!p.id),value=choice===0?null:destinations[choice-1].id;if(command("pagePatch",{scope:"composition",value:{[mode]:value}})){mode="inspector"}return}
        if(mode==="navSections"){
            if(choice<2){if(command("navigation",{kind:"patch",id:structureId,value:{sections:choice===0?null:navSections}}))mode="inspector";return}
            const id=page.categories[choice-2].id,next=navSections.slice(),index=next.indexOf(id);if(index>=0)next.splice(index,1);else next.push(id);navSections=next;return
        }
        if(mode==="pageContext") {
            if(choice===0){mode="canvas";toolbarFocus=false;if(!current && scopedItems.length)selectedId=scopedItems[0].id}
            else if(choice===1){openInspector(true);choice=0}
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
        if(mode==="restoreDefaults") {
            if(choice===1) {
                session.invalidate()
                const ok=layoutModel.resetToDefaults(),error=layoutModel.errorString
                show("","")
                if(!ok)message=error
            } else {mode="more";choice=9}
            return
        }
        if(mode==="more"){
            if(choice>=8){
                if(session.dirty){message="Apply or discard your draft before changing saved skins or restoring defaults.";return}
                if(choice===8)skinsRequested();else{mode="restoreDefaults";choice=0;message="Restores Home's packaged layout. Other pages, saved skins, apps and Kodi media stay unchanged."}
                return
            }
            if(choice===7){mode="view";choice=0;return}if(choice===6){mode="editorAppearance";choice=0;return}const index=[3,4,2,5,7,9][choice];mode="canvas";chooseTool(index);return
        }
        if(mode==="editorAppearance") {
            if(choice===5){command("studioAppearance",{value:{}});return}
            const key=["surfaceColor","selectedColor","textColor","mutedColor","focusColor"][choice];beginKeyboard("studioColor:"+key,String(palette[key]));return
        }
        if(mode==="context") {
            if(choice===0)beginGesture(false)
            else if(choice===1)openInspector(false)
            else if(choice===2)context(3)
            else if(choice===3)context(4)
            else if(choice===4)context(14)
            return
        }
        if(mode==="properties"){context(choice+2);return}
        if(mode==="flowMove"){if(choice<2)beginRelative(choice===0);else if(choice===2)beginGrid();else mode="canvas";return}
        if(mode==="scopes") {
            scopeOverview=choice===0
            if(choice>0){scopeId=page.sections[choice-1].id || "";const item=items.find(i=>(i.categoryId || "")===scopeId);selectedId=item?item.id:""}
            mode="canvas";toolbarFocus=false;Qt.callLater(()=>preview.revealScope(scopeId));return
        }
        if(mode==="layers") {if(choice===layerRows.length+(multiSelection.length>=2?2:0)){mode="navigationOrder";choice=0;message="Reading order follows this list. D-pad movement uses the displayed row and column in the current scope.";return}if(choice===layerRows.length){if(command("group",{ids:multiSelection})){multiSelection=[];mode="canvas";toolbarFocus=false}return}if(choice===layerRows.length+1){beginArrangement();return}if(layerRows[choice])selectedId=layerRows[choice].item.id;mode="canvas";toolbarFocus=false;return}
        if(mode==="view"){
            if(choice===0)camera.zoom=Math.min(2,camera.zoom+0.1)
            else if(choice===1)camera.zoom=Math.max(0.5,camera.zoom-0.1)
            else if(choice===2)camera.panX=Math.max(-width,camera.panX-40)
            else if(choice===3)camera.panX=Math.min(width,camera.panX+40)
            else if(choice===4)camera.panY=Math.max(-height,camera.panY-40)
            else if(choice===5)camera.panY=Math.min(height,camera.panY+40)
            else {camera.zoom=1;camera.panX=0;camera.panY=0}
            message="View only. Layout unchanged.";return
        }
        if(mode==="childLayout"){if(choice===6){mode="canvas";return}if(choice===5 && command("childLayout",{id:selectedId,value:childLayoutDraft})){mode="canvas";toolbarFocus=false}return}
        if(mode==="arrange"){if(choice<8)previewArrangement(choice);else finishArrangement(choice===8);return}
        if(mode==="pages") {
            if(choice===draftModel.pages.length+1) {beginKeyboard("newPage","");return}
            pageId=choice===0 ? "" : draftModel.pages[choice-1].id;selectedId="";mode="canvas";return
        }
        if(mode==="add") {addingType=["clock","text","container","input-panel"][choice];const prototype=session.componentPrototype(addingType);if(!prototype.root){message=session.errorString;return}origin={};originScopeId=scopeId;originParentId=parentScopeId;gestureMode="move";candidate=Object.assign({},prototype.root.placement,{x:0,y:0});mode="move";toolbarFocus=false;return}
        if(mode==="collision") {if(choice===2)cancelGesture();else if(choice===3){if(collisionPreview.valid){mode="collisionDetails";choice=0;message="Proposed positions only · Back returns to Shift / Swap / Cancel. Nothing has changed."}else message=collisionPreview.reason}else if(collisionPreview.valid)acceptCandidate(choice===0 ? "shift":"swap");else message=collisionPreview.reason;return}
        if(mode==="collisionDetails")return
        if(mode==="moveOptions") {
            if(choice===0){mode="geometry";choice=0}
            else if(choice===1 || choice===2)beginRelative(choice===1)
            else if(choice===3){if(sectionMode){mode="destination";choice=0}else message="Choose a scrolling section layout to move between sections."}
            else if(choice===4){if(gestureMode==="resize"){mode="anchors";choice=0}else message="Choose Width or Height to change the resize anchor."}
            else if(choice===5){mode="groupDestination";choice=0;message="Choose a destination; the complete move stays a preview until placed."}
            else mode=gestureMode
            return
        }
        if(mode==="anchors"){resizeAnchor=["top-left","top-right","bottom-left","bottom-right"][choice];mode="resize";message="";return}
        if(mode==="destination") {
            if(!destinationSections[choice])return
            parentScopeId="";scopeId=destinationSections[choice].id || "";scopeOverview=false;candidate=Object.assign({},candidate,{x:0,y:0});mode="move"
            message="Move to "+destinationSections[choice].name+" · choose a cell, then Confirm. Back restores the original section."
            Qt.callLater(()=>preview.revealScope(scopeId));return
        }
        if(mode==="groupDestination"){chooseGroupDestination(choice);return}
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
            if(choice===7)cancelGrid()
            else if(choice===8){mode="gridExact";choice=0}
            else if(choice===0){const index=gridModes.findIndex(m=>m.id===gridDraft.mode);gridDraft=Object.assign({},gridDraft,{mode:gridModes[(index+1)%gridModes.length].id})}
            else if(choice===6 && previewGrid(gridDraft,sectionHeightDraft)){mode="conversionPreview";choice=0;message=gridDraft.columns+" columns × "+gridDraft.rows+" rows · "+(gridModes.find(m=>m.id===gridDraft.mode)||{}).name}
            return
        }
        if(mode==="gridExact"){
            const count=gridDraft.mode==="sections"?5:4
            if(choice===count){mode="convert";choice=8;return}
            const field=gridField(choice+1);beginKeyboard("grid:"+field.key,String(field.key==="sectionHeight"?sectionHeightDraft:gridDraft[field.key]));return
        }
        if(mode==="conversionPreview") {
            if(choice!==0){cancelGrid();return}
            if(gridGeometryPending){message="Layout is updating…";return}
            if(!gridPreviewValid || preview.layoutError){message=preview.layoutError || "Choose a valid layout first.";return}
            if(!session.acceptPreview()){message=session.errorString;return}
            mode=inspectorReturn?"inspector":"canvas";inspectorReturn=false;message="Page layout updated in draft.";return
        }
        if(mode==="dirty") {mode=choice===0 ? "canvas" : choice===1 ? "discard":"apply";choice=0;return}
        if(mode==="discard") {if(choice===1)close();else mode="canvas";return}
        if(mode==="remove") {
            if(choice===1){const id=selectedId;if(!command("remove",{id:id}))return;deletedSelection=id;selectedId=items.length?items[0].id:""}
            mode="canvas";return
        }
        if(mode==="apply") {if(choice===1)mode="canvas";else if(preview.layoutError)message=preview.layoutError;else if(session.apply(selectedId)){mode="canvas";message="Applied successfully. All pages saved."}else message=session.errorString;return}
        if(mode==="history") {
            if(choice===1){session.continueWithNewHistoryBaseline();mode=addingType?"move":"canvas"}
            else if(addingType)cancelGesture()
            else mode="canvas"
        }
    }
    function handleCommand(c) {
        if (!open) return
        if (!session || !session.active) {close();return}
        if(mode==="convert"){gridEditor.handleCommand(c);return}
        if(mode==="gridExact" && c===inputManager.back){mode="convert";choice=8;return}
        if(mode==="conversionPreview" && c===inputManager.back){cancelGrid();return}
        if(mode==="childLayout"){childLayoutEditor.handleCommand(c);return}
        if(mode==="arrange"){
            if(c===inputManager.back){finishArrangement(false);return}
            if(c===inputManager.confirm){activateChoice();return}
            if(c===inputManager.navigateLeft)choice=Math.max(choice<6?Math.floor(choice/3)*3:choice<8?6:8,choice-1)
            else if(c===inputManager.navigateRight)choice=Math.min(choice<6?Math.floor(choice/3)*3+2:choice<8?7:9,choice+1)
            else if(c===inputManager.navigateUp)choice=choice>=8?choice-2:choice>=6?choice===6?3:5:Math.max(0,choice-3)
            else if(c===inputManager.navigateDown)choice=choice<3?choice+3:choice<6?choice===5?7:6:Math.min(9,choice+2)
            return
        }
        if(mode==="scrolling"){
            if(c===inputManager.back || c===inputManager.confirm && choice===2){session.cancelPreview();mode="inspector";const viewport=scrollingViewport;Qt.callLater(()=>preview.restoreEditorViewport(viewport));return}
            if(c===inputManager.navigateUp)choice=Math.max(0,choice-1)
            else if(c===inputManager.navigateDown)choice=Math.min(2,choice+1)
            else if((c===inputManager.navigateLeft || c===inputManager.navigateRight) && choice===0)previewScrolling(c===inputManager.navigateRight)
            else if(c===inputManager.confirm){if(choice===0)previewScrolling(!scrollingDraft);else if(choice===1){if(!session.proposalModel || session.acceptPreview())mode="inspector";else message=session.errorString}}
            return
        }
        if(mode==="size") {
            if(c===inputManager.back){cancelGesture();return}
            if(c===inputManager.menu){sizeControlsVisible=!sizeControlsVisible;return}
            if(!sizeControlsVisible){sizeControlsVisible=true;return}
            if(c===inputManager.navigateUp)choice=Math.max(0,choice-1)
            else if(c===inputManager.navigateDown)choice=Math.min(5,choice+1)
            else if((c===inputManager.navigateLeft || c===inputManager.navigateRight) && choice<2){const axis=choice===0?"width":"height";setDimension(axis,candidate[axis]+(c===inputManager.navigateRight?1:-1))}
            else if(c===inputManager.confirm){
                if(choice===2)acceptCandidate()
                else if(choice===3)sizeControlsVisible=false
                else if(choice===4){mode="geometry";choice=0}
                else if(choice===5)cancelGesture()
            }
            return
        }
        if(mode==="flowMove") {
            if(c===inputManager.back){cancelGesture();return}
            if(c===inputManager.confirm){if(!flowPreviewValid)return;if(!session.proposalModel || session.acceptPreview()){mode="canvas";message=""}else message=session.errorString;return}
            const direction=c===inputManager.navigateRight || c===inputManager.navigateDown?1:c===inputManager.navigateLeft || c===inputManager.navigateUp?-1:0
            if(!direction)return
            const target=flowTargetIndex+direction
            previewFlowTarget(target)
            return
        }
        if(mode==="catalog"){catalog.handleCommand(c);return}
        if(mode==="inspector"){inspector.handleCommand(c);return}
        if(mode==="keyboard") {keyboard.handleCommand(c);return}
        if(mode==="preview") {if(c===inputManager.back)mode="canvas";else preview.handleCommand(c);return}
        if(c===inputManager.back) {
            if(mode==="history" && addingType){cancelGesture();return}
            if(mode==="collisionDetails"){mode="collision";choice=collisionStrategy==="shift"?0:1;return}
            if(["move","resize","collision","geometry","moveOptions","destination","groupDestination","anchors","noRoom"].indexOf(mode)>=0){cancelGesture();return}
            if(mode!=="canvas") {if(mode==="conversionPreview")session.cancelPreview();mode=inspectorReturn?"inspector":"canvas";inspectorReturn=false;addingType="";pendingScrollEnable=false;return}
            if(!toolbarFocus && parentScopeId){const id=parentScopeId;parentScopeId="";selectedId=id;return}
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
            if(p.x<0 || p.y<0 || p.width<1 || p.height<1 || p.height>grid.rows || p.x+p.width>grid.columns || p.y+p.height>placementRows){message="Grid boundary: choose a position or size inside the grid.";return}
            candidate=p;preview.revealPlacement(p,scopeId,parentScopeId);message="";return
        }
        if(mode!=="canvas") {
            if(mode==="geometry" && choice<4 && (c===inputManager.navigateLeft || c===inputManager.navigateRight)) {
                const key=["x","y","width","height"][choice],step=c===inputManager.navigateRight?1:-1
                let next=Object.assign({},candidate);next[key]+=step
                if(next.x<0 || next.y<0 || next.width<1 || next.height<1 || next.x+next.width>grid.columns || next.y+next.height>placementRows || next.height>grid.rows){message="No room for that size or position.";return}
                candidate=next;preview.revealPlacement(next,scopeId,parentScopeId);message="";return
            }
            const labels=dialogLabels()
            if(mode==="layers" && layerRows[choice]){
                const item=layerRows[choice].item
                if(c===inputManager.menu){const selection=multiSelection.slice(),index=selection.indexOf(item.id);if(index>=0)selection.splice(index,1);else if(selection.length<32)selection.push(item.id);else{message="Select at most 32 components.";return}multiSelection=selection;message="Menu toggles selection · Right expands · Left collapses · Group selected appears at the end.";return}
                if((c===inputManager.navigateLeft || c===inputManager.navigateRight) && item.componentType==="container"){let expanded=Object.assign({},expandedGroups);expanded[item.id]=c===inputManager.navigateRight;expandedGroups=expanded;return}
            }
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
                id:camera;property real zoom:1;property real panX:0;property real panY:0
                width:studio.width;height:studio.height
                scale:Math.min(canvasArea.width/width,canvasArea.height/height)*zoom
                transformOrigin:Item.TopLeft
                x:(canvasArea.width-width*scale)/2+panX;y:(canvasArea.height-height*scale)/2+panY
                HomeScreen {
                    id:preview;objectName:"studioPreview"
                    x:draftNavigation.contentLeft;y:draftNavigation.contentTop;width:parent.width-x;height:parent.height-y
                    items:studio.page?studio.page.items.filter(i=>studio.editorReveals(i)).map(i=>studio.projected(i)):[]
                    sections:studio.page?studio.page.sections.map(s=>Object.assign({},s,{items:(s.items || []).filter(i=>studio.editorReveals(i)).map(i=>studio.projected(i))})):[]
                    appearance:studio.page?studio.page.appearance:studio.palette;composition:studio.page?Object.assign({},studio.page.composition,studio.pendingScrollEnable?{verticalScrolling:true}:{}):({})
                    previewExtentRows:studio.scrollingEnabled && !studio.parentScopeId && studio.manipulating?Math.max(studio.candidate.y+studio.candidate.height,...(studio.collisionPreview.placements || []).map(p=>p.placement.y+p.placement.height)):0
                    grid:studio.pageGrid;placements:studio.page?studio.page.placements:[]
                    editMode:studio.mode!=="preview";scopeView:studio.sectionMode && !studio.scopeOverview;editScopeId:studio.scopeId
                    libraryProvider:studio.provider;pageKey:"studio."+(studio.pageId || "home");previewOnly:true;freezeData:studio.manipulating || studio.gridEditing || studio.mode==="arrange" || studio.mode==="childLayout";samplePreview:studio.samplePreview
                    sampleItems:studio.layoutModel?studio.layoutModel.studioOptions.designSamples || []:[]
                    editSelection:studio.selectedId
                    suppressedComponentIds:studio.collisionPreview.valid?studio.collisionPreview.placements.map(p=>p.id):studio.manipulating && studio.mode!=="flowMove" && !studio.addingType && studio.current?[studio.selectedId]:[]
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
                    readonly property real scrollClipTop:studio.scrollingEnabled?preview.pageViewport.y:0
                    x:preview.x;y:preview.y+scrollClipTop;width:preview.width
                    height:studio.scrollingEnabled?preview.pageViewport.height:preview.height;clip:true;visible:studio.mode!=="preview"
                    Item {
                    y:-parent.scrollClipTop;width:preview.width;height:preview.height
                    Rectangle {
                        objectName:"studioInsertionTarget"
                        visible:studio.mode==="flowMove" && !!rect
                        readonly property var rect:(studio.flowPreviewValid?preview.editRect:studio.flowTargetRect(studio.flowTargetIndex)) || ({x:0,y:0,width:0,height:0})
                        x:rect.x;y:rect.y;width:rect.width;height:rect.height
                        color:"transparent";border.width:3;border.color:studio.palette.focusColor
                        Text {anchors.bottom:parent.top;text:"Position "+(studio.flowTargetIndex+1)+" of "+studio.flowIds.length;color:studio.palette.textColor;style:Text.Outline;styleColor:studio.palette.surfaceColor;font.pixelSize:18*studio.textScale}
                    }
                    Rectangle {
                        visible:!studio.gridMode && !!preview.editRect && !studio.toolbarFocus
                        x:preview.editRect?preview.editRect.x:0;y:preview.editRect?preview.editRect.y:0
                        width:preview.editRect?preview.editRect.width:0;height:preview.editRect?preview.editRect.height:0
                        color:"transparent"
                        Text {anchors.bottom:parent.top;width:parent.width;text:studio.current?studio.current.name || studio.current.widgetType || studio.current.id:"";color:studio.palette.textColor;style:Text.Outline;styleColor:studio.palette.surfaceColor;elide:Text.ElideRight;font.pixelSize:16*studio.textScale}
                    }
                    Repeater {
                        model:studio.gridMode && (studio.mode==="move" || studio.mode==="resize" || studio.mode==="geometry") ? studio.grid.columns*(studio.grid.rows+(studio.scrollingEnabled && !studio.parentScopeId?2:0)) : 0
                        delegate:Rectangle {
                            required property int index
                            objectName:"studioMoveCell-"+index
                            readonly property int column:index%studio.grid.columns
                            readonly property int row:Math.floor(index/studio.grid.columns)+(studio.scrollingEnabled && !studio.parentScopeId?preview.firstVisibleGridRow:0)
                            readonly property var rect:preview.clippedPlacement({x:column,y:row,width:1,height:1},studio.scopeId,studio.parentScopeId)
                            x:rect.x;y:rect.y;width:rect.width;height:rect.height;color:"transparent";border.width:1;border.color:studio.palette.mutedColor;opacity:0.35
                            MouseArea {
                                anchors.fill:parent;enabled:studio.mode==="move"
                                onClicked:{
                                    if(parent.column+studio.candidate.width>studio.grid.columns || parent.row+studio.candidate.height>studio.placementRows){studio.message="Grid boundary: choose a position inside the grid.";return}
                                    studio.candidate=Object.assign({},studio.candidate,{x:parent.column,y:parent.row})
                                    preview.revealPlacement(studio.candidate,studio.scopeId,studio.parentScopeId);studio.message=""
                                }
                            }
                        }
                    }
                    Repeater {
                        model:studio.mode==="flowMove"?studio.flowTargets:[]
                        delegate:Rectangle {
                            required property int index;required property var modelData
                            objectName:"studioFlowTarget-"+index
                            readonly property var rect:studio.flowTargetRect(index) || {x:0,y:0,width:0,height:0}
                            x:rect.x;y:rect.y;width:rect.width;height:rect.height;color:"transparent";border.width:1;border.color:studio.palette.mutedColor;opacity:0.35
                            Accessible.role:Accessible.Button;Accessible.name:"Insertion position "+(index+1)
                            MouseArea {anchors.fill:parent;onClicked:studio.previewFlowTarget(index)}
                        }
                    }
                    Repeater {
                        model:studio.pointerItems.filter(i=>(!studio.gridMode || !!i.placement) && (!i.hidden || i.id===studio.selectedId))
                        delegate:Rectangle {
                            required property var modelData
                            readonly property var rect:studio.gridMode?preview.clippedPlacement(modelData.placement,modelData.categoryId || studio.scopeId,modelData.parentId || ""):preview.editorRectFor(modelData.id) || ({x:0,y:0,width:0,height:0})
                            objectName:"studioSelectionTarget-"+modelData.id
                            x:rect.x;y:rect.y;width:rect.width;height:rect.height;color:"transparent";border.width:0
                            Text {objectName:"studioSelectionName-"+modelData.id;visible:!modelData.parentId && modelData.id===studio.selectedId && !studio.toolbarFocus && !studio.manipulating;anchors.bottom:parent.top;width:Math.min(studio.width-2*studio.spacing,Math.max(parent.width,200*studio.textScale));text:studio.componentName(modelData)+(modelData.hidden?" · Hidden":"")+(modelData.locked?" · Locked":"");color:studio.palette.textColor;style:Text.Outline;styleColor:studio.palette.surfaceColor;font.pixelSize:16*studio.textScale;elide:Text.ElideRight}
                            MouseArea {
                                id:pointerDrag
                                objectName:"studioHit-"+modelData.id
                                anchors.fill:parent
                                property point start
                                property point originScreen
                                property bool dragging:false
                                property int edgeDirection:0
                                property int edgeBudget:0
                                property point edgeAnchor
                                function updateEdge(point) {
                                    if(studio.mode==="flowMove" && studio.flowContainerId){
                                        const local=studio.mapToItem(preview,point.x,point.y),viewport=preview.scopeViewport(studio.flowContainerId)
                                        if(!viewport){edgeDirection=0;edgeBudget=0;return}
                                        const horizontal=studio.flowAxis==="x",coordinate=horizontal?local.x:local.y,start=horizontal?viewport.x:viewport.y,length=horizontal?viewport.width:viewport.height
                                        const band=Math.min(studio.spacing*2,length/6),direction=coordinate>=start+length-band?1:coordinate<=start+band?-1:0
                                        if(!direction){edgeDirection=0;edgeBudget=0;return}
                                        if(direction!==edgeDirection || Math.abs(horizontal?point.x-edgeAnchor.x:point.y-edgeAnchor.y)>=Qt.styleHints.startDragDistance){edgeBudget=studio.flowAxis==="x"?(studio.grid.visibleColumns || studio.grid.columns):studio.grid.rows;edgeAnchor=point}
                                        edgeDirection=direction;return
                                    }
                                    if(!studio.scrollingEnabled || studio.parentScopeId){edgeDirection=0;edgeBudget=0;return}
                                    const local=studio.mapToItem(preview,point.x,point.y),viewport=preview.pageViewport
                                    const band=Math.min(studio.spacing*2,viewport.height/6)
                                    const direction=local.y>=viewport.y+viewport.height-band?1:local.y<=viewport.y+band?-1:0
                                    if(!direction){edgeDirection=0;edgeBudget=0;return}
                                    // One viewport per deliberate edge movement. Holding an
                                    // idle pointer cannot grow an unbounded document forever.
                                    if(direction!==edgeDirection || Math.abs(point.y-edgeAnchor.y)>=Qt.styleHints.startDragDistance){edgeBudget=studio.grid.rows;edgeAnchor=point}
                                    edgeDirection=direction
                                }
                                Timer {
                                    interval:100;repeat:true
                                    running:pointerDrag.pressed && pointerDrag.dragging && (studio.mode==="move" || studio.mode==="flowMove") && pointerDrag.edgeDirection!==0 && pointerDrag.edgeBudget>0
                                    onTriggered:{
                                        if(studio.mode==="flowMove"){const before=studio.flowTargetIndex;studio.previewFlowTarget(before+pointerDrag.edgeDirection);pointerDrag.edgeBudget=studio.flowTargetIndex===before?0:pointerDrag.edgeBudget-1;return}
                                        const before=studio.candidate.y
                                        studio.handleCommand(pointerDrag.edgeDirection>0?inputManager.navigateDown:inputManager.navigateUp)
                                        pointerDrag.edgeBudget=studio.candidate.y===before?0:pointerDrag.edgeBudget-1
                                    }
                                }
                                onPressed:mouse=>{if(studio.mode!=="canvas"){mouse.accepted=false;return}studio.selectedId=modelData.id;studio.toolbarFocus=false;dragging=false;edgeDirection=0;edgeBudget=0;start=mapToItem(studio,mouse.x,mouse.y);originScreen=mapToItem(studio,0,0);edgeAnchor=start}
                                onPositionChanged:mouse=>{
                                    if(!pressed)return
                                    const point=mapToItem(studio,mouse.x,mouse.y)
                                    if(!dragging){if(Math.abs(point.x-start.x)+Math.abs(point.y-start.y)<Qt.styleHints.startDragDistance)return;if(studio.current && studio.current.locked){studio.message="Unlock this component before moving it.";return}studio.beginGesture(false);dragging=studio.mode==="move" || studio.mode==="flowMove"}
                                    if(dragging && studio.mode==="flowMove"){
                                        const local=mapToItem(preview,mouse.x,mouse.y)
                                        for(let index=0;index<studio.flowTargets.length;++index){const r=studio.flowTargetRect(index);if(r && local.x>=r.x && local.x<=r.x+r.width && local.y>=r.y && local.y<=r.y+r.height){studio.previewFlowTarget(index);break}}
                                        updateEdge(point)
                                        return
                                    }
                                    if(!dragging || studio.mode!=="move")return
                                    const screen=mapToItem(studio,0,0),metrics=preview.gridMetricsFor(studio.scopeId,studio.parentScopeId)
                                    let p=Object.assign({},studio.origin)
                                    p.x+=Math.round((point.x-start.x-screen.x+originScreen.x)/(metrics.cellW+metrics.gap))
                                    p.y+=Math.round((point.y-start.y-screen.y+originScreen.y)/(metrics.cellH+metrics.gap))
                                    if(p.x>=0 && p.y>=0 && p.x+p.width<=studio.grid.columns && p.y+p.height<=studio.placementRows){studio.candidate=p;preview.revealPlacement(p,studio.scopeId,studio.parentScopeId);studio.message=""}
                                    else studio.message="Grid boundary: choose a position inside the grid."
                                    updateEdge(point)
                                }
                                onReleased:{if(dragging){if(studio.mode==="move")studio.acceptCandidate();else if(studio.mode==="flowMove")studio.handleCommand(inputManager.confirm)}else if(studio.mode==="canvas"){studio.mode="context";studio.choice=0}dragging=false}
                                onCanceled:{if(dragging && (studio.mode==="move" || studio.mode==="flowMove"))studio.cancelGesture();dragging=false}
                            }
                            Rectangle {
                                objectName:"studioResize-"+modelData.id
                                visible:studio.gridMode && modelData.id===studio.selectedId && !studio.toolbarFocus && !modelData.locked && (studio.mode==="canvas" || studio.mode==="resize")
                                width:30*studio.textScale;height:width;anchors.right:parent.right;anchors.bottom:parent.bottom
                                radius:5*studio.textScale;color:studio.palette.focusColor;border.width:2;border.color:studio.palette.surfaceColor
                                Text {anchors.centerIn:parent;text:"↘";color:studio.palette.surfaceColor;font.pixelSize:22*studio.textScale}
                                MouseArea {
                                    anchors.fill:parent;property point start
                                    onPressed:mouse=>{start=Qt.point(mouse.x,mouse.y);studio.beginGesture(true)}
                                    onPositionChanged:mouse=>{if(!pressed)return;const metrics=preview.gridMetricsFor(studio.scopeId,studio.parentScopeId);let p=Object.assign({},studio.origin);p.width+=Math.round((mouse.x-start.x)/(metrics.cellW+metrics.gap));p.height+=Math.round((mouse.y-start.y)/(metrics.cellH+metrics.gap));if(p.width>=1 && p.height>=1 && p.x+p.width<=studio.grid.columns && p.y+p.height<=studio.placementRows && p.height<=studio.grid.rows)studio.candidate=p;else studio.message="Grid boundary: choose a size inside the grid."}
                                    onReleased:if(studio.mode==="resize")studio.acceptCandidate()
                                    onCanceled:if(studio.mode==="resize")studio.cancelGesture()
                                }
                            }
                        }
                    }
                    Rectangle {
                        objectName:"studioOrigin"
                        visible:!studio.addingType && !!studio.current && (studio.mode==="move" || studio.mode==="resize" || studio.mode==="size" || studio.mode==="collision" || studio.mode==="geometry")
                        readonly property var rect:studio.origin.x!==undefined?preview.placementRectFor(studio.origin,studio.originScopeId,studio.originParentId):({x:0,y:0,width:0,height:0})
                        x:rect.x;y:rect.y;width:rect.width;height:rect.height;color:"transparent";border.width:2;border.color:studio.palette.mutedColor
                    }
                    Rectangle {
                        objectName:"studioGhost"
                        visible:(studio.mode==="noRoom" && !!studio.addingType) || ((studio.mode==="move" || studio.mode==="size") && !studio.collisionPreview.valid) || studio.mode==="resize" || studio.mode==="geometry" || (studio.mode==="collision" && !studio.collisionPreview.valid)
                        readonly property var rect:preview.placementRectFor(studio.candidate,studio.scopeId,studio.parentScopeId)
                        x:rect.x;y:rect.y;width:rect.width;height:rect.height;color:"transparent"
                        readonly property var sourceSection:studio.page?(studio.page.sections || []).find(s=>(s.id || "")===studio.scopeId):null
                        HomeCard {visible:!((studio.addingType?studio.addPrototype.root:studio.current) || {}).componentType || ((studio.addingType?studio.addPrototype.root:studio.current) || {}).componentType!=="container" && ((studio.addingType?studio.addPrototype.root:studio.current) || {}).componentType!=="library";objectName:"studioGhostCard";anchors.fill:parent;enabled:false;unit:preview.componentUnit;outsideColor:studio.page?studio.page.backgroundEnd:studio.layoutModel.backgroundEnd;item:Object.assign({},studio.addingType?(studio.addPrototype.viewItems || [])[0] || {}:studio.current || {},{presentation:Object.assign({},parent.sourceSection?parent.sourceSection.presentation || {}:{},studio.addingType?((studio.addPrototype.viewItems || [])[0] || {}).presentation || {}:studio.current?studio.current.presentation || {}:{})});appearance:studio.page?studio.page.appearance:studio.palette;selected:true;opacity:studio.current && !studio.addingType && studio.current.opacity!==undefined?studio.current.opacity:1}
                        PlacedComponents {
                            anchors.fill:parent;enabled:false;previewOverlay:true;editorSelectionId:studio.addingType?(studio.addPrototype.root || {}).id || "":studio.selectedId
                            readonly property var component:studio.addingType?studio.addPrototype.root:studio.current
                            visible:!!component && (component.componentType==="container" || component.componentType==="library")
                            items:visible?[{item:component,placement:{x:0,y:0,width:1,height:1}}]:[]
                            metrics:({valid:true,margin:0,gap:0,cellW:width,cellH:height});host:preview;unit:preview.componentUnit
                            catalog:studio.addingType?studio.addPrototype.viewItems:studio.items
                        }
                        Text {anchors.bottom:parent.top;width:Math.min(studio.width-2*studio.spacing,Math.max(parent.width,200*studio.textScale));x:Math.max(studio.spacing-preview.x-parent.x,Math.min(0,studio.width-preview.x-parent.x-width-studio.spacing));text:(studio.mode==="resize"?"Sizing "+studio.resizeAxis:"Moving")+" · "+studio.componentName(studio.addingType?null:studio.current);color:studio.palette.textColor;style:Text.Outline;styleColor:studio.palette.surfaceColor;font.pixelSize:18*studio.textScale;elide:Text.ElideRight;Accessible.role:Accessible.StaticText;Accessible.name:studio.operationSummary}
                    }
                    Repeater {
                        model:studio.collisionPreview.valid ? studio.collisionPreview.placements : []
                        delegate:Rectangle {
                            required property var modelData
                            required property int index
                            readonly property var previewItem:studio.items.find(i=>i.id===modelData.id) || (studio.addPrototype.viewItems || []).find(i=>i.id===modelData.id) || {id:modelData.id,kind:"widget",widgetType:studio.addingType,name:studio.addingType}
                            objectName:"studioProposal-"+modelData.id
                            readonly property var section:studio.page.sections.find(s=>(s.id || "")===(modelData.categoryId || ""))
                            readonly property var rect:preview.placementRectFor(modelData.placement,modelData.categoryId!==undefined?modelData.categoryId:studio.scopeId,modelData.parentId || "")
                            x:rect.x;y:rect.y;width:rect.width;height:rect.height;color:"transparent"
                            HomeCard {objectName:"studioProposedCard-"+parent.modelData.id;visible:parent.previewItem.componentType!=="container" && parent.previewItem.componentType!=="library";anchors.fill:parent;enabled:false;unit:preview.componentUnit;outsideColor:studio.page.backgroundEnd;item:Object.assign({},parent.previewItem,{presentation:Object.assign({},parent.section?parent.section.presentation || {}:{},parent.previewItem.presentation || {})});appearance:studio.page.appearance;selected:parent.modelData.id===studio.selectedId || !!studio.addingType && parent.modelData.id===(studio.addPrototype.root || {}).id;opacity:parent.previewItem.opacity===undefined?1:parent.previewItem.opacity}
                            PlacedComponents {
                                anchors.fill:parent;enabled:false;previewOverlay:true;editorSelectionId:studio.addingType?(studio.addPrototype.root || {}).id || "":studio.selectedId
                                visible:parent.previewItem.componentType==="container" || parent.previewItem.componentType==="library"
                                items:visible?[{item:parent.previewItem,placement:{x:0,y:0,width:1,height:1}}]:[]
                                metrics:({valid:true,margin:0,gap:0,cellW:width,cellH:height})
                                host:preview;unit:preview.componentUnit;catalog:studio.addingType?studio.items.concat(studio.addPrototype.viewItems || []):studio.items
                            }
                            Text {anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:7;text:(index+1)+" · "+studio.componentName(parent.previewItem);color:studio.palette.textColor;style:Text.Outline;styleColor:studio.palette.surfaceColor;font.pixelSize:15*studio.textScale;elide:Text.ElideRight}
                        }
                    }
                }
                }
            }
            Text {anchors.centerIn:parent;visible:!studio.items.length && studio.mode==="canvas";text:"Empty page · Confirm to add a component";color:studio.palette.textColor;font.pixelSize:26*studio.textScale}
        }
        Text {parent:studio;anchors.bottom:parent.bottom;anchors.left:parent.left;anchors.margins:studio.spacing;z:2;width:parent.width-2*studio.spacing;visible:!studio.toolbarFocus && studio.mode!=="preview" && studio.mode!=="catalog";text:studio.footerText;color:studio.palette.textColor;style:Text.Outline;styleColor:studio.palette.surfaceColor;wrapMode:Text.Wrap;font.pixelSize:17*studio.textScale}
    }
    Rectangle {
        objectName:"studioScrollingPanel";visible:studio.mode==="scrolling";z:6
        width:Math.min(parent.width-2*studio.spacing,480*studio.textScale);height:scrollColumn.implicitHeight+2*studio.spacing
        anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.margins:studio.spacing
        color:studio.palette.surfaceColor;radius:studio.spacing;border.width:2;border.color:studio.palette.focusColor
        Column {
            id:scrollColumn;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:studio.spacing;spacing:studio.spacing
            Text {text:"Page layout";color:studio.palette.textColor;font.pixelSize:24*studio.textScale}
            CheckBox {
                id:scrollToggle;objectName:"studioScrollingToggle";width:parent.width;height:48*studio.textScale
                checkable:false;checked:studio.scrollingDraft;Accessible.name:"Allow vertical scrolling"
                indicator:Rectangle {x:0;anchors.verticalCenter:parent.verticalCenter;width:28*studio.textScale;height:width;radius:4*studio.textScale;color:scrollToggle.checked?studio.palette.focusColor:studio.palette.surfaceColor;border.width:studio.choice===0?3:1;border.color:studio.palette.focusColor;Text {anchors.centerIn:parent;text:scrollToggle.checked?"✓":"";color:studio.palette.surfaceColor}}
                contentItem:Text {leftPadding:40*studio.textScale;text:"Allow vertical scrolling";verticalAlignment:Text.AlignVCenter;color:studio.palette.textColor;font.pixelSize:18*studio.textScale;wrapMode:Text.Wrap}
                onClicked:studio.previewScrolling(!studio.scrollingDraft)
            }
            Text {visible:!!studio.message;width:parent.width;text:studio.message;color:studio.palette.textColor;font.pixelSize:18*studio.textScale;wrapMode:Text.Wrap}
            Repeater {
                model:["Done","Cancel"]
                delegate:Rectangle {
                    required property string modelData;required property int index
                    width:scrollColumn.width;height:44*studio.textScale;radius:6*studio.textScale
                    color:studio.choice===index+1?studio.palette.selectedColor:studio.palette.surfaceColor
                    border.width:studio.choice===index+1?2:0;border.color:studio.palette.focusColor
                    Text {anchors.centerIn:parent;text:modelData;color:studio.palette.textColor;font.pixelSize:18*studio.textScale}
                    Accessible.role:Accessible.Button;Accessible.name:modelData
                    MouseArea {anchors.fill:parent;onClicked:{studio.choice=index+1;studio.handleCommand(inputManager.confirm)}}
                }
            }
        }
    }
    Rectangle {
        id:moveActions;objectName:"studioMoveActions";visible:studio.open && (studio.mode==="move" || studio.mode==="flowMove");z:4
        width:Math.min(studio.width-2*studio.spacing,480*studio.textScale);height:moveActionColumn.implicitHeight+2*studio.spacing
        readonly property var target:studio.mode==="flowMove"?(preview.editRect || {x:0,y:0,width:0,height:0}):preview.placementRectFor(studio.candidate,studio.scopeId,studio.parentScopeId)
        readonly property point targetPoint:preview.mapToItem(studio,target.x,target.y)
        x:targetPoint.x+target.width*camera.scale/2<studio.width/2?studio.width-width-studio.spacing:studio.spacing
        y:studio.spacing;color:studio.palette.surfaceColor;border.width:1;border.color:studio.palette.mutedColor;radius:studio.spacing/2
        Column {
            id:moveActionColumn;width:parent.width-2*studio.spacing;x:studio.spacing;y:studio.spacing;spacing:studio.spacing/2
            Text {width:parent.width;text:studio.mode==="flowMove"?"Position "+(studio.flowTargetIndex+1)+" of "+studio.flowIds.length:studio.parentScopeId?"Move into "+studio.componentName(studio.parentGroup):studio.sectionMode?"Move to "+(studio.selectedSection?studio.selectedSection.name:"Uncategorized"):"Move on page";color:studio.palette.textColor;font.pixelSize:18*studio.textScale;elide:Text.ElideRight}
            Row {
                width:parent.width;spacing:studio.spacing/2
                Repeater {
                    model:studio.mode==="flowMove"?["Place here","Cancel"]:["Place here","Options…","Cancel"]
                    delegate:Rectangle {
                        required property int index;required property string modelData
                        objectName:studio.mode==="flowMove"?["studioMovePlace","studioMoveCancel"][index]:["studioMovePlace","studioMoveOptions","studioMoveCancel"][index]
                        readonly property bool available:index!==0 || (studio.mode==="flowMove"?studio.flowPreviewValid:!!studio.collisionPreview.valid)
                        width:(moveActionColumn.width-studio.spacing*(studio.mode==="flowMove"?0.5:1))/(studio.mode==="flowMove"?2:3);height:44*studio.textScale;radius:studio.spacing/4
                        color:index===0?studio.palette.selectedColor:"transparent";border.width:1;border.color:studio.palette.mutedColor
                        Text {anchors.centerIn:parent;text:modelData;color:parent.available?studio.palette.textColor:studio.palette.mutedColor;font.pixelSize:16*studio.textScale}
                        Accessible.role:Accessible.Button;Accessible.name:modelData
                        MouseArea {anchors.fill:parent;enabled:parent.available;onClicked:studio.handleCommand(index===0?inputManager.confirm:studio.mode!=="flowMove" && index===1?inputManager.menu:inputManager.back)}
                    }
                }
            }
        }
    }
    Rectangle {
        id:sizePanel;objectName:"studioSizePanel";visible:studio.mode==="size";z:4
        width:Math.min(parent.width-2*studio.spacing,studio.layoutModel.studioOptions.menuWidth*1.4*studio.textScale)
        height:sizeColumn.implicitHeight+2*studio.spacing
        readonly property var targetRect:{
            if(!studio.gridMode || !studio.current)return {x:0,y:0,width:0,height:0}
            const r=preview.placementRectFor(studio.candidate,studio.scopeId,studio.parentScopeId)
            const p=preview.mapToItem(studio,r.x,r.y),end=preview.mapToItem(studio,r.x+r.width,r.y+r.height)
            return {x:p.x,y:p.y,width:end.x-p.x,height:end.y-p.y}
        }
        x:targetRect.x+targetRect.width+width+2*studio.spacing<=parent.width?targetRect.x+targetRect.width+studio.spacing:targetRect.x-width-2*studio.spacing>=0?targetRect.x-width-studio.spacing:parent.width-width-studio.spacing
        y:Math.max(studio.spacing,Math.min(parent.height-height-studio.spacing,targetRect.y))
        color:studio.palette.surfaceColor;radius:studio.spacing/2;border.width:2;border.color:studio.palette.focusColor
        Column {
            id:sizeColumn;anchors.left:parent.left;anchors.right:parent.right;anchors.margins:studio.spacing;y:studio.spacing;spacing:studio.spacing/2
            Text {text:"Size · "+studio.componentName(studio.addingType?null:studio.current);width:parent.width;elide:Text.ElideRight;color:studio.palette.textColor;font.pixelSize:22*studio.textScale}
            Repeater {
                model:studio.mode==="size" && studio.sizeControlsVisible?["width","height"]:[]
                delegate:Column {
                    id:dimensionRow
                    required property string modelData;required property int index
                    width:sizeColumn.width;spacing:studio.spacing/4
                    readonly property int maximum:studio.gridMode?(modelData==="width"?studio.grid.columns:studio.grid.rows):1
                    Text {text:(parent.modelData==="width"?"Width":"Height")+" · "+studio.candidate[parent.modelData];color:studio.palette.textColor;font.pixelSize:18*studio.textScale}
                    Slider {
                        id:dimensionSlider;objectName:"studioSize-"+dimensionRow.modelData;width:parent.width;height:2*studio.spacing
                        from:1;to:dimensionRow.maximum;stepSize:1;snapMode:Slider.SnapAlways;value:studio.candidate[dimensionRow.modelData]
                        Accessible.name:dimensionRow.modelData==="width"?"Component width":"Component height"
                        onMoved:{studio.choice=dimensionRow.index;studio.setDimension(dimensionRow.modelData,Math.round(value))}
                        background:Rectangle {x:dimensionSlider.leftPadding;y:dimensionSlider.topPadding+(dimensionSlider.availableHeight-height)/2;width:dimensionSlider.availableWidth;height:studio.spacing/4;radius:height/2;color:studio.palette.mutedColor}
                        handle:Rectangle {x:dimensionSlider.leftPadding+dimensionSlider.visualPosition*(dimensionSlider.availableWidth-width);y:dimensionSlider.topPadding+(dimensionSlider.availableHeight-height)/2;width:studio.spacing;height:width;radius:width/2;color:studio.palette.focusColor;border.width:studio.choice===dimensionRow.index?3:1;border.color:studio.palette.textColor}
                    }
                    Row {
                        width:parent.width
                        Text {width:parent.width/2;text:"1";color:studio.palette.mutedColor;font.pixelSize:14*studio.textScale}
                        Text {width:parent.width/2;horizontalAlignment:Text.AlignRight;text:modelData==="width"?"Full width":"Full height";color:studio.palette.mutedColor;font.pixelSize:14*studio.textScale}
                    }
                }
            }
            Text {visible:studio.sizeControlsVisible && !studio.collisionPreview.valid;text:studio.collisionPreview.reason || studio.message;width:parent.width;wrapMode:Text.Wrap;color:studio.palette.textColor;font.pixelSize:16*studio.textScale}
            Repeater {
                model:studio.sizeControlsVisible?["Done","Hide controls","Advanced…","Cancel"]:["Show size controls"]
                delegate:Rectangle {
                    required property int index;required property string modelData
                    width:sizeColumn.width;height:2*studio.spacing;radius:studio.spacing/4
                    color:!studio.sizeControlsVisible || studio.choice===index+2?studio.palette.selectedColor:"transparent";border.width:!studio.sizeControlsVisible || studio.choice===index+2?2:0;border.color:studio.palette.focusColor
                    Accessible.role:Accessible.Button;Accessible.name:modelData
                    Text {anchors.fill:parent;anchors.margins:studio.spacing/4;verticalAlignment:Text.AlignVCenter;text:modelData;color:studio.palette.textColor;font.pixelSize:18*studio.textScale}
                    MouseArea {anchors.fill:parent;onClicked:{if(!studio.sizeControlsVisible){studio.sizeControlsVisible=true;return}studio.choice=index+2;studio.handleCommand(inputManager.confirm)}}
                }
            }
        }
    }
    Rectangle {
        visible:studio.mode!=="canvas" && studio.mode!=="flowMove" && studio.mode!=="move" && studio.mode!=="resize" && studio.mode!=="size" && studio.mode!=="preview" && studio.mode!=="inspector" && studio.mode!=="catalog" && studio.mode!=="scrolling" && studio.mode!=="arrange" && studio.mode!=="childLayout" && studio.mode!=="convert"
        z:3
        width:Math.min(parent.width-2*studio.spacing,560*studio.textScale);height:Math.min(parent.height-2*studio.spacing,dialogColumn.implicitHeight+2*studio.spacing)
        readonly property var localSelection:studio.gridMode && studio.current && studio.current.placement?preview.placementRectFor(studio.current.placement,studio.scopeId,studio.current.parentId || ""):preview.editRect
        readonly property var selectionRect:localSelection?Object.assign({},localSelection,{x:preview.x+localSelection.x,y:preview.y+localSelection.y}):null
        x:selectionRect && (studio.mode==="context" || studio.mode==="properties")?(selectionRect.x+selectionRect.width+width+2*studio.spacing<=studio.width?selectionRect.x+selectionRect.width+studio.spacing:Math.max(studio.spacing,selectionRect.x-width-studio.spacing)):(parent.width-width)/2
        y:selectionRect && (studio.mode==="context" || studio.mode==="properties")?Math.max(studio.spacing,Math.min(parent.height-height-studio.spacing,selectionRect.y)):(parent.height-height)/2
        color:studio.palette.surfaceColor;border.width:3;border.color:studio.palette.focusColor;radius:studio.spacing
        Column {
            id:dialogColumn;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:studio.spacing;spacing:studio.spacing/2
            Text {id:dialogHeading;width:parent.width;text:studio.operationTitle?studio.operationTitle:studio.mode==="context" || studio.mode==="properties"?(studio.current?studio.current.name || studio.current.widgetType || "Component":"Component"):studio.mode==="pageContext"?(studio.pageEntries.find(p=>p.id===studio.pageId)||{}).name:studio.mode==="pageAppearance"?"Page appearance":studio.mode==="more"?"More tools":studio.mode.toUpperCase();color:studio.palette.textColor;font.pixelSize:24*studio.textScale;elide:Text.ElideRight}
            Text {id:dialogMessage;width:parent.width;text:studio.mode==="apply" ? "Save the complete interface draft: "+(session?session.changeCount:0)+" operations across Home and pages." : studio.mode==="remove" ? "Remove "+studio.componentName(studio.current)+" and its children? This is one undoable draft change; Apply saves it." : studio.mode==="collision" && studio.choice!==2 ? (studio.collisionPreview.valid ? "Preview: "+studio.collisionPreview.placements.length+" affected components. Confirm commits one undoable draft operation." : "Unavailable: "+studio.collisionPreview.reason) : studio.message;color:studio.palette.mutedColor;wrapMode:Text.Wrap;font.pixelSize:18*studio.textScale}
            ListView {
                width:parent.width;height:Math.min(studio.dialogLabels().length*42*studio.textScale,Math.max(42*studio.textScale,studio.height-dialogHeading.implicitHeight-dialogMessage.implicitHeight-6*studio.spacing))
                clip:true;currentIndex:studio.choice;highlightRangeMode:ListView.ApplyRange
                model:studio.mode==="keyboard" ? []:studio.dialogLabels()
                delegate:Rectangle {
                    required property int index;required property string modelData
                    objectName:"studioDialogChoice-"+index
                    readonly property bool available:studio.mode==="conversionPreview" && index===0?studio.gridPreviewValid && !studio.gridGeometryPending && !preview.layoutError:studio.mode!=="collision" || index>1 || (index===0?!!studio.shiftPreview.valid:!!studio.swapPreview.valid)
                    width:dialogColumn.width;height:42*studio.textScale;color:studio.choice===index?studio.palette.selectedColor:"transparent";border.width:studio.choice===index?2:0;border.color:studio.palette.focusColor
                    Text {anchors.fill:parent;anchors.margins:8;text:modelData;color:parent.available?studio.palette.textColor:studio.palette.mutedColor;font.pixelSize:18*studio.textScale;elide:Text.ElideRight}
                    Accessible.role:Accessible.Button;Accessible.name:modelData;Accessible.description:available?"":"Unavailable. Choose another proposal or Cancel."
                    MouseArea {anchors.fill:parent;onClicked:{studio.choice=index;studio.activateChoice()}}
                }
            }
        }
    }
    Rectangle {
        id:arrangementPanel
        objectName:"studioArrangement"
        visible:studio.open && studio.mode==="arrange";z:4
        width:Math.min(studio.width-2*studio.spacing,480*studio.textScale)
        height:Math.min(studio.height-2*studio.spacing,arrangementColumn.implicitHeight+2*studio.spacing)
        // Keep the authored selection visible whenever one side has room.
        readonly property var selectedRect:studio.gridMode && studio.current && studio.current.placement?preview.placementRectFor(studio.current.placement,studio.scopeId,studio.current.parentId || ""):preview.editRect
        readonly property point selectedPoint:selectedRect?preview.mapToItem(studio,selectedRect.x,selectedRect.y):Qt.point(studio.width/2,0)
        x:selectedPoint.x<studio.width/2?studio.width-width-studio.spacing:studio.spacing
        y:Math.max(studio.spacing,(studio.height-height)/2)
        color:studio.palette.surfaceColor;border.color:studio.palette.focusColor;border.width:2;radius:studio.spacing
        Column {
            id:arrangementColumn
            anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:studio.spacing
            spacing:studio.spacing
            Text {text:"Arrange selection";color:studio.palette.textColor;font.pixelSize:24*studio.textScale}
            Text {text:studio.multiSelection.length+" components";color:studio.palette.mutedColor;font.pixelSize:16*studio.textScale}
            Grid {
                width:parent.width;columns:3;spacing:studio.spacing/2
                Repeater {
                    model:6
                    delegate:Rectangle {
                        required property int index
                        objectName:"studioAlign-"+studio.arrangementModes[index]
                        width:(arrangementColumn.width-studio.spacing)/3;height:96*studio.textScale
                        radius:studio.spacing/2;color:studio.choice===index?studio.palette.selectedColor:"transparent"
                        border.width:studio.choice===index?2:1;border.color:studio.choice===index?studio.palette.focusColor:studio.palette.mutedColor
                        Item {
                            width:42*studio.textScale;height:28*studio.textScale;anchors.horizontalCenter:parent.horizontalCenter;y:12*studio.textScale
                            Repeater {
                                model:3
                                delegate:Rectangle {
                                    required property int index
                                    readonly property int alignment:parent.parent.index%3
                                    readonly property bool horizontal:parent.parent.index<3
                                    readonly property real length:(12+index*8)*studio.textScale
                                    width:horizontal?length:5*studio.textScale;height:horizontal?5*studio.textScale:length
                                    x:horizontal?(alignment===0?0:alignment===1?(parent.width-width)/2:parent.width-width):index*12*studio.textScale
                                    y:horizontal?index*10*studio.textScale:alignment===0?0:alignment===1?(parent.height-height)/2:parent.height-height
                                    color:studio.palette.textColor;radius:studio.textScale
                                }
                            }
                        }
                        Text {anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.margins:8*studio.textScale;text:studio.arrangementNames[index];horizontalAlignment:Text.AlignHCenter;wrapMode:Text.Wrap;color:studio.palette.textColor;font.pixelSize:16*studio.textScale}
                        Accessible.role:Accessible.Button;Accessible.name:"Align "+studio.arrangementNames[index]
                        MouseArea {anchors.fill:parent;onClicked:studio.previewArrangement(index)}
                    }
                }
            }
            Row {
                width:parent.width;spacing:studio.spacing/2
                Repeater {
                    model:2
                    delegate:Rectangle {
                        required property int index
                        objectName:"studioDistribute-"+index
                        width:(arrangementColumn.width-studio.spacing/2)/2;height:64*studio.textScale;radius:studio.spacing/2
                        color:studio.choice===index+6?studio.palette.selectedColor:"transparent";border.width:studio.choice===index+6?2:1;border.color:studio.choice===index+6?studio.palette.focusColor:studio.palette.mutedColor
                        Text {anchors.centerIn:parent;width:parent.width-16*studio.textScale;text:(index===0?"↔  ":"↕  ")+studio.arrangementNames[index+6];wrapMode:Text.Wrap;horizontalAlignment:Text.AlignHCenter;color:studio.palette.textColor;font.pixelSize:16*studio.textScale}
                        Accessible.role:Accessible.Button;Accessible.name:studio.arrangementNames[index+6]
                        MouseArea {anchors.fill:parent;onClicked:studio.previewArrangement(index+6)}
                    }
                }
            }
            Text {width:parent.width;visible:text.length>0;text:studio.message;wrapMode:Text.Wrap;color:studio.arrangementValid?studio.palette.textColor:studio.palette.mutedColor;font.pixelSize:16*studio.textScale}
            Row {
                width:parent.width;spacing:studio.spacing/2
                Repeater {
                    model:["Done","Cancel"]
                    delegate:Rectangle {
                        required property int index;required property string modelData
                        objectName:index===0?"studioArrangeDone":"studioArrangeCancel"
                        width:(arrangementColumn.width-studio.spacing/2)/2;height:48*studio.textScale;radius:studio.spacing/2
                        color:studio.choice===index+8?studio.palette.selectedColor:"transparent";border.width:studio.choice===index+8?2:0;border.color:studio.palette.focusColor
                        Text {anchors.centerIn:parent;text:modelData;color:index===0 && !studio.arrangementValid?studio.palette.mutedColor:studio.palette.textColor;font.pixelSize:18*studio.textScale}
                        Accessible.role:Accessible.Button;Accessible.name:modelData;Accessible.description:index===0 && !studio.arrangementValid?"Choose a valid arrangement first":""
                        MouseArea {anchors.fill:parent;onClicked:{studio.choice=index+8;studio.finishArrangement(index===0)}}
                    }
                }
            }
        }
    }
    StudioGridEditor {
        id:gridEditor;objectName:"studioGridEditor";anchors.fill:parent;z:5
        visible:studio.open && studio.mode==="convert"
        value:studio.gridDraft;sectionHeight:studio.sectionHeightDraft;modes:studio.gridModes
        fields:[studio.gridField(1),studio.gridField(2),studio.gridField(3),studio.gridField(4),studio.gridField(5)]
        metrics:preview.gridMetricsFor(studio.scopeId,"");appearance:studio.palette;choice:studio.choice
        message:studio.message;valid:studio.gridPreviewValid
        selectionRect:{
            if(!studio.current || !studio.current.placement)return null
            const rect=preview.placementRectFor(studio.current.placement,studio.scopeId,studio.current.parentId || "")
            const point=preview.mapToItem(studio,rect.x,rect.y)
            return {x:point.x,y:point.y,width:rect.width*camera.scale,height:rect.height*camera.scale}
        }
        onFocused:index=>studio.choice=index
        onProposed:(value,height)=>studio.previewGrid(value,height)
        onActivated:index=>{studio.choice=index;studio.activateChoice()}
        onCanceled:studio.cancelGrid()
    }
    StudioLayoutEditor {
        id:childLayoutEditor;objectName:"studioChildLayoutEditor"
        anchors.fill:parent;z:5;visible:studio.open && studio.mode==="childLayout"
        value:studio.childLayoutDraft;appearance:studio.palette;message:studio.message;valid:studio.childLayoutValid
        selectionRect: {
            if(!studio.current || !studio.current.placement)return null
            const r=preview.placementRectFor(studio.current.placement,studio.scopeId,studio.current.parentId || "")
            const point=preview.mapToItem(studio,r.x,r.y)
            return {x:point.x,y:point.y,width:r.width*camera.scale,height:r.height*camera.scale}
        }
        onProposed:value=>studio.previewChildLayout(value)
        onAccepted:studio.finishChildLayout(true)
        onCanceled:studio.finishChildLayout(false)
    }
    StudioCatalog {
        id:catalog;anchors.fill:parent;z:5;visible:studio.open && studio.mode==="catalog"
        provider:studio.provider;session:studio.session;options:visible && studio.layoutModel?studio.layoutModel.studioOptions:({});appearance:studio.palette
        onCanceled:{studio.mode="canvas";studio.addingType="";studio.pendingScrollEnable=false}
        onPicked:(type,configuration)=>studio.startAddition(type,configuration)
    }
    StudioInspector {
        id:inspector;anchors.fill:parent;z:5;visible:studio.open && studio.mode==="inspector"
        provider:studio.provider;session:studio.session;page:studio.page;pageId:studio.pageId;options:studio.layoutModel?studio.layoutModel.studioOptions:({});appearance:studio.palette
        selectedIds:studio.multiSelection.length>1 && studio.multiSelection.indexOf(studio.selectedId)>=0?studio.multiSelection:studio.selectedId?[studio.selectedId]:[]
        selectionRect:{
            if(inspector.pageMode || inspector.structureType || !studio.current || !studio.current.placement)return null
            const r=preview.placementRectFor(studio.current.placement,studio.scopeId,studio.current.parentId || "")
            const p=preview.mapToItem(studio,r.x,r.y)
            return {x:p.x,y:p.y,width:r.width*camera.scale,height:r.height*camera.scale}
        }
        onClosed:{studio.mode="canvas";studio.toolbarFocus=false}
        onOperation:name=>studio.inspectorOperation(name)
        onSamplesChanged:enabled=>studio.samplePreview=enabled
    }
    OnScreenKeyboard {
        id:keyboard;objectName:"studioKeyboard";visible:studio.mode==="keyboard";anchors.fill:parent;z:10;appearance:studio.palette;hint:studio.message
        onCanceled:studio.mode=studio.keyboardReturnMode
        onAccepted:text=>{
            if(studio.keyboardAction==="navAdd" || studio.keyboardAction==="sectionAdd"){
                const section=studio.keyboardAction==="sectionAdd";if(studio.command(section?"section":"navigation",{kind:"add",name:text})){studio.mode=section?"sections":"navigation";studio.choice=0}return
            }
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
                const key=studio.keyboardAction.slice(5),field=studio.gridField(({columns:1,rows:2,gap:3,margin:4,sectionHeight:5})[key]),number=Number(text)
                if(!text.trim() || !Number.isFinite(number) || (field.integer && !Number.isInteger(number)) || number<field.min || number>field.max){studio.message="Enter a value between "+field.min+" and "+field.max+".";return}
                const value=Object.assign({},studio.gridDraft),height=key==="sectionHeight"?number:studio.sectionHeightDraft
                if(key!=="sectionHeight")value[key]=number
                studio.mode=studio.keyboardReturnMode;studio.previewGrid(value,height);return
            }
            if(studio.keyboardAction.indexOf("geometry")===0) {
                const field=Number(studio.keyboardAction.slice(8)), number=Number(text)
                if(!Number.isInteger(number) || number<1){studio.message="Enter a positive whole number.";return}
                let p=Object.assign({},studio.candidate)
                if(field===0)p.x=number-1
                else if(field===1)p.y=number-1
                else if(field===2)p.width=number
                else p.height=number
                if(p.x+p.width>studio.grid.columns || p.y+p.height>studio.placementRows || p.height>studio.grid.rows){studio.message="The placement exceeds the grid.";return}
                studio.candidate=p;preview.revealPlacement(p,studio.scopeId,studio.parentScopeId);studio.mode="geometry";return
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
                if(studio.mode==="canvas" && studio.sectionMode && studio.current && !studio.scopeOverview){if(!studio.current.parentId)studio.scopeId=studio.current.categoryId || "";preview.revealPlacement(studio.current.placement,studio.scopeId)}
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
