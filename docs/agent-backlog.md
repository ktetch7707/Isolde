# Isolde Agent Backlog â€” single source of truth
Location: `docs/agent-backlog.md` (this file). Entrypoint: `AGENTS.md` at repo root points here.
Sequencing authority: `docs/implementation-order.md`. Requirements: `docs/unified-master-plan.txt`, `docs/studio-builder.md`, `docs/master-roadmap.md`.

## How percent completion works (agent must follow)
- Active tasks are T01,T03-T05,T09-T28 (24 tasks). T02 is out of scope; T06-T08 are superseded by the Page Studio tasks and excluded from math. Each active task uses equally weighted checkboxes.
- Task % = round(checked / total * 100). Update `| % |` cell when you check a box.
- Overall % = round(average of the 24 ACTIVE task %s). Recompute on every edit.
- `100% x 24 = active backlog implemented` ONLY if every active box is `[x]` AND `Evidence` has log/screenshot paths. No evidence = max 90%.
- Workflow: use the priority-ranked table below, not numerical task ID. Select its first unfinished ACTIVE task whose Depends tasks are 100%; do not skip a blocker silently. Read only its Entry docs/files, implement its Scope, run Verify, then update checked evidence and percentages.
- Standing gates for every task: `tests/visual/home-reference/` + `tests/visual/home-pastel-reference/` recreation+reset pass; profile isolation; atomic save+rollback; Kodi owns media/progress/playback (no parallel DB/player); offline Generator Test; Settings/Edit Home always reachable.
- Scope decision (user, 2026-10-19): collection curation is the USER's responsibility via Kodi. Isolde does not build episode/tag/playlist/exclusion/template curation. T02 struck. Existing movie/show manual-collection code stays as-is (no removal, no extension).

Audit/deployment evidence is in [planning-audit.md](planning-audit.md) and [vm-update-2026-10-19.md](vm-update-2026-10-19.md). The user-selected Page Studio redesign now has priority; the existing editor code is committed in `25f56bf`, not an accepted replacement design. Read [vm-oracle.md](vm-oracle.md) before VM testing.

## Execution priority — Page Studio first (user, 2026-10-19)

Page Studio is the highest-priority feature package. Complete **T15 → T16 → T17 → T18 → T19 → T20 → T21 → T22 → T23 → T24** first: design, safe drafts/history, grid/canvas, workspace, remote editing, desktop/nested editing, inspector, templates, Preview/Apply, and installed usability/cutover. **T16 is in progress; T15 user review remains deferred to T24 under the user-authorized provisional-design exception.** Existing current-source build/regression gates remain mandatory during each implementation slice; this ordering does not waive safety tests or hardware limits.

Next complete the related shell improvements: **T27 → T25 → T26 → T28** (shared theme, contained source notices, All Apps gallery, genuine Web switch). These extend the accepted Studio and no longer block its templates, Preview/Apply or core cutover. Then follow the remaining ranked rows for evidence repair, media preservation, input/system polish, playback, hardware/guide, appliance distribution and optional online work.

The table AND task sections are physically in execution order. Task IDs remain stable for commit/evidence references; their numeric values are not priority. Struck/superseded rows are archived at the end. T17 supplies T05's remaining geometry evidence; when reviewing that later ledger task, reuse valid current-source evidence rather than repeating implementation. A blocker must be reported/resolved, not used to silently switch to a lower-priority feature. The planning-only restriction applied to the priority-edit request. The subsequent user request explicitly authorizes application implementation and VM deployment using the design provisionally.

The first Studio expansion changed 13 active tasks to 20 (21% to 14%). This additive revision adds four zero-percent tasks, giving 24 active tasks and **11% overall** (275/24 rounded). Existing implementation credit is unchanged. Planning/specification work is not delivery of the new system.

## Overall (update every edit)
| Task | Priority | Title | % | Status |
|------|----------|-------|---|--------|
| T15 | 1 | Page Studio interaction design and usability validation | 75% | provisional design only; T15.4 user acceptance OPEN, deferred to T24 |
| T16 | 2 | Shared draft transactions, history and profile safety | 0% | planned |
| T17 | 3 | Shared canvas geometry, grids and legacy compatibility | 0% | planned |
| T18 | 4 | Unified Page Studio workspace and selection | 0% | planned |
| T19 | 5 | Remote pick-up/drop, resize and collision workflows | 0% | planned |
| T20 | 6 | Desktop parity, layers and nested/group editing | 0% | planned |
| T21 | 7 | Contextual inspector, catalog and capability binding | 0% | planned |
| T22 | 8 | Templates, tokens, reusable sections and portability | 0% | planned |
| T23 | 9 | Preview, Apply, restore and navigation safety | 0% | planned |
| T24 | 10 | Installed acceptance, usability and editor cutover | 0% | planned |
| T27 | 11 | Shared theme inheritance across shell surfaces | 0% | planned |
| T25 | 12 | Detected-source policy, styling and containment | 0% | planned |
| T26 | 13 | Dedicated configurable All Apps destination | 0% | planned |
| T28 | 14 | Genuine top-right Web search toggle | 0% | planned |
| T01 | 15 | Bootstrap + gate check | 67% | verification incomplete |
| T05 | 16 | Studio grid renderer consumes placements | 67% | verification incomplete |
| T03 | 17 | Local search: Kodi titles via bridge (no curation adapters) | 75% | installed acceptance pending |
| T04 | 18 | Kodi preservation + desktop import | 33% | in progress |
| T12 | 19 | System polish + input + on-screen keyboard | 0% | not started |
| T09 | 20 | Playback preference + viewing options | 0% | not started |
| T10 | 21 | Capture + HDMI + audio editors (gated) | 33% | in progress |
| T11 | 22 | Guide / PseudoTV | 0% | not started |
| T13 | 23 | Appliance + release distribution | 0% | not started |
| T14 | 24 | Optional online discovery + linguist subtitles | 0% | not started |
| T02 | — | ~~Finish curation~~ â€” OUT OF SCOPE (user curates via Kodi) | n/a | struck |
| T06 | — | Remote + desktop grid editor → T17-T21/T24 | n/a | superseded |
| T07 | — | Preview/Apply/Undo → T16/T23/T24 | n/a | superseded |
| T08 | — | Templates/tokens/portability → T22-T24 | n/a | superseded |
| **Overall** | — | **average of 24 active tasks** | **15%** | T15 provisional (75%); T16 in progress |

## Highest priority: core Page Studio

## T15 — Page Studio interaction design and usability validation (M, depends: none; selected priority)
Entry docs: `docs/page-studio-redesign.md` sections 1-4, 10-11; `docs/shell-surfaces-design.md` sections 1-6; `docs/interface-design.md` Page Studio amendment.
Entry files: `qml/CustomizeHomeView.qml`, `qml/PageManager.qml` (read-only workflow inventory); new design artifacts under `docs/design/page-studio/`.
Scope:
- [x] T15.1 Inventory every currently supported editor operation plus source notices, All Apps, themed shell surfaces and the Search header switch; map each to Studio and identify capability gaps without treating UI labels as backend proof.
- [x] T15.2 Produce reviewed TV and desktop wireframes for blank/existing pages, Add, selection, inspector, pick-up/drop, resize, collision, dirty exit, preview, apply error and nested scope.
- [x] T15.3 Walk through the complete D-pad/Confirm/Back/Menu state model, including field editing, focus restoration, error feedback and accessible movement alternatives.
- [ ] T15.4 OPEN — genuine user/representative review; resolve confusing transitions and establish measurable acceptance journeys. Designer-only artifact walkthrough done (W1-W8 resolved in artifacts). Sequencing exception (user, 2026-10-19): proceed with this pack as provisional basis for T16-T24; genuine user usability acceptance deferred to T24. No review evidence invented.
Done when: T15.1-T15.3 artifacts agree with the specifications; T15.4 user walkthrough deferred to T24 per sequencing exception; remote movement is understandable without a pointer, number keys or hidden gesture.
Verify: design state-transition/feature-coverage review and recorded uncoached walkthrough; no application build is claimed by static mockups. Evidence: design pack under `docs/design/page-studio/` — `feature-map.md` (T15.1; every Customize Home action + PageManager operation + the four shell surfaces mapped with backend-path citations, labels not treated as proof), `wireframes.md` (T15.2; 12 TV+desktop screen states), `interaction-states.md` (T15.3; full D-pad/Confirm/Back/Menu table, focus restoration, error feedback, accessible movement alternatives), `review-and-acceptance.md` (T15.4 remains OPEN; designer-only walkthrough with 8 findings + 10 proposed acceptance journeys); static coverage gate `tools/verify-t15-design.py` 56/56 PASS, log in `docs/design/page-studio/verify-t15.log`. Uncoached installed walkthrough with the actual user remains T24's cutover gate per spec §10.

## T16 — Shared draft transactions, history and profile safety (L, depends: T15)
Entry docs: `docs/page-studio-redesign.md` sections 3, 5, 7; `docs/components.md` persistence contracts.
Entry files: `src/home/HomeLayout.{h,cpp}`, `src/home/PageModel.{h,cpp}`, `src/home/HomePages.cpp`, `src/home/ComponentSchema.{h,cpp}`, `tests/tst_components.cpp`.
Scope:
- [ ] T16.1 Add one profile-scoped in-process edit session over the complete interface document, with baseline revision and live draft view; opening/cancel/undo do not write the applied document or profile assets.
- [ ] T16.2 Introduce shared typed semantic edit commands and complete-result validation; multi-item/group/collision operations accept all affected changes or none, with one undo record.
- [ ] T16.3 Implement deterministic canonical serialization and centrally allocated identities; remote/pointer/inspector replays produce identical layout bytes from identical fixture identities.
- [ ] T16.4 Implement bounded undo/redo, explicit history-budget handling, revision conflicts, dirty-exit semantics and profile teardown/client invalidation.
- [ ] T16.5 Prove failed draft operations and atomic apply failures preserve original bytes, applied model, recoverable draft and focus origin; keep v1 backup and no-write-on-read behavior.
Done when: a minimal offscreen command-to-draft-rendering journey demonstrates move/property edit/undo/redo/cancel/error without persistence leakage; the new shell can consume the session without calling live setters.
Verify: extend `component_contracts` for multi-node rollback, canonical parity, bounds, revision conflict and cross-profile cases; run `ctest --test-dir build --output-on-failure`. Evidence: pending; existing single-edit rollback tests do not satisfy draft transactions.

## T17 — Shared canvas geometry, grids and legacy compatibility (L, depends: T16)
Entry docs: `docs/page-studio-redesign.md` sections 2, 5, 9; `docs/home-composition.md` Universal grid.
Entry files: `qml/ComposedHome.qml`, `qml/HomeScreen.qml`, `qml/PageSurface.qml`, `src/home/ComponentSchema.{h,cpp}`, `src/home/HomeLayout.{h,cpp}`, `src/home/PageModel.{h,cpp}`, `tests/tst_home_navigation.cpp`.
Scope:
- [ ] T17.1 Establish shared measured geometry for runtime rendering, canvas, hit-testing and spatial focus; include safe area, reserved header, cell spans, real empty cells and out-of-bounds refusal.
- [ ] T17.2 Render the same draft at fitted/zoomed/panned editor views and actual device aspects; camera/panel changes never change layout data or reflow the page.
- [ ] T17.3 Add validated one-screen root and scrolling section-grid modes plus bounded nested/internal grids; no implicit row growth or placement overwrite.
- [ ] T17.4 Open legacy flow/no-grid pages unchanged through the canvas contract; explicit conversion previews differences and preserves IDs/routes/artwork, with no startup/read/cancel migration write.
- [ ] T17.5 Add placement-specific geometry/focus tests at 720p, 1080p, 4K and the legacy 1280x800 fixture viewport; rerun and review both real fixture recreation/reset pairs.
Done when: canvas-to-runtime geometry agrees at cell boundaries and scaling extremes; legacy fixtures remain generic; T05's remaining placement gate has concrete evidence and is updated separately.
Verify: extend `universalGridValidatesAndPersists` and `home_component_navigation` with actual placed widgets, holes/header offsets/scroll containers; full CTest and both VM visual fixtures. Evidence: pending; historical no-grid fixture screenshots alone do not close this task.

## T18 — Unified Page Studio workspace and selection (M, depends: T17)
Entry docs: `docs/page-studio-redesign.md` sections 2-4, 6; T15 approved wireframes and feature map.
Entry files: `qml/CustomizeHomeView.qml`, `qml/PageManager.qml`, `qml/PageSurface.qml`, `qml/Main.qml`, `src/home/HomePages.cpp`, `tests/tst_home_navigation.cpp`; new native Studio view files only as needed.
Scope:
- [ ] T18.1 Add the canvas-first shell: named profile/page/scope, draft indicator, contextual toolbar, Add, Pages/Layers, Undo/Redo, Fit/Grid, Preview and Apply; all bind to the shared draft session.
- [ ] T18.2 Implement TV one-panel-at-a-time drawers and wide-screen rails, deterministic selection/hit-testing, named outlines, breadcrumbs and exact focus restoration.
- [ ] T18.3 Implement page create/duplicate/name/reorder/Home/route choices in the draft, with thumbnails, searchable names, incoming-link removal/remap preview and page limits.
- [ ] T18.4 Make blank, empty, hidden, missing-source and unreadable-draft states recoverable using remote commands and independently readable editor chrome.
Done when: remote-only entry -> page selection/create -> component selection -> panel edit -> Back returns to the exact canvas origin; no live saves or accidental playback.
Verify: offscreen workspace/focus/empty-state tests in `home_component_navigation`, page draft/isolation tests in `component_contracts`, and installed screenshot/key journey on the VM. Evidence: pending; developer flag only until T24 cutover.

## T19 — Remote pick-up/drop, resize and collision workflows (L, depends: T18)
Entry docs: `docs/page-studio-redesign.md` sections 4-5; T15 movement/collision wireframes.
Entry files: `src/input/InputManager.{h,cpp}`, `src/home/ComponentSchema.{h,cpp}`, shared edit session from T16, shared canvas from T17, Studio views from T18, `tests/tst_home_navigation.cpp`.
Scope:
- [ ] T19.1 Implement Confirm pick-up, one-cell D-pad ghost movement, valid drop and Back cancel; show original rectangle, row/column/span, invalid reason and accessible non-color feedback.
- [ ] T19.2 Implement anchored remote resize, numeric row/column/span entry with on-screen keyboard and Move before/after/to-cell alternatives; reject overflow without clamping.
- [ ] T19.3 Implement explicit Shift/Swap/Cancel proposal dialogs, full affected-item preview, deterministic bounded cascade solving, disabled impossible choices and one atomic draft transaction/undo record.
- [ ] T19.4 Support insertion, duplicate, hide, lock, removal consequences, edge auto-scroll, empty layouts and movement between named eligible container scopes; prevent cycles or hidden overwrites.
- [ ] T19.5 Verify key repeat, cancellation during every state, full-grid failure, multi-collider failure, induced write failure and restored focus; no dependency on mouse, long press or physical number keys.
Done when: remote-only add/move/resize/collision/duplicate/remove journey is complete and intelligible; accepted drops change only the draft and cancellation has zero document/history effects.
Verify: extend `home_component_navigation` with full remote state sequences and `component_contracts` with transaction/boundary failures; full CTest, both fixture pairs and installed remote evidence. Evidence: pending; current basic resize test is baseline coverage only.

## T20 — Desktop parity, layers and nested/group editing (L, depends: T19)
Entry docs: `docs/page-studio-redesign.md` sections 2, 4-7; T15 desktop and scope wireframes.
Entry files: Studio/shared canvas/edit-session files from T16-T19, `src/home/ComponentSchema.{h,cpp}`, `src/home/PageModel.{h,cpp}`, `tests/tst_components.cpp`, `tests/tst_home_navigation.cpp`.
Scope:
- [ ] T20.1 Implement pointer drag/drop/resize/snap indicators and inspector position entry through the same proposal/transaction path; pan/zoom/fit never alter page content.
- [ ] T20.2 Implement synchronized Layers with expand/collapse, reorder/reparent, parent breadcrumbs, hidden reveal, locks and separately inspectable focus/reading order.
- [ ] T20.3 Implement remote and pointer multi-selection/group actions, nested-container editing and widget-local sub-grids, including an honest unavailable eight-input 4x2 presentation; bound depth/selection and refuse invalid parents.
- [ ] T20.4 Prove remote, mouse and numeric inspector operations produce identical serialized layouts and identical collision/error/undo outcomes; keyboard shortcuts are optional accelerators.
Done when: the same nested/group arrangement can be authored entirely by remote or desktop with byte-identical output; no desktop-only operation or small-handle requirement blocks remote parity.
Verify: parity replay fixtures in `component_contracts`, pointer plus remote focus tests in `home_component_navigation`, installed TV/desktop captures and both standing fixtures. Evidence: pending; no physical HDMI/audio claim.

## T21 — Contextual inspector, catalog and capability binding (L, depends: T20)
Entry docs: `docs/page-studio-redesign.md` sections 6-8; T15 operation-to-control coverage map; `docs/components.md` action/source contracts.
Entry files: `qml/AppearanceEditor.qml`, `qml/HomeDesignEditor.qml`, `qml/ComponentLayoutEditor.qml`, `src/home/ComponentSchema.{h,cpp}`, `src/home/ActionDispatcher.{h,cpp}`, `src/home/PageData.{h,cpp}`, `src/home/PageModel.{h,cpp}`, shared Studio/edit-session files.
Scope:
- [ ] T21.1 Implement searchable visual catalog backed by registered component schema/renderer/editor contracts, useful variants and clear capability/unavailable states; add-to-cell preview is draft-only.
- [ ] T21.2 Implement Layout/Content/Appearance/Actions/Accessibility inspector groups, basic/advanced disclosure, inherited versus overridden values, reset override and multi-selection mixed states.
- [ ] T21.3 Provide remote artwork browsing/import/crop/focal controls, text entry, density/labels/opacity/corners/focus/spacing and valid named source/action/page pickers; no raw IDs or required file chooser.
- [ ] T21.4 Bind real bounded Kodi data through existing adapters, freeze preview data during gestures, avoid appearance-driven refetches and keep clearly labeled sample/unavailable states separate from real progress/playback.
- [ ] T21.5 Cover every supported legacy edit in the T15 feature map; reject unknown versions/types/actions/unsafe assets and provide repair links without adding T02 curation or claiming new device backends.
Done when: selecting a component exposes relevant understandable controls and a live visible draft result, with complete remote error/retry/focus paths and no live profile writes.
Verify: schema rejection and inspector coverage/parity tests, `kodi_media_client` request-count regressions, `home_component_navigation`, full CTest and both VM fixtures. Evidence: pending; T10 still owns qualified hardware editors.

## T22 — Templates, tokens, reusable sections and portability (L, depends: T21)
Entry docs: `docs/page-studio-redesign.md` sections 6-9; `docs/skins.md` existing snapshot/asset bounds.
Entry files: `src/home/HomeSkins.cpp`, `src/home/HomePages.cpp`, `src/home/HomeLayout.{h,cpp}`, `src/home/ComponentSchema.{h,cpp}`, `config/skins/`, Studio catalog/inspector/edit-session files, `tests/tst_components.cpp`.
Scope:
- [ ] T22.1 Ship editable compact launcher, scrolling dashboard, poster/sidebar library and minimal/pastel templates; preview current-page versus whole-interface scope and preserve existing pages by default.
- [ ] T22.2 Add shared tokens and reusable sections/symbols with inheritance, instance overrides, named linked-instance impact preview, Edit all/Detach choice and atomic propagation.
- [ ] T22.3 Implement page/interface import/export with version/action/path/art validation, scope preview and missing-source remap; placeholders stay editable and unknown executable content is rejected.
- [ ] T22.4 Exclude accounts, credentials, media, Kodi databases, machine-specific grants and private absolute paths from portable exports; preserve existing personal skins and source ownership on round trip.
Done when: all four templates can be customized with remote controls; linked changes and portable round trips pass without accidental replacement, privacy leakage or required online access.
Verify: skin/page round-trip, token inheritance, symbol-cycle/propagation rollback, remap and export privacy fixtures in `component_contracts`; installed template selection and both visual fixtures. Evidence: pending; existing three skins are not four-template/reusable-symbol acceptance.

## T23 — Preview, Apply, restore and navigation safety (M, depends: T22)
Entry docs: `docs/page-studio-redesign.md` sections 3, 7-9; `docs/components.md` atomic failure contract.
Entry files: shared edit-session/Studio/renderer files from T16-T22, `src/home/HomeLayout.cpp`, `src/home/HomeSkins.cpp`, `src/home/ActionDispatcher.cpp`, `qml/Main.qml`, `tests/tst_components.cpp`, `tests/tst_home_navigation.cpp`.
Scope:
- [ ] T23.1 Implement full-viewport real-renderer Draft preview and Try navigation, with exact editor return; intercept playback/apps/device/install/network/live-settings side effects honestly.
- [ ] T23.2 Implement complete change summary, changed-page list, error-to-widget focus, baseline conflict handling, single atomic Apply, retained draft on failure and Retry/Discard/Continue choices.
- [ ] T23.3 Integrate bounded versioned backups, restore previous/default scope and original downgrade backup; retain apps/accounts/device settings/Kodi media and never uninstall on reset.
- [ ] T23.4 Verify dirty exit, profile switch, import/template application and empty/removed origin restore exact focus or permanent recovery; preview/cancel/undo leave applied bytes and profile assets unchanged.
Done when: installed edit -> preview -> cancel -> edit -> Apply -> restart and deterministic failure/retry flows are complete, safe and understandable across Home plus a destination page.
Verify: transaction/write-failure/backup/revision/profile tests in `component_contracts`, preview action-denial/focus tests in `home_component_navigation`, full CTest, installed journey and both fixture pairs. Evidence: pending; backend atomic save alone is not full Apply UX acceptance.

## T24 — Installed acceptance, usability and editor cutover (L, depends: T23)
Entry docs: `docs/page-studio-redesign.md` sections 9-11; T15 feature map/wireframes; `docs/vm-oracle.md`; both visual fixture READMEs.
Entry files: `qml/Main.qml`, `qml/SettingsView.qml`, `qml/CustomizeHomeView.qml`, `qml/PageSurface.qml`, Studio files from T18-T23, `tests/tst_components.cpp`, `tests/tst_home_navigation.cpp`, `tests/tst_kodi_media.cpp`, `tools/home-components-acceptance.py`, `tools/run-visual-acceptance.ps1`.
Scope:
- [ ] T24.1 Run the complete core Studio acceptance matrix: blank/existing/multiple/nested pages; remote and desktop parity; collisions/full grid; history; profile isolation; invalid docs; asset/route repair; atomic failure and recovery.
- [ ] T24.2 Capture and explicitly review both recreation/reset pairs on the exact installed source; verify legacy conversion/read preservation, 720p/1080p/4K scaling and real-Kodi page-source integration without personal-library mutation.
- [ ] T24.3 Measure remote-response/frame/large-layout/client-history targets and accessibility/contrast/reduced-motion/long-label behavior; run offline editor/network-denial and recorded uncoached user/representative journeys, iterating failures.
- [ ] T24.4 Route all normal Edit Home/Edit page/context/Settings paths to Studio, verify the legacy feature map, then retire duplicate action-grid editing flows and temporary developer flag without stranding layouts/recovery.
- [ ] T24.5 Publish source/release hashes, logs, actual captures, usability/performance results, outstanding hardware/product limitations and service-restoration proof; update dependent gate evidence without inventing completion.
Done when: the new workspace is the default usable editor, every supported operation is discoverable and remote-complete, both fixtures and rollback pass, and a representative can create/customize/preview/apply a page without coaching or accidental launches.
Verify: `ctest --test-dir build --output-on-failure` plus installed remote/pointer journeys, both visual wrappers with explicit SourceRoot, isolated real-Kodi pages test, offline denial, failure injection, profiling and usability report using the VM runbook. Evidence: pending. Physical hardware and full appliance Generator Test remain separate product gates.

## Next: related shell and presentation fixes

## T27 — Shared theme inheritance across shell surfaces (M, depends: T24)
Entry docs: `docs/shell-surfaces-design.md` sections 1, 4, 6; `docs/page-studio-redesign.md` sections 2, 6-8; `docs/home-customization.md` planned theme amendment.
Entry files: `src/home/HomeLayout.{h,cpp}`, `src/home/PageModel.{h,cpp}`, `src/home/ComponentSchema.{h,cpp}`, `qml/Main.qml`, `qml/CommandCenter.qml`, `qml/SettingsView.qml`, `qml/KodiLibraryView.qml`, `qml/KodiSettingsView.qml`, `qml/KodiImportView.qml`, `qml/MediaFoldersView.qml`, `qml/OnScreenKeyboard.qml`, their child dialogs/views and shared appearance helpers, `tests/tst_home_navigation.cpp`.
Scope:
- [ ] T27.1 Implement one effective theme context/resolver: packaged semantic defaults -> active profile/Home compatibility theme -> originating page inheritance -> explicit surface/component override; no migration write or stale previous-profile tokens.
- [ ] T27.2 Expose Inherit page/Use profile theme/Custom override plus reset and draft preview in Studio; validate shared state, text, focus, input, scrim, status and accessibility roles without scattered QML palette constants.
- [ ] T27.3 Apply the context to Search/All Apps, Settings navigation/subpages/dialogs/keyboards and every Isolde-owned Kodi browse/details/media-configuration/error/loading surface; inventory child controls so parent-background theming alone cannot pass.
- [ ] T27.4 Preserve query/selection/focus and Kodi request counts across appearance changes; isolate preview versus applied contexts, profile switches and destination/origin overrides; respect high contrast, reduced motion, art and semantic error readability.
- [ ] T27.5 Capture dark, pastel and arbitrary custom palettes across affected surfaces and assert resolved token/state values, accessibility and unchanged native Kodi skin/settings/media state.
Done when: opening Search, Settings or Isolde's Kodi pages from Home/custom pages follows the documented theme hierarchy throughout the interaction and nested controls; native Kodi remains untouched.
Verify: token precedence/profile/invalid-override tests in `component_contracts`, expanded `kodiSettingsFollowUserTheme` plus Search/Settings/keyboard/detail/error QML tests, Kodi request-count regressions, full CTest and real themed surface/fixture captures. Evidence: pending; existing partial appearance bindings are baseline only. Reuse the completed T22 token model/editor; do not add a competing resolver.

## T25 — Detected-source policy, styling and containment (M, depends: T27)
Entry docs: `docs/shell-surfaces-design.md` sections 1-2, 6; `docs/page-studio-redesign.md` sections 5-8.
Entry files: `src/sources/SourceManager.{h,cpp}`, `qml/SourceSettingsView.qml`, `qml/Main.qml`, `qml/HomeScreen.qml`, `qml/ComposedHome.qml`, `src/home/ComponentSchema.{h,cpp}`, Studio/edit-session files from T16-T21, `tests/tst_isolde.cpp`, `tests/tst_home_navigation.cpp`.
Scope:
- [ ] T25.1 Separate discovery facts from active-profile display policy and presentation; retain existing enable/hide/keep/handler choices and introduce allowed-page/slot/event/lifetime/suppression controls without broader scans or new media grants.
- [ ] T25.2 Register contained Detected sources and Source notice slot components with layout, variants, maximum visible items, typography/art/labels/colors/opacity/corners/focus/spacing and bounded region controls in Studio/Settings.
- [ ] T25.3 Replace unconditional section prepending with validated region projection; default to a compact Home status notice, contain overflow with View all, and never cover/reflow unrelated layout or steal focus. Missing slots route to Sources rather than floating injection.
- [ ] T25.4 Implement remote source actions, repeat-scan deduplication, reconnect/dismiss/hidden restore, unplugged-origin focus and safe idempotent promotion/error retry; cancel/draft changes leave applied policy/layout intact.
- [ ] T25.5 Prove per-profile persistence/rollback and fixture isolation with injected roots; no test-folder name special cases, deletion/unmount, Kodi writes, unsolicited launch or automatic entitlement/permission.
Done when: a user can configure what appears, where, when and how; multiple detections stay inside a declared themed container and every action/error/empty state remains remote-accessible.
Verify: extend source-policy/probe tests in `isolde_tests` and schema/draft tests in `component_contracts`; QML containment/focus/remote/unplug tests; full CTest plus actual dark/pastel multi-source captures and both standing fixtures using the VM oracle. Evidence: pending; existing global toggle/hide controls do not satisfy presentation/placement acceptance.

## T26 — Dedicated configurable All Apps destination (M, depends: T27)
Entry docs: `docs/shell-surfaces-design.md` sections 1, 3-4, 6; `docs/navigation.md` planned route amendment; `docs/page-studio-redesign.md` sections 6-9.
Entry files: `src/home/ActionDispatcher.{h,cpp}`, `src/home/ComponentSchema.{h,cpp}`, `src/app/AppRegistry.{h,cpp}`, `src/app/AppCatalog.{h,cpp}`, `src/home/HomePages.cpp`, `src/home/PageModel.{h,cpp}`, `qml/Main.qml`, `qml/CommandCenter.qml`, shared page/Studio views, `tests/tst_components.cpp`, `tests/tst_home_navigation.cpp`.
Scope:
- [ ] T26.1 Define distinct typed All Apps and Search routes; resolve legacy app-drawer to the gallery without rewriting stored layouts/skins and audit every More Apps/card/header/page entry point.
- [ ] T26.2 Register an app-catalog adapter and themed gallery page preset using existing page/layout machinery, stable deduplicated app IDs and real installed/registered states; exclude uninstalled Store offers and internal navigation from app counts.
- [ ] T26.3 Expose gallery layout/grid/list/density/art/labels/sort/favorites/local-filter controls through Studio; preserve packaged fallback and profile/page/document limits, with no parallel persisted app library or layout file.
- [ ] T26.4 Implement remote browsing, first/remembered selection, empty-state Store/Settings, real launch/failure/retry and exact gallery/origin focus+scroll restoration; local filtering never opens universal Search or triggers web requests.
- [ ] T26.5 Test legacy/current/custom-page/skin routes, duplicate entries, offline/needs-setup/failed states and registry changes/uninstall of selected app; gallery opening requires neither a query nor automatic keyboard focus.
Done when: All Apps/More Apps reliably opens a configurable installed-app page, and opening Search is a separate explicit action; app launches and Back preserve the complete origin chain.
Verify: action-dispatch/legacy-read tests in `component_contracts`, app registry/catalog state tests, QML gallery launch/return/filter/empty tests and an assertion that app-drawer never opens Command Center; full CTest, installed remote journey and both visual fixtures. Evidence: pending; changing only the All Apps label is insufficient.

## T28 — Genuine top-right Web search toggle (S, depends: T25,T26,T27)
Entry docs: `docs/shell-surfaces-design.md` sections 1, 5-6; `docs/unified-master-plan.txt` Section 13 Web OFF boundary.
Entry files: `qml/CommandCenter.qml`, `qml/Main.qml`, `src/settings/SearchSettings.{h,cpp}`, `src/home/ComponentSchema.{h,cpp}`, shared search-header/presentation/edit-session helpers, `tests/tst_home_navigation.cpp`, `tests/tst_components.cpp`, `tools/verify-t03-search.py`.
Scope:
- [ ] T28.1 Remove the searchToggle pseudo-result and render one genuine labeled On/Off switch in the header's top-right default slot, with themed/stylable presentation, accessible checked state and no result-count/index participation.
- [ ] T28.2 Implement query/header/switch/results/keyboard D-pad focus transitions, Confirm/click activation, empty/late-result stability and exact Back origin; no arrow-driven accidental toggle or forced mouse/Tab usage.
- [ ] T28.3 Use the existing single SearchSettings authority with default OFF, active-profile persistence and atomic rollback on failure; retain query, selection and visible state, and clear old profile context on switch.
- [ ] T28.4 Preserve request-generation cancellation/network denial when switched OFF, suppress late/cached online matches and distinguish ON from actual provider availability; no provider shows honest header status and local results, not fake result rows.
- [ ] T28.5 Replace string-pattern-only toggle checks with state/focus/results/persistence tests; verify dark/pastel/custom palette, reduced-motion, long-label and offline/no-provider behavior, then rerun the combined T25-T28 surface journeys against the accepted Studio without regressing T24.
Done when: Web search is visibly and behaviorally a real top-right header switch, its persisted state is truthful and accessible, and its control never masquerades as a search result or enables unintended external requests.
Verify: QML header-switch/query/results/empty/keyboard/late-callback tests in `home_component_navigation`, preference/profile/write-failure tests, runtime network-denial regression plus updated static helper, full CTest and installed themed remote captures. Evidence: pending; T14 still owns real provider implementation/terms, and T03's existing preference model is not header-control acceptance.

## Then: remaining product and release work

## T01 â€” Bootstrap + gate check (S, depends: none)
Entry docs: `README.md`, `docs/implementation-order.md:19-38`, `tests/visual/home-reference/README.md`.
Entry files: `CMakeLists.txt`, `tools/home-reference-acceptance.py`.
Scope:
- [x] T01.1 Linux Qt6.4+ configure+build+ctest passes (`cmake -S . -B build -G Ninja`, `cmake --build build`, `ctest --test-dir build --output-on-failure`)
- [x] T01.2 Disposable-profile acceptance run: home-reference, home-components, tv-library `--pages --inspect`
- [ ] T01.3 Record current-source Linux build/test evidence and Windows limitations
Done when: all currently registered Linux suites pass and both fixtures have recreation/reset evidence tied to the tested source.
Verify: exact configure/build/CTest logs, source identity, result.json and screenshot pairs. Historical fixture evidence: `tests/visual/home-reference/last-review.json`, `tests/visual/home-pastel-reference/last-review.json`; historical TV evidence: `/tmp/isolde-tv-acceptance-m986n9dt/result.json` (guest; retention not verified). T01.1/T01.2 retain historical credit; rerun before closing T01.3. The earlier claim of Linux configure/build/CTest passing in the Windows environment with wildcard evidence was unsupported and is withdrawn. See [planning audit](planning-audit.md) and [VM oracle](vm-oracle.md).

## T05 â€” Studio grid renderer consumes placements (M, depends: T01)
Entry docs: `docs/studio-builder.md` Universal grid, `docs/home-composition.md` Universal grid.
Entry files: `src/home/HomeLayout.h`, `src/home/ComponentSchema.h`, `qml/ComposedHome.qml`, `qml/PageSurface.qml`.
Scope:
- [x] T05.1 Render `composition.grid` + per-widget `placement` (snap-only, overflow refused, empty cells valid, 16:9 scale, guides edit-mode only) via `PageModel.grid/placements`
- [x] T05.2 Legacy no-grid layouts unchanged; invalid stored placements fallback to packaged defaults without rewriting file
- [ ] T05.3 Both visual fixtures plus placement-specific QML geometry evidence pass against the renderer source
Done when: renderer consumes grid, no new schema file, fixtures green.
Verify: `universalGridValidatesAndPersists` + QML geometry tests + fixture pair. Evidence: data contract `HomeLayout::grid()/setGrid()/setItemPlacement()/gridCollisions()` + `ComponentSchema::validGrid()/validPlacement()` + `universalGridValidatesAndPersists` test; QML renderer `ComposedHome.qml` consumes grid via `placementMode`, `placementCell`, `placedItems`, `placementGrid` (snap-only, overflow refused, empty cells valid, 16:9 scale, guides edit-mode only). Source credit only for T05.1/T05.2; current-source geometry/CTest logs and both fixture pairs are still required. Current installed release `deacfc0b6d02d96d759303da` passes 14/14 suites and both reviewed recreation/reset pairs; see `docs/vm-update-2026-10-19.md`. Placement-specific geometry/empty-cell/scale acceptance remains open, so T05.3 stays unchecked.

## T03 â€” Local search over Kodi library (M, depends: T01; curation adapters removed from scope)
Entry docs: `docs/unified-master-plan.txt` Sec 13, `docs/kodi-media.md`.
Entry files: `qml/CommandCenter.qml`, `src/home/PageData.h`, `qml/Main.qml` (openSearchFromPage).
Scope (Kodi-bridge only â€” no tag/playlist/curation adapters; those were T02, struck):
- [x] T03.1 Kodi title search (Movies/Shows/Episodes) with per-type bounded paging, debounce + cancel-aware, explicit Kodi-unavailable state
- [x] T03.2 Persisted Web OFF/ON toggle data model default OFF (OFF = zero external requests; ON path stubbed, no endpoint wired)
- [x] T03.3 Page-scoped sources via `pageSearchSources()` surfaced + routed back to originating page
- [ ] T03.4 Installed remote-only + real-Kodi acceptance
Done when: offscreen Command Center + bridge bounds tests pass, Web OFF provably silent.
Verify: component suite `pagesPersistAndRouteAndTravelWithSkins`, network-denial check. Evidence: T03.3 done per Oct-18 M4 (PageModel::pageSearchSources, CommandCenter pageSources, Main openSearchFromPage). T03.1/T03.2/T03.3 done per Oct-18 M4 with 8/8 static checks passing (verify-t03-search.py). T03.4 manual acceptance only.

## T04 â€” Kodi preservation + desktop import (M, depends: T01)
Entry docs: `docs/master-roadmap.md` steps 1-2, `docs/install.md`.
Entry files: `src/media/KodiService.h`, `src/sessions/DesktopSession.h`, `src/media/KodiImporter.{h,cpp}`, `qml/KodiImportView.qml`.
Scope:
- [ ] T04.1 Explicit backup/preview/import/conflict flow for non-default skin/sources/add-ons/watch-resume, rollback supported
- [ ] T04.2 Clean ownership handoff + guided first-library setup + Desktop/Return workflow
- [x] T04.3 Nested-folder scan to direct playback, no media moves/renames, no Kodi DB edits
Done when: real round-trip preserves skin+sources+watch-state; import rollback tested.
Verify: appliance/vm acceptance with disposable fixture (hashes unchanged). Evidence: docs/kodi-media.md remembered media folders + nested discovery + direct playback done (hashes unchanged); T04.1/T04.2 remain open: importer/UI/setup source exists (`6ff2bcb`), but import does not transfer settings and rollback copies onto existing files. No safe installed preservation/rollback evidence; see `docs/planning-audit.md`.

## T12 â€” System polish + input + on-screen keyboard (M, depends: T05)
Entry docs: `docs/architecture.md` Audio/Remote, `docs/development-plan.md` keyboard, `docs/interface-design.md`.
Entry files: `src/input/InputManager.h`, `src/settings/*.h`, `qml/OnScreenKeyboard.qml`.
Scope:
- [ ] T12.1 Audio/display/power/network onboarding polish, a11y/l10n, reduced-motion/contrast
- [ ] T12.2 On-screen keyboard for Isolde-owned fields first (Network Settings, Store search): D-pad/Back/shift/symbols/focus-restore, never obscures field, passwords masked
- [ ] T12.3 Remote-only journey + offline-denial tests
Done when: keyboard + polish acceptance passes without breaking fixtures.
Verify: input/a11y tests + installed remote run. Evidence: InputManager commands + AudioSettings/RemoteSettings/NetworkSettings/DisplaySettings pages + mixer doubles + volume/remote acceptance exist; `qml/OnScreenKeyboard.qml` exists and is used by Network Settings and Store; remote journey/masking/visibility/focus evidence remains required; a11y/l10n/reduced-motion incomplete. All boxes open.

## T09 â€” Playback preference + viewing options (M, depends: T24)
Entry docs: `docs/unified-master-plan.txt` Sec 16, `docs/kodi-media.md`.
Entry files: `src/media/KodiMedia.cpp`, `src/settings/SetupState.h`, `qml/SettingsView.qml`.
Scope:
- [ ] T09.1 `Settings â†’ Media â†’ Playback Preferences â†’ Always Play Locally When Available` default ON
- [ ] T09.2 ON: local match from shelf/search plays/resumes immediately via Kodi; series tile â†’ episodes unless Continue/Play-Next; details via context menu
- [ ] T09.3 OFF: Web ON discovery may show viewing-choices; library shelves still direct; resume/subtitle/watched retained via Kodi
- [ ] T09.4 6-row Sec-16 acceptance table passes on installed build
Done when: direct-play matrix verified, Web OFF silent.
Verify: playback acceptance (movie shelf/search, Continue resume, seriesâ†’episodes, OFF unchanged). Evidence: partial â€” direct Kodi playback/resume, Continue Watching (inprogress), seriesâ†’episodes, shared player path exist per docs/kodi-media.md; Always-Play-Locally preference + viewing-choices page + 6-row table open.

## T10 â€” Capture + HDMI + audio editors, gated (L, depends: T24)
Entry docs: `docs/master-roadmap.md` capture section, `docs/audio.md`, `docs/remotes.md`.
Entry files: `src/media/CaptureController.h`, `src/settings/AudioSettings.h`, `qml/CaptureSettingsView.qml`.
Scope:
- [x] T10.1 Fake-backend state-machine + protocol/action-schema/dispatch unit tests (software only, no hardware claims)
- [ ] T10.2 Real RS-232/capture/audio/latency/hotplug matrix qualification
- [ ] T10.3 Capture-input editor + 8-input HDMI switcher editor + audio-routes editor + keyboard/remote integration (only after T10.2)
Done when: hardware matrix signed off; VM-only never counted as live pass.
Verify: `tst_capture` + hardware matrix log. Evidence: CaptureController (devices/formats/inputs/serial/commandTemplate) + HDMI-inputs settings page exist; hardware qualification + device editors open per docs/implementation-order.md:49.

## T11 â€” Guide / PseudoTV (L, depends: T24)
Entry docs: `docs/unified-master-plan.txt` PseudoTV Sec 1-10, `docs/implementation-order.md:50`.
Entry files: `src/media/` (new guide store), `qml/GuideView.qml` (new), `qml/ComposedHome.qml` (compact widget).
Scope:
- [ ] T11.1 Inspect actual backend first, then PVR or playlist/EPG adapter; compact widget (3-5ch + progress) â†’ overlay â†’ full EPG
- [ ] T11.2 SQLite WAL store (channels/rules/slots/migrations), persisted broadcast clock (real-time/pause-when-off/fixed/shuffle), stable committed slots, targeted repair, no global reindex on guide open
- [ ] T11.3 12-hour-off seek test + restart-stability (7-day shuffle unchanged) + Cartoon-Network-in-one-save + offline cached listings
Done when: Sec-10 acceptance tests pass with evidence.
Verify: guide acceptance log + DB slot-stability tests. Evidence: none â€” no guide store/widget/EPG in tree; guide-summary cards are static config per docs/home-composition.md (no backend). All open.

## T13 — Appliance + release distribution (L, depends: T01,T03-T05,T09-T12,T15-T28; T14 optional before release)
Entry docs: `docs/install.md`, `docs/packages.md`, `docs/architecture.md` appliance.
Entry files: `tests/test_appliance_install.py`, `tests/test_recovery.py`, `src/recovery/Recovery.qml`.
Scope:
- [ ] T13.1 Dedicated compositor/autologin, clean-machine onboarding, signed artifacts, offline dependency/model manifests
- [ ] T13.2 Repair/rollback + recovery media (`isolde-recovery`), explicit disk-selection consent
- [ ] T13.3 Fresh install + cold boot + power-loss + failed-upgrade + disconnected Generator Test on target hardware
Done when: public-release gate passes. Release cannot precede this.
Verify: appliance_install + appliance_recovery + Generator Test log. Evidence: partial foundation â€” install-appliance.py (Ubuntu 26.04 dev/pilot only), wait-for-display gate, atomic content-addressed releases, isolde-recovery, rollback path exist per docs/install.md; dedicated compositor/autologin/signed images/offline media/first-run wizard/hardware Generator Test all open.

## T14 â€” Optional online discovery + linguist subtitles (M, depends: T03; may run after T13)
Entry docs: `docs/implementation-order.md:53-54`, `docs/unified-master-plan.txt` Sec 13-15.
Entry files: `qml/CommandCenter.qml`, `src/media/KodiMedia.cpp` (subtitle load path).
Scope:
- [ ] T14.1 Online discovery opt-in: regional freshness/attribution, provider launch routes, OFF cancels late/cached results, remakes distinct, installed app never = entitlement (requires provider/terms decision first)
- [ ] T14.2 Linguist: SRT translate/generate/queue/review/cancel, offline, non-overwriting Unicode, provenance, no library-wide auto jobs or implicit downloads
Done when: OFF-silent + provenance tests pass; provider gate documented.
Verify: network-denial + subtitle provenance tests. Evidence: persisted Web preference exists and ON remains an unavailable provider stub; T28 owns its header-switch UX, not a provider implementation. No linguist/SRT implementation evidence; T14 remains open.

## Archive: excluded from execution and completion math

## T02 â€” ~~Finish curation~~ STRUCK / OUT OF SCOPE
Decision (user 2026-10-19): curating the collection is the user's responsibility. Isolde uses Kodi to handle what they have. No episode/tag/playlist/exclusion/template curation work. Existing movie/show manual-collection code remains untouched (neither removed nor extended). Excluded from Overall %.

## T06 — Superseded: remote + desktop editor

Excluded from progress math. The old list/action-grid editing plan is replaced by T17-T21 and T24. Its move/resize/collision/duplicate/remove, internal sub-grid and remote/desktop parity obligations are retained in those tasks. Existing source/test coverage is a reusable baseline, not acceptance of the new workspace.

## T07 — Superseded: Preview/Apply/Undo

Excluded from progress math. Drafts, atomic multi-item operations, history, compatible persistence and profile isolation move to T16; preview/apply/backups/recovery UX moves to T23; integrated failure acceptance moves to T24. Draft transactions must land before new manipulation UI.

## T08 — Superseded: templates/tokens/portability

Excluded from progress math. Four templates, tokens/symbols, page/interface export/import and missing-source repair move to T22 with T23 apply integration and T24 cutover acceptance. Existing skins and pages are preserved.

## Priority revision verification

2026-10-19: verified all 24 active priority ranks and task-section order, every dependency preceding its consumer, preserved task IDs/checkbox credit, resolved document links and documentation-only scope. Overall stays 11%. Evidence: `C:/Users/kketc/.codex/visualizations/2026/10/19/01a15691-1cd3-77d0-928f-854e1c7a566b/task-priority-review-20261019.log`. No application implementation, tests or VM changes in this revision.
## Completion rule

Overall = round(sum(active task percentages)/24). Active set: T01,T03-T05,T09-T28. Exclude T02 (struck) and T06-T08 (superseded). When every active checkbox and Done when gate has specific evidence and all 24 active tasks read 100%, this active backlog is implemented. Documentation, mockups, source presence and old-release screenshots do not count as delivery. If an active task lacks required evidence, it cannot read 100%; the existing 90% no-evidence cap also applies. Adding tasks requires updating the active count, table, dependencies and AGENTS.md consistently.
