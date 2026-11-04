import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    Rectangle {anchors.fill:parent;z:-1;color:shell.surface}
    property var appearance: ({})
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
            id: contents; width: parent.width; spacing: 16 * shell.gapScale
            Text { font.family: shell.fontFamily; text: "COMPONENTS"; color: shell.text; font.pixelSize: 30 * shell.textScale }
            Text { font.family: shell.fontFamily; width: parent.width; text: componentHealth.status; wrapMode: Text.WordWrap; color: shell.muted; font.pixelSize: 19 * shell.textScale }
            Repeater {
                model: componentHealth.components
                delegate: Rectangle {
                    required property var modelData
                    width: parent.width; height: info.implicitHeight + 28; radius: shell.radius; color: shell.surface
                    Column {
                        id: info; anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: 14; spacing: 6 * shell.gapScale
                        Text { font.family: shell.fontFamily; text: modelData.name + " · " + modelData.version; width: parent.width; wrapMode: Text.WordWrap; color: shell.text; font.pixelSize: 20 * shell.textScale }
                        Text { font.family: shell.fontFamily; text: modelData.status; width: parent.width; wrapMode: Text.WordWrap; color: shell.muted; font.pixelSize: 17 * shell.textScale }
                    }
                }
            }
            Rectangle {
                width: parent.width; height: 60; radius: shell.radius; color: shell.selected; border.color: shell.text
                Text { font.family: shell.fontFamily; anchors.centerIn: parent; text: componentHealth.busy ? "Checking…" : "Check again · Enter"; color: shell.text; font.pixelSize: 20 * shell.textScale }
                MouseArea { anchors.fill: parent; onClicked: componentHealth.refresh() }
            }
            Text { font.family: shell.fontFamily; width: parent.width; wrapMode: Text.WordWrap; text: "Capture setup is available under System → HDMI Inputs. Device presence and installed runtimes do not certify signal, switch routing, or hardware compatibility."; color: shell.muted; font.pixelSize: 17 * shell.textScale }
            Rectangle {
                width: parent.width; height: 60; radius: shell.radius; color: shell.surface
                border.width: root.captureSetupFocused ? 3 : 1; border.color: root.captureSetupFocused ? shell.text : shell.focus
                Text { font.family: shell.fontFamily; anchors.centerIn: parent; text: "Open HDMI Inputs"; color: shell.text; font.pixelSize: 19 * shell.textScale; font.weight: Font.DemiBold }
                MouseArea { anchors.fill: parent; onClicked: root.captureSetupRequested() }
            }
        }
    }
}
