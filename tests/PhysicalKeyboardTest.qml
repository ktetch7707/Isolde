import QtQuick
import "../qml"

Item {
    focus: true
    property bool typing: true
    property string acceptedValue: ""
    property int shellCommands: 0
    Keys.onPressed: event => {
        const command = inputManager.commandForKey(event.key)
        if (command !== inputManager.none) {
            if (typing) keyboard.handleCommand(command)
            else shellCommands++
            event.accepted = true
        }
    }
    OnScreenKeyboard {
        id: keyboard
        objectName: "physicalKeyboard"
        anchors.fill: parent
        visible: parent.typing
        passwordMode: true
        onAccepted: text => { parent.acceptedValue = text; parent.typing = false }
        onCanceled: parent.typing = false
    }
}
