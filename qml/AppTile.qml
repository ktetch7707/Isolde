import QtQuick

Rectangle {
    id: root
    required property var app
    required property bool selected
    required property real roundness
    property var appearance: ({})
    property var style: ({})
    property bool frameFocus: false
    property bool centerContent: false
    readonly property real glowSpread:style.iconGlow===undefined?8:style.iconGlow
    readonly property real glowPad:2*glowSpread
    readonly property real preferredLogoSize:style.iconSize || (compact?52:70)
    readonly property real contentMargin:centerContent?Math.min(16,height*.12):(compact?16:24)
    readonly property real iconArea:Math.max(0,height-2*contentMargin-(captions.visible?captions.height:0))
    readonly property real logoSize:centerContent?Math.min(preferredLogoSize,iconArea,width*.8):preferredLogoSize
    readonly property real artworkY:centerContent?contentMargin+(iconArea-logoSize)/2:(compact?16:24)
    readonly property real artworkX:centerContent?(width-logoSize)/2:(compact?20:24)
    readonly property color pageSurface:appearance.surfaceColor || color
    readonly property bool contrastBacking:!!app.iconContrastBackdrop && (pageSurface.r+pageSurface.g+pageSurface.b)/3>0.65
    property string selectMode: "single"
    property string selectTrigger: "click"
    property string selectAction: "navigate"
    property string selectionColor: ""
    readonly property real textScale: appearance.typographyScale || 1
    readonly property bool compact: height < 180
    signal activated()
    signal selectionChanged(bool selected)
    SettingsUpdateBadge { anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 4; z: 50; item: root.app }
    onSelectedChanged: root.selectionChanged(root.selected)
    readonly property color selColor: root.selectionColor !== "" ? root.selectionColor : (style.focusColor || appearance.focusColor || "#C994A5")
    readonly property color fillInk: selected ? appearance.selectedColor || "#353039" : style.surfaceColor || appearance.surfaceColor || "#202126"
    radius: style.tileRadius===undefined?Math.min(width,height)*0.5*roundness:Math.min(style.tileRadius,width/2,height/2)
    color: Qt.rgba(fillInk.r,fillInk.g,fillInk.b,fillInk.a*(style.surfaceOpacity ?? 1))
    border.width:selected && (frameFocus || !app.cutoutLogoSource)?(frameFocus?3:4):1
    border.color:selected && (frameFocus || !app.cutoutLogoSource)?root.selColor:Qt.lighter(color,1.2)
    scale: selected && !frameFocus ? 1.025 : 1.0
    readonly property bool showTitle: app.showTitle === undefined ? true : app.showTitle
    readonly property bool showType: app.showType === undefined ? true : app.showType
    readonly property bool showLogo: app.showLogo === undefined ? true : app.showLogo
    readonly property bool logoFill: app.logoFill === undefined ? false : app.logoFill
    readonly property string sectionLabel: app.contextual === true && app.sourceType ? app.sourceType.toUpperCase().replace("-", " ") : (app.section || "").toUpperCase()

    Behavior on scale { enabled: root.appearance.motionEnabled !== false; NumberAnimation { duration: 100 } }
    Behavior on color { enabled: root.appearance.motionEnabled !== false; ColorAnimation { duration: 100 } }

    Rectangle {
        id: logoBackground
        visible: root.showLogo && !root.app.logoSource
        anchors.fill: root.logoFill ? parent : undefined
        x: root.artworkX
        y: root.artworkY
        width: root.logoFill ? parent.width : root.centerContent?root.logoSize:compact?52:70
        height: root.logoFill ? parent.height : width
        radius: root.logoFill ? root.radius : Math.min(width, height) * 0.25 * root.roundness
        color: root.app.accent || root.appearance.focusColor || root.color
        Text {
            anchors.centerIn: parent
            text: root.app.symbol || ""
            color: "white"
        font.pixelSize: Math.max(1,Math.min(root.logoSize*.6,root.compact ? 23 : 30))
            font.weight: Font.Bold
        }
    }

    Rectangle {visible:root.showLogo && !root.logoFill && root.contrastBacking;x:root.artworkX;y:root.artworkY;width:root.logoSize;height:width;radius:width/4;color:root.app.iconContrastBackdrop || "transparent"}
    ArtworkImage {
        objectName:"appTileArtwork"
        visible: root.showLogo && !!root.app.logoSource
        source:!root.logoFill?root.app.cutoutLogoSource || root.app.logoSource || "":root.app.logoSource || ""
        anchors.fill: root.logoFill ? parent : undefined
        x:root.artworkX-root.glowPad;y:root.artworkY-root.glowPad
        contentInset:root.logoFill?0:root.glowPad+(root.style.iconPadding || 0)
        width:root.logoFill?parent.width:root.logoSize+2*root.glowPad
        height:root.logoFill?parent.height:root.logoSize+2*root.glowPad
        fit:root.logoFill?"cover":"contain"
        outlineWidth:root.selected && !root.logoFill && !root.frameFocus?(root.style.focusWidth===undefined?2:root.style.focusWidth):0;outlineColor:root.selColor
        glowWidth:root.selected && !root.logoFill && !root.frameFocus?root.glowSpread:0;glowOpacity:root.style.iconGlowOpacity===undefined?.65:root.style.iconGlowOpacity
    }

    Rectangle {
        visible: root.logoFill && (root.showTitle || root.showType)
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: parent.height * 0.45; color: "#99050A12"
    }

    Column {
        id:captions
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: root.contentMargin
        spacing: root.compact ? 2 : 5
        visible: root.showTitle || root.showType
        Text { objectName:"appTileTitle"; visible: root.showTitle; width: parent.width; horizontalAlignment:root.centerContent?Text.AlignHCenter:Text.AlignLeft;elide: Text.ElideRight; text: root.app.name; color: (root.selected ? root.appearance.selectedTextColor : root.appearance.textColor) || root.appearance.textColor || "#F5F1F3"; font.family:root.appearance.fontFamily || "DejaVu Sans";font.pixelSize: (root.centerContent?20:root.compact?22:27) * root.textScale; font.weight: Font.DemiBold }
        Text { visible: root.showType; width: parent.width; horizontalAlignment:root.centerContent?Text.AlignHCenter:Text.AlignLeft;elide: Text.ElideRight; text: root.centerContent?(root.app.section || ""):root.sectionLabel; color: (root.selected ? root.appearance.selectedTextColor || root.appearance.textColor : root.appearance.mutedColor) || "#AAA5AF";font.family:root.appearance.fontFamily || "DejaVu Sans";font.pixelSize: (root.compact ? 11 : 13) * root.textScale; font.letterSpacing:root.centerContent?0:2 }
    }

    MouseArea { id: mouseArea; anchors.fill: parent; hoverEnabled: root.selectTrigger === "hover" || root.selectTrigger === "dwell"; onEntered: if (root.selectTrigger === "hover") root.activated(); onClicked: if (root.selectAction !== "none") root.activated() }
}
