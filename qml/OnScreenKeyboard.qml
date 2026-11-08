import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    property bool passwordMode: false
    property var appearance: ({})
        property string hint: mode === "letters" ? "D-pad choose    Enter type/select    Esc cancel" : (mode === "numbers" ? "D-pad choose    Enter    Esc cancel") : "D-pad choose    Enter type/select    Esc cancel"
    signal focusRestored()
    property string focusRestorerId: ""
    property string value: ""
    property int selectedIndex: 0
    property string mode: "letters"
    property bool shift: false
    readonly property var keys: (function(){
        var caps=["1","2","3","4","5","6","7","8","9","0","Q","W","E","R","T","Y","U","I","O","P","A","S","D","F","G","H","J","K","L","-","Shift","Z","X","C","V","B","N","M","_","Del","Space",".","@","#","!","?",":","/","{","}","\\","Clear","Done"]
        var lower=["1","2","3","4","5","6","7","8","9","0","q","w","e","r","t","y","u","i","o","p","a","s","d","f","g","h","j","k","l","-","z","x","c","v","b","n","m","_","Del","Space",".","@","#","!","?",":","/","{","}","\\","Clear","Done"]
        var numbers=["1","2","3","4","5","6","7","8","9","0",".","Del","Space","Done"]
        var symbols=["1","2","3","4","5","6","7","8","9","0","`","~","[","]",";","'","%","&","*","(",")","$","€","£","¥","_","+","|","\\","/","<",">","=","?","!","{","}","[","]",":",";",'"',"'",",","<",">","\\.","@","#","£","€","™","§","%","+","=","-","_","|","\\","/"]
        if (mode==="numbers") return numbers
        if (mode==="symbols") return symbols
        if (mode==="letters" && shift) return caps
        return lower
    })()
        ? ["1","2","3","4","5","6","7","8","9","0","Q","W","E","R","T","Y","U","I","O","P","A","S","D","F","G","H","J","K","L","-","Shift","Z","X","C","V","B","N","M","_","Del","Space",".","@","#","!","?",",",":","/","{","}","\\","Clear","Done"]
        : ["1","2","3","4","5","6","7","8","9","0","q","w","e","r","t","y","u","i","o","p","a","s","d","f","g","h","j","k","l","-","Shift","z","x","c","v","b","n","m","_","Del","Space",".","@","#","!","?",",",":","/","Clear","Done"]
    readonly property int columns: mode === "numbers" ? 4 : 10
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
            if (mode === "symbols") {
                shift = !shift
            } else {
                shift = !shift
            }
        } else if (key === "Del") {
            value = value.slice(0, -1)
        } else if (key === "Space") {
            value += " "
        } else if (key === "Clear") {
            value = ""
        } else if (key === "Done") {
            accepted(value)
            if (focusRestorerId.length) {
                focusRestored()
            }
        } else {
            value += key
            if (mode === "symbols" && shift) {
                shift = false
            } else if (shift) {
                shift = false
            }
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
                            font.pixelSize: 18 * (modelData === "Shift" ? 0.85 : 1)
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }
            Row {
                width: parent.width
                height: 48
                spacing: 12 * shell.gapScale
                Repeater {
                    model: ["letters", "numbers", "symbols"]
                    delegate: Rectangle {
                        width: (parent.width - 24 * shell.gapScale) / 3
                        height: 36
                        radius: shell.radius
                        color: root.mode === modelData ? shell.selected : shell.surface
                        border.width: root.mode === modelData ? 2 : 1
                        border.color: root.mode === modelData ? shell.selectedFocus : shell.outline
                        Text { font.family: shell.fontFamily; anchors.fill: parent; anchors.margins: 8
                            text: modelData
                            color: root.mode === modelData ? shell.selectedText : shell.backgroundText
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        MouseArea { anchors.fill: parent
                            onClicked: { root.mode = modelData }
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
