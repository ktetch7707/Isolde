import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    required property var homeItems
    required property var availableApps
    required property var artwork
    required property var themes
    property var appearance: ({})
    property int selectedSection: 0
    property bool inPage: false
    property string editPageId: ""
    property string editComponentId: ""
    readonly property bool homeEditing: inPage && selectedSection === 1 && homePage.editing
    signal previewAmbientRequested()
    signal pageRequested(string id)
    readonly property var sections: [
        { name: "Network", description: "Connection status, devices, and Wi-Fi control" },
        { name: "Home", description: "Tiles, categories, background, artwork, and widgets" },
        { name: "Sources", description: "Detected discs, drives, media folders, and automatic Home tiles" },
        { name: "Remotes", description: "Physical remotes, controllers, pairing, button tests, and mappings" },
        { name: "System", description: "Display, audio, power, support, updates, and about" },
        { name: "Profiles", description: "People, personal layouts, and separate web sign-ins" },
        { name: "VPN", description: "Install and configure Surfshark and Proton VPN" },
        { name: "Online Store", description: "Verified Android repositories and Chromium web apps" },
        { name: "Media", description: "Library and playback preferences" }
    ]

    function openSection(index) {
        selectedSection = Math.max(0, Math.min(sections.length - 1, index))
        inPage = true
    }
    function openCaptureSettings() {
        openSection(4)
        systemPage.activePage = 8
    }

    function handleCommand(command) {
        if(inPage && selectedSection===8){
            if(command===inputManager.back){if(!mediaPage.back())inPage=false}
            else mediaPage.handleCommand(command)
            return
        }
        if (inPage && selectedSection === 2 && sourcePage.editing) { sourcePage.handleCommand(command); return }
        if (inPage && selectedSection === 6 && (vpnPage.editing || command !== inputManager.back)) {
            vpnPage.handleCommand(command)
            return
        }
        if (inPage && selectedSection === 1 && homePage.editing) { homePage.handleCommand(command); return }
        if (inPage && selectedSection === 5 && (profilePage.editing || command !== inputManager.back)) {
            profilePage.handleCommand(command)
            return
        }
        if (inPage && selectedSection === 0 && (networkPage.passwordPrompt || networkPage.hiddenNetworkPrompt || networkPage.connectionPrompt)) {
            networkPage.handleCommand(command)
            return
        }
        if (inPage && selectedSection === 4 && systemPage.activePage >= 0) {
            systemPage.handleCommand(command)
            return
        }
        if (inPage && command === inputManager.back) {
            inPage = false
            return
        }
        if (inPage && selectedSection === 0) {
            networkPage.handleCommand(command)
            return
        }
        if (inPage && selectedSection === 1) {
            homePage.handleCommand(command)
            return
        }
        if (inPage && selectedSection === 2) {
            sourcePage.handleCommand(command)
            return
        }
        if (inPage && selectedSection === 3) {
            remotePage.handleCommand(command)
            return
        }
        if (inPage && selectedSection === 4) {
            systemPage.handleCommand(command)
            return
        }
        if (inPage && selectedSection === 7) {
            storePage.handleCommand(command)
            return
        }
        if (command === inputManager.navigateUp)
            selectedSection = Math.max(0, selectedSection - 1)
        else if (command === inputManager.navigateDown)
            selectedSection = Math.min(sections.length - 1, selectedSection + 1)
        else if (command === inputManager.confirm)
            inPage = true
    }

    Rectangle { anchors.fill: parent; color: shell.background }

    CustomizeHomeView {
        id: homePage
        anchors.fill: parent
        visible: root.inPage && root.selectedSection === 1
        initialPageId: root.editPageId
        initialComponentId: root.editComponentId
        items: root.homeItems
        availableApps: root.availableApps
        artwork: root.artwork
        themes: root.themes
        z: 5
        onPageRequested:id=>root.pageRequested(id)
        onClosed: root.inPage = false
    }

    Row {
        anchors.fill: parent
        anchors.margins: 54
        spacing: 28 * shell.gapScale
        visible: !(root.inPage && root.selectedSection === 1)

        Column {
            width: 360
            height: parent.height
            spacing: 12 * shell.gapScale
            Text { font.family: shell.fontFamily; text: "SETTINGS"; color: shell.backgroundText; font.pixelSize: 38 * shell.textScale; font.weight: Font.DemiBold; font.letterSpacing: 3 }
            Text { font.family: shell.fontFamily; width:parent.width;wrapMode:Text.Wrap;text: "Theme: " + (root.appearance.themeSource || "Home") + "\n" + (inPage ? "Esc returns to settings" : "↑ ↓ choose    Enter open    Esc home"); color: shell.backgroundMuted; font.pixelSize: 16 * shell.textScale }
            ListView {
                objectName:"settingsSections"
                width:parent.width;height:parent.height-y;clip:true
                spacing:8*shell.gapScale;currentIndex:root.selectedSection
                highlightRangeMode:ListView.ApplyRange;highlightMoveDuration:0
                model: root.sections
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: ListView.view.width; height: 76 * shell.textScale; radius: shell.radius
                    color: root.selectedSection === index ? shell.selected : shell.surface
                    border.width: root.selectedSection === index ? 3 : 1
                    border.color: root.selectedSection === index ? shell.selectedFocus : shell.outline
                    Column {
                        anchors.fill: parent; anchors.margins: 14; spacing: 6 * shell.gapScale
                        Text { font.family: shell.fontFamily; text: modelData.name; color: (root.selectedSection === index) ? shell.selectedText : shell.text; font.pixelSize: 22 * shell.textScale; font.weight: Font.DemiBold }
                        Text { font.family: shell.fontFamily; text: modelData.description; color: (root.selectedSection === index) ? shell.selectedText : shell.muted; font.pixelSize: 13 * shell.textScale; elide: Text.ElideRight; width: parent.width }
                    }
                }
            }
        }

        Rectangle {
            width: parent.width - 360 - parent.spacing
            height: parent.height
            radius: shell.radius
            color: shell.elevated
            border.color: shell.outline
            Item {
                anchors.fill: parent
                anchors.margins: 26
                NetworkSettingsView { appearance: root.appearance; id: networkPage; anchors.fill: parent; visible: root.selectedSection === 0 }
                VpnSettingsView { id: vpnPage; anchors.fill: parent; appearance: root.appearance; visible: root.inPage && root.selectedSection === 6 }
                StoreSettingsView { id: storePage; anchors.fill: parent; appearance:root.appearance; visible: root.inPage && root.selectedSection === 7 }
                KodiSettingsView {id:mediaPage;anchors.fill:parent;appearance:root.appearance;visible:root.inPage && root.selectedSection===8}
                Column {
                    visible: root.selectedSection === 1
                    anchors.centerIn: parent
                    spacing: 10 * shell.gapScale
                    Text { font.family: shell.fontFamily; text: "HOME"; color: shell.elevatedText; font.pixelSize: 30 * shell.textScale; font.weight: Font.DemiBold; font.letterSpacing: 3 }
                    Text { font.family: shell.fontFamily; text: "Open to edit tiles, artwork, categories, and background"; color: shell.elevatedMuted; font.pixelSize: 18 * shell.textScale }
                }
                SourceSettingsView { appearance: root.appearance; id: sourcePage; anchors.fill: parent; visible: root.selectedSection === 2 }
                ProfileSettingsView { appearance: root.appearance; id: profilePage; anchors.fill: parent; visible: root.selectedSection === 5 }
                RemoteSettingsView { appearance: root.appearance; id: remotePage; anchors.fill: parent; visible: root.selectedSection === 3 }
                SystemSettingsView {
                    id: systemPage
                    anchors.fill: parent
                    visible: root.selectedSection === 4
                    appearance: root.appearance
                    onPreviewRequested: root.previewAmbientRequested()
                }
            }
        }
    }
}
