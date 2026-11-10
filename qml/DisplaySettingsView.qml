import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    Rectangle {anchors.fill:parent;z:-1;color:shell.surface}
    property var appearance: ({})
    property var layoutModel: homeLayout
    readonly property var appearanceModel: layoutModel.homePageId ? layoutModel.pageModel(layoutModel.homePageId) : layoutModel
    property string errorString: ""
    property int selectedIndex: 0
    readonly property var rows: [
        { name: "Safe Area", value: displaySettings.safeArea + "%", help: "Pulls Isolde's interface inward for TVs that crop the edges." },
        { name: "UI Scale", value: displaySettings.uiScale + "%", help: "Makes the living-room interface larger or smaller." },
        { name: "Reduced Motion", value: displaySettings.reduceMotion ? "On" : "Off", help: "Reduces animations and motion for accessibility." },
        { name: "High Contrast", value: layoutModel.homeAppearance.highContrast ? "On" : "Off", help: "Increases text and border contrast for visibility." },
        { name: "Reset Display", value: "Defaults", help: "Restore safe area, scale, motion, and contrast defaults." }
    ]

    function handleCommand(command) {
        if (command === inputManager.navigateUp)
            selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown)
            selectedIndex = Math.min(rows.length - 1, selectedIndex + 1)
        else if (command === inputManager.navigateLeft) {
            if (selectedIndex === 0) displaySettings.adjustSafeArea(-1)
            else if (selectedIndex === 1) displaySettings.adjustUiScale(-5)
        } else if (command === inputManager.navigateRight) {
            if (selectedIndex === 0) displaySettings.adjustSafeArea(1)
            else if (selectedIndex === 1) displaySettings.adjustUiScale(5)
        } else if (command === inputManager.confirm) {
            if (selectedIndex === 4) {
                if (appearanceModel.setAppearance({highContrast:false})) { displaySettings.reset(); errorString = "" }
                else errorString = appearanceModel.errorString
            }
            else if (selectedIndex === 2) displaySettings.toggleReduceMotion()
            else if (selectedIndex === 3) {
                errorString = appearanceModel.setAppearance({highContrast:!layoutModel.homeAppearance.highContrast}) ? "" : appearanceModel.errorString
            }
        }
    }

    Column {
        anchors.fill: parent
        spacing: 18 * shell.gapScale

        Text { font.family: shell.fontFamily; text: "DISPLAY"; color: shell.text; font.pixelSize: 30 * shell.textScale; font.weight: Font.DemiBold; font.letterSpacing: 3 }
        Text { font.family: shell.fontFamily;
            width: parent.width
            text: "Tune Isolde for real TVs: cropped edges, couch-distance readability, and motion comfort."
            color: shell.muted
            font.pixelSize: 18 * shell.textScale
            wrapMode: Text.WordWrap
        }

        Rectangle {
            width: parent.width
            height: 210
            radius: 14
            color: shell.background
            border.color: shell.outline

            Rectangle {
                anchors.fill: parent
                anchors.margins: Math.round(parent.width * displaySettings.safeArea / 100)
                radius: shell.radius
                color: shell.elevated
                border.width: 3
                border.color: shell.elevatedFocus

                Text { font.family: shell.fontFamily;
                    anchors.centerIn: parent
                    text: "VISIBLE TV AREA"
                    color: shell.elevatedText
                    font.pixelSize: Math.round(20 * displaySettings.uiScale / 100)
                    font.weight: Font.DemiBold
                    font.letterSpacing: 3
                }
            }
        }

        Repeater {
            model: root.rows
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: parent.width
                height: 76
                radius: shell.radius
                color: root.selectedIndex === index ? shell.selected : shell.surface
                border.width: root.selectedIndex === index ? 3 : 1
                border.color: root.selectedIndex === index ? shell.selectedFocus : shell.outline

                Column {
                    anchors.left: parent.left
                    anchors.right: valueText.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 18
                    anchors.rightMargin: 16
                    spacing: 4 * shell.gapScale
                    Text { font.family: shell.fontFamily; text: modelData.name; color: (root.selectedIndex === index) ? shell.selectedText : shell.text; font.pixelSize: 20 * shell.textScale; font.weight: Font.DemiBold }
                    Text { font.family: shell.fontFamily; width: parent.width; text: modelData.help; color: (root.selectedIndex === index) ? shell.selectedText : shell.muted; font.pixelSize: 14 * shell.textScale; elide: Text.ElideRight }
                }

                Text { font.family: shell.fontFamily;
                    id: valueText
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    width: 150
                    horizontalAlignment: Text.AlignRight
                    text: modelData.value
                    color: (root.selectedIndex === index) ? shell.selectedText : shell.text
                    font.pixelSize: 18 * shell.textScale
                    font.weight: Font.DemiBold
                }
            }
        }

        Text { font.family: shell.fontFamily;
            text: root.errorString || "← → adjust    Enter toggle/cycle    Esc back"
            color: shell.unavailable
            font.pixelSize: 15 * shell.textScale
        }
    }
}
