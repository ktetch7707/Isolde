# AGENTS — Isolde coding agent entrypoint

> Persistent instruction. Do not delete.

1. On session start, read `docs/agent-backlog.md` — it is the single source of truth for sequencing and percent completion.
Before VM actions, read `docs/vm-oracle.md`. Explicit user-requested architecture/planning/testing reviews may inspect relevant plans and evidence without implementing the next feature. Preserve unrelated working-tree changes.

2. Pick the lowest-numbered task whose `status` is not `100%`. Do not skip ahead unless its `Depends` tasks are `100%`.
3. Within a task, read ONLY its `Entry docs` + `Entry files`. Do not re-read the entire codebase or all planning docs.
4. Implement its `Scope` to satisfy its `Done when` gate, preserving standing gates (both visual fixtures, profile isolation, atomic save/rollback, Kodi media ownership, offline Generator Test).
5. Before finishing, run its `Verify` commands/tests and update `docs/agent-backlog.md`: check off sub-boxes, recompute that task's `%`, recompute `Overall %`.
6. Never mark `100%` without evidence paths (test logs, fixture screenshots, `/tmp/...` evidence). `100% x 13 active (T02 struck) = plan implemented`.
7. Sequencing authority: `docs/implementation-order.md`. Requirements: `docs/unified-master-plan.txt`, `docs/studio-builder.md`, `docs/master-roadmap.md`.
8. Core thesis — configurability + UI. Everything is configurable; nothing requires configuration. The default must be polished and TV-ready, and every tab, shelf, widget, color, density, and navigation choice must be rearrangeable via validated data. Layout is data, never executable QML/JS/shell. A theme/skin is declarative data, never code.
9. Configurability commands (must obey):
   - NEVER hard-code a layout, destination, tab, color, or category into QML/C++. Add it as validated schema + editor control + default preset value.
   - NEVER special-case `tests/visual/home-reference` or `home-pastel-reference` — both must pass from the same generic renderer.
   - NEVER rewrite or migrate a user's stored layout on read/startup. Load v1 compat, write v2 atomically, keep backup. Invalid stored docs fall back to packaged defaults without rewriting the file.
   - Every new visual component MUST expose its presentation choices through the shared appearance/composition model (size, art source/crop, labels, opacity, corners, focus, spacing). No scattered fixed values.
   - Validate at the boundary: `ComponentSchema` for data, `ActionDispatcher` for execution. Reject unknown versions/types, unsafe IDs/paths, overflow/collision, non-managed artwork. Surface failures via `errorString()` with atomic rollback.
   - Bound everything: layout 4 MiB / 1024 components / 128 categories; pages 64/profile; grid 4-16 x 3-10; artwork type/size limits. Warn and refuse invalid, never clamp silently or overwrite placement.
10. UI commands (remote-first TV product):
    - Every workflow MUST complete with D-pad + Confirm + Back + Menu/Search only. Desktop drag/inspector/zoom must produce byte-identical layout data to remote editing.
    - Focus MUST always be visible, restorable, and never stranded: restore exact component/tab origin after launch/playback/dialog; Up from first row reaches Settings/Edit Home; empty Home still reaches recovery; preview/cancel writes nothing.
    - TV-distance legibility: readable type, contrast, safe-area margins, no obscured field, no autoplay/ads/feeds in default. Reduced-motion and contrast options respected.
    - Editor UX: selected item, operation, edit mode, preview vs applied state, and consequences of remove must always be unambiguous. Collision = shift/swap/cancel dialog, never silent overwrite.
11. Engineering best practices (must obey):
    - Small vertical slices: remote input → data → visible result → error/retry → focus restored → test. One task at a time, lowest-numbered ACTIVE first.
    - Profile isolation: edits affect active profile only. No cross-profile reads/writes. Teardown invalidates clients.
    - Kodi owns media/progress/playback/artwork; Isolde owns presentation/navigation. No parallel media DB, no scraper, no Kodi DB/skin writes, no fake playback claims. Bounded read-only bridge calls (50/page).
    - Offline-first: Generator Test (cold boot, local media, saved art, local search, layouts, recovery) passes disconnected. Online features degrade independently; Web OFF issues zero external requests.
    - Atomicity: validate → write temp → fsync → rename → rollback on failure. Keep deterministic write-failure regression passing.
    - Tests before done: extend `tst_components` / `tst_kodi_media` / QML offscreen suites for each slice; run `ctest --test-dir build --output-on-failure` + task Verify tools; capture `/tmp/isolde-*` evidence.
    - No new dependencies, frameworks, or side services without task scope. Reuse `HomeLayout/ComponentSchema/HomePages/HomeSkins/PageData/ActionDispatcher`, `KodiMedia/KodiShelves/KodiService`, `InputManager`, `SessionManager`.
12. Committing:
    - One logical change per commit, prefixed with the task ID: `T05: description`.
    - Branch: always commit to `agent-backlog-progress` (not `main`).
    - After committing, push to `origin agent-backlog-progress` and verify: `git log -1 --oneline && git status`.
    - Do not commit to `main`; it is the archived baseline.
    - Never include build artifacts, IDE files, or binary artwork in commits.
    - Keep commit messages short but descriptive; reference docs/task if relevant.
    - Accessibility/security: passwords masked, no credentialed URLs, no shell expansion of Exec lines, content-addressed managed artwork only, explicit consent for disk/install/update actions.
