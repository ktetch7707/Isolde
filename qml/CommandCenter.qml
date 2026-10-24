import QtQuick
import QtQuick.Controls

Item {
    id: root
    required property var apps
    required property var packages
    property var movieSearchClient: null
    property var showSearchClient: null
    property var episodeSearchClient: null
    property var mediaCollections: []
    property string pageId: ""
    property var pageSources: []
    property var searchSettings: null
    property var appearance: ({})
    SystemPalette {id:systemPalette}
    readonly property color ink:appearance.textColor || systemPalette.text
    readonly property color muted:appearance.mutedColor || systemPalette.mid
    readonly property color surface:appearance.surfaceColor || systemPalette.window
    readonly property color focusInk:appearance.focusColor || systemPalette.highlight
    readonly property color selectedSurface:appearance.selectedColor || systemPalette.highlight
    readonly property bool webSearchEnabled: searchSettings ? searchSettings.webEnabled : false
    property bool open: false
    property bool keyboardOpen: false
    property string query: ""
    property int selectedIndex: 0
    property string focusArea:"query"
    property string lastHeader:"query"
    signal launchApp(string appId)
    signal openSettings(int section)
    signal installPackage(string packageId)
    signal setupPackage(string packageId)
    signal mediaRequested(string mediaType, int mediaId, string query, string collectionId, int pageStart)
    signal pageSourceRequested(string pageId, string sourceId, string kind, string view, string value)
    signal closeRequested()

    readonly property var settingsTargets: [
        { kind: "setting", id: "settings-network", name: "Network", description: "Wi-Fi, saved networks, connection details", action: "Open Settings", section: 0, accent: "#4D7ED8", symbol: "N" },
        { kind: "setting", id: "settings-home", name: "Customize Home", description: "Tiles, categories, background, artwork, widgets", action: "Open Settings", section: 1, accent: "#9B6BE3", symbol: "H" },
        { kind: "setting", id: "settings-sources", name: "Sources", description: "Detected discs, drives, media folders, and source policy", action: "Open Settings", section: 2, accent: "#06B6D4", symbol: "▣" },
        { kind: "setting", id: "settings-remotes", name: "Remotes", description: "Physical remote setup, controllers, button tests, mappings", action: "Open Settings", section: 3, accent: "#22C55E", symbol: "R" },
        { kind: "setting", id: "settings-system", name: "System", description: "Display, audio, power, support, about", action: "Open Settings", section: 4, accent: "#D58A45", symbol: "S" }
    ]

    function contains(text, needle) {
        return String(text || "").toLowerCase().indexOf(needle) >= 0
    }

    function networkLabel(requirement) {
        if (requirement === "internet")
            return "Internet"
        if (requirement === "local-network")
            return "Local network"
        return "Local"
    }

    function packageStateLabel(state) {
        if (state === "needs-setup")
            return "Needs setup"
        if (state === "ready")
            return "Ready"
        if (state === "failed")
            return "Failed"
        return "Available"
    }

    function invalidateMediaSearch() {
        if (movieSearchClient) movieSearchClient.invalidate("Search closed.")
        if (showSearchClient) showSearchClient.invalidate("Search closed.")
        if (episodeSearchClient) episodeSearchClient.invalidate("Search closed.")
    }

    function updateMediaSearch() {
        const text = query.trim()
        if (!open || text.length < 2) { invalidateMediaSearch(); return }
        if (!movieSearchClient || !showSearchClient || !episodeSearchClient) return
        movieSearchClient.openView("movies", "search", text)
        showSearchClient.openView("shows", "search", text)
        episodeSearchClient.openView("episodes", "search", text)
    }

    function appendMediaResults(result, client, mediaType, term) {
        if (!client) return
        const label = mediaType === "movie" ? "Movies" : mediaType === "show" ? "Shows" : "Episodes"
        if (client.busy) {
            result.push({ kind: "mediaStatus", id: "searching-" + mediaType, name: "Searching " + label, description: "Querying this profile's local Kodi library", action: "Searching", accent: "#344864", symbol: "…" })
            return
        }
        if (!client.connected) {
            result.push({ kind: "mediaStatus", id: "offline-" + mediaType, name: label + " unavailable", description: client.status || "Open Kodi to reconnect the local library", action: "Unavailable", accent: "#6A4545", symbol: "!" })
            return
        }
        if (client.pageStart > 0)
            result.push({ kind: "mediaPage", id: "previous-" + mediaType, name: "Previous " + label, description: "Results " + (client.pageStart + 1) + "–" + (client.pageStart + client.items.length) + " of " + client.totalMovies, action: "Previous", mediaType: mediaType, accent: "#344864", symbol: "‹" })
        for (let i = 0; i < client.items.length; ++i) {
            const entry = client.items[i]
            const identity = mediaType === "movie" ? entry.movieid : mediaType === "show" ? entry.tvshowid : entry.episodeid
            if (!identity) continue
            let description = mediaType === "movie" ? "Movie" + (entry.year ? " · " + entry.year : "")
                : mediaType === "show" ? "TV show · " + (entry.episode || 0) + " episodes"
                : "Episode · " + (entry.showtitle || "TV show") + " · S" + String(entry.season || 0).padStart(2, "0") + " E" + String(entry.episode || 0).padStart(2, "0")
            result.push({ kind: "media", id: mediaType + "-" + identity, mediaType: mediaType, mediaId: identity,
                name: entry.title || "Untitled", description: description, action: mediaType === "show" ? "Browse" : "Play",
                pageStart: client.pageStart,
                accent: mediaType === "movie" ? "#5B8DEF" : mediaType === "show" ? "#9575CD" : "#468A74",
                symbol: mediaType === "movie" ? "M" : mediaType === "show" ? "TV" : "▶" })
        }
        if (client.hasNextPage)
            result.push({ kind: "mediaPage", id: "next-" + mediaType, name: "More " + label, description: "More matches in the local Kodi library", action: "Next", mediaType: mediaType, accent: "#344864", symbol: "›" })
    }

    function resultItems() {
        const q = query.trim().toLowerCase()
        let result = []
        for (let i = 0; i < apps.length; ++i) {
            const app = apps[i]
            const item = { kind: "app", id: app.id, name: app.name, description: app.section + " · " + app.presentationMode + " · " + networkLabel(app.networkRequirement), action: "Launch", accent: app.accent, logoSource: app.logoSource || "", symbol: app.symbol || app.name[0] || "A" }
            if (!q || contains(item.name, q) || contains(item.description, q))
                result.push(item)
        }
        for (let j = 0; j < packages.length; ++j) {
            const pack = packages[j]
            const state = pack.state || (pack.installed ? "ready" : "available")
            const prompt = state === "needs-setup" && pack.setupPrompt ? " · " + pack.setupPrompt : ""
            const failure = state === "failed" && pack.failureReason ? " · " + pack.failureReason : ""
            const pitem = {
                kind: "package",
                id: pack.id,
                name: pack.name,
                description: packageStateLabel(state) + " · " + pack.category + " · " + pack.presentationMode + " · " + networkLabel(pack.networkRequirement) + " · " + pack.description + prompt + failure,
                action: state === "needs-setup" ? "Set Up" : state === "failed" ? "Retry Setup" : pack.installed ? "Launch" : "Install",
                accent: pack.accent,
                symbol: pack.symbol || "+",
                installed: pack.installed,
                state: state
            }
            if (!q || contains(pitem.name, q) || contains(pitem.description, q) || contains(pack.publisher, q))
                result.push(pitem)
        }
        for (let k = 0; k < settingsTargets.length; ++k) {
            const setting = settingsTargets[k]
            if (!q || contains(setting.name, q) || contains(setting.description, q))
                result.push(setting)
        }
        if (q.length >= 2) {
            appendMediaResults(result, movieSearchClient, "movie", q)
            appendMediaResults(result, showSearchClient, "show", q)
            appendMediaResults(result, episodeSearchClient, "episode", q)
            for (let c = 0; c < mediaCollections.length; ++c) {
                const collection = mediaCollections[c]
                if (contains(collection.name, q)) result.push({ kind: "mediaCollection", id: "collection-" + collection.id,
                    collectionId: collection.id, name: collection.name,
                    description: (collection.media || "movies") + " · Saved local Kodi rule", action: "Browse collection",
                    accent: "#5B8DEF", symbol: "≡" })
            }
            for (let s = 0; s < pageSources.length; ++s) {
                const source = pageSources[s]
                const label = source.kind === "shows" ? "Shows" : "Movies"
                const viewLabel = source.view === "continue" ? "Continue watching"
                    : source.view === "recent" ? "Recently added"
                    : source.view === "search" ? "Search “" + source.value + "”"
                    : source.view === "genre" ? "Genre · " + source.value
                    : source.view === "tag" ? "Tag · " + source.value
                    : source.view === "collection" ? "Saved collection" : "All titles"
                if (contains(source.name, q) || contains(label, q) || contains(viewLabel, q))
                    result.push({ kind: "pageSource", id: "page-source-" + source.id, pageSourceId: source.id,
                        pageSourceKind: source.kind, pageSourceView: source.view, pageSourceValue: source.value,
                        name: source.name, description: label + " · " + viewLabel + " · Page source",
                        action: "Open on page", accent: "#468A74", symbol: "▤" })
            }
        }
        return result
    }

    readonly property var results: resultItems()

    onOpenChanged: {
        if (open) {
            selectedIndex = 0
            focusArea="query";lastHeader="query"
            keyboardOpen = false
            searchKeyboard.value = query
            searchKeyboard.selectedIndex = 0
            mediaSearchTimer.restart()
        } else {
            keyboardOpen = false
            mediaSearchTimer.stop()
            invalidateMediaSearch()
        }
    }
    onQueryChanged: if (open) mediaSearchTimer.restart()

    onResultsChanged: selectedIndex = Math.max(0, Math.min(selectedIndex, results.length - 1))

    function handleCommand(command) {
        if (!open)
            return
        if (keyboardOpen) {
            searchKeyboard.handleCommand(command)
            return
        }
        if (command === inputManager.back) {
            closeRequested()
            return
        }
        if(focusArea!=="results"){
            if(command===inputManager.navigateRight && searchSettings){focusArea="web";lastHeader="web"}
            else if(command===inputManager.navigateLeft){focusArea="query";lastHeader="query"}
            else if(command===inputManager.navigateDown)focusArea="results"
            else if(command===inputManager.confirm){
                if(focusArea==="web"){if(searchSettings)searchSettings.toggleWebEnabled()}
                else {keyboardOpen=true;searchKeyboard.value=query;searchKeyboard.selectedIndex=0}
            }
            return
        }
        if(command===inputManager.navigateUp){if(selectedIndex<=0)focusArea=lastHeader;else --selectedIndex}
        else if(command===inputManager.navigateDown)selectedIndex=Math.min(results.length-1,selectedIndex+1)
        else if(command===inputManager.navigateLeft){focusArea="query";lastHeader="query"}
        else if(command===inputManager.confirm){if(results.length)activateSelected();else focusArea="query"}
    }

    function activateSelected() {
        if (results.length === 0)
            return
        const item = results[selectedIndex]
        if (item.kind === "app") {
            closeRequested()
            launchApp(item.id)
        } else if (item.kind === "setting") {
            closeRequested()
            openSettings(item.section)
        } else if (item.kind === "package") {
            closeRequested()
            if (item.state === "needs-setup" || item.state === "failed")
                setupPackage(item.id)
            else if (item.installed)
                launchApp(item.id)
            else
                installPackage(item.id)
        } else if (item.kind === "media") {
            closeRequested()
            mediaRequested(item.mediaType, item.mediaId, query.trim(), "", item.pageStart)
        } else if (item.kind === "mediaCollection") {
            closeRequested()
            mediaRequested("collection", 0, query.trim(), item.collectionId, 0)
        } else if (item.kind === "pageSource") {
            closeRequested()
            pageSourceRequested(pageId, item.pageSourceId, item.pageSourceKind, item.pageSourceView, item.pageSourceValue)
        } else if (item.kind === "mediaPage") {
            const client = item.mediaType === "movie" ? movieSearchClient : item.mediaType === "show" ? showSearchClient : episodeSearchClient
            if (item.action === "Next") client.nextPage()
            else client.previousPage()
        }
    }

    Timer { id: mediaSearchTimer; interval: 250; repeat: false; onTriggered: root.updateMediaSearch() }

    visible: open
    z: 50

    Rectangle {
        anchors.fill: parent
        color: "#DD050812"
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 90, 980)
        height: Math.min(parent.height - 70, 610)
        radius: 18
        color: root.surface
        border.width: 2
        border.color: root.focusInk

        Column {
            anchors.fill: parent
            anchors.margins: 28
            spacing: 18

            Item {
                id:searchHeader
                width: parent.width
                height:56
                Text {anchors.left:parent.left;anchors.verticalCenter:parent.verticalCenter; text: "Search"; color: root.ink; font.pixelSize: 34*(root.appearance.typographyScale || 1); font.weight: Font.DemiBold}
                Switch {
                    id:webSwitch;objectName:"searchWebToggle";anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter
                    visible:!!root.searchSettings;checked:root.webSearchEnabled
                    text:"Web search · "+(checked?"On":"Off")
                    Accessible.name:"Web search";Accessible.description:checked?"On. No web provider configured; local results only.":"Off. Local results only."
                    onClicked:{root.focusArea="web";root.lastHeader="web";root.searchSettings.toggleWebEnabled();checked=Qt.binding(()=>root.webSearchEnabled)}
                    contentItem:Text {text:webSwitch.text;color:root.ink;font.pixelSize:18*(root.appearance.typographyScale || 1);verticalAlignment:Text.AlignVCenter;rightPadding:webSwitch.indicator.width+12}
                    indicator:Rectangle {x:webSwitch.width-width;y:(webSwitch.height-height)/2;width:54;height:30;radius:height/2;color:webSwitch.checked?root.focusInk:root.muted
                        Rectangle {width:24;height:24;radius:12;x:webSwitch.checked?parent.width-width-3:3;y:3;color:root.surface}
                    }
                    background:Rectangle {color:"transparent";radius:8;border.width:root.focusArea==="web"?2:0;border.color:root.focusInk}
                }
            }

            Rectangle {
                id:queryControl;objectName:"searchQueryControl"
                width: parent.width
                height: 58
                radius: 999
                color: root.surface
                border.width: root.keyboardOpen || root.focusArea==="query" ? 3 : 1
                border.color: root.focusArea==="query" ? root.focusInk : root.muted
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 24
                    text: root.query.length ? root.query : "Search your library, apps and settings"
                    color: root.query.length ? root.ink : root.muted
                    font.pixelSize: 22
                }
                MouseArea {anchors.fill:parent;onClicked:{root.focusArea="query";root.lastHeader="query";root.keyboardOpen=true;searchKeyboard.value=root.query;searchKeyboard.selectedIndex=0}}
            }

            Text {
                id:resultCount
                text: root.results.length + " result" + (root.results.length === 1 ? "" : "s")
                color: root.muted
                font.pixelSize: 15
                font.weight: Font.DemiBold
                font.letterSpacing: 3
            }
            Text {id:searchStatus;objectName:"searchWebStatus";width:parent.width;wrapMode:Text.WordWrap;text:root.searchSettings && root.searchSettings.errorString?root.searchSettings.errorString:root.webSearchEnabled?"No web provider configured — local results only":root.query.trim().length===1?"Type at least 2 characters to search your Kodi library":"Local results only";color:root.muted;font.pixelSize:14}

            Flickable {
                id: resultScroller
                width: parent.width
                height:Math.max(60,parent.height-searchHeader.height-queryControl.height-resultCount.height-searchStatus.height-searchFooter.height-5*parent.spacing)
                contentHeight: resultColumn.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentY: Math.max(0, Math.min(Math.max(0, contentHeight - height), root.selectedIndex * 82 - 110))

                Column {
                    id: resultColumn
                    width: parent.width
                    spacing: 10
                    Repeater {
                        model: root.results
                        delegate: Rectangle {
                            required property int index
                            required property var modelData
                            width: resultColumn.width
                            height: 76
                            radius: 12
                            color: root.selectedIndex === index && root.focusArea==="results" ? root.selectedSurface : root.surface
                            border.width: root.selectedIndex === index && root.focusArea==="results" ? 3 : 1
                            border.color: root.selectedIndex===index && root.focusArea==="results" ? root.focusInk : root.muted
                            Rectangle {
                                width: 48; height: 48; radius: 12
                                anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter
                                color: modelData.logoSource ? "transparent" : modelData.accent
                                Image { anchors.fill:parent; source:modelData.logoSource || ""; fillMode:Image.PreserveAspectFit; visible:!!modelData.logoSource }
                                Text { anchors.centerIn: parent; visible:!modelData.logoSource; text: modelData.symbol; color: root.ink; font.pixelSize: 20; font.weight: Font.DemiBold }
                            }
                            Column {
                                anchors.left: parent.left; anchors.leftMargin: 76
                                anchors.right: actionLabel.left; anchors.rightMargin: 18
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 3
                                Text { text: modelData.name; color: root.ink; font.pixelSize: 21; font.weight: Font.DemiBold; elide: Text.ElideRight; width: parent.width }
                                Text { text: modelData.description; color: root.muted; font.pixelSize: 14; elide: Text.ElideRight; width: parent.width }
                            }
                            Text {
                                id: actionLabel
                                anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: parent.verticalCenter
                                text: modelData.action
                                color: root.focusInk
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                    Text {
                        visible: root.results.length === 0
                        text: "No matches. Select the search field to edit."
                        color: root.muted
                        font.pixelSize: 22
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        topPadding: 70
                    }
                }
            }

            Text {
                id:searchFooter
                text: root.keyboardOpen ? "D-pad type · Confirm selects · Back closes keyboard" : "D-pad navigate · Confirm selects · Back closes Search"
                color: root.muted
                font.pixelSize: 15
            }
        }
    }

    OnScreenKeyboard {
        id: searchKeyboard
        visible: root.keyboardOpen
        z: 60
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 34
        height: Math.min(400, parent.height - 68)
        passwordMode: false
        onAccepted: text => {
            root.query = text
            root.keyboardOpen = false
            root.selectedIndex = 0
            root.focusArea="results"
        }
        onCanceled: root.keyboardOpen = false
    }
}
