import QtQuick

Item {
    id: root
    // Resume/restart confirmation dialog shown when a movie or episode has a
    // saved Kodi resume point. Offers Resume at the saved position or Restart
    // from the beginning. Back dismisses the prompt without starting playback.
    property int resumePosition: 0
    property string mediaTitle: ""
    property string mediaKind: ""
    property int mediaId: 0
    property int selectedIndex: 0 // 0=Resume, 1=Restart
    signal closed(bool resume) // true=resume at saved point, false=restart from beginning
    signal cancelled()

    readonly property bool hasResume: resumePosition > 0
    readonly property var options: hasResume ? [
        {label: qsTr("Resume"), subtitle: qsTr("at %1").arg(stamp(resumePosition)), action: true},
        {label: qsTr("Restart"), subtitle: qsTr("from the beginning"), action: false}
    ] : [
        {label: qsTr("Play from beginning"), subtitle: qsTr("No saved resume point"), action: false}
    ]

    function stamp(seconds) {
        const minutes = Math.floor(seconds / 60) % 60
        const secs = String(seconds % 60).padStart(2, "0")
        return minutes + ":" + secs
    }

    function show(){visible=true;selectedIndex=0}
    function hide(){visible=false;selectedIndex=0}
    // Leave playback untouched if the viewer backs out of the prompt.
    function back(){hide();cancelled()}
    function confirm(){
        const choice = options[selectedIndex]
        hide();closed(choice.action)
    }
    function handleCommand(command){
        if(!visible)return
        if(command===inputManager.back){back();return}
        if(command===inputManager.navigateUp || command===inputManager.navigateDown){
            selectedIndex = Math.max(0, Math.min(options.length-1, selectedIndex + (command===inputManager.navigateUp?-1:1)))
            return
        }
        if(command===inputManager.confirm){confirm();return}
    }

    visible: false
    anchors.fill: parent
    z: 100

    SurfacePalette { id:shell; appearance: root.appearance || ({}) }
    property var appearance: ({})

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width-96, 640)
        height: 300
        radius: 18
        color: shell.surface
        border.width: 2
        border.color: shell.focus
        Column {
            anchors.fill: parent
            anchors.margins: 28
            spacing: 20
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: hasResume ? qsTr("Resume “%1”?").arg(mediaTitle) : qsTr("Play “%1”?").arg(mediaTitle)
                font.family: shell.fontFamily
                font.pixelSize: 24
                color: shell.text
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                wrapMode: Text.Word
                text: hasResume ? qsTr("Resume at %1 or restart from the beginning.").arg(stamp(resumePosition))
                                : qsTr("No saved resume point. Starting from the beginning.")
                font.family: shell.fontFamily
                font.pixelSize: 17
                color: shell.muted
            }
            ListView {
                id: list
                width: parent.width
                height: Math.max(options.length * 56, 112)
                clip: true
                model: options
                currentIndex: root.selectedIndex
                spacing: 8
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: list.width
                    height: 52
                    radius: 10
                    color: index === root.selectedIndex ? shell.selected : "transparent"
                    border.width: index === root.selectedIndex ? 2 : 0
                    border.color: shell.selectedFocus
                    Row {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 12
                        Text {
                            text: modelData.label
                            color: index === root.selectedIndex ? shell.selectedText : shell.text
                            font.family: shell.fontFamily
                            font.pixelSize: 20
                            font.bold: true
                            width: parent.width - 160
                            elide: Text.ElideRight
                        }
                        Text {
                            text: modelData.subtitle
                            color: index === root.selectedIndex ? shell.selectedText : shell.muted
                            font.family: shell.fontFamily
                            font.pixelSize: 15
                            anchors.right: parent.right
                            elide: Text.ElideRight
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { root.selectedIndex = index; root.confirm() }
                    }
                }
            }
            Row {
                spacing: 16
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 8
                anchors.rightMargin: 8
                Text {
                    text: qsTr("OK chooses · Back " + (hasResume ? qsTr("resumes") : qsTr("plays")))
                    font.family: shell.fontFamily
                    font.pixelSize: 14
                    color: shell.muted
                }
            }
        }
    }
    MouseArea {
        anchors.fill: parent
        onPressed: {} // swallow clicks outside dialog
    }
}
