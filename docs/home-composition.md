# Configurable Home composition

Planned editor replacement: [Page Studio](page-studio-redesign.md), backlog T15-T24, supersedes the immediate-save editing workflow described below once accepted. This document describes the existing composition contract; Studio extends it through validated data and a shared draft/geometry model, preserving legacy layouts and both visual fixtures. No editor replacement is implemented by this documentation change.

Home supports an optional composed layout alongside existing grid layouts. Existing profiles are not rewritten or switched on startup. [The reference fixture](../tests/visual/home-reference/recreation-config.json) demonstrates a complete configuration. Its artwork and media/progress values are visual test data, not the user's library.

## Layout data

`home-layout.json` retains version 1 read compatibility and version 2 atomic writes with the original v1 backup. Add `composition` with `schemaVersion: 1` and `enabled: true` to select the composed renderer. Omit it or disable it to retain the established grid. Restore defaults removes the override and returns to the packaged layout.

Composition fields:

| Field | Behavior |
| --- | --- |
| `navigation` | Ordered tabs with unique `id` and a user-chosen `name`. A tab may contain a validated `action` or an ordered `sections` array of category IDs. Omit both to show all sections. |
| `initialTab` | Starting tab ID. Local tab identity survives content navigation; external launches retain exact return origins. |
| `columns` | One or two dashboard lanes. Narrow displays use one lane. |
| `margin`, `gap` | Content inset and section spacing in design units. |
| `fit` | Fit configured section rows within the available screen when true. False allows scrolling; directional focus reveals offscreen cards. Avoid excessive density that makes text unreadable. |
| `showProfile` | Optional profile label. Settings/Edit Home remain reachable in the composed header and by remote navigation. |
| `wallpaper` | Managed local PNG/JPEG under `artwork/`. The wallpaper fills the window while controls respect the device safe area. Missing images retain a readable gradient fallback. |
| `headerMode`, `identity` | Navigation tabs or an identity header with a configurable product/personal name. |
| `headerControls` | Ordered Search, Settings and/or Edit controls; retain Settings or Edit for recovery. Search opens the existing shell search/drawer. |
| `showClock`, `designHeight` | Optional live clock/date and the design-height scaling reference. |

## Universal grid (Studio M1, October 17)

An optional `grid` map inside `composition` declares the universal screen grid, and an optional `placement` rectangle on any component assigns it cells. The renderer now contains placement handling, but placement-specific geometry/scale acceptance remains open in T05 and is assigned to T17. Layouts without placements keep their established flow behavior.

| Field | Behavior |
| --- | --- |
| `grid.columns` | Integer 4–16, default 8. |
| `grid.rows` | Integer 3–10, default 5. |
| `grid.gap`, `grid.margin` | Number 0–100, defaults 12 and 20. |
| `grid.snap` | Boolean, default true; cell-snapped placement is the only supported mode. |
| `grid.overlap` | Boolean, default false; while false, placements may not share cells and grid changes may not create overlap. |

`placement` stores `x` (start column), `y` (start row), `width` and `height` (spans) as whole cells that must fit inside the declared grid — overflow is refused, never clamped. `HomeLayout` exposes `grid()`, `setGrid()`, `setItemPlacement()`, `clearItemPlacement()` and `gridCollisions(itemId, placement)` (which returns the `{id, name}` of each occupier, feeding the future shift/swap/cancel dialog). `setGrid` refuses changes that would strand or newly overlap placed widgets. Stored documents whose placements fall outside their grid are rejected on read and the packaged defaults are used; the stored file is never rewritten. Invalid edits fail with `errorString()` and roll back atomically, matching the composition contract. Legacy version-1 layouts load with default grid data and no migration write on read.

### Optional long-page policy (October 21, 2026)

For an enabled screen-grid page, `composition.verticalScrolling` is an optional boolean, default false when absent. Page appearance → Layout exposes **Allow vertical scrolling**, with a draft preview and Done/Cancel. Existing automatic/section layouts retain their behavior and use their explicit conversion workflow; stored documents are never converted on read.

When true, `grid.rows` still means **visible viewport density**, not document length. Root placements may extend downward within a separate 10,240-row bound; component spans remain at most ten rows high and nested grids retain their existing bounds. Content extent is derived from validated placements, with no stored empty-row count. The generic renderer keeps the same cell dimensions, renders a bounded viewport neighborhood and reveals spatial focus by placement. The earlier fixed-root containment rule continues to apply when this policy is false; overlap, locking and document/component limits are unchanged.

Studio's `scrolling` command validates an enabled boolean and previews safely. Turning it off refuses authored content below the first screen. Add suggests a vacant position, and its optional provisional `enableScrolling` flag accepts the policy plus the configured new component atomically; cancel/undo leave the previous fixed page intact. See [implementation and verification](studio-scrolling-2026-10-21.md) for source identities, evidence and remaining scope. This does not make a viewport grid with `rows > 10` valid.

Category `presentation` defines a section independently of its content:

| Field | Behavior |
| --- | --- |
| `variant` | `banner`, `media`, `source`, `summary`, `launcher`, `orb` or `recent`. Orbs have independent icon/art circles and optional label pills; recent cards have separate artwork, title and type subtitle. |
| `columns` | Cards per row, independent of other sections. |
| `height` | Preferred section height, including its heading/subtitle. Rows use the tallest section to calculate fit; neighboring sections can retain different heights. |
| `span` | One or two dashboard lanes. |
| `showHeading`, `subtitle` | Heading visibility and optional supporting text. |
| `contentWidth`, `cardGap` | Center a narrower section (such as a five-column source grid) independently of full-width rows. |
| `panelColor`, `panelOpacity`, `blur`, `padding`, `radius` | Configurable frosted-panel surface and padding. Opacity applies to the background, leaving labels/art opaque. |
| `headingSize`, `headingColor`, `headingBold`, `icon` | Compact or prominent section headings with optional line icons. |
| `trailingLabel`, `trailingAction` | A validated See All/context action reachable from the right edge of the row with directional input. |

Item `presentation` supports a variant override, subtitle, banner label, line-icon choice, independent surface color, image crop and optional progress fraction. Crop coordinates are original-image pixels (`x`, `y`, `width`, `height`). Only local managed artwork paths are accepted: `artwork/<safe-name>.png`, `.jpg` or `.jpeg`. Crop values and numeric layout values are bounded and validated; actions continue through the central dispatcher. No arbitrary code or remote image fetch is introduced.

Orb sizing/labels are configured with `diameter`, `iconSize`, `iconColor`, `labelSize`, `labelPill`, `labelColor` and `selectedLabelColor`. `surfaceOpacity` can soften the circle fill. Recent cards accept `artHeight` and a type `subtitle`. Section defaults are inherited and each item can override them. Artwork clips to its own true rounded/circular boundary, preserving the wallpaper behind its corners.

Frosted panels sample only the wallpaper, not interface content. A trusted optional Qt Quick Effects component adds blur/masking when available; unsupported renderers retain the configured translucent tint. See [Qt MultiEffect](https://doc.qt.io/qt-6/qml-qtquick-effects-multieffect.html) and [ShaderEffectSource](https://doc.qt.io/qt-6/qml-qtquick-shadereffectsource.html). This is not a dependency on user-supplied executable theme code or an online image service.

Banner, artwork and summary cards render their text, focus and controls as separate native components. Local artwork may be cropped without flattening the interface. Missing artwork leaves a usable card surface and label. Line icons include gamepad, display, HDMI and TV. The existing appearance model controls palette, text scale, font family, spacing, corners and motion.

## Editing and navigation

Customize Home → **Component layout** enables/disables composed Home, changes dashboard lanes, selects sections and changes their variant, card count, height and ordering with remote input. These controls save immediately, as the existing Home editor does. Make it yours remains the separate draft-preview/Apply/Cancel appearance editor. Choose image now accepts PNG and JPEG files.

The editor also chooses/removes a local wallpaper, adjusts panel opacity/blur, switches header style and toggles the live clock. Wallpaper import preserves original image bytes in a content-addressed per-profile artwork file and atomically applies its reference. The shared `app-drawer` action lets More Apps/favorites open the actual shell drawer without pretending an unconfigured service is installed or signed in.

Settings → Home → **Design Home** now exposes dashboard, section, card and navigation-tab properties as structured controls, including fine geometry, typography, glass, crops and action bindings. **Home skins** selects all three designs and saves, reapplies, renames or deletes personal snapshots with embedded artwork. See [Home skins](skins.md) for the workflow, storage contract and limits. Portable file import/export and undo remain future work.

Directional navigation follows rendered rows and horizontal positions, crosses sections, enters the tab bar above the first row and reaches Settings/Edit Home above the tabs. Tab lists scroll horizontally. Contextual pages filter the same components instead of duplicating them. Launches preserve exact component/tab origins. Removing sections can leave a tab empty while recovery remains reachable.

## Kodi and backend ownership

These are presentation components, not a new library or schedule database. Real media actions may use the existing validated Kodi collection/movie routes. The reference fixture uses static labels and progress, with media cards opening the real Movies/Shows library; it does not claim to resume those named programs. HDMI 4 and guide actions open setup until their actual backends are qualified. Music/Games/Kids demonstrate configurable local section pages rather than new backend integrations.

Live per-card Kodi artwork/progress binding, actual guide schedules and capture remain separate adapter work. Retain the distinction between a visual match and functional backend acceptance in every test report.

## Verification

The second [pastel reference case](../tests/visual/home-pastel-reference/README.md) exercises circular grids, glass panels, local wallpaper, identity/search/settings/clock, favorites and recent-card variants. Both reference cases run from configuration against the same executable. Added tests cover wallpaper import/rejection/reset/fallback, header recovery, differently sized row navigation, reachable See All, drawer dispatch and old-layout compatibility. Backend entitlement/history remains outside these visual fixtures.

Installed release `cfd551b699cf0c21f10efaf0` passed all thirteen suites and the actual reference recreation/component-editor/default-reset sequence. Both installed captures and the explicit visual review are retained in [the reference case](../tests/visual/home-reference/README.md). The visual review passes the landscape adaptation; the raw helper deliberately leaves visual judgment unset. VM evidence `/tmp/isolde-reference-config-5ax_j_yr`; installer log `/tmp/isolde-order-vsDgGY/composition-install.log`. Household restored active with zero restarts. An earlier screenshot harness attempt hit an X11 mapping race and another timed out while awaiting a capture; neither is counted as a pass. The helper now prevents fatal Xlib errors from bypassing restoration.

Component contract tests cover persistence/reload, malformed layout/actions/presentation, duplicate tabs, path traversal and default reset. QML tests exercise the actual reference layout, one-screen geometry, directional card/section navigation, tab filtering, return origins and recovery controls. The installed screenshot test must still be repeated and visually reviewed whenever these components change; see [the reference procedure](../tests/visual/home-reference/README.md).

Latest verification supersedes the earlier deployment above: release `523f627f0c06211f92b6e43b`, all 13 suites, both references selected/saved/reapplied through Settings, detailed editor opened/closed without changing configuration, and default reset preserving saved skins. See [skin evidence](../tests/visual/home-skins/README.md).
