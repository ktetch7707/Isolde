import QtQuick

Item {
    id:root
    objectName:"pageSurface"
    required property var layoutModel
    required property var provider
    required property var availableApps
    required property var artwork
    required property var themes
    property string activeId:""
    readonly property bool open:activeId!==""
    readonly property var model:layoutModel.pages.some(p=>p.id===activeId)?layoutModel.pageModel(activeId):null
    readonly property var config:model?model.composition:({})
    readonly property var grid:model?model.grid:({})
    readonly property var placements:model?model.placements:([])
    property var history:[]
    property string homeOrigin:""
    readonly property var pageEntries:layoutModel.studioPages
    property bool editing:false
    property bool internetAvailable:true
    property bool localNetworkAvailable:true
    signal closed(string origin)
    signal searchRequested()
    signal settingsRequested(int section)
    function showPage(id,origin,replace){
        if(!layoutModel.pageModel(id))return
        if(!activeId)homeOrigin=origin || ""
        else if(!replace && id!==activeId)history=history.concat([{id:activeId,focus:origin || content.focusedItemId}])
        activeId=id;editing=false
        content.recoveryFocused=false;content.focusedIndex=0
    }
    function closeAll(){const origin=homeOrigin;activeId="";history=[];editing=false;closed(origin)}
    function restoreOrigin(origin){
        if(!content.widgetActive || Object.prototype.hasOwnProperty.call(content.appCollectionFocus,origin))content.restoreFocus(origin)
    }
    function back(){
        if(history.length){let stack=history.slice();const previous=stack.pop();history=stack;showPage(previous.id,"",true);Qt.callLater(()=>content.restoreFocus(previous.focus))}
        else closeAll()
    }
    function handleCommand(command){
        if(editing){editor.handleCommand(command);return}
        if(command===inputManager.home){closeAll();return}
        if(command===inputManager.back){if(content.widgetActive || content.groupActive)content.handleCommand(command);else back();return}
        if(command===inputManager.search){searchRequested();return}
        content.handleCommand(command)
    }
    Connections { target:root.layoutModel;function onLayoutChanged(){if(root.activeId && !root.model)root.closeAll()} }
    Rectangle {anchors.fill:parent;color:root.model?root.model.backgroundEnd:"#101014"}
    PageBackdrop {id:wallpaper;anchors.fill:parent;source:root.model?root.model.wallpaperSource:"";config:root.config.background || {};startColor:root.model?root.model.backgroundStart:"transparent";endColor:root.model?root.model.backgroundEnd:"transparent"}
    HomeScreen {
        id:content;objectName:"pageContent"
        anchors.fill:parent
        items:root.model?root.model.items:[];sections:root.model?root.model.sections:[];composition:model?model.composition:({});grid:root.grid;placements:root.placements;pageKey:root.activeId;libraryProvider:root.provider
        selectMode:model && model.selectMode || "single"; selectTrigger:model && model.selectTrigger || "click"; selectAction:model && model.selectAction || "navigate"; selectionColor:model && model.selectionColor || ""
        editLabel:"Edit page"
        menuPages:root.pageEntries;menuActiveId:root.activeId
        appearance:root.model?root.model.appearance:({});brandVisible:root.model?root.model.brandVisible:false
        backgroundStart:root.model?root.model.backgroundStart:"#101014";backgroundEnd:root.model?root.model.backgroundEnd:"#101014";tileRoundness:root.model?root.model.tileRoundness:0.18
        wallpaperSource:root.model?root.model.wallpaperSource:"";wallpaperItem:wallpaper
        internetAvailable:root.internetAvailable;localNetworkAvailable:root.localNetworkAvailable;hasContextualSources:false
        onActionActivated:(action,origin)=>actionDispatcher.dispatch(action,origin)
        onSearchRequested:root.searchRequested()
        onSettingsRequested:section=>{if(section===1)root.editing=true;else root.settingsRequested(section)}
    }
    CustomizeHomeView {
        id:editor;anchors.fill:parent;visible:root.editing;layoutModel:root.layoutModel;provider:root.provider;initialPageId:root.activeId
        availableApps:root.availableApps;artwork:root.artwork;themes:root.themes
        onPageRequested:id=>root.showPage(id,"",false)
        onClosed: {root.editing=false;content.restoreFocus(content.focusedItemId)}
    }
}
