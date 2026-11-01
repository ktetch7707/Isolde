import QtQuick

Item {
    id:root
    objectName:"studioActionPicker"
    required property var actions
    required property var appearance
    required property var categories
    property string query:""
    property int category:0
    property int selected:0
    property bool toolsFocused:true
    property int tool:0
    property bool searching:false
    property string message:""
    readonly property real unit:Math.min(width/1280,height/720)*(appearance.typographyScale || 1)
    readonly property var tools:["Search links",categories[category]?categories[category].name:"All links","Clear search","Cancel"]
    readonly property var entries:actions.filter(entry=>matches(entry))
    signal picked(var entry)
    signal canceled()
    function matches(entry){
        const types=categories[category]?categories[category].types:[]
        const type=entry.needsMedia?"media-picker":(entry.action || {}).type || "none"
        if(types.length && types.indexOf(type)<0)return false
        const text=(entry.name+" "+(entry.aliases || []).join(" ")).toLocaleLowerCase()
        return query.trim().toLocaleLowerCase().split(/\s+/).every(word=>text.indexOf(word)>=0)
    }
    onEntriesChanged:selected=Math.max(0,Math.min(selected,entries.length-1))
    function activate(){
        if(!toolsFocused){if(entries[selected])picked(entries[selected]);return}
        if(tool===0){keyboard.value=query;keyboard.selectedIndex=0;searching=true}
        else if(tool===1){category=(category+1)%categories.length;selected=0}
        else if(tool===2){query="";category=0;selected=0}
        else canceled()
    }
    function handleCommand(command){
        if(searching){keyboard.handleCommand(command);return}
        if(command===inputManager.back){canceled();return}
        if(command===inputManager.confirm){activate();return}
        if(toolsFocused){
            if(command===inputManager.navigateLeft)tool=Math.max(0,tool-1)
            else if(command===inputManager.navigateRight)tool=Math.min(tools.length-1,tool+1)
            else if(command===inputManager.navigateDown && entries.length)toolsFocused=false
        }else{
            if(command===inputManager.navigateUp){if(selected===0)toolsFocused=true;else selected--}
            else if(command===inputManager.navigateDown)selected=Math.min(entries.length-1,selected+1)
        }
    }
    Rectangle {
        anchors.centerIn:parent;width:parent.width-48*root.unit;height:parent.height-48*root.unit
        radius:root.appearance.radius || 12;color:root.appearance.surfaceColor;border.width:2;border.color:root.appearance.focusColor
        Column {
            anchors.fill:parent;anchors.margins:24*root.unit;spacing:16*root.unit
            Text {textFormat:Text.PlainText;text:"Choose a link";color:root.appearance.textColor;font.family:root.appearance.fontFamily || "DejaVu Sans";font.pixelSize:28*root.unit}
            Row {
                width:parent.width;spacing:8*root.unit
                Repeater {model:root.tools;delegate:Rectangle {
                    required property int index;required property string modelData
                    width:(parent.width-24*root.unit)/4;height:52*root.unit;radius:8*root.unit
                    color:root.toolsFocused && root.tool===index?root.appearance.selectedColor:"transparent"
                    border.width:root.toolsFocused && root.tool===index?3:1;border.color:root.appearance.focusColor
                    Text {anchors.fill:parent;anchors.margins:8*root.unit;textFormat:Text.PlainText;text:modelData;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter;color:root.appearance.textColor;font.family:root.appearance.fontFamily || "DejaVu Sans";font.pixelSize:18*root.unit}
                    Accessible.role:Accessible.Button;Accessible.name:modelData
                    MouseArea {anchors.fill:parent;onClicked:{root.toolsFocused=true;root.tool=index;root.activate()}}
                }}
            }
            Text {width:parent.width;textFormat:Text.PlainText;text:root.message || root.entries.length+" links"+(root.query?" matching “"+root.query+"”":"")+" · Back keeps the current link.";elide:Text.ElideRight;color:root.appearance.mutedColor;font.family:root.appearance.fontFamily || "DejaVu Sans";font.pixelSize:18*root.unit}
            ListView {
                width:parent.width;height:parent.height-168*root.unit;clip:true;model:root.entries;currentIndex:root.selected;highlightRangeMode:ListView.ApplyRange;spacing:8*root.unit
                delegate:Rectangle {
                    required property int index;required property var modelData
                    width:ListView.view.width;height:56*root.unit;radius:8*root.unit
                    color:!root.toolsFocused && root.selected===index?root.appearance.selectedColor:"transparent"
                    border.width:!root.toolsFocused && root.selected===index?3:0;border.color:root.appearance.focusColor
                    Text {anchors.fill:parent;anchors.margins:12*root.unit;textFormat:Text.PlainText;text:modelData.name;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter;color:root.appearance.textColor;font.family:root.appearance.fontFamily || "DejaVu Sans";font.pixelSize:20*root.unit}
                    Accessible.role:Accessible.Button;Accessible.name:modelData.name
                    MouseArea {anchors.fill:parent;onClicked:{root.toolsFocused=false;root.selected=index;root.activate()}}
                }
                Text {visible:!root.entries.length;anchors.centerIn:parent;text:"No matching links. Change category or clear search.";textFormat:Text.PlainText;color:root.appearance.textColor;font.pixelSize:20*root.unit}
            }
        }
    }
    OnScreenKeyboard {
        id:keyboard;anchors.fill:parent;visible:root.searching;appearance:root.appearance;hint:root.message || "Search existing links by name. No link changes until you choose one."
        onCanceled:root.searching=false
        onAccepted:text=>{if(text.length>80){root.message="Use up to 80 characters for search.";return}root.message="";root.query=text;root.selected=0;root.searching=false;root.toolsFocused=!root.entries.length}
    }
}
