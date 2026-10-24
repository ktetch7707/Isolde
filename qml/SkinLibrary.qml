import QtQuick

Rectangle {
    id: root
    objectName: "skinLibrary"
    required property var layoutModel
    property bool open: false
    property int selected: 0
    property int operation: 0
    property string naming: ""
    property bool confirming: false
    property string message: ""
    property var pageUpdates: []
    readonly property var entries: layoutModel.skins || []
    readonly property var choice: entries[selected] || ({})
    readonly property var operations: ["Apply skin", "Save current as new skin", "Rename saved skin", "Delete saved skin", "Update starter pages"]
    color: "#101014"; visible: open
    function begin() { selected = 0; operation = 0; naming = ""; confirming = false; message = ""; open = true }
    function execute() {
        message = ""
        if (operation === 4) {
            pageUpdates = layoutModel.skinPageUpdates(choice.id)
            if (!pageUpdates.length) { message = "No unchanged starter links need updating. Custom links are kept."; return }
            confirming = true
            return
        }
        if (operation === 1 || operation === 2) {
            if (operation === 2 && choice.builtIn) { message = "Save your own copy to give this design a new name."; return }
            naming = operation === 1 ? "save" : "rename"
            keyboard.value = operation === 1 ? "My skin" : choice.name
            keyboard.selectedIndex = 0
        } else if (operation === 3 && choice.builtIn) message = "Built-in skins are always available."
        else confirming = true
    }
    function confirmOperation() {
        const ok = operation === 4 ? layoutModel.applySkinPageUpdates(choice.id) : operation === 0 ? layoutModel.applySkin(choice.id) : layoutModel.deleteSkin(choice.id)
        confirming = false
        selected = Math.min(selected, entries.length - 1)
        message = ok ? operation === 4 ? "Starter links updated. Your design and existing pages are kept." : operation === 0 ? "Skin applied. Back returns to Home settings." : "Saved skin deleted. Your current Home is unchanged." : layoutModel.errorString
    }
    function finishName(text) {
        if (naming === "save") {
            const id = layoutModel.saveSkin(text)
            if (id) { for (let i = 0; i < entries.length; ++i) if (entries[i].id === id) selected = i; message = "Current Home saved with its artwork." }
            else { message = layoutModel.errorString; return }
        } else if (!layoutModel.renameSkin(choice.id, text)) { message = layoutModel.errorString; return }
        naming = ""
    }
    function handleCommand(command) {
        if (naming) { keyboard.handleCommand(command); return }
        if (confirming) {
            if (command === inputManager.back) confirming = false
            else if (command === inputManager.confirm) confirmOperation()
            return
        }
        if (command === inputManager.back) open = false
        else if (command === inputManager.navigateUp) selected = Math.max(0, selected - 1)
        else if (command === inputManager.navigateDown) selected = Math.min(entries.length - 1, selected + 1)
        else if (command === inputManager.navigateLeft) operation = Math.max(0, operation - 1)
        else if (command === inputManager.navigateRight) operation = Math.min(operations.length - 1, operation + 1)
        else if (command === inputManager.confirm) execute()
    }
    MouseArea { anchors.fill: parent }
    Column {
        anchors.fill: parent; anchors.margins: 40; spacing: 14
        Text { text: root.layoutModel.isPage ? "Page skins" : "Home skins"; color: "#F5F1F3"; font.pixelSize: 34 }
        Text { width: parent.width; wrapMode: Text.WordWrap; text: "A skin saves your Home layout, colors, typography, wallpaper and card artwork for this profile. Monitor and audio settings stay with the device."; color: "#AAA5AF"; font.pixelSize: 17 }
        Row {
            spacing: 10
            Repeater {
                model: root.operations
                delegate: Rectangle {
                    required property int index; required property string modelData
                    width: (root.width - 80 - 10 * (root.operations.length - 1)) / root.operations.length; height: 54; radius: 10
                    color: root.operation === index ? "#553C4C" : "#25242C"
                    border.color: root.operation === index ? "#E3ACC3" : "#39353D"
                    Text { anchors.centerIn: parent; width: parent.width - 12; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; text: modelData; color: "#F5F1F3"; font.pixelSize: 15 }
                    MouseArea { anchors.fill: parent; onClicked: { root.operation = index; root.execute() } }
                }
            }
        }
        ListView {
            width: parent.width; height: Math.max(100, parent.height - 250); spacing: 10; clip: true
            model: root.entries; currentIndex: root.selected; highlightRangeMode: ListView.ApplyRange
            delegate: Rectangle {
                required property int index; required property var modelData
                width: ListView.view.width; height: 82; radius: 12
                color: root.selected === index ? "#353039" : "#202126"
                border.width: root.selected === index ? 2 : 0; border.color: "#D19AB1"
                Column {
                    anchors.fill: parent; anchors.margins: 14; spacing: 5
                    Text { text: modelData.name; color: "#F5F1F3"; font.pixelSize: 22 }
                    Text { text: modelData.description; color: "#AAA5AF"; font.pixelSize: 15 }
                }
                MouseArea { anchors.fill: parent; onClicked: root.selected = index; onDoubleClicked: { root.selected = index; root.operation = 0; root.execute() } }
            }
        }
        Text { text: root.message || "↑ ↓ choose a skin · ← → choose an action · OK continues · Back closes"; width: parent.width; wrapMode: Text.WordWrap; color: "#E3ACC3"; font.pixelSize: 17 }
    }
    Rectangle {
        anchors.fill: parent; color: "#ED101014"; visible: root.confirming
        Column {
            anchors.centerIn: parent; width: parent.width - 160; spacing: 20
            Text { width: parent.width; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: "#F5F1F3"; font.pixelSize: 28; text: root.operation === 4 ? "Update starter pages?" : root.operation === 0 ? "Apply “" + root.choice.name + "”?" : "Delete saved skin “" + root.choice.name + "”?" }
            Text { width: parent.width; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: "#AAA5AF"; font.pixelSize: 20; text: root.operation === 4 ? "Update these unchanged starter links: " + root.pageUpdates.map(function(p) { return p.name }).join(", ") + ". Add their pages where missing. Keep your design, custom links and existing pages. Back writes nothing." : root.operation === 0 ? "This replaces the current Home design. Save it as a skin first if you want to keep it." : "The current Home design and its artwork will stay in place." }
            Row {
                spacing: 20
                Rectangle { width: 240; height: 56; radius: 10; color: "#553C4C"; Text { anchors.centerIn: parent; text: "OK · Confirm"; color: "white"; font.pixelSize: 20 } MouseArea { anchors.fill: parent; onClicked: root.confirmOperation() } }
                Rectangle { width: 240; height: 56; radius: 10; color: "#25242C"; Text { anchors.centerIn: parent; text: "Back · Cancel"; color: "white"; font.pixelSize: 20 } MouseArea { anchors.fill: parent; onClicked: root.confirming = false } }
            }
        }
    }
    Rectangle {
        anchors.fill: parent; visible: root.naming !== ""; color: "#101014"
        OnScreenKeyboard { id: keyboard; objectName: "skinNameKeyboard"; anchors.fill: parent; anchors.margins: 50; onAccepted: text => root.finishName(text); onCanceled: root.naming = "" }
    }
}
