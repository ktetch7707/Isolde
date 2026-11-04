import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    Rectangle {anchors.fill:parent;z:-1;color:shell.surface}
    property var appearance: ({})
    property int selectedIndex: 0
    property bool editing: false
    property bool renaming: false
    readonly property var policyDestinations:typeof homeLayout!=="undefined"?homeLayout.studioPages.filter(p=>!!p.id && p.id!==homeLayout.homePageId):[]
    readonly property var policyEntries:[{name:"Allow layout editing: "+(profileManager.editingAllowed?"On":"Off"),policy:"edit"},{name:"Allow app installation: "+(profileManager.installsAllowed?"On":"Off"),policy:"install"}].concat(policyDestinations.map(p=>({name:(profileManager.restrictedDestinations.indexOf(p.id)>=0?"Restricted: ":"Allowed: ")+p.name,policy:"destination",id:p.id})))
    function activate(index) {
        selectedIndex = index
        if (index < profileManager.profiles.length) {
            profileManager.selectProfile(profileManager.profiles[index].id)
        } else if(index>=profileManager.profiles.length+2){
            const entry=policyEntries[index-profileManager.profiles.length-2]
            if(!entry)return
            if(entry.policy==="edit")profileManager.setEditingAllowed(!profileManager.editingAllowed)
            else if(entry.policy==="install")profileManager.setInstallsAllowed(!profileManager.installsAllowed)
            else{let ids=profileManager.restrictedDestinations.slice();const at=ids.indexOf(entry.id);if(at<0)ids.push(entry.id);else ids.splice(at,1);profileManager.setRestrictedDestinations(ids)}
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
        else if (command === inputManager.navigateDown) selectedIndex = Math.min(profileManager.profiles.length + 1 + policyEntries.length, selectedIndex + 1)
        else if (command === inputManager.confirm) activate(selectedIndex)
    }
    Column {
        anchors.fill: parent
        spacing: 14 * shell.gapScale
        Text { font.family: shell.fontFamily; text: "PROFILES"; color: shell.text; font.pixelSize: 30 * shell.textScale }
        Text { font.family: shell.fontFamily; text: "Using " + profileManager.activeName; color: shell.accent; font.pixelSize: 22 * shell.textScale }
        Text { font.family: shell.fontFamily;
            width: parent.width; wrapMode: Text.WordWrap; color: shell.muted; font.pixelSize: 17 * shell.textScale
            text: "Each person keeps their Home layout, screensaver, and app data. Switching closes the current session. Household restrictions prevent accidental changes; Home and Settings remain available for recovery. External apps manage their own purchases and access."
        }
        ListView {
            width: parent.width; height: Math.max(100, root.height - 230)
            clip: true; spacing: 8 * shell.gapScale; currentIndex: root.selectedIndex
            highlightMoveDuration: 0
            model: profileManager.profiles.concat([{name: "Add profile"}, {name: "Rename current profile"}]).concat(root.policyEntries)
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: ListView.view.width; height: 56; radius: shell.radius
                color: root.selectedIndex === index ? shell.selected : shell.surface
                border.color: root.selectedIndex === index ? shell.text : shell.outline
                border.width: root.selectedIndex === index ? 3 : 1
                Text { font.family: shell.fontFamily;
                    anchors.fill: parent; anchors.margins: 14; color: (root.selectedIndex === index) ? shell.selectedText : shell.text; font.pixelSize: 21 * shell.textScale
                    elide: Text.ElideRight
                    text: modelData.name + (modelData.id === profileManager.activeId ? "  •  Active" : "")
                }
                MouseArea { anchors.fill: parent; onClicked: root.activate(index) }
            }
        }
        Text { font.family: shell.fontFamily;
            width: parent.width; wrapMode: Text.WordWrap; color: shell.muted; font.pixelSize: 15 * shell.textScale
            text: "Native apps start with private account folders; existing desktop sign-ins are not imported. Network, volume, display, and installed apps remain shared."
        }
    }
    Rectangle {
        anchors.fill: parent; visible: root.editing; color: shell.elevated; z: 2
        Text { font.family: shell.fontFamily; text: root.renaming ? "Rename profile" : "New profile name"; color: shell.elevatedText; font.pixelSize: 24 * shell.textScale }
        OnScreenKeyboard { appearance: root.appearance;
            id: keyboard; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.topMargin: 48; height: 420
            onCanceled: root.editing = false
            onAccepted: text => {
                if (root.renaming ? profileManager.renameProfile(text) : profileManager.createProfile(text)) root.editing = false
            }
        }
    }
    Text { font.family: shell.fontFamily; anchors.bottom: parent.bottom; width: parent.width; wrapMode: Text.WordWrap; text: profileManager.error; color: shell.error; font.pixelSize: 16 * shell.textScale; z: 3 }
}
