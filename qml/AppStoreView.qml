import QtQuick
import QtWebEngine
import QtWebChannel

Item {
    id: root
    required property var entries
    property bool searchOpen: false
    property string searchText: ""
    WebChannel {
        id: channel
        Component.onCompleted: registerObject("appCatalog", appCatalog)
    }

    function handleCommand(command) {
        if (searchOpen) {
            storeKeyboard.handleCommand(command)
            return
        }
        if (command === inputManager.navigateLeft) {
            searchOpen = true
            storeKeyboard.passwordMode = false
            storeKeyboard.value = searchText
            storeKeyboard.selectedIndex = 0
        } else if (command === inputManager.navigateRight) {
            searchText = ""
            web.runJavaScript("window.isoldeSetSearch && window.isoldeSetSearch('')")
        } else if (command === inputManager.navigateUp) web.runJavaScript("window.isoldeMove && window.isoldeMove(-1)")
        else if (command === inputManager.navigateDown) web.runJavaScript("window.isoldeMove && window.isoldeMove(1)")
        else if (command === inputManager.confirm) web.runJavaScript("window.isoldeActivate && window.isoldeActivate()")
    }
    WebEngineView {
        id: web
        anchors.fill: parent
        url: "qrc:/store/store/index.html"
        webChannel: channel
        settings.localContentCanAccessRemoteUrls: false
        settings.localContentCanAccessFileUrls: false
        settings.javascriptCanOpenWindows: false
    }

    Rectangle {
        visible: root.searchOpen
        anchors.fill: parent
        color: "#CC050812"
        z: 5
    }

    OnScreenKeyboard {
        id: storeKeyboard
        visible: root.searchOpen
        z: 6
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 34
        height: Math.min(400, parent.height - 68)
        onAccepted: text => {
            root.searchText = text
            root.searchOpen = false
            web.runJavaScript("window.isoldeSetSearch && window.isoldeSetSearch(" + JSON.stringify(text) + ")")
        }
        onCanceled: root.searchOpen = false
    }
}
