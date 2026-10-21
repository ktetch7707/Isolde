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

Page Studio is the highest-priority feature package. Complete **T15 → T16 → T17 → T18 → T19 → T20 → T21 → T22 → T23 → T24** first: design, safe drafts/history, grid/canvas, workspace, remote editing, desktop/nested editing, inspector, templates, Preview/Apply, and installed usability/cutover. **Finish reopened T19.6, then T20.5, then T21.6-T21.14 before T22. These gates implement the user's revised movement/color/context UX and real component semantics. T15 user review remains deferred to T24 under the user-authorized provisional-design exception.** Existing current-source build/regression gates remain mandatory during each implementation slice; this ordering does not waive safety tests or hardware limits.

Next complete the related shell improvements: **T27 → T25 → T26 → T28** (shared theme, contained source notices, All Apps gallery, genuine Web switch). These extend the accepted Studio and no longer block its templates, Preview/Apply or core cutover. Then follow the remaining ranked rows for evidence repair, media preservation, input/system polish, playback, hardware/guide, appliance distribution and optional online work.

The table AND task sections are physically in execution order. Task IDs remain stable for commit/evidence references; their numeric values are not priority. Struck/superseded rows are archived at the end. T17 supplies T05's remaining geometry evidence; when reviewing that later ledger task, reuse valid current-source evidence rather than repeating implementation. A blocker must be reported/resolved, not used to silently switch to a lower-priority feature. The planning-only restriction applied to the priority-edit request. The subsequent user request explicitly authorizes application implementation and VM deployment using the design provisionally.

The first Studio expansion changed 13 active tasks to 20 (21% to 14%). This additive revision adds four zero-percent tasks, giving 24 active tasks and **11% overall** (275/24 rounded). Existing implementation credit is unchanged. Planning/specification work is not delivery of the new system.

## Overall (update every edit)
| Task | Priority | Title | % | Status |
|------|----------|-------|---|--------|
| T15 | 1 | Page Studio interaction design and usability validation | 75% | provisional design only; T15.4 user acceptance OPEN, deferred to T24 |
| T16 | 2 | Shared draft transactions, history and profile safety | 100% | task verification complete; T17 next |
| T17 | 3 | Shared canvas geometry, grids and legacy compatibility | 100% | geometry checkpoint verified; T18 next |
| T18 | 4 | Unified Page Studio workspace and selection | 100% | workspace/lifecycle/selection verified; T19 next |
| T19 | 5 | Remote pick-up/drop, resize and collision workflows | 83% | reopened: compact context and direct movement with live reflow; next |
| T20 | 6 | Desktop parity, layers and nested/group editing | 80% | reopened: pointer/reflow parity; depends on T19 |
| T21 | 7 | Contextual inspector, catalog and capability binding | 36% | reopened: component semantics, visual editing, shared selection, editable header and tool organization; depends on T20 |
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
| **Overall** | — | **average of 24 active tasks** | **35%** | T15 provisional; T19-T21 reopened for user-facing usability; T19 next |

## Highest priority: core Page Studio

Control audit (user, 2026-10-21): [visual control replacement requirements](design/page-studio/control-audit-2026-10-21.md) adds property-specific sliders, steppers, switches, segmented choices, visual crop/layout/app selection and relevant exact-input alternatives to existing open T20.5/T21 gates, with T22 inheritance and T24 acceptance. Source review also found the current painted color wheel always writes red; T21.10 remains open until real remote/pointer color selection is verified. Preserve existing execution order and all completion credit: Overall round(849/24) = 35%. This is a static audit/design update, not an application or VM change.

Approved usability expansion (user, 2026-10-21): [15 selected features and acceptance requirements](design/page-studio/usability-expansion-2026-10-21.md) is mandatory scope for the existing open T21–T24 gates. Includes component presets, My components, copy appearance, linked styles, content-slot templates, replacement in place, section isolation, responsive rules, content stress tests, private reference overlay, action search, useful empty states, named versions, household editing permissions and page health checks. The five declined ideas are excluded from this expansion. Finish T19.6 → T20.5 first; no new task IDs, checkbox credit or priority changes. Overall remains round(849/24) = 35%.

Follow-up UX findings (2026-10-20): the selection/header/tool-organization section of [the usability audit](design/page-studio/usability-audit-2026-10-20.md) adds T21.12-T21.14. Studio selection follows the authored Home focus style, header elements are directly editable, and ordinary tools receive named homes instead of a mixed More menu. Controls must be discoverable without persistent instructional prose. Current T21 = 5/14 = 36%; Overall round(849/24) = 35%. Earlier calculations below are historical checkpoints. No implementation credit is added.

Latest usability correction (user, 2026-10-20): [UI audit and interaction contract](design/page-studio/usability-audit-2026-10-20.md) supersedes hex-first colors, top-level Width/Height, buried deletion, Move before/after and mandatory normal-drop Shift dialogs. Context becomes Move/Edit component/Duplicate/Delete; size is inside Edit component; visual color picking includes a remote-operable wheel; movement previews neighbors making room and accepts one reversible transaction. First finish T19.6, then T20.5, then T21's remaining gates before T22. Earlier checked boxes retain historical checkpoint credit, not acceptance of the rejected UX. T19 5/6 = 83%, T20 4/5 = 80%, T21 5/11 = 45%; Overall round(858/24) = 36%. This is a design/audit update only.

User component clarification (2026-10-20): [component contracts](design/page-studio/component-contracts.md) supersedes the earlier six-family taxonomy where different. App and Recently viewed rows must be genuine configurable collections, not hand-placed illustrated buttons. At that component-definition checkpoint T21 retained five historical boxes and added four open gates (56%; Overall 38%); the newer usability audit above adds further gates and supplies current percentages. No new implementation is claimed by this design update. T22 waits for the reopened T21 gates. Existing saved layouts are not rewritten.

Reference-driven UX audit (2026-10-20): [reference-design-audit.md](design/page-studio/reference-design-audit.md) is an Entry doc for T17-T24. Required scope additions: T17 bounded child layout/ratios; T18 unobscured selection/context placement; T19-T20 alignment/distribution/groups; T21 Section/Group tool, independent card/label styling, state/typography/surface controls and honest live/sample states; T22 editable patterns/style reuse and private editor-only reference image overlay; T23 reference/export isolation and error recovery; T24 reconstruct both user references with generic editable components and record uncoached journeys. Arbitrary screenshot conversion and unavailable service/device capabilities are not promised. No boxes are checked by this audit; task percentages and Overall remain unchanged.

Direct-edit refinement (user, 2026-10-19): the latest section of [Page-first Studio](design/page-studio/page-first-design.md#latest-refinement-direct-screen-editing) supersedes Confirm-picks-up and primary Preview controls. The actual runtime-rendered screen is edited live in a draft. Confirm selects; context offers Move, Width, Height and Edit component. Buttons and images share None/Open page/Supported action links. Optional Try interaction is secondary; Apply remains explicit. T18/T19/T21/T23/T24 must verify these journeys; no task credit changes from this planning refinement.

UX revision (user feedback, 2026-10-19): read [Page-first Studio](design/page-studio/page-first-design.md) before T17-T24/T27. Full-size page, overlaying left page menu, single context windows and protected movable Home designation replace the provisional toolbar/fitted-preview shell. T16 backend evidence remains valid; neither the revised design nor the interactive concept is implemented acceptance. Percentages remain unchanged. Minimum Settings/Home theme inheritance is required before T24 cutover; T27's remaining shell adoption follows T24.

## T15 — Page Studio interaction design and usability validation (M, depends: none; selected priority)
Entry docs: `docs/page-studio-redesign.md` sections 1-4, 10-11; `docs/shell-surfaces-design.md` sections 1-6; `docs/interface-design.md` Page Studio amendment.
Entry files: `qml/CustomizeHomeView.qml`, `qml/PageManager.qml` (read-only workflow inventory); new design artifacts under `docs/design/page-studio/`.
Scope:
- [x] T15.1 Inventory every currently supported editor operation plus source notices, All Apps, themed shell surfaces and the Search header switch; map each to Studio and identify capability gaps without treating UI labels as backend proof.
- [x] T15.2 Produce reviewed TV and desktop wireframes for blank/existing pages, Add, selection, inspector, pick-up/drop, resize, collision, dirty exit, preview, apply error and nested scope.
- [x] T15.3 Walk through the complete D-pad/Confirm/Back/Menu state model, including field editing, focus restoration, error feedback and accessible movement alternatives.
- [ ] T15.4 OPEN — genuine user/representative review; resolve confusing transitions and establish measurable acceptance journeys. Designer-only artifact walkthrough done (W1-W8 resolved in artifacts). Sequencing exception (user, 2026-10-19): proceed with this pack as provisional basis for T16-T24; genuine user usability acceptance deferred to T24. No review evidence invented.
Done when: T15.1-T15.3 artifacts agree with the specifications; T15.4 user walkthrough deferred to T24 per sequencing exception; remote movement is understandable without a pointer, number keys or hidden gesture.
Verify: design state-transition/feature-coverage review and recorded uncoached walkthrough; no application build is claimed by static mockups. Evidence: design pack under `docs/design/page-studio/` — `feature-map.md` (T15.1; every Customize Home action + PageManager operation + the four shell surfaces mapped with backend-path citations, labels not treated as proof), `wireframes.md` (T15.2; 12 TV+desktop screen states), `interaction-states.md` (T15.3; full D-pad/Confirm/Back/Menu table, focus restoration, error feedback, accessible movement alternatives), `review-and-acceptance.md` (T15.4 remains OPEN; designer-only walkthrough with 8 findings + 10 proposed acceptance journeys); static coverage gate `tools/verify-t15-design.py` 57/57 PASS, log in `docs/design/page-studio/verify-t15.log`. Uncoached installed walkthrough with the actual user remains T24's cutover gate per spec §10.

## T16 — Shared draft transactions, history and profile safety (L, depends: T15)
Entry docs: `docs/page-studio-redesign.md` sections 3, 5, 7; `docs/components.md` persistence contracts.
Entry files: `src/home/HomeLayout.{h,cpp}`, `src/home/PageModel.{h,cpp}`, `src/home/HomePages.cpp`, `src/home/ComponentSchema.{h,cpp}`, `tests/tst_components.cpp`.
Scope:
- [x] T16.1 Add one profile-scoped in-process edit session over the complete interface document, with baseline revision and live draft view; opening/cancel/undo do not write the applied document or profile assets.
- [x] T16.2 Introduce shared typed semantic edit commands and complete-result validation; multi-item/group/collision operations accept all affected changes or none, with one undo record.
- [x] T16.3 Implement deterministic canonical serialization and centrally allocated identities; remote/pointer/inspector replays produce identical layout bytes from identical fixture identities.
- [x] T16.4 Implement bounded undo/redo, explicit history-budget handling, revision conflicts, dirty-exit semantics and profile teardown/client invalidation.
- [x] T16.5 Prove failed draft operations and atomic apply failures preserve original bytes, applied model, recoverable draft and focus origin; keep v1 backup and no-write-on-read behavior.
Done when: a minimal offscreen command-to-draft-rendering journey demonstrates move/property edit/undo/redo/cancel/error without persistence leakage; the new shell can consume the session without calling live setters.
Verify: extend `component_contracts` for multi-node rollback, canonical parity, bounds, revision conflict and cross-profile cases; run `ctest --test-dir build --output-on-failure`. Evidence: [page-studio-implementation-2026-10-19.md](page-studio-implementation-2026-10-19.md), frozen source `/home/isolde-dev/isolde-runs/studio-implementation-12/source`, sibling `ctest.log` (14/14), `install.log`, installed Studio `/tmp/isolde-components-acceptance-u7lfm2xd`, both fixture runs `/tmp/isolde-reference-config-enw44obb` and `/tmp/isolde-reference-config-xx0hwu86`; durable host logs/captures under `build/studio-installed-12/`. T16.3 replay evidence: `/home/isolde-dev/isolde-runs/studio-implementation-13/targeted-tests.log` and sibling `ctest.log` (14/14); the actual pointer drag, remote keys and numeric inspector produce byte-identical drafts with no applied writes. Snapshot 13 archive SHA-256 `f61fc75fe0accc6cf6644e558a3ee8070d4a61ef686651515ca335249ebafd67`; its only application delta from installed snapshot 12 is a nonvisual MouseArea objectName for locating replay targets. Standing full-product Generator/cutover gates remain open in their owning tasks. The provisional shell/geometry are test consumers of this foundation; T17-T24 are not marked delivered.

## T17 — Shared canvas geometry, grids and legacy compatibility (L, depends: T16)
Entry docs: `docs/page-studio-redesign.md` sections 2, 5, 9; `docs/home-composition.md` Universal grid.
Entry files: `qml/ComposedHome.qml`, `qml/HomeScreen.qml`, `qml/PageSurface.qml`, `src/home/ComponentSchema.{h,cpp}`, `src/home/HomeLayout.{h,cpp}`, `src/home/PageModel.{h,cpp}`, `tests/tst_home_navigation.cpp`.
Scope:
- [x] T17.1 Establish shared measured geometry for runtime rendering, canvas, hit-testing and spatial focus; include safe area, reserved header, cell spans, real empty cells and out-of-bounds refusal.
- [x] T17.2 Default to the full-size authored viewport with overlay tools; fitted/zoomed/panned views are optional. Opening/closing tools never changes page geometry, layout data or runtime aspect.
- [x] T17.3 Add validated one-screen root and scrolling section-grid modes plus bounded nested/internal grids; no implicit row growth or placement overwrite.
- [x] T17.4 Open legacy flow/no-grid pages unchanged through the canvas contract; explicit conversion previews differences and preserves IDs/routes/artwork, with no startup/read/cancel migration write.
- [x] T17.5 Add placement-specific geometry/focus tests at 720p, 1080p, 4K and the legacy 1280x800 fixture viewport; rerun and review both real fixture recreation/reset pairs.
Done when: canvas-to-runtime geometry agrees at cell boundaries and scaling extremes; legacy fixtures remain generic; T05's remaining placement gate has concrete evidence and is updated separately.
Verify: extend `universalGridValidatesAndPersists` and `home_component_navigation` with actual placed widgets, holes/header offsets/scroll containers; full CTest and both VM visual fixtures. Evidence: [studio-geometry-2026-10-20.md](studio-geometry-2026-10-20.md), source snapshot 18 (archive aae05fd1d9dcc9877878543074309a32e4b1717558fe3068265c8dcdc17b6d69), full 14/14 CTest, installed release d30eff6291f378d0fd37b276, both recreation/reset pairs and Studio Apply/reopen; durable logs/config/captures under build/studio-installed-18/. Full nested editing, precision appearance controls and accepted editor chrome remain T18-T24.

## T18 — Unified Page Studio workspace and selection (M, depends: T17)
User-requested entry correction (2026-10-20): the old 32-action Customize Home grid was still the normal entry, and named pages used PageEditor. This supersedes the earlier “developer entry remains until T24” statement below. Normal Home/page editing now routes directly to the same Studio; compatibility-only legacy operation tests are explicitly separate. Current-source verification and installed entry capture: [studio-entry-correction-2026-10-20.md](studio-entry-correction-2026-10-20.md). This narrow correction does not complete T24's feature parity/usability/performance/offline matrix. T18 checkboxes remain 4/4; no new feature credit.
Entry docs: `docs/page-studio-redesign.md` sections 2-4, 6; T15 approved wireframes and feature map.
Entry files: `qml/CustomizeHomeView.qml`, `qml/PageManager.qml`, `qml/PageSurface.qml`, `qml/Main.qml`, `src/home/HomePages.cpp`, `tests/tst_home_navigation.cpp`; new native Studio view files only as needed.
Scope:
- [x] T18.1 Add the page-first shell: full-size page, collapsible overlay left page menu, Add page, draft status, contextual tools, Preview/Apply and secondary history/guides; no permanent top ribbon or forced miniature preview.
- [x] T18.2 Implement one contextual window at a time, page-list Confirm actions, deterministic selection/hit-testing, named outlines, breadcrumbs and exact focus restoration; desktop pinning is optional and never reflows the page.
- [x] T18.3 Implement page create/duplicate/name/reorder and validated profile-scoped Home designation. Protect designated Home from deletion, preview startup/routes/Settings-theme consequences, repair incoming links before deleting other pages, and apply all affected changes atomically with undo and page limits.
- [x] T18.4 Make blank, empty, hidden, missing-source and unreadable-draft states recoverable using remote commands and independently readable editor chrome.
Done when: remote-only entry -> page selection/create -> component selection -> panel edit -> Back returns to the exact canvas origin; no live saves or accidental playback.
Verify: offscreen workspace/focus/empty-state tests in `home_component_navigation`, page draft/isolation tests in `component_contracts`, and installed screenshot/key journey on the VM. Evidence: [studio-page-lifecycle-2026-10-20.md](studio-page-lifecycle-2026-10-20.md), frozen source snapshot 30, archive `c2602bd4b2e18f961b46dcb9032e6734cc2d9b5c2114766bd06064bb8de0cea5`; full 14/14 CTest (57.62 seconds), actual legacy pointer selection/outline geometry/no-dispatch regression, remote create/rename/duplicate/designate/protect/delete/undo, link remap, hidden/locked recovery, readable profile-local chrome and 720p/1080p/4K named-page viewport parity. Installed release `d29207a40cc6296737bc757f`; Studio `/tmp/isolde-components-acceptance-fv1_g0gz`, dark `/tmp/isolde-reference-config-1g6u9fkz`, pastel `/tmp/isolde-reference-config-h6gprdyl`, both recreation/reset pairs reviewed; durable host evidence `build/studio-installed-30/`. Normal Household service/executable restored, active/running, zero restarts. Earlier shell checkpoint: [studio-workspace-2026-10-20.md](studio-workspace-2026-10-20.md). Nested/group Layers, complete inspector/catalog, Apply recovery, user usability/offline Generator/cutover and source-notice containment remain in their owning later tasks. Developer entry remains until T24.
## T19 — Remote pick-up/drop, resize and collision workflows (L, depends: T18)
Entry docs: `docs/page-studio-redesign.md` sections 4-5; T15 movement/collision wireframes.
Entry files: `src/input/InputManager.{h,cpp}`, `src/home/ComponentSchema.{h,cpp}`, shared edit session from T16, shared canvas from T17, Studio views from T18, `tests/tst_home_navigation.cpp`, `tests/tst_components.cpp`, `tools/home-components-acceptance.py` (Studio remote oracle only).
Scope:
- [x] T19.1 Implement Confirm selection with Move/Width/Height/Edit component context; Move picks up for one-cell D-pad movement, Confirm drop and Back cancel. Render the actual component ghost, origin, dimensions and invalid reason; never launch its action in edit mode.
- [x] T19.2 Implement anchored remote resize, numeric row/column/span entry with on-screen keyboard and Move before/after/to-cell alternatives; reject overflow without clamping.
- [x] T19.3 Implement explicit Shift/Swap/Cancel proposal dialogs, full affected-item preview, deterministic bounded cascade solving, disabled impossible choices and one atomic draft transaction/undo record.
- [x] T19.4 Support insertion, duplicate, hide, lock, removal consequences, edge auto-scroll, empty layouts and movement between named eligible container scopes; prevent cycles or hidden overwrites.
- [x] T19.5 Verify key repeat, cancellation during every state, full-grid failure, multi-collider failure, induced write failure and restored focus; no dependency on mouse, long press or physical number keys.
- [ ] T19.6 Replace the first component context with Move/Edit component/Duplicate/Delete; put deletion and consequences there. Implement visual insertion/reflow movement for ordered and grid layouts, live affected-neighbor proposals, one drop/undo, complete Back cancellation, explicit scope targets and understandable blocked targets. No before/after picker, forced conversion or routine occupied-target dialog; preserve locks, size, bounds and non-silent validated data. Required Entry doc: `docs/design/page-studio/usability-audit-2026-10-20.md`.
Movement repair (user, 2026-10-21): [coder review and direct reflow repair](studio-reflow-2026-10-21.md) fixes selected-item visibility during ordered movement, nearby deterministic displacement, blocked-drop canvas recovery, exact scroll cancellation, structured group previews and context-menu dispatch regressions. The record owns current verification evidence. T19.6/T20.5 stay open for their full remaining scope; this slice adds no completion credit. Overall remains round(849/24) = 35%.
Implementation checkpoint (2026-10-20): [first usability slice](studio-reflow-2026-10-20.md) implements the new context, direct ordered movement, live grid neighbor proposals, ordinary reflow drop/undo, primary full-span size sliders and shorter canvas feedback. Full native 14/14 passed on `studio-reflow-full-20261020`; graphical/install evidence is in that record. T19.6 remains open for the remaining acceptance scope; no checkbox or percent credit is added. Current T19 83%, T20 80%, T21 36%; Overall 35% across 24 active tasks.
Done when: remote-only add/move/resize/collision/duplicate/remove journey is complete and intelligible; accepted drops change only the draft and cancellation has zero document/history effects.
Verify: extended native remote state sequences and transaction/boundary failures; full CTest, both fixture pairs and installed remote evidence. Evidence: [studio-remote-editing-2026-10-20.md](studio-remote-editing-2026-10-20.md), frozen application/test snapshot 37, archive `f6ca0071258dc1c31187b014810f1e6023807f2b999d4ade671476b34002188d`; all 14 CTest suites passed (58.75 seconds). Model cases cover bounded indirect cascades, complete-result validation, full-grid refusal, locks, atomic section duplication, transfer/swap/order and no-write/undo; native cases cover repeat/anchors, every gesture-panel cancellation, original/ghost/proposal visibility and scale, disabled legacy-grid metadata, section cancel/undo and existing pointer/numeric parity. Deterministic write-failure/profile/Kodi regression suites remain passing. Installed release `32aa32c0089df0a3684a858e`: expanded Studio journey `/tmp/isolde-components-acceptance-52_xorqq` (EDIT/COLLISION/RESIZE/STUDIO/APPLIED), dark `/tmp/isolde-reference-config-8hgsoxv8`, pastel `/tmp/isolde-reference-config-9cbg5mra`; all nine captures explicitly reviewed. Durable evidence `build/studio-installed-37/`, normal service/executable restored active/running with zero restarts. Nested/group Layers, alignment/distribution, complete inspector/catalog, full-product offline Generator and genuine user/cutover review remain their owning T20-T24 gates; current developer entry remains.
## T20 — Desktop parity, layers and nested/group editing (L, depends: T19)
Entry docs: `docs/page-studio-redesign.md` sections 2, 4-7; T15 desktop and scope wireframes. Control requirements: `docs/design/page-studio/control-audit-2026-10-21.md` (owning rows only).
Entry files: Studio/shared canvas/edit-session files from T16-T19, `src/home/ComponentSchema.{h,cpp}`, `src/home/PageModel.{h,cpp}`, `tests/tst_components.cpp`, `tests/tst_home_navigation.cpp`.
Scope:
- [x] T20.1 Implement pointer drag/drop/resize/snap indicators and inspector position entry through the same proposal/transaction path; pan/zoom/fit never alter page content.
- [x] T20.2 Implement synchronized Layers with expand/collapse, reorder/reparent, parent breadcrumbs, hidden reveal, locks and separately inspectable focus/reading order.
- [x] T20.3 Implement remote and pointer multi-selection/group actions, nested-container editing and widget-local sub-grids, including an honest unavailable eight-input 4x2 presentation; bound depth/selection and refuse invalid parents.
- [x] T20.4 Prove remote, mouse and numeric inspector operations produce identical serialized layouts and identical collision/error/undo outcomes; keyboard shortcuts are optional accelerators.
- [ ] T20.5 Match revised T19 movement with selected-item pointer drag, movement threshold, clear insertion/neighbor previews, explicit cross-container targets, reduced-motion feedback and exact remote/pointer/undo/cancel parity. Test rows/carousels, mixed-size/full/locked grids and decorative overlap policies. Required Entry doc: `docs/design/page-studio/usability-audit-2026-10-20.md`.
Done when: the same nested/group arrangement can be authored entirely by remote or desktop with byte-identical output; no desktop-only operation or small-handle requirement blocks remote parity.
Verify: parity replay fixtures in `component_contracts`, pointer plus remote focus tests in `home_component_navigation`, installed TV/desktop captures and both standing fixtures. Evidence: [studio-nested-editing-2026-10-20.md](studio-nested-editing-2026-10-20.md). Frozen snapshot 48, archive `3e349c473149ae6ad53e056fdb895296755e68ea6be43e73925042fbdae238cd`; native component and navigation suites pass, including nested remote/pointer/numeric byte parity, hierarchy/cycle/depth/lock/refusal, atomic subtree/group/page-copy/undo, named-page Back recovery, camera no-write and internal-spacing repair. Full CTest 14/14 passed (64.50 seconds). Installed release `a4120c77640b16fbae8b0756`; expanded Studio journey `/tmp/isolde-components-acceptance-c1qahjv0`, dark `/tmp/isolde-reference-config-obl_i1qy`, pastel `/tmp/isolde-reference-config-m59s6p9b`; all twelve screenshots explicitly reviewed. Durable evidence `build/studio-installed-48/`. Normal Household service/executable restored active/running, zero restarts. No physical HDMI/audio qualification, full-product offline Generator or genuine user/cutover acceptance is claimed; those remain their owning tasks.

## T21 — Contextual inspector, catalog and capability binding (L, depends: T20)
Additional required Entry doc: `docs/design/page-studio/component-contracts.md`. Additional Entry files: `config/skins/pastel.json`, `tests/visual/home-pastel-reference/recreation-config.json`, `config/studio-defaults.json`, `qml/StudioCatalog.qml`, `qml/StudioInspector.qml`, `qml/PlacedComponents.qml`, `src/home/StudioSession.cpp`, `src/home/ComponentSchema.cpp`, `src/home/PageData.cpp`; follow existing registry/Kodi provider interfaces only as required by the selected slice. The five checked gates below retain earlier checkpoint credit; all nine new unchecked gates are required before the revised task is complete. Additional required Entry doc: `docs/design/page-studio/usability-audit-2026-10-20.md`.
Entry docs: `docs/page-studio-redesign.md` sections 6-8; T15 operation-to-control coverage map; `docs/components.md` action/source contracts. Required expansion: `docs/design/page-studio/usability-expansion-2026-10-21.md` (owning gates in its table). Control requirements: `docs/design/page-studio/control-audit-2026-10-21.md` (owning rows only).
Entry files: `qml/AppearanceEditor.qml`, `qml/HomeDesignEditor.qml`, `qml/ComponentLayoutEditor.qml`, `src/home/ComponentSchema.{h,cpp}`, `src/home/ActionDispatcher.{h,cpp}`, `src/home/PageData.{h,cpp}`, `src/home/PageModel.{h,cpp}`, shared Studio/edit-session files.
Scope:
- [x] T21.1 Implement a visual catalog starting with Button, Button carousel, Media shelf, App bar, Image and Widgets, backed by registered schema/renderer/editor contracts and honest unavailable states; content choice then full-page ghost placement is draft-only.
- [x] T21.2 Implement contextual Content/Appearance/Layout/Actions/Accessibility windows, basic/advanced disclosure, inherited versus overridden values, reset and mixed states. Include page colors, solid/gradient/managed-image background, dimming/crop and readable contrast feedback; no permanent inspector shrinking the page.
- [x] T21.3 Provide remote artwork/crop/focal, text, appearance and named source/action/page pickers. Buttons and images share On select: None/Open page/Supported action with Change/Clear, accessible interactive-image labels/focus and decorative-image focus exclusion. Validate stable page IDs and registered dispatcher capabilities; no raw IDs, executable links or required file chooser.
- [x] T21.4 Bind real bounded Kodi data through existing adapters, freeze preview data during gestures, avoid appearance-driven refetches and keep clearly labeled sample/unavailable states separate from real progress/playback.
- [x] T21.5 Cover every supported legacy edit in the T15 feature map; reject unknown versions/types/actions/unsafe assets and provide repair links without adding T02 curation or claiming new device backends.
- [ ] T21.6 Implement genuine App/Button carousel collection membership, All installed/Selected app modes, remote ordering, configurable visible capacity, scoped More apps/More buttons galleries, dynamic registry changes and exact focus return. No generated per-app page placements or silently truncated collections; validate explicit bounds and source/action identities.
- [ ] T21.7 Implement Recently viewed as real bounded Kodi viewing history plus explicitly selected supported app providers; separate count/capacity, honest unsupported/auth/offline states, deterministic merge, independent instances/profile teardown and zero disabled-provider requests. Recently added and sample artwork do not satisfy this gate; no parallel media/progress DB or browser-history scraping.
- [ ] T21.8 Expose the clarified catalog: first-class Shape decoration/container, Button application/page/action links, Image/GIF with optional identical links and bounded reduced-motion-aware decoding, independent Text, and optional shared visible labels distinct from content/editor/accessibility names. Retain Media shelf/implemented widgets; verify remote editing, layers, focus exclusion, inheritance and atomic staged assets.
- [ ] T21.9 Rebuild packaged Pastel sky and its fixture using real app/recently-viewed collections and generic shapes/buttons; verify saved component structure and live data/overflow/editing behavior alongside both visual recreation/reset pairs. Deterministic fixture providers must feed production components; no hand-placed history/app imitation, startup rewrite of user layouts, or visual-only completion claim.
- [ ] T21.10 Implement a shared visual color picker for component/page/Studio/gradient fields: named swatches, theme/preset/recent choices, remote and pointer wheel, brightness/opacity, visible before/after and contrast, inheritance/reset, canceled-preview rollback and one accepted history entry. Hex/RGB/HSV are optional Advanced controls, never required color selection. Required Entry doc: `docs/design/page-studio/usability-audit-2026-10-20.md`.
- [ ] T21.11 Put width/height and proportional controls in Edit component → Size & layout with live preview, bounds, cancel and history parity; distinguish outer size from card size/capacity. Apply type-specific basic/advanced disclosure, clear labels/Done versus Apply, single unobscured context, short feedback and exact focus restoration from the usability audit. Verify uncoached discovery during T24; do not claim acceptance from property/schema coverage alone.
Latest size refinement (user, 2026-10-20): primary width/height sliders range from 1 to the full page span (container span for nested editing), with visible re-anchoring rather than a shortened maximum, validated neighbor proposals, remote/pointer adjustment and complete cancellation. Advanced exact values are secondary. This refines T21.11 without adding task credit or changing Overall.
- [ ] T21.12 Share authored Home focus/selection styling with Studio for every component shape and page theme; eliminate forced square/opaque selection backgrounds. Preserve visible art/labels, readable context focus distinction, reduced motion and decorative editor-only selection; verify light/dark/pastel and actual item bounds.
- [ ] T21.13 Make the actual header/title/logo/clock/date/Search/Settings directly selectable and editable, with Header in Layers and Edit header in Page appearance for hidden/empty recovery. Expose validated order/layout/visibility/content/styles using one runtime/editor model; preserve supported actions, permanent Settings/Edit recovery, legacy reads, profile isolation and atomic draft/undo/Apply/export behavior.
- [ ] T21.14 Replace ordinary More tools workflows with named locations for Undo/Redo, Layers/scope, page/container Layout, header, Saved designs, secondary Try interaction and Editor settings; separate destructive Reset interface with honest scope. Preserve every implemented capability and exact remote focus, keep the page visible, and verify discovery without persistent on-screen instructions. Follow the usability audit's placement table; renaming the same mixed list does not satisfy this gate.
Done when: selecting a component exposes relevant understandable controls and a live visible draft result, with complete remote error/retry/focus paths and no live profile writes. Every clarified component contract and all T21.6-T21.14 gates must pass; previous screenshot similarity does not prove genuine component semantics, editable headers, shared selection or discoverable editing. Extend schema/collection/provider/remote tests and record the audit's evidence alongside both visual pairs.
Verify: schema rejection and inspector coverage/parity tests, `kodi_media_client` request-count regressions, `home_component_navigation`, full CTest and both VM fixtures. Evidence: [studio-inspector-2026-10-20.md](studio-inspector-2026-10-20.md), frozen source snapshot 62, guest source `/home/isolde-dev/isolde-runs/studio-implementation-62/source`, installed release `4e78f5b019a4f554a4c6c450`. Full CTest 14/14 suites (136 test functions) passed, including `studioCatalogLinksInspectorAndStagedArtwork`, `studioCatalogAndContextualInspector`, `studioRemotePointerAndInspectorParity(page|group)` and the `kodi_media_client` request-count regressions. Installed studio journey `tools/run-visual-acceptance.ps1 -Case studio` reached `WRAPPER_EXIT=0`; guest evidence `/tmp/isolde-components-acceptance-tmt2lsig` reports `passed: true` with preview-cancel, reopen, collision, resize, bounded-repeat and nested-group/layers guarantees true. Eleven phases captured and acknowledged at 1280x800, all non-blank and pairwise distinct, including the catalog, inspector and content captures this task requires; durable host evidence `evidence/t21-62/studio/`. Lounge and pastel standing fixtures both passed with recreation/reset pairs; the recreation captures match the previously human-reviewed baselines at 99.98% and 99.91% identical sampled pixels, so T21 regressed neither fixture. Normal Household service restored active/running with zero restarts on the same release. Two harness defects were fixed rather than worked around: the X11 error handler in `tools/home-components-acceptance.py` and the missing `ssh -n` in `tools/run-visual-acceptance.ps1` that made the capture acknowledgement block on inherited stdin. The explicit human visual verdict against `reference.jpg` and the studio captures, T24 user/cutover acceptance and T10 physical input qualification are not claimed; T10 still owns qualified hardware editors.

## T22 — Templates, tokens, reusable sections and portability (L, depends: T21)
Entry docs: `docs/page-studio-redesign.md` sections 6-9; `docs/skins.md` existing snapshot/asset bounds. Required expansion: `docs/design/page-studio/usability-expansion-2026-10-21.md` (owning gates in its table). Control requirements: `docs/design/page-studio/control-audit-2026-10-21.md` (owning rows only).
Entry files: `src/home/HomeSkins.cpp`, `src/home/HomePages.cpp`, `src/home/HomeLayout.{h,cpp}`, `src/home/ComponentSchema.{h,cpp}`, `config/skins/`, Studio catalog/inspector/edit-session files, `tests/tst_components.cpp`.
Scope:
- [ ] T22.1 Ship editable compact launcher, scrolling dashboard, poster/sidebar library and minimal/pastel templates; preview current-page versus whole-interface scope and preserve existing pages by default.
- [ ] T22.2 Add shared tokens and reusable sections/symbols with Home -> page -> component inheritance, explicit overrides/reset, named linked-instance impact preview, Edit all/Detach and atomic propagation. Implement minimum Settings/subpage/dialog/keyboard inheritance from designated Home before T24, isolated from draft previews and other profiles; T27 completes remaining shell adoption.
- [ ] T22.3 Implement page/interface import/export with version/action/path/art validation, scope preview and missing-source remap; placeholders stay editable and unknown executable content is rejected.
- [ ] T22.4 Exclude accounts, credentials, media, Kodi databases, machine-specific grants and private absolute paths from portable exports; preserve existing personal skins and source ownership on round trip.
Done when: all four templates can be customized with remote controls; linked changes and portable round trips pass without accidental replacement, privacy leakage or required online access.
Verify: skin/page round-trip, token inheritance, symbol-cycle/propagation rollback, remap and export privacy fixtures in `component_contracts`; installed template selection and both visual fixtures. Evidence: pending; existing three skins are not four-template/reusable-symbol acceptance.

## T23 — Preview, Apply, restore and navigation safety (M, depends: T22)
Entry docs: `docs/page-studio-redesign.md` sections 3, 7-9; `docs/components.md` atomic failure contract. Required expansion: `docs/design/page-studio/usability-expansion-2026-10-21.md` (owning gates in its table).
Entry files: shared edit-session/Studio/renderer files from T16-T22, `src/home/HomeLayout.cpp`, `src/home/HomeSkins.cpp`, `src/home/ActionDispatcher.cpp`, `qml/Main.qml`, `tests/tst_components.cpp`, `tests/tst_home_navigation.cpp`.
Scope:
- [ ] T23.1 Edit the actual full-viewport runtime-rendered draft with immediate visual feedback and no primary Preview button. Optional More -> Try interaction tests internal draft navigation, then Back restores exact selection/edit state; intercept external apps/playback/device/install/network/live-settings side effects honestly.
- [ ] T23.2 Implement complete change summary, changed-page list, error-to-widget focus, baseline conflict handling, single atomic Apply, retained draft on failure and Retry/Discard/Continue choices.
- [ ] T23.3 Integrate bounded versioned backups, restore previous/default scope and original downgrade backup; retain apps/accounts/device settings/Kodi media and never uninstall on reset.
- [ ] T23.4 Verify dirty exit, profile switch, import/template application and empty/removed origin restore exact focus or permanent recovery; preview/cancel/undo leave applied bytes and profile assets unchanged.
Done when: installed edit -> preview -> cancel -> edit -> Apply -> restart and deterministic failure/retry flows are complete, safe and understandable across Home plus a destination page.
Verify: transaction/write-failure/backup/revision/profile tests in `component_contracts`, preview action-denial/focus tests in `home_component_navigation`, full CTest, installed journey and both fixture pairs. Evidence: pending; backend atomic save alone is not full Apply UX acceptance.

## T24 — Installed acceptance, usability and editor cutover (L, depends: T23)
Additional required Entry doc: `docs/design/page-studio/usability-audit-2026-10-20.md`. T24.3's uncoached journeys must include choosing a color without a code, moving into an occupied area while neighbors visibly adjust, finding size inside Edit component, deleting through the original component context and undoing. Record assistance, wrong turns, accidental launches/rearrangements and lost focus; screenshot success alone cannot close these gates.
Entry docs: `docs/page-studio-redesign.md` sections 9-11; T15 feature map/wireframes; `docs/vm-oracle.md`; both visual fixture READMEs. Required expansion: `docs/design/page-studio/usability-expansion-2026-10-21.md` (owning gates in its table). Control requirements: `docs/design/page-studio/control-audit-2026-10-21.md` (owning rows only).
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
- [ ] T27.1 Complete the T22 effective theme resolver: packaged defaults -> designated Home theme -> explicit page/surface/component override. Settings always uses designated Home regardless of entry origin; no migration writes or stale previous-profile tokens. Verify T22 Settings baseline rather than implementing a competing resolver.
- [ ] T27.2 Expose Use Home theme/Custom override plus reset and isolated draft preview where applicable; Settings identifies its Home source. Validate text, focus, input, scrim, status and accessibility roles without scattered palette constants.
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
