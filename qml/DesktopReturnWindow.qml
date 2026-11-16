import QtQuick
import QtQuick.Window
import QtQuick.Controls

Window {
    id:root;objectName:"desktopReturnWindow"
    required property var controller
    required property var appearance
    property bool nativeSessionActive:false
    readonly property bool nativeNoticeEligible:controller.active && controller.nativeMode && nativeSessionActive
    property bool nativeNoticeVisible:false
    onNativeNoticeEligibleChanged: {
        nativeNoticeVisible=nativeNoticeEligible
        if(nativeNoticeEligible)nativeNoticeTimer.restart();else nativeNoticeTimer.stop()
    }
    readonly property var style:controller.presentation
    width:style.width || 360;height:controller.nativeMode?noticeText.implicitHeight+24:(style.height || 120)
    x:Screen.width-width-(style.margin || 0);y:style.margin || 0
    title:style.returnLabel || "";color:"transparent"
    flags:Qt.Tool | Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint
          | (controller.nativeMode ? Qt.WindowDoesNotAcceptFocus | Qt.WindowTransparentForInput : 0)
    transientParent:null
    visible:controller.active && (!controller.nativeMode || nativeNoticeVisible)
    onVisibleChanged:if(visible && !controller.nativeMode){requestActivate();Qt.callLater(()=>returnButton.forceActiveFocus())}
    Timer {id:nativeNoticeTimer;interval:10000;onTriggered:root.nativeNoticeVisible=false}
    Rectangle {
        anchors.fill:parent;radius:root.style.radius || 0;color:root.appearance.surfaceColor
        border.width:2;border.color:root.appearance.focusColor
        Column {
            anchors.fill:parent;anchors.margins:12;spacing:8
            Text {
                id:noticeText;objectName:"desktopShortcutDescription";width:parent.width
                text:(root.controller.nativeMode ? (root.style.returnLabel || "") : (root.style.title || ""))+" · "+root.controller.shortcutDescription
                color:root.controller.nativeMode?root.appearance.textColor:root.appearance.mutedColor
                font.pixelSize:(root.style.labelSize || 20)*(root.controller.nativeMode?1:.8)
                wrapMode:root.controller.nativeMode?Text.Wrap:Text.NoWrap
                elide:root.controller.nativeMode?Text.ElideNone:Text.ElideRight
            }
            Button {
                id:returnButton;objectName:"desktopReturnButton";width:parent.width;height:Math.max(40,parent.height-32)
                visible:!root.controller.nativeMode
                focus:true
                text:root.style.returnLabel || "";onClicked:root.controller.returnHome()
                contentItem:Text {text:returnButton.text;color:root.appearance.textColor;font.pixelSize:root.style.labelSize || 20;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
                background:Rectangle {radius:root.style.radius || 0;color:root.appearance.selectedColor;border.width:returnButton.activeFocus?2:0;border.color:root.appearance.focusColor}
                Keys.onPressed:event=>{const command=inputManager.commandForKey(event.key);if(command===inputManager.back || command===inputManager.home){root.controller.returnHome();event.accepted=true}}
            }
        }
    }
}
