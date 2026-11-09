import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    Rectangle {anchors.fill:parent;z:-1;color:shell.surface}
    property var appearance: ({})
    property int selectedIndex: 0
    property string outputDraftId: ""
    property bool outputDraftActive: false
    property bool outputApplyPending: false
    onVisibleChanged: if (!visible) audioSettings.stopTestSound()
    readonly property var candidateOutput: findOutput(outputDraftId)
    readonly property string startupLabel: audioSettings.startupPolicy === "fixed" ? "Fixed at " + audioSettings.startupVolume + "%" : audioSettings.startupPolicy === "muted" ? "Start muted" : "Remember last"
    readonly property string volumeModeLabel: audioSettings.volumeMode === "cec" ? "TV / HDMI-CEC" : audioSettings.volumeMode === "hybrid" ? "Hybrid" : "Isolde output"
    readonly property var rows: [
        { name: "Volume Owner", value: "Isolde output", help: "TV / HDMI-CEC and Hybrid volume are not available yet." },
        { name: "TV Reference Level", value: audioSettings.tvReferenceVolume + "%", help: "One-time calibration note: set the TV near this level, then use Isolde volume normally." },
        { name: "Volume", value: !audioSettings.mixerAvailable ? "Unconfirmed" : audioSettings.muted ? "Muted" : audioSettings.volume + "%", help: "Confirmed system output level; changes are checked with the audio service." },
        { name: "Mute", value: !audioSettings.mixerAvailable ? "Unconfirmed" : audioSettings.muted ? "On" : "Off", help: "Silence Isolde quickly without leaving the TV interface." },
        { name: "Startup Volume", value: startupLabel, help: "Choose whether Isolde remembers volume, starts at a fixed level, or starts muted." },
        { name: "Fixed Startup Level", value: audioSettings.startupVolume + "%", help: "Used when Startup Volume is set to Fixed." },
        { name: "Remote Step", value: audioSettings.volumeStep + "%", help: "How much each remote volume press changes volume." },
        { name: "Night Mode", value: "Unavailable", help: "Audio compression is not implemented; this control does not change sound." },
        { name: "Lip Sync Delay", value: "Unavailable", help: "Playback delay processing is not implemented yet." },
        { name: "Reset Audio", value: "Defaults", help: "Restore Isolde's default audio policy." },
        { name: "Sound Output", value: candidateOutput ? candidateOutput.name + (candidateOutput.active ? " (Active)" : outputDraftActive ? " (Preview)" : "") : "Unavailable", help: audioSettings.outputStatus },
        { name: "Refresh Outputs", value: "Scan", help: "Look for connected TV, speakers, USB audio, or paired Bluetooth outputs." },
        { name: "Test Sound", value: audioSettings.testPlaying ? "Stop" : "Play", help: audioSettings.testStatus }
    ]

    function findOutput(id) {
        for (let i = 0; i < audioSettings.outputs.length; ++i)
            if (audioSettings.outputs[i].id === id) return audioSettings.outputs[i]
        return null
    }
    function beginOutputDraft() {
        if (outputDraftActive || !audioSettings.outputs.length) return
        let active = null
        for (let i = 0; i < audioSettings.outputs.length; ++i)
            if (audioSettings.outputs[i].active) active = audioSettings.outputs[i]
        if (!active && audioSettings.outputs.length) active = audioSettings.outputs[0]
        outputDraftId = active ? active.id : ""
        outputDraftActive = true
    }
    function adjustOutputDraft(direction) {
        if (!outputDraftActive) beginOutputDraft()
        const index = audioSettings.outputs.findIndex(item => item.id === outputDraftId)
        const next = index < 0
            ? (direction > 0 ? 0 : audioSettings.outputs.length - 1)
            : (index + direction + audioSettings.outputs.length) % audioSettings.outputs.length
        if (audioSettings.outputs.length) outputDraftId = audioSettings.outputs[next].id
    }
    function cancelOutputDraft() {
        if (outputApplyPending) return
        outputDraftActive = false
        outputDraftId = ""
        for (let i = 0; i < audioSettings.outputs.length; ++i)
            if (audioSettings.outputs[i].active) outputDraftId = audioSettings.outputs[i].id
    }
    function reconcileOutputDraft() {
        if (!outputApplyPending || audioSettings.outputStatus === "Switching output…") return
        outputApplyPending = false
        if (candidateOutput && candidateOutput.active) outputDraftActive = false
    }
    function handleCommand(command) {
        if (command === inputManager.back) {
            if (outputDraftActive && !outputApplyPending) cancelOutputDraft()
        } else if (command === inputManager.navigateUp)
            selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown)
            selectedIndex = Math.min(rows.length - 1, selectedIndex + 1)
        else if (command === inputManager.navigateLeft) {
            if (selectedIndex === 1) audioSettings.adjustTvReferenceVolume(-5)
            else if (selectedIndex === 2) audioSettings.adjustVolume(-audioSettings.volumeStep)
            else if (selectedIndex === 5) audioSettings.adjustStartupVolume(-5)
            else if (selectedIndex === 6) audioSettings.adjustVolumeStep(-1)
            else if (selectedIndex === 10) adjustOutputDraft(-1)
        } else if (command === inputManager.navigateRight) {
            if (selectedIndex === 1) audioSettings.adjustTvReferenceVolume(5)
            else if (selectedIndex === 2) audioSettings.adjustVolume(audioSettings.volumeStep)
            else if (selectedIndex === 5) audioSettings.adjustStartupVolume(5)
            else if (selectedIndex === 6) audioSettings.adjustVolumeStep(1)
            else if (selectedIndex === 10) adjustOutputDraft(1)
        } else if (command === inputManager.confirm) {
            if (selectedIndex === 3) audioSettings.toggleMute()
            else if (selectedIndex === 4) audioSettings.nextStartupPolicy()
            else if (selectedIndex === 9) audioSettings.reset()
            else if (selectedIndex === 10 && candidateOutput) {
                if (!outputDraftActive) beginOutputDraft()
                else outputApplyPending = actionDispatcher.dispatchEditorAction({
                    type: "audio-output-select", outputId: candidateOutput.id
                })
            }
            else if (selectedIndex === 11) audioSettings.refreshBackend()
            else if (selectedIndex === 12) {
                if (audioSettings.testPlaying) audioSettings.stopTestSound()
                else audioSettings.playTestSound()
            }
        }
    }

    onSelectedIndexChanged: {
        if (selectedIndex === 10 && !outputDraftActive) beginOutputDraft()
        const row = rowsRepeater.itemAt(selectedIndex)
        if (row) scroll.contentY = Math.max(0, Math.min(scroll.contentHeight - scroll.height, row.y - 20))
    }
    Connections {
        target: audioSettings
        function onChanged() { root.reconcileOutputDraft() }
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

        Text { font.family: shell.fontFamily; text: "AUDIO"; color: shell.text; font.pixelSize: 30 * shell.textScale; font.weight: Font.DemiBold; font.letterSpacing: 3 }
        Text { font.family: shell.fontFamily;
            width: parent.width
                text: "Set the TV once at a comfortable middle level, then let Isolde handle everyday volume and mute."
            color: shell.muted
            font.pixelSize: 18 * shell.textScale
            wrapMode: Text.WordWrap
        }

        Rectangle {
            width: parent.width
            height: 140
            radius: 14
            color: shell.background
            border.color: shell.outline

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                objectName: "audioVolumeBar"
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 24
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                height: 18
                radius: 9
                color: shell.input
                Rectangle {
                    width: parent.width * audioSettings.volume / 100
                    height: parent.height
                    radius: parent.radius
                    objectName: "audioVolumeFill"
                    color: shell.inputText
                }
            }

            Text { font.family: shell.fontFamily;
                objectName: "audioVolumeValue"
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 18
                text: !audioSettings.mixerAvailable ? "UNCONFIRMED" : audioSettings.muted ? "MUTED" : audioSettings.volume + "%"
                color: shell.backgroundText
                font.pixelSize: 32 * shell.textScale
                font.weight: Font.DemiBold
                font.letterSpacing: 3
            }
        }

        Repeater {
            id: rowsRepeater
            model: root.rows
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: parent.width
                height: index === 10 || index === 12 ? 120 : 68
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
                    Text { font.family: shell.fontFamily; text: modelData.name; color: (root.selectedIndex === index) ? shell.selectedText : shell.text; font.pixelSize: 19 * shell.textScale; font.weight: Font.DemiBold }
                    Text { font.family: shell.fontFamily; width: parent.width; text: modelData.help; color: (root.selectedIndex === index) ? shell.selectedText : shell.muted; font.pixelSize: 13 * shell.textScale; wrapMode: index === 10 || index === 12 ? Text.WordWrap : Text.NoWrap; elide: Text.ElideRight }
                }

                Text { font.family: shell.fontFamily;
                    id: valueText
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    width: index === 10 ? 260 : 190
                    horizontalAlignment: Text.AlignRight
                    text: modelData.value
                    color: (root.selectedIndex === index) ? shell.selectedText : shell.text
                    font.pixelSize: 17 * shell.textScale
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                    maximumLineCount: 4
                    elide: Text.ElideRight
                }
            }
        }

        Text { font.family: shell.fontFamily;
            width: parent.width
            text: "Backend: " + audioSettings.backendStatus + "    ← → adjust/choose    Enter apply/toggle/reset    Esc back"
            color: shell.unavailable
            font.pixelSize: 15 * shell.textScale
            wrapMode: Text.WordWrap
        }
    }
    }
}
