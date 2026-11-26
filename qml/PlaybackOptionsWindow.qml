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
    property bool pendingConfirm:false
    property int seekTarget:-1
    property int seekDirection:0
    property int seekRepeats:0
    transientParent:null
    flags:Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color:"transparent"
    visible:false
    title:qsTr("Isolde playback controls")
    width:Screen.width;height:Screen.height;x:Screen.virtualX;y:Screen.virtualY
    SurfacePalette { id:shell;appearance:root.appearance }
    readonly property var options:kodiClient.playbackOptions || ({})
    readonly property var timeline:kodiClient.playbackTimeline || ({})
    readonly property int duration:timeline.duration || 0
    readonly property int position:seekTarget >= 0 ? seekTarget : (timeline.position || 0)
    readonly property bool scrubbing:seekTarget >= 0
    readonly property bool canScrub:kodiClient.playbackCanSeek && duration > 1
    // Keep Play centered even when the current title has no neighbors.
    readonly property var transportSlots:{"view":-3,"audio":-2,"previous":-1,"pause":0,"next":1,"subtitles":2,"info":3,"stop":4}
    readonly property var rows:{
        if(waitingAdjacent)return [{label:qsTr("Opening the adjacent title…"),kind:"none"}]
        if(mode==="info")return []
        if(mode==="audio" || mode==="subtitles"){
            const tracks=(options[mode] || []).map(t=>({label:trackLabel(t),kind:mode==="audio"?"audio":"subtitle",value:t.id}))
            return mode==="subtitles"?[{label:options.subtitleEnabled?qsTr("Off"):qsTr("● Off"),kind:"subtitle",value:"off"}].concat(tracks):tracks.concat(tracks.length?[]:[{label:qsTr("No audio tracks reported"),kind:"none"}])
        }
        if(mode==="view")return (options.views || []).map(value=>({label:(options.view===value?"● ":"")+({normal:qsTr("Normal"),zoom:qsTr("Zoom"),"stretch4:3":qsTr("Stretch 4:3"),widezoom:qsTr("Wide zoom"),"stretch16:9":qsTr("Stretch 16:9"),original:qsTr("Original size")}[value] || value),kind:"view",value:value}))
        if(mode==="audiooffset")return [{label:qsTr("−"),kind:"offset",value:-250},{label:qsTr("+"),kind:"offset",value:250},{label:qsTr("Audio offset: %1 ms").arg(kodiClient.audioOffset>=0?"+":"")+kodiClient.audioOffset,kind:"none"}]
        if(mode==="speed")return [{label:"0.5×",kind:"speed",value:0.5},{label:"1.0×",kind:"speed",value:1.0},{label:"1.5×",kind:"speed",value:1.5},{label:"2.0×",kind:"speed",value:2.0},{label:qsTr("Speed: %1×").arg(kodiClient.playbackSpeed),kind:"none"}]
        if(mode==="subtitleoffset")return [{label:qsTr("−"),kind:"subtitleoffset",value:-250},{label:qsTr("+"),kind:"subtitleoffset",value:250},{label:qsTr("Subtitle delay: %1 ms").arg(kodiClient.subtitleDelay>=0?"+":"")+kodiClient.subtitleDelay,kind:"none"}]
        if(mode==="chapters")return kodiClient.chapters().length>0 ? kodiClient.chapters().map(ch=>({label:(ch.selected?"● ":"")+ch.name,kind:"chapter",value:ch.index})):[{label:qsTr("No chapters"),kind:"none"}]
        const paused=timeline.state==="paused"
        let controls=[{label:paused?qsTr("Resume"):qsTr("Pause"),symbol:paused?"▶":"Ⅱ",control:"pause"}]
        if(kodiClient.playbackCanPrevious)controls.push({label:qsTr("Previous title"),symbol:"⏮︎",adjacent:-1})
        if(kodiClient.playbackCanNext)controls.push({label:qsTr("Next title"),symbol:"⏭︎",adjacent:1})
        controls.push({label:qsTr("Information"),symbol:"ⓘ",page:"info"})
        if(kodiClient.playbackMediaType!=="song")controls=controls.concat([{label:qsTr("Audio tracks"),symbol:"♪",page:"audio"},{label:qsTr("Audio offset"),symbol:"±",page:"audiooffset"},{label:qsTr("Playback speed"),symbol:"⏩",page:"speed"},{label:qsTr("Subtitle delay"),symbol:"◐",page:"subtitleoffset"},{label:qsTr("Chapters"),symbol:"📑",page:"chapters"},{label:qsTr("Subtitles"),symbol:"CC",page:"subtitles"},{label:qsTr("Picture size"),symbol:"▣",page:"view"}])
        return controls.concat([{label:qsTr("Stop and return"),symbol:"■",control:"stop"}])
    }
    // Use track metadata when available while keeping unlabeled tracks selectable.
    function trackLabel(track){
        const parts=[track.language ? track.language.toUpperCase() : "",track.name || ""]
        if(!parts.some(value=>value))parts.push(qsTr("Track %1").arg(track.id))
        if(track.channels)parts.push(track.channels===2?qsTr("Stereo"):track.channels===6?"5.1":track.channels===8?"7.1":qsTr("%1 channels").arg(track.channels))
        if(track.forced)parts.push(qsTr("Forced"))
        return (track.selected?"● ":"")+parts.filter(value=>value).join(" · ")
    }
    // Format the current or proposed timestamp at TV distance.
    function stamp(seconds) {
        const hours=Math.floor(seconds/3600)
        const minutes=Math.floor(seconds/60)%60
        const tail=String(seconds%60).padStart(2,"0")
        return hours ? hours+":"+String(minutes).padStart(2,"0")+":"+tail : minutes+":"+tail
    }
    // Clear transient input without changing Kodi's playback position.
    function clearSeek(){seekTarget=-1;seekDirection=0;seekRepeats=0}
    // Resolve the visual order independently of optional transport actions.
    function transportSlot(row){
        if(row.adjacent)return transportSlots[row.adjacent<0?"previous":"next"]
        return transportSlots[row.page || row.control] ?? 0
    }
    // Move between visible controls in their on-screen order.
    function stepTransport(direction){
        const ordered=rows.map((row,index)=>({slot:transportSlot(row),index:index})).sort((a,b)=>a.slot-b.slot)
        const current=ordered.findIndex(item=>item.index===selectedIndex)
        if(current>=0)selectedIndex=ordered[Math.max(0,Math.min(ordered.length-1,current+direction))].index
    }
    // Repeated directional input accelerates the preview without sending seek RPCs.
    function previewSeek(direction, repeated){
        if(!canScrub)return
        seekRepeats=repeated && direction===seekDirection ? seekRepeats+1 : 0
        seekDirection=direction
        const step=seekRepeats>=12 ? 60 : seekRepeats>=5 ? 30 : 10
        seekTarget=Math.max(0,Math.min(duration-1,position+direction*step))
    }
    // Open at the primary playback control so OK can pause immediately.
    function showControls(){waitingAdjacent=false;restoreVideo=true;mode="controls";selectedIndex=0;showFullScreen();raise();requestActivate();body.forceActiveFocus();refreshTimer.start()}
    // Hide the overlay and optionally restore focus to the playing video.
    function closeControls(restore){restoreVideo=restore;hide()}
    // Back discards a preview before navigating away or restoring the video.
    function back(){if(waitingAdjacent){waitingAdjacent=false;kodiClient.playbackControl("stop");closeControls(false)}else if(scrubbing)clearSeek();else if(mode!=="controls"){mode="controls";selectedIndex=Math.min(controlOriginIndex,rows.length-1)}else hide()}
    // Route remote and pointer actions through the same playback controls.
    function handleCommand(command){
        if(command!==inputManager.confirm)pendingConfirm=false
        if(command===inputManager.home){closeControls(false);kodiClient.playbackControl("home");return}
        if(command===inputManager.menu){if(waitingAdjacent)back();else hide();return}
        if(command===inputManager.back){back();return}
        if(command===inputManager.playPause){if(!kodiClient.busy && !waitingAdjacent)kodiClient.playbackControl("pause");return}
        if(mode==="controls" && !waitingAdjacent && (command===inputManager.navigateLeft || command===inputManager.navigateRight)){
            previewSeek(command===inputManager.navigateLeft?-1:1,false)
            return
        }
        if(scrubbing && command!==inputManager.confirm)clearSeek()
        if(command===inputManager.navigateUp || command===inputManager.navigateDown){
            const direction=command===inputManager.navigateUp?-1:1
            if(mode==="controls")stepTransport(direction)
            else if(mode==="audiooffset")selectedIndex=Math.max(0,Math.min(2,selectedIndex+direction))
            else if(mode==="speed")selectedIndex=Math.max(0,Math.min(4,selectedIndex+direction))
            else if(mode==="subtitleoffset")selectedIndex=Math.max(0,Math.min(2,selectedIndex+direction))
            else if(mode==="chapters")selectedIndex=Math.max(0,Math.min(rows.length-1,selectedIndex+direction))
            else selectedIndex=Math.max(0,Math.min(rows.length-1,selectedIndex+direction))
        }
        else if(command===inputManager.confirm){
            if(kodiClient.busy){pendingConfirm=true;return}
            pendingConfirm=false
            if(scrubbing){
                const target=seekTarget
                clearSeek()
                if(canScrub)kodiClient.seekPlayback(target)
                return
            }
            const row=rows[selectedIndex];if(!row)return
            if(row.page){controlOriginIndex=selectedIndex;mode=row.page;selectedIndex=0;if(row.page!=="info")refreshTimer.start()}
            else if(row.adjacent){waitingAdjacent=true;if(kodiClient.playAdjacent(row.adjacent)){selectedIndex=0;refreshTimer.stop()}else waitingAdjacent=false}
            else if(row.control){kodiClient.playbackControl(row.control);if(row.control==="stop")closeControls(false)}
            else if(row.kind==="offset"){kodiClient.setAudioOffset(kodiClient.audioOffset+row.value);selectedIndex=1}
            else if(row.kind==="speed"){kodiClient.setPlaybackSpeed(row.value);selectedIndex=3}
            else if(row.kind==="subtitleoffset"){kodiClient.setSubtitleDelay(kodiClient.subtitleDelay+row.value);selectedIndex=2}
            else if(row.kind==="chapter"){kodiClient.seekChapter(row.value)}
            else if(row.kind && row.kind!=="none")kodiClient.setPlaybackOption(row.kind,row.value)
        }
    }
    onSelectedIndexChanged:list.positionViewAtIndex(selectedIndex,ListView.Contain)
    onVisibleChanged:if(!visible){pendingConfirm=false;clearSeek();refreshTimer.stop();if(restoreVideo && kodiClient.playbackActive)kodiClient.requestVideoPresentation()}
    Connections {
        target:root.kodiClient
        function onPlaybackOptionsRequested(){root.showControls()}
        function onPlaybackStarted(){if(root.waitingAdjacent){root.waitingAdjacent=false;root.closeControls(false)}}
        function onPlaybackReturned(){root.waitingAdjacent=false;root.closeControls(false)}
        function onChanged(){if(!root.waitingAdjacent && !root.kodiClient.playbackActive)root.closeControls(false);root.selectedIndex=Math.min(root.selectedIndex,Math.max(0,root.rows.length-1));if(root.visible && root.pendingConfirm && !root.kodiClient.busy)Qt.callLater(function(){if(root.visible && root.pendingConfirm && !root.kodiClient.busy){root.pendingConfirm=false;root.handleCommand(inputManager.confirm)}})}
    }
    Timer {id:refreshTimer;interval:100;onTriggered:{if(root.visible && root.kodiClient.playbackActive){if(root.kodiClient.busy)restart();else root.kodiClient.refreshPlaybackOptions()}}}
    Rectangle {anchors.fill:parent;color:shell.scrim;visible:root.mode!=="controls"}
    Rectangle {
        anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom
        height:Math.min(parent.height, 320*root.uiScale*shell.textScale)
        visible:root.mode==="controls"
        gradient:Gradient {
            GradientStop {position:0;color:"transparent"}
            GradientStop {position:0.48;color:Qt.rgba(shell.surface.r,shell.surface.g,shell.surface.b,0.88)}
            GradientStop {position:1;color:shell.surface}
        }
    }
    Item {
        id:body;x:root.safeInset;y:root.safeInset
        width:Math.max(1,(root.width-2*root.safeInset)/root.uiScale);height:Math.max(1,(root.height-2*root.safeInset)/root.uiScale)
        scale:root.uiScale;transformOrigin:Item.TopLeft;focus:true
        Keys.onPressed:event=>{
            const command=inputManager.commandForKey(event.key)
            if(command===inputManager.none)return
            if(event.isAutoRepeat && root.mode==="controls" && !root.waitingAdjacent &&
                (command===inputManager.navigateLeft || command===inputManager.navigateRight))
                root.previewSeek(command===inputManager.navigateLeft?-1:1,true)
            else if(!event.isAutoRepeat)root.handleCommand(command)
            event.accepted=true
        }
        Rectangle {
            anchors.horizontalCenter:parent.horizontalCenter;anchors.bottom:parent.bottom;anchors.bottomMargin:24
            width:Math.max(1,Math.min(parent.width-56,root.mode==="controls"?parent.width:620*shell.textScale))
            height:Math.max(1,Math.min(parent.height-32,(root.mode==="controls"?144:root.mode==="info"?230:410)*shell.textScale))
            color:root.mode==="controls"?"transparent":shell.surface;radius:shell.radius
            Column {
                anchors.fill:parent;anchors.margins:root.mode==="controls"?12:24
                spacing:(root.mode==="controls"?9:16)*Math.min(shell.gapScale,1.25)
                Text {id:heading;visible:root.mode!=="controls";width:parent.width;elide:Text.ElideRight;textFormat:Text.PlainText;text:root.mode==="info"?(root.timeline.title || qsTr("Playback information")):root.mode==="audio"?qsTr("Audio tracks"):root.mode==="subtitles"?qsTr("Subtitles"):root.mode==="audiooffset"?qsTr("Audio offset"):root.mode==="speed"?qsTr("Playback speed"):root.mode==="subtitleoffset"?qsTr("Subtitle delay"):root.mode==="chapters"?qsTr("Chapters"):qsTr("Picture size");font.family:shell.fontFamily;font.weight:Font.DemiBold;font.pixelSize:30*shell.textScale;color:shell.text}
                Text {
                    visible:root.mode==="info" && root.duration>0
                    width:parent.width;textFormat:Text.PlainText
                    text:qsTr("%1 elapsed · %2 remaining").arg(root.stamp(root.position)).arg(root.stamp(root.duration-root.position))
                    color:shell.muted;font.family:shell.fontFamily;font.pixelSize:18*shell.textScale
                }
                Text {
                    visible:root.mode==="info";width:parent.width;wrapMode:Text.WordWrap
                    text:root.canScrub?qsTr("Left and Right preview a seek. Hold either direction to move faster. OK jumps to the previewed time. Back cancels the preview."):qsTr("Seeking is unavailable for this source.")
                    color:shell.text;font.family:shell.fontFamily;font.pixelSize:18*shell.textScale
                }
                Item {
                    id:progress;width:parent.width;height:visible?34*shell.textScale:0
                    visible:root.mode==="controls" && root.duration>0
                    Rectangle {
                        width:parent.width;height:3*shell.textScale;radius:height/2;color:shell.muted;opacity:0.45
                    }
                    Rectangle {
                        id:played;width:parent.width*root.position/Math.max(1,root.duration);height:3*shell.textScale;radius:height/2;color:shell.selectedFocus
                    }
                    Rectangle {width:10*shell.textScale;height:width;radius:width/2;x:Math.max(0,Math.min(parent.width-width,played.width-width/2));y:(played.height-height)/2;color:shell.selectedFocus}
                    Text {anchors.bottom:parent.bottom;text:root.scrubbing?qsTr("Seek to %1").arg(root.stamp(root.position)):root.stamp(root.position);color:shell.text;font.family:shell.fontFamily;font.pixelSize:18*shell.textScale}
                    Text {anchors.right:parent.right;anchors.bottom:parent.bottom;text:qsTr("%1 remaining").arg(root.stamp(Math.max(0,root.duration-root.position)));color:shell.muted;font.family:shell.fontFamily;font.pixelSize:18*shell.textScale}
                }
                Text {id:status;width:parent.width;visible:text.length>0;elide:Text.ElideRight;textFormat:Text.PlainText;text:root.mode==="controls"?(["playing","paused","starting"].includes(root.kodiClient.playbackStatus)?"":root.kodiClient.playbackStatus || ""):root.mode==="info"?"":root.kodiClient.playbackOptionsStatus || "";font.family:shell.fontFamily;font.pixelSize:17*shell.textScale;color:shell.muted}
                Item {
                    id:transport
                    visible:root.mode==="controls";width:parent.width;height:54*shell.textScale
                    readonly property real step:Math.min(width/9,62*shell.textScale)
                    Repeater {
                        model:transport.visible?root.rows:[]
                        delegate:Rectangle {
                            required property int index
                            required property var modelData
                            readonly property bool primary:modelData.control==="pause"
                            readonly property bool highlighted:index===root.selectedIndex && !root.scrubbing
                            width:Math.min(transport.step-2,(primary?54:44)*shell.textScale);height:width
                            x:(transport.width-width)/2+root.transportSlot(modelData)*transport.step
                            y:(transport.height-height)/2
                            radius:width/2;color:highlighted?shell.selected:primary?shell.surface:"transparent"
                            border.width:highlighted?2:0;border.color:shell.selectedFocus
                            Accessible.role:Accessible.Button
                            Accessible.name:modelData.label
                            Text {
                                anchors.centerIn:parent;text:modelData.symbol || "…"
                                color:parent.highlighted?shell.selectedText:shell.text;font.family:shell.fontFamily
                                font.pixelSize:(parent.primary?27:modelData.symbol==="CC"?17:22)*shell.textScale
                            }
                            MouseArea {anchors.fill:parent;onClicked:{root.selectedIndex=index;root.handleCommand(inputManager.confirm)}}
                        }
                    }
                }
                ListView {
                    id:list;width:parent.width;height:root.mode==="controls"?48*shell.textScale:Math.max(0,parent.height-heading.height-(status.visible?status.height+parent.spacing:0)-help.height-2*parent.spacing);model:root.rows;clip:true;currentIndex:root.selectedIndex;spacing:6*shell.gapScale
                    orientation:root.mode==="controls"?ListView.Horizontal:ListView.Vertical
                    visible:root.mode!=="info" && root.mode!=="controls"
                    delegate:Rectangle {
                        required property int index;required property var modelData
                        width:root.mode==="controls"?Math.max(48*shell.textScale,label.implicitWidth+24*shell.textScale):list.width
                        height:root.mode==="controls"?list.height:Math.max(58*shell.textScale,label.implicitHeight+24)
                        radius:root.mode==="controls"?height/2:shell.radius
                        color:index===root.selectedIndex && !root.scrubbing?shell.selected:root.mode==="controls"?"transparent":shell.surface
                        border.width:index===root.selectedIndex && !root.scrubbing?2:0;border.color:shell.selectedFocus
                        Accessible.role:Accessible.Button
                        Accessible.name:modelData.label
                        Text {id:label;anchors.fill:parent;anchors.leftMargin:root.mode==="controls"?12*shell.textScale:12;anchors.rightMargin:anchors.leftMargin;verticalAlignment:Text.AlignVCenter;horizontalAlignment:root.mode==="controls"?Text.AlignHCenter:Text.AlignLeft;wrapMode:root.mode==="controls"?Text.NoWrap:Text.WordWrap;textFormat:Text.PlainText;text:modelData.symbol || modelData.label;color:index===root.selectedIndex && !root.scrubbing?shell.selectedText:shell.text;font.family:shell.fontFamily;font.pixelSize:(modelData.symbol==="CC"?17:root.mode==="controls"?23:21)*shell.textScale}
                        MouseArea {anchors.fill:parent;onClicked:{root.selectedIndex=index;root.handleCommand(inputManager.confirm)}}
                    }
                }
                Text {id:help;width:parent.width;horizontalAlignment:root.mode==="controls"?Text.AlignHCenter:Text.AlignLeft;elide:Text.ElideRight;text:root.scrubbing?qsTr("OK seeks · Back cancels"):root.mode==="controls"?(root.rows[root.selectedIndex]?.label || ""):qsTr("Back returns");font.family:shell.fontFamily;font.pixelSize:17*shell.textScale;color:shell.muted}
            }
        }
    }
}
