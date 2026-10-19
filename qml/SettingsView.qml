import QtQuick

Item {
    id: root
    required property var homeItems
    required property var availableApps
    required property var artwork
    required property var themes
    property var appearance: ({})
    property int selectedSection: 0
    property bool inPage: false
    onVisibleChanged: if (!visible) homePage.sourceRepairEditing = false
    signal previewAmbientRequested()
    signal pageRequested(string id)
    readonly property var sections: [
        { name: "Network", description: "Connection status, devices, and Wi-Fi control" },
        { name: "Home", description: "Tiles, categories, background, artwork, and widgets" },
        { name: "Sources", description: "Detected discs, drives, media folders, and automatic Home tiles" },
        { name: "Remotes", description: "Physical remotes, controllers, pairing, button tests, and mappings" },
        { name: "System", description: "Display, audio, power, support, updates, and about" },
        { name: "Profiles", description: "People, personal layouts, and separate web sign-ins" }
    ]

    function openSection(index) {
        selectedSection = Math.max(0, Math.min(sections.length - 1, index))
        inPage = true
    }

    function handleCommand(command) {
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
        if (command === inputManager.navigateUp)
            selectedSection = Math.max(0, selectedSection - 1)
        else if (command === inputManager.navigateDown)
            selectedSection = Math.min(sections.length - 1, selectedSection + 1)
        else if (command === inputManager.confirm)
            inPage = true
    }

    Rectangle { anchors.fill: parent; color: "#080C18" }

    CustomizeHomeView {
        id: homePage
        anchors.fill: parent
        visible: root.inPage && root.selectedSection === 1
        items: root.homeItems
        availableApps: root.availableApps
        artwork: root.artwork
        themes: root.themes
        z: 5
        onPageRequested:id=>root.pageRequested(id)
    }

    Row {
        anchors.fill: parent
        anchors.margins: 54
        spacing: 28
        visible: !(root.inPage && root.selectedSection === 1)

        Column {
            width: 360
            height: parent.height
            spacing: 12
            Text { text: "SETTINGS"; color: "#F8FAFC"; font.pixelSize: 38; font.weight: Font.DemiBold; font.letterSpacing: 3 }
            Text { text: inPage ? "Esc returns to settings" : "↑ ↓ choose    Enter open    Esc home"; color: "#91A0BC"; font.pixelSize: 16 }
            Repeater {
                model: root.sections
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: parent.width; height: Math.max(52, (root.height - 208) / 6 - 12); radius: 8
                    color: root.selectedSection === index ? "#2B416B" : "#111A2A"
                    border.width: root.selectedSection === index ? 3 : 1
                    border.color: root.selectedSection === index ? "#F8FAFC" : "#2B3853"
                    Column {
                        anchors.fill: parent; anchors.margins: 14; spacing: 6
                        Text { text: modelData.name; color: "#F8FAFC"; font.pixelSize: 22; font.weight: Font.DemiBold }
                        Text { text: modelData.description; color: "#9EABC1"; font.pixelSize: 13; elide: Text.ElideRight; width: parent.width }
                    }
                }
            }
        }

        Rectangle {
            width: parent.width - 388
            height: parent.height
            radius: 10
            color: "#0F1728"
            border.color: "#26344F"
            Item {
                anchors.fill: parent
                anchors.margins: 26
                NetworkSettingsView { id: networkPage; anchors.fill: parent; visible: root.selectedSection === 0 }
                Column {
                    visible: root.selectedSection === 1
                    anchors.centerIn: parent
                    spacing: 10
                    Text { text: "HOME"; color: "#F8FAFC"; font.pixelSize: 30; font.weight: Font.DemiBold; font.letterSpacing: 3 }
                    Text { text: "Open to edit tiles, artwork, categories, and background"; color: "#91A0BC"; font.pixelSize: 18 }
                }
                SourceSettingsView { id: sourcePage; anchors.fill: parent; visible: root.selectedSection === 2 }
                ProfileSettingsView { id: profilePage; anchors.fill: parent; visible: root.selectedSection === 5 }
                RemoteSettingsView { id: remotePage; anchors.fill: parent; visible: root.selectedSection === 3 }
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
