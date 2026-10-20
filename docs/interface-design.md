# Interface design and customization

Additive shell-surface requirements (user, 2026-10-19): [shell-surfaces-design.md](shell-surfaces-design.md) governs configurable contained source notices (T25), a dedicated All Apps gallery distinct from Search (T26), shared Home/page theme inheritance through Search/Settings/Isolde Kodi screens (T27), and a genuine default-top-right Search header switch (T28). Priority revision (user): finish the complete core Studio T15-T24 first. T22 depends on T21; the four shell fixes no longer block templates, Preview/Apply or core cutover. Then T27 depends on T24; T25/T26 consume T27; T28 depends on T25/T26/T27 and verifies their combined integration with the accepted Studio. This adds four zero-percent tasks: 24 active tasks, 11% overall. No application or VM change is part of this planning revision.

## Current editing direction — Page Studio

User decision, 2026-10-19: replace Customize Home's item/action-grid workflow with [Page Studio](page-studio-redesign.md), a visual page canvas with remote pick-up/drop, Pages/Layers, a contextual inspector, undo and draft-only editing until Apply. The specification includes TV/desktop wireframes, remote state rules, collision proposals, compatibility and usability acceptance. Backlog T15-T24 is the selected execution track; T06-T08 are superseded. Old editing/implementation-sequence descriptions below remain historical where they conflict. Content curation extensions remain out of scope; resource bounds remain enforced.

A second standing reference now covers a pastel wallpaper/glass/orb layout: [pastel acceptance case](../tests/visual/home-pastel-reference/README.md). Keep both visual cases passing as shared features change. Their opposing appearances must come from configuration, not separate hard-coded screens.

October 16, 2026. The supplied *Isolde Unified Television Interface Design Specification* is the reference for a polished default preset. The user's subsequent instruction takes precedence: users should be able to change arbitrary presentation and organization choices. The proposal's permanent destinations, fixed Home hierarchy, accent and category names are defaults, not restrictions. This document governs interface work alongside the master roadmap.

## Default experience

Use a quiet charcoal and muted-rose palette, generous negative space, restrained typography and focus indicated by outline, contrast and modest scale. Home starts as a device/application launcher, with eight configurable HDMI slots, Desktop, favorites, All Apps and a compact guide shortcut. The default should fit a television without vertical scrolling. Only configured, available sources may be presented as usable; capture and guide capabilities still need their backend and hardware qualification.

Default navigation proposes Home, Library, TV Guide, Music and Games. Search, Settings and a clock are visually unobtrusive and easy to reach. Library defaults to categories at the left and horizontal portrait-poster shelves, around seven to nine visible posters at normal density. Left from the first poster reaches the category navigator. Focus reveals metadata in one clear information area. Advertising, compulsory feeds and autoplay previews are excluded from the default.

These are targets, not a claim that the current grid/list implementation already matches them. Do not overwrite an existing user's layout to introduce the preset. Offer an explicit preview/apply path and retain a compatible backup.

## User ownership

| Area | Required user controls |
| --- | --- |
| Navigation | Add, rename, reorder, hide and remove tabs; choose icons, destinations and initial tab; customize header/clock placement and visibility. Keep Settings/recovery reachable through remote commands even when hidden visually. |
| Home | Add/remove/reorder sections, sources, favorites, shelves and widgets; name and map inputs; choose grid/rows, tile size, density, spacing and alignment; choose a compact one-screen preset or a scrolling personal layout. |
| Library | Add/rename/reorder/hide categories and shelves; overlapping membership; All Titles view; existing Kodi-backed views/collections and folder access; contextual source binding and per-view sorting. T02 manual/smart/tag/playlist/exclusion curation expansion remains excluded. |
| Cards and shelves | Choose poster, landscape, logo or text presentation; art source/crop, size, labels, metadata placement, opacity, corners and focus treatment. Reuse content independently of placement. |
| Visuals | Arbitrary colors, backgrounds/images, optional dimmed fanart, typography, spacing, density, focus effects and motion. Offer coherent presets and accessible contrast/reduced-motion options. |
| Advanced | Widgets and their configuration, validated actions, conditional visibility, reusable themes/layouts, undo, local backup and portable import/export. |

Every new visual component must expose meaningful presentation choices through a shared model rather than scattering fixed values across screens. Backend capabilities and recovery behavior remain validated. A theme is declarative data, not executable QML or shell code. Kodi continues to own library metadata, artwork acquisition/cache, progress and playback. Categories and shelves reference Kodi identities/rules; they do not duplicate media files or build a competing database.

“Unlimited” categories and shelves means no arbitrary product quota. Virtualize lists, load artwork incrementally and page data. Retain documented resource/validation bounds against malformed input, and show an actionable explanation if a bound is reached. Current validated category/component/page bounds remain mandatory; virtualization does not authorize increasing or bypassing them. Any later limits change needs a separately reviewed schema/resource proposal.

Customization stays hidden during ordinary browsing. Context actions and a dedicated editor provide remote-operated editing with live preview, explicit Apply/Cancel, stable focus and a recoverable default. Bad color choices must not make the editor or recovery controls unreadable. A high-contrast editor is allowed to retain its own stable palette while the preview displays the user's draft.

## Implementation sequence

1. **Appearance foundation, implemented:** persist focus color and typography from themes; per-profile arbitrary colors, Home columns, spacing, corners and motion with draft preview/Apply/Cancel. Home tiles, clock, header and library consume the shared appearance. Existing layouts load without a startup rewrite. This does not yet theme every Settings/Store/overlay surface.
2. **Navigation and composition:** editable destination/section model, default preset preview, remote navigation and exact focus restoration, All Apps, header controls and empty-layout recovery. Migrate existing components compatibly. Reach the one-screen default while allowing personal layouts to scroll.
3. **Poster library and reusable content:** virtualized category/shelf browsing and shared source definitions; adapt existing movie rules, shows/episodes and Kodi collections/playlists; add manual membership and smart combinations without changing media. Replace implementation quotas with measured resource bounds.
4. **Complete customization:** contextual item/shelf editor, backgrounds/fanart, per-component variants, sidebar behavior, tab/widget actions, undo and portable themes/layout backup. Extend shared appearance across Settings, Store, Search and overlays. Test long labels, density extremes, empty/offline states and reduced motion.
5. **Connect real destinations:** guide, music and games consume actual adapters when available. Never fill a proposed design with fabricated schedules, media or devices. Continue the remaining master-roadmap hardware, onboarding and appliance work.

Search follows the category/shelf structure and complements it. A known title must remain reachable through browsing. Verify personalization survives restart and profile switches, preview/cancel writes nothing, default upgrades preserve user edits, large libraries remain responsive, and every ordinary action works with directional keys. Capture/guide offline claims require real backend acceptance; a layout screenshot is insufficient.

## Configuration-only reference test October 16

The later component implementation supersedes the failure below for visual composition: [Home composition](home-composition.md) adds configurable navigation, one/two-lane section layouts, banner/media/source/summary cards, local image cropping, below-artwork titles and progress, line icons and a remote section-layout editor. The updated reference fixture adapts the portrait arrangement to a landscape TV, placing banner and media alongside each other above sources and guide. Backend data qualification remains separate; fixture labels/progress are explicitly synthetic.

The original reference is retained in [tests/visual/home-reference/reference.jpg](../tests/visual/home-reference/reference.jpg). Its [acceptance procedure and visual criteria](../tests/visual/home-reference/README.md) govern future Home feature changes. Rerun the configuration-only recreation and default-reset screenshot pair as features land. Require a convincing composition, not merely a matching palette. The previous screenshots and exact attempted configuration are retained beside the reference as the failed baseline.

**Failed.** The user's supplied photo is now a concrete composition acceptance case. The installed release `0bf049b4a83a64ae665985e4` was configured through its existing layout/theme data only, without QML changes. A temporary profile matched charcoal/rose colors, section names/order, rounded tiles and a four-source row. It could not reproduce the editable destination bar, banner geometry, three artwork cards with below-card titles/resume progress, independent compact source-card treatment or compact guide-summary component. Generic Movies/Television launchers under a Continue Watching heading are not a functional Continue Watching shelf. The guide setup launcher is not a working guide. Neither is counted as a match. Image-fill launcher artwork offers a limited banner substitute, not a complete hero component.

Both actual screenshots were captured: `home-reference-config-attempt.png` and `home-restored-default.png` under the session's visualization output directory. Exact attempted settings are retained as `home-reference-attempt-config.json`. `tools/home-reference-acceptance.py` reproduces the test, pauses for screenshot acknowledgements, invokes the existing Restore defaults action with remote keys, verifies removal of the temporary override and shows the first default item. VM evidence: `/tmp/isolde-reference-config-wyfwf2jl/result.json` and `attempt-config.json`. Default reset passed; the normal Household service was restored active with zero restarts. Existing personal configuration was not changed.

Next-slice completion must include configuring a recognizably equivalent television adaptation, capturing it, restoring packaged defaults and capturing those again. The current theme controls pass their own narrower tests but do not pass this composition requirement. Implement the navigation/section/card variants as reusable configuration options; do not hard-code the reference into a special Home screen. Keep backend capability gaps visible and separate from visual-layout acceptance.

All three Home designs are now selectable in Settings → Home → Home skins. The Design Home editor exposes reference-layout properties and personal snapshots save the resulting design with artwork. See [skins.md](skins.md).

The page composer applies Home's design controls to reusable destination/library pages. Pages can share multiple entry routes and contain independent Kodi-backed widgets. See [pages.md](pages.md) for scope, workflow and bounds.
