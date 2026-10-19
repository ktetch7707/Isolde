import QtQuick
import QtQuick.Controls

Item {
    id: root
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
        color: "#CC050812"
        z: 11
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(660, parent.width - 80)
            height: 270
            radius: 10
            color: "#151E32"
            border.width: 2
            border.color: root.selectedConnectionAction === 1 ? "#E85C68" : "#4D7ED8"
            Column {
                anchors.fill: parent
                anchors.margins: 28
                spacing: 18
                Text { text: "SAVED NETWORK"; color: "#F8FAFC"; font.pixelSize: 28; font.weight: Font.DemiBold; font.letterSpacing: 3 }
                Text { text: root.selectedConnectionName; color: "#9EABC1"; font.pixelSize: 19; elide: Text.ElideRight; width: parent.width }
                Text { text: "Connect brings this saved profile online. Forget removes its saved credentials and settings."; color: "#91A0BC"; font.pixelSize: 16; wrapMode: Text.WordWrap; width: parent.width }
                Row {
                    spacing: 12
                    Repeater {
                        model: root.connectionActions
                        delegate: Rectangle {
                            required property int index
                            required property string modelData
                            width: 170; height: 52; radius: 8
                            color: root.selectedConnectionAction === index ? (index === 1 ? "#6B2B35" : "#2B416B") : "#17243A"
                            border.width: root.selectedConnectionAction === index ? 3 : 1
                            border.color: root.selectedConnectionAction === index ? "#F8FAFC" : "#2B3853"
                            Text { anchors.centerIn: parent; text: modelData; color: "#F8FAFC"; font.pixelSize: 17; font.weight: Font.DemiBold }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        visible: root.passwordPrompt || root.hiddenNetworkPrompt
        anchors.fill: parent
        color: "#CC050812"
        z: 10
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(620, parent.width - 80)
            height: root.hiddenNetworkPrompt ? 330 : 260
            radius: 10
            color: "#151E32"
            border.width: 2
            border.color: "#4D7ED8"
            Column {
                anchors.fill: parent
                anchors.margins: 28
                spacing: 16
                Text { text: "JOIN WI-FI"; color: "#F8FAFC"; font.pixelSize: 28; font.weight: Font.DemiBold; font.letterSpacing: 3 }
                Text { visible: !root.hiddenNetworkPrompt; text: root.selectedSsid; color: "#9EABC1"; font.pixelSize: 18; elide: Text.ElideRight; width: parent.width }
                TextField {
                    id: wifiSsid
                    visible: root.hiddenNetworkPrompt
                    width: parent.width
                    height: 54
                    color: "#F8FAFC"
                    font.pixelSize: 19
                    placeholderText: "Network name"
                    placeholderTextColor: "#71809F"
                    background: Rectangle { radius: 8; color: "#0E1628"; border.width: 2; border.color: wifiSsid.activeFocus ? "#F8FAFC" : "#354462" }
                    onActiveFocusChanged: if (activeFocus && root.hiddenNetworkPrompt) root.openKeyboard("ssid")
                    onAccepted: wifiPassword.forceActiveFocus()
                }
                TextField {
                    id: wifiPassword
                    width: parent.width
                    height: 54
                    echoMode: TextInput.Password
                    color: "#F8FAFC"
                    font.pixelSize: 19
                    placeholderText: "Wi-Fi password"
                    placeholderTextColor: "#71809F"
                    background: Rectangle { radius: 8; color: "#0E1628"; border.width: 2; border.color: wifiPassword.activeFocus ? "#F8FAFC" : "#354462" }
                    onActiveFocusChanged: if (activeFocus && (root.passwordPrompt || root.hiddenNetworkPrompt)) root.openKeyboard("password")
                    onAccepted: {
                        networkSettings.connectToWifi(root.hiddenNetworkPrompt ? wifiSsid.text : root.selectedSsid, text)
                        root.closePasswordPrompts()
                    }
                }
                Text { text: "Enter connect    Esc cancel    Text fields open TV keyboard"; color: "#91A0BC"; font.pixelSize: 16 }
            }
        }
    }

    OnScreenKeyboard {
        id: tvKeyboard
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
            root.keyboardOpen = false
            if (root.keyboardTarget === "ssid")
                wifiSsid.forceActiveFocus()
            else
                wifiPassword.forceActiveFocus()
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
            spacing: 20

            Text { text: "NETWORK"; color: "#F8FAFC"; font.pixelSize: 30; font.weight: Font.DemiBold; font.letterSpacing: 3 }
            Text { text: networkSettings.summary + "  |  " + networkSettings.connectivity; color: "#9EABC1"; font.pixelSize: 18 }

        Row {
            spacing: 12
            Repeater {
                model: root.actions
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    width: 220; height: 54; radius: 8
                    color: root.selectedIndex === index ? "#2B416B" : "#17243A"
                    border.width: root.selectedIndex === index ? 3 : 1
                    border.color: root.selectedIndex === index ? "#F8FAFC" : "#2B3853"
                    Text { anchors.centerIn: parent; text: modelData; color: "#F8FAFC"; font.pixelSize: 17; font.weight: Font.DemiBold }
                }
            }
        }

        Text { text: "DEVICES"; color: "#91A0BC"; font.pixelSize: 14; font.weight: Font.DemiBold; font.letterSpacing: 3 }
        Repeater {
            model: networkSettings.devices
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: parent.width; height: 58; radius: 8
                color: root.selectedIndex === index + root.firstDeviceIndex ? "#2B416B" : "#111A2A"
                border.color: "#2B3853"
                Text { anchors.left: parent.left; anchors.leftMargin: 18; anchors.verticalCenter: parent.verticalCenter; text: modelData.name + "  " + modelData.type; color: "#F8FAFC"; font.pixelSize: 18; font.weight: Font.DemiBold }
                Text { anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: parent.verticalCenter; text: modelData.state + (modelData.connection ? "  |  " + modelData.connection : ""); color: "#9EABC1"; font.pixelSize: 16 }
            }
        }

        Text { visible: networkSettings.connectionDetails.length > 0; text: "CONNECTION DETAILS"; color: "#91A0BC"; font.pixelSize: 14; font.weight: Font.DemiBold; font.letterSpacing: 3 }
        Repeater {
            model: networkSettings.connectionDetails
            delegate: Row {
                required property var modelData
                width: parent.width
                height: 26
                spacing: 12
                Text { text: modelData.label; width: 110; color: "#71809F"; font.pixelSize: 15; font.weight: Font.DemiBold }
                Text { text: modelData.value; width: parent.width - 122; color: "#C8D3E5"; font.pixelSize: 15; elide: Text.ElideRight }
            }
        }

        Text { text: "WI-FI NETWORKS"; color: "#91A0BC"; font.pixelSize: 14; font.weight: Font.DemiBold; font.letterSpacing: 3 }
        Repeater {
            model: networkSettings.wifiNetworks
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: parent.width; height: 54; radius: 8
                color: root.selectedIndex === index + root.firstNetworkIndex ? "#2B416B" : "#111A2A"
                border.color: "#2B3853"
                Text { anchors.left: parent.left; anchors.leftMargin: 18; anchors.verticalCenter: parent.verticalCenter; text: (modelData.active ? "* " : "") + modelData.ssid; color: "#F8FAFC"; font.pixelSize: 17; font.weight: Font.DemiBold }
                Text { anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: parent.verticalCenter; text: modelData.signal + "%  " + (modelData.security ? modelData.security : "open"); color: "#9EABC1"; font.pixelSize: 15 }
            }
        }

        Text { text: "SAVED NETWORKS"; color: "#91A0BC"; font.pixelSize: 14; font.weight: Font.DemiBold; font.letterSpacing: 3 }
        Repeater {
            model: networkSettings.savedConnections
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: parent.width; height: 54; radius: 8
                color: root.selectedIndex === index + root.firstSavedIndex ? "#2B416B" : "#111A2A"
                border.width: root.selectedIndex === index + root.firstSavedIndex ? 2 : 1
                border.color: root.selectedIndex === index + root.firstSavedIndex ? "#F8FAFC" : "#2B3853"
                Text { anchors.left: parent.left; anchors.leftMargin: 18; anchors.verticalCenter: parent.verticalCenter; text: modelData.name; color: "#F8FAFC"; font.pixelSize: 17; font.weight: Font.DemiBold }
                Text { anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: parent.verticalCenter; text: modelData.type + (modelData.device && modelData.device !== "--" ? "  |  " + modelData.device : ""); color: "#9EABC1"; font.pixelSize: 15 }
            }
        }

            Text { visible: networkSettings.errorString.length > 0; text: networkSettings.errorString; color: "#E85C68"; font.pixelSize: 16; wrapMode: Text.WordWrap; width: parent.width }
        }
    }
}
