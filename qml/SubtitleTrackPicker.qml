import QtQuick
import QtQuick.Controls

Item {
    id: root
    property var kodiClient: null
    property var appearance: ({})
    property bool open: false
    property int selectedIndex: 0
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
        radius: 16
        color: shell.surface
        border.width: 2
        border.color: shell.focus
        Column {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 12
            Text {
                text: "Subtitle tracks · Kodi"
                color: shell.text
                font.pixelSize: 28 * shell.textScale
                font.weight: Font.DemiBold
            }
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: kodiClient ? kodiClient.subtitleStatus : "Kodi playback is unavailable."
                color: shell.muted
                font.pixelSize: 16 * shell.textScale
            }
            ListView {
                id: tracks
                width: parent.width
                height: parent.height - 72
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
                    height: 58
                    radius: 8
                    color: index === root.selectedIndex ? shell.selected : shell.elevated
                    border.width: index === root.selectedIndex ? 2 : 0
                    border.color: shell.focus
                    Text {
                        anchors.fill: parent
                        anchors.margins: 12
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        text: modelData
                        color: shell.text
                        font.pixelSize: 17 * shell.textScale
                    }
                }
                onCountChanged: positionViewAtIndex(root.selectedIndex, ListView.Contain)
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
