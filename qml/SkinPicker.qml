import QtQuick

Rectangle {
    id: root
    objectName: "skinPicker"
    required property var layoutModel
    property var ambientModel: null
    property var appearance: ({})
    property bool open: false
    property int selected: 0
    property int operation: 0
    property string mode: "list"
    property string message: ""
    property var previewModel: null
    property var previewAmbient: null
    readonly property bool detailsVisible: mode==="actions" || mode==="delete"
    readonly property bool screensaverPreview: detailsVisible && actions[operation]?.id==="screensaver"
    readonly property var previewHome: previewModel
        ? (previewModel.homePageId ? previewModel.pageModel(previewModel.homePageId) || previewModel : previewModel)
        : null
    onVisibleChanged: if (!visible && (previewModel || previewAmbient)) {
        clearPreview()
        mode = "list"
    }
    signal closed()
    signal editSkin(string id)
    signal screensaverRequested(string id)
    signal duplicated(string sourceId, string copyId)

    SurfacePalette { id: skinTheme; appearance: root.appearance }
    color: skinTheme.background
    readonly property var entries: (layoutModel.skins || []).concat([{id:"",name:qsTr("Add new skin")}])
    readonly property var choice: entries[selected] || ({})
    readonly property var actions: (choice.id === layoutModel.activeSkinId ? [] : [{id:"activate",name:qsTr("Use this skin")}])
        .concat([{id:"edit",name:qsTr("Edit Home and pages")},{id:"screensaver",name:qsTr("Screensaver")},
            {id:"duplicate",name:qsTr("Duplicate skin")}], choice.builtIn || choice.id===layoutModel.activeSkinId
                ? [] : [{id:"delete",name:qsTr("Delete skin")}])

    // Open at a stable skin identity so returning from Studio restores focus.
    function begin(id) {
        clearPreview()
        selected=Math.max(0,entries.findIndex(entry=>entry.id===(id || layoutModel.activeSkinId)))
        operation=0;mode="list";message="";open=true
    }
    // Select a skin or open the naming step for a new skin.
    function selectEntry() {
        message="";operation=0
        if(!choice.id){keyboard.value=qsTr("New skin");keyboard.selectedIndex=0;mode="create"}
        else {
            previewModel=layoutModel.openSkinPreview(choice.id)
            previewAmbient=ambientModel ? ambientModel.skinPreview(choice.id) : null
            mode="actions"
        }
    }
    // Release the preview before moving out of the selected skin.
    function clearPreview(){
        previewModel=null
        previewAmbient=null
        layoutModel.closeSkinPreview()
    }
    // Dispatch a chosen action only after selecting a skin from the list.
    function execute() {
        const action=actions[operation]
        if(!action)return
        message=""
        if(action.id==="activate") {
            if(layoutModel.activateSkin(choice.id)){clearPreview();mode="list";message=qsTr("Skin is active.")}
            else message=layoutModel.errorString
        } else if(action.id==="edit") editSkin(choice.id)
        else if(action.id==="screensaver") screensaverRequested(choice.id)
        else if(action.id==="duplicate"){keyboard.value=choice.name+qsTr(" copy");keyboard.selectedIndex=0;mode="duplicate"}
        else mode="delete"
    }
    // Add or copy a skin without activating it, then keep it selected.
    function finishName(name) {
        const creating=mode==="create", origin=choice.id
        const id=creating?layoutModel.createSkin(name):layoutModel.duplicateSkin(origin,name)
        if(!id){message=layoutModel.errorString;return}
        if(!creating)duplicated(origin,id)
        begin(id)
    }
    // Keep all list, detail and naming actions reachable from the remote.
    function handleCommand(command) {
        if(mode==="create" || mode==="duplicate"){keyboard.handleCommand(command);return}
        if(command===inputManager.back || (command===inputManager.navigateLeft && detailsVisible)){
            if(mode==="list"){open=false;closed()}else mode=mode==="delete"?"actions":"list"
            if(mode==="list")clearPreview()
            return
        }
        if(command===inputManager.confirm || command===inputManager.navigateRight){
            if(mode==="list")selectEntry()
            else if(mode==="actions")execute()
            else if(mode==="delete" && command===inputManager.confirm){
                if(layoutModel.deleteSkin(choice.id))begin()
                else message=layoutModel.errorString
            }
            return
        }
        const change=command===inputManager.navigateUp?-1:command===inputManager.navigateDown?1:0
        if(mode==="list")selected=Math.max(0,Math.min(entries.length-1,selected+change))
        else if(mode==="actions")operation=Math.max(0,Math.min(actions.length-1,operation+change))
    }

    MouseArea { anchors.fill:parent }
    Row {
        anchors.fill:parent;anchors.margins:40;spacing:24*skinTheme.gapScale
        Column {
        width:Math.min(parent.width*0.36,360*skinTheme.textScale);height:parent.height;spacing:14
        Text {text:qsTr("Skins");color:skinTheme.backgroundText;font.family:skinTheme.fontFamily;font.pixelSize:Math.round(34*skinTheme.textScale)}
        ListView {
            id:rows
            width:parent.width;height:Math.max(100,parent.height-y-42);spacing:8;clip:true
            visible:root.mode==="list" || root.detailsVisible
            model:root.entries
            currentIndex:root.selected
            highlightRangeMode:ListView.ApplyRange;highlightMoveDuration:0
            delegate:Rectangle {
                required property int index
                required property var modelData
                width:ListView.view.width;height:Math.max(54,64*skinTheme.textScale);radius:skinTheme.radius
                readonly property bool focused:index===rows.currentIndex
                color:focused?skinTheme.selected:skinTheme.surface
                border.width:focused && root.mode==="list"?2:0;border.color:skinTheme.selectedFocus
                Text {
                    anchors.left:parent.left;anchors.leftMargin:16;anchors.verticalCenter:parent.verticalCenter
                    width:parent.width-32;elide:Text.ElideRight;textFormat:Text.PlainText
                    text:modelData.name+(modelData.id===root.layoutModel.activeSkinId?qsTr(" · Active"):"")
                    color:parent.focused?skinTheme.selectedText:skinTheme.text;font.family:skinTheme.fontFamily;font.pixelSize:Math.round(22*skinTheme.textScale)
                }
                MouseArea {
                    anchors.fill:parent
                    onClicked:{root.selected=index;root.selectEntry()}
                }
            }
        }
        }
        Column {
            width:Math.max(0,parent.width-x);height:parent.height;spacing:10*skinTheme.gapScale
            visible:root.detailsVisible || root.message.length>0
            Text {
                width:parent.width;visible:root.detailsVisible;elide:Text.ElideRight;textFormat:Text.PlainText
                text:root.choice.name || "";font.family:skinTheme.fontFamily;font.weight:Font.DemiBold
                color:skinTheme.backgroundText;font.pixelSize:28*skinTheme.textScale
            }
            Item {
                id:previewFrame
                objectName:"skinPreviewFrame"
                readonly property var viewport:root.layoutModel.studioOptions.referenceViewport || ({})
                readonly property real previewWidth:viewport.width || 1280
                readonly property real previewHeight:viewport.height || 720
                visible:root.detailsVisible && !!root.previewHome
                width:Math.min(parent.width,parent.height*0.46*previewWidth/previewHeight)
                height:width*previewHeight/previewWidth;clip:true
                Item {
                    width:previewFrame.previewWidth;height:previewFrame.previewHeight
                    scale:previewFrame.width/width;transformOrigin:Item.TopLeft;enabled:false
                    Loader {
                        anchors.fill:parent;active:root.detailsVisible && !!root.previewHome
                        sourceComponent:root.screensaverPreview && root.previewAmbient?screensaverSample:homeSample
                    }
                    Component {
                        id:homeSample
                        Screensaver { pageModel:root.previewHome;artworkResolver:root.previewModel;reducedMotion:true }
                    }
                    Component {
                        id:screensaverSample
                        AmbientScreen {
                            settings:root.previewAmbient
                            layoutModel:root.previewModel
                            active:!!root.previewAmbient && !!root.previewModel
                            staticPreview:true
                        }
                    }
                }
            }
            Text {
                visible:root.detailsVisible
                text:!root.previewHome ? qsTr("Preview unavailable") : root.screensaverPreview ? qsTr("Screensaver preview") : qsTr("Home preview")
                color:skinTheme.backgroundMuted;font.family:skinTheme.fontFamily;font.pixelSize:16*skinTheme.textScale
            }
            ListView {
                id:actionRows
                visible:root.mode==="actions"
                width:parent.width;height:Math.max(0,parent.height-y-44*skinTheme.textScale)
                clip:true;spacing:4*skinTheme.gapScale;model:root.actions;currentIndex:root.operation
                highlightRangeMode:ListView.ApplyRange;highlightMoveDuration:0
                delegate:Rectangle {
                    required property int index
                    required property var modelData
                    readonly property bool focused:index===root.operation
                    width:actionRows.width;height:Math.max(42,46*skinTheme.textScale)
                    radius:skinTheme.radius;color:focused?skinTheme.selected:skinTheme.surface
                    border.width:focused?2:0;border.color:skinTheme.selectedFocus
                    Text {
                        anchors.fill:parent;anchors.margins:14;verticalAlignment:Text.AlignVCenter
                        text:modelData.name;textFormat:Text.PlainText;elide:Text.ElideRight
                        color:parent.focused?skinTheme.selectedText:skinTheme.text;font.family:skinTheme.fontFamily
                        font.pixelSize:20*skinTheme.textScale
                    }
                    MouseArea {
                        anchors.fill:parent;hoverEnabled:true
                        onEntered:root.operation=index
                        onClicked:{root.operation=index;root.execute()}
                    }
                }
            }
            Text {
                visible:text.length>0;width:parent.width;wrapMode:Text.WordWrap;textFormat:Text.PlainText
                color:skinTheme.backgroundText;font.family:skinTheme.fontFamily;font.pixelSize:18*skinTheme.textScale
                text:root.message || (root.mode==="delete"?qsTr("Delete this skin? Confirm deletes it. Back cancels."):"")
            }
        }
    }
    OnScreenKeyboard {
        id:keyboard
        anchors.fill:parent;anchors.margins:40
        visible:root.mode==="create" || root.mode==="duplicate"
        onAccepted:text=>root.finishName(text)
        onCanceled:root.mode=root.mode==="create"?"list":"actions"
    }
}
