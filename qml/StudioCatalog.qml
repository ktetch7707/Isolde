import QtQuick

Item {
    id:root
    objectName:"studioCatalog"
    required property var session
    required property var options
    property var provider:null
    required property var appearance
    property string view:"families"
    property int selected:0
    property string family:""
    property var configuration:({})
    property var selectedApps:[]
    property var assets:[]
    property string message:"Choose what to add. Content comes first; placement follows on the full page."
    property string keyboardAction:""
    property string keyboardReturn:""
    readonly property var families:options.catalog || []
    readonly property var actions:(session?session.availableActions:[]).concat([{name:"Play a Kodi movie…",needsMedia:"movies"},{name:"Open a Kodi show…",needsMedia:"shows"}])
    readonly property var apps:actions.filter(a=>a.action && a.action.type==="launch-app" && (a.appType==="web" || a.appType==="desktop"))
    readonly property var mediaChoices:[{name:"Movies · All",kind:"movies",view:"all"},{name:"Movies · Recently watched",kind:"movies",view:"watched"},{name:"Movies · Recently added",kind:"movies",view:"recent"},{name:"Movies · Continue watching",kind:"movies",view:"continue"},{name:"Shows · All",kind:"shows",view:"all"},{name:"Shows · Recently watched",kind:"shows",view:"watched"},{name:"Shows · Recently added",kind:"shows",view:"recent"},{name:"Shows · Continue watching",kind:"shows",view:"continue"}].concat(actions.filter(a=>a.action && a.action.type==="open-collection").map(a=>({name:a.name,kind:a.mediaKind || "movies",view:"collection",value:a.action.collectionId})))
    readonly property real unit:(appearance.typographyScale || 1)*Math.min(width/1280,height/720)
    signal picked(string type,var configuration)
    signal canceled()
    function begin(){view="families";selected=0;configuration=({});message="Choose what to add. Content comes first; placement follows on the full page."}
    function labels(){
        if(view==="widgets")return ["Clock","Text / heading","Section / Group","Input slots · eight editable positions, capture availability required"]
        if(view==="links")return ["None · no action"].concat(actions.map(a=>a.name))
        if(view==="media")return mediaChoices.map(m=>m.name)
        if(view==="apps")return ["All installed apps","Use "+selectedApps.length+" selected apps"].concat(apps.map(a=>(selectedApps.indexOf(a.action.appId)>=0?"✓ ":"□ ")+a.name))
        if(view==="carousel")return ["Start with three editable buttons · links are unassigned","Start with an empty carousel"]
        if(view==="artwork")return ["Import a local PNG / JPEG…"].concat(assets.map(a=>a.name+" · "+a.width+" × "+a.height))
        return []
    }
    function chooseFamily(index){
        const entry=families[index];if(!entry)return;family=entry.id;configuration=({});selected=0
        if(family==="button")view="links"
        else if(family==="image"){assets=session.artworkChoices();view="artwork"}
        else if(family==="app-bar"){selectedApps=apps.slice(0,6).map(a=>a.action.appId);view="apps"}
        else if(family==="library")view="media"
        else if(family==="recently-viewed" || family==="music-library" || family==="tv-guide")finish(family)
        else if(family==="carousel")view="carousel"
        else view="widgets"
    }
    function finish(type){const prototype=session.componentPrototype(type,configuration);if(!prototype.root){message=session.errorString;return}picked(type,configuration)}
    function importArtwork(source){const relative=session.stageArtwork(source);if(!relative){message=session.errorString;return false}configuration=Object.assign({},configuration,{customLogo:relative});view="links";selected=0;message="Image chosen. Decorative images have no remote focus; links make an image interactive.";return true}
    function activate(){
        if(view==="families"){chooseFamily(selected);return}
        if(view==="widgets"){finish(["clock","text","container","input-panel"][selected]);return}
        if(view==="carousel"){if(selected===0)configuration={buttons:options.carouselStart || []};finish("carousel");return}
        if(view==="media"){const source=mediaChoices[selected];configuration={presentation:{libraryKind:source.kind,libraryView:source.view,libraryValue:source.value || ""}};finish("library");return}
        if(view==="apps"){
            if(selected===0){configuration={appCollection:{mode:"all",ids:[]}};finish("app-bar");return}
            if(selected===1){configuration={apps:selectedApps};finish("app-bar");return}
            const id=apps[selected-2].action.appId,next=selectedApps.slice(),index=next.indexOf(id)
            if(index>=0)next.splice(index,1);else if(next.length<128)next.push(id);else{message="Choose up to 128 apps.";return}selectedApps=next;return
        }
        if(view==="artwork"){if(selected===0){beginKeyboard("import","","artwork");return}importArtwork(assets[selected-1].source);return}
        if(view==="links"){
            if(selected===0){configuration=Object.assign({},configuration,{action:{}});finish(family);return}
            const entry=actions[selected-1];if(entry.needsMedia){media.libraryKind=entry.needsMedia;media.selected=0;view="mediaAction";return}configuration=Object.assign({},configuration,{action:entry.action})
            if(entry.needsTarget){beginKeyboard("target","",view);return}
            if(family==="image"){beginKeyboard("imageName","Image link",view);return}finish(family)
        }
    }
    function beginKeyboard(action,value,origin){keyboardAction=action;keyboardReturn=origin;keyboard.value=value;keyboard.selectedIndex=0;view="keyboard"}
    function handleCommand(command){
        if(view==="mediaAction"){media.handleCommand(command);return}
        if(view==="keyboard"){keyboard.handleCommand(command);return}
        if(command===inputManager.back){if(view==="families")canceled();else{view="families";selected=0}return}
        if(command===inputManager.confirm){activate();return}
        const count=view==="families"?families.length:labels().length,step=view==="families"?2:1
        if(command===inputManager.navigateDown)selected=Math.min(count-1,selected+step)
        else if(command===inputManager.navigateUp)selected=Math.max(0,selected-step)
        else if(command===inputManager.navigateRight)selected=Math.min(count-1,selected+1)
        else if(command===inputManager.navigateLeft)selected=Math.max(0,selected-1)
    }
    Rectangle {
        anchors.centerIn:parent;width:Math.min(parent.width-48,900*root.unit);height:Math.min(parent.height-48,630*root.unit)
        color:root.appearance.surfaceColor;radius:20*root.unit;border.width:2;border.color:root.appearance.focusColor
        Column {
            anchors.fill:parent;anchors.margins:24*root.unit;spacing:14*root.unit
            Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";text:root.view==="families"?"Add component":root.view==="widgets"?"Widgets":root.view==="links"?"On select":root.view==="media"?"Choose Kodi content":root.view==="apps"?"Choose installed apps":root.view==="artwork"?"Choose artwork":"Button carousel";color:root.appearance.textColor;font.pixelSize:28*root.unit}
            Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";width:parent.width;text:root.message;color:root.appearance.mutedColor;font.pixelSize:17*root.unit;wrapMode:Text.Wrap}
            GridView {
                id:grid;width:parent.width;height:parent.height-120*root.unit;visible:root.view==="families";clip:true
                cellWidth:width/2;cellHeight:height/3;model:root.families;currentIndex:root.selected
                delegate:Rectangle {
                    required property int index;required property var modelData
                    width:grid.cellWidth-12*root.unit;height:grid.cellHeight-12*root.unit;radius:12*root.unit
                    color:index===root.selected?root.appearance.selectedColor:root.appearance.surfaceColor;border.width:index===root.selected?3:1;border.color:root.appearance.focusColor
                    Row {
                        anchors.fill:parent;anchors.margins:14*root.unit;spacing:16*root.unit
                        Rectangle {width:90*root.unit;height:90*root.unit;anchors.verticalCenter:parent.verticalCenter;radius:14*root.unit;color:root.appearance.selectedColor;LineIcon {anchors.centerIn:parent;width:48*root.unit;height:width;name:modelData.icon;ink:root.appearance.textColor}}
                        Column {width:parent.width-106*root.unit;anchors.verticalCenter:parent.verticalCenter;spacing:7*root.unit
                            Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";width:parent.width;text:modelData.name;wrapMode:Text.Wrap;color:root.appearance.textColor;font.pixelSize:21*root.unit;font.bold:true}
                            Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";width:parent.width;text:modelData.description;wrapMode:Text.Wrap;color:root.appearance.mutedColor;font.pixelSize:15*root.unit}
                        }
                    }
                    Accessible.role:Accessible.Button;Accessible.name:modelData.name;Accessible.description:modelData.description
                    MouseArea {anchors.fill:parent;onClicked:{root.selected=index;root.activate()}}
                }
            }
            ListView {
                width:parent.width;height:parent.height-120*root.unit;visible:root.view!=="families" && root.view!=="keyboard";clip:true
                model:root.labels();currentIndex:root.selected;highlightRangeMode:ListView.ApplyRange;spacing:6*root.unit
                delegate:Rectangle {
                    required property int index;required property string modelData
                    width:ListView.view.width;height:48*root.unit;radius:8*root.unit;color:index===root.selected?root.appearance.selectedColor:"transparent";border.width:index===root.selected?2:0;border.color:root.appearance.focusColor
                    Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";anchors.fill:parent;anchors.margins:10*root.unit;text:modelData;color:root.appearance.textColor;font.pixelSize:19*root.unit;elide:Text.ElideRight}
                    Accessible.role:Accessible.Button;Accessible.name:modelData
                    MouseArea {anchors.fill:parent;onClicked:{root.selected=index;root.activate()}}
                }
            }
        }
    }
    StudioMediaPicker {id:media;collections:root.actions.filter(a=>a.action && a.action.type==="open-collection");anchors.fill:parent;visible:root.view==="mediaAction";provider:root.provider;appearance:root.appearance;onCanceled:root.view="links";onPicked:action=>{root.configuration=Object.assign({},root.configuration,{action:action});if(root.family==="image")root.beginKeyboard("imageName","Image link","links");else root.finish(root.family)}}
    OnScreenKeyboard {
        id:keyboard;anchors.fill:parent;visible:root.view==="keyboard";appearance:root.appearance;hint:root.message
        onCanceled:root.view=root.keyboardReturn
        onAccepted:text=>{
            if(root.keyboardAction==="import"){root.importArtwork(text);return}
            if(root.keyboardAction==="imageName"){root.configuration=Object.assign({},root.configuration,{name:text});root.finish("image");return}
            root.configuration=Object.assign({},root.configuration,{action:Object.assign({},root.configuration.action,{target:text})})
            if(root.family==="image")root.beginKeyboard("imageName","Image link","links");else root.finish(root.family)
        }
    }
}
