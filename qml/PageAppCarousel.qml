import QtQuick
import QtQuick.Controls

Item {
    id:root;objectName:"pageAppCarousel"
    property var registry:null
    property var buttonItems:null
    readonly property bool buttons:buttonItems!==null
    property var collection:({mode:"all",ids:[]})
    property var style:({})
    property var appearance:({})
    property real unit:1
    property bool readOnly:false
    property bool engaged:false
    property bool highlighted:false
    property string selectedId:""
    property string galleryId:""
    property bool pendingGalleryRestore:false
    property string message:""
    readonly property var allApps:(registry?registry.apps:[]).filter(a=>(a.type==="web" || a.type==="desktop" || a.type==="android") && (!collection.section || a.section===collection.section))
    readonly property bool collectionTooLarge:collection.mode==="all" && allApps.length>128
    readonly property var eligible:{
        const all=allApps
        return buttons?buttonItems:collection.mode==="selected"?(collection.ids || []).map(id=>all.find(a=>a.id===id) || {id:id,name:"Unavailable app",symbol:"?",unavailableReason:"This app is no longer available in this collection. Choose apps in Edit component to repair it."}):collectionTooLarge?[]:all
    }
    readonly property bool multipleRows:buttons && (style.layout || {}).mode==="grid"
    readonly property int capacity:Math.max(1,multipleRows?(style.layout.columns*style.layout.rows):(style.layout || {}).visibleColumns || 4)
    readonly property int cardColumns:multipleRows?style.layout.columns:Math.max(1,Math.min(capacity,cards.length))
    readonly property int cardRows:multipleRows?Math.max(1,Math.ceil(cards.length/cardColumns)):1
    readonly property bool overflow:eligible.length>capacity
    readonly property var storeApp:buttons || collection.showStore===false?null:(registry?registry.apps:[]).find(a=>a.type==="store") || null
    readonly property var galleryApps:eligible.concat(storeApp?[storeApp]:[])
    readonly property var cards:eligible.slice(0,overflow?capacity-1:capacity).concat(overflow?[{id:"__more__",name:style.moreLabel || "More apps"}]:[])
    onCardsChanged:if(!cards.some(a=>a.id===selectedId))selectedId=cards.length?cards[0].id:""
    onGalleryAppsChanged:if(!galleryApps.some(a=>a.id===galleryId))galleryId=galleryApps.length?galleryApps[0].id:""
    signal engage()
    signal launch(var action)
    function logoSource(app){return (style.logoMode || "cutout")==="cutout"?app.cutoutLogoSource || app.logoSource || "":app.logoSource || ""}
    function cardStyle(gallery){const result=Object.assign({},style);delete result.visibleLabel;delete result.showHeading;delete result.headingSize;if(result.cardGap===undefined)result.cardGap=(style.layout || {}).gap || 0;return Object.assign(result,gallery?{variant:"orb",diameter:style.diameter || 78}:{},{icon:style.icon || "grid",logoMode:style.logoMode || "cutout",artFit:style.artFit || "contain"})}
    function focusState(){return {selectedId:selectedId,galleryId:galleryId,galleryOpen:gallery.opened || pendingGalleryRestore}}
    function restoreFocusState(state){
        if(!state)return
        if(cards.some(a=>a.id===state.selectedId))selectedId=state.selectedId
        pendingGalleryRestore=false
        if(galleryApps.some(a=>a.id===state.galleryId))galleryId=state.galleryId
        if(state.galleryOpen && overflow)gallery.open()
    }
    function activate(id){
        if(readOnly)return
        message=""
        if(id==="__more__"){pendingGalleryRestore=false;galleryId=galleryApps.length?galleryApps[0].id:"";gallery.open();return}
        const app=galleryApps.find(a=>a.id===id)
        if(app && app.unavailableReason){message=app.unavailableReason;return}
        if(app){
            if(buttons && (!app.action || !app.action.type)){message="This button has no link. Choose a link in Edit component.";return}
            pendingGalleryRestore=gallery.opened
            if(gallery.opened)gallery.close()
            if(buttons)launch(app.action)
            else launch({type:"launch-app",appId:id})
        }
    }
    function handleCommand(command){
        if(readOnly)return false
        if(gallery.opened){
            if(command===inputManager.back){pendingGalleryRestore=false;gallery.close();return true}
            const index=galleryApps.findIndex(a=>a.id===galleryId)
            const delta=command===inputManager.navigateLeft?-1:command===inputManager.navigateRight?1:command===inputManager.navigateUp?-4:command===inputManager.navigateDown?4:0
            if(delta){const next=galleryApps[Math.max(0,Math.min(galleryApps.length-1,index+delta))];if(next)galleryId=next.id;return true}
            if(command===inputManager.confirm)activate(galleryId)
            return true
        }
        if(command===inputManager.back)return false
        const index=cards.findIndex(a=>a.id===selectedId)
        if(command===inputManager.navigateUp || command===inputManager.navigateDown){if(!multipleRows)return false;const target=index+(command===inputManager.navigateUp?-cardColumns:cardColumns);if(target<0 || target>=cards.length)return false;selectedId=cards[target].id;return true}
        if(command===inputManager.navigateLeft || command===inputManager.navigateRight){const next=cards[index+(command===inputManager.navigateRight?1:-1)];if(next)selectedId=next.id;return true}
        if(command===inputManager.confirm){activate(selectedId);return true}
        return false
    }
    Grid {
        columns:root.cardColumns
        anchors.fill:parent;spacing:(root.style.cardGap===undefined?(root.style.layout || {}).gap || 0:root.style.cardGap)*root.unit
        Repeater {
            model:root.cards
            delegate:HomeCard {
                required property var modelData
                objectName:"appCarouselCard-"+modelData.id
                width:(root.width-(root.cardColumns-1)*parent.spacing)/root.cardColumns;height:(root.height-(root.cardRows-1)*parent.spacing)/root.cardRows
                item:({name:modelData.name,unavailableReason:modelData.unavailableReason || "",logoSource:root.logoSource(modelData),appSymbol:modelData.id==="__more__"?"":modelData.symbol || modelData.name.slice(0,1),presentation:Object.assign({},root.cardStyle(false),modelData.presentation || {})})
                appearance:root.appearance;unit:root.unit
                selected:(root.engaged || root.highlighted) && root.selectedId===modelData.id
                onActivated:{root.engage();root.selectedId=modelData.id;root.activate(modelData.id)}
            }
        }
    }
    Text {anchors.centerIn:parent;width:parent.width;wrapMode:Text.WordWrap;horizontalAlignment:Text.AlignHCenter;visible:!root.cards.length;text:root.collectionTooLarge?"Choose up to 128 apps":root.style.subtitle || (root.buttons?"Add buttons in Edit contents":root.collection.mode==="selected"?"Choose apps":"No installed apps");color:root.appearance.textColor;font.pixelSize:18*root.unit}
    Text {anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;text:root.message;visible:!!text;wrapMode:Text.Wrap;color:root.appearance.textColor;font.pixelSize:(root.style.labelSize || 18)*root.unit}
    Popup {
        id:gallery;objectName:"appCarouselGallery";parent:Overlay.overlay
        width:Math.min(parent?parent.width-48:900,900);height:Math.min(parent?parent.height-48:500,500)
        x:parent?(parent.width-width)/2:0;y:parent?(parent.height-height)/2:0
        closePolicy:Popup.NoAutoClose;padding:24
        background:Rectangle {color:root.appearance.surfaceColor || "#28262f";radius:20;border.width:2;border.color:root.appearance.focusColor || "white"}
        Column {
            anchors.fill:parent;spacing:16
            Text {width:parent.width;text:root.message || root.style.galleryTitle || "Apps";wrapMode:Text.Wrap;color:root.appearance.textColor;font.pixelSize:root.message?18:28}
            GridView {
                id:galleryGrid;objectName:"appCarouselGalleryGrid";width:parent.width;height:parent.height-50;clip:true
                cellWidth:width/4;cellHeight:130;model:root.galleryApps;currentIndex:root.galleryApps.findIndex(a=>a.id===root.galleryId)
                highlightMoveDuration:0;highlightRangeMode:GridView.ApplyRange
                delegate:HomeCard {
                    required property var modelData
                    width:galleryGrid.cellWidth;height:galleryGrid.cellHeight
                    item:({name:modelData.name,unavailableReason:modelData.unavailableReason || "",logoSource:root.logoSource(modelData),appSymbol:modelData.symbol || modelData.name.slice(0,1),presentation:Object.assign({},root.cardStyle(true),modelData.presentation || {})})
                    appearance:root.appearance;selected:root.galleryId===modelData.id
                    onActivated:{root.galleryId=modelData.id;root.activate(modelData.id)}
                }
            }
        }
    }
}
