import QtQuick
import QtQuick.Controls

Item {
    id: root
    property var appearance: ({})
    readonly property real textScale: appearance.typographyScale || 1
    readonly property color textColor: appearance.textColor || palette.text
    readonly property color surfaceColor: appearance.surfaceColor || palette.window
    readonly property color focusColor: appearance.focusColor || palette.highlight
    property int selectedIndex: 0
    property bool installPrompt: false
    property string pendingProvider: ""
    property bool approveInstall: false
    property string mode: "main"
    property string selectedFile: ""
    property string provider: "surfshark"
    property string country: "CA"
    property string selectedApp: ""
    property bool advanced: false
    readonly property bool editing: installPrompt || mode !== "main"
    readonly property var rows: {
        let result = []
        if(mode === "apps") {
            for(const app of appRegistry.apps) {
                if(app.type === "settings" || app.type === "store" || app.type === "customize")continue
                let location = appVpn.routes[app.id] ? "Location missing — choose again" : "Keep current connection"
                for(const connection of appVpn.connections)if(connection.uuid === appVpn.routes[app.id])location = connection.name
                result.push({label:app.name + " — " + location, action:"chooseApp", target:app.id})
            }
            return result
        }
        if(mode === "locations") {
            result.push({label:"Keep current connection (no automatic switch)", action:"saveRoute", target:""})
            for(const location of appVpn.connections)result.push({label:location.name, action:"saveRoute", target:location.uuid})
            return result
        }
        if(mode === "provider") {
            for(const item of vpnSettings.providers)result.push({label:item.name, action:"chooseProvider", target:item.id})
            return result
        }
        if(mode === "country") {
            for(const item of vpnSettings.countries)result.push({label:item.name, action:"chooseCountry", target:item.code})
            return result
        }
        if(mode === "import")return [{label:"Import for " + provider + " / " + country + ": " + selectedFile, action:"saveImport", target:selectedFile}, {label:"Cancel", action:"cancel", target:""}]
        if(mode === "capture")return [{label:"Save and use the connected country",action:"saveConnected",target:pendingProvider},{label:"Cancel",action:"cancel",target:""}]
        result = [{label: "Install default VPN providers (Surfshark + Proton VPN)", action:"defaults", target:""},
                  {label:"Choose a VPN location for each app", action:"apps", target:""},
                  {label: "Refresh VPN status", action: "status", target: ""}]
        for (const provider of vpnSettings.providers) {
            result.push({label: (provider.installed ? "Reinstall " : "Install ") + provider.name, action: "install", target: provider.id})
            result.push({label: "Open " + provider.name + " — sign in / choose country / settings", action: "open", target: provider.id})
            result.push({label:provider.name + " — Save connected location",action:"capture",target:provider.id})
            if(advanced)result.push({label: provider.name + " — manual configuration page", action: "account", target: provider.id})
        }
        result.push({label:advanced ? "Hide advanced manual setup" : "Advanced manual setup (optional)",action:"advanced",target:""})
        if(advanced)for (const download of vpnSettings.downloads)
            result.push({label: "Add location from " + download.name, action: "import", target: download.name})
        for (const connection of vpnSettings.connections)
            result.push({label: (connection.active ? "Disconnect " : "Connect ") + connection.name, action: connection.active ? "disconnect" : "connect", target: connection.uuid})
        return result
    }
    onRowsChanged: selectedIndex = Math.min(selectedIndex, rows.length - 1)
    onVisibleChanged: if (visible) {vpnSettings.perform("status");appVpn.refresh()}
    function activate(index) {
        if (vpnSettings.busy) return
        selectedIndex = index
        const row = rows[index]
        if(row.action === "advanced"){advanced=!advanced;return}
        if(row.action === "capture"){pendingProvider=row.target;mode="capture";selectedIndex=0;return}
        if(row.action === "saveConnected"){mode="main";selectedIndex=0;vpnSettings.perform("capture",row.target);return}
        if(row.action === "apps") {mode="apps";selectedIndex=0;return}
        if(row.action === "chooseApp") {selectedApp=row.target;mode="locations";selectedIndex=0;return}
        if(row.action === "saveRoute") {if(appVpn.setRoute(selectedApp,row.target)){mode="apps";selectedIndex=0}return}
        if(row.action === "import") {selectedFile=row.target;mode="provider";selectedIndex=0;return}
        if(row.action === "chooseProvider") {provider=row.target;mode="country";selectedIndex=0;return}
        if(row.action === "chooseCountry") {country=row.target;mode="import";selectedIndex=0;return}
        if(row.action === "saveImport") {mode="main";selectedIndex=0;vpnSettings.perform("import",selectedFile,provider,country);return}
        if(row.action === "cancel") {mode="main";selectedIndex=0;return}
        if (row.action === "install" || row.action === "defaults") {
            pendingProvider = row.target
            approveInstall = false
            installPrompt = true
        } else vpnSettings.perform(row.action, row.target)
    }
    function handleCommand(command) {
        if (installPrompt) {
            if (command === inputManager.back) installPrompt = false
            else if (command === inputManager.navigateLeft || command === inputManager.navigateRight) approveInstall = !approveInstall
            else if (command === inputManager.confirm) {
                installPrompt = false
                if (approveInstall) vpnSettings.perform(pendingProvider ? "install" : "defaults", pendingProvider)
            }
            return
        }
        if(command === inputManager.back && mode !== "main"){mode="main";selectedIndex=0;return}
        if (command === inputManager.navigateUp) selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown) selectedIndex = Math.min(rows.length - 1, selectedIndex + 1)
        else if (command === inputManager.confirm) activate(selectedIndex)
    }
    SystemPalette { id: palette; colorGroup: SystemPalette.Active }
    Rectangle { anchors.fill: parent; color: root.surfaceColor }
    ListView {
        id: list
        anchors.fill: parent
        clip: true
        model: root.rows
        currentIndex: root.selectedIndex
        spacing: 8 * root.textScale
        header: Column {
        width: list.width
        spacing: 16 * root.textScale
        Label { text: root.mode === "apps" ? "VPN for each app" : root.mode === "locations" ? "Choose app location" : root.mode === "country" ? "Where is this server?" : "VPN setup"; font.pixelSize: 30 * root.textScale; color: root.textColor }
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            color: root.textColor
            font.pixelSize: 18 * root.textScale
            text: root.mode === "capture" ? "Save the currently connected country for automatic app switching? Isolde privately copies this connection’s WireGuard key inside NetworkManager and switches to the saved location. The provider’s original configuration is kept. Isolde checks the copy before handing over. If the final check fails, review the connection status before retrying. Keys are never displayed or uploaded. Back cancels." : root.mode !== "main" ? "Up / Down choose · Confirm select · Back cancels without saving" : "1. The installer downloads both providers.\n2. Open your provider app, sign in and connect to the country you want.\n3. Return here and Save connected location. Isolde handles the keys and detects the country.\n4. Choose a VPN location for each app.\n\nNo browser or configuration files are needed. Connections affect this whole device. The country check contacts IPinfo through the VPN."
        }
        Label { width: parent.width; wrapMode: Text.WordWrap; text: vpnSettings.message; color: root.textColor; font.pixelSize: 18 * root.textScale }
        Label {width:parent.width;wrapMode:Text.WordWrap;text:appVpn.errorString;color:root.textColor;font.pixelSize:18*root.textScale}
        }
            delegate: Button {
                required property int index
                required property var modelData
                width: list.width
                height: Math.max(64 * root.textScale, contentItem.implicitHeight + 24)
                text: modelData.label
                font.pixelSize: 18 * root.textScale
                contentItem: Text {
                    textFormat: Text.PlainText
                    text: parent.text
                    font: parent.font
                    wrapMode: Text.WordWrap
                    color: root.textColor
                    verticalAlignment: Text.AlignVCenter
                }
                enabled: !vpnSettings.busy
                highlighted: index === root.selectedIndex
                onClicked: root.activate(index)
                background: Rectangle {
                    color: root.surfaceColor
                    border.width: index === root.selectedIndex ? 3 : 1
                    border.color: index === root.selectedIndex ? root.focusColor : root.textColor
                    radius: 8
                }
            }
    }
    Rectangle {
        anchors.fill: parent
        visible: root.installPrompt
        color: root.surfaceColor
        border.color: root.focusColor
        Column {
            anchors.centerIn: parent
            width: parent.width - 48
            spacing: 24
            Label { width: parent.width; wrapMode: Text.WordWrap; color: root.textColor; font.pixelSize: 22 * root.textScale; text: "Install VPN support on this device? This downloads the official clients, adds their repositories and installs Isolde's administrator-owned location-saving tool. Your system account can then use that tool to save the active VPN and switch only your profile-owned saved locations without another password prompt. Other administrator operations stay protected. Approve installer prompts in the guest. Your account password stays in the provider app." }
            Row {
                spacing: 24
                Button { text: "Cancel"; highlighted: !root.approveInstall; onClicked: root.installPrompt = false }
                Button { text: "Install"; highlighted: root.approveInstall; onClicked: { root.installPrompt = false; vpnSettings.perform(root.pendingProvider ? "install" : "defaults", root.pendingProvider) } }
            }
        }
    }
}
