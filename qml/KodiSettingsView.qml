import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    property var appearance: ({})
    property int selectedIndex: 0
    property bool editingFolders: false
    property bool showingImport: false
    property bool showingPlayback: false
    function back() {
        if (showingPlayback) { showingPlayback=false; return true }
        if (showingImport) { if (!importPage.back()) showingImport = false; return true }
        if (!editingFolders) return false
        if (!folderPage.back()) editingFolders = false
        return true
    }
    onSelectedIndexChanged: {
        const row = actions.itemAt(selectedIndex)
        if (row) scroll.contentY = Math.max(0, Math.min(scroll.contentHeight - scroll.height, row.y - 20))
    }
    function handleCommand(command) {
        if (showingPlayback) {
            if(command===inputManager.confirm)kodiService.setAlwaysPlayLocally(!kodiService.alwaysPlayLocally)
            return
        }
        if (showingImport) { importPage.handleCommand(command); return }
        if (editingFolders) { folderPage.handleCommand(command); return }
        if (command === inputManager.navigateUp) selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown) selectedIndex = Math.min(actions.count-1, selectedIndex + 1)
        else if (command === inputManager.confirm) {
            if (kodiService.switching) return
            if (selectedIndex === 9) { showingPlayback=true; return }
            if (selectedIndex === 8) { showingImport = true; return }
            if (selectedIndex === 6) editingFolders = true
            else if (selectedIndex === 4) kodiService.setFrontend(kodiService.frontend === "isolde" ? "kodi" : "isolde")
            else if (selectedIndex === 5) {
                if (kodiService.frontend !== "kodi") return
                sessionManager.goHome()
                sessionManager.launch("kodi")
            }
            else if (selectedIndex === 0) kodiService.setEnabled(!kodiService.enabled)
            else if (selectedIndex === 1) kodiService.retry()
            else if (selectedIndex === 2) kodiMedia.scanMovies()
            else if (selectedIndex === 3) kodiMedia.refresh()
            else if (selectedIndex === 7) searchPreferences.toggleWebEnabled()
            else kodiMedia.refresh()
        }
    }
    Rectangle { anchors.fill: parent; color: shell.surface }
    Flickable {
    id: scroll
    visible: !root.editingFolders && !root.showingImport && !root.showingPlayback
    anchors.fill: parent
    contentHeight: contents.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    Column {
        id: contents
        width: parent.width; spacing: 20 * shell.gapScale
        Text { font.family: shell.fontFamily; text: "MEDIA LIBRARY"; color: shell.text; font.pixelSize: 30 * shell.textScale }
        Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; text: "Keep Kodi ready behind Isolde for this profile. The outgoing Kodi instance closes before another profile starts."; color: shell.muted; font.pixelSize: 20 * shell.textScale }
        Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; text: kodiService.status; color: shell.focus; font.pixelSize: 22 * shell.textScale }
        Repeater {
            id: actions
            model: ["Automatic startup: " + (kodiService.enabled ? "On" : "Off"), "Restart Kodi background service", "Scan for New Movies", "Refresh library connection", "Frontend: " + (kodiService.frontend === "isolde" ? "Isolde (preview) — select for Kodi" : "Kodi application — select for Isolde"), "Open Kodi with your existing setup", "Media folder access · " + kodiService.libraryFolders.length + " allowed", "Web search: " + (searchPreferences.webEnabled ? "ON — no provider configured, Kodi results only" : "OFF — Kodi library only"), "Import existing desktop Kodi · backup/preview/import/rollback", "Playback Preferences"]
            delegate: Rectangle {
                required property int index
                required property string modelData
                objectName: "mediaLibraryActionRow"
                width: parent.width; height: 80; radius: shell.radius
                color: root.selectedIndex === index ? shell.selected : shell.surface
                border.width: root.selectedIndex === index ? 2 : 0
                border.color: root.selectedIndex === index ? shell.selectedFocus : shell.focus
                opacity: kodiService.switching || (index === 5 && kodiService.frontend !== "kodi") || (index === 2 && (!kodiMedia.connected || kodiMedia.busy)) || (index === 3 && kodiMedia.busy) ? 0.45 : 1
                Text { font.family: shell.fontFamily; anchors.fill: parent; anchors.margins: 18; text: modelData; color: (root.selectedIndex === index) ? shell.selectedText : shell.text; font.pixelSize: 21 * shell.textScale; wrapMode: Text.WordWrap }
                MouseArea { anchors.fill: parent; onClicked: { root.selectedIndex = index; root.handleCommand(inputManager.confirm) } }
            }
        }
        Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; text: root.selectedIndex >= 4 ? kodiService.status : (kodiMedia.scanStatus || kodiMedia.status); color: shell.focus; font.pixelSize: 18 * shell.textScale }
        Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; text: "Both frontends use this Isolde profile's same Kodi home. Switching does not replace your skin, add-ons, sources or library. Exit Kodi normally before switching back."; color: shell.muted; font.pixelSize: 17 * shell.textScale }
        Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; text: "Put new movies in your existing movie folder, then scan here. Kodi uses this profile's configured video sources and content settings (including subfolders when enabled). This does not move, rename, import or clean up files. Use Media folder access below to allow a collection, then configure its movie content once in Kodi. Startup alone does not grant file access."; color: shell.muted; font.pixelSize: 17 * shell.textScale }
    }
    }
    Rectangle {
        anchors.fill:parent;visible:root.showingPlayback;color:shell.surface
        Column {
            anchors.fill:parent;spacing:24*shell.gapScale
            Text {width:parent.width;text:"PLAYBACK PREFERENCES";color:shell.text;font.family:shell.fontFamily;font.pixelSize:30*shell.textScale;wrapMode:Text.Wrap}
            Rectangle {
                width:parent.width;height:110;color:shell.selected;radius:shell.radius
                border.width:3;border.color:shell.selectedFocus
                Text {anchors.fill:parent;anchors.margins:18;text:"Always Play Locally When Available: "+(kodiService.alwaysPlayLocally?"On":"Off");color:shell.selectedText;font.family:shell.fontFamily;font.pixelSize:22*shell.textScale;wrapMode:Text.Wrap;verticalAlignment:Text.AlignVCenter}
                MouseArea {anchors.fill:parent;onClicked:root.handleCommand(inputManager.confirm)}
            }
            Text {width:parent.width;text:"On plays verified local matches from discovery immediately. Off shows viewing choices. Library shelves always play directly through Kodi. Enter toggles · Back returns.";color:shell.muted;font.family:shell.fontFamily;font.pixelSize:20*shell.textScale;wrapMode:Text.Wrap}
            Text {width:parent.width;text:kodiService.status;color:shell.muted;font.family:shell.fontFamily;font.pixelSize:18*shell.textScale;wrapMode:Text.Wrap}
        }
    }
    KodiImportView { id: importPage; objectName: "kodiImportPage"; anchors.fill: parent; visible: root.showingImport; appearance: root.appearance }
    MediaFoldersView { id: folderPage; objectName: "mediaFolderAccessPage"; anchors.fill: parent; visible: root.editingFolders; appearance: root.appearance }
}
