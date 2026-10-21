import QtQuick

Item {
    id:root
    property url source
    property var config:({})
    required property color startColor
    required property color endColor
    readonly property string mode:config.mode || (source.toString()?"image":"gradient")
    readonly property bool legacy:Object.keys(config).length===0
    readonly property int status:mode!=="image"?Image.Null:legacy?original.status:art.status
    Rectangle {
        anchors.fill:parent;visible:root.mode!=="image" || root.status!==Image.Ready
        gradient:Gradient {GradientStop{position:0;color:root.startColor}GradientStop{position:1;color:root.mode==="solid"?root.startColor:root.endColor}}
    }
    Image {id:original;anchors.fill:parent;source:root.mode==="image" && root.legacy?root.source:"";fillMode:Image.PreserveAspectCrop}
    ArtworkImage {
        id:art;anchors.fill:parent;source:root.mode==="image" && !root.legacy?root.source:""
        crop:root.config.imageCrop || null;fit:root.config.artFit || "cover"
        focalX:root.config.artFocalX===undefined?0.5:root.config.artFocalX;focalY:root.config.artFocalY===undefined?0.5:root.config.artFocalY;zoom:root.config.artZoom || 1
    }
    Rectangle {
        anchors.fill:parent;visible:root.mode==="image" && root.status===Image.Ready
        readonly property color ink:root.config.tintColor || root.endColor
        color:Qt.rgba(ink.r,ink.g,ink.b,root.config.dim || 0)
    }
}
