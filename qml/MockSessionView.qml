import QtQuick

Rectangle {
    id: root
    required property string appName
    property string presentationMode: "native-tv"
    property color accent: "#64748B"
    color: "#070B15"

    Rectangle {
        anchors.fill: parent
        opacity: 0.18
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.accent }
            GradientStop { position: 0.6; color: "#070B15" }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 18
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: appName.toUpperCase()
            color: "#F8FAFC"
            font.pixelSize: 58
            font.weight: Font.DemiBold
            font.letterSpacing: 4
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.presentationMode === "desktop-bridge" ? "DESKTOP BRIDGE · NOT TV READY" :
                  root.presentationMode === "capture" ? "CAPTURE INPUT SESSION UNAVAILABLE" :
                  root.presentationMode === "game-hub" ? "GAME HUB PLACEHOLDER" :
                  root.presentationMode === "game" ? "GAME SESSION PLACEHOLDER" :
                  root.presentationMode === "disc-media" ? "DISC MEDIA PLACEHOLDER" :
                  root.presentationMode === "disc-game" ? "DISC GAME PLACEHOLDER" :
                  root.presentationMode === "utility-desktop" ? "UTILITY DESKTOP · ADVANCED" :
                  root.presentationMode === "media" ? "MEDIA SESSION PLACEHOLDER" :
                  "SMART TV SESSION PLACEHOLDER"
            color: "#AAB5C9"
            font.pixelSize: 19
            font.letterSpacing: 5
        }
        Text {
            visible: root.presentationMode === "desktop-bridge" || root.presentationMode === "utility-desktop"
            anchors.horizontalCenter: parent.horizontalCenter
            width: 760
            horizontalAlignment: Text.AlignHCenter
            text: root.presentationMode === "utility-desktop"
                  ? "Desktop Mode is an intentional advanced escape hatch for files, troubleshooting, and things that are not TV apps. It should be used rarely and always provide a clear path back to Isolde."
                  : "This app is using a real Linux desktop client for development. The finished experience must be full-screen, remote-first, and free of desktop chrome before it counts as TV-ready."
            color: "#D5B083"
            font.pixelSize: 18
            wrapMode: Text.WordWrap
        }
    }

    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 40
        text: "ESC  Back     H  Home"
        color: "#73819B"
        font.pixelSize: 16
    }
}
