import QtQuick
import QtMultimedia

Item {
    id: root
    property int selectedIndex: 0
    property int keyboardMode: 0
    readonly property real leftColumnWidth: width * 0.46
    readonly property var baudRates: [9600, 19200, 38400, 57600, 115200]
    readonly property var rows: [
        { name: "Capture card", value: deviceName(captureController.videoDevices, captureController.captureDeviceId, "Choose a card") },
        { name: "Video format", value: selectedFormatName() },
        { name: "HDMI audio", value: deviceName(captureController.audioDevices, captureController.audioDeviceId, "Not selected") },
        { name: "RS-232 switch port", value: captureController.serialPortName || "Choose a port" },
        { name: "Serial speed", value: captureController.baudRate + " baud" },
        { name: "Command format", value: protocolName() },
        { name: "Command template", value: captureController.commandTemplate || "Not set" },
        { name: "Input to test", value: inputName(captureController.selectedInput) },
        { name: "Rename this input", value: inputName(captureController.selectedInput) },
        { name: "Send test switch", value: "Switch to " + inputName(captureController.selectedInput) },
        { name: "Start video preview", value: captureController.previewActive ? "Preview running" : "Preview only; no switch command" },
        { name: "Stop preview", value: captureController.previewActive ? "Release capture card" : "Already stopped" },
        { name: "Refresh devices", value: "Rescan capture, audio, and serial devices" }
    ]

    function deviceName(devices, id, fallback) {
        for (let i = 0; i < devices.length; ++i)
            if (devices[i].id === id) return devices[i].name + (devices[i].formats ? " · " + devices[i].formats : "")
        return fallback
    }
    function inputName(number) {
        for (let i = 0; i < captureController.inputs.length; ++i)
            if (captureController.inputs[i].number === number) return captureController.inputs[i].name
        return "HDMI " + number
    }
    function selectedFormatName() {
        for (let i = 0; i < captureController.videoFormats.length; ++i)
            if (captureController.videoFormats[i].selected) return captureController.videoFormats[i].name
        return captureController.videoFormats.length ? "Card default" : "Choose capture card first"
    }
    function protocolName() {
        if (!captureController.commandTemplate) return "Not configured"
        if (captureController.commandTemplate === "{input}\\r") return "Number + CR (generic)"
        if (captureController.commandTemplate === "{input}\\r\\n") return "Number + CRLF (generic)"
        if (captureController.commandTemplate === "HDMI {input}\\r") return "HDMI number + CR (generic)"
        return "Custom command"
    }
    function currentId(devices, id) {
        for (let i = 0; i < devices.length; ++i) if (devices[i].id === id) return i
        return -1
    }
    function cycleDevice(devices, current, direction, setter, keyField) {
        const identity = keyField || "id"
        const emptyOption = { id: "", key: "", name: "Not selected" }
        const options = [emptyOption].concat(devices)
        let index = 0
        for (let i = 0; i < options.length; ++i) if (options[i][identity] === current) index = i
        index = (index + direction + options.length) % options.length
        setter(options[index][identity] || "")
    }
    function editKeyboard(mode) {
        keyboardMode = mode
        keyboard.selectedIndex = 0
        keyboard.value = mode === 1 ? captureController.commandTemplate : inputName(captureController.selectedInput)
    }
    function adjust(direction) {
        if (selectedIndex === 0) cycleDevice(captureController.videoDevices, captureController.captureDeviceId, direction, id => captureController.setCaptureDevice(id))
        else if (selectedIndex === 1) cycleDevice(captureController.videoFormats, captureController.videoFormatKey, direction, key => captureController.setVideoFormat(key), "key")
        else if (selectedIndex === 2) cycleDevice(captureController.audioDevices, captureController.audioDeviceId, direction, id => captureController.setAudioDevice(id))
        else if (selectedIndex === 3) cycleDevice(captureController.serialPorts, captureController.serialPortName, direction, id => captureController.setSerialPort(id))
        else if (selectedIndex === 4) {
            let index = Math.max(0, baudRates.indexOf(captureController.baudRate))
            index = (index + direction + baudRates.length) % baudRates.length
            captureController.setBaudRate(baudRates[index])
        } else if (selectedIndex === 5) {
            const templates = ["", "{input}\\r", "{input}\\r\\n", "HDMI {input}\\r"]
            let index = templates.indexOf(captureController.commandTemplate)
            if (index < 0) index = 0
            index = (index + direction + templates.length) % templates.length
            captureController.setCommandTemplate(templates[index])
        } else if (selectedIndex === 6) {
            captureController.selectInput((captureController.selectedInput - 1 + direction + 8) % 8 + 1)
        }
    }
    function activateSelected() {
        if (selectedIndex <= 5) adjust(1)
        else if (selectedIndex === 6) editKeyboard(1)
        else if (selectedIndex === 7) captureController.selectInput(captureController.selectedInput % 8 + 1)
        else if (selectedIndex === 8) editKeyboard(2)
        else if (selectedIndex === 9) captureController.switchInput(captureController.selectedInput)
        else if (selectedIndex === 10) captureController.startPreview()
        else if (selectedIndex === 11) captureController.stopPreview()
        else captureController.refreshDevices()
    }
    function handleCommand(command) {
        if (keyboardMode) { keyboard.handleCommand(command); return }
        if (command === inputManager.back) { captureController.stopPreview(); return }
        if (command === inputManager.navigateUp) selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown) selectedIndex = Math.min(rows.length - 1, selectedIndex + 1)
        else if (command === inputManager.navigateLeft) adjust(-1)
        else if (command === inputManager.navigateRight) adjust(1)
        else if (command === inputManager.confirm) activateSelected()
    }

    onVisibleChanged: {
        if (visible) captureController.setVideoOutput(previewOutput)
        else captureController.stopPreview()
    }

    Row {
        anchors.fill: parent
        spacing: 24
        Column {
            width: root.leftColumnWidth
            height: parent.height
            spacing: 10
            Text { text: "HDMI INPUTS"; color: "#F8FAFC"; font.pixelSize: 30; font.weight: Font.DemiBold }
            Text {
                width: parent.width; text: "Choose the capture card and audio input. Then select the RS-232 port and a command format from the switch manual. No command is sent until you press Send test switch.";
                color: "#9EABC1"; font.pixelSize: 16; wrapMode: Text.WordWrap
            }
            ListView {
                id: optionList
                width: parent.width; height: parent.height - 125; clip: true; spacing: 5
                model: root.rows; currentIndex: root.selectedIndex
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: optionList.width; height: 49; radius: 7
                    color: root.selectedIndex === index ? "#2B416B" : "#111A2A"
                    border.width: root.selectedIndex === index ? 2 : 1
                    border.color: root.selectedIndex === index ? "#F8FAFC" : "#2B3853"
                    Text { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; width: parent.width * 0.42; text: modelData.name; color: "#F8FAFC"; font.pixelSize: 15; wrapMode: Text.WordWrap }
                    Text { anchors.left: parent.left; anchors.leftMargin: parent.width * 0.43; anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.value; color: "#C8D3E5"; font.pixelSize: 14; elide: Text.ElideRight }
                    MouseArea { anchors.fill: parent; onClicked: { root.selectedIndex = index; root.activateSelected() } }
                }
            }
            Text { width: parent.width; text: "‹ › Change · OK select/test · Back stop preview"; color: "#91A0BC"; font.pixelSize: 15 }
        }

        Column {
            width: root.width - root.leftColumnWidth - 24
            height: parent.height
            spacing: 10
            Rectangle {
                width: parent.width; height: parent.height * 0.72; radius: 10; color: "#05080E"; clip: true
                VideoOutput { id: previewOutput; anchors.fill: parent; fillMode: VideoOutput.PreserveAspectFit; visible: captureController.previewActive }
                Text {
                    anchors.centerIn: parent; width: parent.width - 32; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                    visible: !captureController.previewActive
                    text: "Preview is off\n\nChoose Start video preview to test the capture card without sending an RS-232 command."
                    color: "#9EABC1"; font.pixelSize: 19
                }
            }
            Text { width: parent.width; text: captureController.status + "\n" + captureController.switchStatus; color: captureController.state === "error" || captureController.state === "no-signal" ? "#F2B8B5" : "#C8D3E5"; font.pixelSize: 17; wrapMode: Text.WordWrap }
            Text {
                width: parent.width; text: "Generic command formats are examples, not guaranteed switch protocols. Use the exact command from your switch manual. A sent command is not confirmation that routing changed; visually verify the source.";
                color: "#E2C38C"; font.pixelSize: 15; wrapMode: Text.WordWrap
            }
        }
    }

    OnScreenKeyboard {
        id: keyboard
        anchors.fill: parent; anchors.margins: 34; z: 5; visible: root.keyboardMode !== 0
        onCanceled: root.keyboardMode = 0
        onAccepted: text => {
            const ok = root.keyboardMode === 1 ? captureController.setCommandTemplate(text)
                                               : captureController.setInputLabel(captureController.selectedInput, text)
            if (ok) root.keyboardMode = 0
        }
    }
}