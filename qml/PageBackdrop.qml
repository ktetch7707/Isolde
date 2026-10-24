import QtQuick

Item {
    id:root
    property url source
    property var config:({})
    required property color startColor
    required property color endColor
    readonly property string mode:config.mode || (source.toString()?"image":"gradient")
    readonly property bool legacy:Object.keys(config).length===0
    // Ordinary wallpapers stay in the scene graph. Canvas requires a separate
    // decoded copy and a repaint after the window becomes exposed again.
    readonly property bool directImage:!config.imageCrop && (config.artZoom === undefined || config.artZoom===1) && (config.artFocalX === undefined || config.artFocalX===0.5) && (config.artFocalY === undefined || config.artFocalY===0.5)
    readonly property int status:mode!=="image"?Image.Null:directImage?original.status:art.status
    Rectangle {
        anchors.fill:parent;visible:root.mode!=="image" || root.status!==Image.Ready
        gradient:Gradient {GradientStop{position:0;color:root.startColor}GradientStop{position:1;color:root.mode==="solid"?root.startColor:root.endColor}}
    }
    Image {id:original;objectName:"backdropImage";anchors.fill:parent;source:root.mode==="image" && root.directImage?root.source:"";asynchronous:true;cache:true;fillMode:root.config.artFit==="contain"?Image.PreserveAspectFit:Image.PreserveAspectCrop}
    ArtworkImage {
        id:art;anchors.fill:parent;source:root.mode==="image" && !root.directImage?root.source:""
        crop:root.config.imageCrop || null;fit:root.config.artFit || "cover"
        focalX:root.config.artFocalX===undefined?0.5:root.config.artFocalX;focalY:root.config.artFocalY===undefined?0.5:root.config.artFocalY;zoom:root.config.artZoom || 1
    }
    Rectangle {
        anchors.fill:parent;visible:root.mode==="image" && root.status===Image.Ready
        readonly property color ink:root.config.tintColor || root.endColor
        color:Qt.rgba(ink.r,ink.g,ink.b,root.config.dim || 0)
    }
}
