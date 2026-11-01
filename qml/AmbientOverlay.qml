import QtQuick
import QtQuick.Controls

Popup {
    id: root
    objectName: "ambientOverlay"
    property var settings: ambientSettings
    property real scaleFactor: 1
    property bool active: false
    signal dismissRequested()
    function handleCommand(command) { if (active) dismissRequested() }
    parent: Overlay.overlay
    x: 0; y: 0
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    z: 1300
    padding: 0; margins: 0
    modal: true; dim: false; focus: true
    visible: active
    closePolicy: Popup.NoAutoClose
    background: null
    contentItem: Item {
        focus: true
        Keys.onShortcutOverride: event => { event.accepted = true }
        Keys.onPressed: event => { root.dismissRequested(); event.accepted = true }
        AmbientScreen {
            settings: root.settings
            active: root.active
            width: parent.width / root.scaleFactor
            height: parent.height / root.scaleFactor
            scale: root.scaleFactor
            transformOrigin: Item.TopLeft
            onDismissRequested: root.dismissRequested()
        }
        MouseArea { anchors.fill: parent; onClicked: root.dismissRequested() }
    }
}
