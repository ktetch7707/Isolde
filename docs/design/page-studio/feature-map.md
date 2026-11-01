# Page Studio feature map — current editor to Studio (T15.1)

Status: provisional design inventory, 2026-10-19. Code-inspection evidence only; no installed
acceptance run is claimed and no user review has occurred (deferred to T24). Sources: `qml/CustomizeHomeView.qml`, `qml/PageManager.qml`,
`qml/HomeDesignEditor.qml`, `qml/AppearanceEditor.qml`, `qml/ComponentLayoutEditor.qml`,
`qml/SkinLibrary.qml`, `qml/CommandCenter.qml`, `qml/Main.qml`,
`src/home/ActionDispatcher.{h,cpp}`, `src/home/HomeLayout.{h,cpp}` and
[shell-surfaces-design.md](../../shell-surfaces-design.md) §1. Rules: a UI label is not backend
proof — every row cites the code path that performs the operation, and honest unavailable states
are marked as such. This map is the parity checklist T21.5 and T24 retire against; nothing may be
dropped at cutover without an explicit Studio destination.

## 1. Customize Home item operations (`qml/CustomizeHomeView.qml`)

Two-pane workflow: vertical item list (Up/Down) beside a 3-column action grid (Left/Right), with 31
numbered actions (`actions`, line 49) applied to the selected item. Every action writes through
`HomeLayout` immediately; there is no draft, preview or undo. Studio replaces the orchestration,
not the data contract.

| # | Current operation | Backend path (evidence) | Studio destination | Gap / note |
| --- | --- | --- | --- | --- |
| 0-1 | Move earlier / Move later | `HomeLayout::moveItem(id, ±1)` list reorder | Canvas pick-up/drop + Move to cell / before / after | List order only; no grid movement, no collision on move, no group move |
| 2 | Change size | Span editor → `gridCollisions()` → `setItemPlacement()`, always anchored `x:0,y:0` (line 157) | Canvas resize gesture + numeric inspector width/height | Repositions to row 1/column 1 as a side effect; no anchor choice |
| 3 | Remove item | `removeItem(id)` | Context Remove with subtree/route preview + undo | No undo; no recovery flow for removed components |
| 4-7 | Show / hide title, Show / hide type, Show / hide logo, Logo fills tile | `setTileOption(id, "showTitle"/"showType"/"showLogo"/"logoFill", …)` | Inspector Appearance toggles | QML restricts to non-widget items; keep or expose per-variant |
| 8 | Choose built-in art | `setTileAppearance(id, symbol, accent)` | Inspector artwork picker (managed pack) | Reuse managed-asset bounds |
| 9-10 | Choose image / Remove custom PNG | `setTileLogo(id, url)` / `clearTileLogo(id)` via `FileDialog` | Inspector artwork picker with staged import | Pointer-only today; Studio needs remote path + staging until Apply |
| 11 | Toggle Isolde name | `setBrandVisible(bool)` | Page-level inspector Appearance | Page-wide but buried in the action grid; surface as named property |
| 12 | Preview/apply theme | `applyTheme(theme, includeArtwork)` modal | Template/theme picker + Apply pipeline (T22) | Label says "preview"; it applies immediately — misleading, no draft |
| 13-14 | Assign category / Manage categories | `assignItemCategory`, `addCategory`, `renameCategory`, `removeCategory`, `moveCategory` | Sections in Layers + inspector section picker | Section removal reassigns items implicitly; Studio previews affected items |
| 15 | Item opacity | `setItemOpacity(id, value)` slider | Inspector slider | Immediate write per tick; Studio edits draft only |
| 16 | Tile roundness | `setTileRoundness(value)` slider | Shared appearance token (T22) | Page-wide, immediate write |
| 17 | Add clock | `addWidget("clock")` | Add catalog → Information | Catalog tile shows variant/availability |
| 18 | Add tile | `addActionTile(name, type, target, …)` for open-folder/file/url/disc/switch-input + "Open with" app | Add catalog → Navigation/Actions, validated action form | Handler availability comes from AppRegistry; honest empty state exists — keep it in the catalog |
| 19 | Restore defaults | `resetToDefaults()` | Scoped Restore defaults with confirmation + backup (spec §7) | Whole-document, no scope chooser, no undo |
| 20 | Repair / change source | `sourceChoices(id)` + `rebindSource(id, sourceId)` | Inspector → Sources repair list; blocked items stay repairable | Only reachable via the action grid; Studio surfaces `unavailableReason` on canvas |
| 21 | Make it yours (appearance) | `AppearanceEditor` draft → `setAppearance(draft)` on Apply | Shared profile/page token editor (T22; reused by T27) | The only existing draft-then-apply surface; its token set (focus/background/surface/selected/text/muted colors, typographyScale, homeColumns, spacing, tileRoundness, motionEnabled, fontFamily) seeds the shared appearance model |
| 22 | Component layout | `ComponentLayoutEditor` → `setComposition` / `setSectionPresentation`; "Changes save immediately" | Inspector composition groups + dashboard canvas (T18/T21) | Immediate writes; Left/Right double-duty (section select vs value adjust) is a known confusion (see review) |
| 23 | Home skins / Page skins | `SkinLibrary` → `saveSkin`/`applySkin`/`renameSkin`/`deleteSkin` | Templates & saved designs (T22) | Confirmation dialog exists for apply/delete; keep |
| 24 | Design Home / Design page | `HomeDesignEditor` composition/section/card/tab scopes → `setComposition`/`setSectionPresentation`/`setItemPresentation`/`setItemAction` | Inspector Basic/Advanced + dashboard tabs | Richest field set (variants, crops, actions, library bindings); migrates as schema-backed inspector fields. Nested `action.*`/`trailingAction.*` IDs are raw text today — Studio replaces with named pickers |
| 25 | Pages | `PageManager` → `createPage`/`renamePage`/`removePage`/`linkPage`/`pageModel` | Pages drawer (wireframes §3) | No thumbnails/dirty markers; delete warns about skins only, not incoming routes |
| 26-27 | Add Movies widget / Add Shows widget | `addLibraryWidget(name, kind)` | Add catalog → Media (Kodi-backed) | Availability not shown on the action tile |
| 28-29 | Add text widget / Add button | `addWidget("text")` / `addButton()` | Add catalog → Text & artwork / Navigation | Buttons need the same validated action form as Add tile |
| 30 | Duplicate | `duplicateItem(id)` | Context menu Duplicate | No undo; Studio places the copy adjacently with collision resolution |

| 31 | Page Studio (preview) | HomeLayout::beginStudio() → StudioSession typed commands | Native canvas draft workspace | Provisional implementation entry; legacy editor retained until cutover acceptance |

Modal overlays in the current editor (full-screen; immediate-save except where noted): source
repair, theme preview+apply, built-in art, add tile (+ on-screen keyboard), tile roundness, item
opacity, category manager, span size, and grid collision (Shift other / Swap / Cancel — the one

## 2. PageManager operations (`qml/PageManager.qml`, 17 operations)

| Group | Operations (`operations` array) | Studio destination | Gap |
| --- | --- | --- | --- |
| Navigate/edit | Open page; Edit components | Canvas open + select originating component | Edit components loads the same list/action editor per page (`pageEditor.setSource("CustomizeHomeView.qml", …)`) |
| Create | New blank page; New Movies page; New Shows page; New standalone Movies; Copy Home to page; Rename page | Pages drawer create/duplicate/rename with preset picker (T22) | Presets hard-coded; Studio exposes validated template data |
| Delete | Delete page (confirm overlay) | Pages drawer Remove with incoming-route preview + remap | Warns about skins only; incoming tabs/buttons not previewed (spec §6 requires remap) |
| Link | Add Home tab; Add Home button; Add button to edited page; Add tab to edited page | Named route pickers in Pages drawer + inspector actions | Raw page IDs shown and typed; Studio uses named pickers |
| Library destinations | Use for Movies; Use for Shows; Classic Movies; Classic Shows | Settings-backed destination pickers (unchanged data) | `setComposition` schema edits; keep compatibility |

## 3. Shell surfaces carried into Studio scope (T25-T28)

Not Studio tools, but they share its appearance, action and containment contracts; this map
prevents the Studio catalog from re-encoding the same mistakes.

| Surface | Current behavior (evidence) | Design target | Studio impact |
| --- | --- | --- | --- |
| Detected-source notices | `Main.qml:153-154` prepends/merges `sourceManager.items/sections` into Home sections; `markPermanent`/`hideSource` handlers exist (`Main.qml:174-176`); `SourceSettingsView` has policy controls | Contained configurable notice slot / detected-sources component (T25) | New catalog component + named slot region; per-page policy data; never unmeasured full-width rows |
| All Apps | `ActionDispatcher.cpp:71` emits `drawerRequested` for `app-drawer`; `Main.qml` opens Command Center (Search) | Dedicated installed-app gallery, distinct from Search (T26) | Action picker offers "Open All Apps" separately from "Open Search"; legacy `app-drawer` resolves to the gallery at execution time |
| Themed shell surfaces | `CommandCenter.qml` uses fixed palette literals (e.g. `#F8FAFC`, `#91A0BC`, `#344864`, `#D58A45`) with no appearance input; Settings/Kodi views have partial bindings | One theme-resolution contract for every Isolde surface, dialog and keyboard (T27) | Studio's shared token editor (T22) is reused by T27; Studio must not fork a second resolver |
| Web search switch | `CommandCenter.resultItems()` inserts `kind: "searchToggle"` pseudo-result (line 106), activated like a result (line 237); default OFF via `SearchSettings.webEnabled` | Genuine labeled header switch, top-right default, outside result data/counts (T28) | Not a Studio catalog item; the composition `headerControls` enum must gain a switch slot rather than a results hack |

genuine collision dialog, exercised by `tests/tst_home_navigation.cpp:676-683`).

## 4. Backend contract reused as-is (no new backends implied)

`HomeLayout` Q_INVOKABLEs are the validated data contract Studio orchestrates: items
(`addAppTile`, `addActionTile`, `removeAppTile`, `addWidget`, `addButton`, `addLibraryWidget`,
`moveItem`, `removeItem`, `duplicateItem`), presentation (`setItemSize`, `setItemOpacity`,
`setTileOption`, `setTileAppearance`, `setTileLogo`, `clearTileLogo`, `setItemPresentation`,
`setSectionPresentation`, `setBrandVisible`, `setBackground`, `setTileRoundness`, `setWallpaper`,
`setAppearance`, `applyTheme`, `resetToDefaults`), structure (`addCategory`, `renameCategory`,
`removeCategory`, `moveCategory`, `assignItemCategory`), pages (`createPage`, `renamePage`,
`removePage`, `pageModel`, `linkPage`), grid (`grid`, `setGrid`, `setItemPlacement`,
`clearItemPlacement`, `gridCollisions`), sources (`sourceChoices`, `rebindSource`), skins
(`saveSkin`, `applySkin`, `renameSkin`, `deleteSkin`) and composition (`setComposition`). The
grid/placement data contract and the `ComposedHome` placement renderer already exist (M1/M3
evidence in the backlog T05 rows); Studio T17-T19 build on them rather than adding a second
geometry system. Validation, bounds (4 MiB / 1024 components / 128 categories / 64 pages per
profile), atomic save with backup and rollback, and profile isolation stay in C++.

## 5. Capability gaps the Studio must close (summary)

1. **No draft pipeline** — every current operation persists immediately; only AppearanceEditor has
   a local draft. Studio: one profile-scoped edit session, Apply once (T16/T23).
2. **No undo/redo** anywhere; Restore defaults is the only escape hatch and it is total.
3. **No spatial editing** — movement is list order; the grid backend exists but only the span
   dialog uses it, always anchored at 0,0.
4. **No canvas selection** — the edited page is never visible while editing; item identity comes
   from a list row.
5. **No layers, groups, containers or reading-order editing**; categories are the only hierarchy.
6. **No catalog** — adds are action-grid cells with no availability/variant presentation.
7. **No templates/symbols/portability** — skins are the nearest feature (single-profile
   snapshots).
8. **No preview** and no per-surface scope indicator; nested page editing hides its scope.
9. **No remote-friendly artwork/path input** — `FileDialog` and free-text IDs assume a pointer or
   raw knowledge; Studio needs named pickers, staged assets and the on-screen keyboard everywhere.
10. **Misleading labels** — "Preview/apply theme" applies; "Change size" relocates to 0,0; "Open
    All Apps" currently opens Search. Studio's data pickers must state actual consequences.

Everything in sections 1-3 must have a reachable Studio home before the old editor is retired
(T24 cutover); anything intentionally dropped requires an explicit backlog decision, not silence.

