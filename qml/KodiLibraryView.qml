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
    property bool watchedActionFocused: false
    property bool nextUnwatchedFocused: false
    readonly property bool canFindNextEpisode: hierarchyLevel === "episodes" && !!details && details.mediaType === "episode"
    readonly property bool canChangeWatched: !!details && profileManager.editingAllowed === true
        && (details.mediaType === "movie" || details.mediaType === "episode") && details.mediaId > 0
    onDetailsChanged: { watchedActionFocused = false; nextUnwatchedFocused = false }
    property int toolbarIndex: 0
    property string artworkKey: ""
    property bool choosingPoster: false
    property int posterIndex: 0
    property bool confirmPoster: false
    property string posterStatus: ""
    property var posterChoices: []
    readonly property string hierarchyLevel: kodiMedia.hierarchyLevel
    readonly property bool hierarchyOpen: open && hierarchyLevel !== ""
    readonly property bool hierarchyFullscreen: hierarchyLevel === "seasons"
        ? appearance.seasonPresentation === "fullscreen" : appearance.episodePresentation === "fullscreen"
    readonly property string hierarchyLayout: (hierarchyLevel === "seasons" ? appearance.seasonLayout : appearance.episodeLayout) || "artwork"
    readonly property bool hierarchyShowsArtwork: !hierarchyOpen || hierarchyLayout === "artwork"
    readonly property int hierarchyColumns: hierarchyOpen && hierarchyLayout === "grid" ? 2 : 1
    readonly property bool hierarchyShowsHeader: (hierarchyLevel === "seasons" ? appearance.seasonHeader : appearance.episodeHeader) !== false
    readonly property bool hierarchyShowsMetadata: !hierarchyOpen || (hierarchyLevel === "seasons" ? appearance.seasonMetadata : appearance.episodeMetadata) !== false
    readonly property string episodeWatchedStyle: appearance.episodeWatchedStyle || "text"
    readonly property color episodeWatchedOutlineColor: appearance.episodeWatchedOutlineColor || shell.success
    readonly property bool episodeWatchedCheckmark: episodeWatchedStyle === "checkmark" || episodeWatchedStyle === "checkmark-outline"
    readonly property bool episodeWatchedOutline: episodeWatchedStyle === "outline" || episodeWatchedStyle === "checkmark-outline"
    function closePoster(){choosingPoster=false;confirmPoster=false;posterStatus=""}
    function openPoster(){
        if(!details || !profileManager.editingAllowed || (details.mediaType!=="movie" && details.mediaType!=="tvshow"))return
        posterChoices=homeLayout.captureArtworkChoices();posterIndex=0;confirmPoster=false;posterStatus="";choosingPoster=true
    }
    signal searchRequested()
    signal searchReturnRequested()

    // Use the open detail's identity and let Kodi confirm the change before refreshing the list.
    function changeWatched() {
        if (!canChangeWatched || kodiMedia.busy) return
        if (kodiMedia.markWatched(details.mediaType, details.mediaId, details.watchState !== "watched"))
            details = null
    }

    // Search within this season, then let the viewer confirm playback of Kodi's result.
    function findOrPlayNextEpisode() {
        if (!canFindNextEpisode || kodiMedia.busy) return
        if (kodiMedia.nextUnwatchedCandidateId > 0) {
            if (kodiMedia.activateNextUnwatched()) details = null
        } else {
            kodiMedia.nextUnwatched("episode")
        }
    }
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
    // Describe confirmed watch and aggregate states without turning missing Kodi fields into zero.
    function subtitle(item) {
        if (!item) return ""
        if(item.mediaType==="album" || item.mediaType==="song")return [(item.artist || []).join(", "),item.mediaType==="song"?item.album:"",item.duration>0?Math.ceil(item.duration/60)+" min":""].filter(value=>value).join(" · ")
        if (item.mediaType === "tvshow" || item.mediaType === "season") {
            const total = Number.isInteger(item.episode) && item.episode >= 0 ? qsTr("%1 episodes").arg(item.episode) : ""
            const watched = Number.isInteger(item.watchedepisodes) && item.watchedepisodes >= 0 ? qsTr("%1 watched").arg(item.watchedepisodes) : ""
            return [total, watched].filter(value => value.length > 0).join(" · ")
        }
        let progress = qsTr("Watch status unavailable")
        if (item.watchState === "watched") progress = qsTr("Watched")
        else if (item.watchState === "unwatched") progress = qsTr("Unwatched")
        else if (item.watchState === "inProgress") progress = qsTr("Resume at %1 min").arg(Math.floor(item.resume.position / 60))
        const knownEpisode = Number.isInteger(item.season) && item.season >= 0 && Number.isInteger(item.episode) && item.episode > 0
        const episodeIdentity = knownEpisode ? "S" + String(item.season).padStart(2, "0") + " E" + String(item.episode).padStart(2, "0") : ""
        const episode = item.mediaType === "episode"
        const optional = episode && hierarchyLevel === "episodes"
        if (optional && (episodeWatchedStyle === "hidden" || item.watchState === "watched" && episodeWatchedStyle !== "text"))
            progress = ""
        return [episode ? item.showtitle : item.year, episodeIdentity, progress, episode ? item.firstaired : "",
            item.runtime > 0 ? qsTr("%1 min").arg(Math.round(item.runtime / 60)) : ""].filter(value => value).join(" · ")
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
        if (command === inputManager.back) { if (details) details = null; else if (!kodiMedia.goBack() && !kodiMedia.busy) open = false; return }
        if (kodiMedia.busy) return
        if (hierarchyOpen && !details) {
            if (command === inputManager.confirm) {
                if (kodiMedia.items.length) kodiMedia.activate(Math.max(0, kodiMedia.selectedIndex))
                else kodiMedia.refresh()
            } else if (command === inputManager.menu && selectedItem) details = selectedItem
            else if (command === inputManager.navigateUp) kodiMedia.selectIndex(Math.max(0, kodiMedia.selectedIndex - hierarchyColumns))
            else if (command === inputManager.navigateDown) kodiMedia.selectIndex(kodiMedia.selectedIndex + hierarchyColumns)
            else if (command === inputManager.navigateLeft) {
                if (hierarchyColumns > 1 && kodiMedia.selectedIndex > 0) kodiMedia.selectIndex(kodiMedia.selectedIndex - 1)
                else kodiMedia.previousPage()
            } else if (command === inputManager.navigateRight) {
                if (hierarchyColumns > 1 && kodiMedia.selectedIndex + 1 < kodiMedia.items.length) kodiMedia.selectIndex(kodiMedia.selectedIndex + 1)
                else kodiMedia.nextPage()
            }
            return
        }
        if (command === inputManager.search && kodiMedia.mediaKind !== "seasons") { searchRequested(); return }
        if (details) {
            if (command === inputManager.confirm && nextUnwatchedFocused) findOrPlayNextEpisode()
            else if (command === inputManager.confirm && watchedActionFocused) changeWatched()
            else if (command === inputManager.confirm) { details = null; kodiMedia.activate(kodiMedia.selectedIndex) }
            else if (command === inputManager.navigateDown && !nextUnwatchedFocused) {
                if (canChangeWatched && !watchedActionFocused) watchedActionFocused = true
                else if (canFindNextEpisode) { watchedActionFocused = false; nextUnwatchedFocused = true }
            } else if (command === inputManager.navigateUp) {
                watchedActionFocused = nextUnwatchedFocused && canChangeWatched
                nextUnwatchedFocused = false
            }
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
                    MouseArea { anchors.fill: parent; enabled: !root.hierarchyOpen; onClicked: { kodiMedia.selectIndex(-1); root.toolbarIndex = index; root.toolbarAction(index) } }
                }
            }
        }
        Item {
            id: libraryBody
            width: parent.width; height: Math.max(100, root.height - 335)
        }
        Text { font.family: shell.fontFamily; objectName: "libraryStatusLine"; text: kodiMedia.status + (kodiMedia.playbackStatus ? " · " + kodiMedia.playbackStatus : "") + (kodiArtwork.status ? " · "+kodiArtwork.status:""); width: parent.width; elide: Text.ElideRight; color: !kodiMedia.busy && !kodiMedia.connected ? shell.backgroundError : shell.backgroundMuted; font.pixelSize: 15 * root.textScale }
        Text { font.family: shell.fontFamily; text: "OK open / play / resume    Menu details    ↑ Toolbar    ← → Pages    Back " + (kodiMedia.canGoBack ? "parent" : "Home"); color: shell.backgroundFocus; font.pixelSize: 16 * root.textScale; width: parent.width; elide: Text.ElideRight }
    }
    Rectangle {
        anchors.fill: parent
        visible: root.hierarchyOpen
        color: shell.scrim
        MouseArea { anchors.fill: parent }
        Rectangle {
            id: hierarchyDialog
            objectName: "libraryHierarchyDialog"
            anchors.fill: parent
            anchors.margins: root.hierarchyFullscreen ? 0 : Math.min(root.width, root.height) * 0.07
            color: shell.elevated
            radius: root.hierarchyFullscreen ? 0 : shell.radius
            Accessible.role: Accessible.Dialog
            Accessible.name: kodiMedia.browseTitle
            Column {
                anchors.fill: parent
                anchors.margins: Math.max(12, 20 * shell.gapScale)
                spacing: Math.max(6, 12 * shell.gapScale)
                Text {
                    id: hierarchyTitle
                    objectName: "libraryHierarchyTitle"
                    visible: root.hierarchyShowsHeader
                    width: parent.width
                    text: kodiMedia.browseTitle
                    textFormat: Text.PlainText
                    color: shell.elevatedText
                    font.family: shell.fontFamily
                    font.pixelSize: 32 * root.textScale
                    elide: Text.ElideRight
                }
                Item {
                    id: hierarchyBody
                    width: parent.width
                    height: Math.max(0, parent.height - hierarchyActions.height - parent.spacing
                                     - (hierarchyTitle.visible ? hierarchyTitle.height + parent.spacing : 0))
                }
                Row {
                    id: hierarchyActions
                    spacing: Math.max(8, 20 * shell.gapScale)
                    Repeater {
                        model: [
                            { label: qsTr("Back"), command: inputManager.back, enabled: true },
                            { label: qsTr("Previous page"), pageStep: -1, enabled: !kodiMedia.busy && kodiMedia.pageStart > 0 },
                            { label: qsTr("Next page"), pageStep: 1, enabled: !kodiMedia.busy && kodiMedia.hasNextPage },
                            { label: qsTr("Retry"), command: inputManager.confirm, enabled: !kodiMedia.busy && !kodiMedia.items.length }
                        ]
                        delegate: Text {
                            required property var modelData
                            visible: modelData.enabled
                            text: modelData.label
                            color: shell.elevatedFocus
                            font.family: shell.fontFamily
                            font.pixelSize: 18 * root.textScale
                            Accessible.role: Accessible.Button
                            Accessible.name: text
                            // Page buttons jump directly even when arrow keys are moving within a grid.
                            function activate() {
                                if (modelData.pageStep === -1) kodiMedia.previousPage()
                                else if (modelData.pageStep === 1) kodiMedia.nextPage()
                                else root.handleCommand(modelData.command)
                            }
                            Accessible.onPressAction: activate()
                            MouseArea { anchors.fill: parent; onClicked: parent.activate() }
                        }
                    }
                }
            }
        }
    }
    Row {
            parent: root.hierarchyOpen ? hierarchyBody : libraryBody
            anchors.fill: parent; spacing: 28 * shell.gapScale
            GridView {
                id: list; objectName: "libraryItems"
                width: root.hierarchyShowsArtwork ? parent.width * 0.61 : parent.width
                height: parent.height; clip: true
                cellWidth: width / root.hierarchyColumns
                cellHeight: Math.max(78, 72 * root.textScale) + 9 * shell.gapScale
                model: kodiMedia.items; currentIndex: kodiMedia.selectedIndex; highlightRangeMode: GridView.ApplyRange; highlightMoveDuration: 0
                delegate: Rectangle {
                    required property var modelData; required property int index
                    readonly property bool watchedBadge: root.hierarchyLevel === "episodes" && root.episodeWatchedCheckmark && modelData.watchState === "watched"
                    Accessible.role: Accessible.ListItem
                    Accessible.name: modelData.title || modelData.label || qsTr("Untitled")
                    Accessible.description: root.hierarchyLevel === "episodes" && root.episodeWatchedStyle !== "hidden" && modelData.watchState === "watched" ? qsTr("Watched") : ""
                    width: Math.max(0, GridView.view.cellWidth - (root.hierarchyColumns > 1 ? 8 * shell.gapScale : 0))
                    height: Math.max(78, 72 * root.textScale); radius: shell.radius; color: kodiMedia.selectedIndex === index ? shell.selected : shell.surface
                    border.width: kodiMedia.selectedIndex === index ? 2 : 0; border.color: (kodiMedia.selectedIndex === index) ? shell.selectedFocus : shell.focus
                    Column {
                        anchors.fill: parent; anchors.margins: 13; spacing: 5 * shell.gapScale
                        Row {
                            width: parent.width
                            spacing: 8 * shell.gapScale
                            Text { visible: watchedBadge; text: "✓"; color: kodiMedia.selectedIndex === index ? shell.selectedText : shell.text; font.pixelSize: 21 * root.textScale; Accessible.ignored: true }
                            Text { font.family: shell.fontFamily; width: parent.width - (watchedBadge ? 30 * root.textScale + parent.spacing : 0); text: modelData.title || modelData.label || qsTr("Untitled"); color: kodiMedia.selectedIndex === index ? shell.selectedText : shell.text; font.pixelSize: 21 * root.textScale; elide: Text.ElideRight }
                        }
                        Text { visible: root.hierarchyShowsMetadata; font.family: shell.fontFamily; width: parent.width; text: root.subtitle(modelData); color: (kodiMedia.selectedIndex === index) ? shell.selectedText : shell.muted; font.pixelSize: 15 * root.textScale; elide: Text.ElideRight }
                    }
                    MouseArea { anchors.fill: parent; enabled: !root.details && !kodiMedia.busy && !kodiMedia.playbackActive; acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: mouse => { kodiMedia.selectIndex(index); if (mouse.button === Qt.RightButton) root.details = modelData; else kodiMedia.activate(index) } }
                }
                Text { font.family: shell.fontFamily; anchors.centerIn: parent; width: parent.width - 32; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; visible: !kodiMedia.items.length
                    text: kodiMedia.busy ? qsTr("Loading from Kodi…") : kodiMedia.status + (root.hierarchyOpen ? qsTr("\n\nConfirm to retry · Back to return") : "\n\n↑ Toolbar · Refresh to retry · Back to return"); color: root.hierarchyOpen ? shell.elevatedMuted : shell.backgroundMuted; font.pixelSize: 21 * root.textScale }
            }
            Column {
                objectName: "libraryArtworkPreview"
                visible: root.hierarchyShowsArtwork
                width: visible ? Math.max(0, parent.width - list.width - parent.spacing) : 0
                height: parent.height; spacing: 12 * shell.gapScale
                Rectangle {
                    width: parent.width; height: Math.max(80, parent.height - 125); radius: shell.radius; color: shell.surface; clip: true
                    Image { anchors.fill: parent; anchors.margins: 1; source: kodiArtwork.source; fillMode: Image.PreserveAspectFit; sourceSize.width: 500; sourceSize.height: 650 }
                    Text { font.family: shell.fontFamily; anchors.centerIn: parent; width: parent.width - 30; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; visible: !kodiArtwork.source
                        text: root.selectedItem ? kodiArtwork.status || "Kodi library" : "Choose a title"; color: shell.unavailable; font.pixelSize: 18 * root.textScale }
                    Rectangle {
                        objectName: "episodeWatchedPosterOutline"
                        anchors.fill: parent
                        visible: root.hierarchyLevel === "episodes" && root.episodeWatchedOutline
                                 && !!root.selectedItem && root.selectedItem.watchState === "watched"
                        color: "transparent"
                        radius: shell.radius
                        border.color: root.episodeWatchedOutlineColor
                        border.width: 3
                    }
                }
                Text { font.family: shell.fontFamily; text: root.selectedItem ? root.selectedItem.title : ""; color: root.hierarchyOpen ? shell.elevatedText : shell.backgroundText; font.pixelSize: 23 * root.textScale; width: parent.width; maximumLineCount: 2; wrapMode: Text.WordWrap; elide: Text.ElideRight }
                Text { font.family: shell.fontFamily; text: root.selectedItem ? root.selectedItem.plot || root.subtitle(root.selectedItem) : ""; color: root.hierarchyOpen ? shell.elevatedMuted : shell.backgroundMuted; font.pixelSize: 16 * root.textScale; width: parent.width; maximumLineCount: 3; wrapMode: Text.WordWrap; elide: Text.ElideRight }
            }
    }
    Rectangle {
        objectName: "libraryDetailsPanel"
        anchors.fill: parent; anchors.margins: 35; color: shell.elevated; radius: shell.radius; visible: root.details !== null
        MouseArea { anchors.fill: parent }
        Column {
            anchors.fill: parent; anchors.margins: 35; spacing: 22 * shell.gapScale
            Text { font.family: shell.fontFamily; width: parent.width; text: root.details ? root.details.title : ""; color: shell.elevatedText; font.pixelSize: 34 * root.textScale; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
            Text { font.family: shell.fontFamily; width: parent.width; objectName: "libraryDetailsSubtitle"; text: root.subtitle(root.details); color: shell.elevatedFocus; font.pixelSize: 21 * root.textScale; wrapMode: Text.WordWrap }
            Text { font.family: shell.fontFamily; width: parent.width; text: root.details ? root.details.plot || "No description supplied by Kodi." : ""; color: shell.elevatedText; font.pixelSize: 22 * root.textScale; wrapMode: Text.WordWrap; maximumLineCount: Math.max(2, Math.floor((root.height - (root.canFindNextEpisode ? 450 : 340)) / (28 * root.textScale))); elide: Text.ElideRight }
                    Text { font.family: shell.fontFamily; width: parent.width; text: root.details && root.details.mediaType === "tvshow" ? "OK browse · Menu curate · Back close"
                        : root.details && (root.details.mediaType === "season" || root.details.mediaType === "album") ? "OK browse · Back close"
                        : root.details && root.details.mediaType === "song" ? "OK play · Back close\nDuring playback: Menu controls · Back stop · Home return"
                    : "OK play / resume · Right restart · Menu curate · Back close\nDuring playback: OK pause · Left/Right seek · Back stop · Home return"; color: shell.elevatedMuted; font.pixelSize: 20 * root.textScale; wrapMode: Text.WordWrap }
            Text { font.family: shell.fontFamily; objectName: "libraryDetailsPlayback"; text: kodiMedia.playbackStatus; color: shell.elevatedFocus; font.pixelSize: 18 * root.textScale }
            Rectangle {
                visible: root.canChangeWatched
                width: Math.min(parent.width, watchedLabel.implicitWidth + 24)
                height: watchedLabel.implicitHeight + 16
                radius: shell.radius
                color: root.watchedActionFocused ? shell.selected : shell.elevated
                border.width: root.watchedActionFocused ? 2 : 0
                border.color: shell.selectedFocus
                Accessible.role: Accessible.Button
                Accessible.name: watchedLabel.text
                Accessible.onPressAction: root.changeWatched()
                Text {
                    id: watchedLabel
                    anchors.centerIn: parent
                    text: root.details && root.details.watchState === "watched" ? qsTr("Mark unwatched") : qsTr("Mark watched")
                    font.family: shell.fontFamily
                    font.pixelSize: 18 * root.textScale
                    color: root.watchedActionFocused ? shell.selectedText : shell.elevatedText
                }
                MouseArea { anchors.fill: parent; onClicked: root.changeWatched() }
            }
            Rectangle {
                visible: root.canFindNextEpisode
                width: Math.min(parent.width, nextEpisodeLabel.implicitWidth + 24)
                height: nextEpisodeLabel.implicitHeight + 16
                radius: shell.radius
                color: root.nextUnwatchedFocused ? shell.selected : shell.elevated
                border.width: root.nextUnwatchedFocused ? 2 : 0
                border.color: shell.selectedFocus
                Accessible.role: Accessible.Button
                Accessible.name: nextEpisodeLabel.text
                Accessible.onPressAction: root.findOrPlayNextEpisode()
                Text {
                    id: nextEpisodeLabel
                    anchors.centerIn: parent
                    text: root.canFindNextEpisode && kodiMedia.nextUnwatchedCandidateId > 0
                        ? qsTr("Play unwatched episode") : qsTr("Find first unwatched episode in this season")
                    font.family: shell.fontFamily
                    font.pixelSize: 18 * root.textScale
                    color: root.nextUnwatchedFocused ? shell.selectedText : shell.elevatedText
                }
                MouseArea { anchors.fill: parent; onClicked: { root.watchedActionFocused = false; root.nextUnwatchedFocused = true; root.findOrPlayNextEpisode() } }
            }
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
