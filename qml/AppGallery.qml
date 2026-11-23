import QtQuick

Item {
    id: root
    objectName: "allAppsGallery"
    property bool open: false
    property var entries: []
    property var availableToAdd: []
    property var appearance: ({})
    property var iconStyle:typeof homeLayout!=="undefined"?(homeLayout.studioOptions.appIconStyles || {}).tiles || {}:({})
    property var galleryPresentation: ({layout:"grid",columns:5,density:"standard",sort:"name",showArtwork:true,showLabels:true,filterEnabled:true,favoritesEnabled:true,title:"More"})
    property bool internetAvailable: false
    property bool localNetworkAvailable: false
    property string originId: ""
    property string selectedId: ""
    property int lastSelectedIndex: 0
    property string filterText: ""
    property string focusArea: "grid"
    property bool keyboardOpen: false
    property bool favoritesOnly: false
    property string menuMode: ""
    property int menuIndex: 0
    property string feedback: ""
    property bool launchPending: false
    property string launchAttemptAppId: ""
    property int launchGeneration: 0
    readonly property var favoriteIds: galleryPresentation.favoriteIds || []
    readonly property int columns: galleryPresentation.columns || 5
    readonly property string layoutMode: galleryPresentation.layout || "grid"
    // Sort new apps after the authored order while retaining the folder's configured fallback.
    readonly property var orderedEntries: {
        const order = galleryPresentation.orderIds || []
        const positions = new Map(order.map((id, index) => [id, index]))
        return entries.slice().sort((a, b) =>
            (positions.get(a.id) ?? order.length) - (positions.get(b.id) ?? order.length)
            || (galleryPresentation.sort === "favorites" ? Number(favoriteIds.includes(b.id)) - Number(favoriteIds.includes(a.id)) : 0)
            || String(a.name || "").localeCompare(String(b.name || "")) || String(a.id).localeCompare(String(b.id)))
    }
    readonly property var visibleEntries: {
        const query = filterText.trim().toLowerCase()
        return orderedEntries.filter(app => !(galleryPresentation.hiddenIds || []).includes(app.id)
            && (!favoritesOnly || favoriteIds.includes(app.id)) && (!query
            || String(app.name || "").toLowerCase().includes(query)
            || String(app.section || "").toLowerCase().includes(query)))

    }
    readonly property int selectedIndex: visibleEntries.findIndex(app => app.id === selectedId)
    SurfacePalette { id: theme; appearance: root.appearance }

    // Intercept remote/keyboard arrow keys at grid edges and from controls,
    // so focus never leaks to Settings or other off-grid elements. GridView's
    // native Up/Down/Left/Right within the grid is left untouched.
    Keys.onPressed: function(event) {
        const key = event.key
        // From any control: arrow keys go through handleCommand (to grid, or between controls)
        if (focusArea !== "grid") {
            if (key === Qt.Key_Up) { root.handleCommand(inputManager.navigateUp); event.accepted = true; return }
            if (key === Qt.Key_Down) { root.handleCommand(inputManager.navigateDown); event.accepted = true; return }
            if (key === Qt.Key_Left) { root.handleCommand(inputManager.navigateLeft); event.accepted = true; return }
            if (key === Qt.Key_Right) { root.handleCommand(inputManager.navigateRight); event.accepted = true; return }
        }
        // From grid edges: prevent focus leak to Settings/controls
        if (focusArea === "grid" && visibleEntries.length > 0) {
            const idx = visibleEntries.findIndex(app => app.id === selectedId)
            const step = layoutMode === "list" ? 1 : columns
            const lastIdx = visibleEntries.length - 1
            if (key === Qt.Key_Right && idx === lastIdx) {
                root.handleCommand(inputManager.navigateRight); event.accepted = true; return
            }
            if (key === Qt.Key_Left && idx === 0) {
                root.handleCommand(inputManager.navigateLeft); event.accepted = true; return
            }
            if (key === Qt.Key_Down && idx >= visibleEntries.length - step) {
                root.handleCommand(inputManager.navigateDown); event.accepted = true; return
            }
            if (key === Qt.Key_Up && idx < step) {
                root.handleCommand(inputManager.navigateUp); event.accepted = true; return
            }
        }
        // Confirm/Back from anywhere
        if (key === Qt.Key_Select || key === Qt.Key_Return || key === Qt.Key_Enter) {
            root.handleCommand(inputManager.confirm); event.accepted = true
        }
        else if (key === Qt.Key_Back) {
            root.handleCommand(inputManager.back); event.accepted = true
        }
    }

    signal launchRequested(string appId)
    signal storeRequested()
    signal settingsRequested()
    signal closeRequested(string originId)
    signal searchRequested()
    signal favoriteChanged(string appId, bool favorite)
    signal folderEdited(var patch)

    function showForOrigin(origin) {
        if (originId !== origin) {
            selectedId = ""
            filterText = ""
            favoritesOnly = false
        }
        originId = origin
        open = true
        keyboardOpen = false
        focusArea = "grid"
        menuMode = ""
        if (visibleEntries.length && !visibleEntries.some(app => app.id === selectedId))
            selectedId = visibleEntries[0].id
    }

    function resumeAfterApp() {
        open = true
        keyboardOpen = false
        focusArea = "grid"
        if (!visibleEntries.some(app => app.id === selectedId))
            selectedId = visibleEntries.length ? visibleEntries[0].id : ""
    }

    function dispatchFromGallery(dispatcher, action) {
        const generation = ++launchGeneration
        launchPending = true
        launchAttemptAppId = action.appId || ""
        const accepted = dispatcher.dispatch(action, "all-apps")
        if (generation === launchGeneration) {
            if (accepted && launchPending) open = false
            else if (!accepted) {
                launchPending = false
                launchAttemptAppId = ""
            }
        }
        return accepted
    }

    function clearLaunchAttempt() {
        ++launchGeneration
        launchPending = false
        launchAttemptAppId = ""
    }

    function restoreAfterAction() {
        launchPending = false
        const generation = launchGeneration
        // VPN failure is emitted after returnedHome in the same signal stack.
        Qt.callLater(() => {
            if (generation === launchGeneration && !launchPending) launchAttemptAppId = ""
        })
        resumeAfterApp()
    }

    function vpnLaunchFailed(appId) {
        if (!appId || launchAttemptAppId !== appId) return false
        clearLaunchAttempt()
        resumeAfterApp()
        return true
    }

    function selectNearest(oldIndex) {
        if (visibleEntries.length && !visibleEntries.some(app => app.id === selectedId))
            selectAt(oldIndex)
        else if (!visibleEntries.length)
            selectedId = ""
    }

    function toggleFavorite(appId) {
        if (galleryPresentation.favoritesEnabled === false || !appId) return
        const next = favoriteIds.slice()
        const index = next.indexOf(appId)
        if (index >= 0) next.splice(index, 1)
        else next.push(appId)
        favoriteChanged(appId, index < 0)
    }

    function verticalStep() {
        return layoutMode === "list" ? 1 : columns
    }

    // Translate folder menu commands into a persisted order or view preference.
    function menuActions() {
        if(menuMode==="manage")return orderedEntries.map(app=>({
            label:((galleryPresentation.hiddenIds || []).includes(app.id)?"Show ":"Hide ")+app.name,
            key:"visibility", id:app.id
        })).concat(availableToAdd.map(app=>({label:"Add "+app.name,key:"add",id:app.id})),[{label:"Back",key:"back"}])
        const app = visibleEntries[selectedIndex]
        const actions = []
        if(app){
            if(galleryPresentation.favoritesEnabled !== false && !app.systemShortcut)
                actions.push({label:favoriteIds.includes(app.id)?"Remove from favorites":"Set as favorite",key:"favorite"})
            actions.push({label:"Move up",key:"up"},{label:"Move down",key:"down"},
                         {label:"Move to front",key:"front"},{label:"Move to back",key:"end"})
        }
        actions.push({label:"Manage apps in this folder",key:"manage"})
        if(galleryPresentation.filterEnabled!==false)actions.push({label:"Filter apps",key:"filter"})
        if(galleryPresentation.favoritesEnabled!==false)actions.push({label:favoritesOnly?"Show all apps":"Show favorites only",key:"favorites"})
        return actions.concat([{label:"Back",key:"back"}])
    }

    // Reorder visible neighbors without discarding hidden apps or changing membership.
    function moveApp(key) {
        const ids=orderedEntries.map(app=>app.id)
        const shown=orderedEntries.filter(app=>!(galleryPresentation.hiddenIds || []).includes(app.id))
        const index=shown.findIndex(app=>app.id===selectedId)
        if(index<0)return
        const target=key==="front"?0:key==="end"?shown.length-1:key==="up"?index-1:index+1
        if(target<0 || target>=shown.length || target===index)return
        const destination=ids.indexOf(shown[target].id)
        ids.splice(ids.indexOf(selectedId),1)
        ids.splice(destination,0,selectedId)
        folderEdited({orderIds:ids})
    }

    // Keep focus on the app while applying actions and on the corresponding row in Manage.
    function chooseMenu(index) {
        const action=menuActions()[index]
        if(!action)return
        if(action.key==="manage"){menuMode="manage";menuIndex=0;return}
        if(action.key==="add"){
            folderEdited({addedIds:(galleryPresentation.addedIds || []).concat([action.id])})
            menuIndex=0
            return
        }
        if(action.key==="visibility"){
            const hidden=(galleryPresentation.hiddenIds || []).slice(), at=hidden.indexOf(action.id)
            if(at>=0)hidden.splice(at,1)
            else hidden.push(action.id)
            folderEdited({hiddenIds:hidden})
            return
        }
        if(action.key==="back" && menuMode==="manage"){menuMode="app";menuIndex=0;return}
        menuMode=""
        if(action.key==="favorite")toggleFavorite(selectedId)
        else if(action.key==="filter"){keyboard.value=filterText;keyboard.selectedIndex=0;keyboardOpen=true}
        else if(action.key==="favorites"){favoritesOnly=!favoritesOnly;selectNearest(lastSelectedIndex)}
        else if(["up","down","front","end"].includes(action.key))moveApp(action.key)
    }

    function handleCommand(command) {
        if (!open) return false
        if (keyboardOpen) {
            keyboard.handleCommand(command)
            return true
        }
        if (menuMode && command !== inputManager.home) {
            if(command===inputManager.back || command===inputManager.menu){
                if(menuMode==="manage"){menuMode="app";menuIndex=0}else menuMode=""
            }else if(command===inputManager.navigateUp)menuIndex=Math.max(0,menuIndex-1)
            else if(command===inputManager.navigateDown)menuIndex=Math.min(menuActions().length-1,menuIndex+1)
            else if(command===inputManager.confirm)chooseMenu(menuIndex)
            return true
        }
        if (command === inputManager.back) {
            keyboardOpen = false
            closeRequested(originId)
            return true
        }
        if (command === inputManager.home) {
            closeRequested("")
            return true
        }
        if (command === inputManager.search) {
            keyboardOpen = false
            searchRequested()
            return true
        }
        if (focusArea === "search") {
            if (command === inputManager.navigateLeft || command === inputManager.navigateDown) focusArea = "grid"
            else if (command === inputManager.confirm) searchRequested()
            return true
        }
        if (focusArea === "filter" || focusArea === "favorites" || focusArea === "store" || focusArea === "settings") {
        if (command === inputManager.navigateLeft || command === inputManager.navigateRight) {
            const controls = (galleryPresentation.filterEnabled === false ? [] : ["filter"])
                .concat(galleryPresentation.favoritesEnabled ? ["favorites"] : [], ["store", "settings"])
            const index = controls.indexOf(focusArea)
            focusArea = controls[Math.max(0, Math.min(controls.length - 1,
                index + (command === inputManager.navigateRight ? 1 : -1)))]
            return true
        }
        if (command === inputManager.navigateDown) {
            focusArea = "grid"
            if (!selectedId && visibleEntries.length) selectedId = visibleEntries[0].id
            return true
        }
        if (command === inputManager.confirm) {
            if (focusArea === "filter") {
                keyboard.value = filterText
                keyboard.selectedIndex = 0
                keyboardOpen = true
            } else if (focusArea === "favorites") {
                favoritesOnly = !favoritesOnly
                if (favoritesOnly && !visibleEntries.some(app => app.id === selectedId)) selectedId = visibleEntries.length ? visibleEntries[0].id : ""
            } else if (focusArea === "store") {
                storeRequested()
            } else if (focusArea === "search") {
                searchRequested()
            } else {
                settingsRequested()
            }
            return true
        }
        return true
        }
        if (command === inputManager.navigateUp) {
            const index = selectedIndex
            const step = verticalStep()
            if (index < 0 || index < step) return true
            else selectAt(index - step)
            return true
        }
        if (command === inputManager.navigateDown) {
            if (!visibleEntries.length) {
                menuMode = "app"; menuIndex = 0
                return true
            }
            selectAt(Math.min(visibleEntries.length - 1, Math.max(0, selectedIndex) + verticalStep()))
            return true
        }
        if (command === inputManager.navigateLeft || command === inputManager.navigateRight) {
            if (!visibleEntries.length) return true
            const controls = (galleryPresentation.filterEnabled === false ? [] : ["filter"])
                .concat(galleryPresentation.favoritesEnabled ? ["favorites"] : [], ["store", "settings"])
            const nextIndex = Math.max(0, selectedIndex) + (command === inputManager.navigateRight ? 1 : -1)
            if (command === inputManager.navigateRight && nextIndex >= visibleEntries.length) {
                focusArea = "search"
                return true
            }
            if (command === inputManager.navigateLeft && nextIndex < 0) {
                focusArea = controls[0]
                return true
            }
            selectAt(Math.max(0, Math.min(visibleEntries.length - 1, nextIndex)))
            return true
        }
        if (command === inputManager.confirm) {
            const app = visibleEntries[selectedIndex]
            if (app) activateApp(app)
            else {menuMode = "app";menuIndex = 0}
            return true
        }
        if (command === inputManager.menu) {
            menuMode = "app"
            menuIndex = 0
            return true
        }
        return true
    }

    // System destinations live in the folder alongside installed apps.
    function activateApp(app) {
        if(app.systemShortcut==="store")storeRequested()
        else if(app.systemShortcut==="settings")settingsRequested()
        else launchRequested(app.id)
    }

    function selectAt(index) {
        if (visibleEntries.length) {
            lastSelectedIndex = Math.max(0, Math.min(index, visibleEntries.length - 1))
            selectedId = visibleEntries[lastSelectedIndex].id
        }
    }

    function stateLabel(app) {
        if (app.state === "needs-setup") return "Needs setup" + (app.setupPrompt ? " · " + app.setupPrompt : "")
        if (app.state === "failed") return "Failed" + (app.failureReason ? " · " + app.failureReason : "")
        if (app.networkRequirement === "internet" && !internetAvailable) return "Unavailable offline"
        if (app.networkRequirement === "local-network" && !localNetworkAvailable) return "Local network unavailable"
        return "Ready" + (app.section ? " · " + app.section : "")
    }

    onSelectedIdChanged: {
        const index = visibleEntries.findIndex(app => app.id === selectedId)
        if (index >= 0) lastSelectedIndex = index
    }
    onVisibleEntriesChanged: selectNearest(lastSelectedIndex)

    Rectangle {
        anchors.fill: parent
        radius:theme.radius
        gradient:Gradient {
            GradientStop { position:0;color:root.appearance.backgroundStart || theme.background }
            GradientStop { position:1;color:theme.background }
        }
    }
    Column {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 16 * theme.gapScale
        Row {
            width: parent.width
            spacing: 16 * theme.gapScale
            Text {
                objectName: "appFolderTitle"
                width: parent.width - search.width - parent.spacing
                anchors.verticalCenter: parent.verticalCenter
                text: root.galleryPresentation.title || qsTr("More")
                textFormat: Text.PlainText
                color: theme.backgroundText
                font.family: theme.fontFamily
                font.pixelSize: 34 * theme.textScale
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Rectangle {
                id: search
                width: 130 * theme.textScale
                height: 44 * theme.textScale
                radius: theme.radius
                color: root.focusArea === "search" ? theme.selected : theme.surface
                border.width: root.focusArea === "search" ? 2 : 0
                border.color: theme.selectedFocus
                Text { anchors.centerIn: parent; text: qsTr("Search"); color: theme.text; font.family: theme.fontFamily; font.pixelSize: 18 * theme.textScale }
                MouseArea { anchors.fill: parent; onClicked: root.searchRequested() }
            }
        }
        Text {
            width: parent.width
            visible: !!text
            text: root.visibleEntries.length
                ? root.feedback || (root.filterText ? qsTr("Filter · %1").arg(root.filterText) : root.favoritesOnly ? qsTr("Favorites") : "")
                : root.favoritesOnly ? "No favorite apps match. Select an app and press Menu to mark it favorite."
                    : "No apps shown. Press Menu to manage this folder."
            color: theme.backgroundMuted;font.family:theme.fontFamily
            font.pixelSize: 18 * theme.textScale
        }
        GridView {
            id: appGrid
            objectName: "allAppsGrid"
            width: parent.width
            height: Math.max(0,parent.height-y)
            clip: true
            readonly property real tileGap:root.galleryPresentation.tileGap ?? 12
            cellWidth: root.layoutMode === "list" ? width : width / root.columns
            cellHeight: Math.min(height, (root.galleryPresentation.tileHeight || (root.layoutMode === "list" ? 82 : 172))
                * (root.galleryPresentation.density === "compact" ? 0.82 : root.galleryPresentation.density === "spacious" ? 1.15 : 1))
            model: root.visibleEntries
            currentIndex: root.selectedIndex
            highlightMoveDuration: 0
            delegate: Loader {
                id: entry
                required property var modelData
                width: Math.max(1,appGrid.cellWidth - appGrid.tileGap)
                height: Math.max(1,appGrid.cellHeight - appGrid.tileGap)
                sourceComponent: root.layoutMode === "list" ? listCard : gridCard
                function activateEntry() {
                    root.focusArea = "grid"
                    root.selectedId = modelData.id
                    root.activateApp(modelData)
                }
                Component {
                    id: gridCard
                    AppTile {
                        style:Object.assign({},root.iconStyle,{surfaceOpacity:root.galleryPresentation.surfaceOpacity,
                            surfaceColor:root.galleryPresentation.surfaceColor,focusColor:root.galleryPresentation.focusColor,
                            tileRadius:root.galleryPresentation.radius})
                        frameFocus: true
                        centerContent: true
                        objectName: "allAppsTile"
                        Accessible.role: Accessible.Button
                        Accessible.name: entry.modelData.name
                        Accessible.description: root.stateLabel(entry.modelData)
                        app: Object.assign({}, entry.modelData, {
                            showTitle: root.galleryPresentation.showLabels !== false,
                            showType: root.galleryPresentation.showLabels !== false && !root.stateLabel(entry.modelData).startsWith("Ready"),
                            showLogo: root.galleryPresentation.showArtwork !== false,
                            logoFill: false,
                            section: root.stateLabel(entry.modelData)
                        })
                        roundness: root.appearance.tileRoundness === undefined ? 0.2 : root.appearance.tileRoundness
                        appearance: root.appearance
                        selected: root.open && root.focusArea === "grid" && root.selectedId === entry.modelData.id
                        onActivated: entry.activateEntry()
                    }
                }
                Component {
                    id: listCard
                    HomeCard {
                        objectName: "allAppsListCard"
                        item: ({
                            name: entry.modelData.name,
                            showTitle: root.galleryPresentation.showLabels !== false,
                            unavailableReason: "",
                            action: ({type: "launch-app", appId: entry.modelData.id}),
                            logoSource: entry.modelData.logoSource || "",
                            showLogo: root.galleryPresentation.showArtwork !== false,
                            appSymbol: entry.modelData.symbol || entry.modelData.name.slice(0, 1),
                            presentation: ({
                                surfaceOpacity:root.galleryPresentation.surfaceOpacity,radius:root.galleryPresentation.radius,
                                surfaceColor:root.galleryPresentation.surfaceColor,focusColor:root.galleryPresentation.focusColor,
                                variant: root.galleryPresentation.showLabels === false ? "source" : "summary",
                                subtitle: root.galleryPresentation.showLabels ? root.stateLabel(entry.modelData) : ""
                            })
                        })
                        appearance: root.appearance
                        unit: 1
                        selected: root.open && root.focusArea === "grid" && root.selectedId === entry.modelData.id
                        onActivated: entry.activateEntry()
                    }
                }
            }
        }
    }
    Rectangle {
        visible:root.open && !!root.menuMode && !root.keyboardOpen
        anchors.fill:parent
        z:1
        color:"#90000000"
        MouseArea { anchors.fill:parent }
        Rectangle {
            anchors.centerIn:parent
            width:Math.min(parent.width-64,530)
            height:Math.min(parent.height-64,root.menuActions().length*44*theme.textScale+74)
            radius:theme.radius
            color:theme.surface
            Text {
                anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:16
                text:root.menuMode==="manage"?qsTr("Manage apps in this folder"):root.visibleEntries[root.selectedIndex]?.name || qsTr("Folder options")
                textFormat:Text.PlainText
                color:theme.text;font.family:theme.fontFamily;font.pixelSize:24*theme.textScale;elide:Text.ElideRight
            }
            ListView {
                id:menuRows
                objectName:"appFolderMenu"
                anchors.fill:parent;anchors.margins:16;anchors.topMargin:58
                clip:true;model:parent.parent.visible?root.menuActions():[]
                currentIndex:root.menuIndex
                highlightRangeMode:ListView.ApplyRange
                delegate:Rectangle {
                    required property var modelData
                    required property int index
                    width:ListView.view.width;height:44*theme.textScale
                    Accessible.role:Accessible.Button
                    Accessible.name:modelData.label
                    color:index===root.menuIndex?theme.selected:"transparent";radius:theme.radius
                    border.width:index===root.menuIndex?2:0;border.color:theme.selectedFocus
                    Text {
                        anchors.fill:parent;anchors.leftMargin:12
                        verticalAlignment:Text.AlignVCenter;text:modelData.label;textFormat:Text.PlainText
                        color:index===root.menuIndex?theme.selectedText:theme.text;font.family:theme.fontFamily;font.pixelSize:18*theme.textScale
                        elide:Text.ElideRight
                    }
                    MouseArea {anchors.fill:parent;onClicked:{root.menuIndex=index;root.chooseMenu(index)}}
                }
            }
        }
    }
    OnScreenKeyboard {
        id: keyboard
        objectName: "allAppsFilterKeyboard"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: parent.height * 0.62
        z: 2
        visible: root.open && root.keyboardOpen
        appearance: root.appearance
        value: root.filterText
        onValueChanged: if (root.keyboardOpen) root.filterText = value
        onAccepted: text => { root.filterText = text; root.keyboardOpen = false; root.focusArea = "grid" }
        onCanceled: { root.keyboardOpen = false; root.focusArea = "grid" }
    }
}
