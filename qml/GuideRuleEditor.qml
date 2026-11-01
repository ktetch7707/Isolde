import QtQuick

Item {
    id:root
    required property var appearance
    required property var options
    property var tree:({field:"source",op:"is",value:"kodi"})
    property var path:[]
    property int selectedIndex:0
    property bool typing:false
    property var editingRow:null
    property var typingBaseline:null
    property string message:""
    readonly property real fontSize:options.labelSize*(appearance.typographyScale || 1)
    readonly property var fields:["source","mediaType","title","showtitle","studio","network","genre","tag","year","rating","playcount","duration","language","actor","director","path","season","episode"]
    readonly property var node:nodeAt(tree,path)
    readonly property bool group:node.op==="all" || node.op==="any" || node.op==="not"
    readonly property bool numeric:["year","rating","playcount","duration","season","episode"].indexOf(node.field)>=0
    readonly property var operators:numeric?["is","is-not","before","after","in-range","one-of","missing","present"]:["is","is-not","contains","does-not-contain","starts-with","one-of","missing","present"]
    readonly property var rows:buildRows()
    signal accepted(var rule)
    signal canceled()
    function nodeAt(data,where){let result=data;for(const i of where)result=result.rules[i];return result}
    function replaceNode(value){let copy=JSON.parse(JSON.stringify(tree));if(!path.length)copy=value;else nodeAt(copy,path.slice(0,-1)).rules[path[path.length-1]]=value;tree=copy}
    function begin(value){tree=JSON.parse(JSON.stringify(value));path=[];selectedIndex=0;typing=false}
    function buildRows(){
        let result=[{key:"kind",label:"Rule type: "+(group?node.op==="all"?"Match all":node.op==="any"?"Match any":"Exclude":"Condition")}]
        if(group){
            for(let i=0;i<(node.rules || []).length;i++){const child=node.rules[i];result.push({key:"child",child:i,label:(child.field || (child.op==="all"?"Match all":child.op==="any"?"Match any":"Exclude"))+" · "+(child.value===undefined?child.op:String(child.value))})}
            if(node.op!=="not" || !node.rules.length){result.push({key:"addLeaf",label:"Add condition"});result.push({key:"addGroup",label:"Add group"})}
        }else{
            result.push({key:"field",label:"Field: "+node.field});result.push({key:"op",label:"Operator: "+node.op})
            if(node.op!=="missing" && node.op!=="present"){
                if(Array.isArray(node.value)){
                    for(let i=0;i<node.value.length;i++){
                        result.push({key:"value",valueIndex:i,label:(node.op==="in-range"?(i===0?"Minimum":"Maximum"):"Value "+(i+1))+": "+String(node.value[i])})
                        if(node.op==="one-of" && node.value.length>1)result.push({key:"removeValue",valueIndex:i,label:"Remove value "+(i+1)})
                    }
                    if(node.op==="one-of" && node.value.length<32)result.push({key:"addValue",label:"Add value"})
                }else result.push({key:"value",label:"Value: "+String(node.value)})
            }
        }
        if(path.length){result.push({key:"up",label:"Back to parent group"});result.push({key:"remove",label:"Remove this condition/group"})}
        result.push({key:"done",label:"Use these rules"});result.push({key:"cancel",label:"Cancel rule changes"});return result
    }
    function cycle(values,value,delta){return values[(Math.max(0,values.indexOf(value))+delta+values.length)%values.length]}
    function invoke(delta){
        const row=rows[selectedIndex];let value=JSON.parse(JSON.stringify(node));delta=delta || 1
        if(row.key==="kind"){
            const kind=cycle(["condition","all","any","not"],group?node.op:"condition",delta)
            if(kind==="condition" && group && node.rules.length){message="Remove the group's child rules before changing it to a condition.";return}
            if(kind==="not" && group && node.rules.length>1){message="Exclude accepts one child. Keep the group or remove extra children explicitly.";return}
            message=""
            if(kind==="condition")value={field:"studio",op:"contains",value:""}
            else value={op:kind,rules:group?node.rules:[value]}
            replaceNode(value)
        }else if(row.key==="field"){value.field=cycle(fields,value.field,delta);value.op="is";value.value=["year","rating","playcount","duration","season","episode"].indexOf(value.field)>=0?0:"";replaceNode(value)}
        else if(row.key==="op"){value.op=cycle(operators,value.op,delta);if(value.op==="missing" || value.op==="present")delete value.value;else if(value.op==="one-of")value.value=[numeric?0:""];else if(value.op==="in-range")value.value=[0,1];else value.value=numeric?0:"";replaceNode(value)}
        else if(row.key==="value"){typingBaseline=JSON.parse(JSON.stringify(value));editingRow=row;keyboard.value=String(row.valueIndex===undefined?value.value:value.value[row.valueIndex]);keyboard.selectedIndex=0;typing=true}
        else if(row.key==="addValue"){typingBaseline=JSON.parse(JSON.stringify(value));value.value.push(numeric?0:"");replaceNode(value);editingRow={valueIndex:value.value.length-1};keyboard.value="";keyboard.selectedIndex=0;typing=true}
        else if(row.key==="removeValue"){value.value.splice(row.valueIndex,1);replaceNode(value);selectedIndex=Math.min(selectedIndex,rows.length-1)}
        else if(row.key==="child"){path=path.concat([row.child]);selectedIndex=0}
        else if(row.key==="addLeaf" || row.key==="addGroup"){value.rules.push(row.key==="addLeaf"?{field:"studio",op:"contains",value:""}:{op:"all",rules:[{field:"source",op:"is",value:"kodi"}]});replaceNode(value);path=path.concat([value.rules.length-1]);selectedIndex=0}
        else if(row.key==="up"){path=path.slice(0,-1);selectedIndex=0}
        else if(row.key==="remove"){let copy=JSON.parse(JSON.stringify(tree));nodeAt(copy,path.slice(0,-1)).rules.splice(path[path.length-1],1);tree=copy;path=path.slice(0,-1);selectedIndex=0}
        else if(row.key==="done")accepted(tree)
        else if(row.key==="cancel")canceled()
    }
    function handleCommand(command){
        if(typing){keyboard.handleCommand(command);return}
        if(command===inputManager.back){if(path.length){path=path.slice(0,-1);selectedIndex=0}else canceled()}
        else if(command===inputManager.navigateUp)selectedIndex=Math.max(0,selectedIndex-1)
        else if(command===inputManager.navigateDown)selectedIndex=Math.min(rows.length-1,selectedIndex+1)
        else if(command===inputManager.confirm)invoke(1)
        else if(command===inputManager.navigateLeft || command===inputManager.navigateRight){const key=rows[selectedIndex].key;if(key==="kind" || key==="field" || key==="op")invoke(command===inputManager.navigateLeft?-1:1)}
    }
    Rectangle {anchors.fill:parent;color:root.appearance.surfaceColor;radius:root.options.radius}
    Column {anchors.fill:parent;anchors.margins:root.options.gap*2;spacing:root.options.gap;visible:!root.typing
        Text {width:parent.width;textFormat:Text.PlainText;text:root.message || "Channel rules · "+(root.path.length?"Nested group "+root.path.join(" / "):"Root");color:root.appearance.textColor;font.pixelSize:root.fontSize*1.25;elide:Text.ElideRight}
        ListView {id:list;width:parent.width;height:parent.height-root.fontSize*3;clip:true;model:root.rows;currentIndex:root.selectedIndex;highlightMoveDuration:0;highlightRangeMode:ListView.ApplyRange
            delegate:Rectangle {required property var modelData;required property int index;width:list.width;height:root.fontSize*2.3;radius:root.options.radius;color:root.appearance.surfaceColor;border.color:root.appearance.focusColor;border.width:root.selectedIndex===index?2:0
                Text {anchors.fill:parent;anchors.margins:root.options.gap;textFormat:Text.PlainText;text:modelData.label;color:root.appearance.textColor;font.pixelSize:root.fontSize;elide:Text.ElideRight}
                MouseArea {anchors.fill:parent;onClicked:{root.selectedIndex=index;root.invoke(1)}}
            }
        }
    }
    OnScreenKeyboard {id:keyboard;anchors.centerIn:parent;width:Math.min(parent.width-root.options.gap*4,850);height:Math.min(parent.height,440);visible:root.typing;appearance:root.appearance
        onCanceled:{if(root.typingBaseline)root.replaceNode(root.typingBaseline);root.typingBaseline=null;root.typing=false}
        onAccepted:text=>{
            let value=JSON.parse(JSON.stringify(root.node));const parse=v=>root.numeric?Number(v.trim()):v.trim()
            if(root.editingRow.valueIndex===undefined)value.value=parse(text)
            else value.value[root.editingRow.valueIndex]=parse(text)
            root.replaceNode(value);root.typingBaseline=null;root.typing=false
        }
    }
}
