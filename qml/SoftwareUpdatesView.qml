import QtQuick

Item {
    id: root
    property var appearance: ({})
    property int selectedIndex: 0
    SurfacePalette { id: shell; appearance: root.appearance }
    readonly property var updater: typeof softwareUpdates !== "undefined" ? softwareUpdates : null
    property string confirmation: ""
    property int confirmIndex: 0
    property bool autoDisableWeb: false
    readonly property color ink: shell.text
    property var savedVersions: []

    function refreshSavedVersions() {
        root.savedVersions = updater ? updater.savedVersions() : [];
        root.computeActions();
    }
    function computeActions() {
        const list = [];
        if (updater && !updater.checking)
            list.push({ id: "check", name: qsTr("Check for updates"), enabled: !updater.installing });
        if (updater && updater.available) {
            list.push({ id: "install", name: qsTr("Install update"), enabled: !updater.checking && !updater.installing });
            list.push({ id: "dismiss", name: qsTr("Dismiss notification"), enabled: !updater.installing });
        }
        if (updater && updater.installing)
            list.push({ name: qsTr("Installing…"), enabled: false });
        const saved = root.savedVersions || [];
        const currentSaved = saved.some(function (s) { return s.current; });
        if (saved.length < 3 && !currentSaved)
            list.push({ id: "save", name: qsTr("Save current version"), enabled: !updater.installing });
        for (const entry of saved) {
            list.push({ id: "restore-" + entry.version, name: qsTr("Restore to %1").arg(entry.version), enabled: !updater.installing && !entry.current });
            list.push({ id: "drop-" + entry.version, name: qsTr("Drop %1").arg(entry.version), enabled: !updater.installing });
        }
        root.actions = list;
    }

    property var actions: []

    Component.onCompleted: root.refreshSavedVersions()

    function enableWebTitle() { return qsTr("Enable web to update?"); }
    // Explain the temporary web opt-in as one translatable message.
    function enableWebMessage() {
        return qsTr("Software updates are only available when web search is enabled. Enable web now to check for updates. Web will be turned off again afterward.");
    }

    // Dispatch by stable action identity even when availability changes the row order.
    function activate(index) {
        const action = actions[index]
        if (!updater || !action || !action.enabled) return
        if (action.id === "check") updater.check()
        else if (action.id === "install") updater.install()
        else if (action.id === "dismiss") updater.dismiss()
        else if (action.id === "save") updater.saveVersion(updater.currentInstalledVersion())
        else if (action.id.indexOf("restore-") === 0) updater.restoreVersion(action.id.slice(8))
        else if (action.id.indexOf("drop-") === 0) updater.dropVersion(action.id.slice(5))
    }
    function handleCommand(command) {
        if (confirmation === "enableWeb") {
            if (command === inputManager.back) { confirmation = ""; return }
            else if (command === inputManager.navigateLeft) confirmIndex = Math.max(0, confirmIndex - 1)
            else if (command === inputManager.navigateRight) confirmIndex = Math.min(1, confirmIndex + 1)
            else if (command === inputManager.confirm) {
                if (confirmIndex === 0) { confirmation = ""; return }
                confirmation = "";
                autoDisableWeb = true;
                if (searchPreferences) searchPreferences.setWebEnabled(true);
                updater.check();
                return;
            }
        }
        if (command === inputManager.navigateUp) selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown) selectedIndex = Math.min(Math.max(0, actions.length - 1), selectedIndex + 1)
        else if (command === inputManager.confirm) activate(selectedIndex)
    }
    function revealSelection() {
        const row = actionRows.itemAt(selectedIndex)
        if (!row) return
        if (row.y < scroll.contentY) scroll.contentY = row.y
        else if (row.y + row.height > scroll.contentY + scroll.height)
            scroll.contentY = Math.max(0, row.y + row.height - scroll.height)
    }
    onSelectedIndexChanged: Qt.callLater(revealSelection)
    onActionsChanged: selectedIndex = Math.max(0, Math.min(selectedIndex, actions.length - 1))

    Connections {
        target: updater
        function onWebEnableRequested() {
            if (searchPreferences && searchPreferences.webEnabled) return;
            confirmation = "enableWeb";
            confirmIndex = 0;
        }
        function onChanged() {
            root.refreshSavedVersions();
        }
        function onCheckingChanged() {
            if (autoDisableWeb && !updater.checking) {
                autoDisableWeb = false;
                if (searchPreferences) searchPreferences.setWebEnabled(false);
            }
        }
        function onInstallFinished(success) {
            if (success) root.selectedIndex = 0
        }
    }

    Flickable {
        id: scroll
        anchors.fill: parent
        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Column {
            id: content
            width: parent.width
            spacing: 18 * shell.gapScale
            Text { width: parent.width; text: qsTr("Software Updates"); color: shell.text; font.family: shell.fontFamily; font.pixelSize: 30 * shell.textScale; wrapMode: Text.Wrap }
            Text { width: parent.width; text: qsTr("Installed version: ") + isoldeVersion; color: shell.text; font.family: shell.fontFamily; font.pixelSize: 20 * shell.textScale; wrapMode: Text.Wrap }
            Text { width: parent.width; visible: root.updater && root.updater.latestVersion !== ""; text: qsTr("Candidate version: ") + (root.updater ? root.updater.latestVersion : ""); color: shell.text; font.family: shell.fontFamily; font.pixelSize: 20 * shell.textScale; wrapMode: Text.Wrap }
            Text { width: parent.width; text: root.updater ? root.updater.status : qsTr("Update service unavailable."); color: shell.muted; font.family: shell.fontFamily; font.pixelSize: 18 * shell.textScale; wrapMode: Text.Wrap }
            Repeater {
                id: actionRows
                model: root.actions
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: parent.width; height: 64 * shell.textScale; radius: shell.radius
                    color: index === root.selectedIndex ? shell.selected : shell.surface
                    border.width: index === root.selectedIndex ? 3 : 1
                    border.color: index === root.selectedIndex ? shell.selectedFocus : shell.outline
                    opacity: modelData.enabled ? 1 : 0.6
                    Text { anchors.centerIn: parent; width: parent.width - 24; text: modelData.name; color: index === root.selectedIndex ? shell.selectedText : shell.text; font.family: shell.fontFamily; font.pixelSize: 20 * shell.textScale; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                    MouseArea { anchors.fill: parent; onClicked: { root.selectedIndex = index; root.activate(index) } }
                }
            }
        }
    }
    Rectangle { anchors.fill: parent; visible: !!root.confirmation; z: 5; color: shell.scrim
        MouseArea { anchors.fill: parent }
        Rectangle { anchors.centerIn: parent; width: parent.width * 0.64; height: body.implicitHeight + 24 * shell.gapScale; radius: shell.radius; color: shell.surface
            Column { id: body; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16 * shell.gapScale; spacing: 12 * shell.gapScale
                Text { textFormat: Text.PlainText; width: parent.width; text: root.enableWebTitle(); color: root.ink; font.family: shell.fontFamily; font.pixelSize: 28 * shell.textScale; font.weight: Font.DemiBold; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
                Text { textFormat: Text.PlainText; width: parent.width; text: root.enableWebMessage(); color: root.ink; font.family: shell.fontFamily; font.pixelSize: 20 * shell.textScale; wrapMode: Text.WordWrap }
                Row { width: parent.width; spacing: 10 * shell.gapScale
                    Repeater { model: [qsTr("Cancel"), qsTr("Enable web")]
                        delegate: Rectangle { required property int index; required property string modelData; width: (body.width - 10 * shell.gapScale) / 2; height: 72 * shell.textScale; radius: shell.radius; color: shell.surface; border.width: root.confirmIndex === index ? 3 : 1; border.color: shell.focus
                            Text { textFormat: Text.PlainText; anchors.fill: parent; anchors.margins: 8; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; wrapMode: Text.WordWrap; text: modelData; color: root.ink; font.family: shell.fontFamily; font.pixelSize: 20 * shell.textScale }
                            MouseArea { anchors.fill: parent; onClicked: { root.confirmIndex = index; root.handleCommand(inputManager.confirm) } }
                        }
                    }
                }
            }
        }
    }
}
