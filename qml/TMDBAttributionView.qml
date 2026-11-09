import QtQuick

Item {
    id: root
    property var appearance: ({})
    signal backRequested()
    SurfacePalette { id: shell; appearance: root.appearance }

    function handleCommand(command) {
        if (command === inputManager.back || command === inputManager.menu) {
            backRequested()
            return
        }
        if (command === inputManager.confirm)
            Qt.openUrlExternally("https://www.themoviedb.org")
    }

    Rectangle { anchors.fill: parent; color: shell.surface }
    Column {
        anchors.fill: parent
        anchors.margins: 48
        spacing: 22
        Text {
            text: "ABOUT ISOLDE · DATA ATTRIBUTION"
            color: shell.text
            font.pixelSize: 28 * shell.textScale
            font.weight: Font.DemiBold
            font.letterSpacing: 1
        }
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "This product uses the TMDB API but is not endorsed or certified by TMDB."
            color: shell.text
            font.pixelSize: 22 * shell.textScale
            font.weight: Font.Medium
        }
        Image {
            source: "qrc:/isolde/tmdb-logo.svg"
            sourceSize.width: 150
            sourceSize.height: 20
            width: 150
            height: 20
            fillMode: Image.PreserveAspectFit
            Accessible.name: "The Movie Database logo"
        }
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "TMDB provides optional online discovery metadata. Its provider listings are informational and do not confirm service availability, subscription, access, or entitlement."
            color: shell.muted
            font.pixelSize: 17 * shell.textScale
        }
        Rectangle {
            width: 390
            height: 64
            radius: shell.radius
            color: shell.selected
            border.width: 2
            border.color: shell.selectedFocus
            Text {
                anchors.fill: parent
                anchors.margins: 16
                verticalAlignment: Text.AlignVCenter
                text: "Confirm · Open themoviedb.org"
                color: shell.selectedText
                font.pixelSize: 18 * shell.textScale
                font.weight: Font.DemiBold
            }
        }
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "Official approved TMDB logo and current API terms: themoviedb.org/about/logos-attribution. Commercial deployments may require a separate license."
            color: shell.muted
            font.pixelSize: 14 * shell.textScale
        }
    }
}
