import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    Rectangle {anchors.fill:parent;z:-1;color:shell.surface}
    property var appearance: ({})
    property int selectedIndex: 0
    property bool pagePickerOpen: false
    property int pagePickerIndex: 0
    property var draftPageIds: []
    readonly property var pageEntries: homeLayout.pages
    readonly property var pageChoices: pageEntries.map(page => (draftPageIds.indexOf(page.id)>=0 ? "✓ " : "○ ")+page.name).concat(["Save selection", "Cancel"])
    property bool sourcePanelOpen: false
    property bool pathKeyboardOpen: false
    property int sourceActionIndex: 0
    readonly property var sourceActions: ["Use managed folder", "Import sample photos", "Enter folder path", "Rescan photos", "Close"]
    readonly property var rows: [
        { name: "Ambient Mode", value: ambientSettings.enabled ? "On" : "Off", help: "Shows a TV screensaver after the shell is idle." },
        { name: "Idle Timeout", value: ambientSettings.timeoutMinutes + " min", help: "How long to wait before Ambient Mode starts." },
        { name: "Visual Mode", value: ambientSettings.mode, help: "Slideshow, gradient, or dim screen." },
        { name: "Slideshow Rate", value: ambientSettings.slideshowSeconds + " sec", help: "How often slideshow visuals change." },
        { name: "Photo Source", value: ambientSettings.photoCount + " photos", help: ambientSettings.photoDirectory },
        { name: "Image Fit", value: ambientSettings.imageFit, help: "Fill, fit, or stretch slideshow photos." },
        { name: "Shuffle", value: ambientSettings.shuffle ? "On" : "Off", help: "Randomize slideshow photo order." },
        { name: "Rescan Photos", value: "Scan", help: "Refresh the managed photo folder." },
        { name: "Clock Widget", value: ambientSettings.showClock ? "Shown" : "Hidden", help: "Adds a large clock to Ambient Mode." },
        { name: "Date Widget", value: ambientSettings.showDate ? "Shown" : "Hidden", help: "Adds the date under the clock." },
        { name: "System Hint", value: ambientSettings.showSystemHint ? "Shown" : "Hidden", help: "Shows mode/rate and wake hint." },
        { name: "Preview Now", value: "Open", help: "Immediately preview the screensaver." },
        { name: "Reset Ambient", value: "Defaults", help: "Restore default screensaver settings." },
        { name: "Screensaver Pages", value: (ambientSettings.pageIds || []).length + " selected", help: "Create pages with Page Studio, then choose their rotation here. Select Pages as Visual Mode." }
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
                    if(ambientSettings.setPageIds(draftPageIds))pagePickerOpen=false
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
                if (sourceActionIndex === 0) ambientSettings.useManagedPhotoDirectory()
                else if (sourceActionIndex === 1) ambientSettings.importSamplePhotos()
                else if (sourceActionIndex === 2) {
                    pathKeyboard.value = ambientSettings.photoDirectory
                    pathKeyboard.selectedIndex = 0
                    pathKeyboardOpen = true
                } else if (sourceActionIndex === 3) ambientSettings.rescanPhotos()
                else if (sourceActionIndex === 4) sourcePanelOpen = false
            }
            return
        }
        if (command === inputManager.navigateUp)
            selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown)
            selectedIndex = Math.min(rows.length - 1, selectedIndex + 1)
        else if (command === inputManager.navigateLeft) {
            if (selectedIndex === 1) ambientSettings.adjustTimeout(-1)
            else if (selectedIndex === 3) ambientSettings.adjustSlideshowSeconds(-5)
        } else if (command === inputManager.navigateRight) {
            if (selectedIndex === 1) ambientSettings.adjustTimeout(1)
            else if (selectedIndex === 3) ambientSettings.adjustSlideshowSeconds(5)
        } else if (command === inputManager.confirm) {
            if (selectedIndex === 0) ambientSettings.toggleEnabled()
            else if (selectedIndex === 2) ambientSettings.nextMode()
            else if (selectedIndex === 4) {
                sourcePanelOpen = true
                sourceActionIndex = 0
            }
            else if (selectedIndex === 5) ambientSettings.nextImageFit()
            else if (selectedIndex === 6) ambientSettings.toggleShuffle()
            else if (selectedIndex === 7) ambientSettings.rescanPhotos()
            else if (selectedIndex === 8) ambientSettings.toggleClock()
            else if (selectedIndex === 9) ambientSettings.toggleDate()
            else if (selectedIndex === 10) ambientSettings.toggleSystemHint()
            else if (selectedIndex === 11) previewRequested()
            else if (selectedIndex === 12) ambientSettings.reset()
            else if (selectedIndex === 13) {
                draftPageIds=(ambientSettings.pageIds || []).slice();pagePickerIndex=0;pagePickerOpen=true
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
                Text {width:parent.width;wrapMode:Text.Wrap;text:ambientSettings.errorString;color:shell.error}
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
            visible: ambientSettings.errorString.length > 0
            text: ambientSettings.errorString
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
                Text { font.family: shell.fontFamily; text: ambientSettings.photoCount + " photos · " + ambientSettings.photoDirectory; color: shell.elevatedMuted; font.pixelSize: 15 * shell.textScale; elide: Text.ElideMiddle; width: parent.width }
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
            ambientSettings.setPhotoDirectory(text)
            root.pathKeyboardOpen = false
        }
        onCanceled: root.pathKeyboardOpen = false
    }
}
