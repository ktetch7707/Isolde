# T21 catalog and contextual inspector execution record

T21 implementation is undergoing final installed verification. This record does not claim T24 user acceptance, editor cutover, physical capture qualification or a complete appliance Generator Test.

## Delivered paths

- Add opens a six-family visual catalog: Button, Button carousel, Media shelf, App bar, Image and Widgets. Content/source/link selection precedes placement on the full page. Canceling content or placement does not modify history or the applied document.
- Select a component → Edit component opens Content, Appearance, Layout, Actions, Accessibility and More. Page appearance opens Colors, Background, Layout and More. Floating windows can hide without shrinking the authored page. D-pad/Confirm/Back/Menu and pointer controls call the same typed session commands.
- Fields identify inherited, overridden and mixed values; Menu resets an override. Advanced controls include independent card/label styles, typography, artwork crop/fit/focal/zoom, borders, visual states, accessibility, internal layouts and legacy presentation options. Solid-surface contrast estimates are identified as estimates.
- Button/Image links support None, named pages and registered actions. Settings sections, apps/declared handlers, collections and collection-owned Kodi titles have named choices. New page links require an existing destination; existing broken links remain repairable. Decorative images have no runtime remote focus. Source repair, categories/sections, navigation tabs and Movies/Shows destinations remain editable.
- Artwork is decoded and bounded, privately staged, content-addressed and copied only on Apply. Asset-copy/layout failures preserve applied state and the draft; newly created assets roll back. Managed artwork cannot escape its profile through symlinks. Reads never rewrite stored layouts.
- Real shelves reuse PageData/KodiMedia and at most 50 items per page. Presentation changes do not change the query. Gestures freeze client/items; editing intercepts playback and refresh. Optional samples are explicitly labeled and contain no media IDs, artwork requests or real progress. Teardown denies late profile access.

The capability inventory is in [the feature map](design/page-studio/feature-map.md#t21-implementation-destinations); schema/data details are in [components.md](components.md#t21-studio-catalog-and-contextual-editing). T22 retains templates/tokens, T23 retains scoped restore and Apply UX hardening, and T24 retains cutover/usability. Legacy entry 31 remains available until cutover.

## Frozen source and diagnostic history

Snapshots package tracked working-tree bytes from base HEAD `3870c8a3808ba8813f7ce57268a0c05c58923060`. Each guest source is `/home/isolde-dev/isolde-runs/studio-implementation-N/source`, with sibling build/logs. Host archives/manifests are under `build/studio-implementation-N/`. Archive SHA-256 and every extracted file are verified before CMake. Extraction uses `tar xzmf`.

- 49: native compilation passed; model tests found absent-theme fallback and newly assigned page-link existence gaps. Corrected before installation.
- 50: 28 model cases passed. UI tests found obsolete catalog/inspector routes and a media-only focus regression. Corrected; no deployment.
- 51: 28 model cases passed. UI parser rejected an object-spread expression; replaced with Object.assign. No deployment.
- 52: full application compiled; full CTest had 13 passes and a navigation timeout. Direct UI run identified an over-specific absolute request count after page teardown and a missing explicit grid in the new test setup. These were test setup errors, not a runtime hang; meaningful request-count/freeze assertions were retained with an isolated baseline. Navigation timeout expanded from 30 to 60 seconds for the enlarged suite.
- 53: 28 model cases and 26 UI cases passed, including all-family rendering, remote/pointer/numeric parity, catalog cancel, inspector edits/reset, staged artwork failure and profile teardown. UI duration 29.713 seconds. This is a native checkpoint, not final installed acceptance.
- 54–58: isolated Ninja builds were stopped by verified working-directory/PID identity to include plain-text/scaled controls, named-page picker refresh, the corresponding regression's corrected command name, named Settings destinations, group appearance bindings and accessibility ownership. No installed release or normal profile was changed by these stopped builds.
- 59: archive `4cc604120a73bfd90efb5e4d227c5b40ded3901e9c359306d58bce0924f6370c`; full application compiled and 13/14 suites passed (73.66 seconds). The new named-page refresh regression failed. Replaced the opaque method-only picker binding with an observable session property derived directly from the current draft. No deployment.
- 60: frozen candidate archive `7163d9dd02e13b94b8174fe72d206c84772fe9a9715a928f34746c5111227cbb`. QML syntax validation runs before CMake. Full application/CTest and installed verification results are recorded below when complete.

## Reproduction

Follow [vm-oracle.md](vm-oracle.md), preserve the normal profile and use a new frozen source directory. Build with Ninja Debug/BUILD_TESTING and parallelism 2. Run `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ctest --test-dir ../build --output-on-failure` from the frozen source. Native coverage includes `studioCatalogLinksInspectorAndStagedArtwork`, `studioCatalogAndContextualInspector`, `pageComposerAndIndependentKodiWidgets`, gesture/collision/group/geometry parity and the existing profile/write-failure regressions.

Only after CTest passes, run `python3 tools/install-appliance.py install --build-dir ../build`, restart the user service and verify the real executable path after the launch wrapper exits. Run the Studio, lounge and pastel wrappers sequentially with explicit SourceRoot; never remove/bypass the graphical acceptance lock. The Studio helper captures the catalog and inspector in addition to direct editing, collision, resize, Try interaction, applied state, group contents and Layers. View every captured image; capture acknowledgement is not visual acceptance. Retain helper result/log/config directories and source/build/install/CTest logs on the host.

## Final verification

Frozen source snapshot 62, guest source `/home/isolde-dev/isolde-runs/studio-implementation-62/source`, installed release `4e78f5b019a4f554a4c6c450`.

- CTest: 14/14 suites registered (`ctest -N` lists `isolde_tests`, `capture_protocol`, `component_contracts`, `home_component_navigation`, `audio_backend`, `appliance_installation`, `appliance_recovery`, `kodi_local_bridge`, `kodi_playback`, `kodi_cached_artwork`, `component_health`, `kodi_media_client`, `native_profile_isolation`, `profile_browser_isolation`); 136 test functions, 0 failures. T21 coverage includes `ComponentTests::studioCatalogLinksInspectorAndStagedArtwork`, `HomeNavigationTests::studioCatalogAndContextualInspector`, `HomeNavigationTests::studioRemotePointerAndInspectorParity(page|group)`, `HomeNavigationTests::pageComposerAndIndependentKodiWidgets` and the `kodi_media_client` request-count regressions.
- Studio journey: `tools/run-visual-acceptance.ps1 -Case studio` with `--inspect` reached `WRAPPER_EXIT=0`. Guest evidence `/tmp/isolde-components-acceptance-tmt2lsig`; `result.json` reports `passed: true` with `preview_cancel_preserved_bytes`, `reopen_preserved_bytes`, `remote_collision_cancel_swap_undo`, `remote_resize_cancel_accept_undo`, `bounded_repeat_cancel`, `nested_group_remote_layers_apply` and `eight_slot_layout_only` all true, and `user_usability_acceptance: pending` (T24) and `physical_input_qualification: not claimed` (T10).
- Captures: eleven phases captured and acknowledged, each 1280x800, non-blank and pairwise distinct — `studio-catalog.png`, `studio-edit.png`, `studio-collision.png`, `studio-resize.png`, `studio-studio.png`, `studio-applied.png`, `studio-group.png`, `studio-inspector.png`, `studio-content.png`, `studio-inside.png`, `studio-layers.png`. Durable host evidence `evidence/t21-62/studio/`.
- Standing fixtures: lounge and pastel both passed with recreation and reset pairs (`lounge-custom.png`/`lounge-default.png`, `pastel-custom.png`/`pastel-default.png`). The recreation captures match the previously human-reviewed baselines at 99.98% and 99.91% identical sampled pixels, so T21 introduced no regression in either fixture. The packaged-default captures differ from the October 16 baselines in the same rows (560-680) for both skins, which is the accumulated T15-T21 default-Home change; a human still has to review that difference against `reference.jpg`.
- Isolation: the harness used disposable config/data/cache roots; the normal Household service was stopped for the run and restored `active`/`running` with `NRestarts=0` on release `4e78f5b019a4f554a4c6c450` (`previous` `4ac297501a5e0dc03384cc84` unchanged). No lock was deleted and no owned process was force-killed to escape a timeout.
- Harness fix: `tools/home-components-acceptance.py` installs an X11 error handler and retries focus, so a `BadMatch` race can no longer kill the harness and leave the user service stopped.
- Wrapper fix: `tools/run-visual-acceptance.ps1` now passes `-n` to every `ssh.exe` call. Without it the acknowledgement inherited stdin from a console-less wrapper and blocked forever, so the harness timed out after the first phase. Verified: `-n` returns in ~330 ms where the plain form never returned.
- Not claimed: the explicit human visual verdict against `reference.jpg` and the studio captures, T24 user/cutover acceptance, T10 physical input qualification, and a full-product offline Generator Test. Those remain their owning gates.

T21 backlog credit is recorded for the automated and installed evidence above; the standing visual verdicts remain a human review step.

## Tooling corrections made during this run

- `tools/run-visual-acceptance.ps1`: `ssh.exe` invocations gained `-n` so the capture acknowledgement cannot block on inherited stdin.
- `tools/home-components-acceptance.py`: X11 error handler plus focus retry around `XSetInputFocus`.
- `build/verify-captures.py` and `build/compare-captures.py` (scratch, untracked): decode PNGs and report dimensions, colour diversity, blank detection, pairwise distinctness and baseline divergence for captured evidence.
