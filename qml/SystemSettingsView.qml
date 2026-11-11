import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    Rectangle {anchors.fill:parent;z:-1;color:shell.surface}
    property var appearance: ({})
    property int selectedIndex: 0
    property int activePage: -1
    property int captureReturnPage: -1
    readonly property var rows: [
        { page: 5, name: "Setup guide", detail: "Repeat network, display, sound, remote and profile setup", status: "Open guide" },
        { page: 7, name: "Diagnostics", detail: "Installed media components and capture readiness", status: "Inspect" },
        { page: 4, name: "About Isolde", detail: "Version and acknowledgements", status: "Isolde " + isoldeVersion },
        { page: 3, name: "Power", detail: "OS power and restart controls are not available in this build", status: "Unavailable" }
    ]

    function handleCommand(command) {
        if (activePage === 7) {
            if (command === inputManager.back) activePage = -1
            else componentPage.handleCommand(command)
            return
        }
        if (activePage === 8) {
            if (command === inputManager.back) {
                if (capturePage.draftDirty || capturePage.keyboardMode || capturePage.artworkEditing) capturePage.handleCommand(command)
                else { capturePage.handleCommand(command); activePage = captureReturnPage }
            }
            else capturePage.handleCommand(command)
            return
        }
        if (activePage === 6) {
            if (command === inputManager.back) { if (!kodiPage.back()) activePage = -1 }
            else kodiPage.handleCommand(command)
            return
        }
        if (activePage === 4) {
            if (command === inputManager.back) activePage = -1
            else tmdbAttribution.handleCommand(command)
            return
        }
        if (activePage === 0) {
            if (command === inputManager.back) {
                activePage = -1
                return
            }
            displayPage.handleCommand(command)
            return
        }
        if (activePage === 1) {
            if (command === inputManager.back) {
                if (audioPage.outputDraftActive) audioPage.handleCommand(command)
                else activePage = -1
                return
            }
            audioPage.handleCommand(command)
            return
        }
        if (activePage === 2) {
            if (command === inputManager.back) {
                if (ambientPage.pagePickerOpen || ambientPage.pathKeyboardOpen || ambientPage.sourcePanelOpen) {
                    ambientPage.handleCommand(command)
                    return
                }
                activePage = -1
                return
            }
            ambientPage.handleCommand(command)
            return
        }
        if (command === inputManager.navigateUp)
            selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown)
            selectedIndex = Math.min(rows.length - 1, selectedIndex + 1)
        else if (command === inputManager.confirm && rows[selectedIndex]) {
            const page = rows[selectedIndex].page
            if (page === 5) { if (setupState.restart()) sessionManager.goHome() }
            else if (page !== 3) activePage = page
        }
    }

    DisplaySettingsView { appearance: root.appearance;
        id: displayPage
        anchors.fill: parent
        visible: root.activePage === 0
    }

    AudioSettingsView { appearance: root.appearance;
        id: audioPage
        anchors.fill: parent
        visible: root.activePage === 1
    }

    AmbientSettingsView { appearance: root.appearance;
        id: ambientPage
        anchors.fill: parent
        visible: root.activePage === 2
        onPreviewRequested: root.previewRequested()
    }

    signal previewRequested()
    KodiSettingsView { id: kodiPage; anchors.fill: parent; visible: root.activePage === 6; appearance: root.appearance }
    ComponentSettingsView { appearance: root.appearance;
        id: componentPage; anchors.fill: parent; visible: root.activePage === 7
        onCaptureSetupRequested: { root.captureReturnPage=7; root.activePage = 8 }
    }
    CaptureSettingsView { appearance: root.appearance; id: capturePage; anchors.fill: parent; visible: root.activePage === 8 }
    TMDBAttributionView {
        id: tmdbAttribution
        anchors.fill: parent
        appearance: root.appearance
        visible: root.activePage === 4
        onBackRequested: root.activePage = -1
    }

    function revealSelection() {
        const row = rowsRepeater.itemAt(selectedIndex)
        if (!row) return
        const top=row.y, bottom=top+row.height
        if(top<scroll.contentY)scroll.contentY=top
        else if(bottom>scroll.contentY+scroll.height)scroll.contentY=Math.max(top,bottom-scroll.height)
        scroll.contentY=Math.max(0,Math.min(Math.max(0,scroll.contentHeight-scroll.height),scroll.contentY))
    }
    onSelectedIndexChanged:Qt.callLater(revealSelection)
    onHeightChanged:Qt.callLater(revealSelection)
    onWidthChanged:Qt.callLater(revealSelection)
    Flickable {
        id: scroll
        anchors.fill: parent
        contentHeight: contents.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        visible: root.activePage < 0
    Column {
        id: contents
        width: parent.width
        spacing: 18 * shell.gapScale

        Text { font.family: shell.fontFamily;
            text: "System & Support"
            color: shell.text
            font.pixelSize: 30 * shell.textScale
            font.weight: Font.DemiBold
            font.letterSpacing: 3
        }

        Text { font.family: shell.fontFamily;
            width: parent.width
            text: "Setup, diagnostics and information about this device."
            color: shell.muted
            font.pixelSize: 18 * shell.textScale
            wrapMode: Text.WordWrap
        }

        Repeater {
            id: rowsRepeater
            model: root.rows
            delegate: Rectangle {
                objectName:"systemSettingsRow-"+index
                required property int index
                required property var modelData
                width: parent.width
                height: Math.max(84*shell.textScale,rowLabels.implicitHeight+32,statusText.implicitHeight+32)
                radius: shell.radius
                color: root.selectedIndex === index ? shell.selected : shell.surface
                border.width: root.selectedIndex === index ? 3 : 1
                border.color: root.selectedIndex === index ? shell.selectedFocus : shell.outline

                Column {
                    id:rowLabels
                    anchors.left: parent.left
                    anchors.right: statusText.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 18
                    anchors.rightMargin: 16
                    spacing: 5 * shell.gapScale
                    Text { font.family: shell.fontFamily;
                        width:parent.width
                        wrapMode:Text.Wrap
                        text: modelData.name
                        color: (root.selectedIndex === index) ? shell.selectedText : shell.text
                        font.pixelSize: 21 * shell.textScale
                        font.weight: Font.DemiBold
                    }
                    Text { font.family: shell.fontFamily;
                        width: parent.width
                        text: modelData.detail
                        color: (root.selectedIndex === index) ? shell.selectedText : shell.muted
                        font.pixelSize: 14 * shell.textScale
                        wrapMode: Text.WordWrap
                    }
                }

                Text { font.family: shell.fontFamily;
                    id: statusText
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(190,parent.width*.3)
                    horizontalAlignment: Text.AlignRight
                    text: modelData.status
                    color: root.selectedIndex === index ? shell.selectedText : index === 3 ? shell.warning : shell.muted
                    font.pixelSize: 14 * shell.textScale
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle {
            width: parent.width
            height:footerText.implicitHeight+36
            radius: shell.radius
            color: shell.elevated
            border.color: shell.outline
            Text { font.family: shell.fontFamily;
                id:footerText
                width:parent.width-36
                x:18;y:18
                text: "The setup guide keeps your existing settings and profiles. Power and restart controls are unavailable in this build."
                color: shell.elevatedText
                font.pixelSize: 16 * shell.textScale
                wrapMode: Text.WordWrap
            }
        }
    }
    }
}
