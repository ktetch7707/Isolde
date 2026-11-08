import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    Rectangle {anchors.fill:parent;z:-1;color:shell.surface}
    property var appearance: ({})
    property int selectedIndex: 0
    readonly property string profileLabel: remoteSettings.profile === "media" ? "Media remote" : remoteSettings.profile === "gamepad" ? "Gamepad" : "Simple TV remote"
    readonly property string powerLabel: remoteSettings.powerAction === "tv-standby" ? "TV standby" : remoteSettings.powerAction === "tv-on" ? "TV on" : "Ambient toggle"
    readonly property var rows: [
        { name: "Setup Mode", value: remoteSettings.setupMode ? "Listening" : "Off", help: "Start a friendly pairing/test flow for a physical remote." },
        { name: "Remote Profile", value: profileLabel, help: "Choose the expected remote style before advanced remapping exists." },
        { name: "Test Button", value: remoteSettings.lastInput, help: "When Setup Mode is on, recognized remote buttons appear here live." },
        { name: "Power Button", value: powerLabel, help: "Choose what the remote power button should do." },
        { name: "HDMI-CEC", value: remoteSettings.cecAvailable ? "Available" : "Unavailable", help: remoteSettings.cecStatus },
        { name: "Test TV On", value: "Send", help: "Try to wake the TV through HDMI-CEC." },
        { name: "Test TV Standby", value: "Send", help: "Try to put the TV in standby through HDMI-CEC." },
        { name: "Reset Remote Setup", value: "Defaults", help: "Return to the simple TV remote profile." }
    ]

    function handleCommand(command) {
        if (command === inputManager.navigateUp)
            selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown)
            selectedIndex = Math.min(rows.length - 1, selectedIndex + 1)
        else if (command === inputManager.confirm) {
            if (selectedIndex === 0) remoteSettings.toggleSetupMode()
            else if (selectedIndex === 1) remoteSettings.nextProfile()
            else if (selectedIndex === 2) remoteSettings.recordTestInput("OK / Select")
            else if (selectedIndex === 3) remoteSettings.nextPowerAction()
            else if (selectedIndex === 4) remoteSettings.refreshCec()
            else if (selectedIndex === 5) remoteSettings.turnTvOn()
            else if (selectedIndex === 6) remoteSettings.putTvInStandby()
            else if (selectedIndex === 7) remoteSettings.reset()
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
            text: "Test remote buttons, select an input profile, and configure available HDMI-CEC commands. Custom button remapping is not available yet."
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
                Text { font.family: shell.fontFamily; text: remoteSettings.setupStatus; color: shell.backgroundText; font.pixelSize: 24 * shell.textScale; font.weight: Font.DemiBold }
                Text { font.family: shell.fontFamily;
                    width: parent.width
                    text: "Minimum working remote: Up, Down, Left, Right, OK, Back, Home. Power can toggle Ambient Mode now or control the TV later through HDMI-CEC."
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
                height: 72
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
                    spacing: 3 * shell.gapScale
                    Text { font.family: shell.fontFamily; text: modelData.name; color: (root.selectedIndex === index) ? shell.selectedText : shell.text; font.pixelSize: 20 * shell.textScale; font.weight: Font.DemiBold }
                    Text { font.family: shell.fontFamily; width: parent.width; text: modelData.help; color: (root.selectedIndex === index) ? shell.selectedText : shell.muted; font.pixelSize: 13 * shell.textScale; elide: Text.ElideRight }
                }

                Text { font.family: shell.fontFamily;
                    id: valueText
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
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
            text: "Current backend: keyboard/HID-equivalent input plus safe cec-client detection/commands. Follow-on: evdev/libinput device discovery, Bluetooth pairing, CEC remote events, remap profiles, and guided troubleshooting."
            color: shell.unavailable
            font.pixelSize: 15 * shell.textScale
            wrapMode: Text.WordWrap
        }
    }
    }
}
