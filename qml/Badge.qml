import QtQuick

Rectangle {
    id: root
    // Reusable badge template for skins. Displays a small state chip (watched,
    // progress, source, update, etc.) with configurable label, icon, color,
    // size, corner radius and placement. Skins select this template for badge
    // surfaces and configure layout, appearance and input roles through its
    // properties.
    property string label:""
    property string icon:""
    property color ink:"#FFFFFF"
    property color surface:"#6EA8D1"
    property real unit:1
    property real size:24
    property real corner:12
    property string placement:"top-right"
    property bool visibleLabel:true
    property bool accessible:true
    property var action:({})
    property string unavailableReason:""

    readonly property real textWidth:TextItem.implicitWidth
    readonly property real badgeWidth:visibleLabel && label?Math.max(size,textWidth+12*unit):size
    readonly property real badgeHeight:size

    width:badgeWidth;height:badgeHeight;radius:corner*unit;color:surface;border.color:"transparent"
    Accessible.role:accessible?Accessible.StaticText:Accessible.Ignored
    Accessible.name:accessible&&label?label:(icon?icon:"Badge")
    Accessible.description:accessible&&unavailableReason?unavailableReason:""

    Row {
        id:badgeRow
        anchors.centerIn:parent;spacing:6*unit
        visible:!!icon || (!!label && visibleLabel)
        Image {
            visible:!!icon;source:icon;width:size*0.6;height:width;fillMode:Image.PreserveAspectFit
            color:ink;smooth:true
        }
        Text {
            id:TextItem
            visible:!!label && visibleLabel;text:label;color:ink;font.pixelSize:(size*0.55)*unit
            font.bold:true;elide:Text.ElideRight;maximumLineCount:1;wrapMode:Text.NoWrap
        }
    }
    MouseArea {
        anchors.fill:parent;enabled:!!action.type;hoverEnabled:true
        onClicked:if(action.type)root.clicked(action)
    }
    signal clicked(var action)
}
