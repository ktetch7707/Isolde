import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import QtQuick.Controls.Universal

Item {
    id: root
    property var appearance: ({})
    property string mode: "slideshow"
    property real rotation: 0
    property real idleTimer: 0
    property var photos: []
    property string clockTime: new Date().toLocaleTimeString("en-GB", {hour: "2-digit", minute: "2-digit", hour12: false})
    property bool reducedMotion: false
    property bool paused: false
    property string heroUrl: ""
    property var wallpaperSource: ""
    property string heroText: ""
    signal dismissed()
    signal idle()
    signal resumed()
    signal imageLoaded()

    readonly property real textScale: appearance.typographyScale || 1
    readonly property color textColor: appearance.textColor || "#FFFFFF"
    readonly property color bgColor: appearance.backgroundColor || "#0B1020"
    readonly property color textMuted: appearance.textMutedColor || "#8A9AA8"
    readonly property color accentColor: appearance.accentColor || "#5B8DEF"
    readonly property real textMutedAlpha: reducedMotion ? 0.5 : 0.8

    property real clockX: 30
    property real clockY: 90
    property real dateX: 30
    property real dateY: 140
    property real clockWidth: 480
    property real dateWidth: 480

    property real systemHintY: 60

    Timer {
        id: clockTimer
        interval: 1000
        repeat: mode === "clock"
        triggeredOnTimeout: true
        onTriggered: root.clockTime = new Date().toLocaleTimeString("en-GB", {hour: "2-digit", minute: "2-digit", hour12: false})
    }

    Component.onDestruction: {
        if (clockTimer !== undefined) {
            clockTimer.stop()
            clockTimer.destroy()
        }
    }

    Rectangle {
        id: background
        anchors.fill: parent
        color: shell.background || "#0B1020"
        z: 0

        if (root.mode === "slideshow" && root.photos.length > 0) {
            Image {
                id: slideImage
                anchors.fill: parent
                source: root.photos[0]
                fillMode: root.appearance.imageFit === "fit" ? Image.PreserveAspectFit : (root.appearance.imageFit === "stretch" ? Image.PreserveAspectFill : Image.Stretch)
                smooth: true
                cache: false
                asynchronous: true
                onLoaded: {
                    root.imageLoaded()
                    if (root.heroUrl.length > 0 && slideImage.source === root.heroUrl) {
                        Image {
                            anchors.fill: parent
                            source: root.heroUrl
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            cache: false
                            z: 1
                        }
                    }
                }
                onSourceChanged: root.imageLoaded()
            }
        } else if (root.mode === "gradient") {
            Rectangle {
                anchors.fill: parent
                color: root.appearance.gradientStart || "#1a1a2e"
                border.color: root.accentColor
                border.width: 2
            }
        } else if (root.mode === "dim") {
            Rectangle {
                anchors.fill: parent
                color: root.bgColor
                opacity: 0.7
                border.color: root.accentColor
                border.width: 2
            }
        }

        if (root.mode === "clock") {
            Text {
                id: clockText
                anchors { left: parent.left; top: parent.top; margins: 30 }
                width: root.clockWidth * root.textScale
                color: root.textColor
                font.pixelSize: 140 * root.textScale
                font.weight: Font.DemiBold
                text: root.clockTime
                clip: true
            }
            Text {
                id: dateText
                anchors { left: parent.left; top: parent.top; topMargin: 150 * root.textScale }
                width: root.dateWidth * root.textScale
                color: root.textColor
                font.pixelSize: 42 * root.textScale
                font.weight: Font.Normal
                text: root.clockTime.toLocaleDateString("en-GB", {weekday: "long", day: "numeric", month: "long", year: "numeric"})
                clip: true
            }
            Rectangle {
                anchors { fill: parent; margins: 20; radius: 8 }
                opacity: root.paused ? 0.3 : 0.6
                color: root.accentColor
                Text {
                    anchors { centerIn: parent; top: parent.top; topMargin: 80; left: parent.left; leftMargin: 80 }
                    text: root.clockTime
                    color: "#FFFFFF"
                    font.pixelSize: 64 * root.textScale
                    font.weight: Font.DemiBold
                }
            }
        }

        Text {
            id: systemHint
            anchors { bottom: parent.bottom; left: parent.left; margins: 30 }
            width: 600 * root.textScale
            color: root.textColor
            font.pixelSize: 24 * root.textScale
            font.italic: true
            text: root.reducedMotion ? "Reduced motion active" : (root.mode === "clock" ? "Alive · press any key to return" : "Idle · press any key to return")
            clip: true
        }

        Rectangle {
            anchors { fill: parent; margins: 20; radius: 8 }
            opacity: root.mode === "clock" ? 0.6 : root.paused ? 0.3 : 0
            color: root.accentColor
        }

        if (root.mode === "clock") {
            Text {
                anchors { centerIn: parent; top: parent.top; topMargin: 80; left: parent.left; leftMargin: 80 }
                text: root.clockTime
                color: "#FFFFFF"
                font.pixelSize: 64 * root.textScale
                font.weight: Font.DemiBold
            }
        }

        MouseArea {
            id: tapArea
            anchors.fill: parent
            enabled: true
            onPressed: { root.paused = false; root.resumed() }
            onDoubleClicked: root.paused = true
            onReleased: { root.paused = !root.paused; if (root.paused) root.resumed() }
        }
    }
}