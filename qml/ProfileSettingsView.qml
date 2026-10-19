import QtQuick

Item {
    id: root
    property int selectedIndex: 0
    property bool editing: false
    property bool renaming: false
    function activate(index) {
        selectedIndex = index
        if (index < profileManager.profiles.length) {
            profileManager.selectProfile(profileManager.profiles[index].id)
        } else {
            renaming = index === profileManager.profiles.length + 1
            keyboard.value = renaming ? profileManager.activeName : ""
            keyboard.selectedIndex = 0
            editing = true
        }
    }
    function handleCommand(command) {
        if (editing) { keyboard.handleCommand(command); return }
        if (command === inputManager.navigateUp) selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown) selectedIndex = Math.min(profileManager.profiles.length + 1, selectedIndex + 1)
        else if (command === inputManager.confirm) activate(selectedIndex)
    }
    Column {
        anchors.fill: parent
        spacing: 14
        Text { text: "PROFILES"; color: "#F8FAFC"; font.pixelSize: 30 }
        Text { text: "Using " + profileManager.activeName; color: "#9FC6FF"; font.pixelSize: 22 }
        Text {
            width: parent.width; wrapMode: Text.WordWrap; color: "#AAB5C9"; font.pixelSize: 17
            text: "Each person keeps their Home layout, screensaver, and app data. Switching closes the current session."
        }
        ListView {
            width: parent.width; height: Math.max(100, root.height - 230)
            clip: true; spacing: 8; currentIndex: root.selectedIndex
            highlightMoveDuration: 0
            model: profileManager.profiles.concat([{name: "Add profile"}, {name: "Rename current profile"}])
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: ListView.view.width; height: 56; radius: 8
                color: root.selectedIndex === index ? "#2B416B" : "#172238"
                border.color: root.selectedIndex === index ? "#F8FAFC" : "#354462"
                border.width: root.selectedIndex === index ? 3 : 1
                Text {
                    anchors.fill: parent; anchors.margins: 14; color: "#F8FAFC"; font.pixelSize: 21
                    elide: Text.ElideRight
                    text: modelData.name + (modelData.id === profileManager.activeId ? "  •  Active" : "")
                }
                MouseArea { anchors.fill: parent; onClicked: root.activate(index) }
            }
        }
        Text {
            width: parent.width; wrapMode: Text.WordWrap; color: "#AAB5C9"; font.pixelSize: 15
            text: "Native apps start with private account folders; existing desktop sign-ins are not imported. Network, volume, display, and installed apps remain shared."
        }
    }
    Rectangle {
        anchors.fill: parent; visible: root.editing; color: "#0F1728"; z: 2
        Text { text: root.renaming ? "Rename profile" : "New profile name"; color: "white"; font.pixelSize: 24 }
        OnScreenKeyboard {
            id: keyboard; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.topMargin: 48; height: 420
            onCanceled: root.editing = false
            onAccepted: text => {
                if (root.renaming ? profileManager.renameProfile(text) : profileManager.createProfile(text)) root.editing = false
            }
        }
    }
    Text { anchors.bottom: parent.bottom; width: parent.width; wrapMode: Text.WordWrap; text: profileManager.error; color: "#FFB4A8"; font.pixelSize: 16; z: 3 }
}
