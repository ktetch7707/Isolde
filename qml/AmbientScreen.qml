import QtQuick

Item {
    id: root
    property bool active: false
    property int slideIndex: 0
    property date now: new Date()
    readonly property int photoCount: ambientSettings.photoUrls.length
    readonly property int effectiveSlideCount: Math.max(1, photoCount > 0 ? photoCount : slides.length)
    readonly property var slides: [
        { start: "#07111F", end: "#1B3358", label: "Midnight Harbor" },
        { start: "#180B2F", end: "#5A2D68", label: "Violet Drift" },
        { start: "#07251E", end: "#2F6B56", label: "Glass Forest" },
        { start: "#271609", end: "#8A4F2A", label: "Ember Room" }
    ]
    signal dismissRequested()

    function handleCommand(command) {
        if (active)
            dismissRequested()
    }

    visible: active
    opacity: active ? 1 : 0
    z: 90

    Timer {
        running: root.active && ambientSettings.mode === "slideshow"
        repeat: true
        interval: ambientSettings.slideshowSeconds * 1000
        onTriggered: root.slideIndex = ambientSettings.shuffle
            ? Math.floor(Math.random() * root.effectiveSlideCount)
            : (root.slideIndex + 1) % root.effectiveSlideCount
    }

    Timer {
        running: root.active
        repeat: true
        interval: 1000
        onTriggered: root.now = new Date()
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: ambientSettings.mode === "dim" ? "#02040A" : ambientSettings.mode === "gradient" ? "#101A32" : root.slides[root.slideIndex % root.slides.length].start }
            GradientStop { position: 1; color: ambientSettings.mode === "dim" ? "#02040A" : ambientSettings.mode === "gradient" ? "#050812" : root.slides[root.slideIndex % root.slides.length].end }
        }
    }

    Image {
        visible: ambientSettings.mode === "slideshow" && root.photoCount > 0
        anchors.fill: parent
        source: root.photoCount > 0 ? ambientSettings.photoUrls[root.slideIndex % root.photoCount] : ""
        fillMode: ambientSettings.imageFit === "fit" ? Image.PreserveAspectFit : ambientSettings.imageFit === "stretch" ? Image.Stretch : Image.PreserveAspectCrop
        asynchronous: true
        cache: false
        opacity: 0.96
    }

    Rectangle {
        visible: ambientSettings.mode === "slideshow" && root.photoCount > 0
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: "#22000000" }
            GradientStop { position: 0.72; color: "#00000000" }
            GradientStop { position: 1; color: "#99000000" }
        }
    }

    Rectangle {
        visible: ambientSettings.mode === "slideshow" && root.photoCount === 0
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.72, 860)
        height: Math.min(parent.height * 0.58, 430)
        radius: 28
        color: "#18FFFFFF"
        border.width: 1
        border.color: "#36FFFFFF"
        Text {
            anchors.centerIn: parent
            text: root.slides[root.slideIndex % root.slides.length].label
            color: "#DCE7F7"
            font.pixelSize: 34
            font.weight: Font.DemiBold
            font.letterSpacing: 4
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 26
            text: "Add PNG/JPG/WebP files to " + ambientSettings.photoDirectory
            color: "#91A0BC"
            font.pixelSize: 15
        }
    }

    Column {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 64
        anchors.bottomMargin: 54
        spacing: 4
        Text {
            visible: ambientSettings.showClock
            text: Qt.formatTime(root.now, "h:mm")
            color: "#F8FAFC"
            font.pixelSize: 76
            font.weight: Font.Light
        }
        Text {
            visible: ambientSettings.showDate
            text: Qt.formatDate(root.now, "dddd, MMMM d")
            color: "#B9C7DE"
            font.pixelSize: 22
        }
        Text {
            visible: ambientSettings.showSystemHint
            text: ambientSettings.mode.toUpperCase() + " · " + (root.photoCount > 0 ? root.photoCount + " photos · " + ambientSettings.imageFit : "built-in placeholders") + " · changes every " + ambientSettings.slideshowSeconds + "s · any key wakes"
            color: "#71809F"
            font.pixelSize: 15
            font.letterSpacing: 2
            topPadding: 12
        }
    }
}
