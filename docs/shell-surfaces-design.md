# Home-theme UX revision — 2026-10-19

[Page-first Studio](design/page-studio/page-first-design.md) is the current theme contract: Settings, its children, dialogs and keyboard always inherit the designated Home page's semantic theme, regardless of entry origin. Other authored pages inherit Home unless explicitly overridden. Draft Home/theme changes cannot affect live Settings before atomic Apply. Any originating-page precedence below is superseded for Settings. T18 establishes designation and its consequences; T22 establishes inheritance; T27 completes all shell surfaces. Minimum Settings inheritance is a T24 cutover gate, not work deferred until after cutover. Native Kodi remains untouched.

# Configurable shell surfaces — sources, All Apps, themes and Search

Status: additive design specification, 2026-10-19. User observations require four corrections, implemented later as T25-T28. This revision changes plans only. It extends [Page Studio](page-studio-redesign.md), including its draft/apply, declarative layout, focus, compatibility and evidence rules. Priority revision (user): these capabilities follow the complete core Studio T15-T24 package; they must not delay its templates, Preview/Apply or core cutover.

## 1. Source findings and intended behavior

| Observation | Focused source evidence | Design target |
| --- | --- | --- |
| Detected-source notices lack containment/presentation controls | `qml/Main.qml` prepends `sourceManager.sections` to Home sections and merges its items; `SourceManager`/`SourceSettingsView` already have automatic-source and hide/keep controls | Keep existing discovery capabilities, but route presentation through a configurable notice slot or contained component with per-page rules |
| All Apps opens Search | `ActionDispatcher` emits `drawerRequested` for `app-drawer`; Main opens Command Center | All Apps opens a dedicated installed-app gallery; Search remains a different destination |
| Search/Settings/Kodi colors do not consistently match Home | Command Center has fixed palette values and no shared appearance input; Settings and Kodi views have some appearance bindings but nested surfaces still vary | One theme-resolution contract for every Isolde-owned surface, child control, dialog and keyboard |
| Web toggle looks like a result | `CommandCenter.resultItems()` inserts `kind: searchToggle` and handles it as a selectable result | A genuine labeled switch in the header, default top right, outside result data and counts |

These are code-inspection findings, not a new installed acceptance run. In particular, source policy controls and some theme bindings exist; their incomplete presentation is not evidence that the entire capability is absent.

## 2. Detected sources: separate discovery, policy and presentation (T25)

Discovery reports actual local source identities/availability through SourceManager. It does not decide where to draw UI or grant media access. A profile-owned notification policy decides which events may be shown. Page composition decides the containing slot/component and its appearance. Hiding a notice never unmounts a drive, deletes media or changes Kodi sources.

Default experience: a compact, nonmodal **New source available · 2** chip in the Home preset's reserved status region. It never expands automatically into multiple full-width rows and never steals focus. Confirm opens a contained source list only after the user selects the chip; Menu exposes source actions. The default is limited to Home, with notifications suppressed during playback, setup and editing. The Sources settings page remains reachable when presentation is off or no page allows notices.

Settings → Sources separates:

- **Discovery:** enabled, permitted local discovery roots/types, refresh and current availability. Existing profiles keep their saved choices; do not rescan broader folders or reset hidden/permanent decisions during migration.
- **Display policy:** Off, compact notice, or contained sources widget; allowed pages, allowed named slots/containers, event classes, show-once/reconnect behavior, duration/persistent-until-dismissed and suppressed workflow states. These choices are validated data; no executable path/filter expressions.
- **Presentation:** inherit active theme or explicit overrides; compact/list/card variant, maximum visible items (1-6), source icon and label/detail visibility, art/crop, badge, text scale, surface/focus colors, background opacity, corners, padding/gaps and bounded region height. Duration presets include persistent and 5-60 seconds, default 8 seconds for the compact chip. All controls are remote-complete.
- **Source decisions:** Open, Keep as a normal tile, Dismiss until disconnect, Hide until restored, Choose handler, Show hidden/Restore. Confirm/Menu/Back cover these actions; K/Delete shortcuts are optional accelerators.

Studio exposes a **Detected sources** component and a **Source notice slot**. The component occupies cells or a bounded section like any other widget; a notice slot reserves a named status/header/sidebar region. Page appearance/density choices and policy scope are previewable. A notice can never be injected above the composition as an unmeasured extra section, spill out of its parent, cover neighboring controls, expand the grid or create an unbounded scroll region. Overflow is contained with a visible count and View all action. Invalid destinations/sizes are refused rather than silently moved or clamped.

Sources is the reliable destination for the complete current list. A missing notice slot, empty page, hidden source widget or full grid does not strand source settings: keep the configured page unchanged and make the source available in Sources. Explain a missing slot in Studio/Settings instead of falling back to a floating overlay. Where a user opts into a floating presentation, it still requires a declared bounded notification region that respects safe area; it is never a global free-positioned overlay.

Dedupe repeat scans by source/event identity. A reconnect follows the chosen policy; a periodic refresh is not a new notification. If a focused source disappears, mark the action unavailable and restore focus to the next available item or containing chip, then to the originating page if the list becomes empty. Keeping a source creates a draft/permanent tile only through the existing validated action/grant flow and explicit confirmation; no automatic launch, media permission, file move or Kodi write. Promotion saves and validates the permanent tile before consuming its notice; failures retain a retryable source, and retries must not create duplicate tiles.

Profile display choices, hidden decisions and handlers must not leak into another profile. Discovery may enumerate device facts, but only the active profile's permitted results/actions are projected. Preserve stored files on read; migration/default changes require an explicit save. Test fixtures must be outside real discovery roots or use injected temporary roots/isolated discovery configuration. Do not hardcode acceptance-folder names or hide real user folders to make screenshots pass. Existing acceptance folders must not be silently removed.

## 3. All Apps is an app gallery, distinct from Search (T26)

**All Apps** and **More Apps** open the same named shell destination. It shows installed/registered launchable apps, deduplicated by stable app identity, without requiring a query and without opening a keyboard. Its normal initial focus is the first app, or the previously selected app for that same navigation history. The empty state offers Store and Settings with a clear explanation.

```text
+---------------------------------------------------------------------+
| All Apps                                    Filter apps   Settings   |
| Installed · 12                              Sort: Name               |
|                                                                     |
| [ YouTube ]  [ Spotify ]  [ Kodi ]  [ Steam ]                          |
| [ Plex    ]  [ ...     ]  [ ...  ]  [ ...   ]                          |
|                                                                     |
| App name · Ready / Needs setup / Unavailable offline                  |
| OK open     Menu app actions     Back return                         |
+---------------------------------------------------------------------+
```

Names and counts above are wireframe examples, not packaged app/entitlement claims. Contents come from AppRegistry/AppCatalog and the profile's actual registrations/state. Include installed Ready, Needs setup and Failed entries with truthful state-specific actions. Available-but-uninstalled packages belong in Store, not in an installed-app count. Internal Search/Settings/navigation actions are navigation controls, not fake installed apps. Do not infer entitlement from an installed streaming launcher or fetch online recommendations when opening the gallery.

Default order is name; offer profile-defined order, favorites/filter and density/presentation choices. A local **Filter apps** field filters this gallery only, preserving its selected stable ID and scroll. It makes no web/media discovery requests and does not change the global Search Web preference. A separate Search action may intentionally open universal Search. Global Search can still find apps; the two destinations must not share an ambiguous default-open function/state.

Use the existing page/composition/component machinery and add a registered app-catalog data adapter. The gallery is a built-in named route with a packaged page preset, not a second persisted app library. Its layout, grid/list mode, labels/art, typography, spacing, background and focus style are configurable through Studio and inherit the theme contract below. Persist authored surface overrides within the existing profile document/bounds; do not add a parallel layout file or bypass the 64-page/4 MiB limits. A missing/invalid override falls back to the packaged gallery without rewriting user data. The built-in route stays reachable even when no user-authored page or All Apps tile exists.

Maintain legacy `app-drawer` compatibility by resolving it to All Apps at execution time. Do not rewrite saved layouts/skins on startup. New action pickers explicitly distinguish **Open All Apps** from **Open Search**. Apply the distinction to Home cards, favorites, headers, page buttons, shortcuts and context actions; update help text and test expectations. Missing custom destination bindings fall back to the gallery, never Search.

Opening an app uses ActionDispatcher/SessionManager with an All Apps origin containing stable app/page identity. Back from the app restores gallery selection/scroll; Back from the gallery restores the exact originating Home/page control. Failed launch shows Retry/Details and retains selection. App install/uninstall/state refresh updates the registry projection without rebuilding unrelated media/search clients. If the selected app disappears, choose its nearest surviving neighbor with visible focus.

## 4. Theme inheritance across Isolde surfaces (T27)

The user's Home/page design is the theme source, not an isolated decoration behind unrelated default-colored screens. Define one effective appearance context and pass it across routes, overlays and nested controls. Do not depend on whichever QObject happened to instantiate a page.

Resolution, from least to most specific:

1. Packaged readable semantic tokens, defined in one shared model.
2. Active profile interface theme. Existing Home appearance values provide the compatibility baseline; opening a surface must not migrate/rewrite the profile.
3. Originating page overrides, inherited by transient Search/Settings/Kodi browsing opened from that page unless an explicit surface policy says to use the profile theme.
4. Explicit destination/surface or component override chosen by the user and allowed by the shared appearance schema.

The context carries profile ID, origin page ID and effective tokens. Deep links without a valid origin use the active profile theme; no stale previous-profile tokens. Search opened from a pastel page stays pastel while searching/navigating details; returning restores that page and origin. A destination with its own explicit theme uses that theme. Settings opened from Home inherits Home; from a custom page it inherits that page by default. Studio offers **Inherit page**, **Use profile theme**, and **Custom override** for supported system surfaces, with reset-to-inherited and live draft preview.

Semantic roles cover background, surface/elevated surface, primary/secondary text, outline, selected surface, focus, accent, input field, scrim, success/warning/error and unavailable states, plus typography, spacing, radius, opacity and motion. Derive readable state tokens from the shared palette where no explicit token exists. Do not recolor app artwork, media images or semantic error states into illegibility. Missing/invalid overrides fall back per token; do not reset an entire surface to the old dark palette.

Scope includes Search query/results/header/toggle, All Apps, Settings navigation/content/subpages/dialogs/keyboards, and Isolde-owned Kodi movie/show/season/episode/details/search/Media settings/import/folder-access/error/loading views. Inventory child components; a themed parent rectangle alone is insufficient. Validate text contrast, focus visibility, long labels, opacity and high-contrast/reduced-motion behavior with both dark and pastel designs plus an arbitrary custom palette. No forced animation, dark panel, fixed accent or fixed font outside shared defaults.

Native Kodi's own frontend/skin remains Kodi-owned. This task themes Isolde's Kodi-backed pages and handoff/return surfaces; it must not edit Kodi skin settings, databases, artwork caches or add-ons to simulate a match. Preview theme changes only in the Studio draft context; runtime continues using the applied profile design until Apply. Theme editing must not reset navigation selection, refresh media queries or discard a Search query. Profile teardown invalidates the old appearance context and clients together.

## 5. Web search: a real header toggle (T28)

Default search header:

```text
+---------------------------------------------------------------------+
| Search                                          Web search [ OFF o ] |
| [ Search your library... ]                                           |
| Local results only                                                  |
+---------------------------------------------------------------------+
| Results: actual matching titles/apps/settings only                   |
| ...                                                                 |
+---------------------------------------------------------------------+
```

The label, thumb, track and checked state form one genuine switch control, outside the result model, result count and result keyboard/paging index. Default placement is the header's top-right/trailing slot inside safe area. Studio may reposition it within validated header controls, but it must remain visible/reachable and cannot become a search-result row. Styling uses the effective theme and exposes size, spacing, labels, radius and focus choices through shared presentation data. The control must read clearly as a switch in both pastel and dark themes; On/Off text and accessible checked state carry meaning without relying on color or thumb animation.

The existing SearchSettings preference remains the single state authority. Default OFF; remember explicit user changes per active profile. Confirm/click toggles exactly once and persists atomically. Failure keeps the previous stored/visible state and reports an error without losing the query or focus. Arrows navigate controls; they do not silently change the preference. Do not create an independent QML-only checked flag, a second search preference or a toggle result alias.

Remote focus contract: opening Search focuses its query field/search entry control. Right in the header reaches the switch when not editing text; Left returns to the query. Down from the switch reaches the selected/first result, or the empty-state action when no results exist. Up from the top result returns to the last focused header control. While the on-screen keyboard/text editor is active it owns arrows; Back closes it before leaving Search. Confirm on the switch never selects a result; a late result update cannot move focus away from it. Closing Search restores the originating page/component. Every path works without a mouse or physical Tab key.

Switching OFF cancels/invalidate outstanding external search requests, removes external/cached online matches and ignores late callbacks using the current request generation. Local results/query/selected identity survive where possible. OFF sends zero external discovery/metadata/provider requests. App-gallery filtering stays local regardless of this setting.

Switching ON is opt-in permission for configured discovery, not proof of a working provider, entitlement or connectivity. While the existing provider path is unavailable, retain the genuine ON state but show **No web provider configured — local results only** as header/status text, not a fake result. If offline, report **Web unavailable offline — local search still works**. The preference does not itself launch a network job without a valid query/configured adapter. No provider implementation, new endpoint or Kodi networking-policy change is authorized by this presentation task; T14 retains online-provider selection/terms and discovery implementation scope.

## 6. Combined acceptance and integration

| Journey | Gate |
| --- | --- |
| Discover multiple sources on the pastel Home | Chip/list stays inside its assigned region, inherits theme, never overlaps or rearranges existing components, no unsolicited focus change |
| Disable notices, restrict to a page, hide then reconnect/unplug a source | Policy honored and persisted per profile; source center stays reachable; no lost focus, auto-launch or media/grant changes |
| Relocate the notice component in Studio | Remote/pointer/inspector produce the same validated placement/presentation; Preview/Cancel leaves applied policy/layout unchanged |
| More Apps from Home, a saved skin and a custom page | Each opens app gallery rather than Command Center; no query required, no provider requests or uninstalled-package rows |
| Launch, failure/retry, Back, uninstall selected app | Real dispatch and stable focus/scroll restoration; surviving neighbor chosen only when the origin app is gone |
| Open Search, Settings and all Isolde Kodi subviews under dark/pastel/custom themes | Semantic token assertions and real captures show consistent chrome/content/controls, including errors/dialogs/keyboard; native Kodi settings hashes unchanged |
| Navigate query → toggle → results → header using remote | Distinct visible switch with accessible checked state; result count/index never includes it; no accidental toggle or result launch |
| Toggle OFF during external request; persist failure; profile switch | Zero late/cached external matches, generation cancellation and network denial; atomic state rollback, separate profile state and restored focus |

Extend existing component/QML/Kodi/input suites and relevant SourceManager/app-registry tests. Add specific assertions that `app-drawer` does not invoke Search and that Search results contain no toggle row. Use injected source probes and disposable profiles; no personal media mutation or hardcoded fixture exclusions. Capture the exact installed source/release, both standing fixture recreation/reset pairs, all affected themed surfaces, remote paths and final normal-service restoration via [vm-oracle.md](vm-oracle.md).

Execution order is now core Studio T15-T24 first, then T27 → T25 → T26 → T28. T22 depends only on T21 and delivers the shared profile/page token editor/templates; T23/T24 finish Preview/Apply and core acceptance. T27 then extends that same appearance model to system surfaces, reusing the token editor rather than introducing a competing resolver. T25/T26 consume T27; T28 depends on T25/T26/T27 and verifies the combined shell-surface matrix without regressing the accepted Studio. Related control locations/policy remain in the design inventory so later additions fit the same workspace, but are not blocking implementation gates for T24. None of these four tasks is implemented by writing this specification.
## 7. Original planning verification (before priority revision)

Documentation-only scope, relative Markdown links, checkbox percentages and dependency ordering were checked. The graph is acyclic and the selected track respects all dependencies. T25-T28 each remain 0% with explicit entry files/docs, scope, completion and verification/evidence gates. At that revision: 24 active tasks, percentage sum 275, rounded overall 11%; previous checked implementation credit is unchanged. Check log: `C:/Users/kketc/.codex/visualizations/2026/10/19/01a15691-1cd3-77d0-928f-854e1c7a566b/shell-surfaces-planning-20261019.log`. No application tests, VM operations or implementation changes were performed for this revision. These checks establish plan consistency, not delivered behavior.
