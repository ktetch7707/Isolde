import QtQuick
import QtQuick.Dialogs
import QtQuick.Controls

Item {
    id: root
    property var layoutModel: homeLayout
    signal pageRequested(string id)
    required property var items
    required property var availableApps
    required property var artwork
    required property var themes
    property int selectedIndex: 0
    property int actionIndex: 0
    property int themeIndex: 0
    property bool roundnessEditing: false
    property bool opacityEditing: false
    property bool categoryEditing: false
    property bool addTileEditing: false
    property bool artworkEditing: false
    property bool themePreviewEditing: false
    property bool sourceRepairEditing: false
    property bool spanEditing: false
    property bool gridCollisionEditing: false
    property string message: ""
    property string spanTargetId: ""
    property int spanWidth: 1
    property int spanHeight: 1
    property var collisionPlacement: ({})
    property var collisionTargets: []
    property int collisionChoice: 0
    property string repairItemId: ""
    property var repairChoices: []
    property int repairIndex: 0
    readonly property bool editing: studio.open || pageManager.open || skinLibrary.open || designEditor.open || componentEditor.open || appearanceEditor.open || sourceRepairEditing || addTileEditing || artworkEditing || themePreviewEditing || opacityEditing || categoryEditing || roundnessEditing || spanEditing || gridCollisionEditing
    property int addTileActionIndex: 0
    property int addTileOpenWithIndex: 0
    property int addTileFieldIndex: 0
    property int addTileKeyboardField: 0
    property bool addTileKeyboardOpen: false
    property bool categoryAssignMode: true
    property int selectedCategory: 0
    property int categoryAction: 0
    readonly property var categoryActions: ["Create", "Rename", "Delete", "Move earlier", "Move later", "Assign", "Done"]
    property int artworkIndex: 0
    property int themePreviewIndex: 0
    property bool themeApplyArtwork: false
    onVisibleChanged: if (!visible) { if (appearanceEditor.open) appearanceEditor.close(); componentEditor.open = false; skinLibrary.open = false; designEditor.open = false; pageManager.open = false; spanEditing = false; gridCollisionEditing = false }
    readonly property var actions: ["Move earlier", "Move later", "Change size", "Remove item", "Show / hide title", "Show / hide type", "Show / hide logo", "Logo fills tile", "Choose built-in art", "Choose image", "Remove custom PNG", "Toggle Isolde name", "Preview/apply theme", "Assign category", "Manage categories", "Item opacity", "Tile roundness", "Add clock", "Add tile", "Restore defaults", "Repair / change source", "Make it yours", "Component layout", root.layoutModel.isPage ? "Page skins" : "Home skins", root.layoutModel.isPage ? "Design page" : "Design Home", "Pages", "Add Movies widget", "Add Shows widget", "Add text widget", "Add button", "Duplicate", "Page Studio (preview)"]
    readonly property var addTileActions: [
        { type: "open-folder", label: "Open folder", hint: "Folder path, e.g. /media/movies" },
        { type: "open-file", label: "Open file", hint: "File path, e.g. /media/trailers/movie.mp4" },
        { type: "open-url", label: "Open URL", hint: "URL, e.g. https://example.com" },
        { type: "open-disc", label: "Open disc", hint: "Disc target, e.g. /dev/sr0" },
        { type: "switch-input", label: "HDMI input", hint: "Input number 1–8" }
    ]
    readonly property var openWithOptions: {
        let result = []
        const selectedAction = addTileActions[addTileActionIndex] ? addTileActions[addTileActionIndex].type : ""
        if (selectedAction === "switch-input") return [{ id: "isolde-capture", name: "Isolde capture", mode: "Built-in" }]
        for (let i = 0; i < availableApps.length; ++i) {
            const app = availableApps[i]
            if (app.type === "store" || app.type === "settings")
                continue
            const handled = app.handledActions || []
            if (handled.indexOf(selectedAction) < 0)
                continue
            result.push({ id: app.id, name: app.name, mode: app.presentationMode })
        }
        return result
    }

    onActionIndexChanged: Qt.callLater(revealAction)
    function revealAction() {
        const top = Math.floor(actionIndex / 3) * 81
        if (top < actionScroll.contentY) actionScroll.contentY = top
        else if (top + 72 > actionScroll.contentY + actionScroll.height)
            actionScroll.contentY = Math.max(0, top + 72 - actionScroll.height)
    }

    onAddTileActionIndexChanged: addTileOpenWithIndex = 0
    function handleCommand(command) {
        if (studio.open) { studio.handleCommand(command); return }
        if (pageManager.open) { pageManager.handleCommand(command); return }
        if (skinLibrary.open) { skinLibrary.handleCommand(command); return }
        if (designEditor.open) { designEditor.handleCommand(command); return }
        if (componentEditor.open) { componentEditor.handleCommand(command); return }
        if (appearanceEditor.open) { appearanceEditor.handleCommand(command); return }
        if (sourceRepairEditing) {
            if (command === inputManager.back) sourceRepairEditing = false
            else if (command === inputManager.navigateUp) repairIndex = Math.max(0, repairIndex - 1)
            else if (command === inputManager.navigateDown) repairIndex = Math.min(repairChoices.length - 1, repairIndex + 1)
            else if (command === inputManager.confirm && repairChoices[repairIndex]) {
                if (root.layoutModel.rebindSource(repairItemId, repairChoices[repairIndex].id)) sourceRepairEditing = false
            }
            return
        }
        if (addTileKeyboardOpen) { addTileKeyboard.handleCommand(command); return }
        if (addTileEditing) {
            if (command === inputManager.navigateUp) addTileFieldIndex = Math.max(0, addTileFieldIndex - 1)
            else if (command === inputManager.navigateDown) addTileFieldIndex = Math.min(4, addTileFieldIndex + 1)
            else if (command === inputManager.navigateLeft && addTileFieldIndex === 2) addTileActionIndex = Math.max(0, addTileActionIndex - 1)
            else if (command === inputManager.navigateRight && addTileFieldIndex === 2) addTileActionIndex = Math.min(addTileActions.length - 1, addTileActionIndex + 1)
            else if (command === inputManager.navigateLeft && addTileFieldIndex === 3) addTileOpenWithIndex = openWithOptions.length ? Math.max(0, addTileOpenWithIndex - 1) : 0
            else if (command === inputManager.navigateRight && addTileFieldIndex === 3) addTileOpenWithIndex = openWithOptions.length ? Math.min(openWithOptions.length - 1, addTileOpenWithIndex + 1) : 0
            else if (command === inputManager.confirm && addTileFieldIndex < 2) editAddTileField(addTileFieldIndex)
            else if (command === inputManager.confirm && addTileFieldIndex === 4) commitAddTile()
            else if (command === inputManager.back) addTileEditing = false
            return
        }
        if (artworkEditing) {
            if (command === inputManager.navigateUp) artworkIndex = Math.max(0, artworkIndex - 1)
            else if (command === inputManager.navigateDown) artworkIndex = Math.min(artwork.length - 1, artworkIndex + 1)
            else if (command === inputManager.confirm) commitArtwork()
            else if (command === inputManager.back) artworkEditing = false
            return
        }
        if (themePreviewEditing) {
            if (command === inputManager.navigateUp) themePreviewIndex = Math.max(0, themePreviewIndex - 1)
            else if (command === inputManager.navigateDown) themePreviewIndex = Math.min(themes.length - 1, themePreviewIndex + 1)
            else if (command === inputManager.navigateLeft || command === inputManager.navigateRight) themeApplyArtwork = !themeApplyArtwork
            else if (command === inputManager.confirm) commitTheme()
            else if (command === inputManager.back) themePreviewEditing = false
            return
        }
        if (opacityEditing) {
            if (command === inputManager.navigateLeft) opacitySlider.value = Math.max(0, opacitySlider.value - 0.05)
            else if (command === inputManager.navigateRight) opacitySlider.value = Math.min(1, opacitySlider.value + 0.05)
            else if (command === inputManager.confirm || command === inputManager.back) opacityEditing = false
            return
        }
        if (categoryEditing) {
            if (categoryName.activeFocus) return
            if (command === inputManager.navigateUp) selectedCategory = Math.max(0, selectedCategory - 1)
            else if (command === inputManager.navigateDown) selectedCategory = Math.min(root.layoutModel.categories.length, selectedCategory + 1)
            else if (command === inputManager.navigateLeft) categoryAction = Math.max(0, categoryAction - 1)
            else if (command === inputManager.navigateRight) categoryAction = Math.min(categoryActions.length - 1, categoryAction + 1)
            else if (command === inputManager.confirm) applyCategoryAction()
            else if (command === inputManager.back) categoryEditing = false
            return
        }
        if (roundnessEditing) {
            if (command === inputManager.navigateLeft)
                roundnessSlider.value = Math.max(0, roundnessSlider.value - 0.05)
            else if (command === inputManager.navigateRight)
                roundnessSlider.value = Math.min(1, roundnessSlider.value + 0.05)
            else if (command === inputManager.confirm || command === inputManager.back)
                roundnessEditing = false
            return
        }
        if (spanEditing) {
            const grid = root.layoutModel.grid
            if (command === inputManager.navigateUp) { spanHeight = Math.max(1, spanHeight - 1); spanHeight = Math.min(spanHeight, grid.rows) }
            else if (command === inputManager.navigateDown) { spanHeight = Math.min(grid.rows, spanHeight + 1) }
            else if (command === inputManager.navigateLeft) { spanWidth = Math.max(1, spanWidth - 1); spanWidth = Math.min(spanWidth, grid.columns) }
            else if (command === inputManager.navigateRight) { spanWidth = Math.min(grid.columns, spanWidth + 1) }
            else if (command === inputManager.confirm) {
                const placement = { x: 0, y: 0, width: spanWidth, height: spanHeight }
                if (placement.width < 1 || placement.height < 1 || placement.width > grid.columns || placement.height > grid.rows) { root.message = "Span exceeds grid bounds."; return }
                const collisions = root.layoutModel.gridCollisions(spanTargetId, placement)
                if (collisions.length > 0) {
                    root.collisionPlacement = placement
                    root.collisionTargets = collisions
                    root.gridCollisionEditing = true
                    root.spanEditing = false
                    return
                }
                if (!root.layoutModel.setItemPlacement(spanTargetId, placement)) { root.message = root.layoutModel.errorString; return }
                root.spanEditing = false; root.message = ""
            }
            else if (command === inputManager.back) spanEditing = false
            return
        }
        if (gridCollisionEditing) {
            if (command === inputManager.navigateLeft) collisionChoice = Math.max(0, collisionChoice - 1)
            else if (command === inputManager.navigateRight) collisionChoice = Math.min(2, collisionChoice + 1)
            else if (command === inputManager.confirm) {
                if (collisionChoice === 0) root.applySpanWithShift()
                else if (collisionChoice === 1) root.applySpanWithSwap()
                else { root.gridCollisionEditing = false; root.spanEditing = false; root.message = "" }
            }
            else if (command === inputManager.back) { root.gridCollisionEditing = false; root.spanEditing = false; root.message = "" }
            return
        }
        if (command === inputManager.navigateUp)
            selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown)
            selectedIndex = Math.min(items.length - 1, selectedIndex + 1)
        else if (command === inputManager.navigateLeft)
            actionIndex = Math.max(0, actionIndex - 1)
        else if (command === inputManager.navigateRight)
            actionIndex = Math.min(actions.length - 1, actionIndex + 1)
        else if (command === inputManager.confirm)
            applyAction()
    }

    function applyAction() {
        if (actionIndex === 31) { studio.show("", items[selectedIndex] ? items[selectedIndex].id : ""); return }
        if(actionIndex === 26 || actionIndex === 27){root.layoutModel.addLibraryWidget(actionIndex === 26?"Movies":"Shows",actionIndex === 26?"movies":"shows");return}
        if(actionIndex === 29){root.layoutModel.addButton();return}
        if(actionIndex === 28){root.layoutModel.addWidget("text");return}
        if (actionIndex === 25) { pageManager.begin(); return }
        if (actionIndex === 23) { skinLibrary.begin(); return }
        if (actionIndex === 24) { designEditor.begin(); return }
        if (actionIndex === 22) { componentEditor.open = true; return }
        if (actionIndex === 21) { appearanceEditor.begin(); return }
        if (actionIndex === 20 && items.length) {
            repairItemId = items[selectedIndex].id
            repairChoices = root.layoutModel.sourceChoices(repairItemId)
            repairIndex = 0; sourceRepairEditing = true; return
        }
        if (actionIndex === 13) { categoryAssignMode = true; categoryAction = 5; selectedCategory = 0; categoryEditing = true; return }
        if (actionIndex === 14) { categoryAssignMode = false; categoryAction = 0; selectedCategory = 0; categoryEditing = true; return }
        if (actionIndex === 15 && items.length) { opacityEditing = true; opacitySlider.value = items[selectedIndex].opacity === undefined ? 1.0 : items[selectedIndex].opacity; return }
        if (actionIndex === 16) { roundnessEditing = true; roundnessSlider.value = root.layoutModel.tileRoundness; return }
        if (actionIndex === 17) { root.layoutModel.addWidget("clock"); selectedIndex = items.length - 1; return }
        if (actionIndex === 18) { addTileEditing = true; addTileActionIndex = 0; addTileOpenWithIndex = 0; addTileFieldIndex = 0; addTileKeyboardOpen = false; addTileName.text = ""; addTileTarget.text = ""; return }
        if (actionIndex === 19) { root.layoutModel.resetToDefaults(); selectedIndex = 0; return }
        if (!items.length) return
        const item = items[Math.min(selectedIndex, items.length - 1)]
        if (actionIndex === 0) { root.layoutModel.moveItem(item.id, -1); selectedIndex = Math.max(0, selectedIndex - 1) }
        else if (actionIndex === 1) { root.layoutModel.moveItem(item.id, 1); selectedIndex = Math.min(items.length - 1, selectedIndex + 1) }
        else if (actionIndex === 2) {
            spanTargetId = item.id
            const grid = root.layoutModel.grid
            const current = item.placement || ({})
            spanWidth = current.width || 1
            spanHeight = current.height || 1
            spanEditing = true
        } else if (actionIndex === 30) {
            if (!root.layoutModel.duplicateItem(item.id)) { root.message = root.layoutModel.errorString; return }
            selectedIndex = Math.min(selectedIndex + 1, items.length - 1)
        } else if (actionIndex === 3) { root.layoutModel.removeItem(item.id); selectedIndex = Math.max(0, Math.min(selectedIndex, items.length - 2)) }
        else if (actionIndex === 4 && item.kind !== "widget") root.layoutModel.setTileOption(item.id, "showTitle", item.showTitle === false)
        else if (actionIndex === 5 && item.kind !== "widget") root.layoutModel.setTileOption(item.id, "showType", item.showType === false)
        else if (actionIndex === 6 && item.kind !== "widget") root.layoutModel.setTileOption(item.id, "showLogo", item.showLogo === false)
        else if (actionIndex === 7 && item.kind !== "widget") root.layoutModel.setTileOption(item.id, "logoFill", item.logoFill !== true)
        else if (actionIndex === 8 && item.kind !== "widget" && artwork.length) { artworkIndex = 0; artworkEditing = true }
        else if (actionIndex === 9 && item.kind !== "widget") logoPicker.open()
        else if (actionIndex === 10 && item.kind !== "widget") root.layoutModel.clearTileLogo(item.id)
        else if (actionIndex === 11) root.layoutModel.setBrandVisible(!root.layoutModel.brandVisible)
        else if (actionIndex === 12 && themes.length) { themePreviewIndex = 0; themeApplyArtwork = false; themePreviewEditing = true }
    }

    function commitArtwork() {
        if (!items.length || !artwork.length) return
        const item = items[Math.min(selectedIndex, items.length - 1)]
        const art = artwork[Math.max(0, Math.min(artworkIndex, artwork.length - 1))]
        if (item.kind === "widget" || !art) return
        if (root.layoutModel.setTileAppearance(item.id, art.symbol, art.accent))
            artworkEditing = false
    }

    function commitTheme() {
        const theme = themes[Math.max(0, Math.min(themePreviewIndex, themes.length - 1))]
        if (theme && root.layoutModel.applyTheme(theme, themeApplyArtwork))
            themePreviewEditing = false
    }

    function commitAddTile() {
        const action = addTileActions[addTileActionIndex]
        const opener = openWithOptions[Math.max(0, Math.min(addTileOpenWithIndex, openWithOptions.length - 1))]
        if (!action || !opener || !addTileName.text.trim() || !addTileTarget.text.trim()) return
        const openWith = action.type === "switch-input" ? "" : opener.id
        const ok = root.layoutModel.addActionTile(addTileName.text, action.type, addTileTarget.text, "", openWith)
        if (ok) {
            addTileEditing = false
            selectedIndex = Math.max(0, items.length)
        }
    }

    function editAddTileField(field) {
        addTileKeyboardField = field
        addTileKeyboardOpen = true
        addTileKeyboard.selectedIndex = 0
        addTileKeyboard.value = field === 0 ? addTileName.text : addTileTarget.text
    }

    function applyCategoryAction() {
        const categoryId = selectedCategory === 0 ? "" : root.layoutModel.categories[selectedCategory - 1].id
        if (categoryAction === 0) { categoryName.text = ""; categoryName.placeholderText = "New category name"; categoryName.forceActiveFocus() }
        else if (categoryAction === 1 && categoryId) { categoryName.text = root.layoutModel.categories[selectedCategory - 1].name; categoryName.selectAll(); categoryName.placeholderText = "Rename category"; categoryName.forceActiveFocus() }
        else if (categoryAction === 2 && categoryId) { root.layoutModel.removeCategory(categoryId); selectedCategory = Math.min(selectedCategory, root.layoutModel.categories.length) }
        else if (categoryAction === 3 && categoryId) { root.layoutModel.moveCategory(categoryId, -1); selectedCategory = Math.max(1, selectedCategory - 1) }
        else if (categoryAction === 4 && categoryId) { root.layoutModel.moveCategory(categoryId, 1); selectedCategory = Math.min(root.layoutModel.categories.length, selectedCategory + 1) }
        else if (categoryAction === 5 && items.length) { root.layoutModel.assignItemCategory(items[selectedIndex].id, categoryId); categoryEditing = false }
        else if (categoryAction === 6) categoryEditing = false
    }

    function commitCategoryName() {
        if (!categoryName.text.trim()) { categoryName.focus = false; return }
        if (categoryAction === 0) root.layoutModel.addCategory(categoryName.text)
        else if (categoryAction === 1 && selectedCategory > 0) root.layoutModel.renameCategory(root.layoutModel.categories[selectedCategory - 1].id, categoryName.text)
        categoryName.focus = false
    }

    function findEmptyCell(width, height, excludeId, reserved) {
        const grid = root.layoutModel.grid
        const columns = grid.columns, rows = grid.rows
        for (let y = 0; y + height <= rows; ++y) {
            for (let x = 0; x + width <= columns; ++x) {
                const trial = { x: x, y: y, width: width, height: height }
                if (reserved && x < reserved.x + reserved.width && reserved.x < x + width
                    && y < reserved.y + reserved.height && reserved.y < y + height) continue
                let ok = true
                for (let i = 0; i < root.items.length; ++i) {
                    const it = root.items[i]
                    if (it.id === excludeId || !it.placement) continue
                    const p = it.placement
                    if (x < p.x + p.width && p.x < x + width && y < p.y + p.height && p.y < y + height) { ok = false; break }
                }
                if (ok) return trial
            }
        }
        return null
    }

    function applySpanWithShift() {
        const placement = root.collisionPlacement
        const targetId = root.spanTargetId
        const collidingId = root.collisionTargets[0] ? root.collisionTargets[0].id : ""
        if (!collidingId) { root.gridCollisionEditing = false; root.spanEditing = false; root.message = ""; return }
        // Move the colliding item to the target's old placement if that cell is free
        let targetOld = null
        for (let i = 0; i < root.items.length; ++i) {
            if (root.items[i].id === targetId) { targetOld = root.items[i].placement || {x:0,y:0,width:1,height:1}; break }
        }
        if (targetOld && root.layoutModel.gridCollisions(collidingId, targetOld).length === 0) {
            root.layoutModel.setItemPlacement(collidingId, targetOld)
        } else {
            // Try any empty cell that fits the colliding item
            const ci = root.items.find(i => i.id === collidingId)
            const oldP = ci ? ci.placement : null
            if (oldP) {
                const empty = findEmptyCell(oldP.width, oldP.height, collidingId, placement)
                if (empty) root.layoutModel.setItemPlacement(collidingId, empty)
            }
        }
        if (!root.layoutModel.setItemPlacement(targetId, placement)) { root.message = root.layoutModel.errorString; return }
        root.gridCollisionEditing = false; root.spanEditing = false; root.message = ""
    }

    function applySpanWithSwap() {
        const placement = root.collisionPlacement
        const targetId = root.spanTargetId
        const collidingId = root.collisionTargets[0] ? root.collisionTargets[0].id : ""
        if (!collidingId) { root.gridCollisionEditing = false; root.spanEditing = false; root.message = ""; return }
        // Swap: target takes colliding's old placement, colliding gets the desired new placement
        let collidingOld = null
        for (let i = 0; i < root.items.length; ++i) {
            if (root.items[i].id === collidingId) { collidingOld = root.items[i].placement || {x:0,y:0,width:1,height:1}; break }
        }
        if (collidingOld) {
            root.layoutModel.setItemPlacement(targetId, collidingOld)
        }
        if (!root.layoutModel.setItemPlacement(collidingId, placement)) { root.message = root.layoutModel.errorString; return }
        root.gridCollisionEditing = false; root.spanEditing = false; root.message = ""
    }

    AppearanceEditor { id: appearanceEditor; anchors.fill: parent; layoutModel: root.layoutModel; z: 50 }
    ComponentLayoutEditor { id: componentEditor; anchors.fill: parent; layoutModel: root.layoutModel; z: 51 }
    SkinLibrary { id: skinLibrary; anchors.fill: parent; layoutModel: root.layoutModel; z: 52 }
    PageManager { id:pageManager; anchors.fill:parent; layoutModel:homeLayout; hostModel:root.layoutModel; availableApps:root.availableApps; artwork:root.artwork; themes:root.themes; z:54; onPageRequested:id => root.pageRequested(id) }
    HomeDesignEditor { id: designEditor; anchors.fill: parent; layoutModel: root.layoutModel; z: 53 }

    PageStudioView { id: studio; anchors.fill: parent; layoutModel: homeLayout; z: 100 }

    FileDialog {
        id: logoPicker
        title: "Choose artwork"
        nameFilters: ["Images (*.png *.jpg *.jpeg)"]
        onAccepted: if (root.items.length) root.layoutModel.setTileLogo(root.items[root.selectedIndex].id, selectedFile.toString())
    }

    Rectangle {
        anchors.fill: parent; z: 40; visible: root.sourceRepairEditing; color: "#F0080C18"
        Column {
            anchors.fill: parent; anchors.margins: 54; spacing: 18
            Text { text: "CHOOSE SOURCE"; color: "#F8FAFC"; font.pixelSize: 32 }
            Text { width: parent.width; wrapMode: Text.WordWrap; color: "#AAB5C9"; font.pixelSize: 20
                text: root.repairChoices.length ? "Choose a compatible source. The tile keeps its position and appearance. OK applies · Back cancels."
                    : "No compatible sources are available. Install an app or create a local collection first, then return here. Back cancels." }
            ListView {
                width: parent.width; height: Math.max(80, parent.height - 180); clip: true; spacing: 10
                model: root.repairChoices; currentIndex: root.repairIndex
                delegate: Rectangle {
                    required property int index; required property var modelData
                    width: ListView.view.width; height: 64; radius: 12
                    color: index === root.repairIndex ? "#2B416B" : "#172238"
                    Text { anchors.fill: parent; anchors.margins: 16; text: modelData.name; color: "#F8FAFC"; font.pixelSize: 22; elide: Text.ElideRight }
                    MouseArea { anchors.fill: parent; onClicked: { root.repairIndex = index; root.handleCommand(inputManager.confirm) } }
                }
            }
            Text { text: root.layoutModel.errorString; color: "#FCA5A5"; font.pixelSize: 18 }
        }
    }

    Rectangle {
        id: themePreviewOverlay
        visible: root.themePreviewEditing
        anchors.fill: parent; color: "#E6050812"; z: 25
        readonly property var theme: root.themes[Math.max(0, Math.min(root.themePreviewIndex, root.themes.length - 1))] || {}
        Rectangle {
            anchors.centerIn: parent; width: Math.min(1040, parent.width - 90); height: Math.min(660, parent.height - 60)
            radius: 24; color: "#151E32"; border.width: 2; border.color: "#D58A45"
            Row {
                anchors.fill: parent; anchors.margins: 28; spacing: 24
                Column {
                    width: 330; height: parent.height; spacing: 14
                    Text { text: "THEMES"; color: "white"; font.pixelSize: 30; font.weight: Font.DemiBold; font.letterSpacing: 3 }
                    Text { text: "Choose a theme, preview it, then apply. Esc leaves your current Home unchanged."; color: "#AAB5C9"; font.pixelSize: 15; wrapMode: Text.WordWrap; width: parent.width }
                    Rectangle {
                        width: parent.width; height: 50; radius: 10
                        color: root.themeApplyArtwork ? "#26524F" : "#1D2B45"
                        border.width: 2; border.color: root.themeApplyArtwork ? "#A7F3D0" : "#93C5FD"
                        Text {
                            anchors.centerIn: parent
                            text: root.themeApplyArtwork ? "MODE: THEME + SUGGESTED ART" : "MODE: THEME ONLY"
                            color: "white"; font.pixelSize: 14; font.weight: Font.DemiBold; font.letterSpacing: 1.4
                        }
                    }
                    ListView {
                        width: parent.width; height: parent.height - 176; spacing: 8; clip: true
                        model: root.themes
                        currentIndex: root.themePreviewIndex
                        delegate: Rectangle {
                            required property int index; required property var modelData
                            width: ListView.view.width; height: 76; radius: 12
                            color: index === root.themePreviewIndex ? "#3A2D1C" : "#111A2A"
                            border.width: index === root.themePreviewIndex ? 3 : 1
                            border.color: index === root.themePreviewIndex ? "#F8FAFC" : "#2B3853"
                            Rectangle {
                                anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter
                                width: 46; height: 46; radius: 10
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: modelData.backgroundStart }
                                    GradientStop { position: 1.0; color: modelData.backgroundEnd }
                                }
                            }
                            Column {
                                anchors.left: parent.left; anchors.leftMargin: 72
                                anchors.right: parent.right; anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter; spacing: 3
                                Text { text: modelData.name; color: "#F8FAFC"; font.pixelSize: 18; font.weight: Font.DemiBold; width: parent.width; elide: Text.ElideRight }
                                Text { text: modelData.description; color: "#91A0BC"; font.pixelSize: 13; width: parent.width; elide: Text.ElideRight }
                            }
                            MouseArea { anchors.fill: parent; onClicked: root.themePreviewIndex = index }
                        }
                    }
                }
                Rectangle {
                    width: parent.width - 354; height: parent.height; radius: 18; clip: true
                    border.width: 2; border.color: "#31415F"
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: themePreviewOverlay.theme.backgroundStart || "#111A32" }
                        GradientStop { position: 1.0; color: themePreviewOverlay.theme.backgroundEnd || "#080C18" }
                    }
                    Column {
                        anchors.fill: parent; anchors.margins: 28; spacing: 20
                        Text {
                            text: (themePreviewOverlay.theme.name || "Theme Preview").toUpperCase()
                            color: "#F8FAFC"; font.pixelSize: 28; font.weight: Font.DemiBold; font.letterSpacing: 3
                        }
                        Text {
                            text: "Tile roundness " + Math.round((themePreviewOverlay.theme.tileRoundness || 0) * 100) + "%"
                            color: "#C8D3E5"; font.pixelSize: 15
                        }
                        Row {
                            spacing: 10
                            Text { text: "Focus"; color: "#C8D3E5"; font.pixelSize: 14; font.weight: Font.DemiBold }
                            Rectangle { width: 54; height: 18; radius: 9; color: themePreviewOverlay.theme.focusColor || "#F8FAFC" }
                            Text { text: "Type " + Math.round((themePreviewOverlay.theme.typographyScale || 1) * 100) + "%"; color: "#C8D3E5"; font.pixelSize: 14 }
                            Text { text: "Opacity " + Math.round((themePreviewOverlay.theme.tileOpacity || 1) * 100) + "%"; color: "#C8D3E5"; font.pixelSize: 14 }
                        }
                        Grid {
                            width: parent.width; columns: 2; spacing: 16
                            Repeater {
                                model: [
                                    { name: "Movies", section: "MEDIA", symbol: "◉", accent: "#06B6D4" },
                                    { name: "Steam", section: "GAME HUB", symbol: "S", accent: "#4F83A8" },
                                    { name: "Music", section: "AUDIO", symbol: "♪", accent: "#1DB954" },
                                    { name: "Settings", section: "SYSTEM", symbol: "⚙", accent: "#9B6BE3" }
                                ]
                                delegate: Rectangle {
                                    required property var modelData
                                    width: (parent.width - 16) / 2; height: 118
                                    radius: Math.min(width, height) * 0.5 * (themePreviewOverlay.theme.tileRoundness || 0.18)
                                    opacity: themePreviewOverlay.theme.tileOpacity || 1.0
                                    color: "#CC151E32"; border.width: 2; border.color: modelData.name === "Movies" ? (themePreviewOverlay.theme.focusColor || "#F8FAFC") : "#44536D"
                                    Rectangle {
                                        x: 18; y: 16; width: 48; height: 48
                                        radius: Math.min(width, height) * 0.25 * (themePreviewOverlay.theme.tileRoundness || 0.18)
                                        color: modelData.accent
                                        Text { anchors.centerIn: parent; text: modelData.symbol; color: "white"; font.pixelSize: 20; font.weight: Font.Bold }
                                    }
                                    Column {
                                        anchors.left: parent.left; anchors.leftMargin: 18
                                        anchors.right: parent.right; anchors.rightMargin: 18
                                        anchors.bottom: parent.bottom; anchors.bottomMargin: 16
                                        Text { text: modelData.name; color: "#F8FAFC"; font.pixelSize: 21; font.weight: Font.DemiBold }
                                        Text { text: modelData.section; color: "#91A0BC"; font.pixelSize: 11; font.letterSpacing: 2 }
                                    }
                                }
                            }
                        }
                        Rectangle {
                            width: parent.width; height: 58; radius: 12; color: "#D58A45"
                            Text { anchors.centerIn: parent; text: root.themeApplyArtwork ? "APPLY THEME + ART" : "APPLY THEME ONLY"; color: "white"; font.pixelSize: 18; font.weight: Font.Bold; font.letterSpacing: 2 }
                            MouseArea { anchors.fill: parent; onClicked: root.commitTheme() }
                        }
                        Text { text: "↑ ↓ choose theme    ← → apply mode    Enter apply    Esc cancel"; color: "#C8D3E5"; font.pixelSize: 15 }
                    }
                }
            }
        }
    }

    Rectangle {
        visible: root.artworkEditing
        anchors.fill: parent; color: "#E6050812"; z: 23
        Rectangle {
            anchors.centerIn: parent; width: Math.min(860, parent.width - 100); height: Math.min(620, parent.height - 80)
            radius: 24; color: "#151E32"; border.width: 2; border.color: "#14B8A6"
            Column {
                anchors.fill: parent; anchors.margins: 30; spacing: 16
                Text { text: "BUILT-IN ART"; color: "white"; font.pixelSize: 30; font.weight: Font.DemiBold; font.letterSpacing: 3 }
                Text { text: "Apply an Isolde-owned badge and color from the built-in Living Room Starter pack. Official or personal PNG artwork can still be chosen separately."; color: "#AAB5C9"; font.pixelSize: 16; wrapMode: Text.WordWrap; width: parent.width }
                ListView {
                    width: parent.width; height: parent.height - 142; spacing: 8; clip: true
                    model: root.artwork
                    currentIndex: root.artworkIndex
                    delegate: Rectangle {
                        required property int index; required property var modelData
                        width: ListView.view.width; height: 58; radius: 10
                        color: index === root.artworkIndex ? "#26524F" : "#111A2A"
                        border.width: index === root.artworkIndex ? 3 : 1
                        border.color: index === root.artworkIndex ? "#D1FAE5" : "#2B3853"
                        Rectangle {
                            width: 40; height: 40; radius: 10
                            anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter
                            color: modelData.accent
                            Text { anchors.centerIn: parent; text: modelData.symbol; color: "white"; font.pixelSize: 16; font.weight: Font.Bold }
                        }
                        Column {
                            anchors.left: parent.left; anchors.leftMargin: 66
                            anchors.right: parent.right; anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text { text: modelData.name; color: "#F8FAFC"; font.pixelSize: 18; font.weight: Font.DemiBold; width: parent.width; elide: Text.ElideRight }
                            Text { text: modelData.category + " · " + modelData.packId; color: "#91A0BC"; font.pixelSize: 13; width: parent.width; elide: Text.ElideRight }
                        }
                        MouseArea { anchors.fill: parent; onClicked: { root.artworkIndex = index; root.commitArtwork() } }
                    }
                }
                Text { text: "↑ ↓ choose artwork    Enter apply    Esc cancel"; color: "#91A0BC"; font.pixelSize: 15 }
            }
        }
    }

    Rectangle {
        visible: root.addTileEditing
        anchors.fill: parent; color: "#E6050812"; z: 24
        Rectangle {
            anchors.centerIn: parent; width: Math.min(960, parent.width - 100); height: Math.min(650, parent.height - 70)
            radius: 24; color: "#151E32"; border.width: 2; border.color: "#D58A45"
            Column {
                anchors.fill: parent; anchors.margins: 30; spacing: 16
                Text { text: "ADD TILE"; color: "white"; font.pixelSize: 30; font.weight: Font.DemiBold; font.letterSpacing: 3 }
                Text { text: "Create a permanent tile by assigning a safe action. Detected discs and drives can later use this same path when you choose Keep on Home."; color: "#AAB5C9"; font.pixelSize: 16; wrapMode: Text.WordWrap; width: parent.width }
                TextField {
                    id: addTileName; width: parent.width; height: 52; color: "white"; font.pixelSize: 18
                    placeholderText: "Tile name · press Enter to edit"
                    placeholderTextColor: "#71809F"
                    readOnly: true
                    background: Rectangle { radius: 9; color: "#0E1628"; border.width: root.addTileFieldIndex === 0 ? 2 : 1; border.color: root.addTileFieldIndex === 0 ? "#FFFFFF" : "#354462" }
                    MouseArea { anchors.fill: parent; onClicked: root.editAddTileField(0) }
                }
                TextField {
                    id: addTileTarget; width: parent.width; height: 52; color: "white"; font.pixelSize: 18
                    placeholderText: root.addTileActions[root.addTileActionIndex].hint
                    placeholderTextColor: "#71809F"
                    readOnly: true
                    background: Rectangle { radius: 9; color: "#0E1628"; border.width: root.addTileFieldIndex === 1 ? 2 : 1; border.color: root.addTileFieldIndex === 1 ? "#FFFFFF" : "#354462" }
                    MouseArea { anchors.fill: parent; onClicked: root.editAddTileField(1) }
                }
                Text { text: "OPEN WITH"; color: "#F8FAFC"; font.pixelSize: 17; font.weight: Font.DemiBold; font.letterSpacing: 2 }
                ListView {
                    width: parent.width; height: 112; spacing: 7; clip: true
                    model: root.openWithOptions
                    currentIndex: root.addTileOpenWithIndex
                    delegate: Rectangle {
                        required property int index; required property var modelData
                        width: ListView.view.width; height: 48; radius: 9
                        color: index === root.addTileOpenWithIndex ? "#2B416B" : "#111A2A"
                        border.width: index === root.addTileOpenWithIndex && root.addTileFieldIndex === 3 ? 2 : 1
                        border.color: index === root.addTileOpenWithIndex ? "#F8FAFC" : "#2B3853"
                        Text { anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 14; text: modelData.name + " · " + modelData.mode; color: "white"; font.pixelSize: 16 }
                        MouseArea { anchors.fill: parent; onClicked: { root.addTileFieldIndex = 3; root.addTileOpenWithIndex = index } }
                    }
                }
                Text {
                    visible: root.openWithOptions.length === 0
                    text: root.addTileActions[root.addTileActionIndex].type === "switch-input" ? "Uses Isolde's configured capture card and RS-232 switch." : "No installed app declares support for this action yet. Install or enable a compatible app first."
                    color: "#FCA5A5"; font.pixelSize: 15; wrapMode: Text.WordWrap; width: parent.width
                }
                Grid {
                    width: parent.width; columns: 4; spacing: 8
                    Repeater {
                        model: root.addTileActions
                        delegate: Rectangle {
                            required property int index; required property var modelData
                            width: (parent.width - 24) / 4; height: 78; radius: 10
                            color: index === root.addTileActionIndex ? "#D58A45" : "#202D47"
                            border.width: index === root.addTileActionIndex && root.addTileFieldIndex === 2 ? 2 : 0; border.color: "white"
                            Column {
                                anchors.centerIn: parent; width: parent.width - 14; spacing: 4
                                Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: modelData.label; color: "white"; font.pixelSize: 15; font.weight: Font.DemiBold; wrapMode: Text.WordWrap }
                                Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: modelData.type; color: "#FFE6C2"; font.pixelSize: 11 }
                            }
                            MouseArea { anchors.fill: parent; onClicked: { root.addTileFieldIndex = 2; root.addTileActionIndex = index } }
                        }
                    }
                }
                Rectangle {
                    width: parent.width; height: 58; radius: 12
                    color: addTileName.text.trim() && addTileTarget.text.trim() && root.openWithOptions.length > 0 ? "#D58A45" : "#35435A"
                    Text { anchors.centerIn: parent; text: "CREATE TILE"; color: "white"; font.pixelSize: 18; font.weight: Font.Bold; font.letterSpacing: 2 }
                    border.width: root.addTileFieldIndex === 4 ? 2 : 0; border.color: "white"
                    MouseArea { anchors.fill: parent; onClicked: { root.addTileFieldIndex = 4; root.commitAddTile() } }
                }
                Text { text: "↑ ↓ choose field    ← → change action    OK edit/create    Back cancel"; color: "#91A0BC"; font.pixelSize: 15 }
            }
        }
    }

    OnScreenKeyboard {
        id: addTileKeyboard
        anchors.fill: parent; anchors.margins: 42; z: 25; visible: root.addTileKeyboardOpen
        onCanceled: root.addTileKeyboardOpen = false
        onAccepted: text => {
            if (root.addTileKeyboardField === 0) addTileName.text = text
            else addTileTarget.text = text
            root.addTileKeyboardOpen = false
            root.addTileFieldIndex = root.addTileKeyboardField
        }
    }

    Rectangle {
        visible: root.roundnessEditing
        anchors.fill: parent
        color: "#CC050812"
        z: 20
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(700, parent.width - 120); height: 260
            radius: 24; color: "#151E32"; border.width: 2; border.color: "#6E55B8"
            Column {
                anchors.fill: parent; anchors.margins: 34; spacing: 22
                Text { text: "TILE ROUNDNESS"; color: "white"; font.pixelSize: 28; font.weight: Font.DemiBold }
                Slider {
                    id: roundnessSlider
                    width: parent.width; from: 0; to: 1; stepSize: 0.01
                    value: root.layoutModel.tileRoundness
                    onMoved: root.layoutModel.setTileRoundness(value)
                    onValueChanged: if (root.roundnessEditing) root.layoutModel.setTileRoundness(value)
                }
                Row {
                    width: parent.width
                    Text { width: parent.width / 2; text: "SQUARE  0%"; color: "#91A0BC"; font.pixelSize: 16 }
                    Text { width: parent.width / 2; horizontalAlignment: Text.AlignRight; text: "100%  FULLY ROUNDED"; color: "#91A0BC"; font.pixelSize: 16 }
                }
                Text { text: "← → adjust    Enter or Esc done    " + Math.round(roundnessSlider.value * 100) + "%"; color: "#F8FAFC"; font.pixelSize: 17 }
            }
        }
    }

    Rectangle {
        visible: root.opacityEditing
        anchors.fill: parent; color: "#CC050812"; z: 22
        Rectangle {
            anchors.centerIn: parent; width: Math.min(700, parent.width - 120); height: 260
            radius: 24; color: "#151E32"; border.width: 2; border.color: "#45B978"
            Column {
                anchors.fill: parent; anchors.margins: 34; spacing: 22
                Text { text: "ITEM OPACITY"; color: "white"; font.pixelSize: 28; font.weight: Font.DemiBold }
                Slider {
                    id: opacitySlider; width: parent.width; from: 0; to: 1; stepSize: 0.01; value: 1
                    onMoved: if (root.items.length) root.layoutModel.setItemOpacity(root.items[root.selectedIndex].id, value)
                    onValueChanged: if (root.opacityEditing && root.items.length) root.layoutModel.setItemOpacity(root.items[root.selectedIndex].id, value)
                }
                Row {
                    width: parent.width
                    Text { width: parent.width / 2; text: "INVISIBLE  0%"; color: "#91A0BC"; font.pixelSize: 16 }
                    Text { width: parent.width / 2; horizontalAlignment: Text.AlignRight; text: "100%  OPAQUE"; color: "#91A0BC"; font.pixelSize: 16 }
                }
                Text { text: "← → adjust    Enter or Esc done    " + Math.round(opacitySlider.value * 100) + "%"; color: "#F8FAFC"; font.pixelSize: 17 }
            }
        }
    }

    Rectangle {
        visible: root.categoryEditing
        anchors.fill: parent; color: "#E6050812"; z: 21
        Rectangle {
            anchors.centerIn: parent; width: Math.min(820, parent.width - 100); height: Math.min(610, parent.height - 80)
            radius: 24; color: "#151E32"; border.width: 2; border.color: "#4D7ED8"
            Column {
                anchors.fill: parent; anchors.margins: 30; spacing: 16
                Text { text: root.categoryAssignMode ? "ASSIGN CATEGORY" : "MANAGE CATEGORIES"; color: "white"; font.pixelSize: 28; font.weight: Font.DemiBold }
                TextField {
                    id: categoryName; width: parent.width; height: 52; color: "white"; font.pixelSize: 18
                    placeholderTextColor: "#71809F"
                    background: Rectangle { radius: 9; color: "#0E1628"; border.color: categoryName.activeFocus ? "#FFFFFF" : "#354462" }
                    onAccepted: root.commitCategoryName()
                }
                ListView {
                    width: parent.width; height: 250; spacing: 7; clip: true
                    model: [{id: "", name: "Uncategorized"}].concat(root.layoutModel.categories)
                    currentIndex: root.selectedCategory
                    delegate: Rectangle {
                        required property int index; required property var modelData
                        width: ListView.view.width; height: 42; radius: 8; color: index === root.selectedCategory ? "#2B416B" : "transparent"
                        Text { anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 14; text: modelData.name; color: "white"; font.pixelSize: 17 }
                    }
                }
                Grid {
                    width: parent.width; columns: 4; spacing: 8
                    Repeater {
                        model: root.categoryActions
                        delegate: Rectangle {
                            required property int index; required property string modelData
                            width: (parent.width - 24) / 4; height: 46; radius: 9; color: index === root.categoryAction ? "#4D7ED8" : "#202D47"
                            border.width: index === root.categoryAction ? 2 : 0; border.color: "white"
                            Text { anchors.centerIn: parent; text: modelData; color: "white"; font.pixelSize: 15 }
                            MouseArea { anchors.fill: parent; onClicked: { root.categoryAction = index; root.applyCategoryAction() } }
                        }
                    }
                }
                Text { text: "↑ ↓ category    ← → action    Enter apply    Type names with keyboard"; color: "#91A0BC"; font.pixelSize: 15 }
            }
        }
    }

    Rectangle {
        visible: root.spanEditing
        anchors.fill: parent; color: "#CC050812"; z: 22
        Rectangle {
            anchors.centerIn: parent; width: Math.min(600, parent.width - 120); height: 340
            radius: 24; color: "#151E32"; border.width: 2; border.color: "#D58A45"
            Column {
                anchors.fill: parent; anchors.margins: 34; spacing: 20
                Text { text: "TILE SPAN (WIDTH × HEIGHT)"; color: "white"; font.pixelSize: 26; font.weight: Font.DemiBold }
                Text { text: "Enter cells for the selected tile. Grid: " + root.layoutModel.grid.columns + " × " + root.layoutModel.grid.rows + "."; color: "#91A0BC"; font.pixelSize: 15; wrapMode: Text.WordWrap }
                Row { spacing: 16
                    Text { text: "Width"; width: 100; color: "white"; font.pixelSize: 17 }
                    TextField { id: spanWidthField; width: 120; height: 44; color: "white"; font.pixelSize: 18; text: root.spanWidth
                        background: Rectangle { radius: 9; color: "#0E1628"; border.color: spanWidthField.activeFocus ? "#FFFFFF" : "#354462" }
                        onAccepted: root.spanWidth = Math.max(1, Math.min(root.layoutModel.grid.columns, Number(text) || 1)) }
                }
                Row { spacing: 16
                    Text { text: "Height"; width: 100; color: "white"; font.pixelSize: 17 }
                    TextField { id: spanHeightField; width: 120; height: 44; color: "white"; font.pixelSize: 18; text: root.spanHeight
                        background: Rectangle { radius: 9; color: "#0E1628"; border.color: spanHeightField.activeFocus ? "#FFFFFF" : "#354462" }
                        onAccepted: root.spanHeight = Math.max(1, Math.min(root.layoutModel.grid.rows, Number(text) || 1)) }
                }
                Text { text: root.message; color: "#F88273"; font.pixelSize: 15; wrapMode: Text.WordWrap }
                Row { spacing: 12
                    Rectangle { width: 140; height: 46; radius: 9; color: "#D58A45"; border.width: 1; border.color: "white"
                        Text { anchors.centerIn: parent; text: "APPLY SPAN"; color: "white"; font.pixelSize: 16; font.weight: Font.Bold }
                        MouseArea { anchors.fill: parent; onClicked: {
                            const placement = { x: 0, y: 0, width: root.spanWidth, height: root.spanHeight }
                            const grid = root.layoutModel.grid
                            if (placement.width < 1 || placement.height < 1 || placement.width > grid.columns || placement.height > grid.rows) { root.message = "Span exceeds grid bounds."; return }
                            const collisions = root.layoutModel.gridCollisions(root.spanTargetId, placement)
                            if (collisions.length > 0) {
                                root.collisionPlacement = placement
                                root.collisionTargets = collisions
                                root.gridCollisionEditing = true
                                root.spanEditing = false
                                return
                            }
                            if (!root.layoutModel.setItemPlacement(root.spanTargetId, placement)) { root.message = root.layoutModel.errorString; return }
                            root.spanEditing = false; root.message = ""
                        } }
                    }
                    Rectangle { width: 140; height: 46; radius: 9; color: "#35435A"; border.width: 1; border.color: "white"
                        Text { anchors.centerIn: parent; text: "CANCEL"; color: "white"; font.pixelSize: 16 }
                        MouseArea { anchors.fill: parent; onClicked: root.spanEditing = false } }
                }
                Text { text: "← → adjust    Enter apply    Back cancel"; color: "#91A0BC"; font.pixelSize: 15 }
            }
        }
    }
    Rectangle {
        visible: root.gridCollisionEditing
        anchors.fill: parent; color: "#CC050812"; z: 23
        Rectangle {
            anchors.centerIn: parent; width: Math.min(700, parent.width - 120); height: 360
            radius: 24; color: "#151E32"; border.width: 2; border.color: "#F88273"
            Column {
                anchors.fill: parent; anchors.margins: 34; spacing: 20
                Text { text: "CELLS OCCUPIED"; color: "white"; font.pixelSize: 26; font.weight: Font.DemiBold }
                Text { text: "Another widget occupies those cells. Choose how to resolve:"; color: "#91A0BC"; font.pixelSize: 15; wrapMode: Text.WordWrap }
                ListView {
                    width: parent.width; height: 180; spacing: 8; clip: true
                    model: root.collisionTargets
                    delegate: Rectangle {
                        required property int index; required property var modelData
                        width: ListView.view.width; height: 44; radius: 8; color: "#202D47"
                        Text { anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 14; text: modelData.name || modelData.id; color: "white"; font.pixelSize: 17 }
                    }
                }
                Grid { width: parent.width; columns: 3; spacing: 10
                    Repeater {
                        model: ["Shift other", "Swap places", "Cancel"]
                        delegate: Rectangle {
                            required property int index; required property string modelData
                            width: (parent.width - 20) / 3; height: 48; radius: 9; color: "#35435A"; border.width: 1; border.color: "#2B3853"
                            Text { anchors.centerIn: parent; text: modelData; color: "white"; font.pixelSize: 16 }
                            MouseArea { anchors.fill: parent; onClicked: {
                                if (index === 0) { root.applySpanWithShift(); return }
                                if (index === 1) { root.applySpanWithSwap(); return }
                                root.gridCollisionEditing = false; root.spanEditing = false; root.message = ""
                            } }
                        }
                    }
                }
                Text { text: "↑ ↓ choose    Enter apply    Back cancel"; color: "#91A0BC"; font.pixelSize: 15 }
            }
        }
    }

    Rectangle { anchors.fill: parent; color: "#080C18" }
    Column {
        anchors.fill: parent; anchors.margins: 62; spacing: 22
        Text { text: root.layoutModel.isPage ? "CUSTOMIZE PAGE" : "CUSTOMIZE HOME"; color: "#F8FAFC"; font.pixelSize: 38; font.weight: Font.DemiBold; font.letterSpacing: 3 }
        Text { text: "↑ ↓ choose a tile    ← → choose an action    Enter apply    Esc return home"; color: "#91A0BC"; font.pixelSize: 17 }
        Row {
            width: parent.width; height: parent.height - 150; spacing: 30
            Rectangle {
                width: parent.width * 0.47; height: parent.height; radius: 18; color: "#111A2A"; border.color: "#2B3853"
                ListView {
                    id: itemList; anchors.fill: parent; anchors.margins: 14; spacing: 8; clip: true; model: root.items; currentIndex: root.selectedIndex
                    delegate: Rectangle {
                        required property int index; required property var modelData
                        width: ListView.view.width; height: 54; radius: 10
                        color: index === root.selectedIndex ? "#2B416B" : "transparent"
                        Text { anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 16; width: parent.width - 100; elide: Text.ElideRight; text: (modelData.kind === "widget" ? "Widget · " + modelData.widgetType : modelData.name) + (modelData.unavailableReason ? " · source missing" : ""); color: "#F8FAFC"; font.pixelSize: 19 }
                        Text { anchors.verticalCenter: parent.verticalCenter; anchors.right: parent.right; anchors.rightMargin: 16; text: modelData.size; color: "#91A0BC"; font.pixelSize: 14 }
                    }
                }
            }
            Rectangle {
                width: parent.width * 0.5; height: parent.height; radius: 18; color: "#111A2A"; border.color: "#2B3853"
                Flickable {
                    id: actionScroll
                    anchors.fill: parent; anchors.margins: 18; clip: true
                    contentHeight: actionGrid.height; boundsBehavior: Flickable.StopAtBounds
                    onHeightChanged: Qt.callLater(root.revealAction)
                Grid {
                    id: actionGrid
                    width: parent.width; columns: 3; spacing: 9
                    Repeater {
                        model: root.actions
                        delegate: Rectangle {
                            required property int index; required property string modelData
                            width: (parent.width - 18) / 3; height: 72; radius: 10
                            color: index === root.actionIndex ? "#6E55B8" : "#18233A"
                            border.width: index === root.actionIndex ? 3 : 1; border.color: index === root.actionIndex ? "white" : "#2B3853"
                            Text { anchors.centerIn: parent; width: parent.width - 12; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; text: modelData; color: "white"; font.pixelSize: 14; font.weight: Font.Medium }
                            MouseArea { anchors.fill: parent; onClicked: { root.actionIndex = index; root.applyAction() } }
                        }
                    }
                }
                }
            }
        }
    }
}
