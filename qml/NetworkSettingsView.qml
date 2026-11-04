import QtQuick
import QtQuick.Controls

Item {
    id: root
    SurfacePalette { id: shell; appearance: root.appearance }
    Rectangle {anchors.fill:parent;z:-1;color:shell.surface}
    property var appearance: ({})
    property int selectedIndex: 0
    property bool passwordPrompt: false
    property bool hiddenNetworkPrompt: false
    property bool connectionPrompt: false
    property string selectedSsid: ""
    property bool selectedNetworkSecured: false
    property string selectedConnectionName: ""
    property string selectedConnectionUuid: ""
    property int selectedConnectionAction: 0
    property bool keyboardOpen: false
    property string keyboardTarget: "password"
    readonly property var actions: ["Refresh", networkSettings.wifiEnabled ? "Turn Wi-Fi off" : "Turn Wi-Fi on", "Join hidden Wi-Fi"]
    readonly property int firstDeviceIndex: actions.length
    readonly property int firstNetworkIndex: actions.length + networkSettings.devices.length
    readonly property int firstSavedIndex: actions.length + networkSettings.devices.length + networkSettings.wifiNetworks.length
    readonly property int totalRows: actions.length + networkSettings.devices.length + networkSettings.wifiNetworks.length + networkSettings.savedConnections.length
    readonly property var connectionActions: ["Connect", "Forget", "Cancel"]

    function selectedNetwork() {
        const index = selectedIndex - firstNetworkIndex
        if (index < 0 || index >= networkSettings.wifiNetworks.length)
            return null
        return networkSettings.wifiNetworks[index]
    }

    function selectedSavedConnection() {
        const index = selectedIndex - firstSavedIndex
        if (index < 0 || index >= networkSettings.savedConnections.length)
            return null
        return networkSettings.savedConnections[index]
    }

    function handleCommand(command) {
        if (keyboardOpen) {
            tvKeyboard.handleCommand(command)
            return
        }
        if (connectionPrompt) {
            if (command === inputManager.navigateLeft)
                selectedConnectionAction = Math.max(0, selectedConnectionAction - 1)
            else if (command === inputManager.navigateRight)
                selectedConnectionAction = Math.min(connectionActions.length - 1, selectedConnectionAction + 1)
            else if (command === inputManager.back) {
                connectionPrompt = false
                selectedConnectionAction = 0
            } else if (command === inputManager.confirm) {
                if (selectedConnectionAction === 0)
                    networkSettings.reconnect(selectedConnectionUuid)
                else if (selectedConnectionAction === 1)
                    networkSettings.forgetConnection(selectedConnectionUuid)
                connectionPrompt = false
                selectedConnectionAction = 0
            }
            return
        }
        if (passwordPrompt || hiddenNetworkPrompt) {
            if (command === inputManager.back) {
                passwordPrompt = false
                hiddenNetworkPrompt = false
                wifiSsid.text = ""
                wifiPassword.text = ""
            } else if (command === inputManager.confirm) {
                if (hiddenNetworkPrompt && wifiSsid.activeFocus) {
                    wifiPassword.forceActiveFocus()
                    return
                }
                networkSettings.connectToWifi(hiddenNetworkPrompt ? wifiSsid.text : selectedSsid, wifiPassword.text)
                passwordPrompt = false
                hiddenNetworkPrompt = false
                wifiSsid.text = ""
                wifiPassword.text = ""
            }
            return
        }
        if (command === inputManager.navigateLeft && selectedIndex > 0 && selectedIndex < actions.length)
            selectedIndex--
        else if (command === inputManager.navigateRight && selectedIndex >= 0 && selectedIndex < actions.length - 1)
            selectedIndex++
        else if (command === inputManager.navigateUp)
            selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown)
            selectedIndex = Math.min(Math.max(0, totalRows - 1), selectedIndex + 1)
        else if (command === inputManager.confirm) {
            if (selectedIndex === 0) networkSettings.refresh()
            else if (selectedIndex === 1) networkSettings.setWifiEnabled(!networkSettings.wifiEnabled)
            else if (selectedIndex === 2) {
                selectedSsid = ""
                hiddenNetworkPrompt = true
                passwordPrompt = true
                wifiSsid.forceActiveFocus()
            }
            else {
                const network = selectedNetwork()
                if (network) {
                    selectedSsid = network.ssid
                    selectedNetworkSecured = network.security.length > 0
                    if (selectedNetworkSecured) {
                        passwordPrompt = true
                        wifiPassword.forceActiveFocus()
                    } else {
                        networkSettings.connectToWifi(selectedSsid, "")
                    }
                }
            }
            const connection = selectedSavedConnection()
            if (connection) {
                selectedConnectionName = connection.name
                selectedConnectionUuid = connection.uuid
                selectedConnectionAction = 0
                connectionPrompt = true
            }
        }
    }

    function openKeyboard(target) {
        keyboardTarget = target
        tvKeyboard.passwordMode = target === "password"
        tvKeyboard.value = target === "ssid" ? wifiSsid.text : wifiPassword.text
        tvKeyboard.selectedIndex = 0
        keyboardOpen = true
        // Remote keys belong to the TV keyboard, not the underlying TextField.
        root.forceActiveFocus()
    }

    function closePasswordPrompts() {
        passwordPrompt = false
        hiddenNetworkPrompt = false
        keyboardOpen = false
        wifiSsid.text = ""
        wifiPassword.text = ""
    }

    Rectangle {
        visible: root.connectionPrompt
        anchors.fill: parent
        color: shell.scrim
        z: 11
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(660, parent.width - 80)
            height: 270
            radius: shell.radius
            color: shell.elevated
            border.width: 2
            border.color: root.selectedConnectionAction === 1 ? shell.elevatedError : shell.elevatedFocus
            Column {
                anchors.fill: parent
                anchors.margins: 28
                spacing: 18 * shell.gapScale
                Text { font.family: shell.fontFamily; text: "SAVED NETWORK"; color: shell.elevatedText; font.pixelSize: 28 * shell.textScale; font.weight: Font.DemiBold; font.letterSpacing: 3 }
                Text { font.family: shell.fontFamily; text: root.selectedConnectionName; color: shell.elevatedMuted; font.pixelSize: 19 * shell.textScale; elide: Text.ElideRight; width: parent.width }
                Text { font.family: shell.fontFamily; text: "Connect brings this saved profile online. Forget removes its saved credentials and settings."; color: shell.elevatedMuted; font.pixelSize: 16 * shell.textScale; wrapMode: Text.WordWrap; width: parent.width }
                Row {
                    spacing: 12 * shell.gapScale
                    Repeater {
                        model: root.connectionActions
                        delegate: Rectangle {
                            required property int index
                            required property string modelData
                            objectName: "networkConnectionAction-" + index
                            width: 170; height: 52; radius: shell.radius
                            color: root.selectedConnectionAction === index ? shell.selected : shell.surface
                            border.width: root.selectedConnectionAction === index ? 3 : 1
                            border.color: root.selectedConnectionAction === index ? shell.selectedFocus : shell.focus
                            Text { font.family: shell.fontFamily; anchors.centerIn: parent; text: modelData; color: root.selectedConnectionAction === index ? shell.selectedText : shell.text; font.pixelSize: 17 * shell.textScale; font.weight: Font.DemiBold }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        visible: root.passwordPrompt || root.hiddenNetworkPrompt
        anchors.fill: parent
        color: shell.scrim
        z: 10
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(620, parent.width - 80)
            height: root.hiddenNetworkPrompt ? 330 : 260
            radius: shell.radius
            color: shell.elevated
            border.width: 2
            border.color: shell.elevatedFocus
            Column {
                anchors.fill: parent
                anchors.margins: 28
                spacing: 16 * shell.gapScale
                Text { font.family: shell.fontFamily; text: "JOIN WI-FI"; color: shell.elevatedText; font.pixelSize: 28 * shell.textScale; font.weight: Font.DemiBold; font.letterSpacing: 3 }
                Text { font.family: shell.fontFamily; visible: !root.hiddenNetworkPrompt; text: root.selectedSsid; color: shell.elevatedMuted; font.pixelSize: 18 * shell.textScale; elide: Text.ElideRight; width: parent.width }
                TextField {
                    id: wifiSsid; objectName: "networkSsidField"
                    visible: root.hiddenNetworkPrompt
                    width: parent.width
                    height: 54
                    color: shell.inputText
                    font.pixelSize: 19 * shell.textScale
                    placeholderText: "Network name"
                    placeholderTextColor: shell.inputMuted
                    background: Rectangle { radius: shell.radius; color: shell.input; border.width: 2; border.color: shell.inputFocus }
                    onActiveFocusChanged: if (activeFocus && root.hiddenNetworkPrompt) root.openKeyboard("ssid")
                    onAccepted: wifiPassword.forceActiveFocus()
                }
                TextField {
                    id: wifiPassword; objectName: "networkPasswordField"
                    width: parent.width
                    height: 54
                    echoMode: TextInput.Password
                    color: shell.inputText
                    font.pixelSize: 19 * shell.textScale
                    placeholderText: "Wi-Fi password"
                    placeholderTextColor: shell.inputMuted
                    background: Rectangle { radius: shell.radius; color: shell.input; border.width: 2; border.color: shell.inputFocus }
                    onActiveFocusChanged: if (activeFocus && (root.passwordPrompt || root.hiddenNetworkPrompt)) root.openKeyboard("password")
                    onAccepted: {
                        networkSettings.connectToWifi(root.hiddenNetworkPrompt ? wifiSsid.text : root.selectedSsid, text)
                        root.closePasswordPrompts()
                    }
                }
                Text { font.family: shell.fontFamily; text: "Enter connect    Esc cancel    Text fields open TV keyboard"; color: shell.elevatedMuted; font.pixelSize: 16 * shell.textScale }
            }
        }
    }

    OnScreenKeyboard { appearance: root.appearance;
        id: tvKeyboard; objectName: "networkTvKeyboard"
        visible: root.keyboardOpen
        z: 20
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 30
        height: Math.min(400, parent.height - 60)
        onAccepted: text => {
            if (root.keyboardTarget === "ssid") {
                wifiSsid.text = text
                root.keyboardOpen = false
                wifiPassword.forceActiveFocus()
            } else {
                wifiPassword.text = text
                networkSettings.connectToWifi(root.hiddenNetworkPrompt ? wifiSsid.text : root.selectedSsid, wifiPassword.text)
                root.closePasswordPrompts()
            }
        }
        onCanceled: {
            root.closePasswordPrompts()
            root.forceActiveFocus()
        }
    }

    Flickable {
        id: networkScroller
        anchors.fill: parent
        contentHeight: contentColumn.height
        contentY: Math.max(0, Math.min(Math.max(0, contentHeight - height), root.selectedIndex * 76 - 120))
        clip: true

        Column {
            id: contentColumn
            width: parent.width
            spacing: 20 * shell.gapScale

            Text { font.family: shell.fontFamily; text: "NETWORK"; color: shell.text; font.pixelSize: 30 * shell.textScale; font.weight: Font.DemiBold; font.letterSpacing: 3 }
            Text { font.family: shell.fontFamily; text: networkSettings.summary + "  |  " + networkSettings.connectivity; color: shell.muted; font.pixelSize: 18 * shell.textScale }

        Row {
            spacing: 12 * shell.gapScale
            Repeater {
                model: root.actions
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    width: 220; height: 54; radius: shell.radius
                    color: root.selectedIndex === index ? shell.selected : shell.surface
                    border.width: root.selectedIndex === index ? 3 : 1
                    border.color: root.selectedIndex === index ? shell.selectedFocus : shell.outline
                    Text { font.family: shell.fontFamily; anchors.centerIn: parent; text: modelData; color: (root.selectedIndex === index) ? shell.selectedText : shell.text; font.pixelSize: 17 * shell.textScale; font.weight: Font.DemiBold }
                }
            }
        }

        Text { font.family: shell.fontFamily; text: "DEVICES"; color: shell.muted; font.pixelSize: 14 * shell.textScale; font.weight: Font.DemiBold; font.letterSpacing: 3 }
        Repeater {
            model: networkSettings.devices
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: parent.width; height: 58; radius: shell.radius
                color: root.selectedIndex === index + root.firstDeviceIndex ? shell.selected : shell.surface
                border.color: shell.outline
                Text { font.family: shell.fontFamily; anchors.left: parent.left; anchors.leftMargin: 18; anchors.verticalCenter: parent.verticalCenter; text: modelData.name + "  " + modelData.type; color: (root.selectedIndex === index + root.firstDeviceIndex) ? shell.selectedText : shell.text; font.pixelSize: 18 * shell.textScale; font.weight: Font.DemiBold }
                Text { font.family: shell.fontFamily; anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: parent.verticalCenter; text: modelData.state + (modelData.connection ? "  |  " + modelData.connection : ""); color: (root.selectedIndex === index + root.firstDeviceIndex) ? shell.selectedText : shell.muted; font.pixelSize: 16 * shell.textScale }
            }
        }

        Text { font.family: shell.fontFamily; visible: networkSettings.connectionDetails.length > 0; text: "CONNECTION DETAILS"; color: shell.muted; font.pixelSize: 14 * shell.textScale; font.weight: Font.DemiBold; font.letterSpacing: 3 }
        Repeater {
            model: networkSettings.connectionDetails
            delegate: Row {
                required property var modelData
                width: parent.width
                height: 26
                spacing: 12 * shell.gapScale
                Text { font.family: shell.fontFamily; text: modelData.label; width: 110; color: shell.unavailable; font.pixelSize: 15 * shell.textScale; font.weight: Font.DemiBold }
                Text { font.family: shell.fontFamily; text: modelData.value; width: parent.width - 122; color: shell.text; font.pixelSize: 15 * shell.textScale; elide: Text.ElideRight }
            }
        }

        Text { font.family: shell.fontFamily; text: "WI-FI NETWORKS"; color: shell.muted; font.pixelSize: 14 * shell.textScale; font.weight: Font.DemiBold; font.letterSpacing: 3 }
        Repeater {
            model: networkSettings.wifiNetworks
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: parent.width; height: 54; radius: shell.radius
                color: root.selectedIndex === index + root.firstNetworkIndex ? shell.selected : shell.surface
                border.color: shell.outline
                Text { font.family: shell.fontFamily; anchors.left: parent.left; anchors.leftMargin: 18; anchors.verticalCenter: parent.verticalCenter; text: (modelData.active ? "* " : "") + modelData.ssid; color: (root.selectedIndex === index + root.firstNetworkIndex) ? shell.selectedText : shell.text; font.pixelSize: 17 * shell.textScale; font.weight: Font.DemiBold }
                Text { font.family: shell.fontFamily; anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: parent.verticalCenter; text: modelData.signal + "%  " + (modelData.security ? modelData.security : "open"); color: (root.selectedIndex === index + root.firstNetworkIndex) ? shell.selectedText : shell.muted; font.pixelSize: 15 * shell.textScale }
            }
        }

        Text { font.family: shell.fontFamily; text: "SAVED NETWORKS"; color: shell.muted; font.pixelSize: 14 * shell.textScale; font.weight: Font.DemiBold; font.letterSpacing: 3 }
        Repeater {
            model: networkSettings.savedConnections
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: parent.width; height: 54; radius: shell.radius
                color: root.selectedIndex === index + root.firstSavedIndex ? shell.selected : shell.surface
                border.width: root.selectedIndex === index + root.firstSavedIndex ? 2 : 1
                border.color: root.selectedIndex === index + root.firstSavedIndex ? shell.selectedFocus : shell.outline
                Text { font.family: shell.fontFamily; anchors.left: parent.left; anchors.leftMargin: 18; anchors.verticalCenter: parent.verticalCenter; text: modelData.name; color: (root.selectedIndex === index + root.firstSavedIndex) ? shell.selectedText : shell.text; font.pixelSize: 17 * shell.textScale; font.weight: Font.DemiBold }
                Text { font.family: shell.fontFamily; anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: parent.verticalCenter; text: modelData.type + (modelData.device && modelData.device !== "--" ? "  |  " + modelData.device : ""); color: (root.selectedIndex === index + root.firstSavedIndex) ? shell.selectedText : shell.muted; font.pixelSize: 15 * shell.textScale }
            }
        }

            Text { font.family: shell.fontFamily; visible: networkSettings.errorString.length > 0; text: networkSettings.errorString; color: shell.error; font.pixelSize: 16 * shell.textScale; wrapMode: Text.WordWrap; width: parent.width }
        }
    }
}
