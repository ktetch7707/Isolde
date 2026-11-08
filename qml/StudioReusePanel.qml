import QtQuick

Rectangle {
    id: root
    objectName: "studioReusePanel"
    property var session
    property real viewportScale: 1
    property var appearance: ({})
    property var defaults: ({})
    SystemPalette {id:systemPalette}
    property string pageId: ""
    property string selectedId: ""
    property string categoryId: ""
    property var selectedPresentation: ({})
    property string view: "menu"
    property int selected: 0
    property string message: ""
    property string pending: ""
    property string libraryId: ""
    property string copiedId: ""
    property string copiedPage: ""
    property var summary: ({})
    property int policy: Math.max(0,policies.indexOf(defaults.policy || "wrap"))
    property int minimumWidth: defaults.minimumWidth || 16
    property int minimumHeight: defaults.minimumHeight || 16
    property int aspectIndex: Math.max(0,aspects.indexOf(defaults.aspect || 16/9))
    readonly property var policies: ["wrap", "scroll", "shrink", "stack"]
    readonly property var aspects: [16/9, 1, 2/3, 4/3]
    readonly property var entries: visible && session && (view==="styles" || view==="components") ? session.reusableChoices(view) : []
    readonly property real textScale: (appearance.typographyScale || 1)*viewportScale
    signal closed()
    signal accepted()
    signal viewportRequested(real ratio)
    color: appearance.surfaceColor || systemPalette.window
    border.width: 2
    border.color: appearance.focusColor || systemPalette.highlight
    onVisibleChanged: if(visible){view="menu";selected=0;message=""}
    function labels() {
        if(view==="menu")return ["Save appearance as shared style", "Choose shared style", "Edit shared style for all instances", "Reset component overrides", "Detach shared style", "Copy appearance", "Paste copied appearance", "Save to My components", "Insert independent copy", "Insert linked instance", "Edit all linked instances", "Detach this instance", "Responsive section rules", "Back"]
        if(view==="styles" || view==="components")return entries.map(e=>e.name).concat(["Back"])
        if(view==="review")return ["Accept one draft change", "Cancel"].concat((summary.changes || []).map(e=>(e.page || "Home")+" · "+(e.name || e.id || "Definition")+" · "+e.change))
        if(view==="responsive")return ["Policy: "+policies[policy], "Minimum width: "+minimumWidth, "Minimum height: "+minimumHeight, "Card aspect: "+aspects[aspectIndex].toFixed(2), "Preview wide viewport", "Preview square viewport", "Preview tall viewport", "Review rule", "Back"]
        return []
    }
    function propose(operation, arguments) {
        summary=session.previewReuse(operation,arguments,pageId)
        if(!summary.valid){message=session.errorString;return}
        message="Review "+(summary.changes || []).length+" affected definitions/instances below. Appearance changes preserve content, sources, layout and placement."
        view="review";selected=1
    }
    function choose(kind, operation){pending=operation;view=kind;selected=0;message=entries.length?"Choose a named definition.":"Save a definition first. Back keeps your draft."}
    function activate() {
        if(view==="review") {
            if(selected===0){if(session.acceptPreview()){view="menu";selected=0;message="Draft updated. Undo restores this complete change; Apply saves it.";accepted()}else message=session.errorString}
            else if(selected===1){session.cancelPreview();view="menu";selected=0}
            return
        }
        if(view==="styles" || view==="components") {
            if(selected>=entries.length){view="menu";selected=0;return}
            libraryId=entries[selected].id
            if(pending==="editStyle")propose(pending,{id:selectedId,libraryId:libraryId,presentation:selectedPresentation})
            else propose(pending==="independent" || pending==="linked"?"insert":pending,{id:selectedId,libraryId:libraryId,linked:pending==="linked"})
            return
        }
        if(view==="responsive") {
            if(selected===0)policy=(policy+1)%policies.length
            else if(selected===1)minimumWidth=minimumWidth>=320?32:minimumWidth+16
            else if(selected===2)minimumHeight=minimumHeight>=240?32:minimumHeight+16
            else if(selected===3)aspectIndex=(aspectIndex+1)%aspects.length
            else if(selected>=4 && selected<=6)viewportRequested([16/9,1,3/4][selected-4])
            else if(selected===7)propose("responsive",{categoryId:categoryId,rule:{schemaVersion:1,policy:policies[policy],minimumWidth:minimumWidth,minimumHeight:minimumHeight,aspect:aspects[aspectIndex]}})
            else {view="menu";selected=12;viewportRequested(0)}
            return
        }
        if(selected===0 || selected===7){pending=selected===0?"saveStyle":"saveComponent";view="name";keyboard.value="";keyboard.selectedIndex=0;return}
        if(selected===1){choose("styles","useStyle");return}
        if(selected===2){choose("styles","editStyle");message="Use this component's current appearance as the shared style. Explicit overrides on other instances remain.";return}
        if(selected===3){propose("resetOverrides",{id:selectedId});return}
        if(selected===4){propose("detachStyle",{id:selectedId});return}
        if(selected===5){copiedId=selectedId;copiedPage=pageId;message=copiedId?"Appearance copied for this editor session; content and placement are excluded.":"Select a component first.";return}
        if(selected===6){propose("pasteAppearance",{id:selectedId,sourceId:copiedId,sourcePage:copiedPage});return}
        if(selected===8 || selected===9){choose("components",selected===8?"independent":"linked");return}
        if(selected===10){propose("editAll",{id:selectedId});return}
        if(selected===11){propose("detach",{id:selectedId});return}
        if(selected===12){view="responsive";selected=0;message="Flow sections only. Placement grids are preserved. Alternate viewports never change saved geometry.";return}
        viewportRequested(0);closed()
    }
    function handleCommand(command) {
        if(view==="name"){keyboard.handleCommand(command);return}
        if(command===inputManager.back){if(view==="review")session.cancelPreview();if(view==="menu"){viewportRequested(0);closed()}else {view="menu";selected=0;viewportRequested(0)};return}
        if(command===inputManager.navigateUp)selected=Math.max(0,selected-1)
        else if(command===inputManager.navigateDown)selected=Math.min(labels().length-1,selected+1)
        else if(command===inputManager.confirm)activate()
    }
    Column {
        anchors.fill:parent;anchors.margins:18;spacing:12
        Text {width:parent.width;text:"Shared styles & My components";color:root.appearance.textColor || systemPalette.windowText;font.pixelSize:24*root.textScale;wrapMode:Text.Wrap}
        Text {id:explanation;width:parent.width;text:root.message;textFormat:Text.PlainText;color:root.appearance.mutedColor || systemPalette.mid;font.pixelSize:16*root.textScale;wrapMode:Text.Wrap}
        ListView {
            width:parent.width;height:Math.max(48,root.height-explanation.height-110);clip:true
            model:root.labels();currentIndex:root.selected;highlightRangeMode:ListView.ApplyRange
            delegate:Rectangle {
                required property int index;required property string modelData
                width:ListView.view.width;height:Math.max(48,label.implicitHeight+18)
                color:root.selected===index?root.appearance.selectedColor || systemPalette.highlight:"transparent"
                border.width:root.selected===index?2:0;border.color:root.appearance.focusColor || systemPalette.highlight
                Text {id:label;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:9;text:modelData;textFormat:Text.PlainText;wrapMode:Text.Wrap;color:root.appearance.textColor || systemPalette.windowText;font.pixelSize:18*root.textScale}
                MouseArea {anchors.fill:parent;onClicked:{root.selected=index;root.activate()}}
            }
        }
    }
    OnScreenKeyboard {
        id:keyboard;width:root.width/root.viewportScale;height:root.height/root.viewportScale;scale:root.viewportScale;transformOrigin:Item.TopLeft;visible:root.view==="name";appearance:root.appearance;hint:"Name this saved definition"
        onCanceled:{root.view="menu";root.selected=0}
        onAccepted:text=>root.propose(root.pending,{id:root.selectedId,name:text})
    }
}
