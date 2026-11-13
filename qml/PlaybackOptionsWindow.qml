import QtQuick
import QtQuick.Window

Window {
    id:root
    required property var kodiClient
    property var appearance:({})
    property real uiScale:1
    property real safeInset:0
    property string mode:"controls"
    property int selectedIndex:0
    property int controlOriginIndex:0
    property bool restoreVideo:true
    property bool waitingAdjacent:false
    transientParent:null
    flags:Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color:"transparent"
    visible:false
    title:qsTr("Isolde playback controls")
    width:Screen.width;height:Screen.height;x:Screen.virtualX;y:Screen.virtualY
    SurfacePalette { id:shell;appearance:root.appearance }
    readonly property var options:kodiClient.playbackOptions || ({})
    readonly property var rows:{
        if(waitingAdjacent)return [{label:qsTr("Opening the adjacent title…"),kind:"none"}]
        if(mode==="audio" || mode==="subtitles"){
            const tracks=(options[mode] || []).map(t=>({label:(t.selected?"● ":"")+(t.language?t.language.toUpperCase()+" · ":"")+(t.name || qsTr("Track ")+t.id),kind:mode==="audio"?"audio":"subtitle",value:t.id}))
            return mode==="subtitles"?[{label:qsTr("Off"),kind:"subtitle",value:"off"}].concat(tracks):tracks.concat(tracks.length?[]:[{label:qsTr("No audio tracks reported"),kind:"none"}])
        }
        if(mode==="view")return (options.views || []).map(value=>({label:(options.view===value?"● ":"")+({normal:qsTr("Normal"),zoom:qsTr("Zoom"),"stretch4:3":qsTr("Stretch 4:3"),widezoom:qsTr("Wide zoom"),"stretch16:9":qsTr("Stretch 16:9"),original:qsTr("Original size")}[value] || value),kind:"view",value:value}))
        let controls=[{label:qsTr("Pause / resume"),control:"pause"},{label:qsTr("Seek backward"),control:"backward"},{label:qsTr("Seek forward"),control:"forward"}]
        if(kodiClient.playbackCanPrevious)controls.push({label:qsTr("Previous title"),adjacent:-1})
        if(kodiClient.playbackCanNext)controls.push({label:qsTr("Next title"),adjacent:1})
        if(kodiClient.playbackMediaType!=="song")controls=controls.concat([{label:qsTr("Audio tracks"),page:"audio"},{label:qsTr("Subtitles"),page:"subtitles"},{label:qsTr("Picture size"),page:"view"}])
        return controls.concat([{label:qsTr("Refresh options"),refresh:true},{label:qsTr("Return to playback"),close:true},{label:qsTr("Stop and return"),control:"stop"}])
    }
    function showControls(){waitingAdjacent=false;restoreVideo=true;mode="controls";selectedIndex=0;show();raise();requestActivate();body.forceActiveFocus();refreshTimer.start()}
    function closeControls(restore){restoreVideo=restore;hide()}
    function back(){if(waitingAdjacent){waitingAdjacent=false;kodiClient.playbackControl("stop");closeControls(false)}else if(mode!=="controls"){mode="controls";selectedIndex=Math.min(controlOriginIndex,rows.length-1)}else hide()}
    function handleCommand(command){
        if(command===inputManager.home){closeControls(false);kodiClient.playbackControl("home");return}
        if(command===inputManager.menu){if(waitingAdjacent)back();else hide();return}
        if(command===inputManager.back){back();return}
        if(command===inputManager.navigateUp)selectedIndex=Math.max(0,selectedIndex-1)
        else if(command===inputManager.navigateDown)selectedIndex=Math.min(rows.length-1,selectedIndex+1)
        else if(command===inputManager.confirm && !kodiClient.busy){
            const row=rows[selectedIndex];if(!row)return
            if(row.page){controlOriginIndex=selectedIndex;mode=row.page;selectedIndex=0;refreshTimer.start()}
            else if(row.adjacent){waitingAdjacent=true;if(kodiClient.playAdjacent(row.adjacent)){selectedIndex=0;refreshTimer.stop()}else waitingAdjacent=false}
            else if(row.refresh)refreshTimer.start()
            else if(row.close)hide()
            else if(row.control){kodiClient.playbackControl(row.control);if(row.control==="stop")closeControls(false)}
            else if(row.kind && row.kind!=="none")kodiClient.setPlaybackOption(row.kind,row.value)
        }
    }
    onSelectedIndexChanged:list.positionViewAtIndex(selectedIndex,ListView.Contain)
    onVisibleChanged:if(!visible){refreshTimer.stop();if(restoreVideo && kodiClient.playbackActive)kodiClient.requestVideoPresentation()}
    Connections {
        target:root.kodiClient
        function onPlaybackOptionsRequested(){root.showControls()}
        function onPlaybackStarted(){if(root.waitingAdjacent){root.waitingAdjacent=false;root.closeControls(false)}}
        function onPlaybackReturned(){root.waitingAdjacent=false;root.closeControls(false)}
        function onChanged(){if(!root.waitingAdjacent && !root.kodiClient.playbackActive)root.closeControls(false);root.selectedIndex=Math.min(root.selectedIndex,Math.max(0,root.rows.length-1))}
    }
    Timer {id:refreshTimer;interval:100;onTriggered:{if(root.visible && root.kodiClient.playbackActive){if(root.kodiClient.busy)restart();else root.kodiClient.refreshPlaybackOptions()}}}
    Rectangle {anchors.fill:parent;color:shell.scrim}
    Item {
        id:body;x:root.safeInset;y:root.safeInset
        width:Math.max(1,(root.width-2*root.safeInset)/root.uiScale);height:Math.max(1,(root.height-2*root.safeInset)/root.uiScale)
        scale:root.uiScale;transformOrigin:Item.TopLeft;focus:true
        Keys.onPressed:event=>{const command=inputManager.commandForKey(event.key);if(command!==inputManager.none){root.handleCommand(command);event.accepted=true}}
        Rectangle {
            anchors.centerIn:parent;width:Math.min(parent.width-64,800*shell.textScale);height:parent.height-64;color:shell.surface;radius:shell.radius
            Column {
                anchors.fill:parent;anchors.margins:24;spacing:16*shell.gapScale
                Text {id:heading;width:parent.width;wrapMode:Text.WordWrap;text:root.mode==="controls"?qsTr("PLAYBACK"):root.mode==="audio"?qsTr("AUDIO TRACKS"):root.mode==="subtitles"?qsTr("SUBTITLES"):qsTr("PICTURE SIZE");font.family:shell.fontFamily;font.pixelSize:28*shell.textScale;color:shell.text}
                Text {id:status;width:parent.width;wrapMode:Text.WordWrap;textFormat:Text.PlainText;text:[root.kodiClient.playbackStatus,root.kodiClient.playbackOptionsStatus].filter(v=>v).join(" · ");font.family:shell.fontFamily;font.pixelSize:17*shell.textScale;color:shell.muted}
                ListView {
                    id:list;width:parent.width;height:Math.max(0,parent.height-heading.height-status.height-help.height-3*parent.spacing);model:root.rows;clip:true;currentIndex:root.selectedIndex;spacing:8*shell.gapScale
                    delegate:Rectangle {
                        required property int index;required property var modelData
                        width:list.width;height:Math.max(58*shell.textScale,label.implicitHeight+24);radius:shell.radius;color:index===root.selectedIndex?shell.selected:shell.surface
                        border.width:index===root.selectedIndex?2:0;border.color:shell.selectedFocus
                        Text {id:label;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12;wrapMode:Text.WordWrap;textFormat:Text.PlainText;text:modelData.label;color:index===root.selectedIndex?shell.selectedText:shell.text;font.family:shell.fontFamily;font.pixelSize:21*shell.textScale}
                        MouseArea {anchors.fill:parent;onClicked:{root.selectedIndex=index;root.handleCommand(inputManager.confirm)}}
                    }
                }
                Text {id:help;width:parent.width;wrapMode:Text.WordWrap;text:qsTr("Up/Down chooses · OK applies · Back returns · Menu closes controls");font.family:shell.fontFamily;font.pixelSize:17*shell.textScale;color:shell.muted}
            }
        }
    }
}
