import QtQuick

Rectangle {
    id: root
    // Reusable progress indicator template for skins. Renders a bounded progress
    // bar (resume, watched, download, etc.) with configurable track color, fill
    // color, height, corner radius and value. Skins select this template for
    // progress surfaces and configure layout, appearance and input roles through
    // its properties.
    property real value:0          // 0..1
    property color trackColor:"#48414E"
    property color fillColor:"#6EA8D1"
    property real unit:1
    property real height:4
    property real corner:2
    property bool accessible:true

    width:parent.width;height:height*unit;radius:corner*unit;color:trackColor;border.color:"transparent"
    Accessible.role:accessible?Accessible.ProgressBar:Accessible.Ignored
    Accessible.name:qsTr("Progress")
    Accessible.value:accessible?Math.round(root.value*100)+"%":""

    Rectangle {
        anchors.left:parent.left;anchors.verticalCenter:parent.verticalCenter
        width:Math.max(0,parent.width*(root.value));height:parent.height
        radius:root.corner*root.unit;color:root.fillColor
    }
}
