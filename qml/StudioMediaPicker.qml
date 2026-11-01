import QtQuick

Item {
    id:root
    property var provider:null
    property var appearance:({})
    readonly property real unit:(appearance.typographyScale || 1)*Math.min(width/1280,height/720)
    property string libraryKind:"movies"
    property var collections:[]
    readonly property var availableCollections:collections.filter(c=>(c.mediaKind || "movies")===libraryKind)
    property string collectionId:""
    onVisibleChanged:if(visible){collectionId="";selected=0}
    property int selected:0
    readonly property var client:visible && provider && collectionId?provider.client("studio-action-picker",{libraryKind:libraryKind,libraryView:"collection",libraryValue:collectionId}):null
    readonly property var items:client?client.items.filter(i=>libraryKind==="shows"?i.mediaType==="tvshow":i.mediaType==="movie"):[]
    readonly property var labels:!collectionId?availableCollections.map(c=>c.name):["Previous page","Next page"].concat(items.slice(0,50).map(i=>i.title || i.label || "Untitled"))
    signal picked(var action)
    signal canceled()
    function activate(){if(!collectionId){const entry=availableCollections[selected];if(entry){collectionId=entry.action.collectionId;selected=0}return}if(!client)return;if(selected===0){client.previousPage();selected=0;return}if(selected===1){client.nextPage();selected=0;return}const item=items[selected-2];if(!item)return;picked(libraryKind==="shows"?{type:"open-show",collectionId:collectionId,tvShowId:item.mediaId}:{type:"play-movie",collectionId:collectionId,movieId:item.mediaId})}
    function handleCommand(command){if(command===inputManager.back){if(collectionId){collectionId="";selected=0}else canceled();return}if(command===inputManager.confirm){activate();return}if(command===inputManager.navigateDown || command===inputManager.navigateRight)selected=Math.min(labels.length-1,selected+1);else if(command===inputManager.navigateUp || command===inputManager.navigateLeft)selected=Math.max(0,selected-1)}
    Rectangle {
        anchors.centerIn:parent;width:Math.min(parent.width-48*root.unit,700*root.unit);height:Math.min(parent.height-48*root.unit,620*root.unit);radius:18;color:root.appearance.surfaceColor;border.width:2;border.color:root.appearance.focusColor
        Column {anchors.fill:parent;anchors.margins:24*root.unit;spacing:14*root.unit
            Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";text:root.libraryKind==="shows"?"Choose a Kodi show":"Choose a Kodi movie";color:root.appearance.textColor;font.pixelSize:26*root.unit}
            Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";width:parent.width;text:!root.collectionId?(root.availableCollections.length?"Choose a saved Kodi collection":"No saved Kodi collections · configure your library first. Back returns."):root.client?root.client.status:"Kodi unavailable · Back returns without changes";color:root.appearance.mutedColor;font.pixelSize:17*root.unit;wrapMode:Text.Wrap}
            ListView {width:parent.width;height:parent.height-116*root.unit;clip:true;model:root.labels;currentIndex:root.selected;spacing:6;highlightRangeMode:ListView.ApplyRange
                delegate:Rectangle {required property int index;required property string modelData;width:ListView.view.width;height:48*root.unit;radius:8;color:index===root.selected?root.appearance.selectedColor:"transparent";border.width:index===root.selected?2:0;border.color:root.appearance.focusColor
                    Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";anchors.fill:parent;anchors.margins:10;text:modelData;color:root.appearance.textColor;font.pixelSize:19*root.unit;elide:Text.ElideRight}
                    MouseArea {anchors.fill:parent;onClicked:{root.selected=index;root.activate()}}
                }
            }
        }
    }
}
