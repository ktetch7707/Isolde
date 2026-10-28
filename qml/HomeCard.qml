import QtQuick

Item {
    id: root
    required property var item
    property string variant: "source"
    property var appearance: ({})
    property var artworkResolver:typeof homeLayout!=="undefined"?homeLayout:null
    property bool artworkMotionAllowed:true
    property bool selected: false
    property string selectMode: "single"
    property string selectTrigger: "click"
    property string selectAction: "navigate"
    property string selectionColor: ""
    property color outsideColor: "#0e1018"
    property real unit: 1
    property date now: new Date()
    readonly property string title: style.visibleLabel===undefined?item.name || (item.widgetType === "clock" ? Qt.formatTime(now,"h:mm") : ""):style.visibleLabel
    Timer { interval: 1000; running: root.visible && root.item.widgetType === "clock"; repeat: true; onTriggered: root.now = new Date() }
    Timer { id: dwellTimer; interval: 700; repeat: false; running: root.selectTrigger === "dwell" && root.selected === false && mouseArea.containsMouse; onTriggered: { mouseArea.forceActiveFocus(); root.activated() } }
    readonly property var baseStyle:item.presentation || ({})
    readonly property string visualState:item.designState || (item.unavailableReason?"unavailable":mouseArea.pressed?"pressed":item.current?"selected":selected?"focused":"normal")
    readonly property var style:Object.assign({},baseStyle,(baseStyle.states || {}).normal || {},(baseStyle.states || {})[visualState] || {})
    readonly property string kind: item.componentType==="shape"?"shape":item.widgetType === "text" ? "text" : (style.variant || variant) === "launcher" ? "source" : (style.variant || variant)
    readonly property color ink: appearance.textColor || "#EEEAF0"
    readonly property color muted: appearance.mutedColor || "#AAA4B2"
    readonly property color accent: appearance.focusColor || "#BD8198"
    readonly property color selColor: root.selectionColor !== "" ? root.selectionColor : root.accent
    readonly property real textScale: (appearance.typographyScale || 1) * unit
    readonly property bool ellipse:kind==="shape" && style.shape==="ellipse"
    readonly property real bodyHeight:customLabel && style.labelPosition==="below"?Math.max(0,height-customLabelSurface.height):height
    readonly property real ellipseScaleX:ellipse && surface.width?width/surface.width:1
    readonly property real ellipseScaleY:ellipse && surface.height?bodyHeight/surface.height:1
    readonly property real corner:kind==="shape"?(ellipse?Math.max(width,bodyHeight)/2:style.shape==="pill"?Math.min(width,bodyHeight)/2:style.shape==="rounded"?Math.min((style.radius || 0)*unit,width/2,bodyHeight/2):0):Math.min((style.radius === undefined ? 22 : style.radius) * unit, Math.min(width, height) * (appearance.tileRoundness === undefined ? 0.2 : appearance.tileRoundness))
    readonly property color surfaceInk: root.style.surfaceColor || root.appearance.surfaceColor || "#28262F"
    readonly property bool customLabel:style.labelPosition!==undefined
    readonly property bool interactive:item.componentType!=="shape" && (item.componentType!=="image" || !!item.action)
    // Geometry comes from the rendered surface, including the orb's own diameter.
    // Editors can retain this outline when the authored body is transparent.
    readonly property var focusFrame:kind==="orb" ? orb.focusFrame : ({x:(surface.x-3*unit)*ellipseScaleX,y:(surface.y-3*unit)*ellipseScaleY,width:(surface.width+6*unit)*ellipseScaleX,height:(surface.height+6*unit)*ellipseScaleY,radius:surface.radius+3*unit,ellipse:ellipse,color:style.focusColor || selColor,lineWidth:Math.max(2,style.focusWidth===undefined?3:style.focusWidth)*unit})
    Accessible.role:interactive?Accessible.Button:Accessible.Graphic
    Accessible.name:(item.accessibility || {}).name || item.name || title
    Accessible.description:(item.accessibility || {}).hint || item.unavailableReason || ""
    signal activated()
    signal selectionChanged(bool selected)
    onSelectedChanged: root.selectionChanged(root.selected)
    Rectangle {
        id: surface
        objectName: "cardSurface"
        visible: root.kind !== "orb"
        width:root.ellipse?Math.max(parent.width,root.bodyHeight):parent.width; height:root.ellipse?width:root.kind==="shape" && root.style.shape==="divider"?(root.style.borderWidth || 2)*root.unit:root.customLabel && root.style.labelPosition==="below"?root.bodyHeight:root.kind === "recent" ? Math.min(parent.height - 36*root.unit,(root.style.artHeight || 86)*root.unit) : root.kind === "media" ? Math.max(35, parent.height - 58 * root.unit) : parent.height
        y:root.kind==="shape" && root.style.shape==="divider"?(parent.height-height)/2:0
        transform:Scale {xScale:root.ellipseScaleX;yScale:root.ellipseScaleY}
        radius: root.corner
        color: Qt.rgba(root.surfaceInk.r,root.surfaceInk.g,root.surfaceInk.b,root.style.surfaceOpacity === undefined ? 1 : root.style.surfaceOpacity)
        gradient:root.style.fillMode==="gradient"?fillGradient:null
        Gradient {id:fillGradient;orientation:root.style.fillDirection==="horizontal"?Gradient.Horizontal:Gradient.Vertical
            GradientStop {position:0;color:surface.color}
            GradientStop {position:1;color:{const ink=Qt.darker(root.style.fillEndColor || root.surfaceInk,1);return Qt.rgba(ink.r,ink.g,ink.b,root.style.surfaceOpacity===undefined?1:root.style.surfaceOpacity)}}
        }
        border.width:root.kind==="shape" && root.style.shape==="divider"?0:root.style.borderWidth===undefined?(root.kind === "recent" ? 1 : 0):root.style.borderWidth*root.unit; border.color:root.style.borderColor || "#B0FFFFFF"
        ArtworkImage {
            objectName: "cardArtwork"
            animationResolver:root.artworkResolver;managedAsset:root.item.customLogo || ""
            motionEnabled:root.artworkMotionAllowed && root.style.animateArtwork===true;reducedMotion:root.appearance.motionEnabled===false
            anchors.fill: parent; source: root.kind === "orb" ? "" : root.item.logoSource || ""
            crop: root.style.imageCrop || null; radius: surface.radius
            visible:root.item.showLogo!==false
            fit:root.style.artFit || "cover";focalX:root.style.artFocalX===undefined?0.5:root.style.artFocalX;focalY:root.style.artFocalY===undefined?0.5:root.style.artFocalY;zoom:root.style.artZoom || 1
        }
        LineIcon {
            visible: (root.kind === "source" || root.kind === "summary") && root.item.widgetType !== "clock"
            name: root.style.icon || "display"; ink: root.style.iconColor || (root.kind === "summary" ? root.accent : root.ink)
            width: (root.style.iconSize===undefined?28:root.style.iconSize) * root.unit; height: width
            x: root.kind === "source" ? (parent.width - width) / 2 : 24 * root.unit
            y: root.kind === "source" ? parent.height * 0.21 : (parent.height - height) / 2
        }
        Text {textFormat:Text.PlainText;
            visible: root.kind === "source" && root.item.showTitle!==false && !root.customLabel; anchors.bottom: parent.bottom; anchors.bottomMargin: 16 * root.unit
            width: parent.width - 16; anchors.horizontalCenter: parent.horizontalCenter; horizontalAlignment: Text.AlignHCenter
            text: root.title; color: root.ink; font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: 19 * root.textScale; elide: Text.ElideRight
        }
        Column {
            visible: root.kind === "summary"; x: 72 * root.unit; anchors.verticalCenter: parent.verticalCenter
            width: parent.width - x - 55 * root.unit; spacing: 5 * root.unit
            Text {textFormat:Text.PlainText; width: parent.width; text: root.title; color: root.ink; font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: 21 * root.textScale; elide: Text.ElideRight }
            Text {textFormat:Text.PlainText; width: parent.width; text: root.style.subtitle || ""; color: root.muted; font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: 16 * root.textScale; elide: Text.ElideRight }
        }
        Text {textFormat:Text.PlainText; visible: root.kind === "summary"; anchors.right: parent.right; anchors.rightMargin: 24 * root.unit; anchors.verticalCenter: parent.verticalCenter; text: "›"; color: root.ink; font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: 32 * root.unit }
        Rectangle {
            visible: root.kind === "banner" && !!root.style.label
            x: 20 * root.unit; anchors.bottom: parent.bottom; anchors.bottomMargin: 18 * root.unit
            width: bannerLabel.implicitWidth + 24 * root.unit; height: 38 * root.unit; radius: 14 * root.unit; color: "#D5222521"
            Text {textFormat:Text.PlainText; id: bannerLabel; anchors.centerIn: parent; text: root.style.label || ""; color: root.ink; font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: (root.style.labelSize || (root.kind === "recent" ? 12 : 18)) * root.textScale }
        }
        Rectangle { anchors.fill: parent; anchors.margins: -3 * root.unit; color: "transparent"; radius: parent.radius + 3 * root.unit; border.width: root.selected ? Math.max(2,root.style.focusEnabled===false?2:(root.style.focusWidth===undefined?3:root.style.focusWidth))*root.unit : 0; border.color: root.style.focusColor || root.selColor }
    }
    Text {
        objectName:"componentBodyText"
        visible:root.kind==="text";anchors.fill:parent;anchors.margins:16;text:root.style.bodyText===undefined?root.style.subtitle || root.style.label || root.item.name || "":root.style.bodyText;textFormat:Text.PlainText;wrapMode:Text.WordWrap;color:root.style.labelColor || root.ink;font.pixelSize:(root.style.labelSize || 22)*root.textScale;font.family:root.appearance.fontFamily || "DejaVu Sans";verticalAlignment:Text.AlignVCenter
    }
    Text {textFormat:Text.PlainText;
        visible: (root.kind === "media" || root.kind === "recent") && !root.customLabel && root.item.showTitle!==false; x: 0; y: surface.height + (root.kind === "recent" ? 5 : 12) * root.unit; width: parent.width
        text: root.title; color: root.ink; font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: (root.style.labelSize || (root.kind === "recent" ? 12 : 18)) * root.textScale
        maximumLineCount: 2; wrapMode: Text.WordWrap; elide: Text.ElideRight
    }
    Text {textFormat:Text.PlainText;
        visible: root.kind === "recent"; x: 0; y: surface.height + 22*root.unit; width: parent.width
        text: root.style.subtitle || ""; color: root.muted; font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: 11*root.textScale; elide: Text.ElideRight
    }
    OrbCard { id:orb;anchors.fill: parent;enabled:root.interactive; visible: root.kind === "orb"; item: root.item; style: root.style; appearance: root.appearance; unit: root.unit; selected: root.selected; onActivated: root.activated() }
    Rectangle {
        objectName: "resumeTrack"
        visible: root.kind === "media" && root.style.progress !== undefined
        anchors.bottom: parent.bottom; width: parent.width; height: 4 * root.unit; radius: height / 2; color: "#48414E"
        Rectangle { width: parent.width * (root.style.progress || 0); height: parent.height; radius: parent.radius; color: root.accent }
    }
    Rectangle {
        id:customLabelSurface
        visible:root.customLabel && root.style.labelPosition!=="hidden" && root.item.showTitle!==false
        x:0;y:root.style.labelPosition==="below"?(root.ellipse?root.bodyHeight:surface.height):Math.max(0,(root.ellipse?root.bodyHeight:surface.height)-height)
        width:parent.width;height:customTitle.implicitHeight+12*root.unit;radius:root.style.labelPill?height/2:0
        color:root.style.labelSurfaceColor || "transparent"
        Text {id:customTitle;anchors.centerIn:parent;width:parent.width-12*root.unit;text:root.title;textFormat:Text.PlainText;color:root.style.labelColor || root.ink;horizontalAlignment:root.style.textAlign==="left"?Text.AlignLeft:root.style.textAlign==="right"?Text.AlignRight:Text.AlignHCenter;wrapMode:root.style.labelWrap===false?Text.NoWrap:Text.Wrap;maximumLineCount:root.style.labelLines || 2;elide:Text.ElideRight;font.pixelSize:(root.style.labelSize || 20)*root.textScale;font.bold:!!root.style.labelBold;font.family:root.appearance.fontFamily || "DejaVu Sans"}
    }
    MouseArea { id: mouseArea; anchors.fill: parent;enabled:root.interactive; hoverEnabled: root.selectTrigger === "hover" || root.selectTrigger === "dwell"; onEntered: if (root.selectTrigger === "hover") root.activated(); onExited: dwellTimer.stop(); onClicked: if (root.selectAction !== "none") { if (root.selectAction === "preview") root.selected = true; else root.activated() } }
}
