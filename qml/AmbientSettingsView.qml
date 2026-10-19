import QtQuick

Item {
    id: root
    property int selectedIndex: 0
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
        { name: "Reset Ambient", value: "Defaults", help: "Restore default screensaver settings." }
    ]
    signal previewRequested()

    function handleCommand(command) {
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
        spacing: 18
        Text { text: "AMBIENT MODE"; color: "#F8FAFC"; font.pixelSize: 30; font.weight: Font.DemiBold; font.letterSpacing: 3 }
        Text { text: "Customize timeout, slideshow behavior, and screensaver widgets."; color: "#9EABC1"; font.pixelSize: 18; wrapMode: Text.WordWrap; width: parent.width }

        Repeater {
            model: root.rows
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: parent.width
                height: 70
                radius: 10
                color: root.selectedIndex === index ? "#2B416B" : "#111A2A"
                border.width: root.selectedIndex === index ? 3 : 1
                border.color: root.selectedIndex === index ? "#F8FAFC" : "#2B3853"
                Column {
                    anchors.left: parent.left
                    anchors.right: valueText.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 18
                    anchors.rightMargin: 16
                    spacing: 4
                    Text { text: modelData.name; color: "#F8FAFC"; font.pixelSize: 20; font.weight: Font.DemiBold }
                    Text { text: modelData.help; color: "#9EABC1"; font.pixelSize: 13; elide: Text.ElideMiddle; width: parent.width }
                }
                Text {
                    id: valueText
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.value
                    color: "#D5B083"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
            }
        }

        Text {
            text: "← → adjust values    Enter toggle/cycle/preview    Esc back"
            color: "#91A0BC"
            font.pixelSize: 15
        }
        Text {
            visible: ambientSettings.errorString.length > 0
            text: ambientSettings.errorString
            color: "#E85C68"
            font.pixelSize: 15
            wrapMode: Text.WordWrap
            width: parent.width
        }
    }
    }

    Rectangle {
        visible: root.sourcePanelOpen
        anchors.fill: parent
        color: "#CC050812"
        z: 10
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 70, 680)
            height: root.pathKeyboardOpen ? parent.height - 40 : 360
            radius: 14
            color: "#151E32"
            border.width: 2
            border.color: "#4D7ED8"
            Column {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 14
                Text { text: "PHOTO SOURCE"; color: "#F8FAFC"; font.pixelSize: 28; font.weight: Font.DemiBold; font.letterSpacing: 3 }
                Text { text: ambientSettings.photoCount + " photos · " + ambientSettings.photoDirectory; color: "#9EABC1"; font.pixelSize: 15; elide: Text.ElideMiddle; width: parent.width }
                Repeater {
                    model: root.sourceActions
                    delegate: Rectangle {
                        required property int index
                        required property string modelData
                        width: parent.width
                        height: 42
                        radius: 8
                        color: root.sourceActionIndex === index ? "#2B416B" : "#111A2A"
                        border.width: root.sourceActionIndex === index ? 2 : 1
                        border.color: root.sourceActionIndex === index ? "#F8FAFC" : "#2B3853"
                        Text { anchors.centerIn: parent; text: modelData; color: "#F8FAFC"; font.pixelSize: 16; font.weight: Font.DemiBold }
                    }
                }
                Text { text: "Managed folder is the TV-safe default. Enter folder path supports advanced users until a full media picker exists."; color: "#91A0BC"; font.pixelSize: 13; wrapMode: Text.WordWrap; width: parent.width }
            }
        }
    }

    OnScreenKeyboard {
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
