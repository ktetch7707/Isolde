import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    property var appearance: ({})
    property int selectedIndex: 0
    property string pendingAction: ""

    function back() {
        if (kodiImporter.importInProgress) { kodiImporter.cancel(); return true }
        if (pendingAction.length) { pendingAction = ""; kodiImporter.releasePreview(); return true }
        return false
    }
    function handleCommand(command) {
        if (command === inputManager.navigateUp) { pendingAction = ""; selectedIndex = Math.max(0, selectedIndex - 1) }
        else if (command === inputManager.navigateDown) { pendingAction = ""; selectedIndex = Math.min(7, selectedIndex + 1) }
        else if (command === inputManager.confirm) {
            if (kodiImporter.importInProgress) return
            if (selectedIndex === 0) kodiImporter.scanExistingKodi()
            else if (selectedIndex === 1) kodiImporter.createBackup()
            else if (selectedIndex === 2) kodiImporter.previewImport()
            else if (selectedIndex === 3) {
                if (pendingAction === "import") { pendingAction = ""; kodiImporter["import"](); }
                else pendingAction = "import"
            }
            else if (selectedIndex === 4) {
                if (pendingAction === "rollback") { pendingAction = ""; kodiImporter.rollback(); }
                else pendingAction = "rollback"
            }
            else if (selectedIndex === 5) kodiImporter.clearConflicts()
            else if (selectedIndex === 6) kodiMedia.refresh()
            else if (selectedIndex === 7) sessionManager.goHome()
        }
        else if (command === inputManager.back) back()
    }
    onSelectedIndexChanged: {
        const row = actions.itemAt(selectedIndex)
        if (row) scroll.contentY = Math.max(0, Math.min(scroll.contentHeight - scroll.height, row.y - 20))
    }
    onVisibleChanged: if (!visible) { pendingAction = ""; selectedIndex = 0; kodiImporter.releasePreview() }

    Rectangle { anchors.fill: parent; color: shell.surface }
    Flickable {
        id: scroll
        anchors.fill: parent
        contentHeight: contents.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Column {
            id: contents
            width: parent.width; spacing: 20 * shell.gapScale
            Text { font.family: shell.fontFamily; text: "KODI DESKTOP IMPORT"; color: shell.text; font.pixelSize: 30 * shell.textScale }
            Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; text: "Import your existing desktop Kodi setup (skin, sources, add-ons, watch/resume state). A verified copy is installed into this Isolde profile. The original desktop configuration and media stay unchanged. Rollback restores this profile's previous configuration. Stop playback before continuing."; color: shell.muted; font.pixelSize: 18 * shell.textScale }
            Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; textFormat: Text.PlainText; text: kodiImporter.importStatus; color: shell.focus; font.pixelSize: 20 * shell.textScale }

            Repeater {
                id: actions
                model: [
                    "1 · Scan existing Kodi setup",
                    "2 · Create backup before import",
                    "3 · Preview what will be imported",
                    root.pendingAction === "import" ? "4 · CONFIRM import (press again)" : "4 · Import settings",
                    root.pendingAction === "rollback" ? "5 · CONFIRM rollback (press again)" : "5 · Rollback from backup",
                    "6 · Clear conflict notices",
                    "7 · Refresh library connection",
                    "8 · Return to Isolde Home"
                ]
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    objectName: "kodiImportActionRow"
                    width: parent.width; height: 74; radius: shell.radius
                    color: root.selectedIndex === index ? shell.selected : shell.surface
                    border.width: root.selectedIndex === index ? 2 : 0
                    border.color: root.selectedIndex === index ? shell.selectedFocus : shell.focus
                    opacity: kodiImporter.importInProgress && index !== 7 ? 0.45 : 1
                    Text { font.family: shell.fontFamily; anchors.fill: parent; anchors.margins: 18; text: modelData; color: (root.selectedIndex === index) ? shell.selectedText : shell.text; font.pixelSize: 19 * shell.textScale; wrapMode: Text.WordWrap }
                    MouseArea { anchors.fill: parent; onClicked: { root.selectedIndex = index; root.handleCommand(inputManager.confirm) } }
                }
            }

            Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; textFormat: Text.PlainText; text: kodiImporter.previewText; color: shell.text; font.pixelSize: 17 * shell.textScale; visible: kodiImporter.previewText.length > 0 }
            Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; textFormat: Text.PlainText; text: "Conflicts: " + kodiImporter.conflicts.join("  "); color: shell.error; font.pixelSize: 16 * shell.textScale; visible: kodiImporter.conflicts.length > 0 }

            Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; text: "Backup: " + (kodiImporter.backupAvailable ? kodiImporter.backupPath : "none"); color: shell.muted; font.pixelSize: 15 * shell.textScale; visible: kodiImporter.backupAvailable }
            Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; text: "Rollback available: " + (kodiImporter.canRollback ? "Yes" : "No"); color: shell.muted; font.pixelSize: 15 * shell.textScale }

            Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; text: "Isolde owns presentation and navigation. Kodi owns media, watched state, resume state, artwork, and playback. Importing desktop Kodi settings does not move, rename, or clean up your media files."; color: shell.muted; font.pixelSize: 16 * shell.textScale }
        }
    }
}
