import QtQuick

Rectangle {
    id: root
    objectName: "skinPicker"
    required property var layoutModel
    property var appearance: ({})
    property bool open: false
    property int selected: 0
    property int operation: 0
    property bool confirming: false
    property string message: ""
    property bool detailsOpen: false
    property var previewModel: null
    readonly property var previewPage: previewModel && previewModel.homePageId
        ? previewModel.pageModel(previewModel.homePageId) || previewModel : previewModel
    signal closed()
    signal editSkin(string id)
    signal addSkin()
    readonly property var entries: layoutModel.skins || []
    readonly property var choice: entries[selected] || ({})
    readonly property var operations: [qsTr("Use this skin"), qsTr("Edit Home and pages"), qsTr("Duplicate skin"), qsTr("Delete skin")]
    SurfacePalette { id: skinTheme; appearance: root.appearance }
    color: skinTheme.background; visible: open
    onVisibleChanged: if (!visible && detailsOpen) clearPreview()
    // Clear transient choices whenever Settings opens the picker.
    function begin() { clearPreview(); selected = 0; operation = 0; confirming = false; namingVisible = false; message = ""; open = true }
    // Retire the scratch model whenever the details pane closes.
    function clearPreview() {
        detailsOpen = false
        previewModel = null
        layoutModel.closeSkinPreview()
    }
    // Inspect the chosen skin before offering any action that could change Home.
    function selectSkin() {
        if (selected === entries.length) { open = false; addSkin(); return }
        operation = 0
        message = ""
        previewModel = layoutModel.openSkinPreview(choice.id)
        if (!previewModel) message = layoutModel.errorString
        detailsOpen = true
    }
    // Apply and edit remain deliberate actions after opening the preview.
    function execute() {
        message = ""
        if (operation === 0) {
            if (layoutModel.applySkin(choice.id)) { message = "Skin applied."; open = false; closed() }
            else { message = layoutModel.errorString; return }
        } else if (operation === 1) {
            open = false; editSkin(choice.id)
        } else if (operation === 2) {
            keyboard.value = choice.name + " copy"; keyboard.selectedIndex = 0; namingVisible = true
        } else if (operation === 3) {
            if (choice.builtIn) { message = "Built-in skins cannot be deleted."; return }
            confirming = true
        }
    }
    property bool namingVisible: false
    // Keep the keyboard available on failure so the name can be corrected.
    function confirmDuplicate(text) {
        if (!text.trim()) { message = "Give the skin a name."; return }
        const id = layoutModel.saveSkin(text.trim())
        if (id) { message = "Skin saved as \"" + text.trim() + "\"."; namingVisible = false }
        else { message = layoutModel.errorString; return }
    }
    // Delete only after explicit confirmation and keep errors visible in the picker.
    function confirmDelete() {
        const ok = layoutModel.deleteSkin(choice.id)
        confirming = false
        selected = Math.min(selected, Math.max(0, entries.length - 1))
        message = ok ? "Skin deleted." : layoutModel.errorString
        if (ok) { open = false; closed() }
    }
    // Delegate to the active overlay so remote input cannot change the underlying selection.
    function handleCommand(command) {
        if (namingVisible) { keyboard.handleCommand(command); return }
        if (confirming) {
            if (command === inputManager.back) confirming = false
            else if (command === inputManager.confirm) confirmDelete()
            return
        }
        if (detailsOpen) {
            if (command === inputManager.back || command === inputManager.navigateLeft) clearPreview()
            else if (command === inputManager.navigateUp) operation = Math.max(0, operation - 1)
            else if (command === inputManager.navigateDown) operation = Math.min(operations.length - 1, operation + 1)
            else if (command === inputManager.confirm) execute()
            return
        }
        if (command === inputManager.back) { open = false; closed(); return }
        else if (command === inputManager.navigateUp) selected = Math.max(0, selected - 1)
        else if (command === inputManager.navigateDown) {
            const last = entries.length > 0 ? entries.length : 0
            selected = Math.min(last, selected + 1)
        }
        else if (command === inputManager.confirm || command === inputManager.navigateRight) selectSkin()
    }
    MouseArea { anchors.fill: parent }
    Row {
        anchors.fill: parent
        anchors.margins: 40
        spacing: 24 * skinTheme.gapScale
        Column {
            width: Math.min(parent.width * 0.36, 360 * skinTheme.textScale)
            height: parent.height
            spacing: 14
            Text { text: qsTr("Skins"); color: skinTheme.backgroundText; font.family: skinTheme.fontFamily; font.pixelSize: 34 * skinTheme.textScale }
            ListView {
                id: skinList
                width: parent.width
                height: parent.height - y
                spacing: 8
                clip: true
                model: root.entries.concat([{name: qsTr("Add new skin")}])
                currentIndex: root.selected
                highlightRangeMode: ListView.ApplyRange
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: skinList.width
                    height: 64 * skinTheme.textScale
                    radius: skinTheme.radius
                    color: index === root.selected ? skinTheme.selected : skinTheme.surface
                    border.width: index === root.selected && !root.detailsOpen ? 2 : 0
                    border.color: skinTheme.selectedFocus
                    Text {
                        anchors.fill: parent
                        anchors.margins: 14
                        verticalAlignment: Text.AlignVCenter
                        text: modelData.name
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        color: index === root.selected ? skinTheme.selectedText : skinTheme.text
                        font.family: skinTheme.fontFamily
                        font.pixelSize: 22 * skinTheme.textScale
                    }
                    MouseArea { anchors.fill: parent; onClicked: { root.selected = index; root.selectSkin() } }
                }
            }
        }
        Column {
            width: Math.max(0, parent.width - x)
            height: parent.height
            spacing: 10 * skinTheme.gapScale
            visible: root.detailsOpen
            Text { width: parent.width; text: root.choice.name || ""; textFormat: Text.PlainText; elide: Text.ElideRight; color: skinTheme.backgroundText; font.family: skinTheme.fontFamily; font.pixelSize: 28 * skinTheme.textScale }
            Item {
                id: previewFrame
                objectName: "skinPreviewFrame"
                readonly property var viewport: root.layoutModel.studioOptions.referenceViewport || ({})
                readonly property real previewWidth: viewport.width || 1280
                readonly property real previewHeight: viewport.height || 720
                visible: !!root.previewPage
                width: Math.min(parent.width, parent.height * 0.52 * previewWidth / previewHeight)
                height: width * previewHeight / previewWidth
                clip: true
                Screensaver {
                    width: previewFrame.previewWidth
                    height: previewFrame.previewHeight
                    scale: previewFrame.width / width
                    transformOrigin: Item.TopLeft
                    enabled: false
                    pageModel: root.detailsOpen ? root.previewPage : null
                    artworkResolver: root.previewModel
                    reducedMotion: true
                }
            }
            Text {
                width: parent.width
                text: root.message || (root.previewPage ? qsTr("Home preview") : qsTr("Preview unavailable"))
                wrapMode: Text.Wrap
                color: skinTheme.backgroundMuted
                font.family: skinTheme.fontFamily
                font.pixelSize: 17 * skinTheme.textScale
            }
            ListView {
                id: actionList
                width: parent.width
                height: Math.max(0, parent.height - y)
                spacing: 4 * skinTheme.gapScale
                clip: true
                model: root.operations
                currentIndex: root.operation
                highlightRangeMode: ListView.ApplyRange
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    width: actionList.width
                    height: 46 * skinTheme.textScale
                    radius: skinTheme.radius
                    color: index === root.operation ? skinTheme.selected : skinTheme.surface
                    border.width: index === root.operation ? 2 : 0
                    border.color: skinTheme.selectedFocus
                    Text {
                        anchors.fill: parent
                        anchors.margins: 14
                        verticalAlignment: Text.AlignVCenter
                        text: modelData
                        color: index === root.operation ? skinTheme.selectedText : skinTheme.text
                        font.family: skinTheme.fontFamily
                        font.pixelSize: 20 * skinTheme.textScale
                    }
                    MouseArea { anchors.fill: parent; onClicked: { root.operation = index; root.execute() } }
                }
            }
        }
    }
    Rectangle {
        anchors.fill: parent
        visible: root.confirming
        color: skinTheme.background
        MouseArea { anchors.fill: parent }
        Column {
            anchors.centerIn: parent
            width: parent.width * 0.75
            spacing: 20
            Text { width: parent.width; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: skinTheme.backgroundText; font.pixelSize: 28 * skinTheme.textScale; text: qsTr("Delete “%1”?").arg(root.choice.name || "") }
            Text { width: parent.width; wrapMode: Text.Wrap; color: skinTheme.backgroundMuted; font.pixelSize: 20 * skinTheme.textScale; text: qsTr("Confirm to delete this saved skin. Back cancels.") }
            Text {
                text: qsTr("Confirm")
                color: skinTheme.backgroundText
                font.pixelSize: 22 * skinTheme.textScale
                MouseArea { anchors.fill: parent; onClicked: root.confirmDelete() }
            }
        }
    }
    OnScreenKeyboard {
        id: keyboard
        anchors.fill: parent
        anchors.margins: 50
        visible: root.namingVisible
        onAccepted: text => root.confirmDuplicate(text)
        onCanceled: root.namingVisible = false
    }
}
