import QtQuick

Item {
    id: root
    objectName: "allAppsGallery"
    property bool open: false
    property var entries: []
    property var appearance: ({})
    property var galleryPresentation: ({layout:"grid",columns:5,density:"standard",sort:"name",showArtwork:true,showLabels:true,filterEnabled:true,favoritesEnabled:true,title:"All Apps"})
    property bool internetAvailable: false
    property bool localNetworkAvailable: false
    property string originId: ""
    property string selectedId: ""
    property int lastSelectedIndex: 0
    property string filterText: ""
    property string focusArea: "grid"
    property bool keyboardOpen: false
    property bool favoritesOnly: false
    property string feedback: ""
    property bool launchPending: false
    property string launchAttemptAppId: ""
    property int launchGeneration: 0
    readonly property var favoriteIds: galleryPresentation.favoriteIds || []
    readonly property int columns: galleryPresentation.columns || 5
    readonly property string layoutMode: galleryPresentation.layout || "grid"
    readonly property var visibleEntries: {
        const query = filterText.trim().toLowerCase()
        return (entries || []).filter(app => (!favoritesOnly || favoriteIds.includes(app.id)) && (!query
            || String(app.name || "").toLowerCase().includes(query)
            || String(app.section || "").toLowerCase().includes(query)))
            .slice().sort((a, b) => (galleryPresentation.sort === "favorites"
                ? (Number(favoriteIds.includes(b.id)) - Number(favoriteIds.includes(a.id))) : 0)
                || String(a.name || "").localeCompare(String(b.name || "")) || String(a.id).localeCompare(String(b.id)))
    }
    readonly property int selectedIndex: visibleEntries.findIndex(app => app.id === selectedId)
    SurfacePalette { id: theme; appearance: root.appearance }

    signal launchRequested(string appId)
    signal storeRequested()
    signal settingsRequested()
    signal closeRequested(string originId)
    signal searchRequested()
    signal favoriteChanged(string appId, bool favorite)

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
        if (!galleryPresentation.favoritesEnabled || !appId) return
        const next = favoriteIds.slice()
        const index = next.indexOf(appId)
        if (index >= 0) next.splice(index, 1)
        else next.push(appId)
        favoriteChanged(appId, index < 0)
    }

    function firstControlArea() {
        return galleryPresentation.filterEnabled !== false ? "filter"
            : galleryPresentation.favoritesEnabled ? "favorites" : "store"
    }

    function verticalStep() {
        return layoutMode === "list" ? 1 : columns
    }

    function handleCommand(command) {
        if (!open) return false
        if (keyboardOpen) {
            keyboard.handleCommand(command)
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
            if (index < 0 || index < step) focusArea = firstControlArea()
            else selectAt(index - step)
            return true
        }
        if (command === inputManager.navigateDown) {
            if (!visibleEntries.length) {
                focusArea = "store"
                return true
            }
            selectAt(Math.min(visibleEntries.length - 1, Math.max(0, selectedIndex) + verticalStep()))
            return true
        }
        if (command === inputManager.navigateLeft || command === inputManager.navigateRight) {
            if (!visibleEntries.length) return true
            selectAt(Math.max(0, Math.min(visibleEntries.length - 1,
                Math.max(0, selectedIndex) + (command === inputManager.navigateRight ? 1 : -1))))
            return true
        }
        if (command === inputManager.confirm) {
            const app = visibleEntries[selectedIndex]
            if (app) launchRequested(app.id)
            else focusArea = "store"
            return true
        }
        if (command === inputManager.menu) {
            if (galleryPresentation.favoritesEnabled) toggleFavorite(selectedId)
            else if (galleryPresentation.filterEnabled) {
                focusArea = "filter"
                keyboard.value = filterText
                keyboard.selectedIndex = 0
                keyboardOpen = true
            }
            return true
        }
        return true
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
        color: theme.background
        border.width: 1
        border.color: theme.outline
    }
    Column {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 20
        Row {
            width: parent.width
            height: 74
            spacing: 18
            Text {
                width: parent.width * 0.30
                anchors.verticalCenter: parent.verticalCenter
                text: root.galleryPresentation.title || "All Apps"
                color: theme.text
                font.family: theme.fontFamily
                font.pixelSize: 34 * theme.textScale
                font.weight: Font.DemiBold
            }
            Rectangle {
                width: parent.width * 0.25
                height: 56
                anchors.verticalCenter: parent.verticalCenter
                radius: theme.radius
                color: root.focusArea === "filter" ? theme.selected : theme.surface
                border.width: root.focusArea === "filter" ? 3 : 1
                border.color: root.focusArea === "filter" ? theme.focus : theme.outline
                visible: root.galleryPresentation.filterEnabled !== false
                Text {
                    anchors.fill: parent
                    anchors.margins: 14
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    text: root.filterText ? "Filter apps · " + root.filterText : "Filter apps"
                    color: theme.text
                    font.pixelSize: 19 * theme.textScale
                }
            }
            Rectangle {
                width: parent.width * 0.12
                height: 56
                anchors.verticalCenter: parent.verticalCenter
                radius: theme.radius
                visible: root.galleryPresentation.favoritesEnabled === true
                color: root.focusArea === "favorites" ? theme.selected : theme.surface
                border.width: root.focusArea === "favorites" ? 3 : 1
                border.color: root.focusArea === "favorites" ? theme.focus : theme.outline
                Text { anchors.centerIn: parent; text: "Favorites" + (root.favoritesOnly ? " · On" : ""); color: theme.text; font.pixelSize: 16 * theme.textScale }
            }
            Rectangle {
                width: parent.width * 0.12
                height: 56
                anchors.verticalCenter: parent.verticalCenter
                radius: theme.radius
                color: root.focusArea === "store" ? theme.selected : theme.surface
                border.width: root.focusArea === "store" ? 3 : 1
                border.color: root.focusArea === "store" ? theme.focus : theme.outline
                Text { anchors.centerIn: parent; text: "Store"; color: theme.text; font.pixelSize: 18 * theme.textScale }
            }
            Rectangle {
                width: parent.width * 0.12
                height: 56
                anchors.verticalCenter: parent.verticalCenter
                radius: theme.radius
                color: root.focusArea === "settings" ? theme.selected : theme.surface
                border.width: root.focusArea === "settings" ? 3 : 1
                border.color: root.focusArea === "settings" ? theme.focus : theme.outline
                Text { anchors.centerIn: parent; text: "Settings"; color: theme.text; font.pixelSize: 18 * theme.textScale }
            }
        }
        Text {
            width: parent.width
            text: root.visibleEntries.length
                ? root.feedback || ("Installed · " + root.visibleEntries.length + "    ·    OK open    Menu favorite    Back return")
                : root.favoritesOnly ? "No favorite apps match. Select an app and press Menu to mark it favorite."
                    : "No installed apps match. Open Store to find apps or Settings to manage this device."
            color: theme.muted
            font.pixelSize: 18 * theme.textScale
        }
        GridView {
            id: appGrid
            objectName: "allAppsGrid"
            width: parent.width
            height: parent.height - 120
            clip: true
            cellWidth: root.layoutMode === "list" ? width : width / root.columns
            cellHeight: root.layoutMode === "list" ? 82 : Math.min(
                height,
                210 * (root.galleryPresentation.density === "compact" ? 0.82 : root.galleryPresentation.density === "spacious" ? 1.15 : 1))
            model: root.visibleEntries
            currentIndex: root.selectedIndex
            highlightMoveDuration: 0
            delegate: HomeCard {
                required property var modelData
                width: appGrid.cellWidth - 12
                height: appGrid.cellHeight - 12
                item: ({
                    name: modelData.name,
                    showTitle: root.galleryPresentation.showLabels !== false,
                    unavailableReason: "",
                    logoSource: root.galleryPresentation.showArtwork ? modelData.logoSource || "" : "",
                    appSymbol: modelData.symbol || modelData.name.slice(0, 1),
                    presentation: ({
                        variant: root.galleryPresentation.showLabels === false ? "source" : "summary",
                        subtitle: root.galleryPresentation.showLabels ? root.stateLabel(modelData) : "",
                        showLogo: root.layoutMode !== "list"
                    })
                })
                appearance: root.appearance
                unit: 1
                selected: root.open && root.focusArea === "grid" && root.selectedId === modelData.id
                onActivated: {
                    root.focusArea = "grid"
                    root.selectedId = modelData.id
                    root.launchRequested(modelData.id)
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
        onAccepted: text => { root.filterText = text; root.keyboardOpen = false; root.focusArea = "filter" }
        onCanceled: { root.keyboardOpen = false; root.focusArea = "filter" }
    }
}
