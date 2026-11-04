import QtQuick

// Consumers only. HomeLayout/ComponentSchema resolves inheritance and contrast.
// Fallbacks support isolated controls and startup before a profile is loaded.
QtObject {
    property var appearance: ({})
    readonly property color background: appearance.backgroundEnd || surface
    readonly property color surface: appearance.surfaceColor || "#202126"
    readonly property color selected: appearance.selectedColor || "#353039"
    readonly property color elevated: appearance.elevatedColor || surface
    readonly property color text: appearance.textColor || "#f5f1f3"
    readonly property color selectedText: appearance.selectedTextColor || text
    readonly property color elevatedText: appearance.elevatedTextColor || text
    readonly property color elevatedMuted: appearance.elevatedMutedColor || muted
    readonly property color elevatedFocus: appearance.elevatedFocusColor || focus
    readonly property color backgroundText: appearance.backgroundTextColor || text
    readonly property color backgroundMuted: appearance.backgroundMutedColor || muted
    readonly property color muted: appearance.mutedColor || "#aaa5af"
    readonly property color focus: appearance.focusColor || "#c994a5"
    readonly property color selectedFocus: appearance.selectedFocusColor || focus
    readonly property color outline: appearance.outlineColor || muted
    readonly property color accent: appearance.accentColor || focus
    readonly property color input: appearance.inputColor || selected
    readonly property color inputText: appearance.inputTextColor || text
    readonly property color success: appearance.successColor || text
    readonly property color warning: appearance.warningColor || text
    readonly property color error: appearance.errorColor || text
    readonly property color unavailable: appearance.unavailableColor || muted
    readonly property color scrim: Qt.rgba(scrimBase.r, scrimBase.g, scrimBase.b, appearance.scrimOpacity === undefined ? 0.85 : appearance.scrimOpacity)
    readonly property color scrimBase: appearance.scrimColor || "#000000"
    readonly property real radius: appearance.cornerRadius === undefined ? 4 + 12 * (appearance.tileRoundness === undefined ? 0.18 : appearance.tileRoundness) : appearance.cornerRadius
    readonly property real textScale: appearance.typographyScale || 1
    readonly property real gapScale: appearance.spacing === undefined ? 1 : appearance.spacing / 20
    readonly property string fontFamily: appearance.fontFamily || "DejaVu Sans"
    readonly property bool motion: appearance.motionEnabled !== false
}
