import QtQuick

Rectangle {
    id: root
    // Context menu for media items using typed action descriptors.
    // Shows actions relevant to the selected media item (play, resume, mark
    // watched/unwatched, browse) and dispatches them through ActionDispatcher.
    // D-pad operable; Confirm selects, Back closes without writing.
    property var dispatcher: null
    property string originId: ""
    property var item: null          // media item map with mediaType, mediaId, watchState, resume
    property bool visible: false
    property int selectedIndex: 0
    signal closed()
    signal actionActivated(var action)

    readonly property var actions: root.item?root.buildActions():[]
    readonly property int actionCount: root.actions.length

    function buildActions() {
        if (!root.item) return []
        const actions = []
        const mediaType = root.item.mediaType || root.item.kind || ""
        const mediaId = root.item.mediaId || root.item.id || 0
        const watchState = root.item.watchState || ""
        const hasResume = root.item.resume && root.item.resume.position > 0
        // Play / Resume
        if (mediaType === "movie" || mediaType === "episode" || mediaType === "song") {
            if (watchState === "watched" || hasResume)
                actions.push({type: "media.resume", mediaKind: mediaType, mediaId: mediaId, name: "Resume", symbol: "▶‖"})
            else
                actions.push({type: "media.play", mediaKind: mediaType, mediaId: mediaId, name: "Play", symbol: "▶"})
        }
        // Mark watched / unwatched
        if (mediaType === "movie" || mediaType === "episode") {
            if (watchState === "watched")
                actions.push({type: "media.markWatched", mediaKind: mediaType, mediaId: mediaId, watched: false, name: "Mark unwatched", symbol: "○"})
            else
                actions.push({type: "media.markWatched", mediaKind: mediaType, mediaId: mediaId, watched: true, name: "Mark watched", symbol: "✓"})
        }
        // Browse (for shows/collections)
        if (mediaType === "show" || mediaType === "collection")
            actions.push({type: "open-collection", mediaKind: mediaType, mediaId: mediaId, name: "Browse", symbol: "≡"})
        // Always allow cancel
        actions.push({type: "cancel", name: "Back", symbol: "⌫"})
        return actions
    }

    function refresh() { selectedIndex = Math.max(0, Math.min(selectedIndex, Math.max(0, actionCount - 1))) }

    function handleCommand(command) {
        if (!root.visible || actionCount === 0) return
        if (command === inputManager.back) { root.visible = false; root.closed(); return }
        if (command === inputManager.navigateUp) selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown) selectedIndex = Math.min(actionCount - 1, selectedIndex + 1)
        else if (command === inputManager.confirm) {
            if (selectedIndex >= 0 && selectedIndex < actionCount) {
                const action = root.actions[selectedIndex]
                if (action.type === "cancel") { root.visible = false; root.closed(); return }
                root.visible = false
                if (root.dispatcher) root.dispatcher.dispatch(action, root.originId)
                root.actionActivated(action)
            }
        }
    }

    visible: root.visible && actionCount > 0
    color: "#3A3740"
    radius: 12
    border.width: 2
    border.color: shell.focus
    Column {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 6
        Repeater {
            model: root.actions
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: parent.width
                height: 56
                radius: 8
                color: root.selectedIndex === index ? shell.selected : "transparent"
                border.width: root.selectedIndex === index ? 2 : 0
                border.color: shell.selectedFocus
                Row {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12
                    Text { text: modelData.symbol; color: root.selectedIndex === index ? shell.selectedText : shell.text; font.pixelSize: 20 }
                    Text { text: modelData.name; color: root.selectedIndex === index ? shell.selectedText : shell.text; font.pixelSize: 18; width: parent.width - 60; elide: Text.ElideRight }
                }
                MouseArea { anchors.fill: parent; onClicked: { root.selectedIndex = index; root.handleCommand(inputManager.confirm) } }
            }
        }
    }
    SurfacePalette { id: shell; appearance: root.appearance || ({}) }
    property var appearance: ({})
}
