import QtQuick

Item {
    id:root
    SurfacePalette {id:shell;appearance:root.appearance}
    Rectangle {anchors.fill:parent;z:-1;color:shell.surface}
    property var appearance:({})
    property var catalog:appCatalog
    property int selectedIndex:0
    readonly property color ink:shell.text
    readonly property color surface:shell.surface
    readonly property color focusColor:shell.focus
    readonly property real textScale:shell.textScale
    function handleCommand(command) {
        if(command===inputManager.navigateUp)selectedIndex=Math.max(0,selectedIndex-1)
        else if(command===inputManager.navigateDown)selectedIndex=Math.min(2,selectedIndex+1)
        else if(command===inputManager.confirm){
            if(selectedIndex===0)catalog.setOnlineStoreSearch(!catalog.onlineStoreSearch)
            else if(selectedIndex===1 && catalog.onlineStoreSearch)catalog.searchStore("")
            else if(selectedIndex===2)catalog.setAndroidContainment(catalog.androidContainment==="fullscreen"?"windowed":"fullscreen")
        }
    }
    Column {
        anchors.fill:parent;spacing:24*shell.gapScale
        Text {font.family:shell.fontFamily;textFormat:Text.PlainText;width:parent.width;text:qsTr("Store preferences");font.pixelSize:30*root.textScale;color:root.ink;font.weight:Font.DemiBold}
        Text {font.family:shell.fontFamily;textFormat:Text.PlainText;width:parent.width;text:qsTr("Account-free Android apps from verified repositories. Chromium apps use websites you choose.");wrapMode:Text.WordWrap;font.pixelSize:19*root.textScale;color:root.ink}
        Text {font.family:shell.fontFamily;textFormat:Text.PlainText;width:parent.width;text:root.catalog.storeSourceName;wrapMode:Text.WordWrap;font.pixelSize:18*root.textScale;color:shell.muted}
        Repeater {
            model:[qsTr("Online search: ")+(root.catalog.onlineStoreSearch?qsTr("On"):qsTr("Off")),qsTr("Refresh online catalog"),qsTr("Android view: ")+(root.catalog.androidContainment==="windowed"?qsTr("Windowed"):qsTr("Fullscreen"))]
            delegate:Rectangle {
                required property int index;required property string modelData
                objectName:"storePreferenceRow-"+index
                width:parent.width;height:70*root.textScale;radius:shell.radius;color:root.selectedIndex===index?shell.selected:shell.surface
                border.width:root.selectedIndex===index?3:1;border.color:root.selectedIndex===index?shell.selectedFocus:shell.outline;opacity:index===1 && !root.catalog.onlineStoreSearch?0.5:1
                Text {font.family:shell.fontFamily;textFormat:Text.PlainText;anchors.centerIn:parent;text:modelData;color:root.selectedIndex===index?shell.selectedText:shell.text;font.pixelSize:20*root.textScale}
                MouseArea {anchors.fill:parent;onClicked:{root.selectedIndex=index;root.handleCommand(inputManager.confirm)}}
            }
        }
        Text {font.family:shell.fontFamily;textFormat:Text.PlainText;width:parent.width;text:root.catalog.errorString || qsTr("Online search starts only when you turn it on. Your installed apps remain available with it off.");wrapMode:Text.WordWrap;font.pixelSize:18*root.textScale;color:root.ink}
    }
}
