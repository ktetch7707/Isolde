# Home customization

Isolde 0.0.1.2 treats the Home screen as user-owned state. The built-in layout is only the safe first-run and recovery default.

The supplied interface proposal now guides the default preset, with broader customization governed by [interface-design.md](interface-design.md). Permanent tabs, the proposed Home hierarchy and library category names are editable defaults. The current implementation is still the grid Home/list Library; the poster-shelf/navigation composition is the next implementation package.

**Make it yours** in Customize Home provides a per-profile draft preview for arbitrary opaque colors (selection, background, surfaces and text), text scale, Home columns, spacing, corners and focus animation. Up/down chooses a field; left/right adjusts numbers; OK edits a color using the remote keyboard or toggles motion. Apply commits atomically, while Back/Cancel discards the draft. The editor keeps a readable fixed palette even when the preview colors are unreadable. Theme application now persists focus color and text scale, which previously appeared only in previews. The Quiet Rose preset offers the proposal's charcoal/rose direction. Existing saved layouts retain their backgrounds and arrangement; no startup rewrite occurs. Settings/Store and some overlays still need shared-theme adoption.

Verified/deployed October 16: release `0bf049b4a83a64ae665985e4`, all thirteen automated suites passed at installation. Added coverage checks arbitrary colors, invalid values, profile isolation, reload, failed-save rollback and QML preview/cancel/apply. Installed window-targeted remote acceptance passed source repair/migration, empty-Home recovery and appearance preview/cancel/apply. The preview was visually inspected. Evidence on the VM: `/tmp/isolde-components-acceptance-9no6g23t/result.json` and `shell.log`; installer log `/tmp/isolde-order-vsDgGY/appearance-install.log`. Household restored active with zero restarts. This verifies appearance controls, not completion of the proposed navigation/poster interface.

## Current editor

Open **Customize Home** from its Home tile, or press `C` as a recovery shortcut. The editor is remote-first: up/down selects a Home item, left/right selects an operation, and Enter applies it.

The editor can reorder and remove items, cycle compact/standard/wide tile sizes, adjust each tile or widget's opacity continuously from invisible to opaque, independently show or hide each tile's title, type label, and logo, expand logos to fill their tiles, choose arbitrary PNG artwork, remove custom artwork, adjust global tile roundness continuously from square to fully rounded, show or hide the Isolde name, select a background palette, add clock widgets, and restore defaults. This supports logo-only, title-only, text-only, and completely minimal tiles. PNG alpha channels are preserved byte-for-byte, so transparent artwork composites naturally over the tile and Home background. Changes are written atomically to the platform application-config directory as `home-layout.json`; selected PNGs are validated and copied into Isolde's managed `artwork` directory.

Built-in art packs provide Isolde-owned starter badges and colors for common platforms, consoles, app categories, and detected source types. Applying built-in art changes the tile's user-owned presentation without changing the installed app definition. User-selected PNGs remain available for exact personal artwork, and future official art packs can use the same catalog boundary.

Theme packs are previewed before they are applied. The Home editor opens a TV-scale preview surface with the selected pack's background, focus color, typography scale, tile opacity, tile roundness, sample media/game/music/system tiles, and explicit Apply/Esc behavior. Previewing must not mutate the saved Home layout; only Apply writes the selected theme. Users can apply only the theme's global look or apply the theme plus suggested badge/color mappings for matching tiles.

The Isolde name is off by default. Product branding can be used on a future startup splash without consuming permanent Home-screen space.

## User-defined categories

Home categories are arbitrary, ordered user data rather than hard-coded app types. Users can create and rename categories, assign any app tile or widget, remove categories, or leave items Uncategorized. Removing a category never removes its contents; those items safely return to Uncategorized. The default demonstrates Widgets, Apps, Music, Inputs, and System, but none of those names are enforced.

Permanent sources should be created intentionally through Add Tile → Assign Action rather than appearing forever just because Isolde detected them once. The layout model now supports typed action metadata for permanent tiles: launch an app, open a folder, open a file, open a URL, open a disc, or enter Desktop Mode. Action tiles also carry an `openWith` handler so the user can choose Kodi, Plex, VLC, an emulator, or another compatible installed app without hard-coding one product path into Isolde. Apps declare their `handledActions`, so the Open With list shows only apps that can handle the selected action. Desktop handlers also declare `actionHandlers`, which tell Isolde how to pass the target to that app. Automatic tiles are reserved for newly detected/contextual sources such as inserted movie discs, game discs, USB media, or removable drives. A user should be able to turn automatic source tiles off, hide a specific detected source, or promote a detected source into a normal permanent tile.

Detected sources appear in a separate `Detected Sources` Home section and are labeled `NEW SOURCE`. They are not saved into the permanent layout unless the user keeps them. Source tiles carry classification metadata such as movie disc, game disc, media folder, or file source so the Home label and future handler ranking can make different sources feel distinct. This preserves the rule that a stable NAS folder, always-attached media drive, or favorite disc target should be created intentionally, while a newly inserted drive can still appear immediately enough to feel appliance-like.

## Widget direction

Widgets use the same ordered item model as app tiles. Clock is the first supported widget type. Weather is planned as a permission-aware widget backed by a provider interface so location access, refresh policy, offline behavior, and attribution are explicit rather than embedded in the Home screen.

Future editor work includes direct drag/reorder input, custom images and gradients, per-tile artwork, widget configuration, profiles, layout export/import, and undo history.

Home designs can now be selected and saved in Settings → Home → **Home skins**. **Design Home** exposes the detailed composition and card controls. See [Home skins](skins.md).
