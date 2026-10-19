import QtQuick

Item {
    id: root
    property int selectedIndex: 0
    onVisibleChanged: if (!visible) audioSettings.stopTestSound()
    property int outputIndex: 0
    readonly property var candidateOutput: audioSettings.outputs.length > 0 ? audioSettings.outputs[Math.min(outputIndex, audioSettings.outputs.length - 1)] : null
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
        { name: "Sound Output", value: candidateOutput ? candidateOutput.name + (candidateOutput.active ? " (Active)" : "") : "Unavailable", help: audioSettings.outputStatus },
        { name: "Refresh Outputs", value: "Scan", help: "Look for connected TV, speakers, USB audio, or paired Bluetooth outputs." },
        { name: "Test Sound", value: audioSettings.testPlaying ? "Stop" : "Play", help: audioSettings.testStatus }
    ]

    function handleCommand(command) {
        if (command === inputManager.navigateUp)
            selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown)
            selectedIndex = Math.min(rows.length - 1, selectedIndex + 1)
        else if (command === inputManager.navigateLeft) {
            if (selectedIndex === 1) audioSettings.adjustTvReferenceVolume(-5)
            else if (selectedIndex === 2) audioSettings.adjustVolume(-audioSettings.volumeStep)
            else if (selectedIndex === 5) audioSettings.adjustStartupVolume(-5)
            else if (selectedIndex === 6) audioSettings.adjustVolumeStep(-1)
            else if (selectedIndex === 10) outputIndex = Math.max(0, outputIndex - 1)
        } else if (command === inputManager.navigateRight) {
            if (selectedIndex === 1) audioSettings.adjustTvReferenceVolume(5)
            else if (selectedIndex === 2) audioSettings.adjustVolume(audioSettings.volumeStep)
            else if (selectedIndex === 5) audioSettings.adjustStartupVolume(5)
            else if (selectedIndex === 6) audioSettings.adjustVolumeStep(1)
            else if (selectedIndex === 10) outputIndex = Math.min(Math.max(0, audioSettings.outputs.length - 1), outputIndex + 1)
        } else if (command === inputManager.confirm) {
            if (selectedIndex === 3) audioSettings.toggleMute()
            else if (selectedIndex === 4) audioSettings.nextStartupPolicy()
            else if (selectedIndex === 9) audioSettings.reset()
            else if (selectedIndex === 10 && candidateOutput) audioSettings.selectOutput(candidateOutput.id)
            else if (selectedIndex === 11) audioSettings.refreshBackend()
            else if (selectedIndex === 12) {
                if (audioSettings.testPlaying) audioSettings.stopTestSound()
                else audioSettings.playTestSound()
            }
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
        spacing: 18

        Text { text: "AUDIO"; color: "#F8FAFC"; font.pixelSize: 30; font.weight: Font.DemiBold; font.letterSpacing: 3 }
        Text {
            width: parent.width
                text: "Set the TV once at a comfortable middle level, then let Isolde handle everyday volume and mute."
            color: "#9EABC1"
            font.pixelSize: 18
            wrapMode: Text.WordWrap
        }

        Rectangle {
            width: parent.width
            height: 140
            radius: 14
            color: "#080C18"
            border.color: "#26344F"

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                height: 18
                radius: 9
                color: "#1A2740"
                Rectangle {
                    width: parent.width * audioSettings.volume / 100
                    height: parent.height
                    radius: parent.radius
                    color: audioSettings.muted ? "#64748B" : "#4D7ED8"
                }
            }

            Text {
                anchors.centerIn: parent
                text: !audioSettings.mixerAvailable ? "UNCONFIRMED" : audioSettings.muted ? "MUTED" : audioSettings.volume + "%"
                color: "#F8FAFC"
                font.pixelSize: 32
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
                radius: 10
                color: root.selectedIndex === index ? "#2B416B" : "#111A2A"
                border.width: root.selectedIndex === index ? 3 : 1
                border.color: root.selectedIndex === index ? "#F8FAFC" : "#2B3853"

                Column {
                    anchors.left: parent.left
                    anchors.right: valueText.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 18
                    anchors.rightMargin: 16
                    spacing: 3
                    Text { text: modelData.name; color: "#F8FAFC"; font.pixelSize: 19; font.weight: Font.DemiBold }
                    Text { width: parent.width; text: modelData.help; color: "#9EABC1"; font.pixelSize: 13; wrapMode: index === 10 || index === 12 ? Text.WordWrap : Text.NoWrap; elide: Text.ElideRight }
                }

                Text {
                    id: valueText
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    width: index === 10 ? 260 : 190
                    horizontalAlignment: Text.AlignRight
                    text: modelData.value
                    color: "#C8D3E5"
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                    maximumLineCount: 4
                    elide: Text.ElideRight
                }
            }
        }

        Text {
            width: parent.width
            text: "Backend: " + audioSettings.backendStatus + "    ← → adjust/choose    Enter apply/toggle/reset    Esc back"
            color: "#73819B"
            font.pixelSize: 15
            wrapMode: Text.WordWrap
        }
    }
    }
}
