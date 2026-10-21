import QtQuick

Item {
    id:root
    objectName:"pageLibraryWidget"
    required property var provider
    required property string widgetKey
    property var style: ({})
    property var appearance: ({})
    property bool engaged:false
    property bool highlighted:false
    property bool readOnly:false
    property bool frozen:false
    property bool samplePreview:false
    property var sampleItems:[]
    property var heldClient:null
    property var frozenItems:[]
    readonly property var client:frozen?heldClient:provider ? provider.client(widgetKey,style) : null
    onClientChanged:if(!frozen)heldClient=client
    onFrozenChanged:{if(frozen)frozenItems=heldClient?heldClient.items.slice():[];else frozenItems=[]}
    readonly property var liveItems:frozen?frozenItems:client?client.items:[]
    readonly property bool usingSamples:readOnly && samplePreview && !liveItems.length
    readonly property var displayedItems:usingSamples?sampleItems:liveItems.slice(0,style.libraryLimit || 50)
    property int toolbarIndex:0
    property var details:null
    readonly property var actions:["Refresh","Previous","Next","All","Recent","Continue"]
    readonly property bool listMode:style.libraryLayout === "list"
    readonly property bool rowMode:style.libraryLayout === "row"
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
        if(!client)return command !== inputManager.back && command !== inputManager.navigateUp
        const index=client.selectedIndex, step=columns
        if(command===inputManager.back){if(details){details=null;return true}if(client.goBack())return true;return false}
        if(details){if(command===inputManager.confirm || command===inputManager.navigateRight){provider.activate(client,index,command!==inputManager.navigateRight);details=null}return true}
        if(command===inputManager.menu && index>=0){details=client.items[index];return true}
        if(index<0){
            if(command===inputManager.navigateUp || command===inputManager.navigateLeft && toolbarIndex===0)return false
            if(command===inputManager.navigateLeft)toolbarIndex=Math.max(0,toolbarIndex-1)
            else if(command===inputManager.navigateRight)toolbarIndex=Math.min(actions.length-1,toolbarIndex+1)
            else if(command===inputManager.navigateDown)client.selectIndex(0)
            else if(command===inputManager.confirm)invoke(toolbarIndex)
            return true
        }
        if(command===inputManager.confirm){provider.activate(client,index);return true}
        if(command===inputManager.navigateUp){client.selectIndex(index<step?-1:index-step);return true}
        if(command===inputManager.navigateDown){if(rowMode||index+step>=client.items.length)return false;client.selectIndex(index+step);return true}
        if(command===inputManager.navigateLeft){if(index===0)return false;client.selectIndex(index-1);return true}
        if(command===inputManager.navigateRight){client.selectIndex(index+1);return true}
        return false
    }
    Rectangle { anchors.fill:parent; color:"transparent"; radius:root.style.radius || 12; border.width:root.engaged || root.highlighted?2:0; border.color:root.appearance.focusColor || "#C994A5" }
    Column {
        anchors.fill:parent; anchors.margins:5; spacing:8
        Row {
            width:parent.width; height:36; spacing:5
            Repeater { model:root.actions
                delegate:Rectangle {
                    required property int index; required property string modelData
                    width:(parent.width-25)/6;height:36;radius:8;color:root.appearance.surfaceColor || "#202126"
                    border.width:root.engaged && root.client && root.client.selectedIndex<0 && root.toolbarIndex===index?2:0;border.color:root.appearance.focusColor || "#C994A5"
                    Text {textFormat:Text.PlainText; anchors.centerIn:parent;text:modelData;color:root.appearance.textColor || "#F5F1F3";font.pixelSize:13*(root.appearance.typographyScale || 1) }
                    MouseArea { anchors.fill:parent;onClicked:{root.engage();if(root.client)root.client.selectIndex(-1);root.toolbarIndex=index;root.invoke(index)} }
                }
            }
        }
        GridView {
            id:grid; objectName:"pageLibraryGrid";width:parent.width;height:Math.max(50,parent.height-84);clip:true
            flow:root.rowMode?GridView.FlowTopToBottom:GridView.FlowLeftToRight
            cellWidth:width/root.columns;cellHeight:root.listMode?78:root.rowMode?height:Math.max(140,root.style.artHeight || 230)
            model:root.displayedItems;currentIndex:root.client?root.client.selectedIndex:-1;highlightRangeMode:GridView.ApplyRange;highlightMoveDuration:0
            delegate:Item {
                required property int index;required property var modelData
                width:grid.cellWidth;height:grid.cellHeight
                readonly property var art: { const revision=root.provider ? root.provider.revision : 0; return root.provider && !root.usingSamples && !root.listMode ? root.provider.artwork(modelData.mediaType,modelData.mediaId) : null }
                HomeCard {
                    objectName:"libraryCard-"+index
                    anchors.fill:parent;anchors.margins:8
                    item:({name:modelData.title || modelData.label || "Untitled",logoSource:parent.art?parent.art.source:"",presentation:Object.assign({},root.style,{variant:root.style.libraryCardStyle || (root.listMode?"summary":"media"),subtitle:modelData.year?String(modelData.year):modelData.showtitle || "",progress:modelData.resume && modelData.resume.total>0?modelData.resume.position/modelData.resume.total:0})})
                    appearance:root.appearance;selected:root.engaged && root.client && root.client.selectedIndex===index
                    onActivated:{root.engage();if(root.client && !root.readOnly && !root.frozen){root.client.selectIndex(index);root.provider.activate(root.client,index)}}
                }
            }
            Text {textFormat:Text.PlainText; anchors.centerIn:parent;width:parent.width-20;wrapMode:Text.WordWrap;horizontalAlignment:Text.AlignHCenter;visible:!grid.count;text:root.client?root.client.status:"Choose a valid Kodi source in Design Home. Back returns to the page.";color:root.appearance.mutedColor || "#AAA5AF";font.pixelSize:17 }
        }
        Text {textFormat:Text.PlainText; width:parent.width;elide:Text.ElideRight;text:root.usingSamples?"Design samples · not your library or viewing history":root.readOnly?"Read-only Kodi preview · playback and refresh are intercepted":(root.client?root.client.status:"")+" · OK enters / plays · Menu details · Back leaves widget";color:root.appearance.mutedColor || "#AAA5AF";font.pixelSize:12 }
    }
    Rectangle {
        anchors.fill:parent;visible:!!root.details;color:root.appearance.surfaceColor || "#202126";radius:12
        Column { anchors.fill:parent;anchors.margins:22;spacing:12
            Text {textFormat:Text.PlainText; text:root.details?root.details.title || "":"";color:root.appearance.textColor || "white";font.pixelSize:24;width:parent.width;elide:Text.ElideRight }
            Text {textFormat:Text.PlainText; text:root.details?root.details.plot || "No description supplied by Kodi.":"";color:root.appearance.mutedColor || "#AAA5AF";font.pixelSize:17;width:parent.width;wrapMode:Text.WordWrap;maximumLineCount:Math.max(1,Math.floor((root.height-130)/22));elide:Text.ElideRight }
            Text {textFormat:Text.PlainText; text:"OK resume / browse · Right restart · Back close";color:root.appearance.focusColor || "#C994A5";font.pixelSize:15 }
        }
        MouseArea { anchors.right:parent.right;anchors.top:parent.top;width:80;height:40;onClicked:root.details=null;Text {textFormat:Text.PlainText;anchors.centerIn:parent;text:"Back";color:root.appearance.textColor || "white"} }
    }
}
