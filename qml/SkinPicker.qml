import QtQuick

Rectangle {
    id: root
    objectName: "skinPicker"
    required property var layoutModel
    property bool open: false
    property int selected: 0
    property int operation: 0
    property bool confirming: false
    property string message: ""
    signal closed()
    signal editSkin(string id)
    signal addSkin()
    readonly property var entries: layoutModel.skins || []
    readonly property var choice: entries[selected] || ({})
    readonly property var operations: ["Apply", "Edit", "Duplicate", "Delete"]
    color: "#101014"; visible: open
    function begin() { selected = 0; operation = 0; confirming = false; message = ""; open = true }
    function execute() {
        message = ""
        if (operation === 0) {
            if (layoutModel.applySkin(choice.id)) { message = "Skin applied."; open = false; emit closed() }
            else { message = layoutModel.errorString; return }
        } else if (operation === 1) {
            emit editSkin(choice.id); open = false; emit closed()
        } else if (operation === 2) {
            naming = "duplicate"; namingVisible = true
        } else if (operation === 3) {
            if (choice.builtIn) { message = "Built-in skins cannot be deleted."; return }
            confirming = true
        }
    }
    property string naming: ""
    property bool namingVisible: false
    function confirmDuplicate(text) {
        if (!text.trim()) { message = "Give the skin a name."; return }
        const id = layoutModel.saveSkin(text.trim())
        if (id) { message = "Skin saved as \"" + text.trim() + "\"."; namingVisible = false; naming = "" }
        else { message = layoutModel.errorString; return }
    }
    function confirmDelete() {
        const ok = layoutModel.deleteSkin(choice.id)
        confirming = false
        selected = Math.min(selected, Math.max(0, entries.length - 1))
        message = ok ? "Skin deleted." : layoutModel.errorString
        if (ok) emit closed()
    }
    function handleCommand(command) {
        if (namingVisible) { return }
        if (confirming) {
            if (command === inputManager.back) confirming = false
            else if (command === inputManager.confirm) confirmDelete()
            return
        }
        if (command === inputManager.back) { open = false; emit closed(); return }
        else if (command === inputManager.navigateUp) selected = Math.max(0, selected - 1)
        else if (command === inputManager.navigateDown) {
            const last = entries.length > 0 ? entries.length : 0
            selected = Math.min(last, selected + 1)
        }
        else if (command === inputManager.navigateLeft) operation = Math.max(0, operation - 1)
        else if (command === inputManager.navigateRight) operation = Math.min(operations.length - 1, operation + 1)
        else if (command === inputManager.confirm) execute()
    }
    MouseArea { anchors.fill: parent }
    Column {
        anchors.fill: parent; anchors.margins: 40; spacing: 14
        Text { text: root.layoutModel.isPage ? "Page skins" : "Home skins"; color: "#F5F1F3"; font.pixelSize: 34 }
        Text { width: parent.width; wrapMode: Text.Wrap; text: "Choose a skin, then apply, edit, duplicate or delete. Add creates a new skin from your current Home."; color: "#AAA5AF"; font.pixelSize: 17 }
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
            id: skinList
            width: parent.width; height: Math.max(100, parent.height - 320); spacing: 10; clip: true
            model: root.entries; currentIndex: root.selected; highlightRangeMode: ListView.ApplyRange
            delegate: Rectangle {
                required property int index; required property var modelData
                width: ListView.view.width; height: 82; radius: 12
                color: root.selected === index ? "#353039" : "#202126"
                border.width: root.selected === index ? 2 : 0; border.color: "#D19AB1"
                Column {
                    anchors.fill: parent; anchors.margins: 14; spacing: 5
                    Text { text: modelData.name; color: "#F5F1F3"; font.pixelSize: 22 }
                    Text { text: modelData.builtIn ? "Built-in · " + modelData.description : modelData.description; color: "#AAA5AF"; font.pixelSize: 15 }
                }
                MouseArea { anchors.fill: parent; onClicked: { root.selected = index; root.operation = 0; root.execute() } }
            }
        }
        Rectangle {
            width: parent.width; height: 64; radius: 10; color: "#25242C"; border.width: 1; border.color: "#39353D"
            Text { anchors.centerIn: parent; text: "+ Add new skin"; color: "#F5F1F3"; font.pixelSize: 20 }
            MouseArea { anchors.fill: parent; onClicked: { root.addSkin(); root.open = false; root.emit root.closed() } }
        }
        Text { text: root.message || "↑ ↓ choose a skin · ← → choose an action · OK continues · Back closes"; width: parent.width; wrapMode: Text.Wrap; color: "#E3ACC3"; font.pixelSize: 17 }
        Rectangle {
            anchors.fill: parent; visible: root.confirming; color: "#ED101014"
            Column {
                anchors.centerIn: parent; width: parent.width - 160; spacing: 20
                Text { width: parent.width; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: "#F5F1F3"; font.pixelSize: 28; text: "Delete “" + root.choice.name + "”?" }
                Text { width: parent.width; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: "#AAA5AF"; font.pixelSize: 20; text: "The current Home design and its artwork will stay in place. Back writes nothing." }
                Row {
                    spacing: 20
                    Rectangle { width: 240; height: 56; radius: 10; color: "#553C4C"; Text { anchors.centerIn: parent; text: "OK · Confirm"; color: "white"; font.pixelSize: 20 } MouseArea { anchors.fill: parent; onClicked: root.confirmDelete() } }
                    Rectangle { width: 240; height: 56; radius: 10; color: "#25242C"; Text { anchors.centerIn: parent; text: "Back · Cancel"; color: "white"; font.pixelSize: 20 } MouseArea { anchors.fill: parent; onClicked: root.confirming = false } }
                }
            }
        }
        Rectangle {
            anchors.fill: parent; visible: root.namingVisible; color: "#101014"
            Column {
                anchors.centerIn: parent; width: parent.width - 160; spacing: 20
                Text { text: "Name the duplicate"; color: "#F5F1F3"; font.pixelSize: 28 }
                Text { text: keyboard.value; color: "#BD8198"; font.pixelSize: 24 }
                Row { spacing: 20
                    Rectangle { width: 240; height: 56; radius: 10; color: "#06B6D4"; Text { anchors.centerIn: parent; text: "Save"; color: "#101014"; font.pixelSize: 20 } MouseArea { anchors.fill: parent; onClicked: { root.confirmDuplicate(keyboard.value); root.namingVisible = false } } }
                    Rectangle { width: 240; height: 56; radius: 10; color: "#25242C"; Text { anchors.centerIn: parent; text: "Cancel"; color: "white"; font.pixelSize: 20 } MouseArea { anchors.fill: parent; onClicked: root.namingVisible = false } }
                }
            }
        }
    }
    OnScreenKeyboard { id: keyboard; anchors.fill: parent; anchors.margins: 50; onAccepted: text => { if (root.namingVisible) root.confirmDuplicate(text); root.namingVisible = false }; onCanceled: root.namingVisible = false }
}