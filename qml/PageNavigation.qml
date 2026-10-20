import QtQuick

Item {
    id: root
    property var composition: ({})
    property var appearance: ({})
    property var pages: []
    property string activeId: ""
    property bool enabledNavigation: true
    property bool navigationFocused: false
    property int selectedPage: 0
    readonly property string navigation: enabledNavigation ? composition.pageNavigation || "left" : "none"
    readonly property real contentLeft: navigation === "left" ? composition.sidebarWidth || 200 : 0
    readonly property real contentTop: navigation === "top" ? 70 : 0
    signal chosen(int index)
    Rectangle {
        visible: root.navigation !== "none"
        width: root.navigation === "left" ? root.contentLeft : parent.width
        height: root.navigation === "left" ? parent.height : root.contentTop
        color: root.appearance.surfaceColor || "#202126"
        ListView {
            anchors.fill: parent; anchors.margins: 12; spacing: 8; clip: true
            orientation: root.navigation === "left" ? ListView.Vertical : ListView.Horizontal
            model: root.pages; currentIndex: root.selectedPage; highlightRangeMode: ListView.ApplyRange
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: root.navigation === "left" ? ListView.view.width : 170; height: 46; radius: 10
                color: root.activeId === modelData.id ? root.appearance.selectedColor || "#353039" : "transparent"
                border.width: root.navigationFocused && root.selectedPage === index ? 2 : 0
                border.color: root.appearance.focusColor || "#C994A5"
                Text {
                    anchors.fill: parent; anchors.margins: 10; verticalAlignment: Text.AlignVCenter
                    text: modelData.name; elide: Text.ElideRight; color: root.appearance.textColor || "white"
                    font.pixelSize: 17 * (root.appearance.typographyScale || 1)
                    font.family: root.appearance.fontFamily || "DejaVu Sans"
                }
                MouseArea { anchors.fill: parent; onClicked: root.chosen(index) }
            }
        }
    }
}
