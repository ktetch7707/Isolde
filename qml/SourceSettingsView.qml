import QtQuick

Item {
    id: root; objectName: "sourceSettings"
    property var appearance: ({})
    property var manager: typeof sourceManager!=="undefined"?sourceManager:null
    property var layoutModel: typeof homeLayout!=="undefined"?homeLayout:null
    property var dispatcher: typeof actionDispatcher!=="undefined"?actionDispatcher:null
    property int selectedIndex: 0
    property string view: "list"
    readonly property bool editing: view!=="list"
    property bool showHidden: false
    property var draft: ({})
    property var activeField: ({})
    property string sourceId: ""
    property string message: ""
    property string selectedSourceId: ""
    property bool policyOnly: false
    signal policyAccepted(var policy)
    signal policyCancelled()
    SurfacePalette { id: shell; appearance: root.appearance }
    Rectangle { anchors.fill:parent; color:shell.surface; z:-1 }
    readonly property var fields: layoutModel?(layoutModel.studioOptions.sourcePolicyFields || []):[]
    readonly property string policyNotice:!manager?"":manager.policy.mode==="off"?"Page notices are off. The complete source list remains available here.":!(manager.policy.pages || []).length?"No pages allow notices. Choose allowed pages in Display policy.":!fieldOptions({options:"slots"}).length?"No declared source slot exists. Add one in Studio; sources remain available here.":""
    readonly property var currentSources: manager?(showHidden?manager.hiddenSources:manager.sources):[]
    readonly property var source: manager?(manager.sources.concat(manager.hiddenSources).find(s=>s.id===sourceId) || null):null
    readonly property var rows: {
        if(view==="policy")return fields.map(f=>({kind:"field",name:f.label,detail:fieldSummary(f),field:f})).concat([{kind:"save",name:"Save display policy",detail:"Apply all displayed policy choices together."},{kind:"cancel",name:"Cancel policy changes",detail:"Keep the applied policy unchanged."}])
        if(view==="options")return fieldOptions(activeField).map(o=>({kind:"option",name:(activeField.multiple?((draft[activeField.key] || []).indexOf(o.value)>=0?"✓ ":"□ "):"")+o.name,value:o.value})).concat([{kind:"optionsBack",name:"Done choosing",detail:"Return to the policy draft; Save applies it."}])
        if(view==="keep")return [{kind:"keep",name:"Keep as a normal Home tile",detail:"Save a permanent shortcut before consuming this notice. No media is copied or launched."},{kind:"actionsBack",name:"Cancel",detail:"Keep source and layout unchanged."}]
        if(view==="handlers")return (manager?manager.handlers(sourceId):[]).map(a=>({kind:"handler",name:a.name,value:a.id})).concat([{kind:"actionsBack",name:"Cancel handler choice"}])
        if(view==="actions")return showHidden?[{kind:"restore",name:"Restore source",detail:"Make this detected source visible again."},{kind:"listBack",name:"Back to Sources"}]:[
            {kind:"open",name:"Open source",detail:source?source.unavailableReason || "Use the selected installed handler.":"This source is disconnected."},
            {kind:"confirmKeep",name:"Keep as a normal tile…",detail:"Review before adding a permanent shortcut."},
            {kind:"dismiss",name:"Dismiss until disconnect",detail:"Leave files, drives and layout unchanged."},
            {kind:"hide",name:"Hide until restored",detail:"Restore it later from Show hidden sources."},
            {kind:"handlers",name:"Choose handler…",detail:"Choose an installed compatible app."},
            {kind:"listBack",name:"Back to Sources"}]
        return [{kind:"policy",name:"Display policy",detail:"Choose discovery, pages, slots, events, duration and suppression."},{kind:"refresh",name:"Refresh detected sources",detail:"Scan only the existing permitted local roots."},{kind:"hidden",name:showHidden?"Show current sources":"Show hidden sources",detail:"Hiding never deletes or unmounts media."}].concat(currentSources.map(s=>({kind:"source",id:s.id,name:s.name,detail:s.unavailableReason || s.target})))
    }
    function fieldOptions(field) {
        let options=field.choices || []
        if(field.options==="pages")options=[{name:"Designated Home",value:"home"}].concat(layoutModel?layoutModel.studioPages.map(p=>({name:p.name,value:p.id})):[])
        if(field.options==="slots"){
            options=[];const add=(id,name)=>{if(!options.some(o=>o.value===id))options.push({name:name,value:id})}
            if(layoutModel)for(const page of layoutModel.studioPages){
                const model=page.id?layoutModel.pageModel(page.id):layoutModel;if(!model)continue
                if((model.composition.headerControls || []).indexOf("sources")>=0)add("header-sources","Home status / header slot")
                for(const item of model.components || model.items || [])if(item.family==="source-notice" || item.family==="detected-sources")add(item.id,page.name+" · "+item.name)
            }
        }
        return options
    }
    function fieldSummary(field) {
        const value=draft[field.key]
        if(field.multiple)return (value || []).length?((value || []).map(v=>(fieldOptions(field).find(o=>o.value===v) || {name:v+" (missing)"}).name).join(", ")):(field.key==="slots"?"Any declared source slot":"None")
        return (fieldOptions(field).find(o=>o.value===value) || {name:String(value)}).name
    }
    function returnToList() {
        if(policyOnly){policyCancelled();return}
        view="list";draft=({});message=""
        const index=rows.findIndex(row=>row.id===sourceId);selectedIndex=index>=0?index:Math.min(selectedIndex,rows.length-1)
    }
    function activate() {
        const row=rows[selectedIndex];if(!row || (!manager && !policyOnly))return
        message=""
        if(row.kind==="policy"){draft=Object.assign({},manager.policy);view="policy";selectedIndex=0}
        else if(row.kind==="field"){activeField=row.field;view="options";selectedIndex=0}
        else if(row.kind==="option"){
            let next=Object.assign({},draft)
            if(activeField.multiple){let values=(next[activeField.key] || []).slice(),index=values.indexOf(row.value);if(index>=0)values.splice(index,1);else values.push(row.value);next[activeField.key]=values}
            else next[activeField.key]=row.value
            draft=next
        } else if(row.kind==="optionsBack"){view="policy";selectedIndex=Math.max(0,fields.findIndex(f=>f.key===activeField.key))}
        else if(row.kind==="save"){if(policyOnly){policyAccepted(draft);return}if(manager.setPolicy(draft))returnToList();else message=manager.errorString}
        else if(row.kind==="cancel" || row.kind==="listBack")returnToList()
        else if(row.kind==="refresh")manager.refresh()
        else if(row.kind==="hidden"){showHidden=!showHidden;selectedIndex=2}
        else if(row.kind==="source"){sourceId=row.id;view="actions";selectedIndex=0}
        else if(row.kind==="confirmKeep"){view="keep";selectedIndex=0}
        else if(row.kind==="handlers"){view="handlers";selectedIndex=0}
        else if(row.kind==="actionsBack"){view="actions";selectedIndex=0}
        else if(row.kind==="handler"){if(manager.chooseHandler(sourceId,row.value)){view="actions";selectedIndex=0}else message=manager.errorString}
        else if(row.kind==="open"){
            const action=manager.sourceAction(sourceId)
            if(!action.type){message="This source is disconnected or has no installed handler.";return}
            if(dispatcher && !dispatcher.dispatch(action,"source-"+sourceId))message=dispatcher.errorString || "Could not open this source. Retry or choose another handler."
        } else {
            const ok=row.kind==="hide"?manager.hideSource(sourceId):row.kind==="dismiss"?manager.dismissSource(sourceId):row.kind==="restore"?manager.forgetSourceDecision(sourceId):row.kind==="keep"?manager.promoteSource(sourceId):false
            if(ok)returnToList();else message=manager.errorString
        }
    }
    function handleCommand(command) {
        if(command===inputManager.back){if(view==="options"){view="policy";selectedIndex=0}else if(view==="handlers" || view==="keep"){view="actions";selectedIndex=0}else returnToList();return}
        if(command===inputManager.navigateUp)selectedIndex=Math.max(0,selectedIndex-1)
        else if(command===inputManager.navigateDown)selectedIndex=Math.min(rows.length-1,selectedIndex+1)
        else if(command===inputManager.confirm || command===inputManager.menu)activate()
    }
    onSelectedIndexChanged:{if(view==="list"){const row=rows[selectedIndex];selectedSourceId=row && row.kind==="source"?row.id:""}list.positionViewAtIndex(selectedIndex,ListView.Contain)}
    onVisibleChanged:if(visible && manager && manager.requestedSourceId){const id=manager.requestedSourceId;manager.requestSource("");showHidden=false;sourceId=id;view="actions";selectedIndex=0}
    Connections {target:root.manager;function onSourcesChanged(){if(root.view!=="list")return;const index=root.rows.findIndex(r=>r.id===root.selectedSourceId);if(index>=0)root.selectedIndex=index;else root.selectedIndex=Math.min(root.selectedIndex,root.rows.length-1)}}
    Column {
        anchors.fill:parent;spacing:12*shell.gapScale
        Text {width:parent.width;text:root.view==="policy" || root.view==="options"?"Source display policy · draft":root.view==="list"?"Sources":root.source?root.source.name:"Source disconnected";textFormat:Text.PlainText;color:shell.text;font.family:shell.fontFamily;font.pixelSize:28*shell.textScale;wrapMode:Text.Wrap}
        Text {width:parent.width;text:root.message || (root.manager?root.manager.errorString:"") || (root.editing?"Confirm chooses · Back cancels this screen. Policy changes are private until Save.":root.policyNotice || "Confirm / Menu opens source actions. Sources never change your media or Kodi library.");textFormat:Text.PlainText;color:shell.muted;font.family:shell.fontFamily;font.pixelSize:16*shell.textScale;wrapMode:Text.Wrap}
        ListView {
            id:list;objectName:"sourceRows";width:parent.width;height:Math.max(0,root.height-y);clip:true;spacing:6*shell.gapScale;model:root.rows
            delegate:Rectangle {
                required property var modelData;required property int index
                width:list.width;height:Math.max(72,description.implicitHeight+title.implicitHeight+24)*shell.textScale;radius:shell.radius
                color:root.selectedIndex===index?shell.selected:shell.elevated;border.width:root.selectedIndex===index?3:1;border.color:root.selectedIndex===index?shell.selectedFocus:shell.outline
                Accessible.role:Accessible.Button;Accessible.name:modelData.name;Accessible.description:modelData.detail || ""
                Column {anchors.fill:parent;anchors.margins:10;spacing:4
                    Text {id:title;width:parent.width;text:modelData.name;textFormat:Text.PlainText;color:root.selectedIndex===index?shell.selectedText:shell.text;font.family:shell.fontFamily;font.pixelSize:20*shell.textScale;wrapMode:Text.Wrap}
                    Text {id:description;width:parent.width;text:modelData.detail || "";textFormat:Text.PlainText;color:root.selectedIndex===index?shell.selectedText:shell.muted;font.family:shell.fontFamily;font.pixelSize:14*shell.textScale;wrapMode:Text.Wrap}
                }
                MouseArea {anchors.fill:parent;onClicked:{root.selectedIndex=index;root.activate()}}
            }
        }
    }
}
