import QtQuick

Item {
    id: root
    required property var item
    property string variant: "source"
    property var appearance: ({})
    property bool selected: false
    property string selectMode: "single"
    property string selectTrigger: "click"
    property string selectAction: "navigate"
    property string selectionColor: ""
    property color outsideColor: "#0e1018"
    property real unit: 1
    property date now: new Date()
    readonly property string title: item.name || (item.widgetType === "clock" ? Qt.formatTime(now,"h:mm") : "")
    Timer { interval: 1000; running: root.visible && root.item.widgetType === "clock"; repeat: true; onTriggered: root.now = new Date() }
    Timer { id: dwellTimer; interval: 700; repeat: false; running: root.selectTrigger === "dwell" && root.selected === false && mouseArea.containsMouse; onTriggered: { mouseArea.forceActiveFocus(); root.activated() } }
    readonly property var style: item.presentation || ({})
    readonly property string kind: item.widgetType === "text" ? "text" : (style.variant || variant) === "launcher" ? "source" : (style.variant || variant)
    readonly property color ink: appearance.textColor || "#EEEAF0"
    readonly property color muted: appearance.mutedColor || "#AAA4B2"
    readonly property color accent: appearance.focusColor || "#BD8198"
    readonly property color selColor: root.selectionColor !== "" ? root.selectionColor : root.accent
    readonly property real textScale: (appearance.typographyScale || 1) * unit
    readonly property real corner: Math.min((style.radius || 22) * unit, Math.min(width, height) * (appearance.tileRoundness === undefined ? 0.2 : appearance.tileRoundness))
    signal activated()
    signal selectionChanged(bool selected)
    onSelectedChanged: root.selectionChanged(root.selected)
    Rectangle {
        id: surface
        objectName: "cardSurface"
        visible: root.kind !== "orb"
        width: parent.width; height: root.kind === "recent" ? Math.min(parent.height - 36*root.unit,(root.style.artHeight || 86)*root.unit) : root.kind === "media" ? Math.max(35, parent.height - 58 * root.unit) : parent.height
        radius: root.corner
        color: root.style.surfaceColor || root.appearance.surfaceColor || "#28262F"
        border.width: root.kind === "recent" ? 1 : 0; border.color: "#B0FFFFFF"
        ArtworkImage {
            objectName: "cardArtwork"
            anchors.fill: parent; source: root.kind === "orb" ? "" : root.item.logoSource || ""
            crop: root.style.imageCrop || null; radius: surface.radius
        }
        LineIcon {
            visible: (root.kind === "source" || root.kind === "summary") && root.item.widgetType !== "clock"
            name: root.style.icon || "display"; ink: root.kind === "summary" ? root.accent : root.ink
            width: 28 * root.unit; height: width
            x: root.kind === "source" ? (parent.width - width) / 2 : 24 * root.unit
            y: root.kind === "source" ? parent.height * 0.21 : (parent.height - height) / 2
        }
        Text {
            visible: root.kind === "source"; anchors.bottom: parent.bottom; anchors.bottomMargin: 16 * root.unit
            width: parent.width - 16; anchors.horizontalCenter: parent.horizontalCenter; horizontalAlignment: Text.AlignHCenter
            text: root.title; color: root.ink; font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: 19 * root.textScale; elide: Text.ElideRight
        }
        Column {
            visible: root.kind === "summary"; x: 72 * root.unit; anchors.verticalCenter: parent.verticalCenter
            width: parent.width - x - 55 * root.unit; spacing: 5 * root.unit
            Text { width: parent.width; text: root.title; color: root.ink; font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: 21 * root.textScale; elide: Text.ElideRight }
            Text { width: parent.width; text: root.style.subtitle || ""; color: root.muted; font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: 16 * root.textScale; elide: Text.ElideRight }
        }
        Text { visible: root.kind === "summary"; anchors.right: parent.right; anchors.rightMargin: 24 * root.unit; anchors.verticalCenter: parent.verticalCenter; text: "›"; color: root.ink; font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: 32 * root.unit }
        Rectangle {
            visible: root.kind === "banner" && !!root.style.label
            x: 20 * root.unit; anchors.bottom: parent.bottom; anchors.bottomMargin: 18 * root.unit
            width: bannerLabel.implicitWidth + 24 * root.unit; height: 38 * root.unit; radius: 14 * root.unit; color: "#D5222521"
            Text { id: bannerLabel; anchors.centerIn: parent; text: root.style.label || ""; color: root.ink; font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: (root.style.labelSize || (root.kind === "recent" ? 12 : 18)) * root.textScale }
        }
        Rectangle { anchors.fill: parent; anchors.margins: -3 * root.unit; color: "transparent"; radius: parent.radius + 3 * root.unit; border.width: root.selected ? 3 * root.unit : 0; border.color: root.selColor }
    }
    Text {
        visible:root.kind==="text";anchors.fill:parent;anchors.margins:16;text:root.style.subtitle || root.style.label || root.title;textFormat:Text.PlainText;wrapMode:Text.WordWrap;color:root.style.labelColor || root.ink;font.pixelSize:(root.style.labelSize || 22)*root.textScale;font.family:root.appearance.fontFamily || "DejaVu Sans";verticalAlignment:Text.AlignVCenter
    }
    Text {
        visible: root.kind === "media" || root.kind === "recent"; x: 0; y: surface.height + (root.kind === "recent" ? 5 : 12) * root.unit; width: parent.width
        text: root.title; color: root.ink; font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: (root.style.labelSize || (root.kind === "recent" ? 12 : 18)) * root.textScale
        maximumLineCount: 2; wrapMode: Text.WordWrap; elide: Text.ElideRight
    }
    Text {
        visible: root.kind === "recent"; x: 0; y: surface.height + 22*root.unit; width: parent.width
        text: root.style.subtitle || ""; color: root.muted; font.family: root.appearance.fontFamily || "DejaVu Sans"; font.pixelSize: 11*root.textScale; elide: Text.ElideRight
    }
    OrbCard { anchors.fill: parent; visible: root.kind === "orb"; item: root.item; style: root.style; appearance: root.appearance; unit: root.unit; selected: root.selected; onActivated: root.activated() }
    Rectangle {
        objectName: "resumeTrack"
        visible: root.kind === "media" && root.style.progress !== undefined
        anchors.bottom: parent.bottom; width: parent.width; height: 4 * root.unit; radius: height / 2; color: "#48414E"
        Rectangle { width: parent.width * (root.style.progress || 0); height: parent.height; radius: parent.radius; color: root.accent }
    }
    MouseArea { id: mouseArea; anchors.fill: parent; hoverEnabled: root.selectTrigger === "hover" || root.selectTrigger === "dwell"; onEntered: if (root.selectTrigger === "hover") root.activated(); onExited: dwellTimer.stop(); onClicked: if (root.selectAction !== "none") { if (root.selectAction === "preview") root.selected = true; else root.activated() } }
}
