import QtQuick

Item {
    id: root
    property var appearance: ({})
    property int selectedIndex: 0
    property int activePage: -1
    readonly property var rows: [
        { name: "Display", detail: "Safe area and interface scale", status: "TV controls" },
        { name: "Audio", detail: "Volume, mute, startup volume, and remote volume step", status: audioSettings.muted ? "Muted" : audioSettings.volume + "%" },
        { name: "Ambient Mode", detail: "Idle timeout, slideshow, clock/date widgets, and screen saver preview", status: ambientSettings.enabled ? ambientSettings.timeoutMinutes + " min" : "Off" },
        { name: "Power", detail: "Appliance power and OS restart controls are not available yet", status: "Unavailable" },
        { name: "About Isolde", detail: "Current development version", status: "Isolde " + isoldeVersion },
        { name: "Setup guide", detail: "Repeat network, TV, audio, remote, and profile setup without resetting your data", status: "Open guide" },
        { name: "Media Library", detail: "Kodi frontend, scan for new movies, and startup", status: kodiService.frontend === "kodi" ? "Kodi app" : "Isolde" },
        { name: "Components", detail: "Local Kodi and GStreamer versions and capture readiness", status: "Inspect" },
        { name: "HDMI Inputs", detail: "Capture card, RS-232 switch, input names, and live preview", status: captureController.state === "receiving-frames" ? "Frames arriving" : "Set up" }
    ]

    function handleCommand(command) {
        if (activePage === 7) {
            if (command === inputManager.back) activePage = -1
            else componentPage.handleCommand(command)
            return
        }
        if (activePage === 8) {
            if (command === inputManager.back) { capturePage.handleCommand(command); activePage = -1 }
            else capturePage.handleCommand(command)
            return
        }
        if (activePage === 6) {
            if (command === inputManager.back) { if (!kodiPage.back()) activePage = -1 }
            else kodiPage.handleCommand(command)
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
                activePage = -1
                return
            }
            audioPage.handleCommand(command)
            return
        }
        if (activePage === 2) {
            if (command === inputManager.back) {
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
        else if (command === inputManager.confirm && (selectedIndex === 0 || selectedIndex === 1 || selectedIndex === 2))
            activePage = selectedIndex
        else if (command === inputManager.confirm && selectedIndex === 5) {
            if (setupState.restart()) sessionManager.goHome()
        }
        else if (command === inputManager.confirm && selectedIndex === 6) activePage = 6
        else if (command === inputManager.confirm && selectedIndex === 7) activePage = 7
        else if (command === inputManager.confirm && selectedIndex === 8) activePage = 8
    }

    DisplaySettingsView {
        id: displayPage
        anchors.fill: parent
        visible: root.activePage === 0
    }

    AudioSettingsView {
        id: audioPage
        anchors.fill: parent
        visible: root.activePage === 1
    }

    AmbientSettingsView {
        id: ambientPage
        anchors.fill: parent
        visible: root.activePage === 2
        onPreviewRequested: root.previewRequested()
    }

    signal previewRequested()
    KodiSettingsView { id: kodiPage; anchors.fill: parent; visible: root.activePage === 6; appearance: root.appearance }
    ComponentSettingsView {
        id: componentPage; anchors.fill: parent; visible: root.activePage === 7
        onCaptureSetupRequested: root.activePage = 8
    }
    CaptureSettingsView { id: capturePage; anchors.fill: parent; visible: root.activePage === 8 }

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
        visible: root.activePage < 0
    Column {
        id: contents
        width: parent.width
        spacing: 18

        Text {
            text: "SYSTEM"
            color: "#F8FAFC"
            font.pixelSize: 30
            font.weight: Font.DemiBold
            font.letterSpacing: 3
        }

        Text {
            width: parent.width
            text: "Core TV controls should live here so normal users never need the Ubuntu desktop."
            color: "#9EABC1"
            font.pixelSize: 18
            wrapMode: Text.WordWrap
        }

        Repeater {
            id: rowsRepeater
            model: root.rows
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: parent.width
                height: 84
                radius: 10
                color: root.selectedIndex === index ? "#2B416B" : "#111A2A"
                border.width: root.selectedIndex === index ? 3 : 1
                border.color: root.selectedIndex === index ? "#F8FAFC" : "#2B3853"

                Column {
                    anchors.left: parent.left
                    anchors.right: statusText.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 18
                    anchors.rightMargin: 16
                    spacing: 5
                    Text {
                        text: modelData.name
                        color: "#F8FAFC"
                        font.pixelSize: 21
                        font.weight: Font.DemiBold
                    }
                    Text {
                        width: parent.width
                        text: modelData.detail
                        color: "#9EABC1"
                        font.pixelSize: 14
                        elide: Text.ElideRight
                    }
                }

                Text {
                    id: statusText
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    width: 190
                    horizontalAlignment: Text.AlignRight
                    text: modelData.status
                    color: index === 1 ? "#C8D3E5" : index === 3 ? "#D5B083" : "#91A0BC"
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 88
            radius: 10
            color: "#101A2D"
            border.color: "#26344F"
            Text {
                anchors.fill: parent
                anchors.margins: 18
                text: "Volume uses the available audio backend. Application crash recovery is installed with appliance startup. OS power/update controls remain unavailable; existing settings and profiles are preserved."
                color: "#C8D3E5"
                font.pixelSize: 16
                wrapMode: Text.WordWrap
            }
        }
    }
    }
}
