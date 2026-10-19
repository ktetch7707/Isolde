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
    property bool browsing: false
    property int selectedIndex: 0
    property string pendingRemoval: ""
    readonly property var rows: browsing
        ? ["Up / storage locations", "Allow this folder read-only"].concat(kodiService.folderChoices)
        : ["Add media folder…"].concat(kodiService.libraryFolders)
    function resetSelection() { selectedIndex = 0; scroll.contentY = 0; pendingRemoval = "" }
    function back() {
        if (pendingRemoval.length) { pendingRemoval = ""; return true }
        if (browsing) { browsing = false; resetSelection(); return true }
        return false
    }
    function handleCommand(command) {
        if (command === inputManager.navigateUp) { pendingRemoval = ""; selectedIndex = Math.max(0, selectedIndex - 1) }
        else if (command === inputManager.navigateDown) { pendingRemoval = ""; selectedIndex = Math.min(rows.length - 1, selectedIndex + 1) }
        else if (command === inputManager.confirm) {
            if (browsing) {
                if (selectedIndex === 0) { kodiService.browseLibraryParent(); resetSelection() }
                else if (selectedIndex === 1) kodiService.allowBrowsedFolder()
                else { kodiService.browseLibraryFolder(kodiService.folderChoices[selectedIndex - 2]); resetSelection() }
            } else if (selectedIndex === 0) {
                browsing = true; kodiService.browseLibraryFolder(""); resetSelection()
            } else {
                const path = kodiService.libraryFolders[selectedIndex - 1]
                if (pendingRemoval === path) { kodiService.removeLibraryFolder(path); resetSelection() }
                else pendingRemoval = path
            }
        }
    }
    onSelectedIndexChanged: {
        const row = items.itemAt(selectedIndex)
        if (row) scroll.contentY = Math.max(0, Math.min(scroll.contentHeight - scroll.height, row.y - 20))
    }
    onVisibleChanged: if (!visible) { browsing = false; resetSelection() }
    Rectangle { anchors.fill: parent; color: root.appearance.backgroundEnd || "#101014" }
    Flickable {
        id: scroll; anchors.fill: parent; contentHeight: contents.implicitHeight; clip: true
        Column {
            id: contents; width: parent.width; spacing: 16
            Text { text: "MEDIA FOLDER ACCESS"; color: root.textColor; font.pixelSize: 28 }
            Text { width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 18; color: root.mutedColor
                text: browsing ? (kodiService.browsePath || "Choose a storage location") : "Allow Kodi to read an existing media folder and all its subfolders. Both frontends use the same paths. This grants access; it does not reorganize files or change Kodi sources." }
            Text { width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 18; color: root.accentColor
                text: kodiService.canEditFolders ? "Read-only access. Select an allowed folder twice to remove permission." : "To change access: switch to Kodi application mode, then exit Kodi normally." }
            Repeater {
                id: items; model: root.rows
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    objectName: "mediaFolderActionRow"
                    width: parent.width; height: label.implicitHeight + 32; radius: root.cardRadius
                    color: root.selectedIndex === index ? root.selectedColor : root.surfaceColor
                    border.width: root.selectedIndex === index ? 2 : 0
                    border.color: root.accentColor
                    Text { id: label; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16
                        text: root.pendingRemoval === modelData ? "Remove permission? Press OK again: " + modelData : modelData
                        wrapMode: Text.WrapAnywhere; color: root.textColor; font.pixelSize: 19 }
                    MouseArea { anchors.fill: parent; onClicked: { root.selectedIndex = index; root.handleCommand(inputManager.confirm) } }
                }
            }
            Text { width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 18; color: root.accentColor; text: kodiService.folderStatus }
            Text { width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 17; color: root.mutedColor
                text: "First setup: Open Kodi → Videos → Files → Add videos. Select the allowed folder at its original path; choose Movies and enable recursive scanning. Use your preferred scraper or Local information only. Existing Kodi sources keep their settings. Then return to Isolde mode and Scan for New Movies. Missing drives do not erase library records." }
        }
    }
}
