import QtQuick

// HomeLayout/ComponentSchema is the single contrast authority for raw tokens
// and already-resolved route contexts. No mutation or second opacity blend.
// Fallbacks support isolated controls and startup before a profile is loaded.
QtObject {
    property var appearance: ({})
    readonly property var tokens: typeof homeLayout !== "undefined" && homeLayout
        && typeof homeLayout.resolvedSurfacePalette === "function"
        ? homeLayout.resolvedSurfacePalette(appearance) : appearance
    readonly property color background: tokens.backgroundEnd || surface
    readonly property color surface: tokens.surfaceColor || "#202126"
    readonly property color selected: tokens.selectedColor || "#353039"
    readonly property color elevated: tokens.elevatedColor || surface
    readonly property color text: tokens.textColor || "#f5f1f3"
    readonly property color selectedText: tokens.selectedTextColor || text
    readonly property color elevatedText: tokens.elevatedTextColor || text
    readonly property color elevatedMuted: tokens.elevatedMutedColor || muted
    readonly property color elevatedFocus: tokens.elevatedFocusColor || focus
    readonly property color backgroundText: tokens.backgroundTextColor || text
    readonly property color backgroundFocus: tokens.backgroundFocusColor || focus
    readonly property color backgroundError: tokens.backgroundErrorColor || error
    readonly property color elevatedError: tokens.elevatedErrorColor || error
    readonly property color backgroundMuted: tokens.backgroundMutedColor || muted
    readonly property color muted: tokens.mutedColor || "#aaa5af"
    readonly property color focus: tokens.focusColor || "#c994a5"
    readonly property color selectedFocus: tokens.selectedFocusColor || focus
    readonly property color outline: tokens.outlineColor || muted
    readonly property color accent: tokens.accentColor || focus
    readonly property color input: tokens.inputColor || selected
    readonly property color inputFocus: tokens.inputFocusColor || focus
    readonly property color inputMuted: tokens.inputMutedColor || muted
    readonly property color inputText: tokens.inputTextColor || text
    readonly property color success: tokens.successColor || text
    readonly property color warning: tokens.warningColor || text
    readonly property color error: tokens.errorColor || text
    readonly property color unavailable: tokens.unavailableColor || muted
    readonly property color scrim: Qt.rgba(scrimBase.r, scrimBase.g, scrimBase.b, tokens.scrimOpacity === undefined ? 0.85 : tokens.scrimOpacity)
    readonly property color scrimBase: tokens.scrimColor || "#000000"
    readonly property real radius: tokens.cornerRadius === undefined ? 4 + 12 * (tokens.tileRoundness === undefined ? 0.18 : tokens.tileRoundness) : tokens.cornerRadius
    readonly property real textScale: tokens.typographyScale || 1
    readonly property real gapScale: tokens.spacing === undefined ? 1 : tokens.spacing / 20
    readonly property string fontFamily: tokens.fontFamily || "DejaVu Sans"
    readonly property bool motion: tokens.motionEnabled !== false
}
