import QtQuick

Item {
    id: root
    property var appearance: ({})
    property int selectedIndex: 0
    SurfacePalette { id: shell; appearance: root.appearance }
    readonly property var updater: typeof softwareUpdates !== "undefined" ? softwareUpdates : null
    readonly property var actions: [
        { name: qsTr("Check for updates"), enabled: updater && !updater.checking },
        { name: qsTr("Dismiss notification"), enabled: updater && updater.available }
    ]

    function activate(index) {
        if (!updater || !actions[index].enabled) return
        if (index === 0) updater.check()
        else updater.dismiss()
    }
    function handleCommand(command) {
        if (command === inputManager.navigateUp) selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown) selectedIndex = Math.min(actions.length - 1, selectedIndex + 1)
        else if (command === inputManager.confirm) activate(selectedIndex)
    }
    function revealSelection() {
        const row = actionRows.itemAt(selectedIndex)
        if (!row) return
        if (row.y < scroll.contentY) scroll.contentY = row.y
        else if (row.y + row.height > scroll.contentY + scroll.height)
            scroll.contentY = Math.max(0, row.y + row.height - scroll.height)
    }
    onSelectedIndexChanged: Qt.callLater(revealSelection)

    Flickable {
        id: scroll
        anchors.fill: parent
        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Column {
            id: content
            width: parent.width
            spacing: 18 * shell.gapScale
            Text { width: parent.width; text: qsTr("Software Updates"); color: shell.text; font.family: shell.fontFamily; font.pixelSize: 30 * shell.textScale; wrapMode: Text.Wrap }
            Text { width: parent.width; text: qsTr("Installed version: ") + isoldeVersion; color: shell.text; font.family: shell.fontFamily; font.pixelSize: 20 * shell.textScale; wrapMode: Text.Wrap }
            Text { width: parent.width; visible: root.updater && root.updater.latestVersion !== ""; text: qsTr("Candidate version: ") + (root.updater ? root.updater.latestVersion : ""); color: shell.text; font.family: shell.fontFamily; font.pixelSize: 20 * shell.textScale; wrapMode: Text.Wrap }
            Text { width: parent.width; text: root.updater ? root.updater.status : qsTr("Update service unavailable."); color: shell.muted; font.family: shell.fontFamily; font.pixelSize: 18 * shell.textScale; wrapMode: Text.Wrap }
            Text { width: parent.width; text: qsTr("Download and installation are not yet available. Release authentication and recovery must be configured first."); color: shell.muted; font.family: shell.fontFamily; font.pixelSize: 18 * shell.textScale; wrapMode: Text.Wrap }
            Repeater {
                id: actionRows
                model: root.actions
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: parent.width; height: 64 * shell.textScale; radius: shell.radius
                    color: index === root.selectedIndex ? shell.selected : shell.surface
                    border.width: index === root.selectedIndex ? 3 : 1
                    border.color: index === root.selectedIndex ? shell.selectedFocus : shell.outline
                    opacity: modelData.enabled ? 1 : 0.6
                    Text { anchors.centerIn: parent; width: parent.width - 24; text: modelData.name; color: index === root.selectedIndex ? shell.selectedText : shell.text; font.family: shell.fontFamily; font.pixelSize: 20 * shell.textScale; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                    MouseArea { anchors.fill: parent; onClicked: { root.selectedIndex = index; root.activate(index) } }
                }
            }
        }
    }
}
