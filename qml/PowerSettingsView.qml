import QtQuick

Item {
    id: root
    property var appearance: ({})
    property var backend: typeof powerSettings!=="undefined"?powerSettings:null
    property int selectedIndex: 0
    property string pendingAction: ""
    readonly property var actions: ["refresh","suspend","reboot","poweroff","status"]
    readonly property var labels: [qsTr("Refresh power capabilities"),qsTr("Suspend"),qsTr("Restart"),qsTr("Shut down"),qsTr("Status")]
    SurfacePalette { id: shell; appearance:root.appearance }
    function back() { if(pendingAction.length){pendingAction="";return true}return false }
    function handleCommand(command) {
        if(command===inputManager.back){back();return}
        if(command===inputManager.navigateUp){pendingAction="";selectedIndex=Math.max(0,selectedIndex-1)}
        else if(command===inputManager.navigateDown){pendingAction="";selectedIndex=Math.min(actions.length-1,selectedIndex+1)}
        else if(command===inputManager.confirm && backend && !backend.busy){
            const action=actions[selectedIndex]
            if(action==="status")return
            if(action==="refresh"){pendingAction="";backend.refresh()}
            else if(pendingAction===action){pendingAction="";backend.request(action)}
            else pendingAction=action
        }
    }
    function revealSelection(){
        const row=powerRows.itemAt(selectedIndex)
        if(!row)return
        if(row.y<scroll.contentY)scroll.contentY=row.y
        else if(row.y+row.height>scroll.contentY+scroll.height)scroll.contentY=row.y+row.height-scroll.height
    }
    onSelectedIndexChanged:Qt.callLater(revealSelection)
    onVisibleChanged: { pendingAction="";if(visible && backend)backend.refresh() }
    Rectangle { anchors.fill:parent;color:shell.surface }
    Flickable {
    id:scroll;anchors.fill:parent;contentHeight:contents.implicitHeight;clip:true;boundsBehavior:Flickable.StopAtBounds
    Column {
        id:contents;width:parent.width;spacing:18*shell.gapScale
        Text { text:qsTr("POWER");font.family:shell.fontFamily;font.pixelSize:30*shell.textScale;color:shell.text }
        Text { width:parent.width;wrapMode:Text.WordWrap;text:qsTr("Confirm an action twice to apply it. Back cancels the confirmation. Save your work before restarting or shutting down.");font.family:shell.fontFamily;font.pixelSize:18*shell.textScale;color:shell.muted }
        Repeater {
            id:powerRows
            model:root.actions
            delegate:Rectangle {
                required property int index
                required property string modelData
                width:parent.width;height:Math.max(74*shell.textScale,powerRowLabel.implicitHeight+32);radius:shell.radius;color:root.selectedIndex===index?shell.selected:shell.surface
                border.width:root.selectedIndex===index?2:0;border.color:shell.selectedFocus
                Text { id:powerRowLabel;anchors.fill:parent;anchors.margins:16;verticalAlignment:Text.AlignVCenter;wrapMode:Text.WordWrap;textFormat:Text.PlainText;font.family:shell.fontFamily;font.pixelSize:21*shell.textScale;color:root.selectedIndex===index?shell.selectedText:shell.text
                    text:modelData==="status"?(root.backend?root.backend.status:qsTr("System power service unavailable.")):(root.pendingAction===modelData?qsTr("CONFIRM · "):"")+root.labels[index]+(index && root.backend?" · "+(root.backend.capabilities[modelData] || qsTr("checking")):"") }
                MouseArea { anchors.fill:parent;onClicked:{root.selectedIndex=index;root.handleCommand(inputManager.confirm)} }
            }
        }
    }
    }
}
