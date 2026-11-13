import QtQuick

Rectangle {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    objectName:"libraryBrowseControls"
    property var provider:null
    property var appearance:({})
    property string pendingPlaylistName:""
    property string savedPlaylistFile:""
    readonly property var playlistCatalog:(mode==="playlists" || playlistManageMode.length>0) && provider && provider.playlistCatalog?provider.playlistCatalog():null
    property bool refreshPlaylistPending: false
    property bool loadingMembers: false
    property bool loadingRulePreview: false
    property bool rulePreviewAccepted: false
    property bool confirmRule: false
    property var memberDraft: []
    property var memberBaseline: []
    property int movingMember: -1
    property int ruleIndex: 0
    property string ruleValue: ""
    property int sortIndex: 0
    property bool sortDescending: false
    readonly property var ruleKeys: ["all","search","tag","genre","recent","continue","watched"]
    readonly property var ruleLabels: ["All items","Title contains","Tag equals","Genre equals","Recently added","In progress","Previously played"]
    readonly property var sortKeys: ["title","year","dateadded","lastplayed","episode"]
    readonly property var sortLabels: ["Title","Year","Date added","Last played","Episode"]
    function playlistOperationRows() {
        if(playlistManageMode==="editRule")return ["Rule · "+ruleLabels[ruleIndex],"Value · "+(ruleValue || "not set"),"Preview matching items",confirmRule?"CONFIRM replace existing rules":rulePreviewAccepted?"Save rule · replaces existing rules":"Save unavailable · preview this draft first","Cancel · keep existing rules"]
        if(playlistManageMode==="previewRule")return ["Back to rule draft"].concat((kodiMedia.playlistPreview || []).map(row=>row.title))
        if(playlistManageMode==="sortPlaylist")return sortLabels.map((label,i)=>(sortIndex===i?"● ":"")+label).concat(["Direction · "+(sortDescending?"descending":"ascending"),"Save order","Cancel"])
        return ["Save complete order","Cancel · keep saved order"].concat(memberDraft.map((row,i)=>(movingMember===i?"Moving · ":"")+(i+1)+" · "+row.title))
    }
    function finishPlaylist() { mode="playlists";playlistManageMode="";pendingPlaylistIndex=-1;selectedIndex=0;loadingMembers=false;loadingRulePreview=false;movingMember=-1 }
    function playlistHelp() {
        if(playlistManageMode==="editRule" || playlistManageMode==="previewRule")return "Draft only. Preview reads up to 50 Kodi matches. Confirming Save replaces all existing rules; Back keeps the saved playlist."
        if(playlistManageMode==="reorderMembers")return movingMember>=0?"Up/Down moves the selected item in the draft. Confirm accepts this move; Back restores the order before this move.":"Confirm an item to move it. Save applies the complete order; Back writes nothing."
        if(playlistManageMode==="sortPlaylist")return "Choose sort field and direction, then Save. Back keeps the existing order. Episode order is available for episode playlists."
        if(playlistManageMode==="renamePlaylist")return "Enter a new filename with the same extension. Collisions keep both saved playlists. Back cancels."
        if(playlistManageMode==="deletePlaylist")return "Confirm removes only this playlist definition; media files stay unchanged."
        return "Saved playlists in this Kodi profile · Open, rename, reorder, replace rules, or delete."
    }
    Connections {
        target:kodiMedia
        function onPlaylistSaved(filename){root.savedPlaylistFile=filename}
        function onChanged(){
            if(!kodiMedia.busy && root.loadingMembers){root.loadingMembers=false;root.memberDraft=(kodiMedia.playlistMembers || []).slice();root.memberBaseline=root.memberDraft.slice()}
            if(!kodiMedia.busy && root.loadingRulePreview){root.loadingRulePreview=false;root.rulePreviewAccepted=kodiMedia.playlistPreviewReady}
            if(root.refreshPlaylistPending && !kodiMedia.busy){
                root.refreshPlaylistPending=false
                if(root.playlistCatalog)root.provider.refresh(root.playlistCatalog)
            }
        }
    }
    property string mode: ""
    property int selectedIndex: 0
    property var selectedCollection: null
    property var pendingItem: null
    property string playlistManageMode: ""
    property int pendingPlaylistIndex: -1
    readonly property bool active: mode.length > 0 || playlistManageMode.length > 0
    readonly property bool typing: ["search", "save", "savePlaylist", "tag", "genre", "rename", "newManual", "renamePlaylist", "ruleValue"].indexOf(mode) >= 0
    readonly property var playlistItem: pendingPlaylistIndex >= 0 && playlistCatalog && pendingPlaylistIndex < playlistCatalog.items.length ? playlistCatalog.items[pendingPlaylistIndex] : null
    readonly property var rows: ["editRule","previewRule","sortPlaylist","reorderMembers"].indexOf(playlistManageMode)>=0?playlistOperationRows():playlistManageMode==="manage"?(playlistItem?["Open playlist","Rename…","Reorder…","Delete…"].concat(playlistItem.playlistName.toLowerCase().endsWith(".xsp")?["Replace rules…","Back"]:["Back"]):["Back"]):playlistManageMode==="renamePlaylist"?["Enter new name","Back"]:playlistManageMode==="deletePlaylist"?["Confirm delete","Cancel"]:mode==="playlists"?["Save current view as playlist…","Refresh saved playlists"].concat(playlistCatalog?(playlistCatalog.items || []).map(i=>i.title+" · manage"):[]):mode==="playlistSaveState"?(savedPlaylistFile?["Return to library","Browse saved playlists"]:["Return to library","Retry name / save"]):mode === "collections" ? ["New Kodi tag view…", "New genre view…"].concat(kodiMedia.collections.map(c => c.name + " · " + c.media))
        : mode === "addSelected" ? addSelectedRows()
        : mode === "remove" ? ["Cancel", "Remove collection definition (keep Kodi media)"]
        : ["Open collection", "Rename", "Remove…",
            selectedCollection && selectedCollection.home ? "Hide shelf from Home" : "Show shelf on Home",
            "Move earlier", "Move later", "Shelf " + (selectedCollection && selectedCollection.media === "shows" ? "shows" : "movies")
                + ": " + (selectedCollection && selectedCollection.count ? selectedCollection.count : 6) + " · change", "Back"]
    visible: active; color: shell.surface; radius: shell.radius
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
        if(["editRule","previewRule","sortPlaylist","reorderMembers"].indexOf(playlistManageMode)>=0){
            if(command===inputManager.back){
                if(movingMember>=0){memberDraft=memberBaseline.slice();movingMember=-1;selectedIndex=2;return}
                if(playlistManageMode==="previewRule"){playlistManageMode="editRule";selectedIndex=3;loadingRulePreview=false;return}
                playlistManageMode="manage";selectedIndex=0;loadingMembers=false;confirmRule=false;return
            }
            if(kodiMedia.busy)return
            const delta=command===inputManager.navigateUp?-1:command===inputManager.navigateDown?1:0
            if(delta){
                if(playlistManageMode==="reorderMembers" && movingMember>=0){
                    const target=movingMember+delta
                    if(target>=0 && target<memberDraft.length){const draft=memberDraft.slice();const row=draft.splice(movingMember,1)[0];draft.splice(target,0,row);memberDraft=draft;movingMember=target;selectedIndex=target+2}
                }else selectedIndex=Math.max(0,Math.min(rows.length-1,selectedIndex+delta))
                return
            }
            if(command!==inputManager.confirm)return
            if(playlistManageMode==="previewRule"){if(selectedIndex===0){playlistManageMode="editRule";selectedIndex=3}return}
            if(playlistManageMode==="sortPlaylist"){
                if(selectedIndex<5)sortIndex=selectedIndex
                else if(selectedIndex===5)sortDescending=!sortDescending
                else if(selectedIndex===6 && playlistItem){refreshPlaylistPending=true;kodiMedia.sortPlaylist(playlistItem.playlistName,sortKeys[sortIndex],sortDescending?"descending":"ascending");finishPlaylist()}
                else {playlistManageMode="manage";selectedIndex=0}
                return
            }
            if(playlistManageMode==="reorderMembers"){
                if(movingMember>=0){movingMember=-1;selectedIndex=0;return}
                if(selectedIndex===0 && memberDraft.length && playlistItem){refreshPlaylistPending=true;kodiMedia.reorderPlaylist(playlistItem.playlistName,memberDraft.map(row=>row.index));finishPlaylist()}
                else if(selectedIndex===1){playlistManageMode="manage";selectedIndex=0}
                else if(selectedIndex>=2){memberBaseline=memberDraft.slice();movingMember=selectedIndex-2}
                return
            }
            if(selectedIndex===0){ruleIndex=(ruleIndex+1)%ruleKeys.length;rulePreviewAccepted=false;confirmRule=false}
            else if(selectedIndex===1){keyboard.value=ruleValue;keyboard.selectedIndex=0;mode="ruleValue"}
            else if(selectedIndex===2 && playlistItem){rulePreviewAccepted=false;confirmRule=false;loadingRulePreview=true;kodiMedia.previewPlaylist(playlistItem.playlistName,ruleKeys[ruleIndex],ruleValue);playlistManageMode="previewRule";selectedIndex=0}
            else if(selectedIndex===3 && rulePreviewAccepted && playlistItem){
                if(!confirmRule){confirmRule=true;return}
                refreshPlaylistPending=true;kodiMedia.editPlaylist(playlistItem.playlistName,ruleKeys[ruleIndex],ruleValue);finishPlaylist()
            }else if(selectedIndex===4){playlistManageMode="manage";selectedIndex=0}
            return
        }
        if(playlistManageMode.length){
            if(command===inputManager.back){playlistManageMode="";pendingPlaylistIndex=-1;selectedIndex=0;return}
            if(kodiMedia.busy)return
            if(command===inputManager.navigateUp)selectedIndex=Math.max(0,selectedIndex-1)
            else if(command===inputManager.navigateDown)selectedIndex=Math.min(rows.length-1,selectedIndex+1)
            else if(command===inputManager.confirm){
                if(playlistManageMode==="manage"){
                    if(selectedIndex===0){
                        if(playlistItem){const name=playlistItem.playlistName;playlistManageMode="";mode="";kodiMedia.openView(kodiMedia.mediaKind,"playlist",name)}
                        return
                    }
                    else if(selectedIndex===1){playlistManageMode="renamePlaylist";selectedIndex=0;return}
                    else if(selectedIndex===2){
                        if(!playlistItem)return
                        if(playlistItem.playlistName.toLowerCase().endsWith(".xsp")){playlistManageMode="sortPlaylist";sortIndex=0;sortDescending=false}
                        else{playlistManageMode="reorderMembers";memberDraft=[];movingMember=-1;loadingMembers=true;kodiMedia.loadPlaylistMembers(playlistItem.playlistName)}
                        selectedIndex=0;return
                    }
                    else if(selectedIndex===3){playlistManageMode="deletePlaylist";selectedIndex=0;return}
                    else if(selectedIndex===4 && playlistItem && playlistItem.playlistName.toLowerCase().endsWith(".xsp")){playlistManageMode="editRule";ruleIndex=0;ruleValue="";rulePreviewAccepted=false;confirmRule=false;selectedIndex=0;return}
                    else {playlistManageMode="";pendingPlaylistIndex=-1;selectedIndex=0;return}
                }else if(playlistManageMode==="renamePlaylist"){
                    if(selectedIndex===0){keyboard.value=playlistItem?playlistItem.playlistName:"";keyboard.selectedIndex=0;mode="renamePlaylist";return}
                    playlistManageMode="manage";selectedIndex=0;return
                }else if(playlistManageMode==="deletePlaylist"){
                    if(selectedIndex===0 && playlistItem && playlistItem.playlistName){
                        refreshPlaylistPending=true;kodiMedia.deletePlaylist(playlistItem.playlistName)
                        if(playlistCatalog && !kodiMedia.busy)provider.refresh(playlistCatalog)
                    }
                    playlistManageMode="";pendingPlaylistIndex=-1;selectedIndex=0;return
                }
            }
            return
        }
        if(mode==="playlists" || mode==="playlistSaveState"){
            if(command===inputManager.back){mode="";return}
            if(command===inputManager.navigateUp)selectedIndex=Math.max(0,selectedIndex-1)
            else if(command===inputManager.navigateDown)selectedIndex=Math.min(rows.length-1,selectedIndex+1)
            else if(command===inputManager.confirm){
                if(mode==="playlistSaveState"){
                    if(selectedIndex===0)mode="";else if(!kodiMedia.busy)begin(savedPlaylistFile?"playlists":"savePlaylist")
                }else if(selectedIndex===0)begin("savePlaylist")
                else if(selectedIndex===1){if(playlistCatalog)provider.refresh(playlistCatalog)}
                else {const entry=playlistCatalog?playlistCatalog.items[selectedIndex-2]:null;if(entry && !kodiMedia.busy){pendingPlaylistIndex=selectedIndex-2;playlistManageMode="manage";selectedIndex=0}}
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
    Text { textFormat: Text.PlainText; font.family: shell.fontFamily; id: heading; x: 30; y: 25; width: parent.width - 60; color: shell.text; font.pixelSize: 27*(root.appearance.typographyScale || 1)
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
        onCanceled: { if(root.playlistManageMode.length){const rule=root.mode==="ruleValue";root.mode="playlists";root.playlistManageMode=rule?"editRule":"manage";root.selectedIndex=0}else root.mode="" }
        onAccepted: text => {
            if(root.mode==="ruleValue"){
                if(text.length>160)return
                root.ruleValue=text;root.rulePreviewAccepted=false;root.confirmRule=false;root.mode="playlists";root.playlistManageMode="editRule";root.selectedIndex=2;return
            }
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
            else if (root.mode === "renamePlaylist") {
                if(!text.trim().length || text.trim().length>80)return
                if(playlistItem && playlistItem.playlistName){
                    refreshPlaylistPending=true;kodiMedia.renamePlaylist(playlistItem.playlistName, text.trim())
                    if(playlistCatalog && !kodiMedia.busy)provider.refresh(playlistCatalog)
                }
                mode="playlists";playlistManageMode="";pendingPlaylistIndex=-1;selectedIndex=0;return
            }
            else { kodiMedia.setView(root.mode, text); root.mode = "" }
        }
    }
    ListView {
        id: choices; x: 30; y: heading.y + heading.height + 20; width: parent.width - 60; height: Math.max(0,parent.height-y-statusText.height-36)
        visible: !root.typing; model: root.rows; clip: true; spacing: 8 * shell.gapScale; currentIndex: root.selectedIndex
        highlightRangeMode: ListView.ApplyRange
        delegate: Rectangle {
            required property int index; required property string modelData
            width: choices.width; height: 58; radius: shell.radius; color: root.selectedIndex === index ? shell.selected : shell.surface;border.width:root.selectedIndex===index?2:0;border.color:root.selectedIndex===index?shell.selectedFocus:shell.focus
            Text { textFormat: Text.PlainText; font.family: shell.fontFamily; anchors.fill: parent; anchors.margins: 14; color: (root.selectedIndex === index) ? shell.selectedText : shell.text; font.pixelSize: 21*(root.appearance.typographyScale || 1); elide: Text.ElideRight; text: modelData }
            MouseArea { anchors.fill: parent; onClicked: { root.selectedIndex = index; root.handleCommand(inputManager.confirm) } }
        }
    }
    Text { id: statusText; textFormat: Text.PlainText; font.family: shell.fontFamily; x: 30; anchors.bottom: parent.bottom; anchors.bottomMargin: 20; width: parent.width - 60; wrapMode: Text.WordWrap; color: shell.muted; font.pixelSize: 17*(root.appearance.typographyScale || 1)
        text:(root.mode==="playlists" || root.playlistManageMode.length>0)?root.playlistHelp()+"\n"+kodiMedia.playlistStatus:playlistManageMode==="renamePlaylist"?"Enter new playlist name · Back cancels":playlistManageMode==="reorderPlaylist"?"Enter item order as comma-separated indices (e.g. 2,0,1) · Back cancels":playlistManageMode==="deletePlaylist"?"Delete playlist · " + (playlistItem?playlistItem.title:"") + " · Confirm or Back cancels":playlistManageMode==="manage"?"Manage playlist · " + (playlistItem?playlistItem.title:"") + " · Open, rename, reorder, or delete":root.mode==="playlistSaveState"?kodiMedia.playlistStatus:root.mode==="playlists"?(root.playlistCatalog?root.playlistCatalog.status:"Kodi unavailable · Back returns"):root.mode==="savePlaylist"?"Saves this library view as a playlist in Kodi. Back cancels without writing.":kodiMedia.collectionStatus || "Collections are reusable local rules. They do not move files or edit Kodi tags. Back cancels."
    }
}
