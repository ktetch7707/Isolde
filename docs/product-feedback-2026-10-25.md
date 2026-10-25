# User acceptance repairs — 2026-10-25

These twelve user-reported failures are implementation requirements, not accepted features. This explicitly requested slice extends the existing T21/shell exceptions. Work in small verified slices; keep the unfinished VPN goal and existing Studio gates open.

| # | Required behavior | Owner / acceptance |
|---|---|---|
| 1 | Home and other pages use the same canvas. Home designation selects the startup destination only. Navigation is a Page menu component the user places, moves, edits or removes; no automatic left sidebar. | T21.11/T21.13. Runtime/editor geometry and focus agree; old files load without startup writes. |
| 2 | Media shelf source picker includes existing Kodi playlists. | T21.4/T21.8. Read bounded Kodi-owned playlists through existing adapters; user selects, refreshes and plays a real item. No Isolde playlist curation or parallel media DB. |
| 3 | Settings and VPN use the shared visual theme, legible sizing, consistent controls, scrolling and remote focus. | T12/T22.2/T27. Verify actual dark/light/Pastel Settings and VPN, including long status/error and consent screens. |
| 4 | Author screensaver pages with the shared page/component system: widgets, slideshows, text, managed GIFs and hero panels. | T21.8/T22/T12. Profile-owned page selection/rotation, reduced motion, safe idle entry and exact focus restoration; no executable layouts. |
| 5 | Every owned app, including Disney+, has a universal remote return to Isolde. Esc currently fails for Disney+. | T12.3. Verify actual external focus, configured return key, graceful owned-session closure and exact originating card. Ordinary in-app navigation and emergency recovery must be clear. |
| 6 | Kodi starts automatically as the background media backend. Its launcher UI must not appear over Isolde. | T04/T12. Preserve Kodi ownership and real playback. Interpret “headless” as background service/library availability, with video presentation only when requested; verify startup and failure recovery. |
| 7 | Games is Isolde's Steam-backed library/frontend, not a Steam launcher tile as the entire page. | T04/T21. Bounded actual installed game data/art, direct Steam-owned launch, offline/unavailable states and exact return. Do not claim gameplay from client startup. |
| 8 | Movies and TV starter pages contain separate genre media shelves, not one monolithic library component. | T21/T22. Declarative starter data from bounded actual Kodi genres; independently editable shelves. Do not rewrite existing authored layouts on load. |
| 9 | More apps includes a clear App Store button. | T21.6/T26. Typed existing Store action, remote focus and exact gallery/card restoration. |
| 10 | Sharp app artwork at displayed size; stop enlarging tiny favicons. | T21.6/T27. Dimension-aware bounded source selection, original/vector/high-resolution assets where supported, no repeated startup downloads. Inspect actual TV-distance result; acquisition success alone is not a quality pass. |
| 11 | Screensaver appears above More apps and every ordinary shell overlay. | T12/T26. Enter idle with gallery open; dismiss once and restore exact overlay selection. No hidden/background screensaver or click-through. |
| 12 | ISOLDE title/logo, Settings and Search are selectable editable components. | T21.13. Shared schema/catalog/renderer/editor, full presentation/actions, move/delete/undo/Apply and export. Permanent recovery remains reachable without forcing visible header furniture. |

Initial diagnosis: `HomePages.cpp` creates most pages with `pageNavigation: left`; `PageSurface.qml` and `PageStudioView.qml` reserve navigation space and render fixed `PageNavigation`. A Page menu component already exists. `AmbientScreen` is inside the low shell layer while More apps is a Qt Quick Controls popup in `Overlay.overlay` (and CommandCenter has root z=1100). Raising an ordinary child inside the shell cannot cover that popup. These are findings, not verification passes.

The VPN slice remains unfinished: both clients and approved profile-only grants are installed; copied Surfshark tunnels handshake but have not passed traffic/country checks. The native app reports WireGuard quantum-safe and Kill Switch Off. Do not mark saved Canada/US or CBC acceptance complete. Full AQ passes 30/30; its review was interrupted by this new feedback. Normal profiles/layouts are preserved.

No checkbox or percentage credit: T12 0%, T20 80%, T21 36%; 24 active task percentages sum to 866, Overall round(866/24) = 36%.

## First repair: automatic sidebars

New page presets use no fixed navigation. Runtime and Studio no longer instantiate the fixed page-navigation renderer or reserve a sidebar/top strip. The existing Page menu component remains available for explicit placement and editing. Old navigation fields still validate for compatible reads; loading does not rewrite layouts. Obsolete fixed-navigation controls are removed from the appearance editor. Editable header conversion (#12) remains open.

`build/vm-oracle/feedback-20261025-a/report/ctest.log`: **30/30 pass**, 253-second CTest stage. The viewport regression asserts the full canvas at 1280×720, 1920×1080 and 3840×2160 for old left/top/none values and exact runtime/editor equality. Deployed release `4d7f9fb7313b9d38ec67e8ab`.

All three installed functional graphical journeys pass. All **28** captures under `build/vm-oracle/feedback-20261025-a/visual/` are reviewed; both recreation/reset pairs are compared with their references. The Music, Games and TV Guide frames show the named-page full-width canvas. Lounge composition is preserved; the pre-existing Pastel artwork/content/proportion and broader design gaps remain open. All **20** native captures are identical to the reviewed AD set. Guest evidence: `/tmp/isolde-components-acceptance-cfaedxs2`, `/tmp/isolde-reference-config-neqnit1l`, `/tmp/isolde-reference-config-qtbqkv8m`. Household/Studio Demo layouts remain byte-identical and the service restores active with zero automatic restarts. This is a narrow verified sidebar repair, not completion of the twelve requirements or T21.
