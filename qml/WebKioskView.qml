import QtQuick
import QtWebEngine

Item {
    id: root
    required property string appName
    required property string url
    property var allowedOrigins: []
    property color accent: "#64748B"
    property bool failed: false
    property string errorText: ""
    property var appearance:({})
    signal exitRequested()

    function remoteCommand(name) {
        web.runJavaScript("Boolean(globalThis.__isoldeRemote && globalThis.__isoldeRemote.handle("+JSON.stringify(name)+"))",WebEngineScript.ApplicationWorld,handled=>{
            if(!root.visible)return
            if(name==="back" && !handled){if(web.canGoBack)web.goBack();else root.exitRequested()}
        })
    }
    onVisibleChanged:if(visible)Qt.callLater(()=>{if(root.visible)web.forceActiveFocus()})

    function handleCommand(command) {
        if (failed && command === inputManager.confirm) {
            failed = false
            web.reload()
        } else if (command === inputManager.navigateUp)
            remoteCommand("up")
        else if (command === inputManager.navigateDown)
            remoteCommand("down")
        else if (command === inputManager.navigateLeft)
            remoteCommand("left")
        else if (command === inputManager.navigateRight)
            remoteCommand("right")
        else if (command === inputManager.confirm)
            remoteCommand("confirm")
        else if(command===inputManager.back)remoteCommand("back")
        else if(command===inputManager.menu)remoteCommand("menu")
        else if(command===inputManager.search)remoteCommand("search")
    }

    function originFor(candidate) {
        const parts = String(candidate).match(/^([^:]+:\/\/[^/]+)/)
        return parts ? parts[1] : ""
    }

    function isAllowed(candidate) {
        if (!allowedOrigins || allowedOrigins.length === 0)
            return true
        const origin = originFor(candidate)
        for (let i = 0; i < allowedOrigins.length; ++i) {
            if (origin === allowedOrigins[i])
                return true
        }
        return false
    }

    Rectangle {
        anchors.fill: parent
        color: "#050812"
    }

    WebEngineView {
        id: web
        anchors.fill: parent
        url: root.visible && root.url.length ? root.url : "about:blank"
        userScripts.collection:[{name:"isolde-remote",injectionPoint:WebEngineScript.DocumentReady,worldId:WebEngineScript.ApplicationWorld,runsOnSubFrames:true,sourceCode:inputManager.webRemoteScript(root.appearance)}]
        settings.javascriptCanOpenWindows: false
        settings.localContentCanAccessFileUrls: false
        settings.localContentCanAccessRemoteUrls: false
        profile: IsoldeWebProfile {
            storageName: browserStorageName
        }
        onNavigationRequested: request => {
            if(String(request.url)==="about:blank")return
            if (!root.isAllowed(request.url)) {
                request.action = WebEngineNavigationRequest.IgnoreRequest
                root.failed = true
                root.errorText = "Blocked navigation outside this TV app: " + root.originFor(request.url)
            }
        }
        onLoadingChanged: loadRequest => {
            if (loadRequest.status === WebEngineView.LoadStartedStatus) {
                root.failed = false
                root.errorText = ""
            } else if (loadRequest.status === WebEngineView.LoadFailedStatus) {
                root.failed = true
                root.errorText = loadRequest.errorString
            } else if(loadRequest.status===WebEngineView.LoadSucceededStatus && root.visible){
                web.forceActiveFocus()
            }
        }
    }

    Rectangle {
        visible: web.loading && !root.failed
        anchors.fill: parent
        color: "#DD050812"
        Column {
            anchors.centerIn: parent
            width: Math.min(680, parent.width - 80)
            spacing: 20
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.appName.toUpperCase()
                color: "#F8FAFC"
                font.pixelSize: 42
                font.weight: Font.DemiBold
                font.letterSpacing: 5
            }
            Rectangle {
                width: parent.width
                height: 12
                radius: 6
                color: "#17243A"
                Rectangle {
                    width: parent.width * web.loadProgress / 100
                    height: parent.height
                    radius: parent.radius
                    color: root.accent
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Loading TV app  ·  " + web.loadProgress + "%"
                color: "#AAB5C9"
                font.pixelSize: 18
            }
        }
    }

    Rectangle {
        visible: root.failed
        anchors.fill: parent
        color: "#EE050812"
        Column {
            anchors.centerIn: parent
            width: Math.min(760, parent.width - 90)
            spacing: 16
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "CAN'T OPEN " + root.appName.toUpperCase()
                color: "#F8FAFC"
                font.pixelSize: 36
                font.weight: Font.DemiBold
                font.letterSpacing: 4
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: root.errorText || "The TV web app did not load."
                color: "#D5B083"
                font.pixelSize: 18
                wrapMode: Text.WordWrap
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: "Enter retry    Esc Back    H Home"
                color: "#AAB5C9"
                font.pixelSize: 17
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: web.loading || root.failed
        height: 52
        color: "#CC050812"
        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 28
            spacing: 24
            Text { text: root.appName; color: "#F8FAFC"; font.pixelSize: 16; font.weight: Font.DemiBold }
            Text { text: web.loading ? "Loading " + web.loadProgress + "%    Esc Back    H Home" : "↑↓ scroll    ← back    → forward    Enter focus/retry    Esc Back    H Home"; color: "#AAB5C9"; font.pixelSize: 15 }
        }
    }
}
