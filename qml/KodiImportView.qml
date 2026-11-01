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
    property string pendingAction: ""

    function back() {
        if (pendingAction.length) { pendingAction = ""; return true }
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
            else if (selectedIndex === 3) { if (pendingAction === "import") kodiImporter.import(); else pendingAction = "import" }
            else if (selectedIndex === 4) { if (pendingAction === "rollback") kodiImporter.rollback(); else pendingAction = "rollback" }
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
    onVisibleChanged: if (!visible) { pendingAction = ""; selectedIndex = 0 }

    Rectangle { anchors.fill: parent; color: root.appearance.backgroundEnd || "#101014" }
    Flickable {
        id: scroll
        anchors.fill: parent
        contentHeight: contents.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Column {
            id: contents
            width: parent.width; spacing: 20
            Text { text: "KODI DESKTOP IMPORT"; color: root.textColor; font.pixelSize: 30 }
            Text { width: parent.width; wrapMode: Text.WordWrap; text: "Import your existing desktop Kodi setup (skin, sources, add-ons, watch/resume state). Isolde preserves Kodi's configuration by reference and never edits Kodi databases or media files."; color: root.mutedColor; font.pixelSize: 18 }
            Text { width: parent.width; wrapMode: Text.WordWrap; text: kodiImporter.importStatus; color: root.accentColor; font.pixelSize: 20 }

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
                    width: parent.width; height: 74; radius: root.cardRadius
                    color: root.selectedIndex === index ? root.selectedColor : root.surfaceColor
                    border.width: root.selectedIndex === index ? 2 : 0
                    border.color: root.accentColor
                    opacity: kodiImporter.importInProgress && index !== 7 ? 0.45 : 1
                    Text { anchors.fill: parent; anchors.margins: 18; text: modelData; color: root.textColor; font.pixelSize: 19; wrapMode: Text.WordWrap }
                    MouseArea { anchors.fill: parent; onClicked: { root.selectedIndex = index; root.handleCommand(inputManager.confirm) } }
                }
            }

            Text { width: parent.width; wrapMode: Text.WordWrap; text: kodiImporter.previewText; color: root.textColor; font.pixelSize: 17; visible: kodiImporter.previewText.length > 0 }
            Text { width: parent.width; wrapMode: Text.WordWrap; text: "Conflicts: " + kodiImporter.conflicts.join("  "); color: "#FFB4A9"; font.pixelSize: 16; visible: kodiImporter.conflicts.length > 0 }

            Text { width: parent.width; wrapMode: Text.WordWrap; text: "Backup: " + (kodiImporter.backupAvailable ? kodiImporter.backupPath : "none"); color: root.mutedColor; font.pixelSize: 15; visible: kodiImporter.backupAvailable }
            Text { width: parent.width; wrapMode: Text.WordWrap; text: "Rollback available: " + (kodiImporter.canRollback ? "Yes" : "No"); color: root.mutedColor; font.pixelSize: 15 }

            Text { width: parent.width; wrapMode: Text.WordWrap; text: "Isolde owns presentation and navigation. Kodi owns media, watched state, resume state, artwork, and playback. Importing desktop Kodi settings does not move, rename, or clean up your media files."; color: root.mutedColor; font.pixelSize: 16 }
        }
    }
}