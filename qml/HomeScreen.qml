import QtQuick
import QtQuick.Layouts

Item {
    id: root
    required property var items
    // Profile-authored components are distinct from transient provider notices.
    property var authoredItems: items
    property var authoredSections: sections
    property bool editMode:false
    property var suppressedComponentIds:[]
    property string editSelection:""
    property var editTargets:({})
    property var groupSurfaces:({})
    function registerGroupSurface(id,surface,remove) {
        let groups=Object.assign({},groupSurfaces),entries=(groups[id] || []).filter(item=>item!==surface)
        if(!remove)entries.push(surface)
        if(entries.length)groups[id]=entries;else delete groups[id]
        groupSurfaces=groups
    }
    function groupSurface(id){return (groupSurfaces[id || ""] || []).find(surface=>surface && editorVisible(surface) && surface.item) || null}
    function scopeViewport(id){const surface=groupSurface(id);return surface && surface.viewportRect?surface.viewportRect():null}
    function clippedPlacement(p,scope,parentId){
        const r=placementRectFor(p,scope,parentId),viewport=scopeViewport(parentId)
        if(!viewport)return r
        const x=Math.max(r.x,viewport.x),y=Math.max(r.y,viewport.y),right=Math.min(r.x+r.width,viewport.x+viewport.width),bottom=Math.min(r.y+r.height,viewport.y+viewport.height)
        return {x:x,y:y,width:Math.max(0,right-x),height:Math.max(0,bottom-y)}
    }
    signal editSelected(string componentId)
    function registerEditTarget(id,target,remove) {
        let targets=Object.assign({},editTargets),entries=(targets[id] || []).filter(t=>t!==target)
        if(!remove)entries.push(target)
        if(entries.length)targets[id]=entries;else delete targets[id]
        editTargets=targets
    }
    function editorVisible(target) {
        while(target && target!==root){if(!target.visible)return false;target=target.parent}
        return target===root
    }
    readonly property var editTarget:(editTargets[editSelection] || []).find(t=>t && editorVisible(t) && t.width>0 && t.height>0) || null
    readonly property var editRect: {
        const target=editTarget,scroll=sectionFlick.contentY+composed.scrollOffset
        if(!target)return null
        const geometry=target.x+target.y+target.width+target.height+scroll
        // mapToItem alone does not subscribe to ancestor layout changes.
        let ancestor=target
        while(ancestor && ancestor!==root){const transform=ancestor.x+ancestor.y+ancestor.width+ancestor.height+ancestor.scale+ancestor.rotation;ancestor=ancestor.parent}
        const point=target.mapToItem(root,0,0)
        return {x:point.x,y:point.y,width:target.width,height:target.height}
    }
    function revealEditorSelection() {
        if(!editMode || !editTarget)return
        if(composition.enabled)composed.revealCard(editTarget,true)
        else reveal(editTarget)
    }
    onEditSelectionChanged:Qt.callLater(()=>{if(editMode){restoreFocus(editSelection);revealEditorSelection()}})
    onEditTargetChanged:if(editMode)Qt.callLater(revealEditorSelection)
    property bool scopeView:false
    property string editScopeId:""
    required property var sections
    required property bool brandVisible
    required property color backgroundStart
    required property color backgroundEnd
    required property real tileRoundness
    property var appearance: ({})
    property var composition: ({})
    property var grid: ({})
    property var placements: []
    property int previewExtentRows:0
    readonly property int firstVisibleGridRow:composed.firstVisibleGridRow
    readonly property var pageViewport:composed.viewportRect()
    readonly property var gridMetrics: composed.placementCell
    readonly property real componentUnit:composed.unit
    readonly property string layoutError: {
        if(composed.layoutError)return composed.layoutError
        for(const id of Object.keys(groupSurfaces)){const surface=groupSurface(id);if(surface && surface.item.metrics.valid===false)return "A group's spacing leaves no room for its children. Edit its internal layout or increase its size."}
        return ""
    }
    function placementRect(p) { return composed.placementRect(p,"") }
    function placementRectFor(p,scope,parentId) {
        const surface=groupSurface(parentId)
        if(!surface || !surface.item)return composed.placementRect(p,scope)
        const c=surface.item.metrics,scroll=composed.scrollOffset
        let ancestor=surface;while(ancestor && ancestor!==root){const geometry=ancestor.x+ancestor.y+ancestor.width+ancestor.height+ancestor.scale;ancestor=ancestor.parent}
        const origin=surface.mapToItem(root,0,0)
        return {x:origin.x+p.x*(c.cellW+c.gap),y:origin.y+p.y*(c.cellH+c.gap),width:p.width*c.cellW+(p.width-1)*c.gap,height:p.height*c.cellH+(p.height-1)*c.gap}
    }
    function gridMetricsFor(scope,parentId) { const surface=groupSurface(parentId);return surface && surface.item?surface.item.metrics:composed.gridMetricsFor(scope) }
    function revealScope(scope) { composed.revealScope(scope) }
    function revealPlacement(p,scope,parentId) {
        const surface=groupSurface(parentId)
        if(surface && surface.revealPlacement)surface.revealPlacement(p)
        else if(composition.verticalScrolling && grid.mode==="screen"){
            // Candidate growth changes contentHeight in the same input event.
            // Reveal after that binding settles, including the final step.
            const placement=Object.assign({},p)
            Qt.callLater(()=>composed.revealPlacement(placement,scope))
        }else composed.revealPlacement(p,scope)
    }
    function editorViewport() {
        const groups={}
        for(const id of Object.keys(groupSurfaces)){const surface=groupSurface(id);if(surface && surface.editorViewport)groups[id]=surface.editorViewport()}
        return {legacyY:sectionFlick.contentY,composed:composed.editorViewport(),groups:groups}
    }
    function restoreEditorViewport(state) {
        if(!state || state.legacyY===undefined)return
        sectionFlick.cancelFlick();sectionFlick.contentY=state.legacyY
        composed.restoreEditorViewport(state.composed)
        for(const id of Object.keys(state.groups || {})){const surface=groupSurface(id);if(surface && surface.restoreEditorViewport)surface.restoreEditorViewport(state.groups[id])}
    }
    function registerMediaWidget(id,widget,remove){composed.registerWidget(id,widget,remove)}
    function engageMediaWidget(id){focusedIndex=itemIndex(id);recoveryFocused=false;composed.activeWidget=id}
    property var setItemPlacement: null
    property var gridCollisions: null
    property var libraryProvider: null
    property bool previewOnly:false
    property bool freezeData:false
    property bool samplePreview:false
    property var sampleItems:[]
    property string pageKey: "home"
    property string editLabel: "Edit Home"
    property string selectMode: "single"
    property string selectTrigger: "click"
    property string selectAction: "navigate"
    property string selectionColor: ""
    readonly property bool widgetActive: composed.activeWidget !== ""
    readonly property string activeMediaWidgetId:composed.activeWidget
    readonly property bool groupActive: composed.activeGroup !== ""
    property string wallpaperSource: ""
    property var wallpaperItem: null
    readonly property var backdropItem: wallpaperItem || localWallpaper
    readonly property bool hasWallpaper: wallpaperItem ? wallpaperItem.status === Image.Ready : localWallpaper.status === Image.Ready
    readonly property bool customImageBackground:(composition.background || {}).mode==="image"
    readonly property real textScale: appearance.typographyScale || 1
    required property bool internetAvailable
    required property bool localNetworkAvailable
    required property bool hasContextualSources
    signal actionActivated(var action, string originId)
    signal settingsRequested(int section)
    signal searchRequested()
    signal keepSourceRequested(var source)
    signal hideSourceRequested(string sourceId)
    property int focusedIndex: 0
    property string focusedItemId: ""
    property bool recoveryFocused: false
    property int recoveryIndex: 0
    onFocusedIndexChanged: if (items[focusedIndex]) focusedItemId = items[focusedIndex].id
    onItemsChanged: {
        const retained = itemIndex(focusedItemId)
        focusedIndex = retained >= 0 ? retained : Math.max(0, Math.min(focusedIndex, items.length - 1))
        focusedItemId = items[focusedIndex] ? items[focusedIndex].id : ""
        repairFocus()
    }
    Component.onCompleted:{if (items[focusedIndex])focusedItemId=items[focusedIndex].id;repairFocus()}
    onEditModeChanged:repairFocus()
    readonly property int columns: Math.min(appearance.homeColumns || 3, (width < 1000 ? 2 : Math.floor(width / 200)))
    readonly property int gridUnits: columns * 2

    function spanFor(item) {
        if (item.size === "compact")
            return 1
        if (item.size === "wide")
            return 4
        return 2
    }

    function unavailableReason(item) {
        if (item && item.unavailableReason) return item.unavailableReason
        if (!item || (item.kind !== "app" && item.kind !== "action"))
            return ""
        if (item.networkRequirement === "internet" && !internetAvailable)
            return "Internet needed"
        if (item.networkRequirement === "local-network" && !localNetworkAvailable)
            return "Network needed"
        return ""
    }
    function focusable(item){return editMode || item && !(item.componentType==="image" && !item.action)}
    function repairFocus(){if(editMode)return;if(composition.enabled && (sections || []).some(s=>(s.presentation || {}).variant==="library")){recoveryFocused=false;return;}if(items[focusedIndex] && focusable(items[focusedIndex]))return;const first=items.findIndex(i=>focusable(i));if(first<0){recoveryFocused=true;return}focusedIndex=first;focusedItemId=items[first].id;recoveryFocused=false}

    function visualEntries() {
        let result = []
        let row = 0
        for (let s = 0; s < sections.length; ++s) {
            let col = 0
            const sectionItems = sections[s].items || []
            for (let i = 0; i < sectionItems.length; ++i) {
                const item = sectionItems[i]
                const span = Math.min(gridUnits, spanFor(item))
                if (col + span > gridUnits) {
                    row++
                    col = 0
                }
                if(focusable(item))result.push({ index: itemIndex(item.id), row: row, col: col, span: span, center: col + span / 2 })
                col += span
                if (col >= gridUnits) {
                    row++
                    col = 0
                }
            }
            if (sectionItems.length > 0 && col > 0)
                row++
        }
        return result
    }

    function nearestInRow(entries, row, center) {
        let best = -1
        let bestDistance = 99999
        for (let i = 0; i < entries.length; ++i) {
            if (entries[i].row !== row)
                continue
            const distance = Math.abs(entries[i].center - center)
            if (distance < bestDistance) {
                best = entries[i].index
                bestDistance = distance
            }
        }
        return best
    }

    function moveVertical(direction) {
        const entries = visualEntries()
        const current = entries.find(e => e.index === focusedIndex)
        if (!current)
            return focusedIndex
        let row = current.row + direction
        while (row >= 0 && row <= entries[entries.length - 1].row) {
            const next = nearestInRow(entries, row, current.center)
            if (next >= 0)
                return next
            row += direction
        }
        return focusedIndex
    }

    function moveHorizontal(direction) {
        const entries = visualEntries()
        const current = entries.find(e => e.index === focusedIndex)
        if (!current)
            return focusedIndex
        const rowEntries = entries.filter(e => e.row === current.row).sort((a, b) => a.col - b.col)
        const rowIndex = rowEntries.findIndex(e => e.index === focusedIndex)
        const next = rowEntries[rowIndex + direction]
        return next ? next.index : focusedIndex
    }

    function handleCommand(command) {
        if (composition.enabled) { composed.handleCommand(command); return }
        if (recoveryFocused || !items.length) {
            recoveryFocused = true
            if (command === inputManager.navigateLeft) recoveryIndex = 0
            else if (command === inputManager.navigateRight) recoveryIndex = 1
            else if (command === inputManager.navigateDown && items.some(i=>focusable(i))) recoveryFocused = false
            else if (command === inputManager.confirm) settingsRequested(recoveryIndex === 0 ? -1 : 1)
            return
        }
        let next = focusedIndex
        if (command === inputManager.navigateLeft)
            next = moveHorizontal(-1)
        else if (command === inputManager.navigateRight)
            next = moveHorizontal(1)
        else if (command === inputManager.navigateUp) {
            next = moveVertical(-1)
            if (next === focusedIndex) recoveryFocused = true
        }
        else if (command === inputManager.navigateDown)
            next = moveVertical(1)
        else if (command === inputManager.confirm) {
            const item = items[focusedIndex]
            if (item.unavailableReason) settingsRequested(1)
            else if (item.action && unavailableReason(item) === "") actionActivated(item.action, item.id)
        }
        focusedIndex = next
    }

    function restoreFocus(originId) {
        if (composition.enabled && originId) composed.restoreFocus(originId)
        if (!originId) return
        const exact = itemIndex(originId)
        if (exact >= 0) { focusedIndex = exact; focusedItemId = originId; recoveryFocused = false; return }
        for (let i = 0; i < items.length; ++i) {
            if (items[i].appId === originId) {
                focusedIndex = i
                recoveryFocused = false
                return
            }
        }
        focusedIndex = Math.max(0, Math.min(focusedIndex, items.length - 1))
        focusedItemId = items[focusedIndex] ? items[focusedIndex].id : ""
        recoveryFocused = !items.length
    }

    function itemIndex(itemId) {
        for (let i = 0; i < items.length; ++i)
            if (items[i].id === itemId) return i
        return -1
    }

    function focusedItem() {
        return items.length ? items[Math.max(0, Math.min(focusedIndex, items.length - 1))] : null
    }

    function keepFocusedSource() {
        const item = focusedItem()
        if (item && item.contextual === true)
            keepSourceRequested(item)
    }

    function hideFocusedSource() {
        const item = focusedItem()
        if (item && item.contextual === true)
            hideSourceRequested(item.id)
    }

    function reveal(item) {
        const point = item.mapToItem(sectionContent, 0, 0)
        if (point.y < sectionFlick.contentY) sectionFlick.contentY = Math.max(0, point.y - 10)
        else if (point.y + item.height > sectionFlick.contentY + sectionFlick.height)
            sectionFlick.contentY = Math.min(sectionFlick.contentHeight - sectionFlick.height, point.y + item.height - sectionFlick.height + 10)
    }

    Rectangle {
        visible: root.customImageBackground?!root.hasWallpaper:!root.composition.enabled || !root.hasWallpaper
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.backgroundStart }
            GradientStop { position: 1.0; color: (root.composition.background || {}).mode==="solid"?root.backgroundStart:root.backgroundEnd }
        }
    }
    PageBackdrop {id:localWallpaper;anchors.fill:parent;source:root.wallpaperSource;config:root.composition.background || {};startColor:root.backgroundStart;endColor:root.backgroundEnd;visible:(!!root.composition.enabled || !!root.composition.background) && !root.wallpaperItem}

    Column {
        visible: !root.composition.enabled
        anchors.fill: parent
        anchors.margins: Math.max(48, root.width * 0.055)
        spacing: root.brandVisible ? 28 : 16

        Row {
            width: parent.width; height: 42; spacing: 12
            Repeater {
                model: ["Settings", "Edit Home"]
                delegate: Rectangle {
                    required property int index; required property string modelData
                    width: 150; height: 42; radius: 12; color: root.appearance.surfaceColor || "#202126"
                    border.width: !root.editMode && (root.recoveryFocused || !root.items.length) && root.recoveryIndex === index ? 3 : 0
                    border.color: root.appearance.focusColor || "#C994A5"
                    Text { anchors.centerIn: parent; text: modelData; color: root.appearance.textColor || "#F5F1F3"; font.pixelSize: 19 * root.textScale }
                    MouseArea { anchors.fill: parent; onClicked: { root.recoveryFocused = true; root.recoveryIndex = index; root.settingsRequested(index === 0 ? -1 : 1) } }
                }
            }
            Text { anchors.verticalCenter: parent.verticalCenter; text: root.items.length ? "↑ Settings · Menu Search" : "Home is empty · OK opens Settings"; color: root.appearance.mutedColor || "#AAA5AF"; font.pixelSize: 16 * root.textScale }
        }

        Text {
            visible: root.hasContextualSources
            width: parent.width
            text: "New source detected. Enter opens it · K keeps it on Home · Delete hides it"
            color: "#A7F3D0"
            font.pixelSize: 15 * root.textScale
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignRight
        }

        Row {
            width: parent.width
            height: root.brandVisible ? 92 : 0
            visible: root.brandVisible
            spacing: 18
            Text {
                text: "ISOLDE"
                color: root.appearance.textColor || "#F5F1F3"
                font.pixelSize: 46 * root.textScale
                font.weight: Font.DemiBold
                font.letterSpacing: 6
            }
            Text {
                anchors.baseline: parent.children[0].baseline
                text: isoldeVersion
                color: "#71809F"
                font.pixelSize: 17 * root.textScale
            }
        }

        Flickable {
            id: sectionFlick
            objectName: "homeContent"
            width: parent.width
            height: Math.max(0, parent.height - y)
            contentHeight: sectionContent.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Column {
                id: sectionContent
                width: parent.width
                spacing: 28
                Repeater {
                    model: root.sections
                    delegate: Column {
                        required property var modelData
                        width: sectionContent.width
                        spacing: 12
                        Text { text: modelData.name.toUpperCase(); color: root.appearance.mutedColor || "#AAA5AF"; font.pixelSize: 15 * root.textScale; font.weight: Font.DemiBold; font.letterSpacing: 3 }
                        GridLayout {
                            width: parent.width
                            columns: root.columns * 2
                            columnSpacing: root.appearance.spacing || 20; rowSpacing: root.appearance.spacing || 20
                            Repeater {
                                model: modelData.items
                                delegate: Item {
                                    id: tileHost
                                    required property var modelData
                                    Component.onCompleted:root.registerEditTarget(modelData.id,tileHost,false)
                                    Component.onDestruction:root.registerEditTarget(modelData.id,tileHost,true)
                                    readonly property int globalIndex: root.itemIndex(modelData.id)
                                    readonly property bool selected: !root.editMode && !root.recoveryFocused && globalIndex === root.focusedIndex
                                    Layout.columnSpan: Math.min(root.gridUnits, modelData.size === "compact" ? 1 : modelData.size === "wide" ? 4 : 2)
                                    Layout.fillWidth: true
                                    height: 150
                                    onSelectedChanged: if (selected) root.reveal(tileHost)
                                    Loader {
                                        anchors.fill: parent
                                        opacity: (modelData.opacity === undefined ? 1.0 : modelData.opacity) * (root.unavailableReason(modelData) === "" ? 1.0 : 0.42)
                                        sourceComponent: modelData.widgetType === "clock" ? clockComponent : modelData.widgetType === "text" ? textComponent : appComponent
                                    }
                                    Rectangle {
                                        visible: root.unavailableReason(modelData) !== ""
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 12
                                        width: Math.min(parent.width - 24, reasonLabel.implicitWidth + 22)
                                        height: 30
                                        radius: 15
                                        color: "#CC111827"
                                        border.width: tileHost.selected ? 1 : 0
                                        border.color: root.appearance.focusColor || "#C994A5"
                                        Text {
                                            id: reasonLabel
                                            anchors.centerIn: parent
                                            text: root.unavailableReason(modelData).toUpperCase()
                                            width: parent.width - 22; elide: Text.ElideRight
                                            color: "#FCA5A5"
                                            font.pixelSize: 11 * root.textScale
                                            font.weight: Font.DemiBold
                                            font.letterSpacing: 1.2
                                        }
                                    }
                                    Rectangle {
                                        visible: modelData.contextual === true
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.margins: 12
                                        width: sourceLabel.implicitWidth + 22
                                        height: 30
                                        radius: 15
                                        color: "#CC064E3B"
                                        border.width: tileHost.selected ? 1 : 0
                                        border.color: "#D1FAE5"
                                        Text {
                                            id: sourceLabel
                                            anchors.centerIn: parent
                                            text: "NEW SOURCE"
                                            color: "#A7F3D0"
                                            font.pixelSize: 11 * root.textScale
                                            font.weight: Font.DemiBold
                                            font.letterSpacing: 1.2
                                        }
                                    }
                                    Rectangle {
                                        anchors.fill: parent; color: "transparent"
                                        radius: Math.min(width, height) * 0.5 * root.tileRoundness
                                        border.width: tileHost.selected ? 4 : 0; border.color: root.appearance.focusColor || "#C994A5"
                                        visible: tileHost.selected && (modelData.opacity !== undefined && modelData.opacity < 0.35)
                                    }
                                    Component {
                                        id: appComponent
                                        AppTile {
                                            app: modelData
                                            selected: tileHost.selected
                                            roundness: root.tileRoundness
                                            appearance: root.appearance
                                            onActivated: {
                                                root.focusedIndex = tileHost.globalIndex
                                                root.recoveryFocused = false
                                                if (modelData.unavailableReason) { root.settingsRequested(1); return }
                                                if (root.unavailableReason(modelData) !== "")
                                                    return
                                                if (modelData.action) root.actionActivated(modelData.action, modelData.id)
                                            }
                                        }
                                    }
                                    Component { id: clockComponent; ClockWidget { selected: tileHost.selected; roundness: root.tileRoundness; appearance: root.appearance } }
                                    Component { id:textComponent; HomeCard { objectName:"legacyTextWidget-"+modelData.id; item:modelData; appearance:root.appearance; selected:tileHost.selected; onActivated:{root.focusedIndex=tileHost.globalIndex;root.recoveryFocused=false} } }
                                    MouseArea {anchors.fill:parent;enabled:root.editMode;onClicked:root.editSelected(tileHost.modelData.id)}
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    ComposedHome { id: composed; anchors.fill: parent; host: root; visible: !!root.composition.enabled }
}
