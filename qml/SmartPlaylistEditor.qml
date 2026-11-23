import QtQuick

Rectangle {
    id: root
    objectName: "smartPlaylistEditor"
    required property var client
    property var appearance: ({})
    property string filename: ""
    property string screen: "draft"
    property int selected: 0
    property int ruleIndex: 0
    property bool loading: false
    property bool saving: false
    property string message: ""
    property var draft: ({})
    signal closed(bool saved)
    SurfacePalette { id: theme; appearance: root.appearance }
    color: theme.surface
    readonly property var fields: [
        {id:"year", label:qsTr("Release year"), operators:["is","greaterthan","lessthan"], value:"2000"},
        {id:"title", label:qsTr("Title"), operators:["contains","is","startswith"], value:""},
        {id:"genre", label:qsTr("Genre"), operators:["is","isnot"], value:""},
        {id:"tag", label:qsTr("Tag"), operators:["is","isnot"], value:""},
        {id:"playcount", label:qsTr("Times played"), operators:["is","greaterthan","lessthan"], value:"0"},
        {id:"inprogress",label:qsTr("In progress"),operators:["true","false"],value:""},
        {id:"lastplayed",label:qsTr("Last played"),operators:["after","before"],value:"2000-01-01"}
    ]
    readonly property var operatorLabels: ({is:qsTr("is"),isnot:qsTr("is not"),greaterthan:qsTr("is greater than"),
        lessthan:qsTr("is less than"),contains:qsTr("contains"),startswith:qsTr("starts with"),
        "true":qsTr("yes"),"false":qsTr("no"),after:qsTr("is after"),before:qsTr("is before")})
    readonly property var sortKeys: ["title","year","dateadded","lastplayed"]
    readonly property var sortLabels: [qsTr("Title"),qsTr("Release year"),qsTr("Date added"),qsTr("Last played")]
    readonly property var rules: draft.rules || []
    readonly property var rule: rules[ruleIndex] || ({})
    readonly property var currentField: fields.find(field=>field.id===rule.field) || fields[0]

    // Load an existing definition or start a private new draft.
    function begin(name, kind) {
        filename=name || "";message="";selected=0;screen="draft";saving=false;loading=!!filename
        draft={title:qsTr("New playlist"),kind:kind==="shows"?"shows":"movies",match:"all",rules:[],sort:"title",direction:"ascending"}
        if(loading)client.loadSmartPlaylist(filename)
    }
    // Merge a single property without mutating the model supplied by the client.
    function change(key,value) {draft=Object.assign({},draft,{[key]:value});message=""}
    // Keep rule edits independent until Save succeeds.
    function patchRule(changes) {
        const next=rules.slice()
        next[ruleIndex]=Object.assign({},rule,changes)
        change("rules",next)
    }
    // Put inclusive release decades into Kodi's strict greater-than and less-than rules.
    function addDecade(year) {
        change("rules",rules.concat([{field:"year",operator:"greaterthan",value:String(year-1)},
            {field:"year",operator:"lessthan",value:String(year+10)}]))
        if(draft.title===qsTr("New playlist"))change("title",year+qsTr("s"))
        change("match","all")
        screen="draft";selected=0
    }
    // Show only actions that belong to the current step of the editor.
    function rows() {
        if(loading)return [qsTr("Loading playlist…")]
        if(screen==="unavailable")return [qsTr("Back")]
        if(screen==="decade")return Array.from({length:25},(_,index)=>(1900+index*10)+qsTr("s"))
        if(screen==="field")return fields.map(field=>field.label)
        if(screen==="operator")return currentField.operators.map(operator=>operatorLabels[operator])
        if(screen==="preview")return [qsTr("Back to rules")].concat((client.playlistPreview || []).map(item=>item.title))
        if(screen==="rule")return [qsTr("Field · ")+currentField.label,qsTr("Comparison · ")+operatorLabels[rule.operator],
            rule.field==="inprogress"?qsTr("Value · Set by comparison"):qsTr("Value · ")+(rule.value || qsTr("Choose a value")),qsTr("Remove rule"),qsTr("Back to rules")]
        return [qsTr("Name · ")+draft.title,qsTr("Match · ")+(draft.match==="all"?qsTr("All rules"):qsTr("Any rule")),
            qsTr("Add release decade…"),qsTr("Add rule…")]
            .concat(rules.map(item=>fields.find(field=>field.id===item.field).label+" "+operatorLabels[item.operator]+" "+item.value),
            [qsTr("Sort by · ")+sortLabels[sortKeys.indexOf(draft.sort)],qsTr("Direction · ")+(draft.direction==="ascending"?qsTr("Ascending"):qsTr("Descending")),
                qsTr("Preview matching titles"),qsTr("Save smart playlist"),qsTr("Cancel")])
    }
    // Validate required values before asking the library to preview or save.
    function validDraft() {
        if(!draft.title.trim() || rules.some(item=>item.field!=="inprogress" && !item.value.trim())){
            message=qsTr("Enter a name and fill in every rule before previewing or saving.")
            return false
        }
        return true
    }
    // Return to the appropriate parent without writing an unfinished draft.
    function back() {
        if(saving)return
        if(screen==="field" || screen==="operator"){screen="rule";selected=0}
        else if(screen!=="draft" && screen!=="unavailable"){screen="draft";selected=0}
        else closed(false)
    }
    // Use the same actions for D-pad confirmation and pointer selection.
    function activate() {
        if(loading || client.busy)return
        if(screen==="unavailable"){back();return}
        if(screen==="decade"){addDecade(1900+selected*10);return}
        if(screen==="field"){
            const field=fields[selected];patchRule({field:field.id,operator:field.operators[0],value:field.value})
            screen="rule";selected=0;return
        }
        if(screen==="operator"){patchRule({operator:currentField.operators[selected]});screen="rule";selected=1;return}
        if(screen==="preview"){if(selected===0)back();return}
        if(screen==="rule"){
            if(selected===0){screen="field";selected=fields.indexOf(currentField)}
            else if(selected===1){screen="operator";selected=currentField.operators.indexOf(rule.operator)}
            else if(selected===2){if(rule.field!=="inprogress"){keyboard.value=rule.value;keyboard.selectedIndex=0;screen="value"}}
            else if(selected===3){change("rules",rules.filter((_,index)=>index!==ruleIndex));back()}
            else back()
            return
        }
        if(selected===0){keyboard.value=draft.title;keyboard.selectedIndex=0;screen="name"}
        else if(selected===1)change("match",draft.match==="all"?"one":"all")
        else if(selected===2){if(rules.length<=14){screen="decade";selected=8}else message=qsTr("Remove rules before adding another decade.")}
        else if(selected===3){
            if(rules.length>=16){message=qsTr("Use up to 16 rules.");return}
            ruleIndex=rules.length;change("rules",rules.concat([{field:"year",operator:"is",value:"2000"}]));screen="rule";selected=0
        } else if(selected<4+rules.length){ruleIndex=selected-4;screen="rule";selected=0}
        else {
            const option=selected-4-rules.length
            if(option===0)change("sort",sortKeys[(sortKeys.indexOf(draft.sort)+1)%sortKeys.length])
            else if(option===1)change("direction",draft.direction==="ascending"?"descending":"ascending")
            else if(option===2 && validDraft()){screen="preview";selected=0;client.previewSmartPlaylist(draft)}
            else if(option===3 && validDraft()){saving=true;client.saveSmartPlaylist(filename,draft)}
            else if(option===4)back()
        }
    }
    // Keep Back and Confirm working from every editor screen.
    function handleCommand(command) {
        if(screen==="name" || screen==="value"){keyboard.handleCommand(command);return}
        if(command===inputManager.back){back();return}
        if(command===inputManager.confirm){activate();return}
        if(command===inputManager.navigateUp)selected=Math.max(0,selected-1)
        else if(command===inputManager.navigateDown)selected=Math.min(rows().length-1,selected+1)
    }
    Connections {
        target:root.client
        function onChanged(){
            if(!root.visible || root.client.busy)return
            if(root.loading){
                root.loading=false
                if(root.client.smartPlaylist && root.client.smartPlaylist.title)root.draft=root.client.smartPlaylist
                else root.screen="unavailable"
            }
            // Create acknowledgements emit changed before playlistSaved in the same event turn.
            if(root.saving)Qt.callLater(()=>{if(root.visible && !root.client.busy)root.saving=false})
        }
        function onPlaylistSaved(name){if(root.visible && root.saving)root.closed(true)}
    }
    MouseArea {anchors.fill:parent}
    Column {
        anchors.fill:parent;anchors.margins:25;spacing:12
        Text {text:qsTr("Smart playlist");color:theme.text;font.pixelSize:28}
        ListView {
            id: list
            width:parent.width;height:Math.max(100,parent.height-95);clip:true;spacing:5
            model:root.rows();currentIndex:root.selected;highlightRangeMode:ListView.ApplyRange
            delegate:Rectangle {
                required property int index
                required property string modelData
                width:list.width;height:45;radius:theme.radius;color:index===root.selected?theme.selected:theme.surface
                Text {anchors.left:parent.left;anchors.leftMargin:12;anchors.verticalCenter:parent.verticalCenter
                    width:parent.width-24;elide:Text.ElideRight;textFormat:Text.PlainText;text:modelData;color:theme.text;font.pixelSize:18}
                MouseArea {anchors.fill:parent;onClicked:{root.selected=index;root.activate()}}
            }
        }
        Text {width:parent.width;wrapMode:Text.WordWrap;textFormat:Text.PlainText;color:theme.muted;font.pixelSize:16
            text:root.message || root.client.playlistStatus || qsTr("Back cancels the draft.")}
    }
    OnScreenKeyboard {
        id:keyboard;anchors.fill:parent;appearance:root.appearance
        visible:root.screen==="name" || root.screen==="value";hint:root.message
        onAccepted:text=>{
            const value=text.trim()
            if(!value || value.length>(root.screen==="name"?80:160)){root.message=qsTr("Enter a valid value.");return}
            if(root.screen==="name"){root.change("title",value);root.screen="draft";root.selected=0}
            else {root.patchRule({value:value});root.screen="rule";root.selected=2}
        }
        onCanceled:root.screen=root.screen==="name"?"draft":"rule"
    }
}
