import QtQuick

Item {
    id:root
    property var pages:[]
    property string activeId:""
    property var style:({})
    property var appearance:({})
    property real unit:1
    property bool readOnly:false
    property bool engaged:false
    property bool highlighted:false
    property int selected:Math.max(0,pages.findIndex(p=>p.id===activeId))
    readonly property var layout:style.layout || ({})
    readonly property bool horizontal:layout.mode==="row"
    readonly property int capacity:horizontal?layout.visibleColumns || layout.columns || 6:layout.rows || 6
    readonly property real padding:(layout.padding || 0)*unit
    readonly property bool validGeometry:width>2*padding+(horizontal?(capacity-1)*(layout.gap || 0)*unit:0) && height>2*padding+(horizontal?0:(capacity-1)*(layout.gap || 0)*unit)
    signal engage()
    signal launch(var action)
    onPagesChanged:{const active=pages.findIndex(p=>p.id===activeId);selected=active>=0?active:Math.max(0,Math.min(selected,pages.length-1))}
    onActiveIdChanged:{const active=pages.findIndex(p=>p.id===activeId);if(active>=0)selected=active}
    function activate(index){
        if(readOnly || !validGeometry || !pages[index])return
        selected=index
        const page=pages[index]
        launch(page.id?{type:"open-page",pageId:page.id}:{type:"root-page"})
    }
    function handleCommand(command){
        if(readOnly || !validGeometry)return false
        if(command===inputManager.back)return false
        if(command===inputManager.confirm){activate(selected);return true}
        const previous=horizontal?inputManager.navigateLeft:inputManager.navigateUp
        const next=horizontal?inputManager.navigateRight:inputManager.navigateDown
        if(command!==previous && command!==next)return false
        const index=selected+(command===previous?-1:1)
        if(index<0 || index>=pages.length)return false
        selected=index;return true
    }
    Rectangle {anchors.fill:parent;radius:(root.style.radius || 0)*root.unit
        readonly property color ink:root.style.surfaceColor || root.appearance.surfaceColor || "transparent"
        color:Qt.rgba(ink.r,ink.g,ink.b,root.style.surfaceOpacity===undefined?1:root.style.surfaceOpacity)
    }
    ListView {
        id:list;objectName:"pageMenuList"
        visible:root.validGeometry
        anchors.fill:parent;anchors.margins:root.padding;clip:true
        orientation:root.horizontal?ListView.Horizontal:ListView.Vertical
        spacing:(root.layout.gap || 0)*root.unit;model:root.validGeometry?root.pages:[]
        currentIndex:root.selected;highlightRangeMode:ListView.ApplyRange
        onCurrentIndexChanged:positionViewAtIndex(currentIndex,ListView.Contain)
        delegate:HomeCard {
            required property var modelData
            required property int index
            objectName:"pageMenuEntry-"+index
            width:root.horizontal?(list.width-(root.capacity-1)*list.spacing)/root.capacity:list.width
            height:root.horizontal?list.height:(list.height-(root.capacity-1)*list.spacing)/root.capacity
            item:({name:modelData.name,current:modelData.id===root.activeId,presentation:Object.assign({},root.style,{variant:"text",layout:undefined})})
            appearance:root.appearance;unit:root.unit
            selected:(root.engaged || root.highlighted) && root.selected===index
            onActivated:{if(!root.readOnly){root.engage();root.activate(index)}}
        }
    }
    Text {anchors.fill:parent;visible:!root.validGeometry;text:"Enlarge the menu or reduce its visible entries.";wrapMode:Text.WordWrap;color:root.appearance.textColor || "white";font.pixelSize:(root.style.labelSize || 20)*root.unit}
}
