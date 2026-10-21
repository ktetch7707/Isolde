import QtQuick

Rectangle {
    id:root
    objectName:"pageManager"
    required property var layoutModel
    property var hostModel:layoutModel
    required property var availableApps
    required property var artwork
    required property var themes
    property bool open:false
    property int selected:0
    property int operation:0
    property string naming:""
    property string message:""
    property bool deleting:false
    property var editModel:null
    readonly property var pages:layoutModel.pages
    readonly property var choice:pages[selected] || ({})
    readonly property var operations:["Open page","Edit components","New blank page","New Movies page","New Shows page","New standalone Movies","Copy Home to page","Rename page","Delete page","Add Home tab","Add Home button","Use for Movies","Use for Shows","Classic Movies","Classic Shows","Add button to edited page","Add tab to edited page"]
    signal pageRequested(string id)
    color:"#101014";visible:open
    function begin(){selected=0;operation=0;naming="";editModel=null;deleting=false;message="";open=true}
    function execute(){
        message=""
        if(operation>=2 && operation<=7){naming=operation===7?"rename":["blank","movies","shows","standalone","home"][operation-2];keyboard.value=naming==="rename"?choice.name || "":"My page";keyboard.selectedIndex=0;return}
        if(operation===13 || operation===14){let config=Object.assign({},layoutModel.composition);config.schemaVersion=1;delete config[operation===13?"moviePage":"showsPage"];layoutModel.setComposition(config);message="Classic library restored";return}
        if(!choice.id){message="Create a page first.";return}
        if(operation===0)pageRequested(choice.id)
        else if(operation===1)editModel=layoutModel.pageModel(choice.id)
        else if(operation===8)deleting=true
        else if(operation===9 || operation===10){message=layoutModel.linkPage(choice.id,operation===9)?"Home link added":layoutModel.errorString}
        else if(operation===15 || operation===16){message=hostModel.linkPage(choice.id,operation===16)?"Page link added":hostModel.errorString}
        else {let config=Object.assign({},layoutModel.composition);config.schemaVersion=1;config[operation===11?"moviePage":"showsPage"]=choice.id;message=layoutModel.setComposition(config)?"Library destination updated":layoutModel.errorString}
    }
    function acceptName(text){
        if(naming==="rename"){if(!layoutModel.renamePage(choice.id,text)){message=layoutModel.errorString;return}}
        else {const id=layoutModel.createPage(text,naming);if(!id){message=layoutModel.errorString;return}for(let i=0;i<pages.length;i++)if(pages[i].id===id)selected=i}
        naming="";message="Saved"
    }
    function handleCommand(command){
        if(editModel){if(command===inputManager.back && (!pageEditor.item || !pageEditor.item.editing))editModel=null;else if(pageEditor.item) pageEditor.item.handleCommand(command);return}
        if(naming){keyboard.handleCommand(command);return}
        if(deleting){if(command===inputManager.back)deleting=false;else if(command===inputManager.confirm){message=layoutModel.removePage(choice.id)?"Page removed. Its links remain visible and repairable.":layoutModel.errorString;selected=Math.max(0,Math.min(selected,pages.length-1));deleting=false}return}
        if(command===inputManager.back)open=false
        else if(command===inputManager.navigateUp)selected=Math.max(0,selected-1)
        else if(command===inputManager.navigateDown)selected=Math.min(pages.length-1,selected+1)
        else if(command===inputManager.navigateLeft)operation=Math.max(0,operation-1)
        else if(command===inputManager.navigateRight)operation=Math.min(operations.length-1,operation+1)
        else if(command===inputManager.confirm)execute()
    }
    MouseArea {anchors.fill:parent}
    Column {
        anchors.fill:parent;anchors.margins:40;spacing:16
        Text {text:"Pages & library destinations";color:"#F5F1F3";font.pixelSize:32}
        Text {text:"Create a page, place components, then link it from Home or make it your Movies/Shows destination. Pages are included in Home skins.";width:parent.width;wrapMode:Text.WordWrap;color:"#AAA5AF";font.pixelSize:17}
        ListView {
            width:parent.width;height:58;orientation:ListView.Horizontal;spacing:8;clip:true;model:root.operations;currentIndex:root.operation;highlightRangeMode:ListView.ApplyRange
            delegate:Rectangle {
                required property int index;required property string modelData
                width:180;height:58;radius:10;color:root.operation===index?"#553C4C":"#25242C";border.color:root.operation===index?"#E3ACC3":"#39353D"
                Text {anchors.fill:parent;anchors.margins:8;verticalAlignment:Text.AlignVCenter;horizontalAlignment:Text.AlignHCenter;wrapMode:Text.WordWrap;text:modelData;color:"#F5F1F3";font.pixelSize:15}
                MouseArea {anchors.fill:parent;onClicked:{root.operation=index;root.execute()}}
            }
        }
        Text {text:"Action: "+root.operations[root.operation];color:"#E3ACC3";font.pixelSize:18}
        ListView {
            width:parent.width;height:Math.max(80,parent.height-290);spacing:8;clip:true;model:root.pages;currentIndex:root.selected;highlightRangeMode:ListView.ApplyRange
            delegate:Rectangle {
                required property int index;required property var modelData
                width:ListView.view.width;height:75;radius:10;color:root.selected===index?"#353039":"#202126";border.width:root.selected===index?2:0;border.color:"#D19AB1"
                Column {anchors.fill:parent;anchors.margins:12;spacing:5;Text {text:modelData.name;color:"#F5F1F3";font.pixelSize:22} Text {text:modelData.id;color:"#AAA5AF";font.pixelSize:12}}
                MouseArea {anchors.fill:parent;onClicked:root.selected=index}
            }
            Text {anchors.centerIn:parent;visible:!root.pages.length;text:"No pages yet. Choose New blank page or a library template.";color:"#AAA5AF";font.pixelSize:20}
        }
        Text {text:root.message || "↑ ↓ page · ← → action · OK continues · Back closes";width:parent.width;wrapMode:Text.WordWrap;color:"#E3ACC3";font.pixelSize:17}
    }
    Rectangle {anchors.fill:parent;visible:root.deleting;color:"#101014";Text {anchors.centerIn:parent;width:parent.width-140;wrapMode:Text.WordWrap;text:"Delete “"+(root.choice.name || "")+"”? Saved skins keep their snapshots.\nOK deletes · Back cancels";color:"white";font.pixelSize:26}}
    Rectangle {anchors.fill:parent;visible:root.naming!=="";color:"#101014";OnScreenKeyboard {id:keyboard;objectName:"pageNameKeyboard";anchors.fill:parent;anchors.margins:45;onAccepted:text=>root.acceptName(text);onCanceled:root.naming=""} Text {anchors.bottom:parent.bottom;anchors.bottomMargin:12;x:45;text:root.message;color:"#FFB4B4";font.pixelSize:17}}
    onEditModelChanged: {
        if(editModel) pageEditor.setSource("LegacyCustomizeHomeView.qml", {layoutModel:editModel,items:Qt.binding(()=>root.editModel ? root.editModel.items:[]),availableApps:availableApps,artwork:artwork,themes:themes})
        else pageEditor.source=""
    }
    Loader { id:pageEditor; anchors.fill:parent; onLoaded:item.pageRequested.connect(id=>root.pageRequested(id)) }
}
