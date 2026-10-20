import QtQuick
import QtQuick.Layouts

Item {
    id: root
    objectName: "composedHome"
    required property var host
    readonly property var config: host.composition
    readonly property var tabs: config.navigation || []
    property string activeTabId: config.initialTab || (tabs[0] ? tabs[0].id : "")
    readonly property int activeTab: Math.max(0,tabs.findIndex(t => t.id === activeTabId))
    property int focusedTab: activeTab
    onFocusedTabChanged: Qt.callLater(() => {
        const item = navigationTabs.itemAt(focusedTab); if (!item) return
        if (item.x < navigationStrip.contentX) navigationStrip.contentX = item.x
        else if (item.x + item.width > navigationStrip.contentX + navigationStrip.width)
            navigationStrip.contentX = Math.max(0,item.x + item.width - navigationStrip.width)
    })
    property bool navigationFocused: false
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
    readonly property var headerControls: config.headerControls || (config.headerMode === "identity" ? ["search", "settings"] : ["settings", "edit"])
    readonly property bool identityHeader: config.headerMode === "identity"
    property date now: new Date()
    property string sectionActionId: ""
    property string activeWidget: ""
    property var widgets: ({})
    readonly property var grid: host.grid || ({})
    readonly property var placements: host.placements || []
    readonly property bool placementMode: {
        if(!root.config.grid)return false
        const authored=host.authoredItems || host.items || []
        if(!authored.length)return false
        for(const item of authored)if(!item.placement)return false
        return true
    }
    readonly property var placementCell: {
        const g = root.grid || {}
        const columns = g.columns || 8
        const rows = g.rows || 5
        const gap = (g.gap === undefined ? 20 : g.gap) * root.unit
        const margin = (g.margin === undefined ? 40 : g.margin) * root.unit
        const cellW = Math.max(1, (content.width - 2*margin - (columns-1)*gap) / columns)
        const cellH = Math.max(1, (content.height - 2*margin - (rows-1)*gap) / rows)
        return { columns: columns, rows: rows, gap: gap, margin: margin, cellW: cellW, cellH: cellH }
    }
    function placementRect(p) {
        const c = placementCell
        const origin = placementGrid.mapToItem(host, c.margin, c.margin)
        return {x: origin.x + p.x*(c.cellW+c.gap), y: origin.y + p.y*(c.cellH+c.gap),
            width: p.width*c.cellW + (p.width-1)*c.gap,
            height: p.height*c.cellH + (p.height-1)*c.gap}
    }
    function revealCard(card) {
        if (!card || root.placementMode) return
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
        for (const item of items) {
            const placement = map[item.id]
            if (placement) result.push({ item: item, placement: placement })
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
        return !tab || !tab.sections ? host.sections : tab.sections.map(id => host.sections.find(s => s.id === id)).filter(s => !!s)
    }
    readonly property var geometry: {
        let rows = [], entries = [], column = 0, row = 0
        for (const section of visibleSections) {
            const style = section.presentation || {}
            const span = Math.min(columns, style.span || columns)
            if (column + span > columns) { row++; column = 0 }
            const height = (style.height || 180) * unit
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
        if (placementMode) return placedItems.map(e=>({id:e.item.id,row:e.placement.y,x:e.placement.x+e.placement.width/2}))
        let entries = []
        for (let s = 0; s < visibleSections.length; s++) {
            const section = visibleSections[s], g = geometry.entries[s] || {row:s,column:0,span:columns}, style = section.presentation || {}
            const cards = style.columns || (style.variant === "banner" || style.variant === "summary" ? 1 : 4)
            const values = section.items || []
            if (style.variant === "library") entries.push({id:"widget-"+section.id,widget:true,row:g.row*100,x:g.column+g.span/2})
            const sectionWidth = (content.width - gap*(columns-1))*g.span/columns + (g.span-1)*gap
            const innerWidth = Math.min(sectionWidth,(style.contentWidth || 3840)*unit) - 2*(style.padding || 0)*unit
            const left = g.column*(content.width+gap)/columns + (sectionWidth-innerWidth)/2
            for (let i = 0; i < values.length; i++) entries.push({ id: values[i].id,
                row: g.row * 100 + Math.floor(i/cards), x: (left + (i % cards + 0.5)*innerWidth/cards)/content.width*columns })
            if (style.trailingAction) entries.push({id:"section-"+section.id, sectionAction:true, row:g.row*100+Math.floor(Math.max(0,values.length-1)/cards),x:g.column+g.span+0.5})
        }
        return entries
    }
    function activateTab(index) {
        const tab = tabs[index]; if (!tab) return
        focusedTab = index
        if (tab.action) host.actionActivated(tab.action, "tab-" + tab.id)
        else { activeTabId = tab.id; const first = focusEntries()[0]; if (first) host.focusedIndex = host.itemIndex(first.id) }
    }
    function restoreFocus(id) {
        sectionActionId = id.startsWith("section-") ? id : ""
        navigationFocused = id.startsWith("tab-")
        if (navigationFocused) { const i = tabs.findIndex(t => "tab-" + t.id === id); if (i >= 0) focusedTab = i }
        else if (!focusEntries().some(e => e.id === id)) {
            const tab = tabs.findIndex(t => !t.action && (!t.sections || host.sections.some(s => t.sections.indexOf(s.id) >= 0 && (s.items || []).some(i => i.id === id))))
            if (tab >= 0) activeTabId = tabs[tab].id
        }
    }
    function activateControl(index) {
        const control = headerControls[index]
        if (control === "search") host.searchRequested()
        else host.settingsRequested(control === "edit" ? 1 : -1)
    }
    function handleCommand(command) {
        if (activeWidget) {
            const widget = widgets[activeWidget]
            if (widget && widget.handleCommand(command)) return
            activeWidget = ""
            if (command === inputManager.back) return
        }
        if (host.recoveryFocused) {
            if (command === inputManager.navigateDown) { host.recoveryFocused = false; navigationFocused = tabs.length > 0 }
            else if (command === inputManager.navigateLeft) host.recoveryIndex = Math.max(0,host.recoveryIndex-1)
            else if (command === inputManager.navigateRight) host.recoveryIndex = Math.min(headerControls.length-1,host.recoveryIndex+1)
            else if (command === inputManager.confirm) activateControl(host.recoveryIndex)
            return
        }
        if (navigationFocused) {
            if (command === inputManager.navigateLeft) focusedTab = Math.max(0, focusedTab - 1)
            else if (command === inputManager.navigateRight) focusedTab = Math.min(tabs.length - 1, focusedTab + 1)
            else if (command === inputManager.confirm) activateTab(focusedTab)
            else if (command === inputManager.navigateUp) host.recoveryFocused = true
            else if (command === inputManager.navigateDown) { const first = focusEntries()[0]; if (first) { sectionActionId = first.widget ? first.id : ""; if (!first.widget) host.focusedIndex = host.itemIndex(first.id); navigationFocused = false } }
            return
        }
        const entries = focusEntries(), current = entries.find(e => sectionActionId ? e.id === sectionActionId : !e.sectionAction && host.itemIndex(e.id) === host.focusedIndex) || entries[0]
        if (!current) { host.recoveryFocused = true; return }
        if (command === inputManager.confirm) {
            if (current.widget) { activeWidget=current.id; sectionActionId=current.id; return }
            if (current.sectionAction) { const section = visibleSections.find(s => "section-"+s.id === current.id); if (section) host.actionActivated(section.presentation.trailingAction,current.id); return }
            const item = host.items[host.itemIndex(current.id)]
            if (host.unavailableReason(item)) host.settingsRequested(1)
            else if (item.action) host.actionActivated(item.action, item.id)
            return
        }
        let candidates = [], next
        if (command === inputManager.navigateLeft || command === inputManager.navigateRight) {
            const direction = command === inputManager.navigateLeft ? -1 : 1
            candidates = entries.filter(e => e.row === current.row && (e.x - current.x)*direction > 0).sort((a,b) => Math.abs(a.x-current.x)-Math.abs(b.x-current.x))
        } else if (command === inputManager.navigateUp || command === inputManager.navigateDown) {
            const direction = command === inputManager.navigateUp ? -1 : 1
            const rows = entries.filter(e => (e.row-current.row)*direction > 0).map(e => e.row)
            if (!rows.length && direction < 0) { navigationFocused = tabs.length > 0; host.recoveryFocused = !tabs.length; return }
            const row = direction < 0 ? Math.max(...rows) : Math.min(...rows)
            candidates = entries.filter(e => e.row === row).sort((a,b) => Math.abs(a.x-current.x)-Math.abs(b.x-current.x))
        }
        next = candidates[0]; if (next) { sectionActionId = next.sectionAction || next.widget ? next.id : ""; if (!next.sectionAction && !next.widget) host.focusedIndex = host.itemIndex(next.id) }
    }
    Column {
        anchors.fill: parent; anchors.margins: root.margin; spacing: root.gap
        Row {
            id: header; width: parent.width; height: (root.identityHeader ? 58 : 42)*root.unit; spacing: 22*root.unit
            Flickable {
                id: navigationStrip; clip: true; contentWidth: root.identityHeader ? width : navigationRow.implicitWidth
                width: parent.width - controls.width - (clock.visible ? clock.width : 0) - parent.spacing*(clock.visible ? 2 : 1); height: parent.height
                Row {
                    visible: root.identityHeader; height: parent.height; spacing: 10*root.unit
                    LineIcon { name:"sparkle"; width:38*root.unit; height:width; anchors.verticalCenter:parent.verticalCenter }
                    Text { anchors.verticalCenter:parent.verticalCenter; text:root.config.identity || "Isolde"; color:host.appearance.textColor || "#514B67"; font.family:host.appearance.fontFamily || "DejaVu Sans"; font.pixelSize:48*root.unit; font.weight:Font.Light }
                }
                Row {
                    id: navigationRow; visible:!root.identityHeader; height:parent.height; spacing:26*root.unit
                    Repeater {
                        id:navigationTabs; model:root.tabs
                        delegate:Item {
                            required property int index; required property var modelData
                            width:tabLabel.implicitWidth; height:header.height
                            Text { id:tabLabel; anchors.verticalCenter:parent.verticalCenter; text:modelData.name
                                color:index === root.activeTab || (root.navigationFocused && index === root.focusedTab) ? host.appearance.textColor || "#EEEAF0" : host.appearance.mutedColor || "#AAA4B2"
                                font.family:host.appearance.fontFamily || "DejaVu Sans"; font.pixelSize:21*root.unit*host.textScale; font.weight:index === root.activeTab ? Font.DemiBold : Font.Normal }
                            Rectangle { anchors.bottom:parent.bottom; width:parent.width; height:2*root.unit; color:host.appearance.focusColor || "#BD8198"; visible:root.navigationFocused && index === root.focusedTab }
                            MouseArea { anchors.fill:parent; onClicked:{root.sectionActionId="";root.navigationFocused=true;root.activateTab(index)} }
                        }
                    }
                }
            }
            Item {
                id:controls; width:controlRow.implicitWidth; height:header.height
                FrostedPanel { anchors.fill:parent; anchors.topMargin:4*root.unit; anchors.bottomMargin:4*root.unit; visible:root.identityHeader; wallpaperItem:host.backdropItem; radius:26*root.unit; tintOpacity:0.5 }
                Row {
                    id:controlRow; height:parent.height; spacing:root.identityHeader ? 0 : 10*root.unit
                    Repeater {
                        model:root.headerControls
                        delegate:Rectangle {
                            required property int index; required property string modelData
                            width:(root.identityHeader ? 120 : 100)*root.unit; height:parent.height; radius:root.identityHeader ? 26*root.unit : 12*root.unit
                            color:root.identityHeader ? "transparent" : host.appearance.surfaceColor || "#28262F"
                            border.width:host.recoveryFocused && host.recoveryIndex === index ? 2 : 0; border.color:host.appearance.focusColor || "#BD8198"
                            LineIcon { visible:root.identityHeader; x:16*root.unit; anchors.verticalCenter:parent.verticalCenter; width:23*root.unit; height:width; name:modelData === "search" ? "search" : "settings"; ink:host.appearance.textColor || "#514B67" }
                            Text { anchors.verticalCenter:parent.verticalCenter; x:root.identityHeader ? 47*root.unit : (parent.width-width)/2
                                text:modelData === "edit" ? host.editLabel : modelData === "search" ? "Search" : "Settings"; color:host.appearance.textColor || "#EEEAF0"; font.family:host.appearance.fontFamily || "DejaVu Sans"; font.pixelSize:(root.identityHeader ? 14 : 15)*root.unit }
                            MouseArea { anchors.fill:parent; onClicked:root.activateControl(index) }
                        }
                    }
                }
            }
            Column {
                id:clock; visible:root.config.showClock === true; width:100*root.unit; anchors.verticalCenter:parent.verticalCenter; spacing:3*root.unit
                Text { text:Qt.formatTime(root.now,"h:mm AP"); color:host.appearance.textColor || "#514B67"; font.family:host.appearance.fontFamily || "DejaVu Sans"; font.pixelSize:18*root.unit }
                Text { text:Qt.formatDate(root.now,"ddd, MMM d"); color:host.appearance.mutedColor || "#625B73"; font.family:host.appearance.fontFamily || "DejaVu Sans"; font.pixelSize:12*root.unit }
            }
        }
        Flickable {
            id:content; objectName:"composedContent"; width:parent.width; height:parent.height-header.height-root.gap; contentHeight:root.placementMode ? height : grid.implicitHeight; clip:true
            GridLayout {
                id:grid; width:parent.width; columns:root.columns; columnSpacing:root.gap; rowSpacing:root.gap; visible:!root.placementMode
                Repeater {
                    model:root.visibleSections
                    delegate:Item {
                        id:sectionView
                        required property int index; required property var modelData
                        readonly property var style:modelData.presentation || ({})
                        readonly property var metrics:root.geometry.entries[index] || ({span:1})
                        readonly property real padding:(style.padding || 0)*root.unit
                        Layout.fillWidth:true; Layout.columnSpan:metrics.span
                        Layout.preferredWidth:(content.width-root.gap*(root.columns-1))*metrics.span/root.columns
                        Layout.preferredHeight:(style.height || 180)*root.unit*root.geometry.factor
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
                                id:heading; visible:sectionView.style.showHeading !== false; width:parent.width
                                height:visible ? (sectionView.style.headingSize || 27)*root.unit+6*root.unit : 0
                                LineIcon { visible:!!sectionView.style.icon; name:sectionView.style.icon || "clock"; ink:sectionView.style.iconColor || sectionView.style.headingColor || host.appearance.textColor || "#EEEAF0"; width:20*root.unit; height:width; anchors.verticalCenter:parent.verticalCenter }
                                Text { x:sectionView.style.icon ? 30*root.unit : 0; anchors.verticalCenter:parent.verticalCenter; width:parent.width-x-trailing.width
                                    text:sectionView.modelData.name; color:sectionView.style.headingColor || host.appearance.textColor || "#EEEAF0"; elide:Text.ElideRight
                                    font.family:host.appearance.fontFamily || "DejaVu Sans"; font.pixelSize:(sectionView.style.headingSize || 27)*root.unit*host.textScale
                                    font.weight:sectionView.style.headingBold === false ? Font.Normal : Font.DemiBold }
                                Rectangle {
                                    id:trailing; visible:!!sectionView.style.trailingLabel; width:visible ? 90*root.unit : 0; height:parent.height; anchors.right:parent.right; radius:height/2
                                    color:root.sectionActionId === "section-"+sectionView.modelData.id ? "#B0FFFFFF" : "#30FFFFFF"
                                    border.width:root.sectionActionId === "section-"+sectionView.modelData.id ? 2 : 0; border.color:host.appearance.focusColor || "white"
                                    Text { anchors.centerIn:parent; text:(sectionView.style.trailingLabel || "")+"  ›"; color:host.appearance.textColor || "#514B67"; font.family:host.appearance.fontFamily || "DejaVu Sans"; font.pixelSize:13*root.unit }
                                    MouseArea { anchors.fill:parent; enabled:!!sectionView.style.trailingAction; onClicked:host.actionActivated(sectionView.style.trailingAction,"section-"+sectionView.modelData.id) }
                                }
                            }
                            Text { id:subtitle; visible:!!sectionView.style.subtitle; height:visible ? 24*root.unit : 0; text:sectionView.style.subtitle || ""; color:host.appearance.mutedColor || "#AAA4B2"; font.family:host.appearance.fontFamily || "DejaVu Sans"; font.pixelSize:17*root.unit*host.textScale; width:parent.width; elide:Text.ElideRight }
                            Grid {
                                visible: sectionView.style.variant !== "library"
                                width:parent.width; columns:sectionView.style.columns || 1; spacing:(sectionView.style.cardGap === undefined ? 16 : sectionView.style.cardGap)*root.unit
                                Repeater {
                                    model:sectionView.modelData.items
                                    delegate:HomeCard {
                                        required property var modelData; required property int index
                                        objectName:"composedCard-"+modelData.id
                                        item:Object.assign({},modelData,{presentation:Object.assign({},sectionView.style,modelData.presentation || {})})
                                        variant:sectionView.style.variant || "source"; appearance:host.appearance; unit:root.unit
                                        selectMode:root.selectMode; selectTrigger:root.selectTrigger; selectAction:root.selectAction; selectionColor:root.selectionColor
                                        width:(parent.width-(parent.columns-1)*parent.spacing)/parent.columns
                                        height:Math.max(40,(sectionView.Layout.preferredHeight-2*sectionView.padding-(heading.visible ? heading.height+sectionBody.spacing : 0)-(subtitle.visible ? subtitle.height+sectionBody.spacing : 0)
                                            -(Math.ceil(sectionView.modelData.items.length/parent.columns)-1)*parent.spacing)/Math.max(1,Math.ceil(sectionView.modelData.items.length/parent.columns)))
                                        selected:!root.sectionActionId && !root.navigationFocused && !host.recoveryFocused && host.focusedIndex === host.itemIndex(modelData.id)
                                        outsideColor:host.backgroundEnd
                                        onSelectedChanged:if(selected) Qt.callLater(root.revealCard,this)
                                        onActivated:{root.sectionActionId="";host.focusedIndex=host.itemIndex(modelData.id);root.navigationFocused=false;host.recoveryFocused=false;root.handleCommand(inputManager.confirm)}
                                    }
                                }
                            }
                            Loader {
                                width:parent.width; height:Math.max(80,parent.height-heading.height-subtitle.height-2*sectionBody.spacing)
                                active: sectionView.style.variant === "library" && !!host.libraryProvider && host.visible
                                visible: active
                                sourceComponent: PageLibraryWidget {
                                    provider:host.libraryProvider; widgetKey:host.pageKey+"."+sectionView.modelData.id
                                    style:sectionView.style; appearance:host.appearance
                                    engaged:root.activeWidget === "widget-"+sectionView.modelData.id
                                    highlighted:!host.recoveryFocused && !root.navigationFocused && (root.sectionActionId==="widget-"+sectionView.modelData.id || !root.sectionActionId && !!root.focusEntries()[0] && root.focusEntries()[0].id==="widget-"+sectionView.modelData.id)
                                    onEngage: { root.activeWidget="widget-"+sectionView.modelData.id; root.sectionActionId=root.activeWidget; host.recoveryFocused=false; root.navigationFocused=false }
                                    Component.onCompleted:root.widgets["widget-"+sectionView.modelData.id]=this
                                    Component.onDestruction:delete root.widgets["widget-"+sectionView.modelData.id]
                                }
                            }
                        }
                    }
                }
            }
                Item {
                    visible: root.placementMode
                    width: parent.width
                    height: parent.height
                    Item {
                        id: placementGrid
                        anchors.fill: parent
                        Repeater {
                            model: root.placedItems
                            delegate: Item {
                                required property var modelData
                                property int dragOriginX: modelData.placement.x
                                property int dragOriginY: modelData.placement.y
                                objectName: "placed-" + modelData.item.id
                                x: root.placementCell.margin + modelData.placement.x*(root.placementCell.cellW+root.placementCell.gap)
                                y: root.placementCell.margin + modelData.placement.y*(root.placementCell.cellH+root.placementCell.gap)
                                width: root.placementCell.cellW * modelData.placement.width + root.placementCell.gap * Math.max(0, modelData.placement.width - 1)
                                height: root.placementCell.cellH * modelData.placement.height + root.placementCell.gap * Math.max(0, modelData.placement.height - 1)
                                Drag.active: dragArea.pressed
                                Drag.source: dragArea
                                MouseArea {
                                    id: dragArea
                                    anchors.fill: parent
                                    z: 2
                                    property real pressX: 0
                                    property real pressY: 0
                                    onPressed: mouse => { pressX = mouse.x; pressY = mouse.y }
                                    onClicked: mouse => { mouse.accepted = false }
                                    propagateComposedEvents: true
                                    onReleased: mouse => {
                                        if (!root.host.gridCollisions || !root.host.setItemPlacement) return
                                        const cell = root.placementCell
                                        const newX = modelData.placement.x + Math.round((mouse.x - pressX) / (cell.cellW + cell.gap))
                                        const newY = modelData.placement.y + Math.round((mouse.y - pressY) / (cell.cellH + cell.gap))
                                        if (newX === modelData.placement.x && newY === modelData.placement.y) return
                                        const placement = { x: newX, y: newY, width: modelData.placement.width, height: modelData.placement.height }
                                        const collisions = root.host.gridCollisions(modelData.item.id, placement)
                                        if (collisions.length > 0) return // never silently overwrite
                                        if (root.host.setItemPlacement(modelData.item.id, placement)) {
                                            // placement applied
                                        }
                                    }
                                }
                                HomeCard {
                                    anchors.fill: parent
                                    item: modelData.item
                                    appearance: host.appearance
                                    unit: root.unit
                                    selectMode: root.selectMode
                                    selectTrigger: root.selectTrigger
                                    selectAction: root.selectAction
                                    selectionColor: root.selectionColor
                                    selected: !root.sectionActionId && !root.navigationFocused && !host.recoveryFocused && host.focusedIndex === host.itemIndex(modelData.item.id)
                                    outsideColor: host.backgroundEnd
                                    onActivated: {
                                        root.sectionActionId = ""
                                        host.focusedIndex = host.itemIndex(modelData.item.id)
                                        root.navigationFocused = false
                                        host.recoveryFocused = false
                                        root.handleCommand(inputManager.confirm)
                                    }
                                }
                            }
                        }
                    }
                }
        }
    }
}
