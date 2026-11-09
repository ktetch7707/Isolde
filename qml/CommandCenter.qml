import QtQuick
import QtQuick.Controls

Item {
    id: root
    required property var apps
    required property var packages
    property var movieSearchClient: null
    property var showSearchClient: null
    property var episodeSearchClient: null
    property var onlineDiscovery: null
    property var mediaCollections: []
    property string pageId: ""
    property var pageSources: []
    property var searchSettings: null
    property var shellActions: []
    property var appearance: ({})
    SurfacePalette {id:shell;appearance:root.appearance}
    readonly property color ink:shell.text
    readonly property color muted:shell.muted
    readonly property color surface:shell.surface
    readonly property color focusInk:shell.focus
    readonly property color searchFocusInk:shell.focus
    readonly property color selectedSurface:shell.selected
    readonly property bool webSearchEnabled: searchSettings ? searchSettings.webEnabled : false
    property bool open: false
    property bool keyboardOpen: false
    property bool regionKeyboardOpen: false
    property string query: ""
    property int selectedIndex: 0
    property string focusArea:"query"
    property string lastHeader:"query"
    readonly property var discoveryRegions:["US","CA","GB","AU","NZ","IN","JP","KR","FR","DE","ES","BR"]
    signal launchApp(string appId)
    signal openSettings(int section)
    signal installPackage(string packageId)
    signal setupPackage(string packageId)
    signal mediaRequested(string mediaType, int mediaId, string query, string collectionId, int pageStart)
    signal pageSourceRequested(string pageId, string sourceId, string kind, string view, string value)
    signal closeRequested()
    signal shellActionRequested(var action)
    signal linguistRequested()

    readonly property var settingsTargets: [
        { kind: "setting", id: "settings-network", name: "Network", description: "Wi-Fi, saved networks, connection details", action: "Open Settings", section: 0, accent: shell.accent, symbol: "N" },
        { kind: "setting", id: "settings-home", name: "Customize Home", description: "Tiles, categories, background, artwork, widgets", action: "Open Settings", section: 1, accent: shell.accent, symbol: "H" },
        { kind: "setting", id: "settings-sources", name: "Sources", description: "Detected discs, drives, media folders, and source policy", action: "Open Settings", section: 2, accent: shell.accent, symbol: "▣" },
        { kind: "setting", id: "settings-remotes", name: "Remotes", description: "Physical remote setup, controllers, button tests, mappings", action: "Open Settings", section: 3, accent: shell.accent, symbol: "R" },
        { kind: "setting", id: "settings-system", name: "System", description: "Display, audio, power, support, about", action: "Open Settings", section: 4, accent: shell.accent, symbol: "S" }
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
        if (!open || text.length < 2) {
            invalidateMediaSearch()
            if (onlineDiscovery) onlineDiscovery.cancel()
            return
        }
        if (!webSearchEnabled) {
            // T28.4: Network denial when web search is switched OFF —
            // cancel any provider-mediated search and remain on local results only.
            invalidateMediaSearch()
            if (onlineDiscovery) onlineDiscovery.cancel()
            return
        }
        if (onlineDiscovery) onlineDiscovery.search(text)
        if (!movieSearchClient || !showSearchClient || !episodeSearchClient) return
        movieSearchClient.openView("movies", "search", text)
        showSearchClient.openView("shows", "search", text)
        episodeSearchClient.openView("episodes", "search", text)
    }

    function appendMediaResults(result, client, mediaType, term) {
        if (!client) return
        const label = mediaType === "movie" ? "Movies" : mediaType === "show" ? "Shows" : "Episodes"
        if (client.busy) {
            result.push({ kind: "mediaStatus", id: "searching-" + mediaType, name: "Searching " + label, description: "Querying this profile's local Kodi library", action: "Searching", accent: shell.unavailable, symbol: "…" })
            return
        }
        if (!client.connected) {
            result.push({ kind: "mediaStatus", id: "offline-" + mediaType, name: label + " unavailable", description: client.status || "Open Kodi to reconnect the local library", action: "Unavailable", accent: shell.error, symbol: "!" })
            return
        }
        if (client.pageStart > 0)
            result.push({ kind: "mediaPage", id: "previous-" + mediaType, name: "Previous " + label, description: "Results " + (client.pageStart + 1) + "–" + (client.pageStart + client.items.length) + " of " + client.totalMovies, action: "Previous", mediaType: mediaType, accent: shell.unavailable, symbol: "‹" })
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
            result.push({ kind: "mediaPage", id: "next-" + mediaType, name: "More " + label, description: "More matches in the local Kodi library", action: "Next", mediaType: mediaType, accent: shell.unavailable, symbol: "›" })
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
        for(let a=0;a<shellActions.length;++a){
            const entry=shellActions[a]
            if(!q || contains(entry.name,q))result.push({kind:"shellAction",id:"shell-action-"+a,name:entry.name,description:"Isolde action",action:"Open",targetAction:entry.action,accent:root.focusInk,symbol:"↗"})
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
                    accent: shell.accent, symbol: "≡" })
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
                        action: "Open on page", accent: shell.accent, symbol: "▤" })
            }
        }
        if (webSearchEnabled && onlineDiscovery) {
            const onlineItems = onlineDiscovery.results || []
            for (let o = 0; o < onlineItems.length; ++o) {
                const title = onlineItems[o]
                result.push({ kind: "onlineTitle", id: title.identity, providerId: title.providerId,
                    mediaType: title.mediaType, name: title.name + (title.year ? " (" + title.year + ")" : ""),
                    description: "TMDB metadata · " + title.region + " · checked " + title.checkedAt + " · not endorsed by TMDB · availability is not entitlement",
                    action: "Check listings", accent: shell.accent, symbol: title.symbol })
            }
            const providers = onlineDiscovery.availability || []
            for (let p = 0; p < providers.length; ++p) {
                const listing = providers[p]
                result.push({ kind: "onlineProvider", id: "provider-" + p, name: listing.name,
                    description: "TMDB listing · " + listing.tier + " · " + listing.region + " · verify with service · no entitlement claim",
                    action: "Provider listing", accent: shell.accent, symbol: "↗" })
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
            regionKeyboardOpen = false
            mediaSearchTimer.stop()
            invalidateMediaSearch()
            if (onlineDiscovery) onlineDiscovery.cancel()
        }
    }
    onQueryChanged: if (open) mediaSearchTimer.restart()
    onWebSearchEnabledChanged: {
        if (!webSearchEnabled && onlineDiscovery) onlineDiscovery.cancel()
        else if (open) mediaSearchTimer.restart()
    }

    onResultsChanged: selectedIndex = Math.max(0, Math.min(selectedIndex, results.length - 1))

    function handleCommand(command) {
        if (!open)
            return
        if (regionKeyboardOpen) {
            regionKeyboard.handleCommand(command)
            return
        }
        if (keyboardOpen) {
            searchKeyboard.handleCommand(command)
            return
        }
        if (command === inputManager.back) {
            closeRequested()
            return
        }
        if(focusArea!=="results"){
            if(focusArea==="region" && (command===inputManager.navigateRight || command===inputManager.navigateLeft) && searchSettings){
                const current=discoveryRegions.indexOf(searchSettings.discoveryRegion)
                const offset=command===inputManager.navigateRight?1:-1
                const next=Math.max(0,Math.min(discoveryRegions.length-1,(current<0?0:current)+offset))
                searchSettings.discoveryRegion=discoveryRegions[next];lastHeader="region";updateMediaSearch()
            }
            else if(command===inputManager.navigateRight && focusArea==="query"){focusArea="linguist";lastHeader="linguist"}
            else if(command===inputManager.navigateRight && focusArea==="linguist" && searchSettings){focusArea="region";lastHeader="region"}
            else if(command===inputManager.navigateRight && focusArea==="region" && searchSettings){focusArea="web";lastHeader="web"}
            else if(command===inputManager.navigateLeft && focusArea==="web"){focusArea="region";lastHeader="region"}
            else if(command===inputManager.navigateLeft && focusArea==="region"){focusArea="linguist";lastHeader="linguist"}
            else if(command===inputManager.navigateLeft){focusArea="query";lastHeader="query"}
            else if(command===inputManager.navigateDown)focusArea="results"
            else if(command===inputManager.confirm){
                if(focusArea==="web"){if(searchSettings)searchSettings.toggleWebEnabled()}
                else if(focusArea==="linguist"){linguistRequested()}
                else if(focusArea==="region" && searchSettings){regionKeyboard.value=searchSettings.discoveryRegion;regionKeyboard.mode="letters";regionKeyboard.selectedIndex=0;regionKeyboardOpen=true}
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
        } else if (item.kind === "shellAction") {
            closeRequested()
            shellActionRequested(item.targetAction)
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
        } else if (item.kind === "onlineTitle" && onlineDiscovery) {
            onlineDiscovery.loadAvailability(item.mediaType, item.providerId)
        }
    }

    Timer { id: mediaSearchTimer; interval: 250; repeat: false; onTriggered: root.updateMediaSearch() }

    visible: open
    z: 50

    Rectangle {
        anchors.fill: parent
        color: shell.scrim
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 90, 980)
        height: Math.min(parent.height - 70, 610)
        radius: 18
        color: root.surface
        border.width: 2
        border.color: root.searchFocusInk

        Column {
            anchors.fill: parent
            anchors.margins: 28
            spacing: 18

            Item {
                id:searchHeader
                width: parent.width
                height:56
                Text {font.family:shell.fontFamily;anchors.left:parent.left;anchors.verticalCenter:parent.verticalCenter; text: "Search"; color: root.ink; font.pixelSize: 34*(root.appearance.typographyScale || 1); font.weight: Font.DemiBold}
                Button {
                    id:linguistButton;objectName:"searchLinguistOpen"
                    anchors.right:regionChoice.left;anchors.rightMargin:10;anchors.verticalCenter:parent.verticalCenter
                    text:"Linguist";Accessible.name:"Open Isolde Linguist subtitle queue"
                    background:Rectangle {color:"transparent";radius:8;border.width:root.focusArea==="linguist"?2:0;border.color:root.searchFocusInk}
                    contentItem:Text {font.family:shell.fontFamily;text:linguistButton.text;color:root.ink;font.pixelSize:18*(root.appearance.typographyScale || 1);verticalAlignment:Text.AlignVCenter}
                    onClicked:{root.focusArea="linguist";root.lastHeader="linguist";root.linguistRequested()}
                }
                Switch {
                    id:webSwitch;objectName:"searchWebToggle";anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter
                    visible:!!root.searchSettings;checked:root.webSearchEnabled
                    text:"Web search · "+(checked?"On":"Off")
                    Accessible.name:"Web search";Accessible.description:checked?"On. No web provider configured; local results only.":"Off. Local results only."
                    onClicked:{root.focusArea="web";root.lastHeader="web";root.searchSettings.toggleWebEnabled();checked=Qt.binding(()=>root.webSearchEnabled)}
                    contentItem:Text {font.family:shell.fontFamily;text:webSwitch.text;color:root.ink;font.pixelSize:18*(root.appearance.typographyScale || 1);verticalAlignment:Text.AlignVCenter;rightPadding:webSwitch.indicator.width+12}
                    indicator:Rectangle {x:webSwitch.width-width;y:(webSwitch.height-height)/2;width:54;height:30;radius:height/2;color:webSwitch.checked?root.searchFocusInk:root.muted
                        Rectangle {width:24;height:24;radius:12;x:webSwitch.checked?parent.width-width-3:3;y:3;color:root.surface}
                    }
                    background:Rectangle {color:"transparent";radius:8;border.width:root.focusArea==="web"?2:0;border.color:root.searchFocusInk}
                }
                ComboBox {
                    id:regionChoice;objectName:"searchDiscoveryRegion"
                    anchors.right:webSwitch.left;anchors.rightMargin:8;anchors.verticalCenter:parent.verticalCenter
                    width:112;height:42;editable:true
                    model:root.discoveryRegions
                    currentIndex:find(root.searchSettings?root.searchSettings.discoveryRegion:"US")
                    onAccepted:if(root.searchSettings){root.searchSettings.discoveryRegion=editText.toUpperCase();root.updateMediaSearch()}
                    onActivated:index=>{if(root.searchSettings){root.searchSettings.discoveryRegion=currentText;root.updateMediaSearch()}}
                    Accessible.name:"Discovery region, ISO 3166-1 alpha-2"
                    background:Rectangle {color:"transparent";radius:8;border.width:root.focusArea==="region"?2:0;border.color:root.searchFocusInk}
                    Component.onCompleted:if(root.searchSettings)editText=root.searchSettings.discoveryRegion
                    Connections {target:root.searchSettings;function onChanged(){if(root.searchSettings && regionChoice.editText!==root.searchSettings.discoveryRegion)regionChoice.editText=root.searchSettings.discoveryRegion}}
                }
            }

            Rectangle {
                id:queryControl;objectName:"searchQueryControl"
                width: parent.width
                height: 58
                radius: 999
                color: shell.input
                border.width: root.keyboardOpen || root.focusArea==="query" ? 3 : 1
                border.color: shell.inputFocus
                Text {font.family:shell.fontFamily;
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 24
                    text: root.query.length ? root.query : "Search your library, apps and settings"
                    color: root.query.length ? shell.inputText : shell.inputMuted
                    font.pixelSize: 22*shell.textScale
                }
                MouseArea {anchors.fill:parent;onClicked:{root.focusArea="query";root.lastHeader="query";root.keyboardOpen=true;searchKeyboard.value=root.query;searchKeyboard.selectedIndex=0}}
            }

            Text {font.family:shell.fontFamily;
                id:resultCount
                text: root.results.length + " result" + (root.results.length === 1 ? "" : "s")
                color: root.muted
                font.pixelSize: 15*shell.textScale
                font.weight: Font.DemiBold
                font.letterSpacing: 3
            }
            Text {font.family:shell.fontFamily;id:searchStatus;objectName:"searchWebStatus";width:parent.width;wrapMode:Text.WordWrap;text:root.searchSettings && root.searchSettings.errorString?root.searchSettings.errorString:root.webSearchEnabled && root.onlineDiscovery?root.onlineDiscovery.status:root.webSearchEnabled?"No web provider configured — local results only":root.query.trim().length===1?"Type at least 2 characters to search your Kodi library":"Local results only";color:root.muted;font.pixelSize:14}

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
                            border.color: root.selectedIndex===index && root.focusArea==="results" ? shell.selectedFocus : shell.outline
                            Rectangle {
                                objectName: "searchResultIcon-"+index
                                width: 48; height: 48; radius: 12; clip: true
                                anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter
                                color: modelData.logoSource ? "transparent" : shell.elevated
                                Image { anchors.fill:parent; source:modelData.logoSource || ""; fillMode:Image.PreserveAspectFit; visible:!!modelData.logoSource }
                                Text {font.family:shell.fontFamily;
                                    objectName: "searchResultSymbol-"+index
                                    anchors.fill: parent; anchors.margins: 4
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                    visible:!modelData.logoSource; text: modelData.symbol; textFormat: Text.PlainText
                                    elide: Text.ElideRight; maximumLineCount: 1
                                    color: shell.elevatedText; font.pixelSize: 20*shell.textScale; font.weight: Font.DemiBold
                                }
                            }
                            Column {
                                anchors.left: parent.left; anchors.leftMargin: 76
                                anchors.right: actionLabel.left; anchors.rightMargin: 18
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 3
                                Text {font.family:shell.fontFamily; objectName:"searchResultTitle-"+index; text: modelData.name; textFormat:Text.PlainText; color: root.selectedIndex === index && root.focusArea==="results" ? shell.selectedText : root.ink; font.pixelSize: 21*shell.textScale; font.weight: Font.DemiBold; elide: Text.ElideRight; width: parent.width }
                                Text {font.family:shell.fontFamily; text: modelData.description; textFormat:Text.PlainText; color: root.selectedIndex === index && root.focusArea==="results" ? shell.selectedText : root.muted; font.pixelSize: 14*shell.textScale; elide: Text.ElideRight; width: parent.width }
                            }
                            Text {font.family:shell.fontFamily;
                                id: actionLabel
                                objectName: "searchActionLabel"
                                anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: parent.verticalCenter
                                text: modelData.action
                                color: root.selectedIndex === index && root.focusArea==="results" ? shell.selectedText : shell.text
                                font.pixelSize: 15*shell.textScale
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                    Text {font.family:shell.fontFamily;
                        visible: root.results.length === 0
                        text: "No matches. Select the search field to edit."
                        color: root.muted
                        font.pixelSize: 22*shell.textScale
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        topPadding: 70
                    }
                }
            }

            Text {font.family:shell.fontFamily;
                id:searchFooter
                text: root.keyboardOpen ? "D-pad type · Confirm selects · Back closes keyboard" : "D-pad navigate · Confirm selects · Back closes Search"
                color: root.muted
                font.pixelSize: 15*shell.textScale
            }
        }
    }

    OnScreenKeyboard {
        appearance: root.appearance
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
    OnScreenKeyboard {
        id: regionKeyboard
        objectName: "searchDiscoveryRegionKeyboard"
        appearance: root.appearance
        visible: root.regionKeyboardOpen
        z: 61
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 34
        height: Math.min(400, parent.height - 68)
        onAccepted: text => {
            if (root.searchSettings) root.searchSettings.discoveryRegion = text.toUpperCase()
            root.regionKeyboardOpen = false
            root.focusArea = "region"
            root.lastHeader = "region"
            root.updateMediaSearch()
        }
        onCanceled: root.regionKeyboardOpen = false
    }
}
