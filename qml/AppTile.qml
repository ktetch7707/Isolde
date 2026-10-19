import QtQuick

Rectangle {
    id: root
    required property var app
    required property bool selected
    required property real roundness
    property var appearance: ({})
    property string selectMode: "single"
    property string selectTrigger: "click"
    property string selectAction: "navigate"
    property color selectionColor: ""
    readonly property real textScale: appearance.typographyScale || 1
    readonly property bool compact: height < 180
    signal activated()
    signal selectionChanged(bool selected)
    onSelectedChanged: root.selectionChanged(root.selected)
    readonly property color selColor: root.selectionColor !== "" ? root.selectionColor : (appearance.focusColor || "#C994A5")
    radius: Math.min(width, height) * 0.5 * roundness
    color: selected ? appearance.selectedColor || "#353039" : appearance.surfaceColor || "#202126"
    border.width: selected ? 4 : 1
    border.color: selected ? root.selColor : Qt.lighter(color, 1.2)
    scale: selected ? 1.025 : 1.0
    readonly property bool showTitle: app.showTitle === undefined ? true : app.showTitle
    readonly property bool showType: app.showType === undefined ? true : app.showType
    readonly property bool showLogo: app.showLogo === undefined ? true : app.showLogo
    readonly property bool logoFill: app.logoFill === undefined ? false : app.logoFill
    readonly property string sectionLabel: app.contextual === true && app.sourceType ? app.sourceType.toUpperCase().replace("-", " ") : app.section.toUpperCase()

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
        color: root.app.accent
        Text {
            anchors.centerIn: parent
            text: root.app.symbol
            color: "white"
            font.pixelSize: root.compact ? 23 : 30
            font.weight: Font.Bold
        }
    }

    Image {
        visible: root.showLogo && !!root.app.logoSource
        source: root.app.logoSource || ""
        anchors.fill: root.logoFill ? parent : undefined
        x: compact ? 20 : 24; y: compact ? 16 : 24
        width: root.logoFill ? parent.width : compact ? 52 : 70
        height: root.logoFill ? parent.height : compact ? 52 : 70
        fillMode: root.logoFill ? Image.PreserveAspectCrop : Image.PreserveAspectFit
        mipmap: true
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
        Text { visible: root.showTitle; width: parent.width; elide: Text.ElideRight; text: root.app.name; color: root.appearance.textColor || "#F5F1F3"; font.pixelSize: (root.compact ? 22 : 27) * root.textScale; font.weight: Font.DemiBold }
        Text { visible: root.showType; width: parent.width; elide: Text.ElideRight; text: root.sectionLabel; color: root.appearance.mutedColor || "#AAA5AF"; font.pixelSize: (root.compact ? 11 : 13) * root.textScale; font.letterSpacing: 2 }
    }

    MouseArea { id: mouseArea; anchors.fill: parent; hoverEnabled: root.selectTrigger === "hover" || root.selectTrigger === "dwell"; onEntered: if (root.selectTrigger === "hover") root.activated(); onClicked: if (root.selectAction !== "none") root.activated() }
}
