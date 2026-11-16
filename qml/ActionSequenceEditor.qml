import QtQuick
import QtQuick.Layouts

Item {
    id: root
    objectName: "actionSequenceEditor"
    required property var layoutModel
    required property var item
    property bool open: false
    property int selectedAction: 0
    property bool pickerOpen: false
    property bool waitEditorOpen: false

    readonly property var actions: item.actions ? item.actions.slice() : []
    readonly property real unit: 24
    readonly property var actionTypes: [
        { type: "open-page", label: "Open Page", detail: "Navigate to a page" },
        { type: "launch-app", label: "Launch App", detail: "Start an installed app" },
        { type: "open-collection", label: "Open Collection", detail: "Open a Kodi collection" },
        { type: "play-movie", label: "Play Movie", detail: "Play a Kodi movie" },
        { type: "open-show", label: "Open Show", detail: "Open a Kodi TV show" },
        { type: "settings", label: "Settings", detail: "Open Settings app" },
        { type: "switch-input", label: "Switch Input", detail: "Switch HDMI input" },
        { type: "desktop-mode", label: "Desktop Mode", detail: "Open desktop" },
        { type: "wait", label: "Wait", detail: "Pause between actions" },
        { type: "open-search", label: "Search", detail: "Open search" },
        { type: "app-drawer", label: "App Drawer", detail: "Open app drawer" },
        { type: "open-apps", label: "Open Apps", detail: "Open all apps grid" },
        { type: "root-page", label: "Root Page", detail: "Go to home page" },
        { type: "open-guide", label: "Guide", detail: "Open TV guide" }
    ]

    function currentAction() {
        return actions.length > 0 ? actions[selectedAction] : {}
    }

    function moveAction(from, to) {
        if (from < 0 || from >= actions.length || to < 0 || to >= actions.length) return
        const moved = actions.splice(from, 1)[0]
        actions.splice(to, 0, moved)
        selectedAction = to
        save()
    }

    function deleteAction() {
        if (actions.length === 0) return
        actions.splice(selectedAction, 1)
        if (selectedAction >= actions.length) selectedAction = Math.max(0, actions.length - 1)
        save()
    }

    function addAction(type) {
        let action = { type: type, schemaVersion: 1 }
        if (type === "wait") action.duration = 500
        else if (type === "open-page") action.pageId = ""
        else if (type === "launch-app") action.appId = ""
        else if (type === "open-collection") action.collectionId = ""
        else if (type === "play-movie") { action.collectionId = ""; action.movieId = 1 }
        else if (type === "open-show") { action.collectionId = ""; action.tvShowId = 1 }
        else if (type === "settings") action.section = 0
        else if (type === "switch-input") action.input = 1
        else if (type === "desktop-mode") action.label = "Desktop Mode"
        else if (type === "open-search") {}
        else if (type === "app-drawer") {}
        else if (type === "open-apps") {}
        else if (type === "root-page") {}
        else if (type === "open-guide") {}
        actions.push(action)
        selectedAction = actions.length - 1
        save()
    }

    function editField(field, value) {
        if (actions.length === 0) return
        actions[selectedAction][field] = value
        save()
    }

    function save() {
        if (layoutModel.setItemAction) {
            layoutModel.setItemAction(item.id, actions)
            root.statusMessage = "Actions saved (" + actions.length + ")"
        }
    }

    property string statusMessage: ""

    Rectangle {
        anchors.fill: parent
        color: "#101014"

        Column {
            anchors.fill: parent
            anchors.margins: 40
            spacing: 20

            Text {
                text: "EDIT ACTIONS"
                color: "#EEEAF0"
                font.pixelSize: 32
                font.weight: Font.DemiBold
            }

            Text {
                text: "↑↓ select action · ←→ reorder · A add · D delete · Enter edit · Back exit"
                color: "#AAA4B2"
                font.pixelSize: 16
            }

            Text {
                text: "Item: " + (item.name || item.id) + "   Actions: " + actions.length
                color: "#BD8198"
                font.pixelSize: 20
            }

            // Action list
            ListView {
                id: actionList
                width: parent.width
                height: 300 * root.unit
                model: actions
                currentIndex: root.selectedAction
                highlightRangeMode: ListView.ApplyRange
                spacing: 8 * root.unit
                onCurrentIndexChanged: {
                    root.waitEditorOpen = actions.length > 0 && actions[root.selectedAction].type === "wait"
                }
                delegate: Rectangle {
                    width: ListView.view.width
                    height: 60 * root.unit
                    color: ListView.view.currentIndex === index ? "#BD8198" : "#28262F"
                    radius: 8
                    border.width: 1
                    border.color: "#3D3A4A"
                    Column {
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            text: modelData.type.toUpperCase() + (modelData.type === "wait" ? " · " + modelData.duration + "ms" : "")
                            color: "#EEEAF0"
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: modelData.pageId || modelData.appId || modelData.collectionId || modelData.target || modelData.label || ""
                            color: "#AAA4B2"
                            font.pixelSize: 14
                            visible: modelData.type !== "wait"
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.selectedAction = index
                    }
                }
            }

            // Action buttons row
            Row {
                spacing: 12 * root.unit

                Rectangle {
                    width: 80 * root.unit; height: 40 * root.unit
                    color: root.selectedAction > 0 ? "#BD8198" : "#3D3A4A"
                    radius: 8
                    Text {
                        anchors.centerIn: parent
                        text: "↑"; color: "#EEEAF0"; font.pixelSize: 18; font.weight: Font.DemiBold
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: root.selectedAction > 0
                        onClicked: root.moveAction(root.selectedAction, root.selectedAction - 1)
                    }
                }

                Rectangle {
                    width: 80 * root.unit; height: 40 * root.unit
                    color: root.selectedAction < actions.length - 1 ? "#BD8198" : "#3D3A4A"
                    radius: 8
                    Text {
                        anchors.centerIn: parent
                        text: "↓"; color: "#EEEAF0"; font.pixelSize: 18; font.weight: Font.DemiBold
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: root.selectedAction < actions.length - 1
                        onClicked: root.moveAction(root.selectedAction, root.selectedAction + 1)
                    }
                }

                Rectangle {
                    width: 80 * root.unit; height: 40 * root.unit
                    color: actions.length > 0 ? "#E57373" : "#3D3A4A"
                    radius: 8
                    Text {
                        anchors.centerIn: parent
                        text: "✕"; color: "#EEEAF0"; font.pixelSize: 18; font.weight: Font.DemiBold
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: actions.length > 0
                        onClicked: root.deleteAction()
                    }
                }

                Rectangle {
                    width: 80 * root.unit; height: 40 * root.unit
                    color: "#06B6D4"
                    radius: 8
                    Text {
                        anchors.centerIn: parent
                        text: "Add"; color: "#101014"; font.pixelSize: 18; font.weight: Font.DemiBold
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.pickerOpen = true
                    }
                }
            }

            // Action type picker (overlay when open)
            Rectangle {
                id: pickerOverlay
                visible: root.pickerOpen
                anchors.fill: parent
                color: "#00000080"

                Column {
                    anchors.center: parent
                    spacing: 12 * root.unit
                    width: 600 * root.unit

                    Text {
                        text: "ADD ACTION"
                        color: "#EEEAF0"
                        font.pixelSize: 28
                        font.weight: Font.DemiBold
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    ListView {
                        width: parent.width
                        height: 400 * root.unit
                        model: root.actionTypes
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 56 * root.unit
                            color: ListView.view.currentIndex === index ? "#BD8198" : "#28262F"
                            radius: 8
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 16 * root.unit
                                text: modelData.label
                                color: "#EEEAF0"
                                font.pixelSize: 20
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: 16 * root.unit
                                text: modelData.detail
                                color: "#AAA4B2"
                                font.pixelSize: 14
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: { root.addAction(modelData.type); root.pickerOpen = false }
                            }
                        }
                    }

                    Rectangle {
                        width: 120 * root.unit; height: 40 * root.unit
                        color: "#E57373"
                        radius: 8
                        Text {
                            anchors.centerIn: parent
                            text: "Cancel"; color: "#EEEAF0"; font.pixelSize: 16
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.pickerOpen = false
                        }
                    }
                }
            }

            // Wait duration editor
            Rectangle {
                visible: root.waitEditorOpen && actions.length > 0 && actions[root.selectedAction].type === "wait"
                anchors.fill: parent
                color: "#000000a0"

                Column {
                    anchors.center: parent
                    spacing: 16 * root.unit
                    width: 500 * root.unit

                    Text {
                        text: "WAIT DURATION"
                        color: "#EEEAF0"
                        font.pixelSize: 28
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: actions[root.selectedAction].duration + " ms"
                        color: "#BD8198"
                        font.pixelSize: 24
                    }

                    Row {
                        spacing: 12 * root.unit
                        Rectangle {
                            width: 80 * root.unit; height: 40 * root.unit
                            color: "#BD8198"; radius: 8
                            Text { anchors.centerIn: parent; text: "−"; color: "#101014"; font.pixelSize: 24 }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: { root.editField("duration", Math.max(50, actions[root.selectedAction].duration - 50)) }
                            }
                        }
                        Rectangle {
                            width: 80 * root.unit; height: 40 * root.unit
                            color: "#BD8198"; radius: 8
                            Text { anchors.centerIn: parent; text: "+"; color: "#101014"; font.pixelSize: 24 }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: { root.editField("duration", Math.min(30000, actions[root.selectedAction].duration + 50)) }
                            }
                        }
                    }

                    Rectangle {
                        width: 120 * root.unit; height: 40 * root.unit
                        color: "#06B6D4"; radius: 8
                        Text { anchors.centerIn: parent; text: "Done"; color: "#101014"; font.pixelSize: 16 }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.waitEditorOpen = false
                        }
                    }
                }
            }

            Text {
                text: root.statusMessage
                color: "#A7F3D0"
                font.pixelSize: 15
                visible: root.statusMessage !== ""
            }
        }
    }
}