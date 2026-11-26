import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    objectName: "composedHome"
    required property var host
    readonly property var config: host.composition
    readonly property var tabs: config.navigation || []
    readonly property bool hasVisibleTabs: !identityHeader && tabs.length > 0
    property string activeTabId: config.initialTab || (tabs[0] ? tabs[0].id : "")
    readonly property int activeTab: Math.max(0,tabs.findIndex(t => t.id === activeTabId))
    property int focusedTab: activeTab
    onFocusedTabChanged: Qt.callLater(() => {
        const item = navigationTabs.itemAt(focusedTab); if (!item) return
        const left = navigationRow.x + item.x
        if (left < navigationStrip.contentX) navigationStrip.contentX = left
        else if (left + item.width > navigationStrip.contentX + navigationStrip.width)
            navigationStrip.contentX = Math.max(0,left + item.width - navigationStrip.width)
    })
    property bool navigationFocused: false
    property int navigationGeneration: 0
    function navigationState() {
        return {activeTabId:activeTabId, focusedTab:focusedTab, navigationFocused:navigationFocused}
    }
    function resetNavigationState(state) {
        ++navigationGeneration
        activeWidget="";activeGroup="";sectionActionId=""
        const saved=state || {}, initial=config.initialTab || (tabs[0] ? tabs[0].id : "")
        activeTabId=tabs.some(tab=>tab.id===saved.activeTabId)?saved.activeTabId:initial
        focusedTab=Number.isInteger(saved.focusedTab) && saved.focusedTab>=0 && saved.focusedTab<tabs.length?saved.focusedTab:activeTab
        navigationFocused=!!saved.navigationFocused
        content.cancelFlick();content.contentY=0
        navigationStrip.cancelFlick();navigationStrip.contentX=0
    }
    property string selectMode: host.selectMode || "single"
    property string selectTrigger: host.selectTrigger || "click"
    property string selectAction: host.selectAction || "navigate"
    property string selectionColor: host.selectionColor || ""
    property real zoom: 1.0
    readonly property real unit: Math.min(width / 1280, height / (config.designHeight || 800)) * zoom
    Keys.onPressed: {
        if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal) { zoom = Math.min(2.0, zoom + 0.1); event.accepted = true }
        else if (event.key === Qt.Key_Minus) { zoom = Math.max(0.5, zoom - 0.1); event.accepted = true }
        else if (event.key === Qt.Key_0) { zoom = 1.0; event.accepted = true }
    }
    readonly property var headerControls: host.visibleHeaderControls(config.headerControls || (config.headerMode === "identity" ? ["search", "settings"] : ["settings", "edit"]))
    readonly property bool identityHeader: config.headerMode === "identity"
    readonly property bool bottomNavigation:headerStyle.position==="bottom"
    property var focusedLibrary:null
    onWidgetsChanged:Qt.callLater(syncFeaturedShelf)
    onActiveWidgetChanged:Qt.callLater(syncFeaturedShelf)
    onSectionActionIdChanged:Qt.callLater(syncFeaturedShelf)
    Connections {
        target:root.host
        function onFocusedIndexChanged(){Qt.callLater(root.syncFeaturedShelf)}
        function onItemsChanged(){Qt.callLater(root.syncFeaturedShelf)}
    }
    // Resolve focus after shelf registration settles because the hero changes viewport geometry.
    function syncFeaturedShelf() {
        focusedLibrary=widgets[activeWidget || sectionActionId || ((host.items || [])[host.focusedIndex] || {}).id] || null
    }
    readonly property var featuredMovie:!host.editMode && focusedLibrary && focusedLibrary.featuredItem || null
    readonly property var featuredStyle:featuredMovie && focusedLibrary.style || ({})
    readonly property string featuredBackground:featuredStyle.libraryHeroBackground || "theme"
    // Blend sampled hues toward the page's own lightness so themes keep readable text.
    function tintWithArtwork(base, paint, strength) {
        const lightness=.2126*base.r+.7152*base.g+.0722*base.b
        const artworkLightness=.2126*paint.r+.7152*paint.g+.0722*paint.b
        const channel=value=>Math.max(0,Math.min(1,value*strength+lightness-artworkLightness*strength))
        return Qt.rgba(channel(paint.r),channel(paint.g),channel(paint.b),1)
    }
    readonly property var featuredBackdrop: {
        const revision=host.libraryProvider?host.libraryProvider.revision:0
        return featuredMovie && host.libraryProvider && host.libraryProvider.backdrop
            ? host.libraryProvider.backdrop(featuredMovie.mediaType,featuredMovie.mediaId) : null
    }
    readonly property var featuredPoster:featuredMovie && host.libraryProvider && (!featuredBackdrop || !featuredBackdrop.source)
        ? host.libraryProvider.artwork(featuredMovie.mediaType,featuredMovie.mediaId) : null
    readonly property var headerStyle:Object.assign({},(host.headerDefaults || {})[identityHeader?"identity":"navigation"] || {},config.header || {})
    readonly property var headerOrder:headerStyle.order || ["title","controls","clock"]
    readonly property color headerInk:headerStyle.textColor || host.appearance.textColor || "#EEEAF0"
    // Align within the space reserved for the title, leaving overflowing tabs scrollable.
    function titleOffset(width, available) {
        const space=Math.max(0,available-width)
        return headerStyle.titleAlignment==="right"?space:headerStyle.titleAlignment==="center"?space/2:0
    }
    readonly property string headerLogo:headerStyle.customLogo && host.artworkResolver?host.artworkResolver.resolveArtwork(headerStyle.customLogo):""
    function headerPosition(part) {
        let position=(headerStyle.padding || 0)*root.unit
        for(const id of headerOrder){if(id===part)break;const size=id==="title"?navigationStrip.width:id==="controls"?controls.width:clock.visible?clock.width:0;if(size>0)position+=size+(headerStyle.gap===undefined?22:headerStyle.gap)*root.unit}
        return position
    }
    property date now: new Date()
    property string sectionActionId: ""
    property string activeWidget: ""
    property string activeGroup: ""
    property var widgets: ({})
    function registerWidget(id,widget,remove){let entries=Object.assign({},widgets);if(remove){if(entries[id]===widget)delete entries[id]}else entries[id]=widget;widgets=entries}
    readonly property var grid: host.grid || ({})
    readonly property var placements: host.placements || []
    readonly property bool sectionMode: !!root.config.grid && root.grid.mode==="sections"
    property var sectionSurfaces: ({})
    readonly property string viewportGeometry: {
        let geometry="",ancestor=content
        while(ancestor && ancestor!==host){
            geometry += [ancestor.x,ancestor.y,ancestor.width,ancestor.height,ancestor.scale].join(":")+";"
            ancestor=ancestor.parent
        }
        return geometry
    }
    readonly property real scrollOffset:content.contentY
    function viewportRect(){
        const point=content.mapToItem(host,0,0)
        return {x:point.x,y:point.y,width:content.width,height:content.height}
    }
    readonly property bool scrollingPage:placementMode && !sectionMode && config.verticalScrolling===true
    readonly property int contentRows: {
        let rows=root.grid.rows || 5
        if(!scrollingPage)return rows
        for(const entry of placedItems)rows=Math.max(rows,entry.placement.y+entry.placement.height)
        return Math.max(rows,host.previewExtentRows || 0)
    }
    readonly property int firstVisibleGridRow:scrollingPage?Math.max(0,Math.floor(content.contentY/(placementCell.cellH+placementCell.gap))):0
    readonly property real pageContentHeight:scrollingPage?Math.max(content.height,2*placementCell.margin+contentRows*(placementCell.cellH+placementCell.gap)-placementCell.gap):content.height
    readonly property int visibleGridRows:Math.max(1,Math.ceil(content.height/(placementCell.cellH+placementCell.gap)))
    // Buffer grid rows, not whole screens or every subpixel scroll update.
    // Hidden page previews retain their data/focus without constructing cards.
    readonly property var viewportItems:!root.visible?[]:scrollingPage?placedItems.filter(entry=>{
        return entry.placement.y+entry.placement.height>=firstVisibleGridRow-2
            && entry.placement.y<=firstVisibleGridRow+visibleGridRows+2
    }):placedItems
    Connections {
        target:host
        function onFocusedIndexChanged(){root.settleRuntimeFocus()}
    }
    function revealFocusedPlacement(){
        let item=(host.items || [])[host.focusedIndex]
        for(let depth=0;item && item.parentId && depth<4;++depth)item=(host.items || []).find(i=>i.id===item.parentId)
        if(item && item.placement)revealPlacement(item.placement,item.categoryId || "")
    }
    function settleRuntimeFocus(){const generation=navigationGeneration;if(scrollingPage && !host.editMode)Qt.callLater(()=>{if(root && generation===root.navigationGeneration)revealFocusedPlacement()})}
    function editorViewport(){return {y:content.contentY,navigationX:navigationStrip.contentX}}
    function restoreEditorViewport(state){if(!state)return;content.cancelFlick();content.contentY=state.y;navigationStrip.cancelFlick();navigationStrip.contentX=state.navigationX}
    readonly property bool placementMode: {
        if(!root.config.grid)return false
        if(root.sectionMode)return true
        const authored=host.authoredItems || host.items || []
        if(!authored.length)return root.config.grid.mode==="screen"
        for(const item of authored)if(!item.placement)return false
        if(root.config.grid.mode===undefined)for(const section of host.authoredSections || host.sections || []) {
            const style=section.presentation || {}
            if(style.variant==="library" || style.trailingAction)return false
        }
        return true
    }
    function measureGrid(w,h,g) {
        const columns = g.columns || 8
        const rows = g.rows || 5
        const gap = (g.gap === undefined ? 20 : g.gap) * root.unit
        const margin = (g.margin === undefined ? 40 : g.margin) * root.unit
        const valid=w>2*margin+(columns-1)*gap && h>2*margin+(rows-1)*gap
        const cellW = valid ? (w - 2*margin - (columns-1)*gap) / columns : 0
        const cellH = valid ? (h - 2*margin - (rows-1)*gap) / rows : 0
        return { columns: columns, rows: rows, gap: gap, margin: margin, cellW: cellW, cellH: cellH,valid:valid }
    }
    readonly property var placementCell:measureGrid(content.width,content.height,root.grid || {})
    readonly property bool editorGeometryReady: {
        if(content.width<=0 || content.height<=0)return false
        if(!sectionMode)return true
        for(const section of visibleSections){
            if((section.presentation || {}).variant==="library")continue
            const surface=sectionSurfaces[section.id || ""]
            if(!surface || !surface.geometryReady)return false
        }
        return true
    }
    readonly property string layoutError: {
        for(const key of Object.keys(sectionSurfaces)) {
            const surface=sectionSurfaces[key]
            if(surface && surface.responsiveError)return surface.responsiveError
        }
        if(!placementMode)return ""
        if(!sectionMode)return placementCell.valid?"":"Grid spacing or margins leave no room for cells. Edit Grid to repair."
        for(const key of Object.keys(sectionSurfaces)) {
            const surface=sectionSurfaces[key]
            if(surface && surface.editableGrid && !surface.metrics.valid)return "Grid spacing or margins leave no room in "+surface.sectionName+". Edit Grid to repair."
        }
        return ""
    }
    function gridMetricsFor(scope) { const surface=sectionMode?sectionSurfaces[scope || ""]:null;return surface?surface.metrics:placementCell }
    function placementRect(p,scope) {
        const surface=sectionMode?sectionSurfaces[scope || ""]:placementGrid
        const c = gridMetricsFor(scope)
        // Keep mapped bounds reactive to ancestor geometry and Flickable scrolling.
        let ancestor=surface || placementGrid
        while(ancestor && ancestor!==content.contentItem){const geometry=ancestor.x+ancestor.y+ancestor.width+ancestor.height;ancestor=ancestor.parent}
        const scroll=content.contentY
        const origin = (surface || placementGrid).mapToItem(content.contentItem, c.margin, c.margin)
        const x=origin.x+p.x*(c.cellW+c.gap),y=origin.y+p.y*(c.cellH+c.gap)
        const width=p.width*c.cellW+(p.width-1)*c.gap,height=p.height*c.cellH+(p.height-1)*c.gap
        const topLeft=content.contentItem.mapToItem(host,x,y),bottomRight=content.contentItem.mapToItem(host,x+width,y+height)
        return {x:topLeft.x,y:topLeft.y,width:bottomRight.x-topLeft.x,height:bottomRight.y-topLeft.y}
    }
    function revealScope(scope, includeHeading) {
        const surface=sectionSurfaces[scope || ""];if(!sectionMode || !surface)return
        const point=(includeHeading?surface.sectionFrame:surface).mapToItem(content.contentItem,0,0)
        content.contentY=Math.max(0,Math.min(content.contentHeight-content.height,point.y))
    }
    function revealPlacement(p,scope) {
        if(scrollingPage){
            const c=placementCell,origin=placementGrid.mapToItem(content.contentItem,c.margin,c.margin)
            const top=origin.y+p.y*(c.cellH+c.gap),bottom=top+p.height*c.cellH+(p.height-1)*c.gap
            if(top<content.contentY)content.contentY=Math.max(0,top-c.gap)
            else if(bottom>content.contentY+content.height)content.contentY=Math.min(Math.max(0,content.contentHeight-content.height),bottom-content.height+c.gap)
            return
        }
        const surface=sectionSurfaces[scope || ""];if(!sectionMode || !surface)return
        const c=surface.metrics,point=surface.mapToItem(content.contentItem,c.margin+p.x*(c.cellW+c.gap),c.margin+p.y*(c.cellH+c.gap))
        const height=p.height*c.cellH+(p.height-1)*c.gap
        if(point.y<content.contentY)content.contentY=Math.max(0,point.y-c.gap)
        else if(point.y+height>content.contentY+content.height)content.contentY=Math.min(Math.max(0,content.contentHeight-content.height),point.y+height-content.height+c.gap)
    }
    function activatePlaced(item) {
        if(host.editMode){host.editSelected(item.id);return}
        activeGroup=item.parentId || ""
        sectionActionId="";host.focusedIndex=host.itemIndex(item.id);navigationFocused=false;host.recoveryFocused=false
        handleCommand(inputManager.confirm)
    }
    function revealCard(card,editing) {
        if (!card || host.editMode && !editing || root.placementMode && !root.sectionMode && !scrollingPage) return
        const p = card.mapToItem(content.contentItem,0,0)
        if(p.y<content.contentY)content.contentY=Math.max(0,p.y-6)
        else if(p.y+card.height>content.contentY+content.height)content.contentY=Math.max(0,p.y+card.height-content.height+6)
    }
    readonly property var placedItems: {
        const map = {}
        const items = host.authoredItems || host.items || []
        const placements = root.placements || []
        for (const p of placements) map[p.id] = p.placement
        const result = []
        const inheritedById={}
        for(const category of host.authoredSections || host.sections || [])inheritedById[category.id || ""]=category.presentation || {}
        const tab=tabs[activeTab]
        for (const item of items) {
            if(item.parentId)continue
            if(tab && tab.sections && tab.sections.indexOf(item.categoryId || "")<0)continue
            const placement = map[item.id]
            const inherited=inheritedById[item.categoryId || ""] || {}
            if (placement) result.push({ item:Object.assign({},item,{presentation:Object.assign({},inherited,item.presentation || {})}), placement: placement })
        }
        return result
    }
    onVisibleSectionsChanged: { activeWidget = ""; sectionActionId = "" }
    Timer { interval: 1000; repeat: true; running: root.visible && root.config.showClock === true; onTriggered: root.now = new Date() }
    readonly property real gap: (config.gap === undefined ? 20 : config.gap) * unit
    readonly property real margin: (config.margin === undefined ? 40 : config.margin) * unit
    readonly property int columns: width < 850 ? 1 : config.columns || 1
    readonly property var visibleSections: {
        if (!config.enabled) return []
        const tab = tabs[activeTab]
        const rawSections=sectionMode?(host.authoredSections || host.sections):host.sections
        const sections=rawSections.map(s=>Object.assign({},s,{presentation:host.artworkResolver && host.artworkResolver.responsivePresentation?host.artworkResolver.responsivePresentation(s.presentation || {},content.width,content.height,root.unit):s.presentation || {}}))
        if(sectionMode && host.editMode && host.scopeView)return sections.filter(s=>(s.id || "")===host.editScopeId)
        return !tab || !tab.sections ? sections : tab.sections.map(id => sections.find(s => s.id === id)).filter(s => !!s)
    }
    // A single library can size and label its section from its own inspector.
    function sectionLibrary(section) {
        const items=section.items || []
        return items.length===1 && items[0].componentType==="library" ? items[0].presentation || {} : (section.presentation || {}).variant==="library" ? section.presentation : {}
    }
    function sectionHeight(style, section) { return (sectionLibrary(section).libraryShelfHeight || (style.height===undefined ? (sectionMode?config.sectionHeight:180):style.height))*unit }
    readonly property var geometry: {
        let rows = [], entries = [], column = 0, row = 0
        for (const section of visibleSections) {
            const style = section.presentation || {}
            const span = Math.min(columns, style.span || columns)
            if (column + span > columns) { row++; column = 0 }
            const height = sectionHeight(style, section)
            rows[row] = Math.max(rows[row] || 0, height)
            entries.push({row: row, column: column, span: span})
            column += span
            if (column === columns) { row++; column = 0 }
        }
        const total = rows.reduce((a,b) => a+b, 0)
        const available = Math.max(100, content.height - Math.max(0, rows.length - 1) * gap)
        return { entries: entries, rows: rows, factor: config.fit === false ? 1 : Math.min(1, available / Math.max(1,total)) }
    }
    function focusEntries() {
        if(activeGroup)return (host.items || []).filter(i=>(i.parentId || "")===activeGroup && !!i.placement && host.focusable(i)).map(i=>({id:i.id,row:i.placement.y,x:i.placement.x+i.placement.width/2,placement:i.placement}))
        if (placementMode && !sectionMode) {
            const tab=tabs[activeTab]
            return placedItems.filter(e=>host.focusable(e.item) && (!tab || !tab.sections || tab.sections.indexOf(e.item.categoryId || "")>=0)).map(e=>({id:e.item.id,row:e.placement.y,x:e.placement.x+e.placement.width/2,placement:e.placement}))
        }
        let entries = []
        for (let s = 0; s < visibleSections.length; s++) {
            const section = visibleSections[s], g = geometry.entries[s] || {row:s,column:0,span:columns}, style = section.presentation || {}
            const cards = style.columns || (style.variant === "banner" || style.variant === "summary" ? 1 : 4)
            const values = section.items || []
            if (style.variant === "library") entries.push({id:"widget-"+section.id,widget:true,row:g.row*100,x:g.column+g.span/2})
            if(sectionMode && style.variant!=="library") {
                for(const item of values)if(item.placement && host.focusable(item))entries.push({id:item.id,row:g.row*100+item.placement.y,x:item.placement.x+item.placement.width/2})
                // The heading action is a separate row above the collection.
                // Horizontal input inside a library belongs to its cards.
                if(style.trailingAction)entries.push({id:"section-"+section.id,sectionAction:true,row:g.row*100-1,x:0})
                continue
            }
            const sectionWidth = (content.width - gap*(columns-1))*g.span/columns + (g.span-1)*gap
            const innerWidth = Math.min(sectionWidth,(style.contentWidth || 3840)*unit) - 2*(style.padding || 0)*unit
            const left = g.column*(content.width+gap)/columns + (sectionWidth-innerWidth)/2
            for (let i = 0; i < values.length; i++) if(host.focusable(values[i]))entries.push({ id: values[i].id,
                row: g.row * 100 + Math.floor(i/cards), x: (left + (i % cards + 0.5)*innerWidth/cards)/content.width*columns })
            if (style.trailingAction) entries.push({id:"section-"+section.id, sectionAction:true, row:g.row*100+Math.floor(Math.max(0,values.length-1)/cards),x:g.column+g.span+0.5})
        }
        return entries
    }
    function activateTab(index) {
        const tab = tabs[index]; if (!tab) return
        focusedTab = index
        if (tab.actions && tab.actions.length) host.actionActivated({actions: tab.actions}, "tab-" + tab.id)
        else if (tab.action) host.actionActivated(tab.action, "tab-" + tab.id)
        else { activeTabId = tab.id; const first = focusEntries()[0]; if (first) host.focusedIndex = host.itemIndex(first.id) }
    }
    function restoreFocus(id) {
        const generation=navigationGeneration
        const entries=focusEntries()
        const original=entries.find(e=>e.id===id)
        const widget=root.widgets[id]
        const saved=(root.host.appCollectionFocus || {})[id]
        if(widget && saved && widget.restoreFocusState){root.activeWidget=id;widget.restoreFocusState(saved)}
        else if(!widget && entries.length){
            // The focused item disappeared after a navigation generation. Fall back to
            // the nearest surviving row on the same horizontal band, then the parent
            // control, then the first focusable entry (safe header).
            let target=entries.find(e=>e.row===original?.row && e.id!==id)
            if(!target){const sameRow=entries.filter(e=>e.row===original?.row);target=sameRow.length?sameRow[0]:null}
            if(!target){const idx=entries.indexOf(original||{id:""});target=idx>=0?entries[Math.min(idx,entries.length-1)]:null}
            if(!target)target=entries[0]
            if(target && target.id!==id){focusEntry(target);root.activeWidget=target.id}
        }
        if(scrollingPage)Qt.callLater(()=>{if(root && generation===root.navigationGeneration)revealFocusedPlacement()})
        const component=(host.items || []).find(i=>i.id===id);activeGroup=component?component.parentId || "":""
        sectionActionId = id.startsWith("section-") ? id : ""
        if(sectionActionId)activeWidget=""
        navigationFocused = id.startsWith("tab-")
        if (navigationFocused) { const i = tabs.findIndex(t => "tab-" + t.id === id); if (i >= 0) focusedTab = i }
        else if (!focusEntries().some(e => e.id === id)) {
            const tab = tabs.findIndex(t => !t.action && (!t.sections || host.sections.some(s => t.sections.indexOf(s.id) >= 0 && (s.items || []).some(i => i.id === id))))
            if (tab >= 0) activeTabId = tabs[tab].id
        }
    }
    function activateSection(section) {
        activeWidget="";sectionActionId="section-"+section.id
        navigationFocused=false;host.recoveryFocused=false
        host.actionActivated(section.presentation.trailingAction,sectionActionId)
    }
    function activateControl(index) {
        const control = headerControls[index]
        if(host.editMode){host.editHeaderSelected(control);return}
        if (control === "search") host.searchRequested()
        else if (control === "sources") host.activateHeaderControl(control)
        else host.settingsRequested(control === "edit" ? 1 : -1)
    }
    function focusEntry(entry) {
        if(!entry)return
        sectionActionId=entry.sectionAction || entry.widget?entry.id:""
        if(!entry.sectionAction && !entry.widget)host.focusedIndex=host.itemIndex(entry.id)
        const item=(host.items || []).find(i=>i.id===entry.id)
        const collection=entry.widget || item && (item.componentType==="library" || item.appCollection || item.family==="page-menu" || item.family==="carousel")
        if(collection && widgets[entry.id])activeWidget=entry.id
        if (sectionMode && item && item.placement && (featuredMovie || item.presentation && item.presentation.libraryHero))
            Qt.callLater(()=>revealScope(item.categoryId || "",true))
    }
    // Compare the full grid footprints so a control can neighbor any row of a tall component.
    function directionalPlacement(entries, current, horizontal, forward) {
        const axis = horizontal ? "x" : "y"
        const length = horizontal ? "width" : "height"
        const cross = horizontal ? "y" : "x"
        const breadth = horizontal ? "height" : "width"
        const origin = current.placement
        const candidates = []
        for (const entry of entries) {
            const target = entry.placement
            const distance = forward ? target[axis] - (origin[axis] + origin[length])
                                     : origin[axis] - (target[axis] + target[length])
            if (distance < 0) continue
            const overlap = Math.min(origin[cross] + origin[breadth], target[cross] + target[breadth])
                          - Math.max(origin[cross], target[cross])
            const offset = Math.abs(origin[cross] + origin[breadth]/2 - target[cross] - target[breadth]/2)
            candidates.push({entry:entry, offAxis:overlap <= 0, gap:Math.max(0,-overlap), distance:distance, offset:offset})
        }
        candidates.sort((a,b) => a.offAxis-b.offAxis || a.gap-b.gap || a.distance-b.distance || a.offset-b.offset)
        return candidates.length ? candidates[0].entry : null
    }
    // Let the focused widget handle its own entries before moving across components.
    function handleCommand(command) {
        if (activeWidget) {
            const widget = widgets[activeWidget]
            if (widget && widget.handleCommand(command)) return
            activeWidget = ""
            if (command === inputManager.back) return
        }
        if(activeGroup && command===inputManager.back){const group=(host.items || []).find(i=>i.id===activeGroup);if(group){host.focusedIndex=host.itemIndex(group.id);activeGroup=group.parentId || ""}else activeGroup="";return}
        if (host.recoveryFocused) {
            if (command === (bottomNavigation?inputManager.navigateUp:inputManager.navigateDown)) { host.recoveryFocused = false; navigationFocused = hasVisibleTabs }
            else if (command === inputManager.navigateLeft) host.recoveryIndex = Math.max(0,host.recoveryIndex-1)
            else if (command === inputManager.navigateRight) host.recoveryIndex = Math.min(headerControls.length-1,host.recoveryIndex+1)
            else if (command === inputManager.confirm) activateControl(host.recoveryIndex)
            return
        }
        if (navigationFocused) {
            if (command === inputManager.navigateLeft) focusedTab = Math.max(0, focusedTab - 1)
            else if (command === inputManager.navigateRight) {
                if (focusedTab === tabs.length - 1 && headerControls.length > 0) {
                    navigationFocused = false
                    host.recoveryFocused = true
                    host.recoveryIndex = 0
                    return
                }
                focusedTab = Math.min(tabs.length - 1, focusedTab + 1)
            }
            else if (command === inputManager.confirm) activateTab(focusedTab)
            else if (command === (bottomNavigation?inputManager.navigateDown:inputManager.navigateUp)) host.focusRecoveryHeader()
            else if (command === (bottomNavigation?inputManager.navigateUp:inputManager.navigateDown)) {
                const entries=focusEntries(), current=entries.find(e=>host.itemIndex(e.id)===host.focusedIndex)
                const target=bottomNavigation && current?current:entries[0]
                if(target){focusEntry(target);navigationFocused=false}
            }
            return
        }
        const entries = focusEntries(), current = entries.find(e => sectionActionId ? e.id === sectionActionId : !e.sectionAction && host.itemIndex(e.id) === host.focusedIndex) || entries[0]
        if (!current) { host.focusRecoveryHeader(); return }
        const focusedItem=(host.items || []).find(i=>i.id===current.id)
        const pageMenu=focusedItem && focusedItem.family==="page-menu"
        if((current.widget || focusedItem && (focusedItem.componentType==="library" || focusedItem.appCollection || ["page-menu","carousel","detected-sources","source-notice"].indexOf(focusedItem.family)>=0)) && widgets[current.id]
            && (command===inputManager.navigateLeft || command===inputManager.navigateRight || command===inputManager.confirm || focusedItem && ["detected-sources","source-notice"].indexOf(focusedItem.family)>=0 && command===inputManager.menu || pageMenu && (command===inputManager.navigateUp || command===inputManager.navigateDown))) {
            activeWidget=current.id
            const handled=widgets[current.id].handleCommand(command)
            if(!pageMenu || handled)return
            activeWidget=""
        }
        if (command === inputManager.confirm) {
            if (current.widget) { activeWidget=current.id; sectionActionId=current.id; return }
            if (current.sectionAction) { const section = visibleSections.find(s => "section-"+s.id === current.id); if (section) root.activateSection(section); return }
            const item = host.items[host.itemIndex(current.id)]
            if(item && (item.componentType==="library" || item.appCollection || ["page-menu","carousel","page-panel"].includes(item.family))){const widget=widgets[item.id];if(widget && (!widget.canEngage || widget.canEngage())){activeWidget=item.id;widget.engage()}return}
            if(item && item.componentType==="container"){
                const children=(host.items || []).filter(i=>(i.parentId || "")===item.id && host.focusable(i))
                if(children.length){activeGroup=item.id;host.focusedIndex=host.itemIndex(children[0].id)}
                return
            }
            if (host.unavailableReason(item)) host.settingsRequested(1)
            else if (item.actions && item.actions.length>0) host.actionActivated({actions:item.actions}, item.id)
            return
        }
        const horizontal = command === inputManager.navigateLeft || command === inputManager.navigateRight
        const vertical = command === inputManager.navigateUp || command === inputManager.navigateDown
        if (current.placement && (horizontal || vertical)) {
            const forward = command === inputManager.navigateRight || command === inputManager.navigateDown
            const target = directionalPlacement(entries, current, horizontal, forward)
            if (target) { focusEntry(target); return }
            if (vertical && !forward) {
                navigationFocused = tabs.length > 0
                if (!navigationFocused) host.focusRecoveryHeader()
            }
            if (!horizontal || !activeGroup) return
        }
        let candidates = [], next
        if (horizontal && !current.placement) {
            const direction = command === inputManager.navigateLeft ? -1 : 1
            candidates = entries.filter(e => e.row === current.row && (e.x - current.x)*direction > 0).sort((a,b) => Math.abs(a.x-current.x)-Math.abs(b.x-current.x))
        } else if (command === inputManager.navigateUp || command === inputManager.navigateDown) {
            const direction = command === inputManager.navigateUp ? -1 : 1
            const rows = entries.filter(e => (e.row-current.row)*direction > 0).map(e => e.row)
            if (!rows.length && (direction < 0 || bottomNavigation)) {
                if(bottomNavigation && direction<0)host.focusRecoveryHeader()
                else {navigationFocused=hasVisibleTabs;if(!hasVisibleTabs)host.focusRecoveryHeader()}
                return
            }
            const row = direction < 0 ? Math.max(...rows) : Math.min(...rows)
            candidates = entries.filter(e => e.row === row).sort((a,b) => Math.abs(a.x-current.x)-Math.abs(b.x-current.x))
        }
        if(!candidates.length && activeGroup && (command===inputManager.navigateLeft || command===inputManager.navigateRight)){const group=host.items.find(i=>i.id===activeGroup);if(group && (group.presentation.layout || {}).wrap)candidates=entries.slice().sort((a,b)=>command===inputManager.navigateRight?a.x-b.x:b.x-a.x)}
        next = candidates[0]; if(next)focusEntry(next)
    }
    Rectangle {
        objectName:"focusedLibraryBackground"
        anchors.fill:parent
        visible:!!root.featuredMovie && (root.featuredBackground==="fixed" && !!root.featuredStyle.libraryHeroColor
            || root.featuredBackground==="artwork" && !!heroArt.backdropColor)
        gradient:Gradient {
            GradientStop {position:0;color:root.featuredBackground==="fixed"?root.featuredStyle.libraryHeroColor || host.backgroundStart:
                heroArt.backdropColor ? root.tintWithArtwork(host.backgroundStart,heroArt.backdropColor,.32) : host.backgroundStart}
            GradientStop {position:1;color:root.featuredBackground==="fixed"?root.featuredStyle.libraryHeroColor || host.backgroundEnd:
                heroArt.backdropColor ? root.tintWithArtwork(host.backgroundEnd,heroArt.backdropColor,.18) : host.backgroundEnd}
        }
    }
    // Keep artwork behind the whole page so increasing it never pushes the shelves away.
    ArtworkImage {
        id:heroArt;objectName:"focusedLibraryArtwork"
        readonly property real widthPercent:root.featuredStyle.libraryHeroWidth || 68
        readonly property real heightPercent:root.featuredStyle.libraryHeroHeight || 37
        readonly property real widthExpansion:Math.max(0,(widthPercent-90)/10)
        readonly property real heightExpansion:Math.max(0,(heightPercent-50)/50)
        x:root.width-root.margin*(1-widthExpansion)-width
        y:(root.margin+featured.y)*(1-heightExpansion)
        width:featured.width*widthPercent/100+(root.width-featured.width)*widthExpansion
        height:heightPercent<=50?featured.height:featured.parent.height*.5*(1-heightExpansion)+root.height*heightExpansion
        source:root.featuredBackdrop && root.featuredBackdrop.source || root.featuredPoster && root.featuredPoster.source || ""
        fit:"cover";featherBackdrop:true;visible:featured.visible
        sampleBackdropColor:root.featuredBackground==="artwork"
    }
    Item {
        anchors.fill: parent; anchors.margins: root.margin
        Item {
            id:featured;objectName:"focusedLibraryDetails"
            // Ease the details back to the normal banner size as artwork expands over the page.
            readonly property real heightPercent:Math.min(50,heroArt.heightPercent)-13*heroArt.heightExpansion
            visible:!!root.featuredMovie;width:parent.width;height:visible?parent.height*heightPercent/100:0
            y:root.bottomNavigation?0:header.height+root.gap;clip:true
            Column {
                anchors.left:parent.left;anchors.leftMargin:12*root.unit;anchors.verticalCenter:parent.verticalCenter
                width:parent.width*.52;spacing:12*root.unit
                Text {objectName:"focusedLibraryTitle";width:parent.width;text:root.featuredMovie?root.featuredMovie.title || root.featuredMovie.label || "":"";textFormat:Text.PlainText;elide:Text.ElideRight;color:host.appearance.textColor || "white";font.pixelSize:40*root.unit;font.weight:Font.DemiBold}
                Text {objectName:"focusedLibrarySynopsis";width:parent.width;text:root.featuredMovie?root.featuredMovie.plot || "":"";textFormat:Text.PlainText;wrapMode:Text.WordWrap;maximumLineCount:4;elide:Text.ElideRight;color:host.appearance.textColor || "white";font.pixelSize:19*root.unit}
                Text {
                    width:parent.width;textFormat:Text.PlainText;elide:Text.ElideRight;color:host.appearance.mutedColor || "white";font.pixelSize:16*root.unit
                    text:{const movie=root.featuredMovie;return movie?[movie.year || "",movie.runtime>0?qsTr("%1 min").arg(Math.round(movie.runtime/60)):"",movie.rating>0?Number(movie.rating).toFixed(1):""].filter(Boolean).join("  ·  "):""}
                }
            }
        }
        Item {
            id: header;objectName:"pageHeader"; width: parent.width
            height: Math.max((root.headerStyle.height || (root.identityHeader ? 58 : 42))*root.unit,
                clock.visible?clock.height+2*(root.headerStyle.padding || 0)*root.unit:0)
            y:root.headerStyle.position==="bottom"?parent.height-height:0
            readonly property real innerHeight:height-2*(root.headerStyle.padding || 0)*root.unit
            Rectangle {anchors.fill:parent;radius:(root.headerStyle.radius || 0)*root.unit;color:root.headerStyle.backgroundColor || host.appearance.surfaceColor || "transparent";opacity:root.headerStyle.opacity || 0}
            Rectangle {anchors.fill:parent;color:"transparent";radius:(root.headerStyle.radius || 0)*root.unit;border.width:host.editMode && !!host.editHeaderSelection?(root.headerStyle.focusWidth || 2)*root.unit:0;border.color:root.headerStyle.focusColor || host.appearance.focusColor || root.headerInk}
            Flickable {
                id: navigationStrip; clip: true; contentWidth: root.identityHeader ? width : Math.max(width,navigationRow.implicitWidth)
                objectName:"pageHeader-title";x:root.headerPosition("title");y:(root.headerStyle.padding || 0)*root.unit
                width: Math.max(0,parent.width - controls.width - (clock.visible ? clock.width : 0) - (root.headerStyle.gap===undefined?22:root.headerStyle.gap)*root.unit*(clock.visible ? 2 : 1)-2*(root.headerStyle.padding || 0)*root.unit); height: header.innerHeight
                Row {
                    visible: root.identityHeader && root.headerStyle.showTitle!==false; height: parent.height; spacing: 10*root.unit
                    objectName:"pageHeader-identity"
                    width:implicitWidth;x:root.titleOffset(width,navigationStrip.width)
                    LineIcon { visible:!root.headerLogo;name:"sparkle"; width:38*root.unit; height:width; anchors.verticalCenter:parent.verticalCenter }
                    ArtworkImage {visible:!!root.headerLogo;source:root.headerLogo;width:(root.headerStyle.titleSize || 48)*root.unit*2;height:parent.height;fit:"contain"}
                    Text { visible:!root.headerLogo;anchors.verticalCenter:parent.verticalCenter;width:Math.min(implicitWidth,Math.max(0,navigationStrip.width-48*root.unit));elide:Text.ElideRight;text:root.config.identity===undefined?"Isolde":root.config.identity; color:root.headerStyle.textColor || host.appearance.headingColor || root.headerInk; font.family:host.appearance.fontFamily || "DejaVu Sans"; font.pixelSize:(root.headerStyle.titleSize || 48)*root.unit; font.weight:Font.Light }
                }
                Row {
                    id: navigationRow;objectName:"pageHeader-navigation";visible:!root.identityHeader; height:parent.height; spacing:26*root.unit
                    width:implicitWidth;x:root.titleOffset(width,navigationStrip.width)
                    Repeater {
                        id:navigationTabs; model:root.tabs
                        delegate:Item {
                            required property int index; required property var modelData
                            width:tabLabel.implicitWidth; height:header.height
                            Text { id:tabLabel; anchors.verticalCenter:parent.verticalCenter; text:modelData.name
                                color:index === root.activeTab || (root.navigationFocused && index === root.focusedTab) ? host.appearance.textColor || "#EEEAF0" : host.appearance.mutedColor || "#AAA4B2"
                                font.family:host.appearance.fontFamily || "DejaVu Sans"; font.pixelSize:21*root.unit*host.textScale; font.weight:index === root.activeTab ? Font.DemiBold : Font.Normal }
                            Rectangle { anchors.bottom:parent.bottom; width:parent.width; height:2*root.unit; color:host.appearance.focusColor || "#BD8198"; visible:root.navigationFocused && index === root.focusedTab }
                            MouseArea { anchors.fill:parent; onClicked:{if(host.editMode){host.editHeaderSelected("title");return}root.sectionActionId="";root.navigationFocused=true;root.activateTab(index)} }
                        }
                    }
                }
                // Flickable children belong to its content item. Empty navigation
                // has zero content width, but its header remains editable.
                MouseArea {width:Math.max(navigationStrip.width,navigationStrip.contentWidth);height:navigationStrip.height;enabled:host.editMode;onClicked:host.editHeaderSelected("title")}
            }
            Item {
                id:controls;objectName:"pageHeader-controls";x:root.headerPosition("controls");y:(root.headerStyle.padding || 0)*root.unit; width:controlRow.implicitWidth; height:header.innerHeight
                FrostedPanel { anchors.fill:parent; anchors.topMargin:4*root.unit; anchors.bottomMargin:4*root.unit; visible:root.identityHeader; wallpaperItem:host.backdropItem; radius:(root.headerStyle.radius===undefined?26:root.headerStyle.radius)*root.unit; tintOpacity:0.5 }
                Row {
                    id:controlRow; height:parent.height; spacing:root.identityHeader ? 0 : 10*root.unit
                    Repeater {
                        model:root.headerControls
                        delegate:Rectangle {
                            required property int index; required property string modelData
                            objectName:"pageHeader-"+modelData
                            width:(root.headerStyle.controlWidth || (root.identityHeader ? 120 : 100))*root.unit; height:parent.height; radius:(root.headerStyle.radius===undefined?(root.identityHeader ? 26:12):root.headerStyle.radius)*root.unit
                            color:root.identityHeader ? "transparent" : host.appearance.surfaceColor || "#28262F"
                            border.width:(!host.editMode && host.recoveryFocused && host.recoveryIndex === index || host.editMode && host.editHeaderSelection===modelData)?(root.headerStyle.focusWidth || 2)*root.unit:0; border.color:root.headerStyle.focusColor || host.appearance.focusColor || "#BD8198"
                            LineIcon { visible:(root.headerStyle.controlDisplay || (root.identityHeader?"both":"label"))!=="label"; x:root.headerStyle.controlDisplay==="icon"?(parent.width-width)/2:16*root.unit; anchors.verticalCenter:parent.verticalCenter; width:(root.headerStyle.iconSize || 23)*root.unit; height:width; name:modelData === "search" ? "search" : "settings"; ink:root.headerInk }
                            SettingsUpdateBadge { anchors.top: parent.top; anchors.right: parent.right; z: 5; settingsTarget: modelData === "settings"; unit: root.unit }
                            Text { visible:root.headerStyle.controlDisplay!=="icon";anchors.verticalCenter:parent.verticalCenter; x:(root.headerStyle.controlDisplay || (root.identityHeader?"both":"label"))==="both"?(24+(root.headerStyle.iconSize || 23))*root.unit:(parent.width-width)/2
                                width:Math.min(implicitWidth,Math.max(0,parent.width-((root.headerStyle.controlDisplay || (root.identityHeader?"both":"label"))==="both"?32+(root.headerStyle.iconSize || 23):16)*root.unit));elide:Text.ElideRight
                                text:host.headerControlLabel(modelData); color:root.headerInk; font.family:host.appearance.fontFamily || "DejaVu Sans"; font.pixelSize:(root.headerStyle.controlSize || (root.identityHeader ? 14 : 15))*root.unit }
                            MouseArea { anchors.fill:parent; onClicked:root.activateControl(index) }
                        }
                    }
                }
            }
            Item {
                id:clock;objectName:"pageHeader-clock";visible:root.config.showClock === true
                width:Math.max((root.headerStyle.clockWidth || 100)*root.unit,timeLabel.implicitWidth,
                    timeMeasure.width,dateLabel.visible?dateLabel.implicitWidth:0)
                height:clockBody.implicitHeight;x:root.headerPosition("clock");anchors.verticalCenter:parent.verticalCenter
                Column {id:clockBody;width:parent.width;spacing:3*root.unit
                Text {id:timeLabel;objectName:"pageHeader-timeLabel";width:parent.width;elide:Text.ElideRight;text:Qt.formatTime(root.now,root.headerStyle.timeFormat || "h:mm AP"); color:root.headerStyle.clockColor || root.headerInk; font.family:host.appearance.fontFamily || "DejaVu Sans"; font.pixelSize:(root.headerStyle.clockSize || 18)*root.unit }
                Text {id:dateLabel;width:parent.width;elide:Text.ElideRight;visible:root.headerStyle.showDate!==false;text:Qt.formatDate(root.now,root.headerStyle.dateFormat || "ddd, MMM d"); color:root.headerStyle.textColor || host.appearance.mutedColor || "#625B73"; font.family:host.appearance.fontFamily || "DejaVu Sans"; font.pixelSize:(root.headerStyle.dateSize || 12)*root.unit }
                }
                TextMetrics {id:timeMeasure;font:timeLabel.font;text:Qt.formatTime(new Date(2000,0,1,23,58,58),root.headerStyle.timeFormat || "h:mm AP")}
                MouseArea {anchors.fill:parent;enabled:host.editMode;onClicked:host.editHeaderSelected("clock")}
            }
        }
        Flickable {
            id:content; objectName:"composedContent"; width:parent.width; height:Math.max(0,parent.height-header.height-root.gap-featured.height); contentHeight:root.placementMode && !root.sectionMode ? root.pageContentHeight : grid.implicitHeight; clip:true
            y:(root.bottomNavigation?0:header.height+root.gap)+featured.height
            boundsBehavior:Flickable.StopAtBounds
            onYChanged:root.settleRuntimeFocus()
            onHeightChanged:root.settleRuntimeFocus()
            onContentHeightChanged:root.settleRuntimeFocus()
            ScrollBar.vertical:ScrollBar {
                visible:root.scrollingPage && content.contentHeight>content.height
                policy:ScrollBar.AsNeeded
                contentItem:Rectangle {implicitWidth:5*root.unit;radius:width/2;color:host.appearance.focusColor || root.headerInk}
                background:Rectangle {color:"transparent"}
            }
            GridLayout {
                id:grid; width:parent.width; columns:root.columns; columnSpacing:root.gap; rowSpacing:root.gap; visible:!root.placementMode || root.sectionMode
                Repeater {
                    model:root.visibleSections
                    delegate:Item {
                        id:sectionView
                        required property int index; required property var modelData
                        readonly property var style:modelData.presentation || ({})
                        readonly property var metrics:root.geometry.entries[index] || ({span:1})
                        readonly property real padding:(style.padding || 0)*root.unit
                        readonly property int cardColumns:style.columns || 1
                        readonly property int cardRows:Math.max(1,Math.ceil((modelData.items || []).length/cardColumns))
                        readonly property real cardGap:(style.cardGap === undefined ? 16 : style.cardGap)*root.unit
                        readonly property real cardAreaHeight:Math.max(0,Layout.preferredHeight-2*padding-(heading.visible?heading.height+sectionBody.spacing:0)-(subtitle.visible?subtitle.height+sectionBody.spacing:0))
                        readonly property real cardWidth:(sectionBody.width-(cardColumns-1)*cardGap)/cardColumns
                        readonly property real fittedCardWidth:!style.responsive || style.responsive.policy==="scroll" ? cardWidth : Math.min(cardWidth,Math.max(0,(cardAreaHeight-(cardRows-1)*cardGap)/cardRows)*style.responsive.aspect)
                        readonly property string responsiveError:!root.sectionMode && style.responsive && (modelData.items || []).length && sectionBody.width>0 && sectionView.height>0
                            && (fittedCardWidth<style.responsive.minimumWidth*root.unit || fittedCardWidth/style.responsive.aspect<style.responsive.minimumHeight*root.unit)
                            ? "Responsive rules leave no room in "+modelData.name+". Increase the section size, reduce minimum sizes or choose Scroll." : ""
                        Layout.fillWidth:true; Layout.columnSpan:metrics.span
                        Layout.preferredWidth:(content.width-root.gap*(root.columns-1))*metrics.span/root.columns
                        Layout.preferredHeight: {
                            const requested=root.sectionHeight(style, modelData)*root.geometry.factor
                            const isFeatured=style.libraryHero || (modelData.items || []).some(item=>item.presentation && item.presentation.libraryHero)
                            return featured.visible && isFeatured ? Math.min(requested,content.height) : requested
                        }
                        Layout.maximumHeight:Layout.preferredHeight; Layout.alignment:Qt.AlignTop
                        FrostedPanel {
                            objectName:"sectionPanel-"+sectionView.modelData.id
                            anchors.fill:parent; visible:sectionView.style.panelOpacity !== undefined && sectionView.style.panelOpacity > 0
                            wallpaperItem:host.backdropItem; tint:sectionView.style.panelColor || "#FFF3FA"; tintOpacity:sectionView.style.panelOpacity || 0
                            blur:sectionView.style.blur === undefined ? 0.6 : sectionView.style.blur; radius:(sectionView.style.radius || 24)*root.unit
                        }
                        Column {
                            id:sectionBody; x:(parent.width-width)/2; y:sectionView.padding
                            width:Math.max(0,Math.min(parent.width,(sectionView.style.contentWidth || 3840)*root.unit)-2*sectionView.padding)
                            height:parent.height-2*sectionView.padding; spacing:9*root.unit
                            Item {
                                id:heading
                                readonly property var libraryStyle:root.sectionLibrary(sectionView.modelData)
                                visible:libraryStyle.libraryShowLabel===undefined?sectionView.style.showHeading!==false:libraryStyle.libraryShowLabel || !!sectionView.style.trailingAction
                                width:parent.width
                                height:visible ? (sectionView.style.headingSize || 27)*root.unit+6*root.unit : 0
                                LineIcon { visible:heading.libraryStyle.libraryShowLabel!==false && !!sectionView.style.icon; name:sectionView.style.icon || "clock"; ink:sectionView.style.iconColor || host.appearance.iconColor || sectionView.style.headingColor || host.appearance.headingColor || host.appearance.textColor || "#EEEAF0"; width:20*root.unit; height:width; anchors.verticalCenter:parent.verticalCenter }
                                Text { objectName:"sectionHeading-"+sectionView.modelData.id; x:sectionView.style.icon ? 30*root.unit : 0; anchors.verticalCenter:parent.verticalCenter; width:parent.width-x-trailing.width
                                    visible:heading.libraryStyle.libraryShowLabel!==false
                                    text:heading.libraryStyle.libraryShowLabel && heading.libraryStyle.visibleLabel || sectionView.modelData.name; color:sectionView.style.headingColor || host.appearance.headingColor || host.appearance.textColor || "#EEEAF0"; elide:Text.ElideRight
                                    font.family:host.appearance.fontFamily || "DejaVu Sans"; font.pixelSize:(sectionView.style.headingSize || 27)*root.unit*host.textScale
                                    font.weight:sectionView.style.headingBold === false ? Font.Normal : Font.DemiBold }
                                Rectangle {
                                    id:trailing; visible:!!sectionView.style.trailingLabel; width:visible ? 90*root.unit : 0; height:parent.height; anchors.right:parent.right; radius:height/2
                                    color:root.sectionActionId === "section-"+sectionView.modelData.id ? "#B0FFFFFF" : "#30FFFFFF"
                                    border.width:root.sectionActionId === "section-"+sectionView.modelData.id ? 2 : 0; border.color:host.appearance.focusColor || "white"
                                    Text { anchors.centerIn:parent; text:(sectionView.style.trailingLabel || "")+"  ›"; color:host.appearance.textColor || "#514B67"; font.family:host.appearance.fontFamily || "DejaVu Sans"; font.pixelSize:13*root.unit }
                                    MouseArea { anchors.fill:parent; enabled:!!sectionView.style.trailingAction; onClicked:root.activateSection(sectionView.modelData) }
                                }
                            }
                            Text { id:subtitle; visible:!!sectionView.style.subtitle; height:visible ? 24*root.unit : 0; text:sectionView.style.subtitle || ""; color:host.appearance.mutedColor || "#AAA4B2"; font.family:host.appearance.fontFamily || "DejaVu Sans"; font.pixelSize:17*root.unit*host.textScale; width:parent.width; elide:Text.ElideRight }
                            Flickable {
                                visible:sectionView.style.variant!=="library" && !root.sectionMode
                                id:sectionScroll;objectName:"responsiveSection-"+sectionView.modelData.id
                                width:parent.width;height:sectionView.style.responsive?Math.min(sectionCards.implicitHeight,sectionView.cardAreaHeight):sectionCards.implicitHeight
                                contentWidth:width;contentHeight:sectionCards.implicitHeight;clip:true
                                interactive:!!sectionView.style.responsive && sectionView.style.responsive.policy==="scroll"
                            Grid {
                                id:sectionCards
                                visible: sectionView.style.variant !== "library" && !root.sectionMode
                                width:parent.width; columns:sectionView.style.columns || 1; spacing:(sectionView.style.cardGap === undefined ? 16 : sectionView.style.cardGap)*root.unit
                                Repeater {
                                    model:sectionView.modelData.items
                                    delegate:HomeCard {
                                        id:authoredCard
                                        artworkResolver:host.artworkResolver;artworkMotionAllowed:host.artworkMotionAllowed!==false && !host.editMode && !host.freezeData
                                        required property var modelData; required property int index
                                        Component.onCompleted:host.registerEditTarget(modelData.id,authoredCard,false)
                                        Component.onDestruction:host.registerEditTarget(modelData.id,authoredCard,true)
                                        objectName:"composedCard-"+modelData.id
                                        item:Object.assign({},modelData,{presentation:Object.assign({},sectionView.style,modelData.presentation || {})})
                                        variant:sectionView.style.variant || "source"; appearance:host.appearance; unit:root.unit
                                        selectMode:root.selectMode; selectTrigger:root.selectTrigger; selectAction:root.selectAction; selectionColor:root.selectionColor
                                        width:sectionView.style.responsive?sectionView.fittedCardWidth:(parent.width-(parent.columns-1)*parent.spacing)/parent.columns
                                        height:sectionView.style.responsive?Math.max(sectionView.style.responsive.minimumHeight*root.unit,width/sectionView.style.responsive.aspect):Math.max(40,(sectionView.Layout.preferredHeight-2*sectionView.padding-(heading.visible ? heading.height+sectionBody.spacing : 0)-(subtitle.visible ? subtitle.height+sectionBody.spacing : 0)
                                            -(Math.ceil(sectionView.modelData.items.length/parent.columns)-1)*parent.spacing)/Math.max(1,Math.ceil(sectionView.modelData.items.length/parent.columns)))
                                        selected:host.editMode ? host.editSelection===modelData.id : !root.sectionActionId && !root.navigationFocused && !host.recoveryFocused && host.focusedIndex === host.itemIndex(modelData.id)
                                        outsideColor:host.backgroundEnd
                                        onSelectedChanged:if(selected){if(sectionScroll.interactive)sectionScroll.contentY=Math.max(0,Math.min(sectionScroll.contentHeight-sectionScroll.height,y+height-sectionScroll.height));Qt.callLater(root.revealCard,this)}
                                        onActivated:{if(host.editMode){host.editSelected(modelData.id);return}root.sectionActionId="";host.focusedIndex=host.itemIndex(modelData.id);root.navigationFocused=false;host.recoveryFocused=false;root.handleCommand(inputManager.confirm)}
                                        MouseArea {anchors.fill:parent;enabled:host.editMode;onClicked:host.editSelected(authoredCard.modelData.id)}
                                    }
                                }
                            }
                            }
                            PlacedComponents {
                                id:sectionPlacement
                                sectionLabel:(sectionView.modelData.items || []).length===1
                                property string sectionId:sectionView.modelData.id || ""
                                property string sectionName:sectionView.modelData.name || ""
                                readonly property Item sectionFrame:sectionView
                                readonly property string responsiveError:sectionView.responsiveError
                                property bool editableGrid:sectionView.style.variant!=="library"
                                readonly property bool geometryReady:sectionView.width>0 && sectionView.height>0 && Math.abs(sectionView.height-sectionView.Layout.preferredHeight)<0.5
                                width:parent.width;height:Math.max(0,parent.height-heading.height-subtitle.height-2*sectionBody.spacing)
                                visible:root.sectionMode && editableGrid
                                metrics:root.measureGrid(width,height,Object.assign({},root.grid,sectionView.style.grid || {}))
                                items:visible?(sectionView.modelData.items || []).filter(i=>!!i.placement).map(i=>({item:Object.assign({},i,{presentation:Object.assign({},sectionView.style,i.presentation || {})}),placement:i.placement})):[]
                                host:root.host;unit:root.unit
                                runtimeFocusEnabled:!root.sectionActionId && !root.navigationFocused
                                onActivated:item=>root.activatePlaced(item)
                                onFocused:card=>root.revealCard(card)
                                Component.onCompleted:{let surfaces=Object.assign({},root.sectionSurfaces);surfaces[sectionId]=sectionPlacement;root.sectionSurfaces=surfaces}
                                Component.onDestruction:{let surfaces=Object.assign({},root.sectionSurfaces);if(surfaces[sectionId]===sectionPlacement)delete surfaces[sectionId];root.sectionSurfaces=surfaces}
                            }
                            Text {
                                visible:sectionView.style.variant==="library" && !host.libraryProvider
                                width:parent.width;wrapMode:Text.Wrap;text:"Library preview unavailable · choose or configure its source in Content."
                                color:host.appearance.mutedColor || root.headerInk;font.pixelSize:18*root.unit
                            }
                            Loader {
                                width:parent.width; height:Math.max(80,parent.height-heading.height-subtitle.height-2*sectionBody.spacing)
                                active: sectionView.style.variant === "library" && !!host.libraryProvider && host.visible
                                visible: active
                                sourceComponent: PageLibraryWidget {
                                    provider:host.libraryProvider; widgetKey:host.pageKey+"."+sectionView.modelData.id
                                    originId:"widget-"+sectionView.modelData.id
                                    readOnly:host.editMode || host.previewOnly;frozen:host.freezeData;samplePreview:host.samplePreview;sampleItems:host.sampleItems
                                    style:sectionView.style; appearance:host.appearance; unit:root.unit; sectionLabel:true
                                    engaged:root.activeWidget === "widget-"+sectionView.modelData.id
                                    highlighted:!host.editMode && !host.recoveryFocused && !root.navigationFocused && (root.sectionActionId==="widget-"+sectionView.modelData.id || !root.sectionActionId && !!root.focusEntries()[0] && root.focusEntries()[0].id==="widget-"+sectionView.modelData.id)
                                    onEngage: { root.activeWidget="widget-"+sectionView.modelData.id; root.sectionActionId=root.activeWidget; host.recoveryFocused=false; root.navigationFocused=false }
                                    Component.onCompleted:root.widgets["widget-"+sectionView.modelData.id]=this
                                    Component.onDestruction:delete root.widgets["widget-"+sectionView.modelData.id]
                                }
                            }
                        }
                    }
                }
            }
            PlacedComponents {
                id:placementGrid
                objectName:"composedPlacementGrid"
                visible:root.placementMode && !root.sectionMode
                width:content.width;height:content.height
                items:visible?root.viewportItems:[];metrics:root.placementCell;host:root.host;unit:root.unit
                onActivated:item=>root.activatePlaced(item)
                // Delegates can announce selection while the viewport's
                // filtered model is being constructed, before their geometry
                // is ready. Reveal authored placement after that construction.
                onFocused:card=>{if(root.scrollingPage)root.settleRuntimeFocus();else root.revealCard(card)}
            }
        }
    }
}
