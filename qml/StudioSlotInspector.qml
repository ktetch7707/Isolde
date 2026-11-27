import QtQuick
import QtQuick.Controls

Item {
    id: root
    // Studio slot inspector: inspects contentSlot definitions on the active page,
    // shows which component occupies each slot, offers Default/Override selection,
    // shared appearance controls, binding selection and state preview.
    // Skins select layout, appearance and configured input roles through properties.
    required property var session
    required property var page
    required property var options
    required property var appearance
    property var selectedIds:[]
    property string pageId:""
    signal closed()
    signal operation(string name)

    readonly property real unit:((appearance || {}).typographyScale || 1)*Math.min(width/1280,height/720)
    readonly property var pageItems:(page && page.items || [])
    // Components that define slots (carry a contentSlot definition).
    readonly property var slots:pageItems.filter(item=>item.contentSlot && item.contentSlot.id)
    // A child links to the slot owner's component ID, which may differ from the public slot name.
    function occupantFor(ownerId){
        return pageItems.find(item=>item.parentId===ownerId) || null
    }
    // Permitted component kinds for a slot.
    function permittedFor(slotId){
        const slot=slots.find(s=>s.contentSlot.id===slotId)
        return slot?slot.contentSlot.permitted || []:[]
    }
    // Components eligible to occupy a slot (matching permitted kinds, or all if empty).
    function eligibleFor(slotId){
        const permitted=permittedFor(slotId)
        const currentSlot=slots.find(s=>s.contentSlot.id===slotId)
        if(!currentSlot)return []
        return pageItems.filter(item=>{
            if(item.id===currentSlot.id)return false
            if(item.parentId && item.parentId!==currentSlot.id)return false
            if(permitted.length===0)return true
            return permitted.indexOf(item.componentType)>=0
        })
    }
    property int selectedSlot:0
    property int selected:0
    property bool overrideMode:false
    property var selectedOccupant:null
    property string message:""

    readonly property var currentSlot:slots[selectedSlot] || null
    readonly property var slotDefinition: currentSlot ? currentSlot.contentSlot : ({})
    readonly property var currentOccupant:currentSlot?occupantFor(currentSlot.id):null
    readonly property var eligible:currentSlot?eligibleFor(currentSlot.contentSlot.id):[]
    readonly property var slotLabels:slots.map(s=>s.contentSlot.label || s.contentSlot.id)
    readonly property var occupantLabels:eligible.map(i=>i.name || i.componentType || "Untitled")

    function close(){closed()}
    function back(){closed()}
    function chooseSlot(index){selectedSlot=index;overrideMode=!!currentOccupant;selectedOccupant=currentOccupant;message=""}
    function toggleOverride(){overrideMode=!overrideMode;if(!overrideMode){selectedOccupant=null;message=""}}
    function pickOccupant(index){
        if(index<0 || index>=eligible.length)return
        const item=eligible[index]
        const slot=currentSlot
        if(!slot)return
        if(item.parentId===slot.id){message=qsTr("Already occupying this slot.");return}
        if(currentOccupant){message=qsTr("Move the current occupant out before assigning a different component.");return}
        const placement=Object.assign({},item.placement,{x:0,y:0})
        const success=run("place",{placements:[{id:item.id,parentId:slot.id,categoryId:slot.categoryId || "",placement:placement}]})
        if(success){selectedOccupant=item;overrideMode=true;message=qsTr("Slot overridden · Apply publishes");view="slots";selected=selectedSlot}
    }
    function run(op,args){
        const success=session.execute(op,args,pageId)
        if(!success){message=session.errorString;return false}
        message="Draft updated · nothing applied.";return true
    }
    function labels(){
        if(view==="occupants")return occupantLabels.concat([qsTr("Back")])
        if(slotLabels.length===0)return ["No slots defined on this page.","Back"]
        return slotLabels.map((l,i)=>(i===selectedSlot?"▸ ":"")+l+(overrideMode && i===selectedSlot?" · Override":"")).concat(["Default","Choose occupant…","Appearance","Bindings","Preview","Back"])
    }
    function activate(){
        const lbl=labels()
        if(view==="occupants"){occupantCommand(inputManager.confirm);return}
        if(selected===lbl.length-1){closed();return}
        if(slots.length && selected<slots.length){chooseSlot(selected);return}
        if(selected===slots.length){toggleOverride();return}
        if(selected===slots.length+1){view="occupants";selected=0;return}
        if(selected===slots.length+2){view="appearance";selected=0;return}
        if(selected===slots.length+3){view="bindings";selected=0;return}
        if(selected===slots.length+4){view="preview";selected=0;return}
    }
    property string view:"slots"
    function handleCommand(command){
        if(view==="occupants"){occupantCommand(command);return}
        if(view==="appearance"){appearanceCommand(command);return}
        if(view==="bindings"){bindingCommand(command);return}
        if(view==="preview"){previewCommand(command);return}
        if(command===inputManager.back){closed();return}
        if(command===inputManager.confirm){activate();return}
        if(command===inputManager.navigateDown || command===inputManager.navigateRight)selected=Math.min(labels().length-1,selected+1)
        else if(command===inputManager.navigateUp || command===inputManager.navigateLeft)selected=Math.max(0,selected-1)
    }
    function occupantCommand(command){
        if(command===inputManager.back){view="slots";selected=0;return}
        if(command===inputManager.confirm){
            if(selected===eligible.length){view="slots";selected=selectedSlot;return}
            pickOccupant(selected);return
        }
        if(command===inputManager.navigateDown || command===inputManager.navigateRight)selected=Math.min(eligible.length,selected+1)
        else if(command===inputManager.navigateUp || command===inputManager.navigateLeft)selected=Math.max(0,selected-1)
    }
    function appearanceCommand(command){
        if(command===inputManager.back){view="slots";selected=0;return}
        if(command===inputManager.confirm){message="Appearance controls open component editor.";operation("geometry");return}
        selected=Math.min(3,command===inputManager.navigateDown || command===inputManager.navigateRight?selected+1:command===inputManager.navigateUp || command===inputManager.navigateLeft?selected-1:selected)
    }
    function bindingCommand(command){
        if(command===inputManager.back){view="slots";selected=0;return}
        if(command===inputManager.confirm){message="Binding selection opens action picker.";operation("action");return}
        selected=Math.min(2,command===inputManager.navigateDown || command===inputManager.navigateRight?selected+1:command===inputManager.navigateUp || command===inputManager.navigateLeft?selected-1:selected)
    }
    function previewCommand(command){
        if(command===inputManager.back){view="slots";selected=0;return}
        selected=Math.min(2,command===inputManager.navigateDown || command===inputManager.navigateRight?selected+1:command===inputManager.navigateUp || command===inputManager.navigateLeft?selected-1:selected)
    }
    Rectangle {
        anchors.centerIn:parent;width:Math.min(parent.width-48*root.unit,760*root.unit);height:Math.min(parent.height-48*root.unit,640*root.unit);radius:18;color:root.appearance.surfaceColor;border.width:2;border.color:root.appearance.focusColor
        Column {anchors.fill:parent;anchors.margins:22*root.unit;spacing:12*root.unit
            Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";width:parent.width;text:"Slot Inspector";color:root.appearance.textColor;font.pixelSize:26*root.unit;elide:Text.ElideRight}
            Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";width:parent.width;text:slots.length?"Select a slot to inspect its occupants and permitted components.":"No contentSlot definitions found on this page.";color:root.appearance.mutedColor;font.pixelSize:17*root.unit;wrapMode:Text.Wrap}
            ListView {id:list;width:parent.width;height:Math.max(120*root.unit,parent.height-200*root.unit);clip:true;model:root.labels();currentIndex:root.selected;spacing:6;highlightRangeMode:ListView.ApplyRange;highlightMoveDuration:0
                delegate:Rectangle {required property int index;required property string modelData;width:list.width;height:52*root.unit;radius:8;color:index===root.selected?root.appearance.selectedColor:"transparent";border.width:index===root.selected?2:0;border.color:root.appearance.focusColor
                    Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";anchors.fill:parent;anchors.margins:12;text:modelData;color:root.appearance.textColor;font.pixelSize:19*root.unit;elide:Text.ElideRight}
                    MouseArea {anchors.fill:parent;onClicked:{root.selected=index;root.activate()}}
                }
            }
            Rectangle {visible:!!currentSlot;width:parent.width;height:1;color:root.appearance.mutedColor}
            Column {visible:!!currentSlot;spacing:10*root.unit;width:parent.width
                Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";text:slotDefinition.label || "Slot";color:root.appearance.textColor;font.pixelSize:22*root.unit;font.bold:true}
                Row {spacing:12*root.unit
                    Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";text:"Kind:";color:root.appearance.mutedColor;font.pixelSize:17*root.unit}
                    Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";text:slotDefinition.kind || "";color:root.appearance.textColor;font.pixelSize:17*root.unit}
                }
                Row {spacing:12*root.unit
                    Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";text:"Permitted:";color:root.appearance.mutedColor;font.pixelSize:17*root.unit}
                    Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";text:(slotDefinition.permitted || []).join(", ") || qsTr("Any");color:root.appearance.textColor;font.pixelSize:17*root.unit;wrapMode:Text.Wrap}
                }
                Row {spacing:12*root.unit
                    Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";text:"Occupant:";color:root.appearance.mutedColor;font.pixelSize:17*root.unit}
                    Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";text:currentOccupant?(currentOccupant.name || currentOccupant.componentType):"None";color:root.appearance.textColor;font.pixelSize:17*root.unit;elide:Text.ElideRight}
                }
                Row {spacing:12*root.unit
                    Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";text:"Mode:";color:root.appearance.mutedColor;font.pixelSize:17*root.unit}
                    Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";text:overrideMode?"Override":"Default";color:overrideMode?root.appearance.focusColor:root.appearance.textColor;font.pixelSize:17*root.unit}
                }
            }
            Text {textFormat:Text.PlainText;font.family:root.appearance.fontFamily || "DejaVu Sans";width:parent.width;text:root.message;color:root.appearance.mutedColor;font.pixelSize:15*root.unit;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight}
        }
    }
}
