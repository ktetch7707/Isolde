import QtQuick

Rectangle {
    id: root
    required property var app
    required property bool selected
    required property real roundness
    property var appearance: ({})
    property var style: ({})
    readonly property real glowSpread:style.iconGlow===undefined?8:style.iconGlow
    readonly property real glowPad:2*glowSpread
    readonly property real logoSize:style.iconSize || (compact?52:70)
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
    onSelectedChanged: root.selectionChanged(root.selected)
    readonly property color selColor: root.selectionColor !== "" ? root.selectionColor : (appearance.focusColor || "#C994A5")
    radius: Math.min(width, height) * 0.5 * roundness
    color: selected ? appearance.selectedColor || "#353039" : appearance.surfaceColor || "#202126"
    border.width:selected && !app.cutoutLogoSource?4:1
    border.color:selected && !app.cutoutLogoSource?root.selColor:Qt.lighter(color,1.2)
    scale: selected ? 1.025 : 1.0
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
        x: compact ? 20 : 24
        y: compact ? 16 : 24
        width: root.logoFill ? parent.width : compact ? 52 : 70
        height: root.logoFill ? parent.height : compact ? 52 : 70
        radius: root.logoFill ? root.radius : Math.min(width, height) * 0.25 * root.roundness
        color: root.app.accent || root.appearance.focusColor || root.color
        Text {
            anchors.centerIn: parent
            text: root.app.symbol || ""
            color: "white"
            font.pixelSize: root.compact ? 23 : 30
            font.weight: Font.Bold
        }
    }

    Rectangle {visible:root.showLogo && !root.logoFill && root.contrastBacking;x:root.compact?20:24;y:root.compact?16:24;width:root.logoSize;height:width;radius:width/4;color:root.app.iconContrastBackdrop || "transparent"}
    ArtworkImage {
        objectName:"appTileArtwork"
        visible: root.showLogo && !!root.app.logoSource
        source:!root.logoFill?root.app.cutoutLogoSource || root.app.logoSource || "":root.app.logoSource || ""
        anchors.fill: root.logoFill ? parent : undefined
        x:(root.compact?20:24)-root.glowPad;y:(root.compact?16:24)-root.glowPad
        contentInset:root.logoFill?0:root.glowPad+(root.style.iconPadding || 0)
        width:root.logoFill?parent.width:root.logoSize+2*root.glowPad
        height:root.logoFill?parent.height:root.logoSize+2*root.glowPad
        fit:root.logoFill?"cover":"contain"
        outlineWidth:root.selected && !root.logoFill?(root.style.focusWidth===undefined?2:root.style.focusWidth):0;outlineColor:root.selColor
        glowWidth:root.selected && !root.logoFill?root.glowSpread:0;glowOpacity:root.style.iconGlowOpacity===undefined?.65:root.style.iconGlowOpacity
    }

    Rectangle {
        visible: root.logoFill && (root.showTitle || root.showType)
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: parent.height * 0.45; color: "#99050A12"
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: root.compact ? 16 : 24
        spacing: root.compact ? 2 : 5
        visible: root.showTitle || root.showType
        Text { objectName:"appTileTitle"; visible: root.showTitle; width: parent.width; elide: Text.ElideRight; text: root.app.name; color: (root.selected ? root.appearance.selectedTextColor : root.appearance.textColor) || root.appearance.textColor || "#F5F1F3"; font.pixelSize: (root.compact ? 22 : 27) * root.textScale; font.weight: Font.DemiBold }
        Text { visible: root.showType; width: parent.width; elide: Text.ElideRight; text: root.sectionLabel; color: (root.selected ? root.appearance.selectedTextColor || root.appearance.textColor : root.appearance.mutedColor) || "#AAA5AF"; font.pixelSize: (root.compact ? 11 : 13) * root.textScale; font.letterSpacing: 2 }
    }

    MouseArea { id: mouseArea; anchors.fill: parent; hoverEnabled: root.selectTrigger === "hover" || root.selectTrigger === "dwell"; onEntered: if (root.selectTrigger === "hover") root.activated(); onClicked: if (root.selectAction !== "none") root.activated() }
}
