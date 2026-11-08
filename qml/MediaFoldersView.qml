import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    property var appearance: ({})
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
    Rectangle { anchors.fill: parent; color: shell.surface }
    Flickable {
        id: scroll; anchors.fill: parent; contentHeight: contents.implicitHeight; clip: true
        Column {
            id: contents; width: parent.width; spacing: 16 * shell.gapScale
            Text { font.family: shell.fontFamily; text: "MEDIA FOLDER ACCESS"; color: shell.text; font.pixelSize: 28 * shell.textScale }
            Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 18 * shell.textScale; color: shell.muted
                text: browsing ? (kodiService.browsePath || "Choose a storage location") : "Allow Kodi to read an existing media folder and all its subfolders. Both frontends use the same paths. This grants access; it does not reorganize files or change Kodi sources." }
            Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 18 * shell.textScale; color: shell.focus
                text: kodiService.canEditFolders ? "Read-only access. Select an allowed folder twice to remove permission." : "To change access: switch to Kodi application mode, then exit Kodi normally." }
            Repeater {
                id: items; model: root.rows
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    objectName: "mediaFolderActionRow"
                    width: parent.width; height: label.implicitHeight + 32; radius: shell.radius
                    color: root.selectedIndex === index ? shell.selected : shell.surface
                    border.width: root.selectedIndex === index ? 2 : 0
                    border.color: root.selectedIndex === index ? shell.selectedFocus : shell.focus
                    Text { font.family: shell.fontFamily; id: label; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16
                        text: root.pendingRemoval === modelData ? "Remove permission? Press OK again: " + modelData : modelData
                        wrapMode: Text.WrapAnywhere; color: (root.selectedIndex === index) ? shell.selectedText : shell.text; font.pixelSize: 19 * shell.textScale }
                    MouseArea { anchors.fill: parent; onClicked: { root.selectedIndex = index; root.handleCommand(inputManager.confirm) } }
                }
            }
            Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 18 * shell.textScale; color: shell.focus; text: kodiService.folderStatus }
            Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 17 * shell.textScale; color: shell.muted
                text: "First setup: Open Kodi → Videos → Files → Add videos. Select the allowed folder at its original path; choose Movies and enable recursive scanning. Use your preferred scraper or Local information only. Existing Kodi sources keep their settings. Then return to Isolde mode and Scan for New Movies. Missing drives do not erase library records." }
        }
    }
}
