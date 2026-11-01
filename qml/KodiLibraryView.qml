import QtQuick

Rectangle {
    id: root
    objectName: "kodiLibrary"
    property var appearance: ({})
    property var provider: null
    readonly property real textScale: appearance.typographyScale || 1
    color: root.appearance.backgroundEnd || "#101014"
    property bool open: false
    property bool returnToHomeAfterPlayback: false
    property bool returnToSearchAfterPlayback: false
    property var details: null
    property int toolbarIndex: 0
    property string artworkKey: ""
    signal searchRequested()
    readonly property var selectedItem: kodiMedia.selectedIndex >= 0 && kodiMedia.selectedIndex < kodiMedia.items.length ? kodiMedia.items[kodiMedia.selectedIndex] : null
    readonly property var toolbar: {
        let rows = [{ label: "Movies", action: "movies" }, { label: "Shows", action: "shows" }]
        if (kodiMedia.mediaKind !== "seasons") rows = rows.concat([{ label: "Recently Added", action: "recent" }, { label: "Continue Watching", action: "continue" }, { label: "Search", action: "search" }])
        if (kodiMedia.mediaKind === "movies" || kodiMedia.mediaKind === "shows") rows = rows.concat([{ label: "Collections", action: "collections" }, { label: "Save playlist", action: "save" }])
        if(kodiMedia.mediaKind==="movies" || kodiMedia.mediaKind==="shows")rows.push({label:"Playlists",action:"playlists"})
        return rows.concat([{ label: "Refresh", action: "refresh" }])
    }
    onToolbarChanged: toolbarIndex = Math.min(toolbarIndex, toolbar.length - 1)
    function toolbarAction(index) {
        const row = toolbar[index]
        if (!row || kodiMedia.busy) return
        details = null
        if (row.action === "movies" || row.action === "shows") kodiMedia.openLibrary(row.action)
        else if (row.action === "recent" || row.action === "continue") kodiMedia.setView(row.action)
        else if (row.action === "search") searchRequested()
        else if (row.action === "collections" || row.action === "save" || row.action === "playlists") browseControls.begin(row.action==="save"?"savePlaylist":row.action)
        else kodiMedia.refresh()
    }
    function subtitle(item) {
        if (!item) return ""
        if (item.mediaType === "tvshow" || item.mediaType === "season") return (item.episode || 0) + " episodes · " + (item.watchedepisodes || 0) + " watched"
        const progress = item.resume && item.resume.position > 0 ? "Resume at " + Math.floor(item.resume.position / 60) + " min" : item.playcount > 0 ? "Watched" : "Unwatched"
        const episode = item.mediaType === "episode" ? (item.showtitle || "") + " · S" + String(item.season).padStart(2, "0") + " E" + String(item.episode).padStart(2, "0") + " · " : item.year ? item.year + " · " : ""
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
    onOpenChanged: { if (!open) { details = null; browseControls.mode = "" }; updateArtwork() }
    Connections {
        target: kodiMedia
        function onChanged() { if (!kodiMedia.connected) root.details = null; root.updateArtwork() }
        function onPlaybackReturned(home) {
            if (root.returnToSearchAfterPlayback) {
                root.returnToSearchAfterPlayback = false
                root.open = false
                if (home) sessionManager.goHome()
                return
            }
            if (home || (root.returnToHomeAfterPlayback && kodiMedia.playbackStatus === "ended")) { root.open = false; sessionManager.goHome() }
            root.returnToHomeAfterPlayback = false
        }
    }
    function handleCommand(command) {
        if (command === inputManager.home) { browseControls.mode = ""; open = false; sessionManager.goHome(); return }
        if (browseControls.active) { browseControls.handleCommand(command); return }
        if (command === inputManager.back) { if (details) details = null; else if (!kodiMedia.goBack()) open = false; return }
        if (kodiMedia.busy) return
        if (command === inputManager.search && kodiMedia.mediaKind !== "seasons") { searchRequested(); return }
        if (details) {
            if (command === inputManager.confirm) { details = null; kodiMedia.activate(kodiMedia.selectedIndex) }
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
        anchors.fill: parent; anchors.margins: Math.max(32, root.width * 0.04); spacing: 16
        Text { text: "YOUR LIBRARY  /  KODI"; color: "#879CBC"; font.pixelSize: 15 * root.textScale; font.letterSpacing: 3 }
        Text { text: kodiMedia.browseTitle; color: root.appearance.textColor || "#F5F1F3"; font.pixelSize: 34 * root.textScale; width: parent.width; elide: Text.ElideRight }
        Text { text: profileManager.activeName + " · " + kodiMedia.viewKind + (kodiMedia.viewValue ? ": " + kodiMedia.viewValue : "") + " · Local library · Web OFF"; color: root.appearance.focusColor || "#C994A5"; font.pixelSize: 17 * root.textScale }
        Row {
            width: parent.width; height: 56; spacing: 8
            Repeater { model: root.toolbar
                delegate: Rectangle {
                    required property int index; required property var modelData
                    width: (parent.width - (root.toolbar.length - 1) * 8) / root.toolbar.length; height: 56; radius: 10
                    color: root.appearance.surfaceColor || "#202126"; border.width: kodiMedia.selectedIndex < 0 && root.toolbarIndex === index ? 3 : 0; border.color: root.appearance.focusColor || "#C994A5"
                    Text { anchors.fill: parent; anchors.margins: 7; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; wrapMode: Text.WordWrap; text: modelData.label; font.pixelSize: 16 * root.textScale; color: root.appearance.textColor || "#F5F1F3" }
                    MouseArea { anchors.fill: parent; onClicked: { kodiMedia.selectIndex(-1); root.toolbarIndex = index; root.toolbarAction(index) } }
                }
            }
        }
        Row {
            width: parent.width; height: Math.max(100, root.height - 335); spacing: 28
            ListView {
                id: list; objectName: "libraryItems"
                width: parent.width * 0.61; height: parent.height; clip: true; spacing: 9
                model: kodiMedia.items; currentIndex: kodiMedia.selectedIndex; highlightRangeMode: ListView.ApplyRange; highlightMoveDuration: 0
                delegate: Rectangle {
                    required property var modelData; required property int index
                    width: ListView.view.width; height: Math.max(78, 72 * root.textScale); radius: 12; color: kodiMedia.selectedIndex === index ? root.appearance.selectedColor || "#353039" : root.appearance.surfaceColor || "#202126"
                    border.width: kodiMedia.selectedIndex === index ? 2 : 0; border.color: root.appearance.focusColor || "#C994A5"
                    Column {
                        anchors.fill: parent; anchors.margins: 13; spacing: 5
                        Text { width: parent.width; text: modelData.title || modelData.label || "Untitled"; color: root.appearance.textColor || "#F5F1F3"; font.pixelSize: 21 * root.textScale; elide: Text.ElideRight }
                        Text { width: parent.width; text: root.subtitle(modelData); color: root.appearance.mutedColor || "#AAA5AF"; font.pixelSize: 15 * root.textScale; elide: Text.ElideRight }
                    }
                    MouseArea { anchors.fill: parent; enabled: !kodiMedia.busy && !kodiMedia.playbackActive; acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: mouse => { kodiMedia.selectIndex(index); if (mouse.button === Qt.RightButton) root.details = modelData; else kodiMedia.activate(index) } }
                }
                Text { anchors.centerIn: parent; width: parent.width - 32; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; visible: !kodiMedia.items.length
                    text: kodiMedia.busy ? "Loading from Kodi…" : kodiMedia.status + "\n\n↑ Toolbar · Refresh to retry · Back to return"; color: root.appearance.mutedColor || "#AAA5AF"; font.pixelSize: 21 * root.textScale }
            }
            Column {
                width: parent.width - list.width - parent.spacing; height: parent.height; spacing: 12
                Rectangle {
                    width: parent.width; height: Math.max(80, parent.height - 125); radius: 16; color: root.appearance.surfaceColor || "#202126"; clip: true
                    Image { anchors.fill: parent; anchors.margins: 1; source: kodiArtwork.source; fillMode: Image.PreserveAspectFit; sourceSize.width: 500; sourceSize.height: 650 }
                    Text { anchors.centerIn: parent; width: parent.width - 30; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; visible: !kodiArtwork.source
                        text: root.selectedItem ? kodiArtwork.status || "Kodi library" : "Choose a title"; color: "#758BAE"; font.pixelSize: 18 * root.textScale }
                }
                Text { text: root.selectedItem ? root.selectedItem.title : ""; color: root.appearance.textColor || "#F5F1F3"; font.pixelSize: 23 * root.textScale; width: parent.width; maximumLineCount: 2; wrapMode: Text.WordWrap; elide: Text.ElideRight }
                Text { text: root.selectedItem ? root.selectedItem.plot || root.subtitle(root.selectedItem) : ""; color: root.appearance.mutedColor || "#AAA5AF"; font.pixelSize: 16 * root.textScale; width: parent.width; maximumLineCount: 3; wrapMode: Text.WordWrap; elide: Text.ElideRight }
            }
        }
        Text { text: kodiMedia.status + (kodiMedia.playbackStatus ? " · " + kodiMedia.playbackStatus : ""); width: parent.width; elide: Text.ElideRight; color: "#9EB0CC"; font.pixelSize: 15 * root.textScale }
        Text { text: "OK open / play / resume    Menu details    ↑ Toolbar    ← → Pages    Back " + (kodiMedia.canGoBack ? "parent" : "Home"); color: root.appearance.focusColor || "#C994A5"; font.pixelSize: 16 * root.textScale; width: parent.width; elide: Text.ElideRight }
    }
    Rectangle {
        anchors.fill: parent; anchors.margins: 35; color: "#101B30"; radius: 18; visible: root.details !== null
        Column {
            anchors.fill: parent; anchors.margins: 35; spacing: 22
            Text { width: parent.width; text: root.details ? root.details.title : ""; color: root.appearance.textColor || "#F5F1F3"; font.pixelSize: 34 * root.textScale; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
            Text { width: parent.width; text: root.subtitle(root.details); color: root.appearance.focusColor || "#C994A5"; font.pixelSize: 21 * root.textScale; wrapMode: Text.WordWrap }
            Text { width: parent.width; text: root.details ? root.details.plot || "No description supplied by Kodi." : ""; color: "#DCE5F3"; font.pixelSize: 22 * root.textScale; wrapMode: Text.WordWrap; maximumLineCount: Math.max(2, Math.floor((root.height - 340) / 28)); elide: Text.ElideRight }
                    Text { width: parent.width; text: root.details && root.details.mediaType === "tvshow" ? "OK browse · Menu curate · Back close"
                        : root.details && root.details.mediaType === "season" ? "OK browse · Back close"
                    : "OK play / resume · Right restart · Menu curate · Back close\nDuring playback: OK pause · Left/Right seek · Back stop · Home return"; color: root.appearance.mutedColor || "#AAA5AF"; font.pixelSize: 20 * root.textScale; wrapMode: Text.WordWrap }
            Text { text: kodiMedia.playbackStatus; color: root.appearance.focusColor || "#C994A5"; font.pixelSize: 18 * root.textScale }
        }
        MouseArea { anchors.right: parent.right; anchors.top: parent.top; width: 90; height: 45; onClicked: root.details = null
            Text { anchors.centerIn: parent; text: "Back"; color: root.appearance.textColor || "#F5F1F3"; font.pixelSize: 20 * root.textScale } }
    }
    LibraryBrowseControls { id: browseControls; provider:root.provider;appearance:root.appearance;anchors.fill: parent; anchors.margins: 35; z: 10 }
}
