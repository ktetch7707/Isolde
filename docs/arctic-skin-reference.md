# Arctic: Zephyr reference pin (task evidence)

Pinned per assignment requirement #1. Source: official Kodi.tv Omega listing + upstream GitHub releases.

## Reference identity
- Skin: Arctic: Zephyr - Reloaded
- Version: 3.0.3 (official Kodi.tv Omega add-on listing)
- Release date: February 8, 2025
- Kodi version line: Omega
- Authors: jurialmunkey, beatmasterrs
- License: Creative Commons Non Commercial 3.0
- Kodi listing: https://kodi.tv/addons/omega/skin.arctic.zephyr.mod/
- Upstream source: https://github.com/beatmasterRS/skin.arctic.zephyr.mod
- GitHub releases: https://github.com/beatmasterRS/skin.arctic.zephyr.mod/releases
  - Latest tagged release on GitHub: 2.0.4 (supports Kodi Nexus), tag `2.0.4`, commit f927009
  - The Omega add-on listing carries 3.0.3, which is the version pinned for this parity task.

## Key visual behaviors inventoried (from listing/screenshots + source inspection)
- Artwork-led Home with poster shelves; poster reflections under movie posters in a shelf (emphasis, spacing, fade).
- Main menu / submenu navigation, hubs, widgets, library view modes, details, context menus, search, playback overlays, Live TV/radio, skin settings.
- Light/dark coordinated appearances; focus transitions; emphasis and spacing conventions.

## Isolde Arctic skin

The Arctic preset uses a bottom navigation row and a scrolling stack of sections. Movies and Continue watching use Kodi-backed poster rows, while an Apps section shows installed apps. The bottom tabs give direct access to Movies, TV Shows, Live TV, Games, and All Apps. Studio can edit the sections, content sources, spacing, and navigation controls.

**Show focused movie artwork and details** is available on library shelves in Studio. The page displays the focused shelf's selected title, synopsis, available metadata, and Kodi fanart, with the poster as a fallback. Left and Right select posters within the shelf. Up and Down move between shelves, and each shelf preserves its selection. Kodi still owns the media and playback actions.

**Featured artwork width** and **Featured artwork height** adjust the hero per shelf. Set both to 100% to fill the page with artwork behind the poster row and navigation. **Featured background** follows the page theme, uses the color chosen in **Featured background color**, or matches the focused artwork. Artwork mode samples the visible image and keeps the theme's underlying lightness for text contrast. Arctic starts in artwork mode.

**Edit component** opens the shelf controls directly. **Show shelf label** reveals the label text field. **Poster width**, **Poster height**, and **Shelf height** set the shelf dimensions, while **Border width** and **Border** in Style set the poster outline. The page's available space constrains the result. From the last component, press Down to reach Arctic's bottom navigation. **Header → Time → Clock color** sets the time color independently of the date.

The media card reserves space below each poster for its reflection. The existing artwork canvas mirrors the painted pixels and fades their alpha, preserving the crop and corners and using the same artwork request. Studio exposes **Poster reflection**, **Reflection height**, and **Reflection opacity** in Appearance. The effect adds no navigation stop.

`config/skins/arctic.json` is a packaged, editable v2 component layout. The fresh-profile launcher uses the Arctic palette with its existing shortcut lineup expressed as v2 components. Saved profiles keep their existing layouts until a skin is applied. The merged arbitrary script action was removed from the layout schema, dispatcher, and editor.
