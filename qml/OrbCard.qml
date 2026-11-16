import QtQuick

Item {
    id: root
    required property var item
    property var style: ({})
    property var appearance: ({})
    property bool selected: false
    property real unit: 1
    readonly property real diameter: Math.min(width-8*unit,height-28*unit,(style.diameter || 110)*unit)
    readonly property color ink: style.labelColor || appearance.labelColor || appearance.textColor || "#514B67"
    readonly property color surface: style.surfaceColor || appearance.surfaceColor || "#E8D4F8"
    readonly property bool horizontal:style.appIconStyle==="tiles" || style.appIconStyle==="rail"
    readonly property real padding:(style.padding===undefined?12:style.padding)*unit
    readonly property real slot:horizontal?Math.min(Math.max(1,height-2*padding),(style.iconSize || 48)*unit):diameter
    readonly property real glowPad:(style.iconGlow===undefined?8:style.iconGlow)*unit*2
    readonly property bool shapeFocus:!!item.logoSource && selected && style.focusEnabled!==false && style.iconFocus!=="frame"
    readonly property color pageSurface:appearance.backgroundStart || appearance.surfaceColor || surface
    readonly property bool contrastBacking:style.iconBacking!=="none" && style.logoMode!=="monochrome" && !!item.iconContrastBackdrop && (pageSurface.r+pageSurface.g+pageSurface.b)/3>0.65
    readonly property bool logoOnly:!!item.logoSource && (style.logoMode==="cutout" || style.logoMode==="original" || style.logoMode==="monochrome")
    readonly property var focusFrame:({x:circle.x,y:circle.y,width:circle.width,height:circle.height,radius:circle.radius,color:style.focusColor || appearance.focusColor || "white",lineWidth:Math.max(2,style.focusWidth===undefined?4:style.focusWidth)*unit})
    signal activated()
    Rectangle {
        id: circle; objectName: "orbSurface"
        width:root.horizontal?root.width:root.diameter; height:root.horizontal?root.height:width; radius:root.horizontal?(root.style.shape==="pill"?height/2:(root.style.radius===undefined?18:root.style.radius)*root.unit):width/2; anchors.horizontalCenter: parent.horizontalCenter
        color:root.logoOnly && !root.horizontal?(root.style.iconBacking==="surface"?root.surface:root.contrastBacking?root.item.iconContrastBackdrop:"transparent"):Qt.rgba(root.surface.r,root.surface.g,root.surface.b,root.style.surfaceOpacity === undefined ? 1 : root.style.surfaceOpacity)
        border.width:root.selected && root.style.focusEnabled!==false && (!root.item.logoSource || root.style.iconFocus==="frame")?(root.style.focusWidth===undefined?4:root.style.focusWidth)*root.unit:root.logoOnly && !root.horizontal?0:root.style.borderWidth || 0
        border.color:root.selected?root.style.focusColor || root.appearance.focusColor || "white":root.style.borderColor || root.appearance.borderColor || "#80FFFFFF"
        Rectangle {visible:root.horizontal && root.contrastBacking;x:root.padding;y:(parent.height-root.slot)/2;width:root.slot;height:width;radius:width/4;color:root.item.iconContrastBackdrop || "transparent"}
        ArtworkImage {
            x:(root.horizontal?root.padding:0)-root.glowPad;y:(root.horizontal?(parent.height-root.slot)/2:0)-root.glowPad
            width:root.slot+2*root.glowPad;height:width
            objectName:"orbArtwork"
            radius:root.logoOnly || root.horizontal?0:width/2;source:root.item.logoSource || "";crop:root.style.imageCrop || null
            contentInset:root.glowPad+(root.style.iconPadding || 0)*root.unit;contentScale:root.item.iconOpticalScale || 1
            tintColor:root.style.logoMode==="monochrome" && root.item.inkAvailable===true?root.style.iconColor || root.appearance.iconColor || root.ink:"transparent"
            outlineWidth:root.shapeFocus?(root.style.focusWidth===undefined?2:root.style.focusWidth)*root.unit:0
            glowWidth:root.shapeFocus?(root.style.iconGlow===undefined?8:root.style.iconGlow)*root.unit:0
            glowOpacity:root.style.iconGlowOpacity===undefined?0.65:root.style.iconGlowOpacity
            outlineColor:root.style.focusColor || root.appearance.focusColor || "white"
            fit:root.style.artFit || (root.style.appIconStyle?"contain":"cover");focalX:root.style.artFocalX===undefined?0.5:root.style.artFocalX;focalY:root.style.artFocalY===undefined?0.5:root.style.artFocalY;zoom:root.style.artZoom || 1
        }
        LineIcon {
            visible: !root.item.logoSource && !root.item.appSymbol
            anchors.centerIn: parent; width: (root.style.iconSize || 44)*root.unit; height: width
            name: root.style.icon || "display"; ink: root.style.iconColor || root.appearance.iconColor || "white"
        }
        Text {
            anchors.centerIn:parent;visible:!!root.item.appSymbol && !root.item.logoSource
            text:root.item.appSymbol || "";textFormat:Text.PlainText
            color:root.style.iconColor || root.appearance.iconColor || root.ink;font.family:root.appearance.fontFamily || "DejaVu Sans"
            font.pixelSize:(root.style.iconSize || 32)*root.unit;Accessible.ignored:true
        }
    }
    Text {objectName:"orbHorizontalLabel";visible:root.horizontal && root.item.showTitle!==false;x:root.padding+root.slot+(root.style.cardGap || 12)*root.unit;y:(parent.height-height)/2;width:Math.max(0,parent.width-x-root.padding);text:root.item.name || "";textFormat:Text.PlainText;color:root.ink;font.pixelSize:(root.style.labelSize || 20)*root.unit*(root.appearance.typographyScale || 1);font.family:root.appearance.fontFamily || "DejaVu Sans";font.weight:root.selected?Font.DemiBold:Font.Normal;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight}
    Rectangle {
        visible:!root.horizontal && root.item.showTitle!==false && root.style.labelPosition===undefined
        anchors.horizontalCenter: parent.horizontalCenter; y: circle.height + 5*root.unit
        width: Math.min(root.width,label.implicitWidth+24*root.unit); height: 23*root.unit; radius: height/2
        objectName: "orbLabelSurface"
        color: root.style.labelPill === false ? "transparent" : root.style.labelSurfaceColor || "#85FFFFFF"
        Text {textFormat:Text.PlainText;
            id: label; objectName: "orbLabel"; anchors.centerIn: parent; width: Math.min(implicitWidth,parent.width-8)
            text: root.style.visibleLabel===undefined?root.item.name || "":root.style.visibleLabel; color: root.selected && root.style.selectedLabelColor ? root.style.selectedLabelColor : root.ink
            font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: (root.style.labelSize || 15)*root.unit*(root.appearance.typographyScale || 1)
            elide: Text.ElideRight; font.weight: root.selected ? Font.DemiBold : Font.Normal
        }
    }
    MouseArea { anchors.fill: parent; onClicked: root.activated() }
}
