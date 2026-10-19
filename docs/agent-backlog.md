# Isolde Agent Backlog — single source of truth
Location: `docs/agent-backlog.md` (this file). Entrypoint: `AGENTS.md` at repo root points here.
Sequencing authority: `docs/implementation-order.md`. Requirements: `docs/unified-master-plan.txt`, `docs/studio-builder.md`, `docs/master-roadmap.md`.

## How percent completion works (agent must follow)
- Each ACTIVE task T01,T03..T14 has checkboxes `[ ]` / `[x]`. All boxes in a task weigh equally. T02 is OUT OF SCOPE (see below) and excluded from math.
- Task % = round(checked / total * 100). Update `| % |` cell when you check a box.
- Overall % = round(average of the 13 ACTIVE task %s). Recompute on every edit.
- `100% x 13 = plan implemented` ONLY if every active box is `[x]` AND `Evidence` has log/screenshot paths. No evidence = max 90%.
- Workflow: open lowest-numbered ACTIVE task < 100%, read ONLY its Entry docs/files, do Scope, run Verify, check boxes, update %.
- Standing gates for every task: `tests/visual/home-reference/` + `tests/visual/home-pastel-reference/` recreation+reset pass; profile isolation; atomic save+rollback; Kodi owns media/progress/playback (no parallel DB/player); offline Generator Test; Settings/Edit Home always reachable.
- Scope decision (user, 2026-10-19): collection curation is the USER's responsibility via Kodi. Isolde does not build episode/tag/playlist/exclusion/template curation. T02 struck. Existing movie/show manual-collection code stays as-is (no removal, no extension).

## Overall (update every edit)
| Task | Title | % | Status |
|------|-------|---|--------|
| T01 | Bootstrap + gate check | 67% | in progress |
| T02 | ~~Finish curation~~ — OUT OF SCOPE (user curates via Kodi) | n/a | struck |
| T03 | Local search: Kodi titles via bridge (no curation adapters) | 75% | in progress |
| T04 | Kodi preservation + desktop import | 33% | in progress |
| T05 | Studio grid renderer consumes placements | 0% | not started |
| T06 | Remote + desktop grid editor | 0% | not started |
| T07 | Preview/Apply/Undo + persistence | 0% | not started |
| T08 | Templates / tokens / portability | 0% | not started |
| T09 | Playback preference + viewing options | 0% | not started |
| T10 | Capture + HDMI + audio editors (gated) | 33% | in progress |
| T11 | Guide / PseudoTV | 0% | not started |
| T12 | System polish + input + on-screen keyboard | 0% | not started |
| T13 | Appliance + release distribution | 0% | not started |
| T14 | Optional online discovery + linguist subtitles | 0% | not started |
| **Overall** | **average of 13 active (T02 excluded)** | **16%** | — |

## T01 — Bootstrap + gate check (S, depends: none)
Entry docs: `README.md`, `docs/implementation-order.md:19-38`, `tests/visual/home-reference/README.md`.
Entry files: `CMakeLists.txt`, `tools/home-reference-acceptance.py`.
Scope:
- [x] T01.1 Linux Qt6.4+ configure+build+ctest passes (`cmake -S . -B build -G Ninja`, `cmake --build build`, `ctest --test-dir build --output-on-failure`)
- [x] T01.2 Disposable-profile acceptance run: home-reference, home-components, tv-library `--pages --inspect`
- [ ] T01.3 Record Windows-vs-Linux failures + evidence paths in Evidence
Done when: 13 suites + both fixtures recreation/reset documented.
Verify: ctest log + `/tmp/isolde-*` result.json + screenshots. Evidence: docs/development-plan.md Oct-18 entries cite Linux 13-suite passes (releases 523f627f, cfd551b, cf9cbe4d, 2af84b94, 45ca0a45); Windows CMake/CTest pending in current env — T01.3 open.
## T02 — ~~Finish curation~~ STRUCK / OUT OF SCOPE
Decision (user 2026-10-19): curating the collection is the user's responsibility. Isolde uses Kodi to handle what they have. No episode/tag/playlist/exclusion/template curation work. Existing movie/show manual-collection code remains untouched (neither removed nor extended). Excluded from Overall %.
## T03 — Local search over Kodi library (M, depends: T01; curation adapters removed from scope)
Entry docs: `docs/unified-master-plan.txt` Sec 13, `docs/kodi-media.md`.
Entry files: `qml/CommandCenter.qml`, `src/home/PageData.h`, `qml/Main.qml` (openSearchFromPage).
Scope (Kodi-bridge only — no tag/playlist/curation adapters; those were T02, struck):
- [ ] T03.1 Kodi title search (Movies/Shows/Episodes) with per-type bounded paging, debounce + cancel-aware, explicit Kodi-unavailable state
- [ ] T03.2 Persisted Web OFF/ON toggle data model default OFF (OFF = zero external requests; ON path stubbed, no endpoint wired)
- [x] T03.3 Page-scoped sources via `pageSearchSources()` surfaced + routed back to originating page
- [ ] T03.4 Installed remote-only + real-Kodi acceptance
Done when: offscreen Command Center + bridge bounds tests pass, Web OFF provably silent.
Verify: component suite `pagesPersistAndRouteAndTravelWithSkins`, network-denial check. Evidence: T03.3 done per Oct-18 M4 (PageModel::pageSearchSources, CommandCenter pageSources, Main openSearchFromPage). T03.1/T03.2/T03.4 NOT credited — prior static-check script was not product work. T03.1 real gap: episode search results never render (no `episodeSearchClient` plumbing in Main.qml); Web toggle has no Settings UI surface. Curation adapters excluded — T02 struck.

## T04 — Kodi preservation + desktop import (M, depends: T01)
Entry docs: `docs/master-roadmap.md` steps 1-2, `docs/install.md`.
Entry files: `src/media/KodiService.h`, `src/sessions/DesktopSession.h`.
Scope:
- [ ] T04.1 Explicit backup/preview/import/conflict flow for non-default skin/sources/add-ons/watch-resume, rollback supported
- [ ] T04.2 Clean ownership handoff + guided first-library setup + Desktop/Return workflow
- [x] T04.3 Nested-folder scan to direct playback, no media moves/renames, no Kodi DB edits
Done when: real round-trip preserves skin+sources+watch-state; import rollback tested.
Verify: appliance/vm acceptance with disposable fixture (hashes unchanged). Evidence: docs/kodi-media.md remembered media folders + nested discovery + direct playback done (hashes unchanged); T04.1 backup/import + T04.2 guided setup/round-trip open per master-roadmap steps 1-2.

## T05 — Studio grid renderer consumes placements (M, depends: T01)
Entry docs: `docs/studio-builder.md` Universal grid, `docs/home-composition.md` Universal grid.
Entry files: `src/home/HomeLayout.h`, `src/home/ComponentSchema.h`, `qml/ComposedHome.qml`, `qml/PageSurface.qml`.
Scope:
- [ ] T05.1 Render `composition.grid` + per-widget `placement` (snap-only, overflow refused, empty cells valid, 16:9 scale, guides edit-mode only) via `PageModel.grid/placements`
- [ ] T05.2 Legacy no-grid layouts unchanged; invalid stored placements fallback to packaged defaults without rewriting file
- [ ] T05.3 Both visual fixtures still pass
Done when: renderer consumes grid, no new schema file, fixtures green.
Verify: `universalGridValidatesAndPersists` + QML geometry tests + fixture pair. Evidence: data contract only — HomeLayout grid()/setGrid/setItemPlacement/gridCollisions + ComponentSchema validGrid/validPlacement + universalGridValidatesAndPersists test exist; NO renderer consumes placements yet per docs/studio-builder.md M1 note. All three boxes open.

## T06 — Remote + desktop grid editor (L, depends: T05)
Entry docs: `docs/studio-builder.md` Editing modes, `docs/interface-design.md` User ownership.
Entry files: `qml/CustomizeHomeView.qml`, `qml/ComposedHome.qml`, `src/home/HomeLayout.cpp` (gridCollisions).
Scope:
- [ ] T06.1 TV-remote: select → Move/Resize/Customize/Duplicate/Remove, numeric span entry, collision dialog shift/swap/cancel (never silent overwrite)
- [ ] T06.2 Desktop: drag-to-place, inspector, shortcuts, zoom — identical layout data to remote
- [ ] T06.3 Internal sub-grid for large widgets (e.g. 4x2 HDMI switcher) edited separately
- [ ] T06.4 Keyboard/mouse parity + remote-only journey test
Done when: QML remote-key tests cover move/resize/collision/duplicate/remove on both surfaces.
Verify: `home_component_navigation` + new editor tests + fixture pair. Evidence: _fill in_

## T07 — Preview/Apply/Undo + persistence (M, depends: T06)
Entry docs: `docs/studio-builder.md` Build sequence, `docs/components.md` Versioned data.
Entry files: `src/home/HomeLayout.cpp`, `src/home/HomeSkins.cpp`.
Scope:
- [ ] T07.1 Draft → Preview (real renderer, no save) → Validate → Atomic apply → Rollback on failure; errors via `errorString()`
- [ ] T07.2 Versioned local backups, undo/redo history, restore-defaults (never uninstalls apps), downgrade backup retained
- [ ] T07.3 Empty-Home recovery reachable via Up → Settings/Edit; write-failure test proves no swap/loss
Done when: deterministic write-failure test passes, backups verified.
Verify: `component_contracts` + write-failure regression + fixture pair. Evidence: partial — atomic save+rollback + failedEditRetainsProfileLayout + v1-read/v2-write + resetToDefaults exist; Preview/undo/versioned-history/downgrade-backup open (Studio M2c unstarted).

## T08 — Templates / tokens / portability (M, depends: T07)
Entry docs: `docs/unified-master-plan.txt` Sec 1-2,17, `docs/skins.md`.
Entry files: `src/home/HomeSkins.cpp`, `src/home/HomePages.cpp`, `config/apps.json`.
Scope:
- [ ] T08.1 Ship 4 templates: (A) 5-tab compact, (B) scrollable dashboard, (C) poster left-sidebar, (D) minimal/pastel/image — non-destructive apply
- [ ] T08.2 Theme tokens (palette/type/spacing/focus/motion), symbols, consistent apply
- [ ] T08.3 Per-page + whole-interface import/export with missing-source remap (unmatched widgets stay editable, never fail screen)
Done when: template apply + import/export round-trip tests pass.
Verify: skin/page round-trip tests + fixture pair. Evidence: partial — 3 built-in skins (default/cinema/pastel per docs/skins.md), personal snapshots, page skins travel with whole-interface skins; 4 unified templates A-D, tokens/symbols, portable import/export all open.

## T09 — Playback preference + viewing options (M, depends: T08)
Entry docs: `docs/unified-master-plan.txt` Sec 16, `docs/kodi-media.md`.
Entry files: `src/media/KodiMedia.cpp`, `src/settings/SetupState.h`, `qml/SettingsView.qml`.
Scope:
- [ ] T09.1 `Settings → Media → Playback Preferences → Always Play Locally When Available` default ON
- [ ] T09.2 ON: local match from shelf/search plays/resumes immediately via Kodi; series tile → episodes unless Continue/Play-Next; details via context menu
- [ ] T09.3 OFF: Web ON discovery may show viewing-choices; library shelves still direct; resume/subtitle/watched retained via Kodi
- [ ] T09.4 6-row Sec-16 acceptance table passes on installed build
Done when: direct-play matrix verified, Web OFF silent.
Verify: playback acceptance (movie shelf/search, Continue resume, series→episodes, OFF unchanged). Evidence: partial — direct Kodi playback/resume, Continue Watching (inprogress), series→episodes, shared player path exist per docs/kodi-media.md; Always-Play-Locally preference + viewing-choices page + 6-row table open.

## T10 — Capture + HDMI + audio editors, gated (L, depends: T08)
Entry docs: `docs/master-roadmap.md` capture section, `docs/audio.md`, `docs/remotes.md`.
Entry files: `src/media/CaptureController.h`, `src/settings/AudioSettings.h`, `qml/CaptureSettingsView.qml`.
Scope:
- [x] T10.1 Fake-backend state-machine + protocol/action-schema/dispatch unit tests (software only, no hardware claims)
- [ ] T10.2 Real RS-232/capture/audio/latency/hotplug matrix qualification
- [ ] T10.3 Capture-input editor + 8-input HDMI switcher editor + audio-routes editor + keyboard/remote integration (only after T10.2)
Done when: hardware matrix signed off; VM-only never counted as live pass.
Verify: `tst_capture` + hardware matrix log. Evidence: CaptureController (devices/formats/inputs/serial/commandTemplate) + HDMI-inputs settings page exist; hardware qualification + device editors open per docs/implementation-order.md:49.

## T11 — Guide / PseudoTV (L, depends: T08)
Entry docs: `docs/unified-master-plan.txt` PseudoTV Sec 1-10, `docs/implementation-order.md:50`.
Entry files: `src/media/` (new guide store), `qml/GuideView.qml` (new), `qml/ComposedHome.qml` (compact widget).
Scope:
- [ ] T11.1 Inspect actual backend first, then PVR or playlist/EPG adapter; compact widget (3-5ch + progress) → overlay → full EPG
- [ ] T11.2 SQLite WAL store (channels/rules/slots/migrations), persisted broadcast clock (real-time/pause-when-off/fixed/shuffle), stable committed slots, targeted repair, no global reindex on guide open
- [ ] T11.3 12-hour-off seek test + restart-stability (7-day shuffle unchanged) + Cartoon-Network-in-one-save + offline cached listings
Done when: Sec-10 acceptance tests pass with evidence.
Verify: guide acceptance log + DB slot-stability tests. Evidence: none — no guide store/widget/EPG in tree; guide-summary cards are static config per docs/home-composition.md (no backend). All open.

## T12 — System polish + input + on-screen keyboard (M, depends: T05)
Entry docs: `docs/architecture.md` Audio/Remote, `docs/development-plan.md` keyboard, `docs/interface-design.md`.
Entry files: `src/input/InputManager.h`, `src/settings/*.h`, `qml/Keyboard.qml` (new or extend).
Scope:
- [ ] T12.1 Audio/display/power/network onboarding polish, a11y/l10n, reduced-motion/contrast
- [ ] T12.2 On-screen keyboard for Isolde-owned fields first (Network Settings, Store search): D-pad/Back/shift/symbols/focus-restore, never obscures field, passwords masked
- [ ] T12.3 Remote-only journey + offline-denial tests
Done when: keyboard + polish acceptance passes without breaking fixtures.
Verify: input/a11y tests + installed remote run. Evidence: InputManager commands + AudioSettings/RemoteSettings/NetworkSettings/DisplaySettings pages + mixer doubles + volume/remote acceptance exist; on-screen keyboard service NOT in tree (requirement only per docs/development-plan.md); a11y/l10n/reduced-motion incomplete. All boxes open.

## T13 — Appliance + release distribution (L, depends: all active T01,T03-T12 except T14; T02 excluded)
Entry docs: `docs/install.md`, `docs/packages.md`, `docs/architecture.md` appliance.
Entry files: `tests/test_appliance_install.py`, `tests/test_recovery.py`, `src/recovery/Recovery.qml`.
Scope:
- [ ] T13.1 Dedicated compositor/autologin, clean-machine onboarding, signed artifacts, offline dependency/model manifests
- [ ] T13.2 Repair/rollback + recovery media (`isolde-recovery`), explicit disk-selection consent
- [ ] T13.3 Fresh install + cold boot + power-loss + failed-upgrade + disconnected Generator Test on target hardware
Done when: public-release gate passes. Release cannot precede this.
Verify: appliance_install + appliance_recovery + Generator Test log. Evidence: partial foundation — install-appliance.py (Ubuntu 26.04 dev/pilot only), wait-for-display gate, atomic content-addressed releases, isolde-recovery, rollback path exist per docs/install.md; dedicated compositor/autologin/signed images/offline media/first-run wizard/hardware Generator Test all open.

## T14 — Optional online discovery + linguist subtitles (M, depends: T03; may run after T13)
Entry docs: `docs/implementation-order.md:53-54`, `docs/unified-master-plan.txt` Sec 13-15.
Entry files: `qml/CommandCenter.qml`, `src/media/KodiMedia.cpp` (subtitle load path).
Scope:
- [ ] T14.1 Online discovery opt-in: regional freshness/attribution, provider launch routes, OFF cancels late/cached results, remakes distinct, installed app never = entitlement (requires provider/terms decision first)
- [ ] T14.2 Linguist: SRT translate/generate/queue/review/cancel, offline, non-overwriting Unicode, provenance, no library-wide auto jobs or implicit downloads
Done when: OFF-silent + provenance tests pass; provider gate documented.
Verify: network-denial + subtitle provenance tests. Evidence: none — Web OFF label informational only, no discovery endpoint; no linguist/SRT in tree per docs/implementation-order.md:53-54. All open.

## Completion rule
Overall = round(sum(active T01,T03..T14 %)/13). T02 struck, excluded. When all 13 active read `100%` with Evidence paths, plan is implemented. If any active task lacks evidence, cap it at 90% and Overall cannot read 100%.
