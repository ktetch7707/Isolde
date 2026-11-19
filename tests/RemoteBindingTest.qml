import QtQuick
import "../qml"

Item {
    focus: true
    Keys.onPressed: event => {
        const command = inputManager.commandForKey(event.key)
        if (command !== inputManager.none) {
            page.handleCommand(command)
            event.accepted = true
        }
    }
    RemoteSettingsView { id: page; objectName: "remotePage"; anchors.fill: parent }
}
