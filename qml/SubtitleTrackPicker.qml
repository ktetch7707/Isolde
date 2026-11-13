import QtQuick
import QtQuick.Controls

Item {
    id: root
    property var kodiClient: null
    property var appearance: ({})
    property bool open: false
    property int selectedIndex: 0
    onSelectedIndexChanged: tracks.positionViewAtIndex(selectedIndex, ListView.Contain)
    signal closeRequested()
    SurfacePalette { id: shell; appearance: root.appearance }

    function handleCommand(command) {
        if (!open) return
        if (command === inputManager.back || command === inputManager.menu) {
            closeRequested()
            return
        }
        const count = (kodiClient ? kodiClient.subtitleTracks.length : 0) + 1
        if (command === inputManager.navigateUp)
            selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown)
            selectedIndex = Math.min(count - 1, selectedIndex + 1)
        else if (command === inputManager.confirm) {
            if (kodiClient) kodiClient.setSubtitleTrack(selectedIndex === 0 ? -1 : kodiClient.subtitleTracks[selectedIndex - 1].id)
        }
    }

    function refreshWhenReady() {
        if (!open || !kodiClient) return
        if (kodiClient.busy) retryTimer.restart()
        else kodiClient.refreshSubtitleTracks()
    }

    onOpenChanged: {
        if (open) {
            selectedIndex = 0
            refreshWhenReady()
        }
    }

    visible: open
    z: 1400
    Rectangle { anchors.fill: parent; color: shell.scrim }
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 80, 680)
        height: Math.min(parent.height - 80, 560)
        radius: shell.radius
        color: shell.surface
        border.width: 2
        border.color: shell.focus
        Column {
            id:trackContents
            anchors.fill: parent
            anchors.margins: 24
            spacing: 12
            Text {
                id:trackHeading
                width:parent.width
                wrapMode:Text.WordWrap
                text: "Subtitle tracks · Kodi"
                color: shell.text
                font.pixelSize: 28 * shell.textScale
                font.weight: Font.DemiBold
            }
            Text {
                id:trackStatus
                width: parent.width
                wrapMode: Text.WordWrap
                text: kodiClient ? kodiClient.subtitleStatus : "Kodi playback is unavailable."
                color: shell.muted
                font.pixelSize: 16 * shell.textScale
            }
            ListView {
                id: tracks
                width: parent.width
                height: Math.max(0,parent.height-trackHeading.height-trackStatus.height-2*parent.spacing)
                clip: true
                model: ["Off"].concat(kodiClient ? kodiClient.subtitleTracks.map(track =>
                    (track.language ? track.language.toUpperCase() + " · " : "") +
                    (track.name || ("Track " + track.id)) +
                    (track.external ? " · external" : "") +
                    (track.selected ? " · selected" : "")) : [])
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    width: tracks.width
                    height: Math.max(58*shell.textScale,trackLabel.implicitHeight+24)
                    radius: shell.radius
                    color: index === root.selectedIndex ? shell.selected : shell.elevated
                    border.width: index === root.selectedIndex ? 2 : 0
                    border.color: shell.focus
                    Text {
                        id:trackLabel
                        anchors.fill: parent
                        anchors.margins: 12
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.WordWrap
                        textFormat:Text.PlainText
                        text: modelData
                        color: index===root.selectedIndex?shell.selectedText:shell.text
                        font.pixelSize: 17 * shell.textScale
                    }
                }
                onCountChanged: {
                    root.selectedIndex=Math.max(0,Math.min(root.selectedIndex,count-1))
                    positionViewAtIndex(root.selectedIndex, ListView.Contain)
                }
            }
        }
    }
    Timer {
        id: retryTimer
        interval: 120
        repeat: false
        onTriggered: root.refreshWhenReady()
    }
}
