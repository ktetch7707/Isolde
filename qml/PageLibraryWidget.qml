import QtQuick

Item {
    id:root
    objectName:"pageLibraryWidget"
    required property var provider
    required property string widgetKey
    property string originId:widgetKey
    property var style: ({})
    property var appearance: ({})
    property bool engaged:false
    property bool highlighted:false
    property bool readOnly:false
    property bool frozen:false
    property bool samplePreview:false
    property var sampleItems:[]
    readonly property bool games:style.libraryKind==="games"
    readonly property bool unifiedGuide:style.libraryKind==="channels" && typeof guideController!=="undefined" && !!guideController
    property var heldClient:null
    property var frozenItems:[]
    readonly property var client:games || unifiedGuide?null:frozen?heldClient:provider ? provider.client(widgetKey,style) : null
    onClientChanged:if(!frozen)heldClient=client
    onFrozenChanged:{if(frozen)frozenItems=heldClient?heldClient.items.slice():[];else frozenItems=[]}
    readonly property var liveItems:frozen?frozenItems:client?client.items:[]
    readonly property bool usingSamples:readOnly && samplePreview && !liveItems.length
    readonly property var displayedItems:usingSamples?sampleItems:liveItems.slice(0,style.libraryLimit || 50)
    onDisplayedItemsChanged:Qt.callLater(function(){
        if(!root.frozen && root.client && !root.client.busy && root.displayedItems.length>0 && root.client.selectedIndex>=root.displayedItems.length)
            root.client.selectIndex(root.displayedItems.length-1)
    })
    property int toolbarIndex:0
    property var details:null
    property string returnMediaKey:""
    function rememberSelection(){const item=client?client.items[client.selectedIndex]:null;if(item)returnMediaKey=item.mediaType+":"+item.mediaId}
    Connections {
        target:root.client
        function onChanged(){
            if(!root.client || root.client.busy || !root.returnMediaKey)return
            const index=root.displayedItems.findIndex(i=>i.mediaType+":"+i.mediaId===root.returnMediaKey)
            if(index>=0 && root.client.selectedIndex!==index)root.client.selectIndex(index)
        }
    }
    readonly property bool history:style.libraryKind==="history"
    readonly property bool music:style.libraryKind==="albums"
    readonly property bool guide:style.libraryKind==="channels"
    readonly property var actions:history?["Refresh"]:music || guide?["Refresh","Previous","Next"]:["Refresh","Previous","Next","All","Recent","Continue"]
    function subtitleFor(item){return guide?item.subtitle || style.subtitle || "":music?(Array.isArray(item.artist)?item.artist.join(", "):String(item.artist || "")):item.year?String(item.year):item.showtitle || ""}
    readonly property bool listMode:style.libraryLayout === "list"
    readonly property bool rowMode:style.libraryLayout === "row"
    readonly property bool chrome:style.libraryChrome!==false
    readonly property int columns:listMode ? 1 : Math.max(1,style.libraryColumns || 4)
    signal engage()
    function invoke(index) {
        if(!client || readOnly || frozen)return
        if(index===0)provider.refresh(client)
        else if(index===1)client.previousPage()
        else if(index===2)client.nextPage()
        else client.openView(style.libraryKind || "movies",index===4?"recent":index===5?"continue":"all","")
    }
    function handleCommand(command) {
        if(readOnly || frozen)return false
        if(games)return gamesLoader.item?gamesLoader.item.handleCommand(command):false
        if(unifiedGuide)return guideLoader.item?guideLoader.item.handleCommand(command):false
        if(!client)return command !== inputManager.back && command !== inputManager.navigateUp
        const index=client.selectedIndex, step=columns
        if(command===inputManager.back){if(details){details=null;return true}if(client.goBack())return true;return false}
        if(details){if(command===inputManager.confirm || !guide && !music && command===inputManager.navigateRight){rememberSelection();provider.activate(client,index,command!==inputManager.navigateRight);details=null}return true}
        if(command===inputManager.menu && index>=0){details=client.items[index];return true}
        if(index<0){
            if(command===inputManager.navigateUp || command===inputManager.navigateLeft && toolbarIndex===0)return false
            if(command===inputManager.navigateLeft)toolbarIndex=Math.max(0,toolbarIndex-1)
            else if(command===inputManager.navigateRight)toolbarIndex=Math.min(actions.length-1,toolbarIndex+1)
            else if(command===inputManager.navigateDown)client.selectIndex(0)
            else if(command===inputManager.confirm)invoke(toolbarIndex)
            return true
        }
        if(command===inputManager.confirm){rememberSelection();provider.activate(client,index);return true}
        if(command===inputManager.navigateUp){if(!chrome && (rowMode || index<step))return false;returnMediaKey="";client.selectIndex(index<step?-1:index-step);return true}
        if(command===inputManager.navigateDown){if(rowMode||index+step>=root.displayedItems.length)return false;returnMediaKey="";client.selectIndex(index+step);return true}
        if(command===inputManager.navigateLeft){if(index===0)return false;returnMediaKey="";client.selectIndex(index-1);return true}
        if(command===inputManager.navigateRight){if(index+1<root.displayedItems.length){returnMediaKey="";client.selectIndex(index+1)}return true}
        return false
    }
    Rectangle { anchors.fill:parent; color:"transparent"; radius:root.style.radius || 12; border.width:root.engaged || root.highlighted?2:0; border.color:root.appearance.focusColor || "#C994A5" }
    Column {
        visible:!root.games && !root.unifiedGuide
        anchors.fill:parent; anchors.margins:root.chrome?5:0; spacing:root.chrome?8:0
        Row {
            visible:root.chrome;width:parent.width; height:visible?36:0; spacing:5
            Repeater { model:root.actions
                delegate:Rectangle {
                    required property int index; required property string modelData
                    width:(parent.width-(root.actions.length-1)*5)/root.actions.length;height:36;radius:8;color:root.appearance.surfaceColor || "#202126"
                    border.width:root.engaged && root.client && root.client.selectedIndex<0 && root.toolbarIndex===index?2:0;border.color:root.appearance.focusColor || "#C994A5"
                    Text {textFormat:Text.PlainText; anchors.centerIn:parent;text:modelData;color:root.appearance.textColor || "#F5F1F3";font.pixelSize:13*(root.appearance.typographyScale || 1) }
                    MouseArea { anchors.fill:parent;onClicked:{root.engage();if(root.client)root.client.selectIndex(-1);root.toolbarIndex=index;root.invoke(index)} }
                }
            }
        }
        GridView {
            id:grid; objectName:"pageLibraryGrid";width:parent.width;height:Math.max(50,parent.height-(root.chrome?84:0));clip:true
            flow:root.rowMode?GridView.FlowTopToBottom:GridView.FlowLeftToRight
            cellWidth:width/root.columns;cellHeight:root.listMode?78:root.rowMode?height:Math.max(140,root.style.artHeight || 230)
            model:root.displayedItems;currentIndex:root.client?root.client.selectedIndex:-1;highlightRangeMode:GridView.ApplyRange;highlightMoveDuration:0
            delegate:Item {
                required property int index;required property var modelData
                width:grid.cellWidth;height:grid.cellHeight
                readonly property var art: { const revision=root.provider ? root.provider.revision : 0; return root.provider && !root.usingSamples && !root.listMode ? root.provider.artwork(modelData.mediaType,modelData.mediaId) : null }
                HomeCard {
                    objectName:"libraryCard-"+index
                    anchors.fill:parent
                    anchors.leftMargin:(root.style.cardGap===undefined?16:root.style.cardGap)/2
                    anchors.rightMargin:anchors.leftMargin
                    anchors.topMargin:root.rowMode?0:anchors.leftMargin
                    anchors.bottomMargin:anchors.topMargin
                    item:({name:modelData.title || modelData.label || "Untitled",logoSource:parent.art?parent.art.source:"",presentation:Object.assign({},root.style,{variant:root.style.libraryCardStyle || (root.listMode?"summary":"media"),subtitle:root.subtitleFor(modelData),progress:modelData.resume && modelData.resume.total>0?modelData.resume.position/modelData.resume.total:undefined})})
                    appearance:root.appearance;selected:root.engaged && root.client && root.client.selectedIndex===index
                    onActivated:{root.engage();if(root.client && !root.readOnly && !root.frozen){root.returnMediaKey="";root.client.selectIndex(index);root.rememberSelection();root.provider.activate(root.client,index)}}
                }
            }
            Text {textFormat:Text.PlainText; anchors.centerIn:parent;width:parent.width-20;wrapMode:Text.WordWrap;horizontalAlignment:Text.AlignHCenter;visible:!grid.count;text:root.client?root.client.status:"Choose a valid Kodi source in Design Home. Back returns to the page.";color:root.appearance.mutedColor || "#AAA5AF";font.pixelSize:17 }
        }
        Text {visible:root.chrome || root.usingSamples;textFormat:Text.PlainText; width:parent.width;elide:Text.ElideRight;text:root.usingSamples?"Design samples · not your library or viewing history":root.readOnly?"Read-only Kodi preview · playback and refresh are intercepted":(root.client?root.client.status:"")+" · OK enters / plays · Menu details · Back leaves widget";color:root.appearance.mutedColor || "#AAA5AF";font.pixelSize:12 }
    }
    Rectangle {
        anchors.fill:parent;visible:!root.games && !root.unifiedGuide && !!root.details;color:root.appearance.surfaceColor || "#202126";radius:12
        Column { anchors.fill:parent;anchors.margins:22;spacing:12
            Text {textFormat:Text.PlainText; text:root.details?root.details.title || "":"";color:root.appearance.textColor || "white";font.pixelSize:24;width:parent.width;elide:Text.ElideRight }
            Text {textFormat:Text.PlainText; text:root.details?root.details.plot || "No description supplied by Kodi.":"";color:root.appearance.mutedColor || "#AAA5AF";font.pixelSize:17;width:parent.width;wrapMode:Text.WordWrap;maximumLineCount:Math.max(1,Math.floor((root.height-130)/22));elide:Text.ElideRight }
            Text {textFormat:Text.PlainText; text:root.guide?"Confirm watches channel · Back closes":root.music?"Confirm plays track · Back closes":"OK resume / browse · Right restart · Back close";color:root.appearance.focusColor || "#C994A5";font.pixelSize:15 }
        }
        MouseArea { anchors.right:parent.right;anchors.top:parent.top;width:80;height:40;onClicked:root.details=null;Text {textFormat:Text.PlainText;anchors.centerIn:parent;text:"Back";color:root.appearance.textColor || "white"} }
    }
    Loader {
        id:guideLoader;anchors.fill:parent;active:root.unifiedGuide
        sourceComponent:GuideWidget {
            options:typeof homeLayout!=="undefined"?homeLayout.studioOptions.guide:({})
            controller:typeof guideController!=="undefined"?guideController:null
            dispatcher:typeof actionDispatcher!=="undefined"?actionDispatcher:null
            originId:root.originId;style:root.style;appearance:root.appearance;engaged:root.engaged;readOnly:root.readOnly || root.frozen
            onEngage:root.engage()
        }
    }
    Loader {
        id:gamesLoader;anchors.fill:parent;active:root.games
        sourceComponent:GamesLibraryWidget {
            library:typeof gameLibrary!=="undefined"?gameLibrary:null
            dispatcher:typeof actionDispatcher!=="undefined"?actionDispatcher:null
            originId:root.originId
            style:root.style;appearance:root.appearance
            engaged:root.engaged;readOnly:root.readOnly || root.frozen
            onEngage:root.engage()
        }
    }
}
