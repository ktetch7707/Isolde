import QtQuick

Rectangle {
    id: root
    objectName:"libraryBrowseControls"
    property var provider:null
    property var appearance:({})
    property string pendingPlaylistName:""
    property string savedPlaylistFile:""
    readonly property var playlistCatalog:mode==="playlists" && provider && provider.playlistCatalog?provider.playlistCatalog():null
    Connections {target:kodiMedia;function onPlaylistSaved(filename){root.savedPlaylistFile=filename}}
    property string mode: ""
    property int selectedIndex: 0
    property var selectedCollection: null
    property var pendingItem: null
    readonly property bool active: mode.length > 0
    readonly property bool typing: ["search", "save", "savePlaylist", "tag", "genre", "rename", "newManual"].indexOf(mode) >= 0
    readonly property var rows: mode==="playlists"?["Save current view as playlist…","Refresh saved playlists"].concat(playlistCatalog?(playlistCatalog.items || []).map(i=>i.title):[]):mode==="playlistSaveState"?(savedPlaylistFile?["Return to library","Browse saved playlists"]:["Return to library","Retry name / save"]):mode === "collections" ? ["New Kodi tag view…", "New genre view…"].concat(kodiMedia.collections.map(c => c.name + " · " + c.media))
        : mode === "addSelected" ? addSelectedRows()
        : mode === "remove" ? ["Cancel", "Remove collection definition (keep Kodi media)"]
        : ["Open collection", "Rename", "Remove…",
            selectedCollection && selectedCollection.home ? "Hide shelf from Home" : "Show shelf on Home",
            "Move earlier", "Move later", "Shelf " + (selectedCollection && selectedCollection.media === "shows" ? "shows" : "movies")
                + ": " + (selectedCollection && selectedCollection.count ? selectedCollection.count : 6) + " · change", "Back"]
    visible: active; color: appearance.surfaceColor || "#10182B"; radius: 14
    function manualCollectionsFor(item) {
        const media = item && item.mediaType === "tvshow" ? "shows" : "movies"
        return kodiMedia.collections.filter(c => c.kind === "manual" && c.media === media)
    }
    function addSelectedRows() {
        let options = ["Create manual movie collection…"]
        if (pendingItem && pendingItem.mediaType === "tvshow") options[0] = "Create manual show collection…"
        if (!pendingItem) return options
        const movieId = pendingItem.movieid
        const itemId = pendingItem.mediaId || movieId || pendingItem.tvshowid
        const collections = manualCollectionsFor(pendingItem)
        for (let i = 0; i < collections.length; ++i) {
            const collection = collections[i]
            const contains = collection.members && collection.members.indexOf(itemId) >= 0
            options.push((contains ? "Remove from " : "Add to ") + collection.name)
        }
        return options
    }
    function begin(next, item) {
        mode = next; selectedIndex = 0
        if(next==="playlists")Qt.callLater(()=>{if(root.playlistCatalog && !root.playlistCatalog.busy)root.provider.refresh(root.playlistCatalog)})
        if (item) pendingItem = item
        keyboard.selectedIndex = 0
        keyboard.value = next==="savePlaylist"?pendingPlaylistName:next === "search" ? kodiMedia.viewValue : next === "rename" && selectedCollection ? selectedCollection.name : ""
    }
    function handleCommand(command) {
        if (typing) { keyboard.handleCommand(command); return }
        if(mode==="playlists" || mode==="playlistSaveState"){
            if(command===inputManager.back){mode="";return}
            if(command===inputManager.navigateUp)selectedIndex=Math.max(0,selectedIndex-1)
            else if(command===inputManager.navigateDown)selectedIndex=Math.min(rows.length-1,selectedIndex+1)
            else if(command===inputManager.confirm){
                if(mode==="playlistSaveState"){
                    if(selectedIndex===0)mode="";else if(!kodiMedia.busy)begin(savedPlaylistFile?"playlists":"savePlaylist")
                }else if(selectedIndex===0)begin("savePlaylist")
                else if(selectedIndex===1){if(playlistCatalog)provider.refresh(playlistCatalog)}
                else {const entry=playlistCatalog?playlistCatalog.items[selectedIndex-2]:null;if(entry && !kodiMedia.busy){kodiMedia.openView(kodiMedia.mediaKind,"playlist",entry.playlistName);mode=""}}
            }
            return
        }
        if (command === inputManager.back) { if (mode === "collections") mode = ""; else begin("collections"); return }
        if (command === inputManager.navigateUp) selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown) selectedIndex = Math.min(rows.length - 1, selectedIndex + 1)
        else if (command === inputManager.confirm) {
            if (mode === "collections") {
                if (selectedIndex < 2) begin(selectedIndex === 0 ? "tag" : "genre")
                else { selectedCollection = kodiMedia.collections[selectedIndex - 2]; begin("edit") }
            } else if (mode === "addSelected") {
                if (selectedIndex === 0) begin("newManual")
                else {
                    const collections = manualCollectionsFor(pendingItem)
                    const collection = collections[selectedIndex - 1]
                    const itemId = pendingItem.mediaId || pendingItem.movieid || pendingItem.tvshowid
                    if (collection.members && collection.members.indexOf(itemId) >= 0)
                        kodiMedia.removeManualCollectionMember(collection.id, itemId)
                    else kodiMedia.addManualCollectionMember(collection.id, pendingItem.mediaType, itemId)
                    begin("collections")
                }
            } else if (mode === "remove") {
                if (selectedIndex === 1) kodiMedia.removeCollection(selectedCollection.id)
                begin("collections")
            } else {
                if (selectedIndex === 0) { kodiMedia.openCollection(selectedCollection.id); mode = "" }
                else if (selectedIndex === 1) begin("rename")
                else if (selectedIndex === 2) begin("remove")
                else if (selectedIndex === 3) { kodiMedia.configureShelf(selectedCollection.id, !selectedCollection.home, selectedCollection.count || 6); begin("collections") }
                else if (selectedIndex === 4 || selectedIndex === 5) { kodiMedia.moveCollection(selectedCollection.id, selectedIndex === 4 ? -1 : 1); begin("collections") }
                else if (selectedIndex === 6) {
                    kodiMedia.configureShelf(selectedCollection.id, !!selectedCollection.home, (selectedCollection.count || 6) >= 12 ? 3 : (selectedCollection.count || 6) + 3)
                    begin("collections")
                }
                else begin("collections")
            }
        }
    }
    MouseArea { anchors.fill: parent }
    Text { id: heading; x: 30; y: 25; width: parent.width - 60; color: root.appearance.textColor || "#F8FAFC"; font.pixelSize: 27*(root.appearance.typographyScale || 1)
        text:root.mode==="playlists"?"Saved playlists · "+kodiMedia.mediaKind:root.mode==="savePlaylist"?"Name this Kodi playlist":root.mode==="playlistSaveState"?"Save playlist":root.mode === "search" ? "SEARCH LOCAL " + kodiMedia.mediaKind.toUpperCase() + " · WEB OFF"
            : root.mode === "tag" ? "KODI TAG · enter the exact tag name"
            : root.mode === "genre" ? "GENRE · enter the exact genre"
            : root.mode === "save" ? "NAME THIS COLLECTION"
            : root.mode === "newManual" ? (root.pendingItem && root.pendingItem.mediaType === "tvshow" ? "NAME THIS MANUAL SHOW COLLECTION" : "NAME THIS MANUAL MOVIE COLLECTION")
            : root.mode === "addSelected" ? "CURATE · " + (root.pendingItem ? root.pendingItem.title : "Selected movie")
            : root.mode === "rename" ? "RENAME COLLECTION" : "COLLECTIONS · " + (root.selectedCollection && root.mode !== "collections" ? root.selectedCollection.name : "This profile")
        wrapMode: Text.WordWrap
    }
    OnScreenKeyboard {
        id: keyboard; objectName:"libraryPlaylistKeyboard";appearance:root.appearance;x: 30; y: heading.y + heading.height + 20; width: parent.width - 60; height: 420; visible: root.typing
        onCanceled: root.mode = ""
        onAccepted: text => {
            if(root.mode==="savePlaylist"){
                if(!text.trim().length || text.trim().length>80)return
                root.pendingPlaylistName=text.trim();root.savedPlaylistFile="";kodiMedia.savePlaylist(text.trim());root.mode="playlistSaveState";root.selectedIndex=0;return
            }
            if (text.trim().length > (root.mode === "save" || root.mode === "rename" || root.mode === "newManual" ? 80 : 160)) return
            if (root.mode === "save") { kodiMedia.saveCollection(text); root.begin("collections") }
            else if (root.mode === "newManual") {
                kodiMedia.createManualCollection(text, root.pendingItem.mediaType, root.pendingItem.mediaId || root.pendingItem.movieid || root.pendingItem.tvshowid)
                root.begin("collections")
                root.selectedIndex = 1 + kodiMedia.collections.length
            }
            else if (root.mode === "rename") { kodiMedia.renameCollection(root.selectedCollection.id, text); root.begin("collections") }
            else { kodiMedia.setView(root.mode, text); root.mode = "" }
        }
    }
    ListView {
        id: choices; x: 30; y: heading.y + heading.height + 20; width: parent.width - 60; height: parent.height - y - 90
        visible: !root.typing; model: root.rows; clip: true; spacing: 8; currentIndex: root.selectedIndex
        highlightRangeMode: ListView.ApplyRange
        delegate: Rectangle {
            required property int index; required property string modelData
            width: choices.width; height: 58; radius: 8; color: root.selectedIndex === index ? root.appearance.selectedColor || "#2B416B" : root.appearance.surfaceColor || "#172238";border.width:root.selectedIndex===index?2:0;border.color:root.appearance.focusColor || "#C994A5"
            Text { anchors.fill: parent; anchors.margins: 14; color: root.appearance.textColor || "#F8FAFC"; font.pixelSize: 21*(root.appearance.typographyScale || 1); elide: Text.ElideRight; text: modelData }
            MouseArea { anchors.fill: parent; onClicked: { root.selectedIndex = index; root.handleCommand(inputManager.confirm) } }
        }
    }
    Text { x: 30; anchors.bottom: parent.bottom; anchors.bottomMargin: 20; width: parent.width - 60; wrapMode: Text.WordWrap; color: root.appearance.mutedColor || "#D5B083"; font.pixelSize: 17*(root.appearance.typographyScale || 1)
        text:root.mode==="playlistSaveState"?kodiMedia.playlistStatus:root.mode==="playlists"?(root.playlistCatalog?root.playlistCatalog.status:"Kodi unavailable · Back returns"):root.mode==="savePlaylist"?"Saves this library view as a playlist in Kodi. Back cancels without writing.":kodiMedia.collectionStatus || "Collections are reusable local rules. They do not move files or edit Kodi tags. Back cancels."
    }
}
