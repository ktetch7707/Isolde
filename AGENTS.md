# AGENTS — Isolde coding agent entrypoint

> Persistent instruction. Do not delete.

## Autonomous backlog execution — 2026-10-29

When the user says “go”, “continue”, or “work on the backlog”, follow `docs/backlog-execution.md` and resume the current checkpoint in `docs/execution-queue.md`. They define work-item selection, the focused/integration/runtime/acceptance testing ladder, failure diagnosis, evidence reuse and handoff. Read the backlog's current sections and selected task, not all historical records. The existing authorized T11 closure is the current exception; afterward return to Studio priority. Do not ask for routine next-step confirmation. Do not silently skip blocked acceptance or start an unrelated track.

These execution rules clarify the testing frequency below: focused checks after complete edit batches; full CTest at coherent integration/closure checkpoints; graphical tests against the same passing payload; no new full build for documentation or a verified harness-only correction. All task Verify gates remain required before task acceptance. This planning change launches no application work or VM tests.

## Machine boundary — standing user preference (2026-10-21)

The **Isolde-Test VM exists solely to test Isolde**. Necessary cleanup, build/test setup, test data and service changes **inside that guest** are authorized without repeated permission requests. Manage obsolete guest build artifacts proactively when space blocks work; preserve useful evidence and current acceptance data where practical. Verify resolved paths belong to the guest before destructive operations; never follow a shared-folder/mount path into the host. Continue using the bounded VM runner and recording what changed. This permission does not make a failed test a pass or waive verification.

The **Windows host is the user's personal machine and contains valuable data**. Limit host writes to the authorized Isolde workspace and explicitly requested artifact outputs. Do not clean unrelated host files, install host toolchains, change host services/settings/security/networking, or touch personal account data. Guest maintenance permission never authorizes host maintenance. Repository edits, Git operations and task evidence within the workspace remain authorized.

1. On session start, read `docs/agent-backlog.md` — it is the single source of truth for sequencing and percent completion.
Before VM actions, read `docs/vm-oracle.md`. Use `tools/vm-test.ps1` for Test/Visual/Deploy/Status/Cancel/Recover; do not hand-roll SSH/build/capture loops. Finish an edit batch before testing, reuse the incremental Linux cache, and stop at the first named failure. Poll an existing tool session rather than starting another build; never interrupt a healthy build for optional polish. Explicit user-requested architecture/planning/testing reviews may inspect relevant plans and evidence without implementing the next feature. Preserve unrelated working-tree changes.

2. Follow the execution-priority table in `docs/agent-backlog.md`: core Page Studio T15-T24 first, related shell fixes next, then remaining work. Task IDs are stable identifiers, not priority. Select the first unfinished ACTIVE row with completed Depends tasks; exclude struck/superseded tasks and report blockers rather than silently switching features. A planning-only request does not authorize application implementation.
3. Within a task, read ONLY its `Entry docs` + `Entry files`. Do not re-read the entire codebase or all planning docs.
4. Implement its `Scope` to satisfy its `Done when` gate, preserving standing gates (both visual fixtures, profile isolation, atomic save/rollback, Kodi media ownership, offline Generator Test).
5. Before finishing, run its `Verify` commands/tests and update `docs/agent-backlog.md`: check off sub-boxes, recompute that task's `%`, recompute `Overall %`.
6. Never mark `100%` without evidence paths (test logs, fixture screenshots, `/tmp/...` evidence). `100% x 27 ACTIVE software tasks = S1 first-product software complete, ready for hardware qualification`; T02 is struck and T06-T08 are superseded/excluded. Keep the active count synchronized with the backlog. T30 owns Games and T31 final integrated software acceptance. H01–H05 separately gate physical qualification/public release; finish software device editors and VM acceptance first, never count simulated input/media as a physical pass.
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
    - Editor UX: selected item, operation, edit mode, preview vs applied state, and consequences of remove must always be unambiguous. User UX revision (2026-10-20): Move previews validated neighbor reflow; Confirm accepts one complete draft transaction and Back restores all affected items. Use explicit alternatives/dialogs for invalid or ambiguous targets, never silent overwrite or routine-dialog interruption. Context is Move/Edit component/Duplicate/Delete; size belongs inside Edit component. Follow `docs/design/page-studio/usability-audit-2026-10-20.md`; named swatches and a remote-operable color wheel replace hex-first selection.
11. Engineering best practices (must obey):
    - Small vertical slices: remote input → data → visible result → error/retry → focus restored → test. One task at a time, ranked backlog priority, never task-number order.
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
