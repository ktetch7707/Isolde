import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    property bool passwordMode: false
    property var appearance: ({})
    property string hint: "D-pad choose    Enter type/select    Esc cancel"
    property string value: ""
    property int selectedIndex: 0
    property bool shift: false
    readonly property var keys: shift
        ? ["1","2","3","4","5","6","7","8","9","0","Q","W","E","R","T","Y","U","I","O","P","A","S","D","F","G","H","J","K","L","-","Shift","Z","X","C","V","B","N","M","_","Del","Space",".","@","#","!","?",",",":","/","{","}","\\","Clear","Done"]
        : ["1","2","3","4","5","6","7","8","9","0","q","w","e","r","t","y","u","i","o","p","a","s","d","f","g","h","j","k","l","-","Shift","z","x","c","v","b","n","m","_","Del","Space",".","@","#","!","?",",",":","/","Clear","Done"]
    readonly property int columns: 10
    signal accepted(string text)
    signal canceled()

    function handleCommand(command) {
        if (command === inputManager.navigateLeft) {
            selectedIndex = Math.max(0, selectedIndex - 1)
        } else if (command === inputManager.navigateRight) {
            selectedIndex = Math.min(keys.length - 1, selectedIndex + 1)
        } else if (command === inputManager.navigateUp) {
            selectedIndex = Math.max(0, selectedIndex - columns)
        } else if (command === inputManager.navigateDown) {
            selectedIndex = Math.min(keys.length - 1, selectedIndex + columns)
        } else if (command === inputManager.back) {
            canceled()
        } else if (command === inputManager.confirm) {
            press(keys[selectedIndex])
        }
    }

    function press(key) {
        if (key === "Shift") {
            shift = !shift
        } else if (key === "Del") {
            value = value.slice(0, -1)
        } else if (key === "Space") {
            value += " "
        } else if (key === "Clear") {
            value = ""
        } else if (key === "Done") {
            accepted(value)
        } else {
            value += key
            if (shift)
                shift = false
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: shell.radius
        color: shell.surface
        border.width: 2
        border.color: shell.focus

        Column {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 14 * shell.gapScale

            Rectangle {
                width: parent.width
                height: 50
                radius: shell.radius
                color: shell.input
                border.width: 1
                border.color: shell.outline
                Text { font.family: shell.fontFamily;
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    text: root.passwordMode && root.value.length > 0 ? "•".repeat(root.value.length) : root.value
                    color: shell.inputText
                    font.pixelSize: 20 * shell.textScale
                    elide: Text.ElideRight
                }
            }

            Grid {
                width: parent.width
                columns: root.columns
                columnSpacing: 8 * shell.gapScale
                rowSpacing: 8 * shell.gapScale
                Repeater {
                    model: root.keys
                    delegate: Rectangle {
                        required property int index
                        required property string modelData
                        width: (parent.width - parent.columnSpacing * (root.columns - 1)) / root.columns
                        height: 40
                        radius: shell.radius
                        color: root.selectedIndex === index ? shell.selected : shell.surface
                        border.width: root.selectedIndex === index ? 3 : 1
                        border.color: root.selectedIndex === index ? shell.selectedFocus : shell.outline
                        Text { font.family: shell.fontFamily;
                            anchors.centerIn: parent
                            text: modelData
                            color: root.selectedIndex === index ? shell.selectedText : shell.text
                            font.pixelSize: modelData.length > 1 ? 13 : 18
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }

            Text { font.family: shell.fontFamily;
                width:parent.width;wrapMode:Text.Wrap
                text: root.hint
                color: shell.muted
                font.pixelSize: 15 * shell.textScale
            }
        }
    }
}
