import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    Rectangle {anchors.fill:parent;z:-1;color:shell.surface}
    property var appearance: ({})
    property var settingsModel: ambientSettings
    property var layoutModel: homeLayout
    property bool canPreview: true
    property int selectedIndex: 0
    property bool pagePickerOpen: false
    property int pagePickerIndex: 0
    property var draftPageIds: []
    readonly property var pageEntries: layoutModel.pages
    readonly property var pageChoices: pageEntries.map(page => (draftPageIds.indexOf(page.id)>=0 ? "✓ " : "○ ")+page.name).concat(["Save selection", "Cancel"])
    property bool sourcePanelOpen: false
    readonly property bool editing:pagePickerOpen || sourcePanelOpen || pathKeyboardOpen
    property bool pathKeyboardOpen: false
    property int sourceActionIndex: 0
    readonly property var sourceActions: ["Use managed folder", "Import sample photos", "Enter folder path", "Rescan photos", "Close"]
    readonly property var rows: [
        { name: "Ambient Mode", value: root.settingsModel.enabled ? "On" : "Off", help: "Shows a TV screensaver after the shell is idle." },
        { name: "Idle Timeout", value: root.settingsModel.timeoutMinutes + " min", help: "How long to wait before Ambient Mode starts." },
        { name: "Visual Mode", value: root.settingsModel.mode, help: "Slideshow, gradient, or dim screen." },
        { name: "Slideshow Rate", value: root.settingsModel.slideshowSeconds + " sec", help: "How often slideshow visuals change." },
        { name: "Photo Source", value: root.settingsModel.photoCount + " photos", help: root.settingsModel.photoDirectory },
        { name: "Image Fit", value: root.settingsModel.imageFit, help: "Fill, fit, or stretch slideshow photos." },
        { name: "Shuffle", value: root.settingsModel.shuffle ? "On" : "Off", help: "Randomize slideshow photo order." },
        { name: "Rescan Photos", value: "Scan", help: "Refresh the managed photo folder." },
        { name: "Clock Widget", value: root.settingsModel.showClock ? "Shown" : "Hidden", help: "Adds a large clock to Ambient Mode." },
        { name: "Date Widget", value: root.settingsModel.showDate ? "Shown" : "Hidden", help: "Adds the date under the clock." },
        { name: "System Hint", value: root.settingsModel.showSystemHint ? "Shown" : "Hidden", help: "Shows mode/rate and wake hint." },
        { name: "Preview Now", value: canPreview ? "Open" : "Inactive skin", help: canPreview ? "Immediately preview the screensaver." : "Activate this skin to preview its screensaver." },
        { name: "Reset Ambient", value: "Defaults", help: "Restore default screensaver settings." },
        { name: "Screensaver Pages", value: (root.settingsModel.pageIds || []).length + " selected", help: "Create pages with Skin Studio, then choose their rotation here. Select Pages as Visual Mode." }
    ]
    signal previewRequested()

    function handleCommand(command) {
        if (pagePickerOpen) {
            if (command === inputManager.navigateUp) pagePickerIndex = Math.max(0,pagePickerIndex-1)
            else if (command === inputManager.navigateDown) pagePickerIndex = Math.min(pageChoices.length-1,pagePickerIndex+1)
            else if (command === inputManager.back) pagePickerOpen = false
            else if (command === inputManager.confirm) {
                if (pagePickerIndex < pageEntries.length) {
                    const id=pageEntries[pagePickerIndex].id, next=draftPageIds.slice(), at=next.indexOf(id)
                    if(at>=0)next.splice(at,1);else next.push(id)
                    draftPageIds=next
                } else if(pagePickerIndex===pageEntries.length) {
                    if(root.settingsModel.setPageIds(draftPageIds))pagePickerOpen=false
                } else pagePickerOpen=false
            }
            return
        }
        if (pathKeyboardOpen) {
            pathKeyboard.handleCommand(command)
            return
        }
        if (sourcePanelOpen) {
            if (command === inputManager.navigateLeft || command === inputManager.navigateUp)
                sourceActionIndex = Math.max(0, sourceActionIndex - 1)
            else if (command === inputManager.navigateRight || command === inputManager.navigateDown)
                sourceActionIndex = Math.min(sourceActions.length - 1, sourceActionIndex + 1)
            else if (command === inputManager.back)
                sourcePanelOpen = false
            else if (command === inputManager.confirm) {
                if (sourceActionIndex === 0) root.settingsModel.useManagedPhotoDirectory()
                else if (sourceActionIndex === 1) root.settingsModel.importSamplePhotos()
                else if (sourceActionIndex === 2) {
                    pathKeyboard.value = root.settingsModel.photoDirectory
                    pathKeyboard.selectedIndex = 0
                    pathKeyboardOpen = true
                } else if (sourceActionIndex === 3) root.settingsModel.rescanPhotos()
                else if (sourceActionIndex === 4) sourcePanelOpen = false
            }
            return
        }
        if (command === inputManager.navigateUp)
            selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown)
            selectedIndex = Math.min(rows.length - 1, selectedIndex + 1)
        else if (command === inputManager.navigateLeft) {
            if (selectedIndex === 1) root.settingsModel.adjustTimeout(-1)
            else if (selectedIndex === 3) root.settingsModel.adjustSlideshowSeconds(-5)
        } else if (command === inputManager.navigateRight) {
            if (selectedIndex === 1) root.settingsModel.adjustTimeout(1)
            else if (selectedIndex === 3) root.settingsModel.adjustSlideshowSeconds(5)
        } else if (command === inputManager.confirm) {
            if (selectedIndex === 0) root.settingsModel.toggleEnabled()
            else if (selectedIndex === 2) root.settingsModel.nextMode()
            else if (selectedIndex === 4) {
                sourcePanelOpen = true
                sourceActionIndex = 0
            }
            else if (selectedIndex === 5) root.settingsModel.nextImageFit()
            else if (selectedIndex === 6) root.settingsModel.toggleShuffle()
            else if (selectedIndex === 7) root.settingsModel.rescanPhotos()
            else if (selectedIndex === 8) root.settingsModel.toggleClock()
            else if (selectedIndex === 9) root.settingsModel.toggleDate()
            else if (selectedIndex === 10) root.settingsModel.toggleSystemHint()
            else if (selectedIndex === 11 && canPreview) previewRequested()
            else if (selectedIndex === 12) root.settingsModel.reset()
            else if (selectedIndex === 13) {
                draftPageIds=(root.settingsModel.pageIds || []).slice();pagePickerIndex=0;pagePickerOpen=true
            }
        }
    }

    Rectangle {
        anchors.fill: parent; z: 20; visible: root.pagePickerOpen; color: shell.surface
        Flickable {
            anchors.fill: parent; contentHeight: pageList.height; clip: true
            contentY: Math.max(0,Math.min(Math.max(0,contentHeight-height),root.pagePickerIndex*64-height/2))
            Column {
                id: pageList; width: parent.width; spacing: 4
                Repeater {
                    model: root.pageChoices
                    delegate: Rectangle {
                        required property int index
                        required property string modelData
                        width: pageList.width; height: 60
                        color: root.pagePickerIndex===index?shell.selected:shell.surface
                        border.width: root.pagePickerIndex===index?3:0
                        border.color: shell.selectedFocus
                        Text {anchors.fill:parent;anchors.margins:12;verticalAlignment:Text.AlignVCenter;text:modelData;color:root.pagePickerIndex===index?shell.selectedText:shell.text;font.pixelSize:22*shell.textScale;elide:Text.ElideRight}
                    }
                }
                Text {width:parent.width;wrapMode:Text.Wrap;text:root.settingsModel.errorString;color:shell.error}
            }
        }
    }
    Flickable {
        anchors.fill: parent
        contentHeight: contentColumn.height
        contentY: Math.max(0, Math.min(Math.max(0, contentHeight - height), root.selectedIndex * 74 - 150))
        clip: true
        boundsBehavior: Flickable.StopAtBounds

    Column {
        id: contentColumn
        width: parent.width
        spacing: 18 * shell.gapScale
        Text { font.family: shell.fontFamily; text: "AMBIENT MODE"; color: shell.text; font.pixelSize: 30 * shell.textScale; font.weight: Font.DemiBold; font.letterSpacing: 3 }
        Text { font.family: shell.fontFamily; text: "Customize timeout, slideshow behavior, and screensaver widgets."; color: shell.muted; font.pixelSize: 18 * shell.textScale; wrapMode: Text.WordWrap; width: parent.width }

        Repeater {
            model: root.rows
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: parent.width
                height: 70
                radius: shell.radius
                color: root.selectedIndex === index ? shell.selected : shell.surface
                border.width: root.selectedIndex === index ? 3 : 1
                border.color: root.selectedIndex === index ? shell.selectedFocus : shell.outline
                Column {
                    anchors.left: parent.left
                    anchors.right: valueText.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 18
                    anchors.rightMargin: 16
                    spacing: 4 * shell.gapScale
                    Text { font.family: shell.fontFamily; text: modelData.name; color: (root.selectedIndex === index) ? shell.selectedText : shell.text; font.pixelSize: 20 * shell.textScale; font.weight: Font.DemiBold }
                    Text { font.family: shell.fontFamily; text: modelData.help; color: (root.selectedIndex === index) ? shell.selectedText : shell.muted; font.pixelSize: 13 * shell.textScale; elide: Text.ElideMiddle; width: parent.width }
                }
                Text { font.family: shell.fontFamily;
                    id: valueText
                    objectName: "ambientValue-" + index
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.value
                    color: root.selectedIndex === index ? shell.selectedText : shell.text
                    font.pixelSize: 16 * shell.textScale
                    font.weight: Font.DemiBold
                }
            }
        }

        Text { font.family: shell.fontFamily;
            text: "← → adjust values    Enter toggle/cycle/preview    Esc back"
            color: shell.muted
            font.pixelSize: 15 * shell.textScale
        }
        Text { font.family: shell.fontFamily;
            visible: root.settingsModel.errorString.length > 0
            text: root.settingsModel.errorString
            color: shell.error
            font.pixelSize: 15 * shell.textScale
            wrapMode: Text.WordWrap
            width: parent.width
        }
    }
    }

    Rectangle {
        visible: root.sourcePanelOpen
        anchors.fill: parent
        color: shell.scrim
        z: 10
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 70, 680)
            height: root.pathKeyboardOpen ? parent.height - 40 : 360
            radius: 14
            color: shell.elevated
            border.width: 2
            border.color: shell.elevatedFocus
            Column {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 14 * shell.gapScale
                Text { font.family: shell.fontFamily; text: "PHOTO SOURCE"; color: shell.elevatedText; font.pixelSize: 28 * shell.textScale; font.weight: Font.DemiBold; font.letterSpacing: 3 }
                Text { font.family: shell.fontFamily; text: root.settingsModel.photoCount + " photos · " + root.settingsModel.photoDirectory; color: shell.elevatedMuted; font.pixelSize: 15 * shell.textScale; elide: Text.ElideMiddle; width: parent.width }
                Repeater {
                    model: root.sourceActions
                    delegate: Rectangle {
                        required property int index
                        required property string modelData
                        width: parent.width
                        height: 42
                        radius: shell.radius
                        color: root.sourceActionIndex === index ? shell.selected : shell.surface
                        border.width: root.sourceActionIndex === index ? 2 : 1
                        border.color: root.sourceActionIndex === index ? shell.selectedFocus : shell.outline
                        Text { font.family: shell.fontFamily; anchors.centerIn: parent; text: modelData; color: (root.sourceActionIndex === index) ? shell.selectedText : shell.text; font.pixelSize: 16 * shell.textScale; font.weight: Font.DemiBold }
                    }
                }
                Text { font.family: shell.fontFamily; text: "Managed folder is the TV-safe default. Enter folder path supports advanced users until a full media picker exists."; color: shell.elevatedMuted; font.pixelSize: 13 * shell.textScale; wrapMode: Text.WordWrap; width: parent.width }
            }
        }
    }

    OnScreenKeyboard { appearance: root.appearance;
        id: pathKeyboard
        visible: root.pathKeyboardOpen
        z: 20
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 28
        height: Math.min(400, parent.height - 56)
        passwordMode: false
        onAccepted: text => {
            root.settingsModel.setPhotoDirectory(text)
            root.pathKeyboardOpen = false
        }
        onCanceled: root.pathKeyboardOpen = false
    }
}
