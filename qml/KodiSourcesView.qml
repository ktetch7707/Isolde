import QtQuick

Item {
    id:root
    property var appearance:({})
    property var kodiClient:kodiMedia
    property string mode:"list"
    property int selectedIndex:0
    property var draft:({})
    property string pending:""
    property string message:""
    property bool confirmSave:false
    property bool newSource:false
    property int refreshAttempts:0
    readonly property var contentTypes:["movies","tvshows","musicvideos","music",""]
    readonly property var contentLabels:[qsTr("Movies"),qsTr("TV shows"),qsTr("Music videos"),qsTr("Music · embedded file tags"),qsTr("Unspecified / inherit")]
    readonly property var availableContentTypes:newSource?contentTypes:draft.category==="music"?["music"]:["movies","tvshows","musicvideos",""]
    readonly property var scrapers:(kodiClient.sourceScrapers || []).filter(row=>row.content===draft.content)
    readonly property string scraperLabel:{const row=scrapers.find(item=>item.id===draft.scraper);return row?row.name:qsTr("Choose an installed provider")}
    SurfacePalette {id:shell;appearance:root.appearance}
    readonly property var rows:{
        if(mode==="folders")return []
        if(mode==="roots")return (kodiClient.sourceRoots || []).map(path=>({label:path,path:path})).concat([{label:qsTr("Back"),back:true}])
        if(mode==="preview")return [{label:qsTr("Back to draft"),back:true},{label:confirmSave?qsTr("CONFIRM · Apply this source change"):qsTr("Apply reviewed source change"),apply:true}]
        if(mode==="edit")return [
            {label:qsTr("Name · ")+(draft.name || ""),key:"name"},
            {label:qsTr("Content · ")+contentLabels[Math.max(0,contentTypes.indexOf(draft.content))],key:"content"},
            {label:qsTr("Metadata provider · ")+(draft.category==="music"?qsTr("Embedded file tags"):scraperLabel),key:"scraper"}].concat(draft.category==="music"?[]:[
            {label:qsTr("Scan subfolders · ")+(draft.recursive?qsTr("On"):qsTr("Off")),key:"recursive"},
            {label:qsTr("Use folder names · ")+(draft.folderNames?qsTr("On"):qsTr("Off")),key:"folderNames"},
            {label:qsTr("Exclude from library updates · ")+(draft.noUpdate?qsTr("On"):qsTr("Off")),key:"noUpdate"},
            {label:qsTr("Exclude from scanning · ")+(draft.exclude?qsTr("On"):qsTr("Off")),key:"exclude"}]).concat([
            {label:qsTr("Preview source settings"),preview:true},
            {label:qsTr("Remove this source · preview consequences"),remove:true},
            {label:qsTr("Cancel · keep saved source"),back:true}])
        return [{label:qsTr("Refresh sources and providers"),refresh:true},{label:qsTr("Allow a media folder · read-only access"),folders:true},{label:qsTr("Add a Kodi source"),add:true}].concat((kodiClient.librarySources || []).map(source=>({label:source.name+(source.path?" · "+source.path:"")+(source.editable?"":" · "+qsTr("preserved, read-only")),source:source})))
    }
    function refresh(){refreshAttempts=0;refreshTimer.restart()}
    function changedDraft(value){draft=Object.assign({},draft,value);confirmSave=false;kodiClient.cancelSourcePreview();message=qsTr("Draft only · preview before applying")}
    function chooseDefaultScraper(){const choices=(kodiClient.sourceScrapers || []).filter(row=>row.content===draft.content);changedDraft({scraper:(choices.find(row=>row.id==="metadata.local") || choices[0] || {}).id || ""})}
    function edit(source,isNew){
        newSource=isNew===true
        draft={name:source.name,path:source.path,category:source.category || "video",content:source.content || "",scraper:source.scraper || "",recursive:source.recursive===true,folderNames:source.folderNames===true,noUpdate:source.noUpdate===true,exclude:source.exclude===true,remove:false}
        kodiClient.cancelSourcePreview();mode="edit";selectedIndex=0;message=qsTr("Draft only · Back leaves the saved source unchanged")
    }
    function back(){
        if(mode==="keyboard"){keyboard.handleCommand(inputManager.back);return true}
        if(mode==="folders"){if(!folderPage.back()){mode="list";selectedIndex=1;refresh()}return true}
        if(mode==="preview"){if(confirmSave)confirmSave=false;else {mode="edit";selectedIndex=rows.findIndex(row=>row.preview)}return true}
        if(mode!=="list"){mode="list";selectedIndex=0;pending="";kodiClient.cancelSourcePreview();return true}
        return false
    }
    function handleCommand(command){
        if(mode==="keyboard"){keyboard.handleCommand(command);return}
        if(mode==="folders"){folderPage.handleCommand(command);return}
        if(command===inputManager.back){back();return}
        if(command===inputManager.navigateUp){confirmSave=false;selectedIndex=Math.max(0,selectedIndex-1)}
        else if(command===inputManager.navigateDown){confirmSave=false;selectedIndex=Math.min(rows.length-1,selectedIndex+1)}
        else if(command===inputManager.confirm && !kodiClient.busy){
            const row=rows[selectedIndex];if(!row)return
            if(row.back){back();return}
            if(row.refresh){refresh();return}
            if(row.folders){mode="folders";return}
            if(!profileManager.editingAllowed){message=qsTr("This profile does not allow source changes");return}
            if(row.add){mode="roots";selectedIndex=0;return}
            if(row.path){const path=row.path.replace(/\/+$/,"")+"/";edit({name:path.split('/').filter(part=>part.length).pop(),path:path,content:"movies",recursive:true},true);chooseDefaultScraper();return}
            if(row.source){if(row.source.editable)edit(row.source);else message=qsTr("This source is preserved. Editing supports allowed local single-folder sources; existing remote and multipath definitions stay intact.");return}
            if(row.apply){if(!confirmSave){confirmSave=true;return}confirmSave=false;pending="apply";kodiClient.applySource(draft);return}
            if(row.preview || row.remove){if(row.remove)changedDraft({remove:true});else if(draft.remove)changedDraft({remove:false});pending="preview";kodiClient.previewSource(draft);return}
            if(row.key==="name"){keyboard.value=draft.name;keyboard.selectedIndex=0;keyboard.mode="letters";keyboard.shift=false;mode="keyboard";return}
            if(row.key==="content"){const content=availableContentTypes[(Math.max(0,availableContentTypes.indexOf(draft.content))+1)%availableContentTypes.length];changedDraft({content:content,category:content==="music"?"music":"video",recursive:content==="music"?true:draft.recursive,folderNames:content==="music"?false:draft.folderNames,noUpdate:content==="music"?false:draft.noUpdate,exclude:content==="music"?false:draft.exclude});chooseDefaultScraper();return}
            if(row.key==="scraper"){if(!scrapers.length){message=qsTr("No installed provider for this content. Unspecified content has no provider.");return}const index=scrapers.findIndex(value=>value.id===draft.scraper);changedDraft({scraper:scrapers[(index+1)%scrapers.length].id});return}
            if(row.key) {const value={};value[row.key]=!draft[row.key];changedDraft(value)}
        }
    }
    function reveal(){const row=sourceRows.itemAt(selectedIndex);if(!row)return;const top=rowColumn.y+row.y;if(top<scroll.contentY)scroll.contentY=top;else if(top+row.height>scroll.contentY+scroll.height)scroll.contentY=top+row.height-scroll.height}
    onSelectedIndexChanged:revealTimer.restart()
    onModeChanged:revealTimer.restart()
    onVisibleChanged:{pending="";confirmSave=false;mode="list";selectedIndex=0;if(visible)refresh();else{refreshTimer.stop();kodiClient.cancelSourcePreview()}}
    Connections {
        target:root.kodiClient
        function onSourceApplied(){if(root.pending==="apply"){root.pending="";root.mode="list";root.selectedIndex=0;root.message=""}}
        function onChanged(){
            if(!root.visible || root.kodiClient.busy || !root.pending.length)return
            const operation=root.pending;root.pending=""
            if(operation==="preview" && root.kodiClient.sourcePreviewReady){root.mode="preview";root.selectedIndex=0;root.message=""}
            else if(operation==="apply"){root.confirmSave=false;root.message=root.kodiClient.sourceStatus}
            else {root.selectedIndex=0;root.message=root.kodiClient.sourceStatus}
        }
    }
    Timer {id:refreshTimer;interval:100;onTriggered:{if(root.visible && root.mode==="list"){if(root.kodiClient.busy || !root.kodiClient.connected){if(++root.refreshAttempts<300)restart();else root.message=qsTr("Library unavailable. Check Media Library status and choose Refresh to retry.")}else root.kodiClient.refreshSources()}}}
    Timer {id:revealTimer;interval:0;onTriggered:if(root.visible)root.reveal()}
    Rectangle {anchors.fill:parent;color:shell.surface}
    Flickable {
        id:scroll;anchors.fill:parent;clip:true;contentHeight:contents.implicitHeight;boundsBehavior:Flickable.StopAtBounds;visible:root.mode!=="folders" && root.mode!=="keyboard"
        Column {
            id:contents;width:parent.width;spacing:16*shell.gapScale
            Text {width:parent.width;wrapMode:Text.WordWrap;text:qsTr("KODI SOURCE SETUP");font.family:shell.fontFamily;font.pixelSize:30*shell.textScale;color:shell.text}
            Text {width:parent.width;wrapMode:Text.WordWrap;textFormat:Text.PlainText;text:root.message || root.kodiClient.sourceStatus || qsTr("Reading sources…");font.family:shell.fontFamily;font.pixelSize:18*shell.textScale;color:shell.focus}
            Text {width:parent.width;visible:root.mode==="edit" || root.mode==="preview";wrapMode:Text.WrapAnywhere;textFormat:Text.PlainText;text:root.draft.path || "";font.family:shell.fontFamily;font.pixelSize:17*shell.textScale;color:shell.muted}
            Text {width:parent.width;visible:root.mode==="preview";wrapMode:Text.WordWrap;textFormat:Text.PlainText;text:root.draft.remove?qsTr("Remove this source definition and exclude its path from scanning. Media files and existing Kodi titles, watched state and resume positions are retained."):[root.draft.name,root.contentLabels[Math.max(0,root.contentTypes.indexOf(root.draft.content))],root.scraperLabel,qsTr("Subfolders: ")+(root.draft.recursive?qsTr("On"):qsTr("Off")),qsTr("Folder names: ")+(root.draft.folderNames?qsTr("On"):qsTr("Off")),qsTr("Exclude from updates: ")+(root.draft.noUpdate?qsTr("On"):qsTr("Off")),qsTr("Exclude from scanning: ")+(root.draft.exclude?qsTr("On"):qsTr("Off"))].join("\n");font.family:shell.fontFamily;font.pixelSize:20*shell.textScale;color:shell.text}
            Column {
                id:rowColumn;width:parent.width;spacing:8*shell.gapScale
                Repeater {
                    id:sourceRows;model:root.rows
                    delegate:Rectangle {
                        required property int index;required property var modelData
                        width:rowColumn.width;height:Math.max(64*shell.textScale,rowLabel.implicitHeight+28);radius:shell.radius;color:root.selectedIndex===index?shell.selected:shell.surface;border.width:root.selectedIndex===index?2:0;border.color:shell.selectedFocus
                        Text {id:rowLabel;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:14;wrapMode:Text.WrapAnywhere;textFormat:Text.PlainText;text:modelData.label;color:root.selectedIndex===index?shell.selectedText:shell.text;font.family:shell.fontFamily;font.pixelSize:21*shell.textScale}
                        MouseArea {anchors.fill:parent;onClicked:{root.selectedIndex=index;root.handleCommand(inputManager.confirm)}}
                    }
                }
            }
            Text {width:parent.width;wrapMode:Text.WordWrap;text:qsTr("Up/Down chooses · OK edits/previews · Back cancels · Saving never moves media or cleans the library. Use Scan in Media Library after applying settings.");font.family:shell.fontFamily;font.pixelSize:17*shell.textScale;color:shell.muted}
        }
    }
    MediaFoldersView {id:folderPage;anchors.fill:parent;appearance:root.appearance;visible:root.mode==="folders" && root.visible}
    OnScreenKeyboard {id:keyboard;anchors.fill:parent;visible:root.mode==="keyboard" && root.visible;maximumLength:160;appearance:root.appearance;onAccepted:value=>{root.mode="edit";root.changedDraft({name:value})};onCanceled:root.mode="edit"}
}
