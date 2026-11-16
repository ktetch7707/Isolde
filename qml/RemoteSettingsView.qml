import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    Rectangle {anchors.fill:parent;z:-1;color:shell.surface}
    property var appearance: ({})
    property int selectedIndex: 0
    property int reviewIndex: 0
    property string dialog: ""
    readonly property bool editing: inputManager.bindingActive || dialog !== ""
    readonly property string profileLabel: remoteSettings.profile === "media" ? "Media remote" : remoteSettings.profile === "gamepad" ? "Gamepad" : "Simple TV remote"
    readonly property string powerLabel: remoteSettings.powerAction === "tv-standby" ? "TV standby" : remoteSettings.powerAction === "tv-on" ? "TV on" : "Ambient toggle"
    readonly property var setupRows: [
        { name: "Setup Mode", value: remoteSettings.setupMode ? "Listening" : "Off", help: "Start a friendly pairing/test flow for a physical remote." },
        { name: "Remote Profile", value: profileLabel, help: "Choose the expected remote style." },
        { name: "Test Button", value: remoteSettings.lastInput, help: "When Setup Mode is on, recognized remote buttons appear here live." },
        { name: "Power Button", value: powerLabel, help: "Choose what the remote power button should do." },
        { name: "HDMI-CEC", value: remoteSettings.cecAvailable ? "Available" : "Unavailable", help: remoteSettings.cecStatus },
        { name: "Test TV On", value: "Send", help: "Try to wake the TV through HDMI-CEC." },
        { name: "Test TV Standby", value: "Send", help: "Try to put the TV in standby through HDMI-CEC." },
        { name: "Reset Remote Setup", value: "Defaults", help: "Return to the simple TV remote profile." },
        { name: "Accept Keyboard Keys", value: inputManager.acceptTextKeys ? "Text when typing" : "Remote controls", help: "For remotes with a rear keyboard: type characters while a text keyboard is open. Turn off for letter-based controls." }
    ]
    readonly property var rows: inputManager.bindingRows.map(function(row) {
        return {name: row.name, value: row.value, category: row.category, command: row.command,
            help: row.custom ? "Custom button · Select to change · Menu to restore default" : "Default buttons · Select, then press your remote button"}
    }).concat([{name: "Restore all default bindings", value: "Review", category: "Recovery", help: "Remove this profile's custom buttons after confirmation."}], setupRows)

    onVisibleChanged: if (!visible) { if (inputManager.bindingActive) inputManager.cancelBinding(); dialog = "" }

    function handleCommand(command) {
        if (editing) {
            if (inputManager.listening) return
            const count = dialog ? 2 : 3
            if (command === inputManager.back || command === inputManager.home) {
                inputManager.cancelBinding(); dialog = ""; return
            }
            if (command === inputManager.navigateUp) reviewIndex = Math.max(0, reviewIndex - 1)
            else if (command === inputManager.navigateDown) reviewIndex = Math.min(count - 1, reviewIndex + 1)
            else if (command === inputManager.confirm) {
                if (dialog) {
                    if (reviewIndex === 1) { dialog = ""; return }
                    const ok = dialog === "reset" ? inputManager.resetBindings() : inputManager.clearBinding(rows[selectedIndex].command)
                    if (ok) dialog = ""
                } else if (reviewIndex === 0) inputManager.saveBinding()
                else if (reviewIndex === 1) inputManager.beginBinding(rows[selectedIndex].command)
                else inputManager.cancelBinding()
            }
            return
        }
        if (command === inputManager.menu && rows[selectedIndex].command) {
            dialog = "clear"; reviewIndex = 1; return
        }
        if (command === inputManager.navigateUp)
            selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown)
            selectedIndex = Math.min(rows.length - 1, selectedIndex + 1)
        else if (command === inputManager.confirm) {
            if (rows[selectedIndex].command) { reviewIndex = 0; inputManager.beginBinding(rows[selectedIndex].command); return }
            const index = selectedIndex - inputManager.bindingRows.length - 1
            if (index === -1) { dialog = "reset"; reviewIndex = 1 }
            else if (index === 0) remoteSettings.toggleSetupMode()
            else if (index === 1) remoteSettings.nextProfile()
            else if (index === 2) remoteSettings.recordTestInput("OK / Select")
            else if (index === 3) remoteSettings.nextPowerAction()
            else if (index === 4) remoteSettings.refreshCec()
            else if (index === 5) remoteSettings.turnTvOn()
            else if (index === 6) remoteSettings.putTvInStandby()
            else if (index === 7) remoteSettings.reset()
            else if (index === 8) inputManager.setAcceptTextKeys(!inputManager.acceptTextKeys)
        }
    }

    onSelectedIndexChanged: {
        const row = rowsRepeater.itemAt(selectedIndex)
        if (row) scroll.contentY = Math.max(0, Math.min(scroll.contentHeight - scroll.height, row.y - 20))
    }
    Flickable {
        id: scroll
        anchors.fill: parent
        contentHeight: contents.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
    Column {
        id: contents
        width: parent.width
        spacing: 18 * shell.gapScale

        Text { font.family: shell.fontFamily; text: "REMOTE SETUP"; color: shell.text; font.pixelSize: 30 * shell.textScale; font.weight: Font.DemiBold; font.letterSpacing: 3 }
        Text { font.family: shell.fontFamily;
            width: parent.width
            text: "Choose an action, press a button on your remote, then review and save the binding. Standard keyboard controls remain available for recovery."
            color: shell.muted
            font.pixelSize: 18 * shell.textScale
            wrapMode: Text.WordWrap
        }

        Rectangle {
            width: parent.width
            height: 128
            radius: 14
            color: shell.background
            border.color: shell.backgroundFocus

            Column {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 8 * shell.gapScale
                Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; text: inputManager.errorString || inputManager.bindingStatus || remoteSettings.setupStatus; color: shell.backgroundText; font.pixelSize: 20 * shell.textScale; font.weight: Font.DemiBold }
                Text { font.family: shell.fontFamily;
                    width: parent.width
                    text: "Bindings apply to the active Isolde profile. Select changes a button; Menu restores that action's default."
                    color: shell.backgroundMuted
                    font.pixelSize: 15 * shell.textScale
                    wrapMode: Text.WordWrap
                }
            }
        }

        Repeater {
            id: rowsRepeater
            model: root.rows
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: parent.width
                readonly property bool groupStart: !!modelData.category && (index === 0 || root.rows[index - 1].category !== modelData.category)
                height: (groupStart ? 108 : 76) * shell.textScale
                radius: shell.radius
                color: root.selectedIndex === index ? shell.selected : shell.surface
                border.width: root.selectedIndex === index ? 3 : 1
                border.color: root.selectedIndex === index ? shell.selectedFocus : shell.outline

                Column {
                    anchors.left: parent.left
                    anchors.right: valueText.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: parent.groupStart ? 14 * shell.textScale : 0
                    anchors.leftMargin: 18
                    anchors.rightMargin: 16
                    spacing: 3 * shell.gapScale
                    Text { font.family: shell.fontFamily; text: modelData.name; color: (root.selectedIndex === index) ? shell.selectedText : shell.text; font.pixelSize: 20 * shell.textScale; font.weight: Font.DemiBold }
                    Text { font.family: shell.fontFamily; width: parent.width; text: modelData.help; color: (root.selectedIndex === index) ? shell.selectedText : shell.muted; font.pixelSize: 13 * shell.textScale; elide: Text.ElideRight }
                }
                Text { font.family: shell.fontFamily; visible: parent.groupStart; anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 12 * shell.textScale; text: modelData.category || ""; color: root.selectedIndex === index ? shell.selectedText : shell.muted; font.pixelSize: 15 * shell.textScale; font.weight: Font.DemiBold }

                Text { font.family: shell.fontFamily;
                    id: valueText
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: parent.groupStart ? 14 * shell.textScale : 0
                    width: 210
                    horizontalAlignment: Text.AlignRight
                    text: modelData.value
                    color: (root.selectedIndex === index) ? shell.selectedText : shell.text
                    font.pixelSize: 16 * shell.textScale
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }
            }
        }

        Text { font.family: shell.fontFamily;
            width: parent.width
            text: "Supports buttons delivered to Isolde as keyboard/media-key events, including compatible USB, Bluetooth and receiver remotes. Pairing and raw IR/CEC input require a supported device backend. Custom bindings here control Isolde screens; external apps retain their existing controls."
            color: shell.unavailable
            font.pixelSize: 15 * shell.textScale
            wrapMode: Text.WordWrap
        }
    }
    }
    Rectangle {
        anchors.fill: parent
        visible: root.editing
        color: shell.surface
        border.color: shell.selectedFocus
        border.width: 3
        radius: shell.radius
        Column {
            anchors.centerIn: parent
            width: parent.width * 0.84
            spacing: 20 * shell.gapScale
            Text { width: parent.width; text: inputManager.listening ? "PRESS A REMOTE BUTTON" : root.dialog ? "RESTORE DEFAULT BUTTONS?" : "REVIEW BUTTON BINDING"; color: shell.text; font.family: shell.fontFamily; font.pixelSize: 26 * shell.textScale; wrapMode: Text.WordWrap }
            Text { width: parent.width; text: root.dialog ? (root.dialog === "reset" ? "Remove all custom bindings from this profile? Standard buttons will remain available." : "Remove the custom button for " + root.rows[root.selectedIndex].name + "?") : inputManager.bindingStatus; color: shell.text; font.family: shell.fontFamily; font.pixelSize: 20 * shell.textScale; wrapMode: Text.WordWrap }
            Text { width: parent.width; text: inputManager.errorString; visible: text !== ""; color: shell.text; font.family: shell.fontFamily; font.pixelSize: 18 * shell.textScale; wrapMode: Text.WordWrap }
            Repeater {
                model: inputManager.listening ? [] : root.dialog ? ["Restore defaults", "Cancel"] : ["Save binding", "Choose another button", "Cancel"]
                Rectangle {
                    required property int index
                    required property string modelData
                    width: parent.width; height: 52 * shell.textScale; radius: shell.radius
                    color: root.reviewIndex === index ? shell.selected : shell.background
                    border.width: root.reviewIndex === index ? 3 : 1
                    border.color: root.reviewIndex === index ? shell.selectedFocus : shell.outline
                    Text { anchors.centerIn: parent; text: modelData; font.family: shell.fontFamily; font.pixelSize: 20 * shell.textScale; color: root.reviewIndex === index ? shell.selectedText : shell.backgroundText }
                }
            }
        }
    }
}
