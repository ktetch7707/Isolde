import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    required property var homeItems
    required property var availableApps
    required property var artwork
    required property var themes
    property var appearance: ({})
    property var layoutModel: homeLayout
    readonly property var packagedSections: (layoutModel.studioOptions.settingsNavigation || {}).categories || []
    property var pendingCategoryOrder: []
    readonly property bool arranging: pendingCategoryOrder.length > 0
    readonly property var sections: {
        const order = (arranging ? pendingCategoryOrder : layoutModel.composition.settingsCategoryOrder || []).filter(id=>id!=="display-skins")
        if (order.length !== packagedSections.length) return packagedSections
        const sorted = order.map(id => packagedSections.find(section => section.id === id))
        return sorted.every(section => !!section) && new Set(order).size === order.length ? sorted : packagedSections
    }
    property string navigationError: ""
    property string navigationProfile: profileManager.activeId
    onNavigationProfileChanged: { pendingCategoryOrder=[]; rowSelections=({}); selectedSection=0; inPage=false; route=""; navigationError="" }
    onVisibleChanged: if (!visible) {
        const id = category.id
        pendingCategoryOrder=[]; navigationError=""
        const index = sections.findIndex(section => section.id === id)
        if (index >= 0) selectedSection=index
    }
    property int selectedSection: 0
    property bool inPage: false
    property string route: ""
    property var rowSelections: ({})
    property string editPageId: ""
    property string editComponentId: ""
    property var skinEditor:null
    property var screensaverEditor:null
    property string editingSkinId:""
    readonly property var category: sections[selectedSection] || ({})
    readonly property var categoryRows: category.rows || []
    readonly property string paneRoute: inPage ? route : (category.route || "")
    readonly property int selectedRow: rowSelections[category.id] || 0
    onSelectedRowChanged: Qt.callLater(revealCategoryRow)
    onCategoryChanged: Qt.callLater(() => { categoryScroll.contentY=0; revealCategoryRow() })
    onHeightChanged: Qt.callLater(revealCategoryRow)
    readonly property bool homeEditing: inPage && route === "studio" && homePage.editing
    readonly property bool fullPage: inPage && route === "studio"
    signal previewAmbientRequested()
    signal pageRequested(string id)
    signal storeRequested()
    signal browserRequested(string appId)

    function revealCategoryRow() {
        const row = categoryItems.itemAt(selectedRow)
        if (!row) return
        if (row.y < categoryScroll.contentY) categoryScroll.contentY = row.y
        else if (row.y + row.height > categoryScroll.contentY + categoryScroll.height)
            categoryScroll.contentY = Math.max(0, row.y + row.height - categoryScroll.height)
        categoryScroll.contentY = Math.max(0, Math.min(Math.max(0, categoryScroll.contentHeight-categoryScroll.height), categoryScroll.contentY))
    }

    function handleCategoryArrangement(command) {
        if (command === inputManager.back) {
            const id = category.id
            pendingCategoryOrder = []
            selectedSection = sections.findIndex(section => section.id === id)
            navigationError = ""
        } else if (command === inputManager.confirm) {
            if (!profileManager.editingAllowed) { navigationError=qsTr("Reordering is restricted in this profile."); return }
            const id = category.id
            const composition = Object.assign({}, layoutModel.composition, {settingsCategoryOrder:pendingCategoryOrder.slice()})
            if (layoutModel.setComposition(composition)) {
                pendingCategoryOrder = []
                selectedSection = sections.findIndex(section => section.id === id)
                navigationError = ""
            } else navigationError = layoutModel.errorString
        } else if (command === inputManager.navigateUp || command === inputManager.navigateDown) {
            const next = selectedSection + (command === inputManager.navigateUp ? -1 : 1)
            if (next < 0 || next >= sections.length) return
            const order = pendingCategoryOrder.slice()
            const moved = order.splice(selectedSection, 1)[0]
            order.splice(next, 0, moved)
            pendingCategoryOrder = order
            selectedSection = next
        }
    }

    function selectRow(index) {
        if (index < 0 || index >= categoryRows.length) return
        const next = Object.assign({}, rowSelections)
        next[category.id] = index
        rowSelections = next
    }

    function openRoute(target) {
        if (category.route !== target && !categoryRows.some(item => item.route === target)) return
        if (target === "store") { storeRequested(); return }
        route = target
        if (target === "capture") { systemPage.captureReturnPage=-1; systemPage.activePage = 8 }
        else if (target === "display") systemPage.activePage = 0
        else if (target === "audio") systemPage.activePage = 1
        else if (target === "ambient") systemPage.activePage = 2
        else if (target === "skins") { skinPicker.begin() }
        else if (target === "system") systemPage.activePage = -1
    }

    function openCategory(id, target) {
        const index = sections.findIndex(section => section.id === id)
        if (index < 0) return
        const entry = sections[index]
        if (target && entry.route !== target && !(entry.rows || []).some(item => item.route === target)) return
        selectedSection = index
        inPage = true
        route = ""
        if (target) {
            const row = categoryRows.findIndex(item => item.route === target)
            if (row >= 0) selectRow(row)
            openRoute(target)
        } else if (category.route) openRoute(category.route)
    }

    // Saved actions keep their original section IDs. Reordering categories must
    // never turn an existing VPN shortcut into another destination.
    function openSection(index) {
        const legacy = [ ["network", "network"], ["home", "studio"],
            ["inputs", "sources"], ["remotes", "remotes"], ["system", "system"],
            ["profiles", "profiles"], ["network", "vpn"],
            ["apps", "store-preferences"], ["media", "media"] ]
        if (!Number.isInteger(index) || index < 0 || index >= legacy.length) return
        openCategory(legacy[index][0], legacy[index][1])
    }

    function openCaptureSettings() { openCategory("inputs", "capture") }
    function openAppsSettings() { openCategory("apps", "") }

    function backToParent() {
        if(editingSkinId && (route==="studio" || route==="skin-screensaver")){
            const id=editingSkinId
            route="skins";editingSkinId="";skinEditor=null;screensaverEditor=null
            layoutModel.closeSkinEditor()
            ambientSettings.closeSkinEditor()
            skinPicker.begin(id)
            return
        }
        if (categoryRows.length && route) route = ""
        else { route = ""; inPage = false }
    }
    readonly property bool remoteEditing: inPage && route === "remotes" && remotePage.editing
    readonly property bool powerConfirmation: visible && inPage && route === "system" && systemPage.activePage === 3

    function handleCommand(command) {
        if (remoteEditing) { remotePage.handleCommand(command); return }
        if (arranging) { handleCategoryArrangement(command); return }
        if (!inPage) {
            if (command === inputManager.navigateUp) selectedSection = Math.max(0, selectedSection - 1)
            else if (command === inputManager.navigateDown) selectedSection = Math.min(sections.length - 1, selectedSection + 1)
            else if (command === inputManager.confirm || command === inputManager.navigateRight) openCategory(category.id, "")
            else if (command === inputManager.menu && sections.length) {
                if (!profileManager.editingAllowed) { navigationError=qsTr("Reordering is restricted in this profile."); return }
                navigationError=""; pendingCategoryOrder=sections.map(section => section.id)
            }
            return
        }
        if (route === "studio") { homePage.handleCommand(command); return }
        if (route === "skins") { skinPicker.handleCommand(command); return }
        if (route === "skin-screensaver") {
            if (skinScreensaver.item) {
                if(command===inputManager.back && !skinScreensaver.item.editing)backToParent()
                else skinScreensaver.item.handleCommand(command)
            }
            return
        }
        if (route === "media") {
            if (command === inputManager.back) { if (!mediaPage.back()) backToParent() }
            else mediaPage.handleCommand(command)
            return
        }
        if (route === "sources" && sourcePage.editing) { sourcePage.handleCommand(command); return }
        if (route === "profiles" && profilePage.editing) { profilePage.handleCommand(command); return }
        if (route === "network" && (networkPage.keyboardOpen || networkPage.passwordPrompt || networkPage.hiddenNetworkPrompt || networkPage.connectionPrompt)) {
            networkPage.handleCommand(command); return
        }
        if (route === "vpn" && vpnPage.editing) { vpnPage.handleCommand(command); return }
        if (["display", "audio", "ambient", "capture", "system"].includes(route) && systemPage.activePage >= 0) {
            systemPage.handleCommand(command)
            if (systemPage.activePage < 0 && route !== "system") backToParent()
            return
        }
        if (command === inputManager.back) { backToParent(); return }
        if (!route) {
            if (command === inputManager.navigateUp) selectRow(Math.max(0, selectedRow - 1))
            else if (command === inputManager.navigateDown) selectRow(Math.min(categoryRows.length - 1, selectedRow + 1))
            else if (command === inputManager.navigateLeft) inPage = false
            else if ((command === inputManager.navigateRight || command === inputManager.confirm) && categoryRows[selectedRow])
                openRoute(categoryRows[selectedRow].route)
            return
        }
        if (route === "network") networkPage.handleCommand(command)
        else if (route === "sources") sourcePage.handleCommand(command)
        else if (route === "remotes") remotePage.handleCommand(command)
        else if (route === "profiles") profilePage.handleCommand(command)
        else if (route === "vpn") vpnPage.handleCommand(command)
        else if (route === "store-preferences") storePage.handleCommand(command)
        else if (route === "interface-sounds") interfaceSoundPage.handleCommand(command)
        else if (route === "system") systemPage.handleCommand(command)
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: root.appearance.backgroundStart || shell.background }
            GradientStop { position: 1; color: shell.background }
        }
    }
    CustomizeHomeView {
        id: homePage
        layoutModel:root.skinEditor || root.layoutModel
        anchors.fill: parent
        visible: root.fullPage
        initialPageId: root.editingSkinId ? "" : root.editPageId
        initialComponentId: root.editingSkinId ? "" : root.editComponentId
        items: root.homeItems
        availableApps: root.availableApps
        artwork: root.artwork
        themes: root.themes
        z: 5
        onPageRequested: id => root.pageRequested(id)
        onClosed: root.backToParent()
    }

    SkinPicker {
        id: skinPicker
        anchors.fill: parent
        visible: open && root.inPage && root.route === "skins"
        layoutModel: root.layoutModel
        ambientModel: ambientSettings
        appearance:root.appearance
        z: 6
        onClosed: if (root.route === "skins") root.backToParent()
        onEditSkin: id => {
            const model=root.layoutModel.openSkinEditor(id)
            if(!model){message=root.layoutModel.errorString;return}
            root.skinEditor=model; root.editingSkinId=id
            root.route = "studio"
        }
        onScreensaverRequested:id=>{
            const model=root.layoutModel.openSkinEditor(id)
            if(!model){message=root.layoutModel.errorString;return}
            root.skinEditor=model;root.editingSkinId=id
            root.screensaverEditor=id===root.layoutModel.activeSkinId?ambientSettings:ambientSettings.openSkinEditor(id)
            root.route="skin-screensaver"
        }
        onDuplicated:(sourceId,copyId)=>ambientSettings.copySkinSettings(sourceId,copyId)
    }
    Loader {
        id:skinScreensaver;anchors.fill:parent;z:7
        active:root.inPage && root.route==="skin-screensaver" && !!root.screensaverEditor && !!root.skinEditor
        sourceComponent:AmbientSettingsView {
            appearance:root.appearance;settingsModel:root.screensaverEditor || ambientSettings
            layoutModel:root.skinEditor || root.layoutModel
            canPreview:root.editingSkinId===root.layoutModel.activeSkinId
            onPreviewRequested:root.previewAmbientRequested()
        }
    }

    Row {
        anchors.fill: parent
        anchors.margins: 44
        spacing: 36 * shell.gapScale
        visible: !root.fullPage
        Column {
            id: sectionColumn
            width: Math.min(350 * shell.textScale, parent.width * .33)
            height: parent.height
            spacing: 16 * shell.gapScale
            Row {
                width:parent.width; spacing:12
                LineIcon { width:28*shell.textScale;height:width;anchors.verticalCenter:parent.verticalCenter;name:"settings";ink:shell.backgroundText }
                Text { font.family:shell.fontFamily;text:qsTr("Settings");color:shell.backgroundText;font.pixelSize:36*shell.textScale;font.weight:Font.DemiBold }
            }
            Text {
                visible:text.length>0;width:parent.width;wrapMode:Text.Wrap;font.family:shell.fontFamily
                text:root.navigationError || (root.arranging?qsTr("Up / Down moves · OK saves · Back cancels"):"")
                color:root.navigationError?shell.backgroundError:shell.backgroundMuted;font.pixelSize:17*shell.textScale
            }
            ListView {
                objectName: "settingsSections"
                width: parent.width; height: parent.height - y; clip: true
                spacing: 5 * shell.gapScale
                currentIndex: root.selectedSection
                highlightRangeMode: ListView.ApplyRange; highlightMoveDuration: 0
                model: root.sections
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    readonly property bool selected: root.selectedSection === index
                    width: ListView.view.width
                    height: Math.max(53*shell.textScale, sectionLabel.implicitHeight+20)
                    radius: shell.radius
                    color: selected ? shell.selected : "transparent"
                    border.width: selected && !root.inPage ? 2 : 0
                    border.color: shell.selectedFocus
                    SettingsUpdateBadge { anchors.top: parent.top; anchors.right: parent.right; z: 5; settingsTarget: modelData.id === "system" }
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.name
                    Accessible.description: modelData.description
                    Text {
                        id:sectionLabel
                        anchors.left:parent.left;anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter
                        anchors.leftMargin:16;anchors.rightMargin:16;elide:Text.ElideRight;textFormat:Text.PlainText
                        font.family:shell.fontFamily;text:modelData.name;color:selected?shell.selectedText:shell.backgroundText
                        font.pixelSize:20*shell.textScale;font.weight:selected?Font.DemiBold:Font.Normal
                    }
                    MouseArea { anchors.fill:parent; enabled:!root.arranging; onClicked:root.openCategory(modelData.id, "") }
                }
            }
        }
        Rectangle {
            width: parent.width - sectionColumn.width - parent.spacing
            height: parent.height
            radius: shell.radius; color: shell.elevated
            Item {
                anchors.fill:parent; anchors.margins:26
                // Preview category contents without allowing a second active focus.
                Flickable {
                    id: categoryScroll
                    anchors.fill:parent; clip:true
                    contentHeight:categoryContents.implicitHeight
                    boundsBehavior:Flickable.StopAtBounds
                    visible: !root.paneRoute || (!root.inPage && root.paneRoute==="skins")
                Column {
                    id: categoryContents
                    width:parent.width
                    spacing:10*shell.gapScale
                    Text { width:parent.width; wrapMode:Text.Wrap; text:root.category.name || qsTr("Settings unavailable"); font.family:shell.fontFamily; font.pixelSize:30*shell.textScale; font.weight:Font.DemiBold; color:shell.elevatedText }
                    Text { width:parent.width; wrapMode:Text.Wrap; text:root.category.description || qsTr("The packaged navigation could not be loaded."); font.family:shell.fontFamily; font.pixelSize:20*shell.textScale; color:shell.elevatedMuted }
                    Repeater {
                        id: categoryItems
                        model: root.categoryRows
                        delegate: Rectangle {
                            required property int index
                            required property var modelData
                            readonly property bool selected: root.inPage && root.selectedRow === index
                            objectName: "settingsCategoryRow-" + modelData.route
                            width:parent.width; height:Math.max(75*shell.textScale, menuLabels.implicitHeight+22)
                            radius:shell.radius; color:selected?shell.selected:shell.surface
                            border.width:selected?2:0; border.color:shell.selectedFocus
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.name
                            Accessible.description: modelData.description
                            LineIcon {
                                id:routeArrow
                                anchors.right:parent.right;anchors.rightMargin:16;anchors.verticalCenter:parent.verticalCenter
                                name:"arrow-right";width:18*shell.textScale;height:width;ink:selected?shell.selectedText:shell.muted
                            }
                            Column {
                                id:menuLabels
                                anchors.left:parent.left; anchors.right:routeArrow.left; anchors.verticalCenter:parent.verticalCenter; anchors.margins:16
                                spacing:3*shell.gapScale
                                Text { width:parent.width; wrapMode:Text.Wrap; text:modelData.name; font.family:shell.fontFamily; font.pixelSize:23*shell.textScale; font.weight:Font.DemiBold; color:selected?shell.selectedText:shell.text }
                                Text { width:parent.width; wrapMode:Text.Wrap; text:modelData.description; font.family:shell.fontFamily; font.pixelSize:18*shell.textScale; color:selected?shell.selectedText:shell.muted }
                            }
                            MouseArea { anchors.fill:parent; enabled:!root.arranging; onClicked:{root.inPage=true;root.selectRow(index);root.openRoute(modelData.route)} }
                        }
                    }
                    Repeater {
                        model: !root.inPage && root.category.route === "skins" ? skinPicker.entries : []
                        delegate: Rectangle {
                            required property int index
                            required property var modelData
                            objectName: "settingsSkin-" + modelData.id
                            width: parent.width
                            height: Math.max(54 * shell.textScale, skinName.implicitHeight + 20)
                            radius: shell.radius
                            color: shell.surface
                            Text {
                                id: skinName
                                anchors.fill: parent
                                anchors.margins: 12
                                verticalAlignment: Text.AlignVCenter
                                text: modelData.name + (modelData.id === root.layoutModel.activeSkinId ? qsTr(" · Active") : "")
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                font.family: shell.fontFamily
                                font.pixelSize: 20 * shell.textScale
                                color: shell.text
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: !root.arranging
                                onClicked: {
                                    const skinIndex = index
                                    root.openCategory(root.category.id, "")
                                    skinPicker.selected = skinIndex
                                    skinPicker.selectEntry()
                                }
                            }
                        }
                    }
                    Text { visible:!root.categoryRows.length && root.category.route !== "skins"; width:parent.width; wrapMode:Text.Wrap; text:qsTr("Confirm to open"); font.family:shell.fontFamily; font.pixelSize:20*shell.textScale; color:shell.elevatedMuted }
                }
                }
                NetworkSettingsView {id:networkPage;anchors.fill:parent;appearance:root.appearance;visible:root.inPage && root.route==="network"}
                VpnSettingsView {id:vpnPage;anchors.fill:parent;appearance:root.appearance;visible:root.inPage && root.route==="vpn"}
                StoreSettingsView {id:storePage;anchors.fill:parent;appearance:root.appearance;visible:root.inPage && root.route==="store-preferences";onBrowserRequested: appId => root.browserRequested(appId)}
                InterfaceSoundSettingsView {id:interfaceSoundPage;anchors.fill:parent;appearance:root.appearance;visible:root.inPage && root.route==="interface-sounds"}
                KodiSettingsView {id:mediaPage;anchors.fill:parent;appearance:root.appearance;visible:root.paneRoute==="media";enabled:root.inPage}
                SourceSettingsView {id:sourcePage;anchors.fill:parent;appearance:root.appearance;visible:root.inPage && root.route==="sources"}
                ProfileSettingsView {id:profilePage;anchors.fill:parent;appearance:root.appearance;visible:root.paneRoute==="profiles";enabled:root.inPage}
                RemoteSettingsView {id:remotePage;anchors.fill:parent;appearance:root.appearance;visible:root.paneRoute==="remotes";enabled:root.inPage}
                SystemSettingsView {id:systemPage;anchors.fill:parent;appearance:root.appearance;visible:["display","audio","ambient","capture","system"].includes(root.paneRoute);enabled:root.inPage;onPreviewRequested:root.previewAmbientRequested()}
            }
        }
    }
}
