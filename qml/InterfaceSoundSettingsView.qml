import QtQuick

Item {
    id: root
    property var appearance: ({})
    property var controller: typeof interfaceSounds !== "undefined" ? interfaceSounds : null
    property int selectedIndex: 0
    property string draftTheme: ""
    readonly property var choices: controller ? controller.themes : []
    readonly property var rows: controller ? controller.fields : []
    readonly property string candidateName: {
        const candidate = choices.find(theme => theme.id === draftTheme)
        return candidate ? candidate.name : controller ? controller.themeName : ""
    }
    SurfacePalette { id: shell; appearance: root.appearance }
    Rectangle { anchors.fill:parent; color:shell.surface }
    onVisibleChanged: {
        if (!controller) return
        if (visible) draftTheme=controller.theme
        else controller.stop()
    }
    function valueFor(key) {
        if (!controller) return ""
        if (key === "enabled") return controller.enabled ? qsTr("On") : qsTr("Off")
        if (key === "theme") return candidateName + (draftTheme !== controller.theme ? qsTr(" · Preview choice") : "")
        if (key === "volume") return controller.volume + "%"
        if (key === "duringPlayback") return controller.duringPlayback ? qsTr("On") : qsTr("Off")
        if (key === "preview") return controller.previewing ? qsTr("Stop") : qsTr("Play")
        return qsTr("Scan")
    }
    function chooseTheme(direction) {
        if (!choices.length) return
        controller.stop()
        const index=choices.findIndex(theme => theme.id === draftTheme)
        const next=index<0 ? 0 : (index+direction+choices.length)%choices.length
        draftTheme=choices[next].id
    }
    function handleCommand(command) {
        if (!controller) return
        if (command === inputManager.navigateUp) selectedIndex=Math.max(0,selectedIndex-1)
        else if (command === inputManager.navigateDown) selectedIndex=Math.min(rows.length-1,selectedIndex+1)
        else if (rows[selectedIndex]) {
            const key=rows[selectedIndex].key
            if (command === inputManager.navigateLeft || command === inputManager.navigateRight) {
                const direction=command===inputManager.navigateRight?1:-1
                if (key === "theme") chooseTheme(direction)
                else if (key === "volume") {
                    const next=controller.volume+direction*5
                    if (next>=0 && next<=100) controller.setVolume(next)
                }
            } else if (command === inputManager.confirm) {
                if (key === "enabled") controller.setEnabled(!controller.enabled)
                else if (key === "theme") controller.selectTheme(draftTheme)
                else if (key === "duringPlayback") controller.setDuringPlayback(!controller.duringPlayback)
                else if (key === "preview") {
                    if (controller.previewing) controller.stop()
                    else controller.preview(draftTheme)
                } else if (key === "refresh") controller.refreshThemes()
            }
        }
    }
    function revealSelection() {
        const row=items.itemAt(selectedIndex)
        if (!row) return
        if (row.y<scroll.contentY) scroll.contentY=row.y
        else if (row.y+row.height>scroll.contentY+scroll.height) scroll.contentY=Math.max(0,row.y+row.height-scroll.height)
        scroll.contentY=Math.max(0,Math.min(Math.max(0,scroll.contentHeight-scroll.height),scroll.contentY))
    }
    onSelectedIndexChanged: Qt.callLater(revealSelection)
    onHeightChanged: Qt.callLater(revealSelection)
    Flickable {
        id:scroll
        anchors.fill:parent;clip:true
        contentHeight:content.implicitHeight
        boundsBehavior:Flickable.StopAtBounds
        Column {
            id:content;width:parent.width;spacing:18*shell.gapScale
            Text {textFormat:Text.PlainText;width:parent.width;wrapMode:Text.Wrap;text:qsTr("Interface sounds");font.family:shell.fontFamily;font.pixelSize:30*shell.textScale;font.weight:Font.DemiBold;color:shell.text}
            Text {textFormat:Text.PlainText;width:parent.width;wrapMode:Text.Wrap;text:qsTr("Choose how Isolde responds to your remote. Kodi sound packs are read without changing Kodi’s settings.");font.family:shell.fontFamily;font.pixelSize:19*shell.textScale;color:shell.muted}
            Repeater {
                id:items;model:root.rows
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    readonly property bool selected:root.selectedIndex===index
                    objectName:"interfaceSoundRow-"+modelData.key
                    width:parent.width;height:Math.max(88*shell.textScale,labels.implicitHeight+32)
                    radius:shell.radius;color:selected?shell.selected:shell.surface
                    border.width:selected?3:1;border.color:selected?shell.selectedFocus:shell.outline
                    Column {
                        id:labels
                        anchors.left:parent.left;anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;anchors.margins:16
                        spacing:6*shell.gapScale
                        Text {textFormat:Text.PlainText;width:parent.width;wrapMode:Text.Wrap;text:modelData.label+" · "+root.valueFor(modelData.key);font.family:shell.fontFamily;font.pixelSize:22*shell.textScale;font.weight:Font.DemiBold;color:selected?shell.selectedText:shell.text}
                        Text {textFormat:Text.PlainText;width:parent.width;wrapMode:Text.Wrap;text:modelData.help;font.family:shell.fontFamily;font.pixelSize:17*shell.textScale;color:selected?shell.selectedText:shell.muted}
                    }
                    MouseArea {anchors.fill:parent;onClicked:{root.selectedIndex=index;root.handleCommand(inputManager.confirm)}}
                }
            }
            Text {textFormat:Text.PlainText;width:parent.width;wrapMode:Text.Wrap;text:root.controller?root.controller.errorString:qsTr("Interface sound settings are unavailable.");visible:text.length>0;font.family:shell.fontFamily;font.pixelSize:19*shell.textScale;color:shell.text}
        }
    }
}
