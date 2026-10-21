import QtQuick

Item {
    id: root
    objectName: "customizeHome"
    property var layoutModel: homeLayout
    property var provider: typeof pageData !== "undefined" ? pageData : null
    property string initialPageId: ""
    property var items: []
    property var availableApps: []
    property var artwork: []
    property var themes: []
    property bool ready: false
    readonly property bool editing: studio.open || skins.open
    signal closed()
    signal pageRequested(string id)
    function start() { if (ready && visible && !editing) studio.show(initialPageId, "") }
    function handleCommand(command) {
        if (skins.open) skins.handleCommand(command)
        else studio.handleCommand(command)
    }
    onVisibleChanged: {
        if (visible) start()
        else {
            skins.open = false
            studio.open = false
            if (studio.session) studio.session.invalidate()
        }
    }
    Component.onCompleted: { ready = true; start() }
    PageStudioView {
        id: studio
        anchors.fill: parent
        layoutModel: root.layoutModel
        provider: root.provider
        onClosed: root.closed()
        onSkinsRequested: {
            if (session) session.invalidate()
            open = false
            skins.begin()
        }
    }
    SkinLibrary {
        id: skins
        anchors.fill: parent
        layoutModel: root.layoutModel
        z: 101
        onOpenChanged: if (!open && root.visible) Qt.callLater(root.start)
    }
}
