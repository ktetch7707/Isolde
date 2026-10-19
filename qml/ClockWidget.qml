import QtQuick

Rectangle {
    id: root
    required property bool selected
    required property real roundness
    property var appearance: ({})
    property date now: new Date()
    radius: Math.min(width, height) * 0.5 * roundness
    color: selected ? appearance.selectedColor || "#353039" : appearance.surfaceColor || "#202126"
    border.width: selected ? 4 : 1
    border.color: selected ? appearance.focusColor || "#C994A5" : Qt.lighter(color, 1.2)

    Timer { interval: 1000; running: true; repeat: true; onTriggered: root.now = new Date() }

    Column {
        anchors.centerIn: parent
        spacing: 4
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Qt.formatTime(root.now, "h:mm")
            color: root.appearance.textColor || "#F5F1F3"
            font.pixelSize: (root.height < 180 ? 40 : 54) * (root.appearance.typographyScale || 1)
            font.weight: Font.Light
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Qt.formatDate(root.now, "dddd, MMMM d")
            color: root.appearance.mutedColor || "#AAA5AF"
            font.pixelSize: (root.height < 180 ? 13 : 16) * (root.appearance.typographyScale || 1)
        }
    }
}
