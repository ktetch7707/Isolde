import QtQuick

Item {
    id: root
    property int selectedIndex: 0
    readonly property var rows: [
        { name: "Safe Area", value: displaySettings.safeArea + "%", help: "Pulls Isolde's interface inward for TVs that crop the edges." },
        { name: "UI Scale", value: displaySettings.uiScale + "%", help: "Makes the living-room interface larger or smaller." },
        { name: "Reduced Motion", value: "Unavailable", help: "Animation reduction is not implemented yet." },
        { name: "Reset Display", value: "Defaults", help: "Restore safe area, scale, and motion defaults." }
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
            if (selectedIndex === 3) displaySettings.reset()
        }
    }

    Column {
        anchors.fill: parent
        spacing: 18

        Text { text: "DISPLAY"; color: "#F8FAFC"; font.pixelSize: 30; font.weight: Font.DemiBold; font.letterSpacing: 3 }
        Text {
            width: parent.width
            text: "Tune Isolde for real TVs: cropped edges, couch-distance readability, and motion comfort."
            color: "#9EABC1"
            font.pixelSize: 18
            wrapMode: Text.WordWrap
        }

        Rectangle {
            width: parent.width
            height: 210
            radius: 14
            color: "#080C18"
            border.color: "#26344F"

            Rectangle {
                anchors.fill: parent
                anchors.margins: Math.round(parent.width * displaySettings.safeArea / 100)
                radius: 10
                color: "#121E34"
                border.width: 3
                border.color: "#F8FAFC"

                Text {
                    anchors.centerIn: parent
                    text: "VISIBLE TV AREA"
                    color: "#F8FAFC"
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
                radius: 10
                color: root.selectedIndex === index ? "#2B416B" : "#111A2A"
                border.width: root.selectedIndex === index ? 3 : 1
                border.color: root.selectedIndex === index ? "#F8FAFC" : "#2B3853"

                Column {
                    anchors.left: parent.left
                    anchors.right: valueText.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 18
                    anchors.rightMargin: 16
                    spacing: 4
                    Text { text: modelData.name; color: "#F8FAFC"; font.pixelSize: 20; font.weight: Font.DemiBold }
                    Text { width: parent.width; text: modelData.help; color: "#9EABC1"; font.pixelSize: 14; elide: Text.ElideRight }
                }

                Text {
                    id: valueText
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    width: 150
                    horizontalAlignment: Text.AlignRight
                    text: modelData.value
                    color: "#C8D3E5"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
            }
        }

        Text {
            text: "← → adjust    Enter toggle/reset    Esc back"
            color: "#73819B"
            font.pixelSize: 15
        }
    }
}
