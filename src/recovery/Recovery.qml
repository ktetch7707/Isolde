import QtQuick
import QtQuick.Window

Window {
    id: window
    visible: true
    visibility: Window.FullScreen
    color: "#080C18"
    title: "Isolde Recovery"
    property int selected: 0
    property bool confirming: false
    onClosing: close => close.accepted = false
    function activate(index) {
        if (index === 0) { Qt.exit(10); return }
        if (!previousAvailable) return
        if (confirming) Qt.exit(11)
        else confirming = true
    }
    Item {
        anchors.fill: parent
        focus: true
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Left || event.key === Qt.Key_Up) { window.selected = 0; window.confirming = false }
            else if (event.key === Qt.Key_Right || event.key === Qt.Key_Down) window.selected = previousAvailable ? 1 : 0
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Select) window.activate(window.selected)
            else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) { window.confirming = false; window.selected = 0 }
            event.accepted = true
        }
        Column {
            anchors.centerIn: parent
            width: Math.min(940, parent.width - 100)
            spacing: 26
            Text { text: "ISOLDE RECOVERY"; color: "#D5B083"; font.pixelSize: 20; font.letterSpacing: 3 }
            Text { text: "Let’s get you back to your TV."; color: "#F8FAFC"; font.pixelSize: 36; width: parent.width; wrapMode: Text.WordWrap }
            Text {
                width: parent.width; wrapMode: Text.WordWrap; color: "#BCC8DD"; font.pixelSize: 22
                text: window.confirming
                    ? "Use the previous installed application release? Your profiles and settings will not be reset. This does not undo OS updates or data migrations. Select Confirm previous release to continue, or Back to cancel."
                    : "Isolde could not stay running after repeated attempts. Automatic restarts have paused. You can retry or return to the previous installed application release. No internet connection is needed."
            }
            Text { text: recoveryError; visible: text.length > 0; color: "#FFB4A9"; font.pixelSize: 19; width: parent.width; wrapMode: Text.WordWrap }
            Repeater {
                model: ["Retry Isolde", window.confirming ? "Confirm previous release" : "Use previous release"]
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    width: parent.width; height: 70; radius: 12
                    opacity: index === 1 && !previousAvailable ? 0.4 : 1
                    color: window.selected === index ? "#D5B083" : "#19263B"
                    Text { anchors.centerIn: parent; text: modelData; color: window.selected === index ? "#080C18" : "#F8FAFC"; font.pixelSize: 23 }
                    MouseArea { anchors.fill: parent; enabled: index === 0 || previousAvailable; onClicked: { window.selected = index; window.activate(index) } }
                }
            }
            Text { text: previousAvailable ? "↑ ↓ choose   OK select   Back cancel" : "No previous release is available on this device."; color: "#92A2BC"; font.pixelSize: 18 }
        }
    }
}
