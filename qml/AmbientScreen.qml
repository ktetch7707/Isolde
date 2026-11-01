import QtQuick

Item {
    id: root
    property var settings: ambientSettings
    property bool active: false
    property int slideIndex: 0
    property date now: new Date()
    readonly property int photoCount: root.settings.photoUrls.length
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
        running: root.active && root.settings.mode === "slideshow"
        repeat: true
        interval: root.settings.slideshowSeconds * 1000
        onTriggered: root.slideIndex = root.settings.shuffle
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
            GradientStop { position: 0; color: root.settings.mode === "dim" ? "#02040A" : root.settings.mode === "gradient" ? "#101A32" : root.slides[root.slideIndex % root.slides.length].start }
            GradientStop { position: 1; color: root.settings.mode === "dim" ? "#02040A" : root.settings.mode === "gradient" ? "#050812" : root.slides[root.slideIndex % root.slides.length].end }
        }
    }

    Image {
        visible: root.settings.mode === "slideshow" && root.photoCount > 0
        anchors.fill: parent
        source: root.photoCount > 0 ? root.settings.photoUrls[root.slideIndex % root.photoCount] : ""
        fillMode: root.settings.imageFit === "fit" ? Image.PreserveAspectFit : root.settings.imageFit === "stretch" ? Image.Stretch : Image.PreserveAspectCrop
        asynchronous: true
        cache: false
        opacity: 0.96
    }

    Rectangle {
        visible: root.settings.mode === "slideshow" && root.photoCount > 0
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: "#22000000" }
            GradientStop { position: 0.72; color: "#00000000" }
            GradientStop { position: 1; color: "#99000000" }
        }
    }

    Rectangle {
        visible: root.settings.mode === "slideshow" && root.photoCount === 0
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
            text: "Add PNG/JPG/WebP files to " + root.settings.photoDirectory
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
            visible: root.settings.showClock
            text: Qt.formatTime(root.now, "h:mm")
            color: "#F8FAFC"
            font.pixelSize: 76
            font.weight: Font.Light
        }
        Text {
            visible: root.settings.showDate
            text: Qt.formatDate(root.now, "dddd, MMMM d")
            color: "#B9C7DE"
            font.pixelSize: 22
        }
        Text {
            visible: root.settings.showSystemHint
            text: root.settings.mode.toUpperCase() + " · " + (root.photoCount > 0 ? root.photoCount + " photos · " + root.settings.imageFit : "built-in placeholders") + " · changes every " + root.settings.slideshowSeconds + "s · any key wakes"
            color: "#71809F"
            font.pixelSize: 15
            font.letterSpacing: 2
            topPadding: 12
        }
    }
}
