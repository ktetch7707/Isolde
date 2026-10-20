# Page Studio — visual page builder

Additive requirement: [shell-surfaces-design.md](shell-surfaces-design.md) adds T25-T28 before final Studio integration: configurable detected-source containment/policy, dedicated All Apps routing, shared cross-surface theme inheritance, and a genuine top-right Web search header switch. See the backlog for execution order. Current active count is 24 (11%); the section 12 check below records the earlier 20-task revision, not current arithmetic.

Status: implementation specification, 2026-10-19; no application code changed by this planning revision. The user rejected the current Customize Home workflow and requested a powerful, intuitive replacement inspired by visual web builders. This specification governs the replacement editor in Home and authored pages. It supersedes the old T06–T08 editor work packages and the old scattered immediate-save editing workflow. The remaining Studio requirements still apply unless explicitly narrowed here.

## 1. Product decision and inspiration

One entry point, **Edit page**, opens the actual page on a large canvas. Pages, layers, adding widgets, inspecting a selection, previewing and applying are parts of this workspace. Existing Home settings entry points route to the relevant Studio tool after cutover. Ordinary browsing stays simple; no customization is required.

Patterns adapted from primary design documentation:

- [Webflow canvas](https://help.webflow.com/hc/en-us/articles/33961319255059-Webflow-canvas-overview): direct selection, named outlines, parent breadcrumbs, movement indicators and an uncluttered preview. Isolde uses these patterns inside its native Qt UI.
- [Webflow grid areas](https://help.webflow.com/hc/en-us/articles/33961389959059-Grid-areas): named regions and visible spans. Isolde requires explicit collision resolution and never generates extra rows to accommodate an invalid drop.
- [Framer layout grids](https://www.framer.com/help/articles/layout-grids/): visible alignment guides with separate column, gap and margin controls. Guides and zoom belong to the editing view, not to the saved page content.
- [Atlassian drag accessibility](https://atlassian.design/components/pragmatic-drag-and-drop/accessibility-guidelines/): provide non-drag movement controls and meaningful feedback. Isolde supplies both remote pick-up/drop and explicit Move to cell/Move before/Move after actions.

These are interaction references, not framework/dependency recommendations. No browser editor, hosted service, cloud account, HTML/CSS interpreter or third-party builder runtime is introduced.

The current editor's problem is structural: a list of items beside a long action grid separates editing from the visible page, mixes page-wide and item actions, and saves operations independently. Adding more numbered actions cannot deliver this design. Reuse the data, native rendering, appearance controls, Kodi adapters and validation; replace the orchestration and navigation.

## 2. Workspace and visual hierarchy

Desktop/wide-screen schematic (labels illustrate configurable content):

```text
+--------------------------------------------------------------------------+
| Page Studio | Home v | Edit: Home / Source grid | Draft: 3 changes         |
| Add | Pages & layers | Undo | Redo | Grid | Fit | Preview | Apply          |
+----------------+--------------------------------------+------------------+
| PAGES          |  TV canvas: current device aspect     | SOURCE GRID      |
| Home           |  Safe-area outline / grid guides      | Layout           |
| Library        | +----------------+-----------------+  | Content          |
| My page        | | Hero           | Continue        |  | Appearance       |
|                | |                | watching        |  | Actions          |
| LAYERS         | +----------------+-----------------+  | Accessibility    |
| Header         | | [ Sources: selected, 8 x 1 ]     |  |                  |
| Hero           | +----------------------------------+  | Row / column     |
| Continue       | | Guide                            |  | Width / height   |
| > Sources      | +----------------------------------+  | Gap / padding    |
| Guide          |                                      | Reset override   |
+----------------+--------------------------------------+------------------+
| Editing draft | Sources: row 3, column 1, size 8 x 1 | Help / errors      |
+--------------------------------------------------------------------------+
```

TV/remote schematic:

```text
+--------------------------------------------------------------------------+
| Edit Home | Add | Pages | Undo | Grid | Preview | Apply | Draft            |
+--------------------------------------------------------------------------+
|                                                                          |
|                 Large fitted preview of the actual page                  |
|                 Named selected widget / visible empty cells              |
|                 Dashed origin + moving ghost when picked up               |
|                                                                          |
+--------------------------------------------------------------------------+
| MOVING Sources | Row 3, column 1 | 8 x 1 | Valid position                  |
| Arrows move one cell     OK drop     Back cancel     Menu more            |
+--------------------------------------------------------------------------+
```

At TV sizes, show one Pages/Layers/Add drawer or inspector sheet at a time. Keep the canvas and operation feedback visible; return closes the drawer and restores its exact component. Do not squeeze three desktop panels onto a TV. Canvas fitting only changes the editor camera; it must not reflow or rewrite the page. Provide a close-up of the selected widget when the fitted canvas would make it hard to inspect.

The top bar always names the profile, page, draft state and nested editing scope. The footer shows the current operation and applicable buttons. Page-level tools live in the top bar; widget actions live next to the selected widget or in its context menu. Basic controls appear first; Advanced expands the same inspector rather than opening another editor.

Page content can be colorful or unreadable while experimenting. Editor chrome uses shared, configurable accessibility/appearance tokens independently of the draft preview, with validated readable focus/contrast. A bad draft must never make Apply, Discard or recovery unreadable.

## 3. Start-to-finish workflow

1. Edit Home or Edit this page opens a draft of the active profile's complete interface document and selects the originating component. Opening the editor writes nothing.
2. Start from the existing page, choose a template, or create a blank page. Blank pages show a real grid and a prominent Add widget action; Settings/recovery remain reachable.
3. Add a widget from a visual catalog. Its tile states what it does and whether its source is available. Choose a valid cell/region with the ghost before inserting it into the draft.
4. Move/resize directly on the canvas. Open the contextual inspector to change content, styling or action. Every accepted operation updates the live draft and its undo history.
5. Preview runs the draft through the same native renderer, without editor decorations. Return restores the selected widget and camera. Try navigation is available separately from side-effecting actions.
6. Apply shows a short change summary, validates the complete document and managed assets, then saves once atomically. Errors retain the draft and the previous applied design, with Retry and Return to editing.
7. Leaving a dirty workspace offers Continue editing, Discard draft or Apply. Back first cancels an active gesture, then closes the current tool, then reaches this workspace-exit choice. Nothing implicitly applies on Back.

Changes to multiple pages remain one interface draft so creating a page and linking it cannot leave a half-saved route. The workspace header says when other pages have changes; Apply lists every affected page. Never suggest that only the visible page is being saved when more will change.

## 4. Remote interaction contract

No long press, pointer emulation, physical number keys, hidden keyboard shortcut or drag gesture is required. D-pad, Confirm, Back and Menu/Search complete every operation. The default canvas is in Edit mode: selecting a media card cannot launch it.

| State | D-pad | Confirm | Back | Menu / Search |
| --- | --- | --- | --- | --- |
| Canvas browsing | Spatially focus components; Up at the top reaches toolbar | Pick up focused component for Move | Exit workspace choice if dirty | Menu opens contextual actions; Search opens Add/catalog search |
| Moving | Move the ghost exactly one cell, maintaining its span | Drop if valid; occupied cells open collision choice | Cancel movement; origin and draft unchanged | Choose Move to cell or a permitted destination container |
| Resizing | Left/Right changes width; Up/Down changes height; top-left stays anchored | Accept candidate size or resolve collision | Restore size from before gesture | Choose another resize anchor or numeric size via on-screen keyboard |
| Selecting drop container | Focus named eligible containers | Enter that container's grid; show destination breadcrumb | Return to previous scope | Show allowed destinations as a list |
| Inspector or drawer | Navigate labeled controls; adjust only the active field | Activate/edit field | Cancel field edit, then close panel | Context help; catalog search only when relevant |
| Collision choice | Choose Shift, Swap or Cancel and inspect affected widgets | Commit the selected valid proposal to the draft | Cancel candidate placement | No hidden alternative behavior |
| Preview / Try navigation | Follow actual page focus routes | Internal navigation; intercepted external actions explained | Return to editor at exact origin | Restore preview tools; no route into live settings mutation |

The context menu contains Move, Resize, Customize, Duplicate, Hide, Remove and Enter contents when applicable. A separate Move to row/column form and Move before/after list make dragging optional. Menu Move and direct OK pick-up produce the same candidate operation. Remote key repeat is bounded; it never skips occupied cells, commits a candidate or auto-scrolls indefinitely. Hitting an edge keeps the last valid candidate and explains the boundary; typed invalid values are refused, never clamped.

During movement show the original outline, the ghost, selected name, row/column and span, and the reason a drop is unavailable. Use outlines, hatching, text and accessible announcements as well as color. Announcements describe the resulting position, collisions, successful drop and cancellation. Occupied cells may be traversed by the ghost; they cannot be silently overwritten. Add/Move across a long page auto-scrolls only inside the existing bounded content extent and keeps the candidate visible.

Example: pick up a 2×1 Clock, move three cells right, drop. The ghost moves on each press; the actual draft changes only on drop. Back after two presses restores the original Clock with no undo entry and no file write. A successful drop creates one undo entry regardless of repeat count or mouse motion events.

## 5. Collisions, grouping and layout rules

- Default root grid stays within 4–16 columns and 3–10 rows. UI coordinates are one-based; stored cells remain zero-based integers. Changing columns/rows shows all affected placements before committing; out-of-bounds widgets require an explicit arrangement or Cancel.
- One-screen pages use a bounded root grid. Scrollable pages use an explicit ordered stack of bounded section grids rather than silently adding rows past the root limit. Extent, scroll mode and section insertion are editable data. Preserve intentional empty cells.
- The grid's inner rectangle accounts for page safe area and reserved header/navigation space once. Canvas, runtime, hit testing, focus and inspector use the same geometry. Zoom, fit, pan, guides and selection are editor camera state only. Test actual device aspect plus 720p/1080p/4K; preserve existing legacy design-height behavior on read.
- Shift displays a deterministic complete proposal for every affected widget. Use stable row/column/ID ordering, preserve unaffected placements and refuse a cascade beyond 64 affected nodes. If no valid proposal exists, disable Shift with the reason. Never move one collider on disk and then fail to place the selected widget.
- Swap is enabled only for one eligible occupant when both resulting rectangles and container rules validate. Different spans are allowed only if both fit. Cancel restores every placement and focus.
- Moving a group or resolving a collision is a single validated transaction with one undo record. Group selection is explicit in Layers and remote context actions, never dependent on modifier keys. Limit one manipulation to 32 selected nodes and refuse additional selection with an explanation.
- Containers have named scopes and visible breadcrumbs. Move a container moves its children intact. Enter contents edits the local arrangement; Back returns to the parent selection. Refuse cycles, unsupported children and out-of-scope drops. Bound container depth to four and count all nodes against the existing 1024-component limit.
- Widget-internal arrangements such as an eight-input 4×2 HDMI panel are a separately validated presentation sub-grid, not a change to the page-grid bounds. Sub-grids are bounded to 1–16 columns and 1–10 rows and cannot host executable content. Existing unavailable hardware remains unavailable.
- Lock position and Hide are distinct, reversible states. A locked component is selectable in Layers but cannot be moved until unlocked. Hidden components stay named/editable in Layers with a reveal-in-editor view. Runtime navigation omits hidden components; editor navigation can always reach them.

## 6. Pages, layers, catalog and inspector

Pages drawer: thumbnail, name, Home designation and dirty indicator. Create, duplicate, rename, reorder and remove pages; choose start page and entry routes with named pickers. Removal previews incoming tabs/buttons/library overrides and offers remap or Cancel. No raw IDs, broken hidden routes or silent page deletion. Respect 64 pages/profile. Search finds a page by name.

Layers: selectable named hierarchy synchronized with the canvas; expand/collapse containers, reorder within permitted parents, reveal hidden items, lock/unlock and choose multiple items. Display and edit reading/focus order explicitly when it differs from visual placement. Changing layer order must not unexpectedly change cell positions. Breadcrumbs select parents without requiring tiny hit targets.

Add catalog: useful visual categories (Layout, Navigation, Apps & inputs, Media, Text & artwork, Information), search and short configurable examples. These are catalog organization, not forced page categories. Offer only registered component capabilities. Provide compact/poster/landscape/circular variants through shared presentation data. Every new component type needs its boundary schema, editor adapter, runtime renderer, defaults, unavailable state and tests before appearing as usable.

Inspector tabs:

| Tab | Required controls |
| --- | --- |
| Layout | Position, span, anchor, gap, margin, padding, fit/scroll, alignment, container and internal grid, lock/visibility |
| Content | Named source picker, compatible Kodi views, sort/count/density, artwork source and crop, labels, empty/loading/error presentation |
| Appearance | Shared tokens/presets, colors and readable contrast preview, typography, opacity, corners, focus outline, spacing and supported motion/reduced-motion fallback |
| Actions | Named page/app/action picker, supported trigger, destination preview and validation; no executable expressions or shell |
| Accessibility | Accessible name/hint, focus/reading order preview, visibility in runtime, contrast/text-size warnings and recovery route |

Controls show inherited values versus local overrides and offer Reset override. Multi-selection shows common/mixed values; incompatible edits are unavailable with an explanation. Crop has a remote focal-point/zoom tool and a mouse equivalent; do not require a desktop file chooser to complete artwork selection. Managed-art library/import uses existing permission boundaries. Unconfigured sources show Setup/Choose source; missing mappings remain editable. T02 curation remains excluded: bind Kodi-owned data and existing collections, do not create new collection editors.

Initially supported shelves/pages use real bounded Kodi adapters. Pause data refresh while manipulating and use an immutable preview snapshot; dragging or changing appearance must not refetch media. Clearly labeled sample data is available for design and never saved as real media/progress. Default templates do not advertise unavailable providers as entitled or hardware as working.

## 7. Shared model and safety architecture

Keep `HomeLayout`, `PageModel`, `HomePages`, `HomeSkins`, `ComponentSchema`, `PageData` and `ActionDispatcher` as the authorities. Add an in-process edit-session/command helper only where needed; no parallel persistent layout file, media store, service, web framework or application rewrite. Implement the editor shell in native QML over these contracts.

The edit session owns the baseline revision, draft document, selected stable IDs, operation candidate, validated proposal and bounded undo/redo history. Input adapters issue the same typed commands: add, move, resize, set properties, resolve collision, group, reorder, link and delete. Validators evaluate the complete proposed result. Apply all affected nodes to the in-memory draft together, or none. All UI surfaces observe that one draft.

Remote/pointer/inspector parity means identical canonical layout bytes for the same starting document and semantic commands, not merely similar screenshots. Centralize ID creation and serialization. Test with the same deterministic fixture identities; do not include zoom, panel sizes, pointer positions or wall-clock metadata in layout bytes. Source connections and focus origins use stable IDs, not list indexes.

Persistence: validate full draft → stage new managed artwork and backup → write temporary layout → fsync/atomic replace → publish new applied model. On any error, retain the previous layout bytes, applied in-memory state and usable draft. Draft gestures, property previews, undo, cancel and workspace switching do not modify applied layout/profile assets. Imported artwork is staged outside the applied profile until Apply; abandoned staging must not delete shared artwork. Keep original v1 backup; do not migrate or rewrite on read/startup.

Undo/redo is bounded to 100 accepted operations and 32 MiB retained history. Before an edit would exceed the budget, explain the limit and offer Continue with a new history baseline or Cancel; do not silently lose undo or partially accept a command. Applied backups keep up to ten versions within 40 MiB of layout data, with a visible retention policy; referenced assets use existing managed-asset bounds. Validate existing 4 MiB/1024-component/128-category/64-page limits before accepting growth.

Only one active profile edit session may write its interface. Profile switch destroys/invalidates preview clients and the old draft after Apply/Discard choice. A baseline-revision conflict blocks Apply and offers comparison/reload/copy; never overwrite a newer design. Removing a component/container previews the complete subtree and affected routes; Undo restores IDs and references. Restore defaults affects the chosen layout scope only and never uninstalls apps or changes credentials/media/device setup.

## 8. Preview, navigation and applying

Preview uses the actual renderer and full device viewport with editor chrome hidden. Show a small persistent Draft preview label and a guaranteed Back return. Try navigation visualizes/executes only allowed internal page navigation in a sandboxed preview context. Playback, app launches, network discovery, device commands, installation, disk writes and live settings mutations are intercepted with an honest Would invoke action explanation; it is not playback acceptance. Cached/offline media reads stay within existing bridge limits; Web OFF issues zero external discovery requests.

Run preflight checks for invalid placement, overflow, missing actions/sources, unreachable focus, tiny/obscured labels and removal consequences. Blocking structural errors prevent Apply; nonblocking unavailable sources/contrast issues have clear locations and repair controls. Selecting an error focuses the corresponding widget/control. After Apply, show Applied, close only when requested, and restore the exact originating runtime page/component or the permanent recovery control if removed.

Templates, saved designs and imports all enter this draft pipeline. Applying a template offers Current page or Whole interface scope with a changed-page summary. Ship compact launcher, scrolling dashboard, poster/sidebar library and minimal/pastel starting points; existing designs remain available. Reusable sections/symbols show linked instances and explicit Edit all versus Detach; propagation is previewed as one transaction. Theme tokens have consistent inheritance and per-instance overrides. Portable imports validate versions/types/paths/actions/assets, remap missing sources by name and retain repairable placeholders. Exclude accounts, credentials, media, Kodi DBs, machine-specific grants and private absolute paths from portable exports.

## 9. Legacy compatibility and rollout

Existing no-grid/flow pages open visually unchanged and remain editable. Adapt them into the same canvas geometry/selection contract in memory. An optional Convert to grid previews the full result and preserves IDs, routes, content and artwork; inability to preserve a property is a blocking report, never a silent approximation or flattened image. Conversion writes only on explicit Apply and retains the original backup.

Use a temporary developer flag while building, default off. New editor entry points are enabled only after the acceptance gate; the existing editor remains a fallback until then. Both may not independently write a page during an edit session. At cutover, ordinary Settings/Edit Home/Edit page and context actions open Studio. Retire duplicate action-grid/inspector flows after parity evidence, not before. Do not run both editing systems indefinitely or require a feature flag for the finished experience.

## 10. Acceptance contract and evidence

All implementation tasks are unchecked until their own done gate and shared regressions have evidence. New proposed test names are requirements, not tests claimed to exist today. Extend `tests/tst_components.cpp`, `tests/tst_home_navigation.cpp` and relevant Kodi/input tests; use the [VM oracle](vm-oracle.md) with source hashes and the exact installed release. Keep the current 14-suite baseline and record the actual suite count if it changes.

| Journey / failure | Required evidence |
| --- | --- |
| Blank → add Clock and app row → move → resize → preview → Apply → restart | Remote-only installed recording/screenshots, persisted/reloaded canonical document and visible focus throughout |
| Pick-up → traverse occupied cells → Back | Applied and draft documents byte-identical to gesture start, no history/file write |
| Shift/swap success; multiple colliders; impossible/full grid; induced save failure | Complete proposal, deterministic geometry, one undo record; no partial move/save; actionable error |
| Remote versus mouse versus numeric inspector | Byte-identical canonical documents, same validation and collision outcomes from identical semantic edits |
| Nested input panel and multi-selection | Explicit scope/breadcrumb, local versus parent movement, lock/hidden recovery, group atomicity and bounds |
| Existing pages/skins and invalid stored layouts | Original-byte preservation on open/cancel; explicit conversion/apply/backup; invalid data fallback without rewrite |
| Page removal, route repair, profile switch and revision conflict | Exact affected-route summary, stable IDs, no cross-profile writes or stale clients, restored focus |
| Offline and missing backends | Designer/catalog/local managed artwork/draft/apply/recovery work disconnected; Web OFF network-denial evidence; honest unavailable states |
| Preview versus runtime | Same inner rectangle, aspect, crop and widget geometry; internal focus/navigation test; zero unintended side effects |
| Both standing visual references | Real recreation/reset screenshot pairs and explicit review from the same generic renderer, no fixture special cases |
| Editor replacement | Existing supported controls mapped to new locations; no hidden lost functionality; old entry points route to Studio; normal service restored |

Measure on the documented VM baseline at 1280×720 and 1920×1080; inspect 4K scaling and existing 1280×800 legacy fixtures. Targets: visible response to a remote step within 100 ms at p95; at least 30 fps during movement of 100 visible widgets; a 1024-node document remains editable using virtualized layers; no input wait over 250 ms without feedback. Record hardware, sample length and profiler evidence; VM results are not target-hardware qualification. Repeated open/cancel/preview/profile switches must not grow clients/history without bound.

Usability acceptance observes the user or representative users completing add/move/resize/style/undo/preview/apply and recovering from a collision without implementation-author coaching. Target: enter movement within two confirmations from a selected component, zero accidental media/app launches, and no lost edit/focus. Record completion, time, wrong turns and help needed; a failed walkthrough requires iteration. The new workspace is not accepted merely because its author can operate it.

## 11. Delivery map and scope boundaries

The core backlog is T15–T24, integrated with T25–T28 from the additive shell-surfaces specification, in [agent-backlog.md](agent-backlog.md): design validation; draft transactions/history; common canvas geometry; workspace shell; remote manipulation; desktop/nesting parity; inspector/capability binding; templates/tokens/portability; preview/apply/restore UX; integrated acceptance/cutover. Draft safety precedes manipulation. Each task has its own entry documents/files and verification gate; do not read/rewrite the whole product as an editor implementation strategy.

Old T06 maps to T17–T21/T24; T07 maps to T16/T23/T24; T08 maps to T22/T23/T24. T05's remaining placement geometry gate is verified during T17, not assumed complete. T01/T03/T04 and remaining product/hardware/release work stay active outside the user-selected Studio priority track. T10 still owns actual device configuration and hardware qualification; Studio may expose unavailable controls honestly but cannot manufacture support. Optional advanced runtime families from the older Studio proposal (for example new guide/recording backends, a general variable/action-sequence engine, or new decoder-dependent backgrounds) need their own capability tasks before entering the catalog; they are not implied by implementing a generic editor.

This revision authorizes planning only in this chat. No VM update, deployment, application implementation or new dependency is part of the planning change.

## 12. Original Page Studio planning revision verification (historical)

Checked the documentation-only diff, local Markdown links, scope mappings, active-task checkbox percentages and dependency graph. There are 20 active tasks with percentage sum 275, giving 14% overall; T15-T24 remain 0%. No active dependency points to struck/superseded work and the graph has no cycles. Every new task has entry documents/files, scope, a completion gate and verification/evidence requirements. Check log: `C:/Users/kketc/.codex/visualizations/2026/10/19/01a15691-1cd3-77d0-928f-854e1c7a566b/page-studio-planning-20261019.log`. Application tests and VM runs were not repeated for this documentation-only revision. These checks verify the plan's consistency, not editor implementation or usability.
