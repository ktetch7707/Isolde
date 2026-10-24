import QtQuick
import QtQuick.Controls

Item {
    id:root;objectName:"pageAppCarousel"
    property var registry:null
    property var collection:({mode:"all",ids:[]})
    property var style:({})
    property var appearance:({})
    property real unit:1
    property bool readOnly:false
    property bool engaged:false
    property bool highlighted:false
    property string selectedId:""
    property string galleryId:""
    readonly property var allApps:(registry?registry.apps:[]).filter(a=>(a.type==="web" || a.type==="desktop") && (!collection.section || a.section===collection.section))
    readonly property bool collectionTooLarge:collection.mode==="all" && allApps.length>128
    readonly property var eligible:{
        const all=allApps
        return collection.mode==="selected"?(collection.ids || []).map(id=>all.find(a=>a.id===id)).filter(a=>!!a):collectionTooLarge?[]:all
    }
    readonly property int capacity:Math.max(1,(style.layout || {}).visibleColumns || 4)
    readonly property bool overflow:eligible.length>capacity
    readonly property var cards:eligible.slice(0,overflow?capacity-1:capacity).concat(overflow?[{id:"__more__",name:"More apps"}]:[])
    onCardsChanged:if(!cards.some(a=>a.id===selectedId))selectedId=cards.length?cards[0].id:""
    onEligibleChanged:if(!eligible.some(a=>a.id===galleryId))galleryId=eligible.length?eligible[0].id:""
    signal engage()
    signal launch(var action)
    function logoSource(app){return (style.logoMode || "cutout")==="cutout"?app.cutoutLogoSource || app.logoSource || "":app.logoSource || ""}
    function focusState(){return {selectedId:selectedId,galleryId:galleryId,galleryOpen:gallery.opened}}
    function restoreFocusState(state){
        if(!state)return
        if(cards.some(a=>a.id===state.selectedId))selectedId=state.selectedId
        if(eligible.some(a=>a.id===state.galleryId))galleryId=state.galleryId
        if(state.galleryOpen && overflow)gallery.open()
    }
    function activate(id){
        if(readOnly)return
        if(id==="__more__"){galleryId=eligible.length?eligible[0].id:"";gallery.open();return}
        if(eligible.some(a=>a.id===id))launch({type:"launch-app",appId:id})
    }
    function handleCommand(command){
        if(readOnly)return false
        if(gallery.opened){
            if(command===inputManager.back){gallery.close();return true}
            const index=eligible.findIndex(a=>a.id===galleryId)
            const delta=command===inputManager.navigateLeft?-1:command===inputManager.navigateRight?1:command===inputManager.navigateUp?-4:command===inputManager.navigateDown?4:0
            if(delta){const next=eligible[Math.max(0,Math.min(eligible.length-1,index+delta))];if(next)galleryId=next.id;return true}
            if(command===inputManager.confirm)activate(galleryId)
            return true
        }
        if(command===inputManager.back || command===inputManager.navigateUp || command===inputManager.navigateDown)return false
        const index=cards.findIndex(a=>a.id===selectedId)
        if(command===inputManager.navigateLeft || command===inputManager.navigateRight){const next=cards[index+(command===inputManager.navigateRight?1:-1)];if(next)selectedId=next.id;return true}
        if(command===inputManager.confirm){activate(selectedId);return true}
        return false
    }
    Row {
        anchors.fill:parent;spacing:(root.style.cardGap || 0)*root.unit
        Repeater {
            model:root.cards
            delegate:HomeCard {
                required property var modelData
                objectName:"appCarouselCard-"+modelData.id
                width:(root.width-(Math.min(root.capacity,root.cards.length)-1)*parent.spacing)/Math.max(1,Math.min(root.capacity,root.cards.length));height:root.height
                item:({name:modelData.name,logoSource:root.logoSource(modelData),appSymbol:modelData.id==="__more__"?"":modelData.symbol || modelData.name.slice(0,1),presentation:Object.assign({},root.style,{icon:root.style.icon || "grid",logoMode:root.style.logoMode || "cutout",artFit:root.style.artFit || "contain"})})
                appearance:root.appearance;unit:root.unit
                selected:(root.engaged || root.highlighted) && root.selectedId===modelData.id
                onActivated:{root.engage();root.selectedId=modelData.id;root.activate(modelData.id)}
            }
        }
    }
    Text {anchors.centerIn:parent;width:parent.width;wrapMode:Text.WordWrap;horizontalAlignment:Text.AlignHCenter;visible:!root.cards.length;text:root.collectionTooLarge?"Choose up to 128 apps":root.style.subtitle || (root.collection.mode==="selected"?"Choose apps":"No installed apps");color:root.appearance.textColor;font.pixelSize:18*root.unit}
    Popup {
        id:gallery;objectName:"appCarouselGallery";parent:Overlay.overlay
        width:Math.min(parent?parent.width-48:900,900);height:Math.min(parent?parent.height-48:500,500)
        x:parent?(parent.width-width)/2:0;y:parent?(parent.height-height)/2:0
        closePolicy:Popup.NoAutoClose;padding:24
        background:Rectangle {color:root.appearance.surfaceColor || "#28262f";radius:20;border.width:2;border.color:root.appearance.focusColor || "white"}
        Column {
            anchors.fill:parent;spacing:16
            Text {text:"Apps";color:root.appearance.textColor;font.pixelSize:28}
            GridView {
                id:galleryGrid;objectName:"appCarouselGalleryGrid";width:parent.width;height:parent.height-50;clip:true
                cellWidth:width/4;cellHeight:130;model:root.eligible;currentIndex:root.eligible.findIndex(a=>a.id===root.galleryId)
                highlightMoveDuration:0;highlightRangeMode:GridView.ApplyRange
                delegate:HomeCard {
                    required property var modelData
                    width:galleryGrid.cellWidth;height:galleryGrid.cellHeight
                    item:({name:modelData.name,logoSource:root.logoSource(modelData),appSymbol:modelData.symbol || modelData.name.slice(0,1),presentation:Object.assign({},root.style,{variant:"orb",diameter:78,icon:root.style.icon || "grid",logoMode:root.style.logoMode || "cutout",artFit:root.style.artFit || "contain"})})
                    appearance:root.appearance;selected:root.galleryId===modelData.id
                    onActivated:{root.galleryId=modelData.id;root.activate(modelData.id)}
                }
            }
        }
    }
}
