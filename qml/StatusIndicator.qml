import QtQuick

Item {
    id: root
    // Reusable status indicator template for skins. Renders loading, error or
    // empty states with configurable icon, message, retry action and appearance.
    // Skins select this template for status surfaces and configure layout,
    // appearance and input roles through its properties.
    property string state:""          // "loading", "error", "empty", "none"
    property string message:""
    property string icon:""
    property color ink:"#AAB5C9"
    property color surface:"transparent"
    property var appearance:({})
    property real unit:1
    property bool retryable:false
    property var retryAction:({})
    property bool accessible:true

    readonly property bool show:state!=="none"
    readonly property bool showLoading:state==="loading"
    readonly property bool showError:state==="error"
    readonly property bool showEmpty:state==="empty"

    width:parent.width;height:parent.height
    Accessible.role:accessible&&show?Accessible.StaticText:Accessible.Ignored
    Accessible.name:accessible&&show?message:""
    Accessible.description:accessible&&show?unavailableReason:""

    property string unavailableReason:""

    Column {
        anchors.centerIn:parent;spacing:10*unit;visible:show
        Image {
            visible:!!icon;source:icon;width:32*unit;height:width;fillMode:Image.PreserveAspectFit
            color:ink;smooth:true
        }
        Text {
            visible:!!message;text:message;color:ink;font.pixelSize:18*unit
            font.family:root.appearance?root.appearance.fontFamily:"DejaVu Sans"
            elide:Text.ElideRight;maximumLineCount:2;wrapMode:Text.WordWrap
            horizontalAlignment:Text.AlignHCenter;width:Math.min(parent.width,400*unit)
        }
        Rectangle {
            visible:retryable && showError;width:120*unit;height:36*unit;radius:height/2
            color:root.ink;border.color:"transparent"
            Text {anchors.centerIn:parent;text:qsTr("Retry");color:root.surface;font.bold:true;font.pixelSize:14*unit}
            MouseArea {anchors.fill:parent;enabled:!!retryAction.type;onClicked:if(retryAction.type)root.retryClicked(retryAction)}
        }
    }
    signal retryClicked(var action)
}
