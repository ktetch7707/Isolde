import QtQuick

Rectangle {
    id: root
    color: "#080C18"
    property bool adjusting: false
    property int selectedButton: 2
    readonly property var titles: ["Welcome to Isolde", "Connect to your network", "Fit your TV", "Set your sound", "Set up your remote", "Make it yours", "Set up your first library", "Ready for your living room"]
    readonly property var descriptions: [
        "Your dedicated media center. Set up the essentials with your remote or keyboard. Your progress is saved after each step.",
        "Connect for streaming and the App Store. You can continue offline; local media remains available.",
        "Make sure every edge is visible and text is comfortable to read from your couch.",
        "Set your TV to a comfortable reference volume, then let Isolde control everyday volume.",
        "Plug in your USB receiver and test the buttons. TV power through HDMI-CEC depends on your hardware.",
        "Rename Household or add people. Profiles keep personal layouts and separate supported app sign-ins.",
        "Point Kodi at your existing media folders so movies and shows appear in Isolde. Files stay where they are; nothing is moved or renamed.",
        "Select Start Isolde to finish. You can change these choices later in Settings. No streaming account is required to finish setup."
    ]
    function page() { return setupState.step === 1 ? network : setupState.step === 2 ? display : setupState.step === 3 ? audio : setupState.step === 4 ? remote : setupState.step === 5 ? profiles : librarySetup }
    function activate(index) {
        if (index === 0) setupState.back()
        else if (index === 1 && setupState.step > 0 && setupState.step < 6) adjusting = true
        else if (index === 2) setupState.advance()
    }
    function handleCommand(command) {
        if (adjusting) {
            const nested = setupState.step === 1 && (network.passwordPrompt || network.hiddenNetworkPrompt || network.connectionPrompt)
                         || setupState.step === 5 && profiles.editing
            if (command === inputManager.back && !nested) { adjusting = false; return }
            page().handleCommand(command)
            return
        }
        if (command === inputManager.navigateLeft) selectedButton = Math.max(0, selectedButton - 1)
        else if (command === inputManager.navigateRight) selectedButton = Math.min(2, selectedButton + 1)
        else if (command === inputManager.confirm) activate(selectedButton)
        else if (command === inputManager.back) setupState.back()
        if ((setupState.step === 0 || setupState.step === 6) && selectedButton === 1)
            selectedButton = command === inputManager.navigateLeft ? 0 : 2
    }
    // Capture pointer events so an invisible Home tile cannot launch behind setup.
    MouseArea { anchors.fill: parent }
    Column {
        id: heading
        visible: !root.adjusting
        anchors { top: parent.top; left: parent.left; right: parent.right; margins: 40 }
        spacing: 10
        Text { text: "SETUP  ·  " + (setupState.step + 1) + " / 7"; color: "#D5B083"; font.pixelSize: 18 }
        Text { text: root.titles[setupState.step]; color: "#F8FAFC"; font.pixelSize: 34; font.weight: Font.DemiBold }
        Text { width: parent.width; text: root.descriptions[setupState.step]; color: "#B9C5D9"; font.pixelSize: 18; wrapMode: Text.WordWrap }
    }
    Item {
        anchors { top: root.adjusting ? parent.top : heading.bottom; bottom: footer.top; left: parent.left; right: parent.right; margins: 32 }
        clip: true
        visible: setupState.step > 0 && setupState.step < 6
        enabled: root.adjusting
        opacity: root.adjusting ? 1 : 0.55
        NetworkSettingsView { id: network; anchors.fill: parent; visible: setupState.step === 1 }
        DisplaySettingsView { id: display; anchors.fill: parent; visible: setupState.step === 2 }
        AudioSettingsView { id: audio; anchors.fill: parent; visible: setupState.step === 3 }
        RemoteSettingsView { id: remote; anchors.fill: parent; visible: setupState.step === 4 }
        ProfileSettingsView { id: profiles; anchors.fill: parent; visible: setupState.step === 5 }
        Item { id: librarySetup; anchors.fill: parent; visible: setupState.step === 6
            Rectangle { anchors.fill: parent; color: "#080C18" }
            Column { anchors.centerIn: parent; spacing: 20; width: parent.width - 80
                Text { text: "SET UP YOUR FIRST LIBRARY"; color: "#F8FAFC"; font.pixelSize: 30; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter }
                Text { width: parent.width; wrapMode: Text.WordWrap; text: "Point Kodi at your existing media folders so movies and shows appear in Isolde. Your files stay where they are — nothing is moved, renamed, or cleaned up."; color: "#B9C5D9"; font.pixelSize: 18; horizontalAlignment: Text.AlignHCenter }
                Text { width: parent.width; wrapMode: Text.WordWrap; text: "1. Open Kodi → Videos → Files → Add videos\n2. Select your media folder at its original path\n3. Choose Movies or TV shows and enable recursive scanning\n4. Return to Isolde and scan for new movies"; color: "#91A0BC"; font.pixelSize: 17; horizontalAlignment: Text.AlignHCenter }
                Text { width: parent.width; wrapMode: Text.WordWrap; text: kodiService.status; color: "#C994A5"; font.pixelSize: 18; visible: kodiService.status.length > 0 }
            }
        }
    }
    Column {
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 40 }
        spacing: 10
        Text { text: setupState.error; color: "#FFB4A9"; width: parent.width; wrapMode: Text.WordWrap; font.pixelSize: 17; visible: text.length > 0 }
        Text { text: root.adjusting ? "Adjust with your remote. Esc / Back returns to setup navigation." : "← → choose    Enter select    Esc / Back previous step"; color: "#91A0BC"; font.pixelSize: 16 }
        Row {
            spacing: 16
            Repeater {
                model: ["Back", "Adjust settings", setupState.step === 7 ? "Start Isolde" : "Continue"]
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    width: 220; height: 54; radius: 10
                    visible: index !== 1 || (setupState.step > 0 && setupState.step < 6)
                    color: !root.adjusting && root.selectedButton === index ? "#D5B083" : "#1C2940"
                    Text { anchors.centerIn: parent; text: modelData; color: !root.adjusting && root.selectedButton === index ? "#080C18" : "#F8FAFC"; font.pixelSize: 19 }
                    MouseArea { anchors.fill: parent; onClicked: { root.adjusting = false; root.selectedButton = index; root.activate(index) } }
                }
            }
        }
    }
}
