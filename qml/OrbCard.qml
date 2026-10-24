import QtQuick

Item {
    id: root
    required property var item
    property var style: ({})
    property var appearance: ({})
    property bool selected: false
    property real unit: 1
    readonly property real diameter: Math.min(width-8*unit,height-28*unit,(style.diameter || 110)*unit)
    readonly property color ink: style.labelColor || appearance.textColor || "#514B67"
    readonly property color surface: style.surfaceColor || appearance.surfaceColor || "#E8D4F8"
    readonly property bool logoOnly:!!item.logoSource && (style.logoMode==="cutout" || style.logoMode==="original")
    readonly property var focusFrame:({x:circle.x,y:circle.y,width:circle.width,height:circle.height,radius:circle.radius,color:style.focusColor || appearance.focusColor || "white",lineWidth:Math.max(2,style.focusWidth===undefined?4:style.focusWidth)*unit})
    signal activated()
    Rectangle {
        id: circle; objectName: "orbSurface"
        width: root.diameter; height: width; radius: width/2; anchors.horizontalCenter: parent.horizontalCenter
        color: root.logoOnly?"transparent":Qt.rgba(root.surface.r,root.surface.g,root.surface.b,root.style.surfaceOpacity === undefined ? 1 : root.style.surfaceOpacity)
        border.width: root.logoOnly?0:root.selected && root.style.focusEnabled!==false ? (root.style.focusWidth===undefined?4:root.style.focusWidth)*root.unit : root.style.borderWidth || 1; border.color: root.selected ? root.style.focusColor || root.appearance.focusColor || "white" : root.style.borderColor || "#80FFFFFF"
        Rectangle {visible:!root.logoOnly; anchors.fill: parent; anchors.margins: -7*root.unit; radius: width/2; color:"transparent"; border.width: root.selected ? 7*root.unit : 0; border.color: "#30FFFFFF" }
        ArtworkImage {
            anchors.fill: parent; anchors.margins: root.selected ? 4*root.unit : 1
            objectName:"orbArtwork"
            radius: root.logoOnly?0:width/2; source: root.item.logoSource || ""; crop: root.style.imageCrop || null
            outlineWidth:root.logoOnly && root.selected && root.style.focusEnabled!==false?(root.style.focusWidth===undefined?4:root.style.focusWidth)*root.unit:0
            outlineColor:root.style.focusColor || root.appearance.focusColor || "white"
            fit:root.style.artFit || "cover";focalX:root.style.artFocalX===undefined?0.5:root.style.artFocalX;focalY:root.style.artFocalY===undefined?0.5:root.style.artFocalY;zoom:root.style.artZoom || 1
        }
        LineIcon {
            visible: !root.item.logoSource && !root.item.appSymbol
            anchors.centerIn: parent; width: (root.style.iconSize || 44)*root.unit; height: width
            name: root.style.icon || "display"; ink: root.style.iconColor || "white"
        }
        Text {
            anchors.centerIn:parent;visible:!!root.item.appSymbol && !root.item.logoSource
            text:root.item.appSymbol || "";textFormat:Text.PlainText
            color:root.style.iconColor || root.ink;font.family:root.appearance.fontFamily || "DejaVu Sans"
            font.pixelSize:(root.style.iconSize || 32)*root.unit;Accessible.ignored:true
        }
    }
    Rectangle {
        visible:root.item.showTitle!==false && root.style.labelPosition===undefined
        anchors.horizontalCenter: parent.horizontalCenter; y: circle.height + 5*root.unit
        width: Math.min(root.width,label.implicitWidth+24*root.unit); height: 23*root.unit; radius: height/2
        color: root.style.labelPill === false ? "transparent" : "#85FFFFFF"
        Text {textFormat:Text.PlainText;
            id: label; anchors.centerIn: parent; width: Math.min(implicitWidth,parent.width-8)
            text: root.item.name || ""; color: root.selected && root.style.selectedLabelColor ? root.style.selectedLabelColor : root.ink
            font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: (root.style.labelSize || 15)*root.unit*(root.appearance.typographyScale || 1)
            elide: Text.ElideRight; font.weight: root.selected ? Font.DemiBold : Font.Normal
        }
    }
    MouseArea { anchors.fill: parent; onClicked: root.activated() }
}
