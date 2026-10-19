import QtQuick
import QtQuick.Dialogs

Rectangle {
    id: root
    required property var layoutModel
    property bool open: false
    property int selected: 0
    property int field: 0
    readonly property var categories: layoutModel.categories
    readonly property var variants: ["source", "media", "banner", "summary", "launcher", "orb", "recent"]
    readonly property var rows: ["Use composed Home", "Dashboard columns", "Section style", "Cards per row", "Section height", "Move section earlier", "Move section later", "Panel opacity", "Panel blur", "Choose wallpaper", "Remove wallpaper", "Header style", "Show clock", "Done"]
    color: "#101014"; visible: open
    function enable() {
        let config = Object.assign({}, layoutModel.composition)
        if (!config.schemaVersion) config = {schemaVersion:1, enabled:true, fit:true, columns:1, margin:40, gap:20, navigation:[{id:"home",name:"Home"}]}
        else config.enabled = !config.enabled
        layoutModel.setComposition(config)
    }
    function adjust(direction) {
        let config = Object.assign({}, layoutModel.composition)
        const category = categories[selected]
        let style = Object.assign({}, category ? category.presentation || {} : {})
        if (field === 0) enable()
        else if (field === 1) { config.columns = config.columns === 2 ? 1 : 2; if (config.schemaVersion) layoutModel.setComposition(config) }
        else if (field === 9) wallpaperPicker.open()
        else if (field === 10) { delete config.wallpaper; if (config.schemaVersion) layoutModel.setComposition(config) }
        else if (field === 11) { config.headerMode = config.headerMode === "identity" ? "navigation" : "identity"; if (config.schemaVersion) layoutModel.setComposition(config) }
        else if (field === 12) { config.showClock = !config.showClock; if (config.schemaVersion) layoutModel.setComposition(config) }
        else if (!category) return
        else if (field === 2) { style.variant = variants[(Math.max(0,variants.indexOf(style.variant || "source")) + direction + variants.length) % variants.length]; layoutModel.setSectionPresentation(category.id,style) }
        else if (field === 3) { style.columns = Math.max(1,Math.min(12,(style.columns || 1) + direction)); layoutModel.setSectionPresentation(category.id,style) }
        else if (field === 4) { style.height = Math.max(60,Math.min(800,(style.height || 180) + direction*10)); layoutModel.setSectionPresentation(category.id,style) }
        else if (field === 5 || field === 6) { const offset = field === 5 ? -1 : 1; if (layoutModel.moveCategory(category.id,offset)) selected = Math.max(0,Math.min(categories.length-1,selected+offset)) }
        else if (field === 7 || field === 8) { const key = field === 7 ? "panelOpacity" : "blur"; style[key] = Math.round(Math.max(0,Math.min(1,(style[key] || 0)+direction*0.1))*10)/10; layoutModel.setSectionPresentation(category.id,style) }
    }
    function handleCommand(command) {
        if (command === inputManager.back) { open = false; return }
        if (command === inputManager.navigateUp) field = Math.max(0,field-1)
        else if (command === inputManager.navigateDown) field = Math.min(rows.length-1,field+1)
        else if (command === inputManager.navigateLeft || command === inputManager.navigateRight) {
            if (field === 2) selected = Math.max(0,Math.min(categories.length-1,selected+(command === inputManager.navigateLeft ? -1 : 1)))
            else adjust(command === inputManager.navigateLeft ? -1 : 1)
        } else if (command === inputManager.confirm) { if (field === 13) open = false; else adjust(1) }
    }
    FileDialog { id:wallpaperPicker; title:"Choose local wallpaper"; nameFilters:["Images (*.png *.jpg *.jpeg)"]; onAccepted:root.layoutModel.setWallpaper(selectedFile.toString()) }
    MouseArea { anchors.fill: parent }
    Column {
        anchors.fill: parent; anchors.margins: 40; spacing: 12
        Text { text: "Component layout"; color: "#EEEAF0"; font.pixelSize: 32 }
        Text { width: parent.width; wrapMode: Text.WordWrap; text: "Up / Down selects a control · Left / Right selects a section or adjusts its values · OK cycles style · Changes save immediately"; color: "#AAA4B2"; font.pixelSize: 17 }
        Text { text: "Section: " + (root.categories[root.selected] ? root.categories[root.selected].name : "No sections"); color: "#BD8198"; font.pixelSize: 23 }
        ListView {
            width:parent.width; height:Math.max(100,parent.height-180); model:root.rows; currentIndex:root.field; spacing:8; clip:true; highlightRangeMode:ListView.ApplyRange
            delegate: Rectangle {
                required property int index; required property string modelData
                width: ListView.view.width; height: 46; radius: 8; color: root.field === index ? "#353039" : "#202126"
                border.width: root.field === index ? 2 : 0; border.color: "#BD8198"
                Text {
                    anchors.fill: parent; anchors.margins: 10; color: "#EEEAF0"; font.pixelSize: 17
                    readonly property var style: root.categories[root.selected] ? root.categories[root.selected].presentation || {} : ({})
                    text: modelData + (index === 0 ? ": " + (root.layoutModel.composition.enabled ? "On" : "Off")
                        : index === 1 ? ": " + (root.layoutModel.composition.columns || 1)
                        : index === 2 ? ": " + (style.variant || "source") : index === 3 ? ": " + (style.columns || 1)
                        : index === 4 ? ": " + (style.height || 180)
                        : index === 7 ? ": " + (style.panelOpacity || 0) : index === 8 ? ": " + (style.blur || 0)
                        : index === 11 ? ": " + (root.layoutModel.composition.headerMode || "navigation")
                        : index === 12 ? ": " + (root.layoutModel.composition.showClock ? "On" : "Off") : "")
                }
                MouseArea { anchors.fill: parent; onClicked: { root.field = index; root.handleCommand(inputManager.confirm) } }
            }
        }
        Text { text: root.layoutModel.errorString; color: "#FFB4B4"; font.pixelSize: 17 }
    }
}
