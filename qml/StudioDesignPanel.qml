import QtQuick

Rectangle {
    id: root
    objectName: "studioDesignTools"
    required property var session
    required property var appearance
    property real viewportScale: 1
    property string pageId: ""
    property string view: "menu"
    property int selected: 0
    property string scope: "page"
    property string pending: ""
    property string selectedDesign: ""
    property string inputPath: ""
    property string message: ""
    property var summary: ({})
    property var listing: ({})
    property bool referenceBrowser: false
    property real referenceOpacity: .35
    property bool referenceVisible: true
    property var appRemap: ({})
    property string remappingApp: ""
    readonly property var appChoices:visible && view==="appChoices" && session?(session.availableActions || []).filter(a=>a.action && a.action.type==="launch-app"):[]
    readonly property bool previewing: view === "preview" && !!session.proposalModel
    readonly property var designs: visible && view==="designs" && session?session.designChoices():[]
    signal closed()
    signal accepted(string scope)
    signal manageRequested()
    readonly property real textScale:(appearance.typographyScale || 1)*viewportScale
    readonly property real spacing:(appearance.spacing || 16)*viewportScale
    color: appearance.surfaceColor
    border.color: appearance.focusColor; border.width: 2
    function begin() { view="menu";selected=0;scope="page";message="";summary=({});appRemap=({}) }
    function labels() {
        if(view==="menu")return ["Choose a starting or saved design", "Import portable design", "Export draft design", "Reference image", "Manage saved copies", "Back to editing"]
        if(view==="designs")return designs.map(d=>d.name).concat(["Back"])
        if(view==="scope")return ["Current page · keep other pages", "Whole interface · review all changed pages", "Cancel"]
        if(view==="preview")return ["Use this draft", "Cancel preview"].concat((summary.missingApps || []).length?["Remap missing apps…"]:[])
        if(view==="missingApps")return (summary.missingApps || []).concat(["Keep placeholders"])
        if(view==="appChoices")return appChoices.map(a=>a.name).concat(["Cancel"])
        if(view==="browser")return ["Parent folder", "Enter a folder path…"].concat((listing.entries || []).map(e=>(e.folder?"▸ ":"")+e.name)).concat((listing.pages || 1)>1?["Previous entries", "Next entries"]:[]).concat(["Cancel"])
        if(view==="reference")return [session.referenceSource?"Replace reference image":"Choose reference image", "Opacity · "+Math.round(referenceOpacity*100)+"%", referenceVisible?"Hide reference":"Show reference", "Remove reference", "Back"]
        return []
    }
    function chooseScope(action) { pending=action;view="scope";selected=0;message="" }
    function browse(directory, page) {
        const data=referenceBrowser?session.browseArtwork(directory || "",page || 0):session.browseDesignFiles(directory || "",page || 0)
        if(!data || !data.directory){message="This folder is unavailable or has too many entries. Choose a smaller local folder.";return}
        listing=data;view="browser";selected=0;message=""
    }
    function preview() {
        summary=pending==="design"?session.previewDesign(selectedDesign,scope,pageId):session.previewImport(inputPath,scope,pageId,appRemap)
        if(!summary || !summary.valid){message=session.errorString;return}
        view="preview";selected=1
        message="Changed pages: "+(summary.changes || []).map(p=>p.name+" ("+p.change+")").join(", ")
        if(!(summary.changes || []).length)message="No page content changes."
        if((summary.missingApps || []).length)message+=" · Missing apps remain editable: "+summary.missingApps.join(", ")
        message+=". This is a private preview. Use this draft is one undoable change; Review & Apply saves it."
    }
    function activate() {
        if(view==="menu") {
            if(selected===0){view="designs";selected=0;message=""}
            else if(selected===1){referenceBrowser=false;pending="import";browse()}
            else if(selected===2)chooseScope("export")
            else if(selected===3){view="reference";selected=0;message=""}
            else if(selected===4)manageRequested()
            else closed()
        } else if(view==="designs") {
            if(selected>=designs.length){view="menu";selected=0;return}
            selectedDesign=designs[selected].id;chooseScope("design")
        } else if(view==="scope") {
            if(selected===2){view="menu";selected=0;return}
            scope=selected===0?"page":"interface"
            if(pending==="export"){view="exportPath";keyboard.value="";keyboard.selectedIndex=0;message="Enter a new full filename ending in .json. Existing files are kept."}
            else preview()
        } else if(view==="preview") {
            if(selected===0){if(session.acceptPreview())accepted(scope);else message=session.errorString}
            else if(selected===2){view="missingApps";selected=0}
            else {session.cancelPreview();view="scope";selected=0;message=""}
        } else if(view==="missingApps") {
            if(selected<(summary.missingApps || []).length){remappingApp=summary.missingApps[selected];view="appChoices";selected=0}
            else {view="preview";selected=1}
        } else if(view==="appChoices") {
            if(selected<appChoices.length){const next=Object.assign({},appRemap);next[remappingApp]=appChoices[selected].action.appId;appRemap=next;preview()}
            else {view="missingApps";selected=0}
        } else if(view==="browser") {
            if(selected===0){browse(listing.parent);return}
            if(selected===1){view="folderPath";keyboard.value=listing.directory;keyboard.selectedIndex=0;return}
            const entries=listing.entries || [], index=selected-2
            if(index<entries.length){const entry=entries[index];if(entry.folder)browse(entry.path);else if(referenceBrowser){if(session.setReferenceImage(entry.path)){view="reference";selected=0;referenceVisible=true;message="Reference is private to this editor session."}else message=session.errorString}else{inputPath=entry.path;chooseScope("import")};return}
            if((listing.pages || 1)>1 && index===entries.length){browse(listing.directory,Math.max(0,listing.page-1));return}
            if((listing.pages || 1)>1 && index===entries.length+1){browse(listing.directory,Math.min(listing.pages-1,listing.page+1));return}
            view=referenceBrowser?"reference":"menu";selected=0
        } else if(view==="reference") {
            if(selected===0){referenceBrowser=true;browse()}
            else if(selected===1)referenceOpacity=Math.min(.8,Math.round((referenceOpacity+.05)*20)/20)
            else if(selected===2)referenceVisible=!referenceVisible
            else if(selected===3)session.clearReferenceImage()
            else {view="menu";selected=3}
        }
    }
    function handleCommand(command) {
        if(view==="folderPath" || view==="exportPath"){keyboard.handleCommand(command);return}
        if(command===inputManager.back){if(view==="preview")session.cancelPreview();if(view==="menu")closed();else{view="menu";selected=0;message=""};return}
        if(command===inputManager.confirm){activate();return}
        if(view==="reference" && selected===1 && (command===inputManager.navigateLeft || command===inputManager.navigateRight)){referenceOpacity=Math.max(.05,Math.min(.8,Math.round((referenceOpacity+(command===inputManager.navigateLeft?-.05:.05))*20)/20));return}
        if(command===inputManager.navigateUp)selected=Math.max(0,selected-1)
        else if(command===inputManager.navigateDown)selected=Math.min(labels().length-1,selected+1)
    }
    Column {
        anchors.fill: parent;anchors.margins:root.spacing;spacing:root.spacing*.75
        Text {text:root.view==="preview"?"Design preview":root.view==="reference"?"Private reference":"Designs & portability";color:root.appearance.textColor;font.pixelSize:24*root.textScale}
        Text {width:parent.width;text:root.message || (root.view==="scope"?"Choose what this operation affects. Other pages are preserved by default.":root.view==="browser"?root.listing.directory:"Choose an operation. Back returns without applying.");color:root.appearance.mutedColor;wrapMode:Text.WordWrap;font.pixelSize:18*root.textScale}
        ListView {
            width:parent.width;height:Math.max(80,root.height-y-24);clip:true;spacing:6
            model:root.labels();currentIndex:root.selected;highlightRangeMode:ListView.ApplyRange
            delegate:Rectangle {
                required property int index;required property string modelData
                width:ListView.view.width;height:60*root.textScale;color:index===root.selected?root.appearance.selectedColor:"transparent"
                border.width:index===root.selected?2:0;border.color:root.appearance.focusColor
                Accessible.role:Accessible.Button;Accessible.name:modelData
                Text {anchors.fill:parent;anchors.margins:8;verticalAlignment:Text.AlignVCenter;text:modelData;wrapMode:Text.WordWrap;color:root.appearance.textColor;font.pixelSize:18*root.textScale}
                MouseArea {anchors.fill:parent;onClicked:{root.selected=index;root.activate()}}
            }
        }
    }
    OnScreenKeyboard {
        id:keyboard;objectName:"studioDesignPathKeyboard";width:root.width/root.viewportScale;height:root.height/root.viewportScale;scale:root.viewportScale;transformOrigin:Item.TopLeft
        visible:root.view==="folderPath" || root.view==="exportPath";appearance:root.appearance
        onCanceled:{root.view=root.view==="folderPath"?"browser":"scope";root.selected=0}
        onAccepted:text=>{
            if(root.view==="folderPath"){root.browse(text);return}
            if(root.session.exportDesign(text,root.scope,root.pageId)){root.view="menu";root.selected=2;root.message="Portable draft exported. Applied pages and accounts are unchanged."}
            else root.message=root.session.errorString
        }
    }
}
