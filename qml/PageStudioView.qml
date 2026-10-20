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
    property int choice: 0
    property string message: ""
    property string addingType: ""
    property var candidate: ({x:0,y:0,width:1,height:1})
    property var origin: ({})
    property var proposal: ({})
    property var gridDraft: ({})
    property string keyboardAction: ""
    property string keyboardReturnMode: "context"
    property var palette: layoutModel ? layoutModel.appearance : ({})
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
        if(!page || !page.composition.grid)return false
        for(const item of items)if(!item.placement)return false
        if(page.composition.grid.mode===undefined)for(const section of page.sections || []) {
            const style=section.presentation || {};if(style.variant==="library" || style.trailingAction)return false
        }
        return true
    }
    readonly property var tools: ["Add", "Pages", "Layers", "Undo", "Redo", "Grid", "Fit", "Preview", "Apply", "Scope"]
    readonly property var contextActions: ["Move", "Resize", "Rename", "Duplicate", "Remove", "Text", "Surface opacity", "Corners", "Position and size"]
    readonly property var names: ["Clock", "Text"]
    readonly property var current: items.find(i=>i.id===selectedId) || null
    readonly property var collisionPreview: {
        if(mode!=="collision" || choice===2 || !session)return ({})
        return addingType ? session.additionProposal(addingType,candidate,choice===0?"shift":"swap",pageId,sectionMode?scopeId:"")
                          : session.placementProposal(selectedId,candidate,choice===0?"shift":"swap",pageId)
    }
    readonly property real textScale: palette.typographyScale || 1
    readonly property real spacing: palette.spacing || 20
    signal closed()
    visible: open
    z: 100
    onSelectedIdChanged:if(current && sectionMode)scopeId=current.categoryId || ""
    onScopeIdChanged:if(open && sectionMode && !scopeOverview)Qt.callLater(()=>{if(open && sectionMode)preview.revealScope(scopeId)})

    function show(id, component) {
        session = layoutModel.beginStudio()
        pageId = id || ""; selectedId = component || ""
        mode = "canvas"; toolbarFocus = true; tool = 0; message = "Draft only. Apply saves all changed pages."
        scopeOverview=false;scopeId=current?current.categoryId || "":page && page.sections.length?page.sections[0].id || "":""
        open = true
    }
    function close() { if(session) session.invalidate(); open=false; closed() }
    function command(op,args) {
        if (!session || !session.execute(op,args || {},op==="newPage" || op==="renamePage" ? "" : pageId)) {
            message = session ? session.errorString : "The edit session is closed."
            return false
        }
        message = "Draft updated. Nothing applied yet."
        return true
    }
    function position(item) { return item && item.placement ? Object.assign({},item.placement) : ({x:0,y:0,width:1,height:1}) }
    function beginGesture(resize) {
        if (!gridMode) { beginGrid(); return }
        if (!current) { message="Choose a component in Layers first."; return }
        addingType=""; origin=position(current); candidate=Object.assign({},origin)
        mode=resize ? "resize" : "move"; toolbarFocus=false
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
            if(!proposal.valid){message=proposal.reason;const previous=mode;mode="collision";if(previous!=="collision")choice=0;return}
            if(command("batch",{commands:proposal.commands})){selectedId=proposal.id;addingType="";mode="canvas"}
            return
        }
        proposal=session.placementProposal(selectedId,candidate,strategy || "cancel",pageId)
        if (!proposal.valid) { message=proposal.reason; const previous=mode; mode="collision"; if(previous!=="collision")choice=0; return }
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
    }
    function dialogLabels() {
        if(mode==="context") return contextActions
        if(mode==="layers") return items.map(i=>i.name || i.widgetType || i.id)
        if(mode==="pages") return ["Home"].concat(draftModel.pages.map(p=>p.name)).concat(["Create blank page"])
        if(mode==="add") return names
        if(mode==="collision") return ["Shift others", "Swap", "Cancel"]
        if(mode==="geometry") return ["Column: "+(candidate.x+1), "Row: "+(candidate.y+1), "Width: "+candidate.width, "Height: "+candidate.height, "Accept placement", "Cancel"]
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
        if(mode==="context") {context(choice);return}
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
        if(mode==="add") {addingType=choice===0 ? "clock" : "text";origin={};candidate={x:0,y:0,width:1,height:1};mode="move";toolbarFocus=false;return}
        if(mode==="collision") {if(choice===2){mode="canvas";addingType=""}else acceptCandidate(choice===0 ? "shift":"swap");return}
        if(mode==="geometry") {
            if(choice===5)mode="canvas"
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
            if(mode!=="canvas") {if(mode==="conversionPreview")session.cancelPreview();mode="canvas";addingType="";return}
            if(session.dirty){mode="dirty";choice=0}else close()
            return
        }
        if(mode==="move" || mode==="resize") {
            if(c===inputManager.confirm){acceptCandidate();return}
            if(c===inputManager.menu){mode="geometry";choice=0;return}
            const dx=c===inputManager.navigateRight?1:c===inputManager.navigateLeft?-1:0
            const dy=c===inputManager.navigateDown?1:c===inputManager.navigateUp?-1:0
            let p=Object.assign({},candidate)
            if(mode==="move"){p.x+=dx;p.y+=dy}else{p.width+=dx;p.height+=dy}
            if(p.x<0 || p.y<0 || p.width<1 || p.height<1 || p.x+p.width>grid.columns || p.y+p.height>grid.rows){message="Grid boundary: choose a position or size inside the grid.";return}
            candidate=p;preview.revealPlacement(p,scopeId);message="Row "+(p.y+1)+", column "+(p.x+1)+", "+p.width+" × "+p.height;return
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
        if(c===inputManager.menu){mode="context";choice=0;return}
        if(c===inputManager.search){chooseTool(0);return}
        if(toolbarFocus) {
            if(c===inputManager.navigateLeft)tool=Math.max(0,tool-1)
            else if(c===inputManager.navigateRight)tool=Math.min(tools.length-1,tool+1)
            else if(c===inputManager.navigateDown){toolbarFocus=false;if(!current && items.length)selectedId=items[0].id}
            else if(c===inputManager.confirm)chooseTool(tool)
        } else {
            if(c===inputManager.confirm){beginGesture(false);return}
            const index=items.findIndex(i=>i.id===selectedId)
            if(gridMode && current && current.placement) {
                const p=current.placement, cx=p.x+p.width/2, cy=p.y+p.height/2
                const dx=c===inputManager.navigateRight?1:c===inputManager.navigateLeft?-1:0
                const dy=c===inputManager.navigateDown?1:c===inputManager.navigateUp?-1:0
                let best=null, score=Infinity
                for(const item of scopedItems) {
                    if(item.id===selectedId || !item.placement)continue
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
        Text {width:parent.width;elide:Text.ElideRight;text:"Page Studio  ·  "+(studio.pageId ? (studio.draftModel.pages.find(p=>p.id===studio.pageId)||{}).name:"Home")+"  ·  "+(session && session.dirty ? "Draft: "+session.changeCount+" changes":"Applied baseline");color:studio.palette.textColor;font.pixelSize:26*studio.textScale}
        Row {
            spacing:studio.spacing/3
            Repeater {
                model:studio.tools
                delegate:Rectangle {
                    required property int index;required property string modelData
                    width:Math.max(65, (studio.width-3*studio.spacing)/studio.tools.length-studio.spacing/3);height:44*studio.textScale;radius:studio.palette.tileRoundness*30
                    color:studio.palette.selectedColor;border.width:studio.toolbarFocus && studio.tool===index?3:0;border.color:studio.palette.focusColor
                    Text {anchors.centerIn:parent;text:modelData;color:studio.palette.textColor;font.pixelSize:17*studio.textScale}
                    MouseArea {anchors.fill:parent;onClicked:{studio.toolbarFocus=true;studio.chooseTool(index)}}
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
                    id:preview;objectName:"studioPreview";anchors.fill:parent
                    items:studio.page?studio.page.items:[];sections:studio.page?studio.page.sections:[]
                    appearance:studio.page?studio.page.appearance:studio.palette;composition:studio.page?studio.page.composition:({})
                    grid:studio.pageGrid;placements:studio.page?studio.page.placements:[]
                    editMode:true;scopeView:studio.sectionMode && !studio.scopeOverview;editScopeId:studio.scopeId
                    brandVisible:studio.page?studio.page.brandVisible:false;tileRoundness:studio.page?studio.page.tileRoundness:0
                    backgroundStart:studio.page?studio.page.backgroundStart:studio.layoutModel.backgroundStart
                    backgroundEnd:studio.page?studio.page.backgroundEnd:studio.layoutModel.backgroundEnd
                    internetAvailable:false;localNetworkAvailable:false;hasContextualSources:false
                    onActionActivated:(action,originId)=>studio.message="Preview only: would invoke "+action.type
                    onSettingsRequested:section=>studio.message="Return to editing to change settings."
                    onSearchRequested:studio.message="Search is unavailable in draft preview."
                }
                Item {
                    anchors.fill:parent;visible:studio.mode!=="preview"
                    Repeater {
                        model:studio.gridMode ? studio.grid.columns*studio.grid.rows : 0
                        delegate:Rectangle {
                            required property int index
                            readonly property var rect:preview.placementRectFor({x:index%studio.grid.columns,y:Math.floor(index/studio.grid.columns),width:1,height:1},studio.scopeId)
                            x:rect.x;y:rect.y;width:rect.width;height:rect.height;color:"transparent";border.width:1;border.color:studio.palette.mutedColor;opacity:0.35
                        }
                    }
                    Repeater {
                        model:studio.gridMode ? studio.scopedItems.filter(i=>!!i.placement):[]
                        delegate:Rectangle {
                            required property var modelData
                            readonly property var rect:preview.placementRectFor(modelData.placement,modelData.categoryId || studio.scopeId)
                            x:rect.x;y:rect.y;width:rect.width;height:rect.height;color:"transparent";border.width:modelData.id===studio.selectedId?4:1;border.color:studio.palette.focusColor
                            MouseArea {
                                objectName:"studioHit-"+modelData.id
                                anchors.fill:parent
                                property point start
                                onPressed:mouse=>{studio.selectedId=modelData.id;studio.toolbarFocus=false;studio.beginGesture(false);start=Qt.point(mouse.x,mouse.y)}
                                onPositionChanged:mouse=>{
                                    if(!pressed)return
                                    let p=Object.assign({},studio.origin)
                                    p.x+=Math.round((mouse.x-start.x)/(preview.gridMetricsFor(studio.scopeId).cellW+preview.gridMetricsFor(studio.scopeId).gap))
                                    p.y+=Math.round((mouse.y-start.y)/(preview.gridMetricsFor(studio.scopeId).cellH+preview.gridMetricsFor(studio.scopeId).gap))
                                    if(p.x>=0 && p.y>=0 && p.x+p.width<=studio.grid.columns && p.y+p.height<=studio.grid.rows)studio.candidate=p
                                }
                                onReleased:studio.acceptCandidate()
                                onCanceled:studio.mode="canvas"
                            }
                        }
                    }
                    Rectangle {
                        visible:studio.mode==="move" || studio.mode==="resize" || studio.mode==="collision" || studio.mode==="geometry"
                        readonly property var rect:preview.placementRectFor(studio.candidate,studio.scopeId)
                        x:rect.x;y:rect.y;width:rect.width;height:rect.height;color:studio.palette.selectedColor;opacity:0.8;border.width:4;border.color:studio.palette.focusColor
                        Text {anchors.centerIn:parent;text:studio.mode.toUpperCase()+"\n"+(studio.addingType || studio.selectedId);color:studio.palette.textColor;wrapMode:Text.Wrap;width:parent.width;horizontalAlignment:Text.AlignHCenter;font.pixelSize:20*studio.textScale}
                    }
                    Repeater {
                        model:studio.collisionPreview.valid ? studio.collisionPreview.placements : []
                        delegate:Rectangle {
                            required property var modelData
                            readonly property var rect:preview.placementRectFor(modelData.placement,modelData.categoryId || studio.scopeId)
                            x:rect.x;y:rect.y;width:rect.width;height:rect.height;color:"transparent";border.width:3;border.color:studio.palette.focusColor
                            Text {anchors.centerIn:parent;text:modelData.id+" → "+(modelData.placement.y+1)+", "+(modelData.placement.x+1);color:studio.palette.textColor;style:Text.Outline;styleColor:studio.palette.surfaceColor;font.pixelSize:16*studio.textScale}
                        }
                    }
                }
            }
            Text {anchors.centerIn:parent;visible:!studio.items.length && studio.mode!=="preview";text:"Empty page · Add a widget from the toolbar";color:studio.palette.textColor;font.pixelSize:26*studio.textScale}
        }
        Text {width:parent.width;text:studio.message+"  ·  OK pick up · Menu edit · Back cancel/exit";color:studio.palette.textColor;wrapMode:Text.Wrap;font.pixelSize:17*studio.textScale}
    }
    Rectangle {
        visible:studio.mode!=="canvas" && studio.mode!=="move" && studio.mode!=="resize" && studio.mode!=="preview"
        z:3
        anchors.centerIn:parent;width:Math.min(parent.width-2*studio.spacing,680);height:Math.min(parent.height-2*studio.spacing,dialogColumn.implicitHeight+2*studio.spacing)
        color:studio.palette.surfaceColor;border.width:3;border.color:studio.palette.focusColor;radius:studio.spacing
        Column {
            id:dialogColumn;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:studio.spacing;spacing:studio.spacing/2
            Text {text:studio.mode.toUpperCase();color:studio.palette.textColor;font.pixelSize:24*studio.textScale}
            Text {width:parent.width;text:studio.mode==="apply" ? "Save the complete interface draft: "+(session?session.changeCount:0)+" operations across Home and pages." : studio.mode==="remove" ? "Remove "+studio.selectedId+"? This stays undoable until Apply." : studio.mode==="collision" && studio.choice!==2 ? (studio.collisionPreview.valid ? "Preview: "+studio.collisionPreview.placements.length+" affected components. Confirm commits one undoable draft operation." : "Unavailable: "+studio.collisionPreview.reason) : studio.message;color:studio.palette.mutedColor;wrapMode:Text.Wrap;font.pixelSize:18*studio.textScale}
            ListView {
                width:parent.width;height:Math.min(360*studio.textScale,Math.max(50,studio.height-220*studio.textScale))
                clip:true;currentIndex:studio.choice;highlightRangeMode:ListView.ApplyRange
                model:studio.mode==="keyboard" ? []:studio.dialogLabels()
                delegate:Rectangle {
                    required property int index;required property string modelData
                    width:dialogColumn.width;height:42*studio.textScale;color:studio.choice===index?studio.palette.selectedColor:"transparent";border.width:studio.choice===index?2:0;border.color:studio.palette.focusColor
                    Text {anchors.fill:parent;anchors.margins:8;text:modelData;color:studio.palette.textColor;font.pixelSize:18*studio.textScale;elide:Text.ElideRight}
                    MouseArea {anchors.fill:parent;onClicked:{studio.choice=index;studio.activateChoice()}}
                }
            }
        }
    }
    OnScreenKeyboard {
        id:keyboard;objectName:"studioKeyboard";visible:studio.mode==="keyboard";anchors.fill:parent;z:10;appearance:studio.palette;hint:studio.message
        onCanceled:studio.mode=studio.keyboardReturnMode
        onAccepted:text=>{
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
            else if(studio.keyboardAction==="newPage")studio.command("newPage",{name:text})
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
    Connections {target:studio.session;function onChanged(){if(studio.session && studio.session.errorString.indexOf("History budget")===0){studio.mode="history";studio.choice=0}}}
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
