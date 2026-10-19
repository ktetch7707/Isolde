import QtQuick

Item {
    id: root
    property var wallpaperItem: null
    property color tint: "#FFF3FA"
    property real tintOpacity: 0.6
    property real blur: 0.6
    property real radius: 24
    property bool blurAvailable: false
    property rect sampleRect: Qt.rect(0,0,width,height)
    property var effect: null
    function updateSample() {
        if (!wallpaperItem || !visible) return
        sampleRect = mapToItem(wallpaperItem,0,0,width,height)
    }
    function createEffect() {
        if (effect || !visible || !wallpaperItem || blur <= 0) return
        try {
            // Trusted optional effect. No user configuration is evaluated as QML.
            effect = Qt.createQmlObject('import QtQuick; import QtQuick.Effects; MultiEffect { anchors.fill: parent; autoPaddingEnabled: false; blurEnabled: true; blurMax: 32; maskEnabled: true }',effectLayer,"optionalPanelBlur")
            effect.source = sample; effect.maskSource = maskTexture
            effect.blur = Qt.binding(() => root.blur); blurAvailable = true
        } catch (error) { blurAvailable = false } // Qt 6.4 / reduced renderers retain the translucent panel.
    }
    Component.onCompleted: { updateSample(); createEffect() }
    onWallpaperItemChanged: { updateSample(); createEffect() }
    onBlurChanged: createEffect()
    onVisibleChanged: if (visible) { updateSample(); createEffect() }
    onWidthChanged: updateSample()
    onHeightChanged: updateSample()
    Timer { interval: 150; repeat: true; running: root.visible && !!root.wallpaperItem && root.blur > 0; onTriggered: root.updateSample() }
    ShaderEffectSource {
        id: sample; sourceItem: root.wallpaperItem; sourceRect: root.sampleRect
        textureSize: Qt.size(Math.max(1,root.width/2),Math.max(1,root.height/2)); visible: false
    }
    Rectangle { id: mask; anchors.fill: parent; radius: root.radius; color: "white"; visible: false }
    ShaderEffectSource { id: maskTexture; sourceItem: mask; visible: false }
    Item { id: effectLayer; anchors.fill: parent; visible: root.blurAvailable && root.blur > 0 }
    Rectangle {
        anchors.fill: parent; radius: root.radius
        color: Qt.rgba(root.tint.r,root.tint.g,root.tint.b,root.tintOpacity)
        border.width: 1; border.color: "#70FFFFFF"
    }
}
