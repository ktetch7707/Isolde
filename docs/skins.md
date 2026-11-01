# Home skins

Planned replacement, 2026-10-19: [Page Studio](page-studio-redesign.md) will consolidate page/layer/template/appearance tools into one visual draft workspace. Existing workflows/storage below describe the current implementation, not the target UX. T15-T24 own the replacement; retain current compatibility until T24 cutover acceptance.

Open **Settings → Home → Home skins**. Use Up/Down to choose a design, Left/Right to choose an action, and OK to continue. Mouse selection is also supported.

- **Isolde default** restores the original Home.
- **Cinema lounge** selects the dark reference design.
- **Pastel sky** selects the cloud wallpaper, circular shortcuts and glass-panel design.
- **Save current as new skin** asks for a name and saves the current Home with its artwork. Saving makes a new independent copy, including when names repeat.
- **Rename saved skin** and **Delete saved skin** manage personal copies. Built-ins remain available. Deleting a saved skin does not change the current Home.

Applying a skin replaces the current Home design after an in-app confirmation. Save the current design first to keep it. Back cancels the confirmation or name entry. Restoring defaults does not delete saved skins.

## Personalizing a design

**Settings → Home → Design Home** exposes structured controls for the dashboard, selected section, selected card and navigation tab. No JSON editing is required. Up/Down chooses a control, Left/Right adjusts numbers/options, and OK opens text entry. Changes save immediately.

Dashboard controls include columns, margin, gap, fit, design height, profile label, header style/name/buttons/clock, starting tab and wallpaper. Section controls include name, columns/span/height/centered width, headings, tint/opacity/blur/padding, link label/action and card defaults. Card controls include all seven presentation variants, subtitles, badges, icons, colors, circle/icon/label sizes, opacity, corners, spacing, artwork height, progress and original-pixel artwork cropping. Navigation tabs can be added, removed, renamed, filtered to section IDs and bound to validated actions. Section IDs are shown in the editor. Empty text removes the corresponding override; reset-presentation clears a section/card's overrides.

**Component layout** retains the compact editor and section reordering. Existing Home controls add/remove/reorder cards and categories, choose sources and change title/logo/type visibility. **Make it yours** edits the palette, typography scale/font family, grid, spacing, corners and animation with preview/Apply/Cancel.

## What a skin contains

A versioned Home document includes colors, typography, composition, navigation, section/card ordering, presentation and existing action/source bindings. PNG/JPEG artwork is embedded in the saved snapshot. Applying imports content-addressed copies into managed artwork, validates the document and writes Home atomically. Missing/invalid assets, malformed layouts and failed Home writes leave the current Home document and in-memory design intact. Failed imports may leave unused content-addressed artwork, which is harmless and is not an applied design.

Skins are stored in `skins/user-<UUID>.json` next to the current profile's `home-layout.json`. They are isolated by profile and survive default resets and later changes/removal of active artwork. Names accept 1–80 characters. Snapshot size is limited to 64 MiB; individual images to 20 MiB. Built-ins are packaged under `config/skins` with a resource fallback.

Device safe area/UI scale, audio, network, installed apps, browser sign-ins, Kodi libraries and playback history are not changed by a skin. Reference presets contain sample cards/artwork and existing validated routes; selecting them does not install or sign into pictured providers or create viewing history. The saved Home can preserve media-folder/action paths, so treat its file as personal configuration. Portable file import/export and undo remain future work.

## Verification

Component tests cover built-ins, snapshot persistence, artwork independence, process reload, profile isolation, renaming/deleting, invalid names/paths/assets and failed-write rollback. Actual QML tests enter both editors from Home settings, apply and save a skin, switch designs, edit geometry/glass/card controls and verify every presentation property in the two reference fixtures is represented.

`tools/home-reference-acceptance.py <installed-executable> --case tests/visual/home-pastel-reference --skin-from-settings --capture-library` starts from default Home in a disposable profile, chooses the preset through Settings, saves/reapplies a personal skin through remote input, pauses for recreation/default screenshots and verifies reset preserves the saved skin. It restores the regular service in `finally`. Repeat without `--capture-library` for `tests/visual/home-reference`.

Installed verification: release `523f627f0c06211f92b6e43b` passed all 13 suites and both Settings-only graphical round trips. [Screenshots and reports](../tests/visual/home-skins/README.md) include actual UI captures. Household restored active with zero restarts.

Whole-interface skins now include authored pages, and the page editor can save page-only skins separately. Built-in Home presets preserve the page collection. See [pages.md](pages.md).
