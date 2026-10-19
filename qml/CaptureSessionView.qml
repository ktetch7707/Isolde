import QtQuick
import QtMultimedia

Item {
    id: root
    property bool open: false
    property string originId: ""
    property bool chooseInput: false
    property bool pendingSwitchConfirmation: true
    property bool selectionPending: false
    signal returned(string originId)

    function openSession(input, origin) {
        originId = origin || ""
        chooseInput = input < 1 || input > 8
        pendingSwitchConfirmation = true
        selectionPending = false
        open = true
        captureController.setVideoOutput(videoOutput)
        if (!chooseInput) {
            captureController.selectInput(input)
            captureController.switchInput(input)
        }
    }
    function closeSession() {
        const origin = originId
        open = false
        originId = ""
        chooseInput = false
        captureController.stopPreview()
        returned(origin)
    }
    function selectedInputName() {
        const inputs = captureController.inputs
        const index = captureController.selectedInput - 1
        return index >= 0 && index < inputs.length ? inputs[index].name : "Input"
    }
    function handleCommand(command) {
        if (command === inputManager.back || command === inputManager.home) { closeSession(); return }
        if (chooseInput) {
            if (command === inputManager.navigateUp || command === inputManager.navigateLeft)
                captureController.selectInput((captureController.selectedInput + 6) % 8 + 1)
            else if (command === inputManager.navigateDown || command === inputManager.navigateRight)
                captureController.selectInput(captureController.selectedInput % 8 + 1)
            else if (command === inputManager.confirm) {
                if (captureController.switchInput(captureController.selectedInput)) {
                    chooseInput = false
                    pendingSwitchConfirmation = true
                }
            }
            return
        }
        if (command === inputManager.navigateLeft) {
            captureController.selectInput((captureController.selectedInput + 6) % 8 + 1)
            selectionPending = true
        } else if (command === inputManager.navigateRight) {
            captureController.selectInput(captureController.selectedInput % 8 + 1)
            selectionPending = true
        } else if (command === inputManager.confirm) {
            if (selectionPending) {
                selectionPending = false
                pendingSwitchConfirmation = true
                captureController.switchInput(captureController.selectedInput)
            } else if (pendingSwitchConfirmation && captureController.state === "receiving-frames") {
                pendingSwitchConfirmation = false
            }
        }
    }

    readonly property string inputHeading: selectionPending ? "READY TO SWITCH · " + root.selectedInputName()
        : pendingSwitchConfirmation ? "VERIFYING INPUT · " + root.selectedInputName()
        : "HDMI INPUT · " + root.selectedInputName()

    visible: open
    z: 1000
    Rectangle { anchors.fill: parent; color: "#05080E" }
    VideoOutput { id: videoOutput; anchors.fill: parent; fillMode: VideoOutput.PreserveAspectFit }
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: 122; color: "#D2080C14"
        Column {
            anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 30; spacing: 8
            Text { text: root.inputHeading; color: "#FFFFFF"; font.pixelSize: 27; font.weight: Font.DemiBold }
            Text { width: parent.width; text: captureController.status + " · " + captureController.switchStatus; color: "#D5DCE8"; font.pixelSize: 17; wrapMode: Text.WordWrap; elide: Text.ElideRight }
        }
    }
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 66; color: "#D2080C14"
        Text { anchors.centerIn: parent; text: root.selectionPending ? "OK Switch to selected input     Back / Home Return"
            : root.pendingSwitchConfirmation ? "Check the picture, then OK confirm     ‹ / › choose another input     Back / Home Return"
            : "‹ / › Choose input     OK Switch     Back / Home Return"; color: "#FFFFFF"; font.pixelSize: 18 }
    }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.NoButton }
    Rectangle {
        visible: root.chooseInput
        z: 10
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 760)
        height: Math.min(parent.height - 80, 620)
        radius: 12
        color: "#F20A101A"
        border.width: 2; border.color: "#91A0BC"
        Column {
            anchors.fill: parent; anchors.margins: 24; spacing: 14
            Text { text: "CHOOSE HDMI INPUT"; color: "#FFFFFF"; font.pixelSize: 29; font.weight: Font.DemiBold }
            Text { text: captureController.switchStatus; width: parent.width; wrapMode: Text.WordWrap; color: "#C8D3E5"; font.pixelSize: 16 }
            ListView {
                id: inputChoices
                width: parent.width; height: parent.height - 118; clip: true; spacing: 6
                model: captureController.inputs
                currentIndex: captureController.selectedInput - 1
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: inputChoices.width; height: 52; radius: 7
                    color: index === captureController.selectedInput - 1 ? "#2B416B" : "#111A2A"
                    border.width: index === captureController.selectedInput - 1 ? 2 : 1
                    border.color: index === captureController.selectedInput - 1 ? "#FFFFFF" : "#354462"
                    Text { anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: modelData.name; color: "#FFFFFF"; font.pixelSize: 19 }
                    Text { anchors.right: parent.right; anchors.rightMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: "HDMI " + modelData.number; color: "#C8D3E5"; font.pixelSize: 16 }
                    MouseArea { anchors.fill: parent; onClicked: captureController.selectInput(index + 1) }
                }
            }
            Text { text: "↑ ↓ Choose · OK Switch · Back Return to Home"; color: "#C8D3E5"; font.pixelSize: 15 }
        }
    }
}