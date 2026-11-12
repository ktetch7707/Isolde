import QtQuick

Item {
    id:root;objectName:"pageSourcesWidget";clip:true
    property var provider:null
    property var style:({})
    property var appearance:({})
    property var centerAction:typeof homeLayout!=="undefined" && homeLayout?(homeLayout.studioOptions.headerActions || {}).sources || ({}):({})
    property string artworkSource:""
    property var workflow:({})
    property string pageId:""
    property string slotId:""
    property bool notice:false
    property bool readOnly:false
    property bool engaged:false
    property bool highlighted:false
    property real unit:1
    readonly property var entries:{const revision=provider?provider.revision:0;return provider?provider.projected(pageId,slotId,notice,workflow):[]}
    readonly property bool compact:style.sourceVariant==="compact"
    readonly property int maximum:style.sourceMaximum || 1
    signal engage()
    signal launch(var action)
    function focusState(){return compact?({}):carousel.focusState()}
    function restoreFocusState(state){if(!compact)carousel.restoreFocusState(state)}
    function openSources(id){if(readOnly)return;if(provider && !provider.requestSource(id || ""))return;if(centerAction.type)launch(centerAction)}
    function handleCommand(command){
        if(readOnly)return false
        if(command===inputManager.menu){openSources(compact?"":carousel.selectedId);return true}
        if(compact || !entries.length){if(command===inputManager.confirm){engage();openSources("");return true}return false}
        return carousel.handleCommand(command)
    }
    HomeCard {
        objectName:"sourceSummaryCard"
        anchors.fill:parent;visible:root.compact || !root.entries.length;appearance:root.appearance;unit:root.unit
        selected:root.highlighted || root.engaged
        item:({name:(root.style.visibleLabel || "Sources")+(root.style.sourceBadge!==false && root.entries.length?" · "+root.entries.length:""),logoSource:root.artworkSource,presentation:Object.assign({},root.style,{subtitle:root.entries.length?(root.style.sourceDetails?root.entries[0].name:""):"No new sources"})})
        onActivated:root.openSources("")
    }
    PageAppCarousel {
        id:carousel;anchors.fill:parent;visible:!root.compact && root.entries.length>0
        buttonItems:root.entries.map(s=>Object.assign({},s,{presentation:Object.assign({},s.presentation || {},{subtitle:root.style.sourceDetails?s.target:""})}))
        style:Object.assign({},root.style,{layout:Object.assign({},root.style.layout || {},{mode:root.style.sourceVariant==="list"?"grid":"row",columns:root.style.sourceVariant==="list"?1:root.maximum,rows:root.style.sourceVariant==="list"?root.maximum:1,visibleColumns:root.maximum}),moreLabel:(root.style.moreLabel || "View all")+" · "+root.entries.length})
        appearance:root.appearance;galleryAppearance:root.appearance;unit:root.unit
        readOnly:root.readOnly;engaged:root.engaged;highlighted:root.highlighted
        onEngage:root.engage()
        onLaunch:action=>root.launch(action)
    }
}
