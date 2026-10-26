import QtQuick

// A content adapter inside the shared page library surface. Presentation comes
// from the same Studio appearance map; catalog records never become layout nodes.
Item {
    id:root
    objectName:"gamesLibraryWidget"
    property var library:null
    property var dispatcher:null
    property string originId:""
    property var style:({})
    property var appearance:({})
    property bool engaged:false
    property bool readOnly:false
    property var items:[]
    property int selected:-1
    property int toolbar:0
    property int offset:0
    property string queryView:style.libraryView || "all"
    property string search:style.libraryValue || ""
    property string selectedId:""
    property var detail:null
    property string mode:"browse"
    property int choice:0
    property string message:""
    property var draft:({})
    property string editingId:""
    property bool importing:false
    property string editionGameId:""
    property var editionChoices:[]
    property int importOffset:0
    property var importItems:[]
    property int field:0
    readonly property var runtimes:library?library.runtimes:[]
    readonly property var tools:["Add game","All","Favorites","Recent","Search","Previous","Next","Import folder"]
    readonly property var importActions:["Import all reviewed files","Cancel · save nothing","Previous preview page","Next preview page"].concat(importItems.map(g=>g.title+" · "+g.target))
    readonly property var detailActions:detail?["Play",detail.favorite?"Unfavorite":"Favorite","Remove from library","Edit / relink source","Add edition / version","Back"]:[]
    readonly property int columns:style.libraryLayout==="list"?1:Math.max(1,style.libraryColumns || 4)
    readonly property real scale:appearance.typographyScale || 1
    readonly property string foreground:appearance.textColor || "#F5F1F3"
    readonly property string muted:appearance.mutedColor || "#AAA5AF"
    readonly property string focusColor:appearance.focusColor || "#C994A5"
    readonly property string surface:appearance.surfaceColor || "#202126"
    signal engage()
    Component.onCompleted:reload()
    onLibraryChanged:reload()
    onQueryViewChanged:{offset=0;reload()}
    onSearchChanged:{offset=0;reload()}
    Connections {target:root.library;function onChanged(){root.reload()}}
    Connections {target:root.library;function onImportChanged(){
        if(root.mode!=="scan" && root.mode!=="importPreview")return
        root.importItems=root.library.importPage(root.importOffset)
        if(root.library.importReady){root.mode="importPreview";root.message=root.library.importCount+" files found. Review filenames before importing; no metadata matching is assumed."}
        else if(!root.library.scanning){root.mode="browse";root.message=root.library.errorString || "Import canceled. Nothing was saved."}
    }}
    Connections {
        target:root.dispatcher
        function onRejected(reason){if(root.mode==="details")root.message=reason}
    }
    function reload(){
        if(!library){items=[];return}
        const remembered=selectedId
        items=library.page(queryView,search,offset)
        if(remembered){const at=items.findIndex(g=>g.id===remembered);if(at>=0)selected=at;else selected=Math.min(selected,items.length-1)}
        else selected=Math.min(selected,items.length-1)
        if(detail){const updated=library.details(detail.id);if(updated.id)detail=updated;else{detail=null;mode="browse"}}
    }
    function openDetails(){if(!items[selected])return;selectedId=items[selected].id;detail=library.details(selectedId);choice=0;message="";mode="details"}
    function beginAdd(asImport){
        if(!library || !library.ready){message=library?library.errorString:"Games is unavailable in this profile.";return}
        if(!runtimes.length){message="No compatible game runtime is installed. Game runtimes must be provisioned before a game can be added.";return}
        importing=!!asImport;editingId="";editionGameId="";draft=({schemaVersion:1,title:"",platform:"PC",runtimeId:"",root:"",target:""});choice=0;mode="runtime";message="Choose the installed game or runtime. Nothing is saved until review is confirmed."
    }
    function beginField(index){field=index;mode="keyboard";keyboard.selectedIndex=0;keyboard.value=index===4?search:index===0?draft.title:index===1?draft.platform:index===2?draft.root:draft.target}
    function activateTool(){
        message=""
        if(toolbar===0){beginAdd();return}
        if(toolbar===7){beginAdd(true);return}
        if(toolbar===4){beginField(4);return}
        if(toolbar===5){offset=Math.max(0,offset-50);selectedId="";selected=-1;reload();return}
        if(toolbar===6){if(items.length===50){offset+=50;selectedId="";selected=-1;reload()}return}
        search="";offset=0;selectedId="";selected=-1;queryView=toolbar===2?"favorites":toolbar===3?"recent":"all";reload()
    }
    function activateChoice(){
        if(mode==="editions"){
            if(choice===editionChoices.length){mode="details";choice=0;return}
            const edition=editionChoices[choice];if(edition)launchEdition(edition.id);return
        }
        if(mode==="scan"){library.cancelImport();mode="browse";return}
        if(mode==="importPreview"){
            if(choice===0){if(!library.commitImport()){message=library.errorString;return}mode="browse";reload();return}
            if(choice===1){library.cancelImport();mode="browse";return}
            if(choice===2)importOffset=Math.max(0,importOffset-50)
            else if(choice===3 && importOffset+50<library.importCount)importOffset+=50
            importItems=library.importPage(importOffset);return
        }
        if(mode==="runtime"){
            const runtime=runtimes[choice];if(!runtime)return
            if(importing && !runtime.file){message="Choose a file-based game runtime for folder import.";return}
            draft=Object.assign({},draft,{runtimeId:runtime.id},runtime.file?{}:{root:"",target:""});beginField(importing?1:0);return
        }
        if(mode==="review"){
            if(choice===1){mode="browse";return}
            const id=editionGameId?(library.addEdition(editionGameId,draft)?editionGameId:""):editingId?(library.updateGame(editingId,draft)?editingId:""):library.addGame(draft)
            if(!id){message=library.errorString;return}
            queryView="all";search="";offset=0;selectedId=id;selected=0;mode="browse";message="Game added. Select its card for details and Play.";reload();return
        }
        if(mode==="remove"){
            if(choice===1 && detail){const id=detail.id;if(!library.removeGame(id)){message=library.errorString;return}detail=null;selectedId=""}
            mode=detail?"details":"browse";choice=0;return
        }
        if(mode==="details" && detail){
            if(choice===0){
                editionChoices=library.editions(detail.id)
                if(editionChoices.length>1){mode="editions";choice=0;message="Choose the edition to play."}
                else launchEdition("")
            }else if(choice===1){if(!library.setFavorite(detail.id,!detail.favorite))message=library.errorString}
            else if(choice===2){mode="remove";choice=0;message="Remove only this library entry? Game files and saves stay where they are."}
            else if(choice===3){importing=false;editionGameId="";editingId=detail.id;draft=library.registration(editingId);detail=null;message="Edit this game or relink its folder. Review confirms the complete change; Back cancels.";choice=0;mode="runtime"}
            else if(choice===4){const game=detail.id;beginAdd(false);if(mode==="runtime"){editionGameId=game;message="Add a version to this game. Its launch settings remain separate. Confirming the first edition backs up and upgrades this catalog."}}
            else{mode="browse";detail=null}
        }
    }
    function launchEdition(id){
        if(!dispatcher){message="Game launching is unavailable.";return}
        const action={schemaVersion:1,type:"launch-game",gameId:detail.id}
        if(id)action.editionId=id
        if(!dispatcher.dispatch(action,originId)){if(library.errorString)message=library.errorString}
        else{mode="browse";detail=null}
    }
    function handleCommand(command){
        if(readOnly)return false
        if(mode==="keyboard"){keyboard.handleCommand(command);return true}
        if(command===inputManager.back){if(mode!=="browse"){if(mode==="scan" || mode==="importPreview")library.cancelImport();mode="browse";detail=null;draft=({});return true}return false}
        if(mode!=="browse"){
            const count=mode==="editions"?editionChoices.length+1:mode==="runtime"?runtimes.length:mode==="details"?detailActions.length:mode==="scan"?1:mode==="importPreview"?importActions.length:2
            if(command===inputManager.navigateDown || command===inputManager.navigateRight)choice=Math.min(count-1,choice+1)
            else if(command===inputManager.navigateUp || command===inputManager.navigateLeft)choice=Math.max(0,choice-1)
            else if(command===inputManager.confirm)activateChoice()
            return true
        }
        if(selected<0){
            if(command===inputManager.navigateUp || command===inputManager.navigateLeft && toolbar===0)return false
            if(command===inputManager.navigateLeft)toolbar=Math.max(0,toolbar-1)
            else if(command===inputManager.navigateRight)toolbar=Math.min(tools.length-1,toolbar+1)
            else if(command===inputManager.navigateDown && items.length){selected=0;selectedId=items[0].id}
            else if(command===inputManager.confirm)activateTool()
            return true
        }
        if(command===inputManager.confirm || command===inputManager.menu){openDetails();return true}
        if(command===inputManager.navigateUp)selected=selected<columns?-1:selected-columns
        else if(command===inputManager.navigateDown){if(selected+columns>=items.length)return false;selected+=columns}
        else if(command===inputManager.navigateLeft){if(selected===0)return false;selected--}
        else if(command===inputManager.navigateRight)selected=Math.min(items.length-1,selected+1)
        if(items[selected])selectedId=items[selected].id
        return true
    }
    Column {
        anchors.fill:parent;spacing:8;visible:root.mode==="browse"
        Row {
            width:parent.width;height:44;spacing:4
            Repeater {model:root.tools
                delegate:Rectangle {
                    required property int index;required property string modelData
                    width:(parent.width-4*(root.tools.length-1))/root.tools.length;height:44;radius:root.style.radius || 8;color:root.surface
                    border.color:root.focusColor;border.width:root.engaged && root.selected<0 && root.toolbar===index?2:0
                    Text {anchors.centerIn:parent;textFormat:Text.PlainText;text:modelData;color:root.foreground;font.pixelSize:14*root.scale}
                    MouseArea {anchors.fill:parent;enabled:!root.readOnly;onClicked:{root.engage();root.selected=-1;root.toolbar=index;root.activateTool()}}
                }
            }
        }
        GridView {
            id:grid;width:parent.width;height:Math.max(50,parent.height-100);clip:true
            cellWidth:width/root.columns;cellHeight:root.style.libraryLayout==="list"?78:Math.max(140,root.style.artHeight || 230)
            model:root.items;currentIndex:root.selected;highlightMoveDuration:0
            delegate:Item {
                required property int index;required property var modelData
                width:grid.cellWidth;height:grid.cellHeight
                HomeCard {
                    anchors.fill:parent;anchors.margins:(root.style.cardGap===undefined?16:root.style.cardGap)/2
                    item:({name:modelData.title,presentation:Object.assign({},root.style,{variant:root.style.libraryCardStyle || "media",subtitle:modelData.platform+(modelData.favorite?" · Favorite":"")})})
                    appearance:root.appearance;selected:root.engaged && root.selected===index
                    onActivated:{if(root.readOnly)return;root.engage();root.selected=index;root.openDetails()}
                }
            }
            Text {
                anchors.centerIn:parent;width:parent.width-32;wrapMode:Text.WordWrap;horizontalAlignment:Text.AlignHCenter
                visible:!grid.count;textFormat:Text.PlainText;text:root.library && root.library.ready?"No games in this view. Choose Add game, or Back to return to the page.":root.library?root.library.errorString:"Games preview is unavailable."
                color:root.muted;font.pixelSize:18*root.scale
            }
        }
        Text {width:parent.width;wrapMode:Text.WordWrap;textFormat:Text.PlainText;text:root.message || "OK details · Menu details · Back leaves library";color:root.muted;font.pixelSize:15*root.scale}
    }
    Rectangle {
        anchors.fill:parent;visible:root.mode!=="browse";color:root.surface;radius:root.style.radius || 12
        Column {
            anchors.fill:parent;anchors.margins:20;spacing:12;visible:root.mode!=="keyboard"
            Text {width:parent.width;wrapMode:Text.WordWrap;textFormat:Text.PlainText;text:root.mode==="details" && root.detail?root.detail.title:root.mode==="runtime"?"Choose game runtime":root.mode==="remove"?"Remove library entry":"Review game";color:root.foreground;font.pixelSize:24*root.scale}
            Text {width:parent.width;wrapMode:Text.WordWrap;textFormat:Text.PlainText;text:root.mode==="review"?root.draft.title+" · "+root.draft.platform+"\n"+(root.draft.root?"Allow this game runtime to read: "+root.draft.root+"\nFile: "+root.draft.target:"Installed game: "+root.draft.runtimeId):root.message;color:root.muted;font.pixelSize:17*root.scale}
            ListView {
                width:parent.width;height:Math.max(60,parent.height-160);clip:true;currentIndex:root.choice;highlightMoveDuration:0
                model:root.mode==="editions"?root.editionChoices.map(e=>e.title+" · "+e.platform).concat(["Back"]):root.mode==="scan"?["Cancel scan · save nothing"]:root.mode==="importPreview"?root.importActions:root.mode==="runtime"?root.runtimes.map(r=>r.name):root.mode==="details"?root.detailActions:root.mode==="remove"?["Cancel","Remove entry · keep files and saves"]:[root.editionGameId?"Add edition · preserve other versions":root.editingId?"Save changes and approve source":"Add game and approve source","Cancel"]
                delegate:Rectangle {
                    required property int index;required property string modelData
                    width:ListView.view.width;height:48;color:root.surface;radius:8
                    border.color:root.focusColor;border.width:root.choice===index?2:0
                    Text {anchors.fill:parent;anchors.margins:10;verticalAlignment:Text.AlignVCenter;textFormat:Text.PlainText;text:modelData;color:root.foreground;font.pixelSize:18*root.scale;elide:Text.ElideRight}
                    MouseArea {anchors.fill:parent;enabled:!root.readOnly;onClicked:{root.choice=index;root.activateChoice()}}
                }
            }
        }
    }
    OnScreenKeyboard {
        id:keyboard;anchors.fill:parent;visible:root.mode==="keyboard";appearance:root.appearance
        hint:root.field===0?"Game title":root.field===1?"Platform name":root.field===2?"Specific absolute game folder · source access reviewed before saving":root.field===3?"Game file relative to that folder":"Search local game titles"
        onCanceled:{root.draft=({});root.mode="browse"}
        onAccepted:text=>{
            if(root.field===4){root.search=text;root.queryView="search";root.mode="browse";root.reload();return}
            const keys=["title","platform","root","target"];const next=Object.assign({},root.draft);next[keys[root.field]]=text;root.draft=next
            const runtime=root.runtimes.find(r=>r.id===root.draft.runtimeId)
            if(root.field===0)root.beginField(1)
            else if(root.field===1 && runtime && runtime.file)root.beginField(2)
            else if(root.field===2 && root.importing){
                root.importOffset=0;root.choice=1;root.mode="scan";root.message="Scanning names only. Nothing is saved until review is confirmed."
                if(!root.library.beginImport(root.draft.root,root.draft.runtimeId,root.draft.platform)){root.mode="browse";root.message=root.library.errorString}
            }
            else if(root.field===2)root.beginField(3)
            else{root.mode="review";root.choice=0;root.message=""}
        }
    }
}
