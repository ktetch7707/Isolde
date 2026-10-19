import QtQuick

Item {
    id: root
    property bool captureSetupFocused: false
    signal captureSetupRequested()
    function handleCommand(command) {
        if (captureSetupFocused) {
            if (command === inputManager.confirm) captureSetupRequested()
            else if (command === inputManager.navigateUp || command === inputManager.back) captureSetupFocused = false
            return
        }
        if (command === inputManager.confirm) componentHealth.refresh()
        else if (command === inputManager.navigateDown) {
            const next = Math.min(scroll.contentHeight - scroll.height, scroll.contentY + 120)
            if (scroll.contentY + scroll.height >= scroll.contentHeight - 24) captureSetupFocused = true
            else scroll.contentY = Math.max(0, next)
        } else if (command === inputManager.navigateUp) scroll.contentY = Math.max(0, scroll.contentY - 120)
    }
    onVisibleChanged: if (visible) componentHealth.refresh()
    Flickable {
        id: scroll
        anchors.fill: parent; clip: true
        contentHeight: contents.implicitHeight
        Column {
            id: contents; width: parent.width; spacing: 16
            Text { text: "COMPONENTS"; color: "#F8FAFC"; font.pixelSize: 30 }
            Text { width: parent.width; text: componentHealth.status; wrapMode: Text.WordWrap; color: "#BCC8DD"; font.pixelSize: 19 }
            Repeater {
                model: componentHealth.components
                delegate: Rectangle {
                    required property var modelData
                    width: parent.width; height: info.implicitHeight + 28; radius: 10; color: "#172238"
                    Column {
                        id: info; anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: 14; spacing: 6
                        Text { text: modelData.name + " · " + modelData.version; width: parent.width; wrapMode: Text.WordWrap; color: "#F8FAFC"; font.pixelSize: 20 }
                        Text { text: modelData.status; width: parent.width; wrapMode: Text.WordWrap; color: "#BCC8DD"; font.pixelSize: 17 }
                    }
                }
            }
            Rectangle {
                width: parent.width; height: 60; radius: 10; color: "#2B416B"; border.color: "#F8FAFC"
                Text { anchors.centerIn: parent; text: componentHealth.busy ? "Checking…" : "Check again · Enter"; color: "white"; font.pixelSize: 20 }
                MouseArea { anchors.fill: parent; onClicked: componentHealth.refresh() }
            }
            Text { width: parent.width; wrapMode: Text.WordWrap; text: "Capture setup is available under System → HDMI Inputs. Device presence and installed runtimes do not certify signal, switch routing, or hardware compatibility."; color: "#91A0BC"; font.pixelSize: 17 }
            Rectangle {
                width: parent.width; height: 60; radius: 8; color: "#172238"
                border.width: root.captureSetupFocused ? 3 : 1; border.color: root.captureSetupFocused ? "#FFFFFF" : "#4D7ED8"
                Text { anchors.centerIn: parent; text: "Open HDMI Inputs"; color: "#F8FAFC"; font.pixelSize: 19; font.weight: Font.DemiBold }
                MouseArea { anchors.fill: parent; onClicked: root.captureSetupRequested() }
            }
        }
    }
}
