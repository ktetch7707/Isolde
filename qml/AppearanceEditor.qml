import QtQuick

Rectangle {
    id: root
    objectName: "appearanceEditor"
    required property var layoutModel
    property bool open: false
    property var draft: ({})
    property int selectedIndex: 0
    property bool typing: false
    property string errorText: ""
    signal closed()
    readonly property var fields: [
        { key: "focusColor", label: "Selection accent", color: true },
        { key: "backgroundStart", label: "Background top", color: true },
        { key: "backgroundEnd", label: "Background bottom", color: true },
        { key: "surfaceColor", label: "Tile surface", color: true },
        { key: "selectedColor", label: "Selected surface", color: true },
        { key: "textColor", label: "Text", color: true },
        { key: "mutedColor", label: "Secondary text", color: true },
        { key: "typographyScale", label: "Text size", min: 0.8, max: 1.3, step: 0.05 },
        { key: "homeColumns", label: "Home columns", min: 2, max: 6, step: 1 },
        { key: "spacing", label: "Spacing", min: 8, max: 40, step: 2 },
        { key: "tileRoundness", label: "Tile roundness", min: 0, max: 1, step: 0.05 },
        { key: "motionEnabled", label: "Focus animation", toggle: true },
        { key: "apply", label: "Apply appearance" }, { key: "cancel", label: "Cancel" },
        { key: "fontFamily", label: "Font family", color: true }
    ]
    visible: open; color: "#101014"
    function begin() {
        draft = Object.assign({}, layoutModel.appearance)
        selectedIndex = 0; typing = false; errorText = ""; open = true
    }
    function close() { open = false; typing = false; closed() }
    function setValue(key, value) { let next = Object.assign({}, draft); next[key] = value; draft = next; errorText = "" }
    function apply() { if (layoutModel.setAppearance(draft)) close(); else errorText = layoutModel.errorString }
    function handleCommand(command) {
        if (typing) { keyboard.handleCommand(command); return }
        if (command === inputManager.back) { close(); return }
        if (command === inputManager.navigateUp) selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown) selectedIndex = Math.min(fields.length - 1, selectedIndex + 1)
        else {
            const field = fields[selectedIndex]
            if ((command === inputManager.navigateLeft || command === inputManager.navigateRight) && field.step) {
                const direction = command === inputManager.navigateLeft ? -1 : 1
                setValue(field.key, Math.round(Math.max(field.min, Math.min(field.max, draft[field.key] + direction * field.step)) * 100) / 100)
            } else if (command === inputManager.confirm) {
                if (field.key === "apply") apply()
                else if (field.key === "cancel") close()
                else if (field.toggle) setValue(field.key, !draft[field.key])
                else if (field.color) { keyboard.value = draft[field.key]; keyboard.selectedIndex = 0; typing = true }
            }
        }
    }
    MouseArea { anchors.fill: parent }
    Column {
        anchors.fill: parent; anchors.margins: 36; spacing: 14
        Text { text: "Make it yours"; color: "#F5F1F3"; font.pixelSize: 34 }
        Text { text: "Preview your look. Apply saves to this profile · Back cancels"; color: "#AAA5AF"; font.pixelSize: 17 }
        Row {
            width: parent.width; height: parent.height - 110; spacing: 28
            ListView {
                id: options; objectName: "appearanceOptions"
                width: parent.width * 0.43; height: parent.height; spacing: 6; clip: true
                model: root.fields; currentIndex: root.selectedIndex; highlightRangeMode: ListView.ApplyRange
                delegate: Rectangle {
                    required property int index; required property var modelData
                    width: options.width; height: 46; radius: 8
                    color: index === root.selectedIndex ? "#353039" : "#202126"
                    border.width: index === root.selectedIndex ? 2 : 0; border.color: "#C994A5"
                    Text {
                        anchors.fill: parent; anchors.margins: 10; color: "#F5F1F3"; font.pixelSize: 16; elide: Text.ElideRight
                        text: modelData.label + (modelData.color ? "  " + root.draft[modelData.key]
                            : modelData.toggle ? root.draft[modelData.key] ? "  On" : "  Off"
                            : modelData.step ? "  " + root.draft[modelData.key] : "")
                    }
                    MouseArea { anchors.fill: parent; onClicked: { root.selectedIndex = index; root.handleCommand(inputManager.confirm) } }
                }
            }
            Rectangle {
                width: parent.width - options.width - parent.spacing; height: parent.height; radius: 18; clip: true
                gradient: Gradient {
                    GradientStop { position: 0; color: root.draft.backgroundStart || "#19191E" }
                    GradientStop { position: 1; color: root.draft.backgroundEnd || "#101014" }
                }
                Column {
                    anchors.fill: parent; anchors.margins: 24; spacing: root.draft.spacing || 20
                    Text { text: "YOUR SPACE"; font.letterSpacing: 3; color: root.draft.mutedColor || "#AAA5AF"; font.pixelSize: 14 }
                    Text { text: "An evening at home"; color: root.draft.textColor || "#F5F1F3"; font.pixelSize: 27 * (root.draft.typographyScale || 1); width: parent.width; elide: Text.ElideRight }
                    AppTile {
                        objectName: "appearancePreviewTile"
                        width: parent.width; height: 170; app: ({ name: "Your favorite", section: "PERSONAL", accent: "#886C7A", symbol: "✦" })
                        selected: true; roundness: root.draft.tileRoundness || 0; appearance: root.draft
                    }
                    Text { width: parent.width; wrapMode: Text.WordWrap; text: "← → adjust numbers · OK edits a color or toggles animation. Colors accept any #RRGGBB value."; color: root.draft.mutedColor || "#AAA5AF"; font.pixelSize: 16 }
                    Text { width: parent.width; wrapMode: Text.WordWrap; text: root.errorText; color: "#FFB4B4"; font.pixelSize: 16 }
                }
            }
        }
    }
    Rectangle {
        anchors.fill: parent; color: "#101014"; visible: root.typing; z: 5
        Text { x: 36; y: 25; text: root.fields[root.selectedIndex].key === "fontFamily" ? "FONT FAMILY" : "COLOR · #RRGGBB"; color: "#F5F1F3"; font.pixelSize: 28 }
        OnScreenKeyboard {
            id: keyboard; objectName: "appearanceKeyboard"
            x: 36; y: 78; width: parent.width - 72; height: Math.min(420, parent.height - 120)
            onCanceled: root.typing = false
            onAccepted: text => {
                if (root.fields[root.selectedIndex].key !== "fontFamily" && !/^#[0-9a-fA-F]{6}$/.test(text.trim())) { root.errorText = "Enter a color such as #C994A5"; return }
                root.setValue(root.fields[root.selectedIndex].key, text.trim()); root.typing = false
            }
        }
        Text { x: 36; anchors.bottom: parent.bottom; anchors.bottomMargin: 20; text: root.errorText; color: "#FFB4B4"; font.pixelSize: 18 }
    }
}
