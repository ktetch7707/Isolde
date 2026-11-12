import QtQuick

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    property bool passwordMode: false
    property var appearance: ({})
    property string hint: "D-pad choose    Enter type/select    Esc cancel"
    signal focusRestored()
    property string focusRestorerId: ""
    property string value: ""
    property int selectedIndex: 0
    property string mode: "letters"
    property bool shift: false
    property int maximumLength: 4096
    property bool physicalTyping: false
    property string inputMessage: ""
    readonly property bool acceptTextKeys: inputManager.acceptTextKeys
    readonly property var keys: keyList()
    readonly property int columns: mode === "numbers" ? 4 : 10
    signal accepted(string text)
    signal canceled()

    function updateTextTarget() {
        inputManager.setTextInputTarget(root, root.visible && root.enabled)
        if (!root.visible) { physicalTyping = false; inputMessage = "" }
    }
    Component.onCompleted: updateTextTarget()
    onVisibleChanged: updateTextTarget()
    onEnabledChanged: updateTextTarget()
    Component.onDestruction: inputManager.setTextInputTarget(root, false)

    function appendText(text) {
        if (value.length + text.length > maximumLength) {
            inputMessage = "Text is too long. Delete some characters before typing more."
            return
        }
        value += text
        inputMessage = ""
    }
    function deleteCharacter() {
        let count = 1
        const last = value.charCodeAt(value.length - 1)
        const previous = value.charCodeAt(value.length - 2)
        if (last >= 0xDC00 && last <= 0xDFFF && previous >= 0xD800 && previous <= 0xDBFF) count = 2
        value = value.slice(0, Math.max(0, value.length - count))
        inputMessage = ""
    }
    function finish() {
        accepted(value)
        if (focusRestorerId.length) focusRestored()
    }
    function handlePhysicalKey(key, text, modifiers, repeated) {
        if (!visible || !enabled || !acceptTextKeys) return false
        if ((modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) && !(modifiers & Qt.GroupSwitchModifier))
            return true // do not let text-field chords trigger global letter shortcuts
        if (key === Qt.Key_Left || key === Qt.Key_Right || key === Qt.Key_Up || key === Qt.Key_Down) {
            physicalTyping = false
            return false
        }
        if (key === Qt.Key_Return || key === Qt.Key_Enter) {
            if (!physicalTyping) return false
            if (!repeated) finish()
            return true
        }
        if (key === Qt.Key_Backspace) {
            deleteCharacter(); physicalTyping = true; return true
        }
        // Use the delivered text (including Shift/layout), never synthesize letters from key codes.
        if (text.length && !/[\u0000-\u001f\u007f-\u009f]/.test(text)) {
            appendText(text); physicalTyping = true; return true
        }
        return false
    }

    function revealSelection() {
        const key = keyRepeater.itemAt(selectedIndex)
        if (!key) return
        const position = key.mapToItem(keyboardContent, 0, 0)
        if (position.y < keyboardFlickable.contentY)
            keyboardFlickable.contentY = position.y
        else if (position.y + key.height > keyboardFlickable.contentY + keyboardFlickable.height)
            keyboardFlickable.contentY = position.y + key.height - keyboardFlickable.height
    }
    onSelectedIndexChanged: Qt.callLater(revealSelection)
    onKeysChanged: Qt.callLater(revealSelection)
    onHeightChanged: Qt.callLater(revealSelection)
    function keyLabel(key) {
        if (key === "Accept keys") return acceptTextKeys ? "Keys: Text" : "Keys: Controls"
        const labels={"Del":"⌫","Space":"␣","Clear":"×","Done":"✓","Shift":"⇧","ABC":"Ab","123":"12","#+=":"#+"}
        return labels[key] || key
    }

    function keyList() {
        const modes = ["ABC", "123", "#+="]
        const actions = ["Del", "Space", "Clear", "Done"].concat(modes, ["Accept keys"])
        if (mode === "numbers")
            return ["1","2","3","4","5","6","7","8","9","0","."].concat(actions)
        if (mode === "symbols")
            return ["`","~","[","]",";","'","%","&","*","(",")","$","€","£","¥","™","§","_","+","|","\\","/","<",">","=","?","!","{","}",":",'"',",",".","@","#","-"].concat(actions)
        const letters = shift
            ? ["Q","W","E","R","T","Y","U","I","O","P","A","S","D","F","G","H","J","K","L","Shift","Z","X","C","V","B","N","M"]
            : ["q","w","e","r","t","y","u","i","o","p","a","s","d","f","g","h","j","k","l","Shift","z","x","c","v","b","n","m"]
        return ["1","2","3","4","5","6","7","8","9","0"].concat(letters, ["-","_",".","@","#","!","?",",",":","/","{","}","\\"], actions)
    }

    function handleCommand(command) {
        if (command === inputManager.navigateLeft || command === inputManager.navigateRight
            || command === inputManager.navigateUp || command === inputManager.navigateDown || command === inputManager.confirm)
            physicalTyping = false
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
        if (key === "ABC" || key === "123" || key === "#+=") {
            mode = key === "ABC" ? "letters" : key === "123" ? "numbers" : "symbols"
            selectedIndex = 0
            shift = false
        } else if (key === "Shift") {
            shift = !shift
        } else if (key === "Del") {
            deleteCharacter()
        } else if (key === "Space") {
            appendText(" ")
        } else if (key === "Clear") {
            value = ""
        } else if (key === "Done") {
            finish()
        } else if (key === "Accept keys") {
            inputMessage = inputManager.setAcceptTextKeys(!acceptTextKeys) ? "" : inputManager.errorString
        } else {
            appendText(key)
            if (shift) {
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

            Rectangle {
                id: valueField
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 18 }
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

        Flickable {
            id: keyboardFlickable
            objectName: "keyboardViewport"
            anchors { left: parent.left; right: parent.right; top: valueField.bottom; bottom: keyboardHint.top; margins: 18 }
            contentHeight: keyboardContent.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Column {
                id: keyboardContent
                width: keyboardFlickable.width
                spacing: 14 * shell.gapScale

            Grid {
                width: parent.width
                columns: root.columns
                columnSpacing: 8 * shell.gapScale
                rowSpacing: 8 * shell.gapScale
                Repeater {
                    id: keyRepeater
                    model: root.keys
                    delegate: Rectangle {
                        required property int index
                        required property string modelData
                        objectName: "keyboardKey-" + index
                        width: (parent.width - parent.columnSpacing * (root.columns - 1)) / root.columns
                        height: 40
                        radius: shell.radius
                        color: root.selectedIndex === index ? shell.selected : shell.surface
                        border.width: root.selectedIndex === index ? 3 : 1
                        border.color: root.selectedIndex === index ? shell.selectedFocus : shell.outline
                        Accessible.name: modelData
                        Text { font.family: shell.fontFamily;
                            anchors.fill: parent
                            anchors.margins: 3
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            text: root.keyLabel(modelData)
                            color: root.selectedIndex === index ? shell.selectedText : shell.text
                            font.pixelSize: 18 * shell.textScale
                            fontSizeMode: Text.Fit
                            minimumPixelSize: 12 * shell.textScale
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
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
                            onClicked: { root.mode = modelData; root.selectedIndex = 0; root.shift = false }
                        }
                    }
                }
            }

        }
        }
            Text { id: keyboardHint; font.family: shell.fontFamily;
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 18 }
                wrapMode:Text.Wrap
                text: root.inputMessage || ((root.keys[root.selectedIndex] || "") + " · " + root.hint
                    + (root.acceptTextKeys ? " · Keyboard text ON: type directly; Enter finishes after typing. Arrows select onscreen keys." : " · Keyboard text OFF: remote bindings control this screen."))
                color: shell.muted
                font.pixelSize: 15 * shell.textScale
            }
    }
}
