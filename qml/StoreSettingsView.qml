import QtQuick

Item {
    id:root
    property var appearance:({})
    property var catalog:appCatalog
    property int selectedIndex:0
    readonly property color ink:appearance.textColor
    readonly property color surface:appearance.surfaceColor
    readonly property color focusColor:appearance.focusColor
    readonly property real textScale:appearance.typographyScale || 1
    function handleCommand(command) {
        if(command===inputManager.navigateUp)selectedIndex=0
        else if(command===inputManager.navigateDown)selectedIndex=1
        else if(command===inputManager.confirm){
            if(selectedIndex===0)catalog.setOnlineStoreSearch(!catalog.onlineStoreSearch)
            else if(catalog.onlineStoreSearch)catalog.searchStore("")
        }
    }
    Column {
        anchors.fill:parent;spacing:24
        Text {textFormat:Text.PlainText;width:parent.width;text:qsTr("Online Store");font.pixelSize:30*root.textScale;color:root.ink;font.weight:Font.DemiBold}
        Text {textFormat:Text.PlainText;width:parent.width;text:qsTr("Account-free Android apps from verified repositories. Chromium apps use websites you choose.");wrapMode:Text.WordWrap;font.pixelSize:19*root.textScale;color:root.ink}
        Text {textFormat:Text.PlainText;width:parent.width;text:root.catalog.storeSourceName;wrapMode:Text.WordWrap;font.pixelSize:18*root.textScale;color:root.ink;opacity:0.7}
        Repeater {
            model:[qsTr("Online search: ")+(root.catalog.onlineStoreSearch?qsTr("On"):qsTr("Off")),qsTr("Refresh online catalog")]
            delegate:Rectangle {
                required property int index;required property string modelData
                width:parent.width;height:70*root.textScale;radius:root.appearance.cornerRadius || 12;color:root.surface
                border.width:root.selectedIndex===index?3:1;border.color:root.focusColor;opacity:index===1 && !root.catalog.onlineStoreSearch?0.5:1
                Text {textFormat:Text.PlainText;anchors.centerIn:parent;text:modelData;color:root.ink;font.pixelSize:20*root.textScale}
                MouseArea {anchors.fill:parent;onClicked:{root.selectedIndex=index;root.handleCommand(inputManager.confirm)}}
            }
        }
        Text {textFormat:Text.PlainText;width:parent.width;text:root.catalog.errorString || qsTr("Online search starts only when you turn it on. Your installed apps remain available with it off.");wrapMode:Text.WordWrap;font.pixelSize:18*root.textScale;color:root.ink}
    }
}
