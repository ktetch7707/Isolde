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

## Isolde implementation (this session)

### Poster reflection (explicit requirement)
- `src/home/ComponentSchema.cpp`: added presentation validators `reflection` (bool flag), `reflectionHeight` (numeric 0–400), `reflectionOpacity` (numeric 0–1) to the presentation handlers map and `validPresentationNumericField` dispatch.
- `qml/HomeCard.qml`: added `reflectionEnabled`/`reflectionHeight`/`reflectionOpacity` computed properties; renders a per-card reflection beneath the poster using `ShaderEffectSource` (samples the already-rendered poster texture — no new artwork request) + `ShaderEffect` that mirrors vertically from the poster's lower edge and fades to transparent (`opacity * (1 - y)`). Anchored at `poster.bottom`, clipped to `reflectionHeight`, gated on `poster.visible && poster.loadedSource`. No focus stop / accessible label.
- `config/studio-defaults.json`: inspector Appearance fields `Poster reflection` (toggle), `Reflection height` (slider 0–200), `Reflection opacity` (slider 0–100%), scoped to `library`/`section` presentation. Reuses the standard scalar/toggle transaction and live preview.

### Default skin
- `config/default-home-layout.json`: fresh-profile default (`builtin-default`) re-themed to Arctic dark palette (background #0b0d13, cards #141720, text #e8e6ea, highlight/accent #6ea8d1, tileRoundness 0.14). All original items/categories kept so existing profiles and navigation behavior are retained.
- `config/skins/arctic.json`: new named built-in skin (schemaVersion-1 wrapper, `{"name","layout","assets","schemaVersion":1}`) with the same Arctic layout; registered as `builtin-arctic` in `HomeLayout::skins()` and `skinSnapshot()` (src/home/HomeSkins.cpp).
- Existing profiles keep their saved skin/layouts via the normal user-path load; default-home-layout.json is only the fresh-profile default.

### Verification status
- No local Qt/build tooling on this host (win32, no qmake/cmake/g++), so C++/QML was not compiled locally and `tools/vm-test.ps1` was not run. JSON files validated by parsing. VM verification via the Isolde-Test VM and `ctest --test-dir build --output-on-failure` remains pending.
- Reflection is off by default (presentation.reflection undefined), so existing offscreen suites are unaffected until a shelf opts in.
