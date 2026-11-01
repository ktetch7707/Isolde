# Pastel Home reference acceptance

The user supplied [reference.jpg](reference.jpg) as a second, harder customization test. Keep the original unchanged. Its SHA-256 is `5bfdf90835b56e2256f20a483de1013fec7afa69186c14c2b00f0766e8952f36`. A convincing recreation must be expressible through normal layout configuration and reusable components. Never display the reference as a flattened Home screen.

## Required composition

- Full-bleed pastel sunrise/cloud/mountain wallpaper, independent of the interface.
- Configurable identity mark/name, Search and Settings controls in a translucent pill, plus a live time/date display.
- Two centered rows of five circular launchers: independent pastel fills, line icons, labels below, optional label pills and a clear white focus ring/glow.
- Favorites inside a rounded translucent/blurred panel, with a small heading/heart and nine circular app entries.
- A second frosted panel containing a small recent-items heading, reachable See All action, and five landscape art cards with separate titles/type subtitles.
- Dark readable type, soft borders, consistent spacing and one-screen fit. All labels and controls must remain independent widgets. The wallpaper must have no baked-in UI.
- Directional navigation must handle different row widths, header controls, See All and exact return focus. Reset must restore the packaged default and remove the wallpaper override.

The 1280 × 720 reference may be adapted to the test display while preserving its hierarchy and proportions. The clock shows actual time rather than the photo's fixed timestamp. Capture output, viewport and differences in the run record.

## Fixture and assets

Source revision, 2026-10-27: the ten shortcut/input tiles are now registered editable Button components, with the same stable IDs, actions, placements and appearance. The packaged preset and recreation data are identical. This is a semantic data revision, not a new native or visual acceptance result; both recreation/reset pairs must be rerun. Existing saved layouts are not migrated.

The current recreation configuration is the layout from the packaged opt-in Pastel preset: one production app collection and one registered Recently viewed component. [collections.json](collections.json) supplies deterministic local movie/episode history, including completed and partial items, and the IDs of actual packaged catalog apps to restore inside the disposable acceptance profile. The acceptance-only provider feeds the normal Kodi bridge and artwork interfaces; it is not part of the application. Runtime presets use real providers or honest empty/unavailable states. Existing user layouts are never rewritten on load.

[recreation-config.json](recreation-config.json) uses the shared composed-Home model. [interaction.json](interaction.json) supplies only the existing remote keys needed to reach the editor; it is test automation, not product layout code. History artwork is original generated geometric test art. The reference is retained for comparison and is never supplied as a rendered card or Home surface. Labels, symbols, circles, panels and focus rings are rendered by the production components.

[wallpaper.png](wallpaper.png) is a clean scene produced with the built-in imagegen tool from the supplied image. It contains no interface. The exact edit prompt and provenance are in [wallpaper-generation.json](wallpaper-generation.json). The image is a local visual fixture and the opt-in Pastel sky skin wallpaper; the original packaged Home remains the default.

Fixture history is synthetic and does not establish actual installed Kodi playback or third-party history permissions. App entries retain real registry identities; the visual test does not launch or install their external programs. Movies/Shows use actual library pages; Desktop uses the existing Desktop action. HDMI/guide tiles open setup. None of these qualify captured inputs, streaming entitlement or music/game integrations.

## Repeat

Run `tools/home-reference-acceptance.py <installed-executable> --case <repo>/tests/visual/home-pastel-reference --inspect`. Follow its CUSTOM_READY/default-reset/DEFAULT_READY capture acknowledgements as described in [the first case](../home-reference/README.md). Retain actual screenshots and config, an explicit visual assessment, release/test evidence and service-restoration status. Run the first case too whenever shared card/header/layout code changes; both appearances must work from configuration in the same executable.

Hardware shader support may vary: panel tint remains usable when optional Qt Quick Effects blur is unavailable. Record whether the target renderer supports the frosted effect; do not count a software-rendered unit test as GPU blur qualification.

## Historical authored-card run

Installed release `523f627f0c06211f92b6e43b` passed all thirteen suites and the Settings-only skin selection/save/default/reapply/reset sequence. The [actual recreation](last-attempt.png), [restored default](last-default.png), [exact configuration](last-attempt-config.json) and [explicit visual review](last-review.json) are retained here. Visual recreation passes as a landscape adaptation at 1280 × 800; the wallpaper is the documented clean reconstruction and the clock shows real time. Evidence: `/tmp/isolde-reference-config-yj_y0vbc`. Saved skins survive reset. Household restored active with zero restarts. See [skin acceptance](../home-skins/README.md).

## Current semantic status (2026-10-23)

The prerequisite rebuild and its verification are recorded in [genuine collection evidence](../../../docs/studio-history-prerequisite-2026-10-23.md). Current populated graphical review remains pending until that record reports actual captures. The historical captures above do not prove current collection semantics. See also the earlier [standing evidence audit](../../../docs/studio-reference-evidence-2026-10-23.md).

## Capacity and gallery checkpoint (2026-10-26)

[Overflow evidence](../../../docs/studio-pastel-overflow-2026-10-26.md) supersedes the old missing-overflow fixture limitation: the fixture restores eleven eligible real catalog definitions, the packaged/fixture collection has nine visible slots, and actual remote More apps → gallery → Back is reviewed with exact origin and unchanged layout bytes. Full C passes 34/34 and all standing graphical journeys; all 30 graphical and 29 native frames are reviewed. Placeholder/installer artwork, the inherited heart glyph on More apps and broader reference/provider/design acceptance remain open. Registered fixture apps are never represented as installed external clients or subscription playback proof. Existing user layouts and the normal installed release remain unchanged.

Subsequent full G in that record verifies the grid overflow glyph and remote-editable More apps icon using the existing shared presentation field; the heading heart is unaffected. Full G passes 34/34 and all standing journeys with all 59 frames reviewed. This closes the inherited-heart finding only; artwork and broader acceptance remain open. No stored-layout migration or deployment is performed.
