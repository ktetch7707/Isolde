import QtQuick
import QtMultimedia

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    Rectangle {anchors.fill:parent;z:-1;color:shell.surface}
    property var appearance: ({})
    property int selectedIndex: 0
    property int keyboardMode: 0
    property var draft: ({})
    property bool draftDirty: false
    property bool artworkEditing: false
    property var artworkDraft: ({})
    property var artworkChoices: []
    property int artworkRow: 0
    property string artworkMessage: ""
    property int deviceListRevision: 0
    readonly property real leftColumnWidth: width * 0.46
    readonly property var baudRates: [9600, 19200, 38400, 57600, 115200]
    readonly property var formatOptions: {
        return deviceListRevision >= 0
            ? captureController.videoFormatsFor(draft.captureDeviceId || "") : []
    }
    readonly property var rows: [
        { name: "Capture card", value: deviceName(captureController.videoDevices, draft.captureDeviceId, "Choose a card") },
        { name: "Video format", value: selectedFormatName() },
        { name: "HDMI audio", value: deviceName(captureController.audioDevices, draft.audioDeviceId, "Not selected") },
        { name: "RS-232 switch port", value: draft.serialPortName || "Choose a port" },
        { name: "Serial speed", value: draft.baudRate + " baud" },
        { name: "Command format", value: protocolName() },
        { name: "Command template", value: draft.commandTemplate || "Not set" },
        { name: "Input to test", value: inputName(draft.selectedInput) },
        { name: "Rename this input", value: draft.inputLabels ? draft.inputLabels[String(draft.selectedInput)] : "" },
        { name: "Send test switch", value: "Switch to " + inputName(draft.selectedInput) },
        { name: "Preview staged devices", value: captureController.previewActive ? "Update preview to staged devices" : "Preview only; no switch command" },
        { name: "Stop preview", value: captureController.previewActive ? "Release capture card" : "Already stopped" },
        { name: "Apply capture changes", value: draftDirty ? "Save all staged settings" : "No pending changes" },
        { name: "Cancel capture changes", value: draftDirty ? "Discard staged settings" : "Restore saved settings" },
        { name: "Refresh devices", value: "Rescan capture, audio, and serial devices" },
        { name: "Session transition", value: draft.sessionTransition || "expand" },
        { name: "Transition duration", value: (draft.transitionDuration || 250) + " ms" },
        { name: "Input fallback artwork", value: "Choose profile images for each HDMI input" }
    ]

    function setDraftValue(key, value) {
        const next = Object.assign({}, draft)
        next[key] = value
        draft = next
        draftDirty = true
    }
    function openArtworkEditor() {
        artworkDraft = Object.assign({}, homeLayout.composition.captureInputArtwork || {})
        artworkChoices = homeLayout.captureArtworkChoices()
        artworkRow = 0
        artworkMessage = "Left / Right choose a managed image. Apply saves all eight mappings; Back discards."
        artworkEditing = true
    }
    function artworkName(input) {
        const id = artworkDraft[String(input)] || ""
        if (!id) return "Neutral / shortcut fallback"
        const match = artworkChoices.find(item => item.id === id)
        return match ? match.name : "Missing image · choose another"
    }
    function adjustArtwork(direction) {
        if (artworkRow >= 8) return
        const key = String(artworkRow + 1), values = [""].concat(artworkChoices.map(item => item.id))
        let index = values.indexOf(artworkDraft[key] || "")
        if (index < 0) index = 0
        const next = Object.assign({}, artworkDraft), id = values[(index + direction + values.length) % values.length]
        if (id) next[key] = id
        else delete next[key]
        artworkDraft = next
    }
    function handleArtworkCommand(command) {
        if (command === inputManager.back) { artworkEditing = false; return }
        if (command === inputManager.navigateUp) artworkRow = Math.max(0, artworkRow - 1)
        else if (command === inputManager.navigateDown) artworkRow = Math.min(9, artworkRow + 1)
        else if (command === inputManager.navigateLeft) adjustArtwork(-1)
        else if (command === inputManager.navigateRight) adjustArtwork(1)
        else if (command === inputManager.confirm) {
            if (artworkRow < 8) adjustArtwork(1)
            else if (artworkRow === 9) artworkEditing = false
            else if (actionDispatcher.dispatchEditorAction({type: "capture-artwork-apply", artwork: artworkDraft}))
                artworkEditing = false
            else artworkMessage = homeLayout.errorString || "Could not apply artwork. Refresh choices and try again."
        }
    }
    function resetDraft() {
        const labels = ({})
        for (let i = 0; i < captureController.inputs.length; ++i)
            labels[String(captureController.inputs[i].number)] = captureController.inputs[i].name
        draft = {
            captureDeviceId: captureController.captureDeviceId,
            videoFormatKey: captureController.videoFormatKey,
            audioDeviceId: captureController.audioDeviceId,
            serialPortName: captureController.serialPortName,
            baudRate: captureController.baudRate,
            commandTemplate: captureController.commandTemplate,
            selectedInput: captureController.selectedInput,
            inputLabels: labels,
            sessionTransition: captureController.sessionTransition,
            transitionDuration: captureController.transitionDuration
        }
        draftDirty = false
    }
    function configurationDraft() {
        return {
            captureDeviceId: draft.captureDeviceId,
            videoFormatKey: draft.videoFormatKey,
            audioDeviceId: draft.audioDeviceId,
            serialPortName: draft.serialPortName,
            baudRate: draft.baudRate,
            commandTemplate: draft.commandTemplate,
            selectedInput: draft.selectedInput,
            inputLabels: draft.inputLabels,
            sessionTransition: draft.sessionTransition,
            transitionDuration: draft.transitionDuration
        }
    }
    function applyDraft() {
        if (!draftDirty) return true
        const ok = actionDispatcher.dispatchEditorAction({
            type: "capture-apply",
            configuration: configurationDraft()
        })
        if (ok) resetDraft()
        return ok
    }
    function cancelDraft() {
        resetDraft()
        actionDispatcher.dispatchEditorAction({type: "capture-preview-stop"})
    }
    function keepSelectedRowVisible() {
        optionList.positionViewAtIndex(selectedIndex, ListView.Contain)
    }
    function deviceName(devices, id, fallback) {
        for (let i = 0; i < devices.length; ++i)
            if (devices[i].id === id) return devices[i].name + (devices[i].formats ? " · " + devices[i].formats : "")
        return fallback
    }
    function inputName(number) {
        if (draft.inputLabels && draft.inputLabels[String(number)])
            return draft.inputLabels[String(number)]
        return "HDMI " + number
    }
    function selectedFormatName() {
        for (let i = 0; i < formatOptions.length; ++i)
            if (formatOptions[i].key === draft.videoFormatKey) return formatOptions[i].name
        return formatOptions.length ? "Card default" : "Choose capture card first"
    }
    function protocolName() {
        if (!draft.commandTemplate) return "Not configured"
        if (draft.commandTemplate === "{input}\\r") return "Number + CR (generic)"
        if (draft.commandTemplate === "{input}\\r\\n") return "Number + CRLF (generic)"
        if (draft.commandTemplate === "HDMI {input}\\r") return "HDMI number + CR (generic)"
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
        let index = -1
        for (let i = 0; i < options.length; ++i) if (options[i][identity] === current) index = i
        if (index < 0) index = direction > 0 ? options.length - 1 : 0
        index = (index + direction + options.length) % options.length
        setter(options[index][identity] || "")
    }
    function editKeyboard(mode) {
        keyboardMode = mode
        keyboard.selectedIndex = 0
        keyboard.value = mode === 1 ? draft.commandTemplate : inputName(draft.selectedInput)
    }
    function adjust(direction) {
        if (selectedIndex === 0) cycleDevice(captureController.videoDevices, draft.captureDeviceId, direction, id => {
            setDraftValue("captureDeviceId", id)
            const options = captureController.videoFormatsFor(id)
            if (draft.videoFormatKey && !options.some(item => item.key === draft.videoFormatKey))
                setDraftValue("videoFormatKey", "")
        })
        else if (selectedIndex === 1) cycleDevice(formatOptions, draft.videoFormatKey, direction, key => setDraftValue("videoFormatKey", key), "key")
        else if (selectedIndex === 2) cycleDevice(captureController.audioDevices, draft.audioDeviceId, direction, id => setDraftValue("audioDeviceId", id))
        else if (selectedIndex === 3) cycleDevice(captureController.serialPorts, draft.serialPortName, direction, id => setDraftValue("serialPortName", id))
        else if (selectedIndex === 4) {
            let index = Math.max(0, baudRates.indexOf(draft.baudRate))
            index = (index + direction + baudRates.length) % baudRates.length
            setDraftValue("baudRate", baudRates[index])
        } else if (selectedIndex === 5) {
            const templates = ["", "{input}\\r", "{input}\\r\\n", "HDMI {input}\\r"]
            let index = templates.indexOf(draft.commandTemplate)
            if (index < 0) index = 0
            index = (index + direction + templates.length) % templates.length
            setDraftValue("commandTemplate", templates[index])
        } else if (selectedIndex === 7) {
            setDraftValue("selectedInput", (draft.selectedInput - 1 + direction + 8) % 8 + 1)
        } else if (selectedIndex === 15 || selectedIndex === 16) {
            const key = selectedIndex === 15 ? "sessionTransition" : "transitionDuration"
            const values = selectedIndex === 15 ? ["instant", "fade", "slide", "expand"] : [200, 250, 300, 350]
            const index = Math.max(0, values.indexOf(draft[key]))
            setDraftValue(key, values[(index + direction + values.length) % values.length])
        }
    }
    function activateSelected() {
        if (selectedIndex <= 5) adjust(1)
        else if (selectedIndex === 6) editKeyboard(1)
        else if (selectedIndex === 7) setDraftValue("selectedInput", draft.selectedInput % 8 + 1)
        else if (selectedIndex === 8) editKeyboard(2)
        else if (selectedIndex === 9) {
            if (applyDraft()) actionDispatcher.dispatchEditorAction({
                type: "capture-switch-input", input: draft.selectedInput
            })
        }
        else if (selectedIndex === 10) actionDispatcher.dispatchEditorAction({
            type: "capture-preview-draft", captureDeviceId: draft.captureDeviceId,
            videoFormatKey: draft.videoFormatKey, audioDeviceId: draft.audioDeviceId
        })
        else if (selectedIndex === 11) actionDispatcher.dispatchEditorAction({type: "capture-preview-stop"})
        else if (selectedIndex === 12) applyDraft()
        else if (selectedIndex === 13) cancelDraft()
        else if (selectedIndex === 14) actionDispatcher.dispatchEditorAction({type: "capture-refresh"})
        else if (selectedIndex === 17) openArtworkEditor()
        else adjust(1)
    }
    function handleCommand(command) {
        if (artworkEditing) { handleArtworkCommand(command); return }
        if (keyboardMode) { keyboard.handleCommand(command); return }
        if (command === inputManager.back) {
            actionDispatcher.dispatchEditorAction({type: "capture-switch-cancel"})
            if (draftDirty) cancelDraft()
            else { artworkEditing = false; actionDispatcher.dispatchEditorAction({type: "capture-preview-stop"}) }
            return
        }
        if (command === inputManager.navigateUp) selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown) selectedIndex = Math.min(rows.length - 1, selectedIndex + 1)
        else if (command === inputManager.navigateLeft) adjust(-1)
        else if (command === inputManager.navigateRight) adjust(1)
        else if (command === inputManager.confirm) activateSelected()
    }

    onVisibleChanged: {
        if (visible) {
            captureController.setVideoOutput(previewOutput)
            resetDraft()
        }
        else actionDispatcher.dispatchEditorAction({type: "capture-preview-stop"})
    }
    onSelectedIndexChanged: keepSelectedRowVisible()
    Connections {
        target: captureController
        function onChanged() { root.deviceListRevision += 1 }
    }

    Rectangle {
        anchors.fill: parent; z: 50; visible: root.artworkEditing; color: shell.background
        Column {
            anchors.fill: parent; anchors.margins: 24; spacing: 10
            Text { text: "HDMI FALLBACK ARTWORK"; color: shell.text; font.pixelSize: 25 * shell.textScale }
            Text { width: parent.width; text: root.artworkMessage; color: shell.muted; font.pixelSize: 15 * shell.textScale; wrapMode: Text.WordWrap }
            Row {
                width: parent.width; height: parent.height - 100; spacing: 20
                ListView {
                    id: artworkList
                    width: parent.width * 0.56; height: parent.height; clip: true
                    model: 10; currentIndex: root.artworkRow
                    onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                    delegate: Rectangle {
                        required property int index
                        width: artworkList.width; height: 45; radius: 5
                        color: root.artworkRow === index ? shell.selected : shell.surface
                        border.width: root.artworkRow === index ? 2 : 1
                        border.color: root.artworkRow === index ? shell.selectedFocus : shell.outline
                        Text {
                            anchors.fill: parent; anchors.margins: 8; verticalAlignment: Text.AlignVCenter
                            text: index < 8 ? root.inputName(index + 1) + " · " + root.artworkName(index + 1)
                                : index === 8 ? "Apply artwork mappings" : "Cancel artwork changes"
                            textFormat: Text.PlainText; elide: Text.ElideRight
                            color: root.artworkRow === index ? shell.selectedText : shell.text; font.pixelSize: 15 * shell.textScale
                        }
                        MouseArea { anchors.fill: parent; onClicked: { root.artworkRow = index; root.handleArtworkCommand(inputManager.confirm) } }
                    }
                }
                Column {
                    width: parent.width * 0.38; spacing: 10
                    Image {
                        width: parent.width; height: Math.max(100, parent.parent.height * 0.65)
                        fillMode: Image.PreserveAspectFit; asynchronous: true
                        source: root.artworkRow < 8 && root.artworkDraft[String(root.artworkRow + 1)]
                            ? homeLayout.resolveArtwork(root.artworkDraft[String(root.artworkRow + 1)]) : ""
                    }
                    Text { width: parent.width; wrapMode: Text.WordWrap; text: "Choose images already imported to this profile in Home Studio. Removing a mapping restores shortcut or neutral fallback."; color: shell.muted; font.pixelSize: 14 * shell.textScale }
                }
            }
        }
    }

    Row {
        anchors.fill: parent
        spacing: 24 * shell.gapScale
        Column {
            width: root.leftColumnWidth
            height: parent.height
            spacing: 10 * shell.gapScale
            Text { font.family: shell.fontFamily; text: "HDMI INPUTS"; color: shell.text; font.pixelSize: 30 * shell.textScale; font.weight: Font.DemiBold }
            Text { font.family: shell.fontFamily;
                width: parent.width; text: "Choose the capture card and audio input. Then select the RS-232 port and a command format from the switch manual. No command is sent until you press Send test switch.";
                color: shell.muted; font.pixelSize: 16 * shell.textScale; wrapMode: Text.WordWrap
            }
            ListView {
                id: optionList
                width: parent.width; height: parent.height - 125; clip: true; spacing: 5 * shell.gapScale
                model: root.rows; currentIndex: root.selectedIndex
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: optionList.width; height: 49; radius: 7
                    color: root.selectedIndex === index ? shell.selected : shell.surface
                    border.width: root.selectedIndex === index ? 2 : 1
                    border.color: root.selectedIndex === index ? shell.selectedFocus : shell.outline
                    Text { font.family: shell.fontFamily; anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; width: parent.width * 0.42; text: modelData.name; color: (root.selectedIndex === index) ? shell.selectedText : shell.text; font.pixelSize: 15 * shell.textScale; wrapMode: Text.WordWrap }
                    Text { font.family: shell.fontFamily; anchors.left: parent.left; anchors.leftMargin: parent.width * 0.43; anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.value; color: (root.selectedIndex === index) ? shell.selectedText : shell.text; font.pixelSize: 14 * shell.textScale; elide: Text.ElideRight }
                    MouseArea { anchors.fill: parent; onClicked: { root.selectedIndex = index; root.activateSelected() } }
                }
            }
            Text { font.family: shell.fontFamily; width: parent.width; text: "‹ › Preview choices · OK edit/apply · Back discard pending changes"; color: shell.muted; font.pixelSize: 15 * shell.textScale }
        }

        Column {
            width: root.width - root.leftColumnWidth - 24
            height: parent.height
            spacing: 10 * shell.gapScale
            Rectangle {
                width: parent.width; height: parent.height * 0.72; radius: shell.radius; color: shell.input; clip: true
                VideoOutput { id: previewOutput; anchors.fill: parent; fillMode: VideoOutput.PreserveAspectFit; visible: captureController.previewActive }
                Rectangle {
                    visible: root.draftDirty && captureController.previewActive
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                    height: 48; color: "#D2080C14"
                    Text { anchors.centerIn: parent; width: parent.width - 20; horizontalAlignment: Text.AlignHCenter
                        text: "Preview shows the last requested devices. Apply saves the draft; choose Preview again after changes."
                        color: "#FFFFFF"; font.pixelSize: 15 * shell.textScale; wrapMode: Text.WordWrap
                    }
                }
                Text { font.family: shell.fontFamily;
                    anchors.centerIn: parent; width: parent.width - 32; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                    visible: !captureController.previewActive
                    text: "Preview is off\n\nChoose Preview staged devices to test the capture card without sending an RS-232 command."
                    color: shell.muted; font.pixelSize: 19 * shell.textScale
                }
            }
            Text { font.family: shell.fontFamily; width: parent.width; text: captureController.status + (captureController.audioStatus ? "\n" + captureController.audioStatus : "") + "\n" + captureController.switchStatus; color: captureController.state === "error" || captureController.state === "no-signal" || captureController.state === "device-missing" || captureController.state === "format-missing" || captureController.state === "serial-missing" || captureController.audioState === "error" || captureController.audioState === "missing" || captureController.switchState === "error" ? shell.error : shell.text; font.pixelSize: 17 * shell.textScale; wrapMode: Text.WordWrap }
            Text { font.family: shell.fontFamily;
                width: parent.width
                text: "Saved adapters: " + (captureController.capabilities.captureConnected ? "capture device detected" : "capture device unavailable")
                    + " · " + (captureController.capabilities.serialConnected ? "serial port detected" : "serial port unavailable")
                    + " · switch " + (captureController.capabilities.switchConfigured ? "configured to test" : "not configured")
                    + " · capture audio " + (!captureController.audioDeviceId ? "not selected"
                        : captureController.capabilities.audioInputSelected ? "device detected" : "unavailable")
                    + " · audio monitoring " + (!captureController.audioDeviceId ? "not selected"
                        : !captureController.capabilities.audioInputSelected ? "input unavailable"
                        : !captureController.capabilities.audioMonitoringSupported ? "video only on this Qt build"
                        : captureController.capabilities.audioMonitoringConfigured ? "configured" : "ready when preview starts")
                color: shell.muted; font.pixelSize: 14 * shell.textScale; wrapMode: Text.WordWrap
            }
            Text { font.family: shell.fontFamily;
                width: parent.width
                text: "Last switch timings: serial write " + (captureController.serialWriteLatencyMs >= 0 ? captureController.serialWriteLatencyMs + " ms" : "not completed")
                    + " · first frame callback " + (captureController.firstFrameCallbackLatencyMs >= 0 ? captureController.firstFrameCallbackLatencyMs + " ms" : "not observed")
                    + " · last preview " + captureController.previewFrameRate.toFixed(1) + " fps, max gap "
                    + (captureController.maxFrameGapMs >= 0 ? captureController.maxFrameGapMs + " ms" : "not measured")
                color: shell.muted; font.pixelSize: 14 * shell.textScale; wrapMode: Text.WordWrap
            }
            Text { font.family: shell.fontFamily;
                width: parent.width; text: "Generic command formats are examples, not guaranteed switch protocols. Use the exact command from your switch manual. A sent command is not confirmation that routing changed; visually verify the source.";
                color: shell.warning; font.pixelSize: 15 * shell.textScale; wrapMode: Text.WordWrap
            }
        }
    }

    OnScreenKeyboard { appearance: root.appearance;
        id: keyboard
        anchors.fill: parent; anchors.margins: 34; z: 5; visible: root.keyboardMode !== 0
        onCanceled: root.keyboardMode = 0
        onAccepted: text => {
            if (root.keyboardMode === 1) root.setDraftValue("commandTemplate", text)
            else {
                const labels = Object.assign({}, root.draft.inputLabels)
                labels[String(root.draft.selectedInput)] = text
                root.setDraftValue("inputLabels", labels)
            }
            root.keyboardMode = 0
        }
    }
}