import QtQuick

Item {
    id: root
    property int selectedIndex: 0

    readonly property var rows: [
        { kind: "toggle", name: "Automatic source tiles", detail: "Show newly detected discs, drives, and media folders on Home when a compatible app is installed.", status: sourceManager.automaticSourcesEnabled ? "On" : "Off" },
        { kind: "refresh", name: "Refresh detected sources", detail: "Scan media and removable-drive locations again.", status: sourceManager.visibleSourceCount + " visible" }
    ].concat(sourceManager.sources)

    function sourceTypeLabel(type) {
        if (type === "movie-disc") return "Movie disc"
        if (type === "game-disc") return "Game disc"
        if (type === "media-folder") return "Media folder"
        if (type === "file-source") return "File source"
        return "Detected source"
    }

    function rowName(row) {
        return row.kind ? row.name : row.name + " · " + sourceTypeLabel(row.sourceType)
    }

    function rowDetail(row) {
        return row.kind ? row.detail : row.target + " · opens with " + row.openWith
    }

    function rowStatus(row) {
        return row.kind ? row.status : row.contextual ? "Contextual" : "Known"
    }

    function handleCommand(command) {
        if (command === inputManager.navigateUp)
            selectedIndex = Math.max(0, selectedIndex - 1)
        else if (command === inputManager.navigateDown)
            selectedIndex = Math.min(rows.length - 1, selectedIndex + 1)
        else if (command === inputManager.navigateLeft || command === inputManager.navigateRight) {
            const row = rows[selectedIndex]
            if (row && row.kind === "toggle")
                sourceManager.automaticSourcesEnabled = !sourceManager.automaticSourcesEnabled
        } else if (command === inputManager.confirm) {
            const row = rows[selectedIndex]
            if (!row)
                return
            if (row.kind === "toggle")
                sourceManager.automaticSourcesEnabled = !sourceManager.automaticSourcesEnabled
            else if (row.kind === "refresh")
                sourceManager.refresh()
            else
                sourceManager.hideSource(row.id)
        }
    }

    Column {
        anchors.fill: parent
        spacing: 18

        Text {
            text: "SOURCES"
            color: "#F8FAFC"
            font.pixelSize: 30
            font.weight: Font.DemiBold
            font.letterSpacing: 3
        }

        Text {
            width: parent.width
            text: "Detected sources are temporary until you keep them on Home. Permanent drives and folders should be added through Add Tile so the user owns the layout."
            color: "#9EABC1"
            font.pixelSize: 18
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: root.rows
            delegate: Rectangle {
                required property int index
                required property var modelData
                width: parent.width
                height: 86
                radius: 10
                color: root.selectedIndex === index ? "#2B416B" : "#111A2A"
                border.width: root.selectedIndex === index ? 3 : 1
                border.color: root.selectedIndex === index ? "#F8FAFC" : "#2B3853"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    width: 54
                    text: modelData.kind ? "⚙" : modelData.symbol
                    color: modelData.kind ? "#91A0BC" : modelData.accent
                    font.pixelSize: 28
                    font.weight: Font.Bold
                    horizontalAlignment: Text.AlignHCenter
                }

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 88
                    anchors.right: statusText.left
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 5
                    Text { text: root.rowName(modelData); color: "#F8FAFC"; font.pixelSize: 21; font.weight: Font.DemiBold; elide: Text.ElideRight; width: parent.width }
                    Text { text: root.rowDetail(modelData); color: "#9EABC1"; font.pixelSize: 14; elide: Text.ElideRight; width: parent.width }
                }

                Text {
                    id: statusText
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    width: 150
                    horizontalAlignment: Text.AlignRight
                    text: root.rowStatus(modelData)
                    color: modelData.kind === "toggle" && sourceManager.automaticSourcesEnabled ? "#A7F3D0" : "#91A0BC"
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 92
            radius: 10
            color: "#101A2D"
            border.color: "#26344F"
            Text {
                anchors.fill: parent
                anchors.margins: 18
                text: "Controls: ← → toggles automatic tiles · Enter refreshes or hides the selected detected source · Home shows new sources first with Keep/Hide commands."
                color: "#C8D3E5"
                font.pixelSize: 16
                wrapMode: Text.WordWrap
            }
        }
    }
}
