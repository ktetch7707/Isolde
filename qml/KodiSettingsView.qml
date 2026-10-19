import QtQuick

Item {
    id: root
    property var appearance: ({})
    readonly property color accentColor: appearance.focusColor || "#C994A5"
    readonly property color surfaceColor: appearance.surfaceColor || "#172238"
    readonly property color selectedColor: appearance.selectedColor || "#353039"
    readonly property color textColor: appearance.textColor || "#F5F1F3"
    readonly property color mutedColor: appearance.mutedColor || "#AAA5AF"
    readonly property int cardRadius: Math.round(4 + 12 * (appearance.tileRoundness === undefined ? 0.2 : appearance.tileRoundness))
    property int selectedIndex: 0
    property bool editingFolders: false
    function back() {
        if (!editingFolders) return false
        if (!folderPage.back()) editingFolders = false
        return true
    }
    onSelectedIndexChanged: {
        const row = actions.itemAt(selectedIndex)
        if (row) scroll.contentY = Math.max(0, Math.min(scroll.contentHeight - scroll.height, row.y - 20))
    }
    function handleCommand(command) {
        if (editingFolders) { folderPage.handleCommand(command); return }
        if (command === inputManager.navigateUp) selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown) selectedIndex = Math.min(7, selectedIndex + 1)
        else if (command === inputManager.confirm) {
            if (kodiService.switching) return
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
            else if (selectedIndex === 7) searchPreferences.toggleWebEnabled()
            else kodiMedia.refresh()
        }
    }
    Rectangle { anchors.fill: parent; color: root.appearance.backgroundEnd || "#101014" }
    Flickable {
      id: scroll
      visible: !root.editingFolders
      anchors.fill: parent
      contentHeight: contents.implicitHeight
      clip: true
      boundsBehavior: Flickable.StopAtBounds
      Column {
        id: contents
        width: parent.width; spacing: 20
        Text { text: "MEDIA LIBRARY"; color: root.textColor; font.pixelSize: 30 }
        Text { width: parent.width; wrapMode: Text.WordWrap; text: "Keep Kodi ready behind Isolde for " + profileManager.activeName + ". The outgoing Kodi instance closes before another profile starts."; color: root.mutedColor; font.pixelSize: 20 }
        Text { width: parent.width; wrapMode: Text.WordWrap; text: kodiService.status; color: root.accentColor; font.pixelSize: 22 }
        Repeater {
            id: actions
            model: ["Automatic startup: " + (kodiService.enabled ? "On" : "Off"), "Restart Kodi background service", "Scan for New Movies", "Refresh library connection", "Frontend: " + (kodiService.frontend === "isolde" ? "Isolde (preview) — select for Kodi" : "Kodi application — select for Isolde"), "Open Kodi with your existing setup", "Media folder access · " + kodiService.libraryFolders.length + " allowed", "Web search: " + (searchPreferences.webEnabled ? "ON — no provider configured, Kodi results only" : "OFF — Kodi library only")]
            delegate: Rectangle {
                required property int index
                required property string modelData
                objectName: "mediaLibraryActionRow"
                width: parent.width; height: 80; radius: root.cardRadius
                color: root.selectedIndex === index ? root.selectedColor : root.surfaceColor
                border.width: root.selectedIndex === index ? 2 : 0
                border.color: root.accentColor
                opacity: kodiService.switching || (index === 5 && kodiService.frontend !== "kodi") || (index === 2 && (!kodiMedia.connected || kodiMedia.busy)) || (index === 3 && kodiMedia.busy) ? 0.45 : 1
                Text { anchors.fill: parent; anchors.margins: 18; text: modelData; color: root.textColor; font.pixelSize: 21; wrapMode: Text.WordWrap }
                MouseArea { anchors.fill: parent; onClicked: { root.selectedIndex = index; root.handleCommand(inputManager.confirm) } }
            }
        }
        Text { width: parent.width; wrapMode: Text.WordWrap; text: root.selectedIndex >= 4 ? kodiService.status : (kodiMedia.scanStatus || kodiMedia.status); color: root.accentColor; font.pixelSize: 18 }
        Text { width: parent.width; wrapMode: Text.WordWrap; text: "Both frontends use this Isolde profile's same Kodi home. Switching does not replace your skin, add-ons, sources or library. Exit Kodi normally before switching back. An existing desktop Kodi setup is separate: importing it safely is not implemented yet."; color: root.mutedColor; font.pixelSize: 17 }
        Text { width: parent.width; wrapMode: Text.WordWrap; text: "Put new movies in your existing movie folder, then scan here. Kodi uses this profile's configured video sources and content settings (including subfolders when enabled). This does not move, rename, import or clean up files. Use Media folder access below to allow a collection, then configure its movie content once in Kodi. Startup alone does not grant file access."; color: root.mutedColor; font.pixelSize: 17 }
      }
    }
    MediaFoldersView { id: folderPage; objectName: "mediaFolderAccessPage"; anchors.fill: parent; visible: root.editingFolders; appearance: root.appearance }
}
