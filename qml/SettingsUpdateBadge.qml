import QtQuick

Rectangle {
    id: root
    property bool settingsTarget: false
    property var item: ({})
    property real unit: 1
    readonly property var actions: item.actions && item.actions.length ? item.actions : item.action ? [item.action] : []
    readonly property bool targetsSettings: settingsTarget || item.appId === "settings" ||
        actions.some(action => action && (action.type === "settings" ||
            (action.type === "launch-app" && action.appId === "settings")))
    visible: targetsSettings && typeof softwareUpdates !== "undefined" && softwareUpdates.available
    objectName: "settingsUpdateBadge"
    width: 18 * unit; height: width; radius: width / 2
    color: "#b9273c"; border.color: "white"; border.width: Math.max(1, unit)
    Accessible.role: Accessible.StaticText
    Accessible.name: qsTr("Isolde update available")
    Text {
        anchors.centerIn: parent
        text: "!"
        color: "white"
        font.bold: true
        font.pixelSize: 13 * root.unit
    }
}
