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

Semantic limitation (user clarification, 2026-10-20): the existing app/recent cards are individually authored illustrative items. This fixture's historical visual passes do not prove App or Recently viewed carousel functionality. T21.9 must rebuild it and the packaged opt-in pastel preset with the production collections and deterministic fixture providers described in [component contracts](../../../docs/design/page-studio/component-contracts.md), testing saved structure and real collection behavior as well as appearance. Runtime presets must use real data or honest empty/unavailable states; existing user layouts must not be rewritten on load.

[recreation-config.json](recreation-config.json) uses the shared composed-Home model. [interaction.json](interaction.json) supplies only the existing remote keys needed to reach the editor; it is test automation, not product layout code. Local logo/recent-art regions are cropped from the supplied reference by the actual artwork component. All surrounding labels, icons, circles, panels and focus rings are rendered separately.

[wallpaper.png](wallpaper.png) is a clean scene produced with the built-in imagegen tool from the supplied image. It contains no interface. The exact edit prompt and provenance are in [wallpaper-generation.json](wallpaper-generation.json). The image is a local visual fixture and the opt-in Pastel sky skin wallpaper; the original packaged Home remains the default.

Fixture apps/media/history are illustrative. Brand cards without a configured provider open the real app drawer. YouTube uses its configured application route; Movies/Shows open the actual Kodi-backed library; Desktop uses the existing Desktop action. HDMI/guide tiles open setup. None of these establish streaming entitlement, captured inputs, music/game integrations or actual recent history.

## Repeat

Run `tools/home-reference-acceptance.py <installed-executable> --case <repo>/tests/visual/home-pastel-reference --inspect`. Follow its CUSTOM_READY/default-reset/DEFAULT_READY capture acknowledgements as described in [the first case](../home-reference/README.md). Retain actual screenshots and config, an explicit visual assessment, release/test evidence and service-restoration status. Run the first case too whenever shared card/header/layout code changes; both appearances must work from configuration in the same executable.

Hardware shader support may vary: panel tint remains usable when optional Qt Quick Effects blur is unavailable. Record whether the target renderer supports the frosted effect; do not count a software-rendered unit test as GPU blur qualification.

## Historical authored-card run

Installed release `523f627f0c06211f92b6e43b` passed all thirteen suites and the Settings-only skin selection/save/default/reapply/reset sequence. The [actual recreation](last-attempt.png), [restored default](last-default.png), [exact configuration](last-attempt-config.json) and [explicit visual review](last-review.json) are retained here. Visual recreation passes as a landscape adaptation at 1280 × 800; the wallpaper is the documented clean reconstruction and the clock shows real time. Evidence: `/tmp/isolde-reference-config-yj_y0vbc`. Saved skins survive reset. Household restored active with zero restarts. See [skin acceptance](../home-skins/README.md).

## Current semantic status (2026-10-23)

The current Settings-selected preset uses production collections; its isolated acceptance profile lacks populated app/history providers. Save/reapply/reset passes, but populated reference recreation remains incomplete. The historical captures above do not prove current collection semantics. See [standing evidence audit](../../../docs/studio-reference-evidence-2026-10-23.md). T21.9 owns the required real Recently viewed and deterministic-provider fixture rebuild.
