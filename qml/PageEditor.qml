import QtQuick
import QtQuick.Layouts

Item {
    id: root
    objectName: "pageEditor"
    required property var layoutModel
    property bool open: false
    property int selectedItem: 0
    property int editField: 0
    property string statusMessage: ""

    readonly property var items: layoutModel ? layoutModel.items : []
    readonly property var placements: layoutModel ? layoutModel.placements : []
    readonly property var grid: layoutModel ? layoutModel.grid : ({})
    readonly property int columns: (grid.columns || 1)
    readonly property real unit: 24

    readonly property var fields: [
        "Position (row,col)", "Size (rows,cols)", "Action", "Presentation", "Done"
    ]

    function placementFor(id) {
        for (let i = 0; i < root.placements.length; ++i) {
            const p = root.placements[i]
            if (p && p.id === id) return p.placement || p
        }
        return { x: 0, y: 0, width: 1, height: 1 }
    }

    function setPlacement(id, placement) {
        if (!layoutModel) return
        const existing = placementFor(id)
        const updated = Object.assign({}, existing, placement)
        layoutModel.setItemPlacement(id, updated)
        root.statusMessage = "Placement saved for " + id
    }

    function clearPlacement(id) {
        if (!layoutModel) return
        layoutModel.clearItemPlacement(id)
        root.statusMessage = "Placement cleared for " + id
    }

    function handleCommand(command) {
        if (command === inputManager.back) { root.open = false; return }
        if (items.length === 0) return

        const id = items[selectedItem] ? items[selectedItem].id : ""
        if (!id) return

        if (editField === 0) {
            const p = placementFor(id)
            if (command === inputManager.navigateLeft)  { p.x = Math.max(0, (p.x || 0) - 1); setPlacement(id, p) }
            else if (command === inputManager.navigateRight) { p.x = (p.x || 0) + 1; setPlacement(id, p) }
            else if (command === inputManager.navigateUp)    { p.y = Math.max(0, (p.y || 0) - 1); setPlacement(id, p) }
            else if (command === inputManager.navigateDown)  { p.y = (p.y || 0) + 1; setPlacement(id, p) }
            else if (command === inputManager.confirm) { editField = 1 }
        } else if (editField === 1) {
            const p = placementFor(id)
            if (command === inputManager.navigateLeft)  { p.width = Math.max(1, (p.width || 1) - 1); setPlacement(id, p) }
            else if (command === inputManager.navigateRight) { p.width = (p.width || 1) + 1; setPlacement(id, p) }
            else if (command === inputManager.navigateUp)    { p.height = Math.max(1, (p.height || 1) - 1); setPlacement(id, p) }
            else if (command === inputManager.navigateDown)  { p.height = (p.height || 1) + 1; setPlacement(id, p) }
            else if (command === inputManager.confirm) { editField = 2 }
        } else if (editField === 2) {
            if (command === inputManager.navigateLeft || command === inputManager.navigateRight) {
                const item = items.find(it => it.id === id)
                const actions = ["", "open", "play", "settings"]
                let idx = actions.indexOf(item ? (item.action || "") : "")
                idx = (idx + (command === inputManager.navigateRight ? 1 : -1) + actions.length) % actions.length
                if (layoutModel.setItemAction) {
                    layoutModel.setItemAction(id, { type: actions[idx] === "" ? undefined : actions[idx] })
                    root.statusMessage = "Action set to " + (actions[idx] || "none")
                }
            } else if (command === inputManager.confirm) {
                editField = 3
            }
        } else if (editField === 3) {
            if (command === inputManager.navigateLeft || command === inputManager.navigateRight) {
                const item = items.find(it => it.id === id)
                const styles = ["source", "media", "banner", "summary", "launcher"]
                const pres = item ? (item.presentation || {}) : {}
                let idx = styles.indexOf(pres.variant || "source")
                idx = (idx + (command === inputManager.navigateRight ? 1 : -1) + styles.length) % styles.length
                pres.variant = styles[idx]
                if (layoutModel.setItemPresentation) {
                    layoutModel.setItemPresentation(id, pres)
                    root.statusMessage = "Style set to " + styles[idx]
                }
            } else if (command === inputManager.confirm) {
                editField = 4
            }
        } else if (editField === 4) {
            if (command === inputManager.confirm) { root.open = false }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#101014"

        Column {
            anchors.fill: parent
            anchors.margins: 40
            spacing: 20

            Text {
                text: "PAGE EDITOR"
                color: "#EEEAF0"
                font.pixelSize: 32
                font.weight: Font.DemiBold
            }

            Text {
                text: "↑↓←→ move · Enter next field · Back exit"
                color: "#AAA4B2"
                font.pixelSize: 16
            }

            Text {
                text: "Field: " + root.fields[root.editField] + "   Item: " + (root.items[root.selectedItem] ? root.items[root.selectedItem].name || root.items[root.selectedItem].id : "none")
                color: "#BD8198"
                font.pixelSize: 20
            }

            GridLayout {
                id: editGrid
                width: parent.width
                columns: root.columns
                columnSpacing: root.unit * 0.5
                rowSpacing: root.unit * 0.5

                Repeater {
                    model: root.items
                    delegate: Item {
                        required property int index
                        required property var modelData
                        readonly property var placement: root.placementFor(modelData.id)
                        readonly property int cellX: placement.x || placement.col || 0
                        readonly property int cellY: placement.y || placement.row || 0
                        readonly property int w: placement.width || placement.cols || 1
                        readonly property int h: placement.height || placement.rows || 1
                        readonly property real cellW: ((parent.width - (parent.columns - 1) * parent.columnSpacing) / parent.columns)
                        readonly property real cellH: 140 * root.unit

                        Layout.row: cellY
                        Layout.column: cellX
                        Layout.rowSpan: h
                        Layout.columnSpan: w
                        Layout.preferredWidth: cellW * w - parent.columnSpacing
                        Layout.preferredHeight: cellH * h

                        DragHandler {
                            id: dragHandler
                            target: card
                            dragThreshold: 8 * root.unit
                            onActiveChanged: {
                                if (!active) {
                                    const nx = Math.round(card.x / cellW)
                                    const ny = Math.round(card.y / cellH)
                                    root.setPlacement(modelData.id, {
                                        x: Math.max(0, nx),
                                        y: Math.max(0, ny),
                                        width: w,
                                        height: h
                                    })
                                }
                            }
                        }

                        HomeCard {
                            id: card
                            anchors.fill: parent
                            anchors.margins: 8
                            item: modelData
                            appearance: root.layoutModel ? root.layoutModel.appearance : ({})
                            unit: root.unit
                            selected: root.selectedItem === index
                            selectMode: "single"
                            selectTrigger: "click"
                            selectAction: "navigate"
                            onActivated: root.selectedItem = index
                        }

                        Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            border.width: root.selectedItem === index ? 3 : 0
                            border.color: root.selectedItem === index ? "#BD8198" : transparent
                            radius: 8
                        }

                        Text {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottomMargin: 6
                            text: "(" + cellX + "," + cellY + ") " + w + "x" + h
                            color: "#AAA4B2"
                            font.pixelSize: 11
                        }

                        Rectangle {
                            id: removeBtn
                            anchors.top: parent.top
                            anchors.right: parent.right
                            width: 28; height: 28
                            color: "#3D3A4A"
                            radius: 14
                            visible: root.selectedItem === index
                            Text {
                                anchors.centerIn: parent
                                text: "✕"; color: "#E57373"; font.pixelSize: 16; font.weight: Font.DemiBold
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    root.clearPlacement(modelData.id)
                                    if (root.selectedItem === index && root.selectedItem > 0)
                                        root.selectedItem = root.selectedItem - 1
                                }
                            }
                        }

                        Rectangle {
                            id: resizeHandle
                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                            width: 16; height: 16
                            color: "#BD8198"
                            radius: 4
                            visible: root.selectedItem === index
                            DragHandler {
                                target: null
                                onActiveChanged: {
                                    if (active) return
                                    const dw = Math.round(persistentTranslation.x / cellW)
                                    const dh = Math.round(persistentTranslation.y / cellH)
                                    root.setPlacement(modelData.id, {
                                        x: cellX, y: cellY,
                                        width: Math.max(1, w + dw),
                                        height: Math.max(1, h + dh)
                                    })
                                    persistentTranslation = Qt.vector2d(0, 0)
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.selectedItem = index
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
