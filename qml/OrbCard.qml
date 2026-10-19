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
    signal activated()
    Rectangle {
        id: circle; objectName: "orbSurface"
        width: root.diameter; height: width; radius: width/2; anchors.horizontalCenter: parent.horizontalCenter
        color: Qt.rgba(root.surface.r,root.surface.g,root.surface.b,root.style.surfaceOpacity === undefined ? 1 : root.style.surfaceOpacity)
        border.width: root.selected ? 4*root.unit : 1; border.color: root.selected ? root.appearance.focusColor || "white" : "#80FFFFFF"
        Rectangle { anchors.fill: parent; anchors.margins: -7*root.unit; radius: width/2; color:"transparent"; border.width: root.selected ? 7*root.unit : 0; border.color: "#30FFFFFF" }
        ArtworkImage {
            anchors.fill: parent; anchors.margins: root.selected ? 4*root.unit : 1
            radius: width/2; source: root.item.logoSource || ""; crop: root.style.imageCrop || null
        }
        LineIcon {
            visible: !root.item.logoSource
            anchors.centerIn: parent; width: (root.style.iconSize || 44)*root.unit; height: width
            name: root.style.icon || "display"; ink: root.style.iconColor || "white"
        }
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter; y: circle.height + 5*root.unit
        width: Math.min(root.width,label.implicitWidth+24*root.unit); height: 23*root.unit; radius: height/2
        color: root.style.labelPill === false ? "transparent" : "#85FFFFFF"
        Text {
            id: label; anchors.centerIn: parent; width: Math.min(implicitWidth,parent.width-8)
            text: root.item.name || ""; color: root.selected && root.style.selectedLabelColor ? root.style.selectedLabelColor : root.ink
            font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: (root.style.labelSize || 15)*root.unit*(root.appearance.typographyScale || 1)
            elide: Text.ElideRight; font.weight: root.selected ? Font.DemiBold : Font.Normal
        }
    }
    MouseArea { anchors.fill: parent; onClicked: root.activated() }
}
