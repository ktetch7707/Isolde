import QtQuick

Rectangle {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    objectName: "kodiLibrary"
    property var appearance: ({})
    property var provider: null
    readonly property real textScale: appearance.typographyScale || 1
    color: shell.background
    property bool open: false
    property bool returnToHomeAfterPlayback: false
    property bool returnToSearchAfterPlayback: false
    property var details: null
    property int toolbarIndex: 0
    property string artworkKey: ""
    property bool choosingPoster: false
    property int posterIndex: 0
    property bool confirmPoster: false
    property string posterStatus: ""
    property var posterChoices: []
    function closePoster(){choosingPoster=false;confirmPoster=false;posterStatus=""}
    function openPoster(){
        if(!details || !profileManager.editingAllowed || (details.mediaType!=="movie" && details.mediaType!=="tvshow"))return
        posterChoices=homeLayout.captureArtworkChoices();posterIndex=0;confirmPoster=false;posterStatus="";choosingPoster=true
    }
    signal searchRequested()
    signal searchReturnRequested()
    readonly property var selectedItem: kodiMedia.selectedIndex >= 0 && kodiMedia.selectedIndex < kodiMedia.items.length ? kodiMedia.items[kodiMedia.selectedIndex] : null
    readonly property var toolbar: {
        const views=kodiService.libraryViews || []
        let rows=views.filter(row=>row.position==="primary").map(row=>({label:row.name,action:"library",kind:row.kind}))
        const music=kodiMedia.mediaKind==="albums" || kodiMedia.mediaKind==="songs"
        if (kodiMedia.mediaKind !== "seasons" && !music) rows = rows.concat([{ label: "Recently Added", action: "recent" }, { label: "Continue Watching", action: "continue" }, { label: "Search", action: "search" }])
        if (kodiMedia.mediaKind === "movies" || kodiMedia.mediaKind === "shows") rows = rows.concat([{ label: "Collections", action: "collections" }, { label: "Save playlist", action: "save" }])
        if(kodiMedia.mediaKind==="movies" || kodiMedia.mediaKind==="shows")rows.push({label:"Playlists",action:"playlists"})
        return rows.concat([{ label: "Refresh", action: "refresh" }]).concat(views.filter(row=>row.position==="trailing").map(row=>({label:row.name,action:"library",kind:row.kind})))
    }
    onToolbarChanged: toolbarIndex = Math.min(toolbarIndex, toolbar.length - 1)
    function toolbarAction(index) {
        const row = toolbar[index]
        if (!row || kodiMedia.busy) return
        details = null
        if (row.action === "library") kodiMedia.openView(row.kind,"all")
        else if (row.action === "recent" || row.action === "continue") kodiMedia.setView(row.action)
        else if (row.action === "search") searchRequested()
        else if (row.action === "collections" || row.action === "save" || row.action === "playlists") browseControls.begin(row.action==="save"?"savePlaylist":row.action)
        else kodiMedia.refresh()
    }
    function subtitle(item) {
        if (!item) return ""
        if(item.mediaType==="album" || item.mediaType==="song")return [(item.artist || []).join(", "),item.mediaType==="song"?item.album:"",item.duration>0?Math.ceil(item.duration/60)+" min":""].filter(value=>value).join(" · ")
        if (item.mediaType === "tvshow" || item.mediaType === "season") return (item.episode || 0) + " episodes · " + (item.watchedepisodes || 0) + " watched"
        const progress = item.resume && item.resume.position > 0 ? "Resume at " + Math.floor(item.resume.position / 60) + " min" : item.playcount > 0 ? "Watched" : "Unwatched"
        const knownEpisode = Number.isInteger(item.season) && item.season >= 0 && Number.isInteger(item.episode) && item.episode > 0
        const episodeIdentity = knownEpisode ? "S" + String(item.season).padStart(2, "0") + " E" + String(item.episode).padStart(2, "0") : ""
        const episode = item.mediaType === "episode" ? [item.showtitle || "", episodeIdentity].filter(value => value.length > 0).join(" · ") + (item.showtitle || episodeIdentity ? " · " : "") : item.year ? item.year + " · " : ""
        return episode + progress + (item.runtime > 0 ? " · " + Math.round(item.runtime / 60) + " min" : "")
    }
    function updateArtwork() {
        const item = selectedItem
        const key = open && item ? item.mediaType + ":" + item.mediaId : ""
        if (key === artworkKey) return
        artworkKey = key
        if (key) kodiArtwork.load(item.mediaType, item.mediaId)
        else kodiArtwork.clear()
    }
    visible: open
    onSelectedItemChanged: updateArtwork()
    onOpenChanged: { if (!open) { closePoster();details = null; browseControls.mode = "" }; updateArtwork() }
    Connections {
        target: kodiMedia
        function onChanged() { if (!kodiMedia.connected) root.details = null; root.updateArtwork() }
        function onPlaybackReturned(home) {
            if (root.returnToSearchAfterPlayback) {
                root.returnToSearchAfterPlayback = false
                root.open = false
                if (home) sessionManager.goHome()
                else root.searchReturnRequested()
                return
            }
            if (home || (root.returnToHomeAfterPlayback && kodiMedia.playbackStatus === "ended")) { root.open = false; sessionManager.goHome() }
            root.returnToHomeAfterPlayback = false
        }
    }
    function handleCommand(command) {
        if (command === inputManager.home) { browseControls.mode = ""; open = false; sessionManager.goHome(); return }
        if(choosingPoster){
            if(command===inputManager.back){if(confirmPoster)confirmPoster=false;else closePoster()}
            else if(command===inputManager.navigateUp || command===inputManager.navigateDown){confirmPoster=false;posterIndex=Math.max(0,Math.min(posterChoices.length-1,posterIndex+(command===inputManager.navigateDown?1:-1)))}
            else if(command===inputManager.confirm && posterChoices.length && details){
                if(!confirmPoster)confirmPoster=true
                else {confirmPoster=false;posterStatus=kodiArtwork.setPoster(details.mediaType,details.mediaId,posterChoices[posterIndex].id)?"Saving confirmed poster…":"The managed image could not be applied. Choose a PNG/JPEG within the supported size and retry."}
            }
            return
        }
        if (browseControls.active) { browseControls.handleCommand(command); return }
        if (command === inputManager.back) { if (details) details = null; else if (!kodiMedia.goBack()) open = false; return }
        if (kodiMedia.busy) return
        if (command === inputManager.search && kodiMedia.mediaKind !== "seasons") { searchRequested(); return }
        if (details) {
            if (command === inputManager.confirm) { details = null; kodiMedia.activate(kodiMedia.selectedIndex) }
            else if(command===inputManager.navigateLeft)openPoster()
            else if (command === inputManager.navigateRight) kodiMedia.playSelected(false)
            else if (command === inputManager.menu && (details.mediaType === "movie" || details.mediaType === "tvshow")) {
                browseControls.begin("addSelected", details)
                details = null
            }
            return
        }
        if (command === inputManager.navigateRight) { if (kodiMedia.selectedIndex < 0) toolbarIndex = Math.min(toolbar.length - 1, toolbarIndex + 1); else kodiMedia.nextPage() }
        else if (command === inputManager.navigateLeft) { if (kodiMedia.selectedIndex < 0) toolbarIndex = Math.max(0, toolbarIndex - 1); else kodiMedia.previousPage() }
        else if (command === inputManager.confirm) { if (kodiMedia.selectedIndex >= 0) kodiMedia.activate(kodiMedia.selectedIndex); else toolbarAction(toolbarIndex) }
        else if (command === inputManager.menu && selectedItem) details = selectedItem
        else if (command === inputManager.navigateDown) kodiMedia.selectIndex(kodiMedia.selectedIndex + 1)
        else if (command === inputManager.navigateUp) kodiMedia.selectIndex(kodiMedia.selectedIndex - 1)
    }
    MouseArea { anchors.fill: parent }
    Column {
        anchors.fill: parent; anchors.margins: Math.max(32, root.width * 0.04); spacing: 16 * shell.gapScale
        Text { font.family: shell.fontFamily; text: "YOUR LIBRARY  /  KODI"; color: shell.backgroundMuted; font.pixelSize: 15 * root.textScale; font.letterSpacing: 3 }
        Text { font.family: shell.fontFamily; text: kodiMedia.browseTitle; color: shell.backgroundText; font.pixelSize: 34 * root.textScale; width: parent.width; elide: Text.ElideRight }
        Text { font.family: shell.fontFamily; text: profileManager.activeName + " · " + kodiMedia.viewKind + (kodiMedia.viewValue ? ": " + kodiMedia.viewValue : "") + " · Local library · Web OFF"; color: shell.backgroundFocus; font.pixelSize: 17 * root.textScale }
        Row {
            width: parent.width; height: 56; spacing: 8 * shell.gapScale
            Repeater { model: root.toolbar
                delegate: Rectangle {
                    required property int index; required property var modelData
                    width: (parent.width - (root.toolbar.length - 1) * 8) / root.toolbar.length; height: 56; radius: shell.radius
                    color: shell.surface; border.width: kodiMedia.selectedIndex < 0 && root.toolbarIndex === index ? 3 : 0; border.color: shell.focus
                    Text { font.family: shell.fontFamily; anchors.fill: parent; anchors.margins: 7; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; wrapMode: Text.WordWrap; text: modelData.label; font.pixelSize: 16 * root.textScale; color: shell.text }
                    MouseArea { anchors.fill: parent; onClicked: { kodiMedia.selectIndex(-1); root.toolbarIndex = index; root.toolbarAction(index) } }
                }
            }
        }
        Row {
            width: parent.width; height: Math.max(100, root.height - 335); spacing: 28 * shell.gapScale
            ListView {
                id: list; objectName: "libraryItems"
                width: parent.width * 0.61; height: parent.height; clip: true; spacing: 9 * shell.gapScale
                model: kodiMedia.items; currentIndex: kodiMedia.selectedIndex; highlightRangeMode: ListView.ApplyRange; highlightMoveDuration: 0
                delegate: Rectangle {
                    required property var modelData; required property int index
                    width: ListView.view.width; height: Math.max(78, 72 * root.textScale); radius: shell.radius; color: kodiMedia.selectedIndex === index ? shell.selected : shell.surface
                    border.width: kodiMedia.selectedIndex === index ? 2 : 0; border.color: (kodiMedia.selectedIndex === index) ? shell.selectedFocus : shell.focus
                    Column {
                        anchors.fill: parent; anchors.margins: 13; spacing: 5 * shell.gapScale
                        Text { font.family: shell.fontFamily; width: parent.width; text: modelData.title || modelData.label || "Untitled"; color: (kodiMedia.selectedIndex === index) ? shell.selectedText : shell.text; font.pixelSize: 21 * root.textScale; elide: Text.ElideRight }
                        Text { font.family: shell.fontFamily; width: parent.width; text: root.subtitle(modelData); color: (kodiMedia.selectedIndex === index) ? shell.selectedText : shell.muted; font.pixelSize: 15 * root.textScale; elide: Text.ElideRight }
                    }
                    MouseArea { anchors.fill: parent; enabled: !kodiMedia.busy && !kodiMedia.playbackActive; acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: mouse => { kodiMedia.selectIndex(index); if (mouse.button === Qt.RightButton) root.details = modelData; else kodiMedia.activate(index) } }
                }
                Text { font.family: shell.fontFamily; anchors.centerIn: parent; width: parent.width - 32; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; visible: !kodiMedia.items.length
                    text: kodiMedia.busy ? "Loading from Kodi…" : kodiMedia.status + "\n\n↑ Toolbar · Refresh to retry · Back to return"; color: shell.backgroundMuted; font.pixelSize: 21 * root.textScale }
            }
            Column {
                width: parent.width - list.width - parent.spacing; height: parent.height; spacing: 12 * shell.gapScale
                Rectangle {
                    width: parent.width; height: Math.max(80, parent.height - 125); radius: shell.radius; color: shell.surface; clip: true
                    Image { anchors.fill: parent; anchors.margins: 1; source: kodiArtwork.source; fillMode: Image.PreserveAspectFit; sourceSize.width: 500; sourceSize.height: 650 }
                    Text { font.family: shell.fontFamily; anchors.centerIn: parent; width: parent.width - 30; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; visible: !kodiArtwork.source
                        text: root.selectedItem ? kodiArtwork.status || "Kodi library" : "Choose a title"; color: shell.unavailable; font.pixelSize: 18 * root.textScale }
                }
                Text { font.family: shell.fontFamily; text: root.selectedItem ? root.selectedItem.title : ""; color: shell.backgroundText; font.pixelSize: 23 * root.textScale; width: parent.width; maximumLineCount: 2; wrapMode: Text.WordWrap; elide: Text.ElideRight }
                Text { font.family: shell.fontFamily; text: root.selectedItem ? root.selectedItem.plot || root.subtitle(root.selectedItem) : ""; color: shell.backgroundMuted; font.pixelSize: 16 * root.textScale; width: parent.width; maximumLineCount: 3; wrapMode: Text.WordWrap; elide: Text.ElideRight }
            }
        }
        Text { font.family: shell.fontFamily; objectName: "libraryStatusLine"; text: kodiMedia.status + (kodiMedia.playbackStatus ? " · " + kodiMedia.playbackStatus : "") + (kodiArtwork.status ? " · "+kodiArtwork.status:""); width: parent.width; elide: Text.ElideRight; color: !kodiMedia.busy && !kodiMedia.connected ? shell.backgroundError : shell.backgroundMuted; font.pixelSize: 15 * root.textScale }
        Text { font.family: shell.fontFamily; text: "OK open / play / resume    Menu details    ↑ Toolbar    ← → Pages    Back " + (kodiMedia.canGoBack ? "parent" : "Home"); color: shell.backgroundFocus; font.pixelSize: 16 * root.textScale; width: parent.width; elide: Text.ElideRight }
    }
    Rectangle {
        objectName: "libraryDetailsPanel"
        anchors.fill: parent; anchors.margins: 35; color: shell.elevated; radius: shell.radius; visible: root.details !== null
        Column {
            anchors.fill: parent; anchors.margins: 35; spacing: 22 * shell.gapScale
            Text { font.family: shell.fontFamily; width: parent.width; text: root.details ? root.details.title : ""; color: shell.elevatedText; font.pixelSize: 34 * root.textScale; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
            Text { font.family: shell.fontFamily; width: parent.width; objectName: "libraryDetailsSubtitle"; text: root.subtitle(root.details); color: shell.elevatedFocus; font.pixelSize: 21 * root.textScale; wrapMode: Text.WordWrap }
            Text { font.family: shell.fontFamily; width: parent.width; text: root.details ? root.details.plot || "No description supplied by Kodi." : ""; color: shell.elevatedText; font.pixelSize: 22 * root.textScale; wrapMode: Text.WordWrap; maximumLineCount: Math.max(2, Math.floor((root.height - 340) / 28)); elide: Text.ElideRight }
                    Text { font.family: shell.fontFamily; width: parent.width; text: root.details && root.details.mediaType === "tvshow" ? "OK browse · Menu curate · Back close"
                        : root.details && (root.details.mediaType === "season" || root.details.mediaType === "album") ? "OK browse · Back close"
                        : root.details && root.details.mediaType === "song" ? "OK play · Back close\nDuring playback: Menu controls · Back stop · Home return"
                    : "OK play / resume · Right restart · Menu curate · Back close\nDuring playback: OK pause · Left/Right seek · Back stop · Home return"; color: shell.elevatedMuted; font.pixelSize: 20 * root.textScale; wrapMode: Text.WordWrap }
            Text { font.family: shell.fontFamily; objectName: "libraryDetailsPlayback"; text: kodiMedia.playbackStatus; color: shell.elevatedFocus; font.pixelSize: 18 * root.textScale }
            Text { width:parent.width;visible:root.details && (root.details.mediaType==="movie" || root.details.mediaType==="tvshow") && profileManager.editingAllowed;text:"Left · Change this title’s Kodi poster";color:shell.elevatedFocus;font.family:shell.fontFamily;font.pixelSize:20*root.textScale;wrapMode:Text.WordWrap }
        }
        MouseArea { anchors.right: parent.right; anchors.top: parent.top; width: 90; height: 45; onClicked: root.details = null
            Text { font.family: shell.fontFamily; anchors.centerIn: parent; text: "Back"; color: shell.elevatedText; font.pixelSize: 20 * root.textScale } }
    }
    LibraryBrowseControls { id: browseControls; provider:root.provider;appearance:root.appearance;anchors.fill: parent; anchors.margins: 35; z: 10 }
    Rectangle {
        anchors.fill:parent;anchors.margins:35;z:11;visible:root.choosingPoster;color:shell.elevated;radius:shell.radius
        Column {
            id:posterContents
            anchors.fill:parent;anchors.margins:24;spacing:16*shell.gapScale
            Text { id:posterHeading;width:parent.width;wrapMode:Text.WordWrap;text:"CHANGE TITLE POSTER";font.family:shell.fontFamily;font.pixelSize:28*root.textScale;color:shell.elevatedText }
            Text { id:posterHelp;width:parent.width;wrapMode:Text.WordWrap;text:root.confirmPoster?"CONFIRM · Save this poster to the selected Kodi title? Back cancels.":"Up/Down previews managed artwork. OK reviews the change; OK again saves to Kodi. Back leaves the title unchanged.";font.family:shell.fontFamily;font.pixelSize:18*root.textScale;color:shell.elevatedFocus }
            Row {
                width:parent.width;height:Math.max(0,parent.height-posterHeading.height-posterHelp.height-posterStatusLabel.height-3*parent.spacing);spacing:16*shell.gapScale
                ListView {
                    width:Math.max(0,parent.width*0.55-parent.spacing);height:parent.height;clip:true;model:root.posterChoices;currentIndex:root.posterIndex
                    delegate:Rectangle {
                        required property int index;required property var modelData
                        width:ListView.view.width;height:posterLabel.implicitHeight+28;radius:shell.radius;color:root.posterIndex===index?shell.selected:shell.elevated
                        border.width:root.posterIndex===index?2:0;border.color:shell.selectedFocus
                        Text { id:posterLabel;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:14;wrapMode:Text.WrapAnywhere;textFormat:Text.PlainText;text:modelData.name;font.family:shell.fontFamily;font.pixelSize:20*root.textScale;color:root.posterIndex===index?shell.selectedText:shell.elevatedText }
                        MouseArea { anchors.fill:parent;onClicked:{if(root.posterIndex!==index){root.posterIndex=index;root.confirmPoster=false}else root.handleCommand(inputManager.confirm)} }
                    }
                    Text { width:parent.width;visible:!root.posterChoices.length;text:"No managed artwork yet. Import an image through Studio’s artwork controls, then reopen this picker.";wrapMode:Text.WordWrap;color:shell.elevatedText;font.family:shell.fontFamily;font.pixelSize:20*root.textScale }
                }
                Image { width:parent.width*0.45;height:parent.height;sourceSize.width:512;sourceSize.height:768;fillMode:Image.PreserveAspectFit;source:root.choosingPoster && root.posterChoices.length?homeLayout.resolveArtwork(root.posterChoices[root.posterIndex].id):"" }
            }
            Text { id:posterStatusLabel;width:parent.width;wrapMode:Text.WordWrap;textFormat:Text.PlainText;text:root.posterStatus?(kodiArtwork.status || root.posterStatus):"Preview · no changes saved";color:shell.elevatedFocus;font.family:shell.fontFamily;font.pixelSize:18*root.textScale }
        }
    }
}
