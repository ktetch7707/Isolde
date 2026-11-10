# Whole-project test campaign — 2026-11-09

Status: repair and verification in progress. Scope: all 25 active tasks, excluding T13/T31. The user's latest instruction authorizes this agent to fix the findings, superseding the original test-blockers-only editing limit. Original findings and immutable failed evidence remain below; repairs are not closed until verified.

Latest checkpoint, November10: focused repair batch S passed all eight reached UI/application registrations; runtime batch Z passed all five runtime registrations, including actual native profile isolation. The runner passed98 self-tests. Final integration candidate `campaign-20261110-final-a` passes all60 registrations after the combined repairs, zero failures/skips. All97 native captures are reviewed (95 exact-hash reuse,2 newly inspected). Graphical/installed journeys remain in progress. BUG-058's six unprovisioned emulator routes are honestly disabled; expanded-generation software remains missing and is not accepted. The sections below preserve the original baseline and subsequent evidence chronologically.

Candidate: `f10bb57` on `agent-backlog-progress`; working tree clean at start. VM evidence: `build/vm-oracle/campaign-20261109-{a..i}/` (each immutable attempt below). One baseline application, immutable attempts containing only documented test blockers, full registered CTest followed by supported graphical journeys. No acceptance credit is awarded merely for executing tests. Untested requirements and fixture-only coverage will remain explicit.

## Findings

### BUG-001 — Backlog Coding% total contradicts all 25 task rows
- Type: reporting defect; low severity.
- Reproduction: read current scope/completion math/Overall row in `docs/agent-backlog.md`.
- Observed: all 25 in-scope rows show Coding 100%, while the total says 2415 / 25 = 97%.
- Expected: 2500 / 25 = 100%, or identify which task claims are incorrect.
- Changes: none; preserve claimed baseline for testing.

## Execution

- Initial full campaign: pending. Graphical selection covers Studio, designs/search, shared themes, desktop, collections/catalog/history, both references, Kodi pages, Guide surfaces/lifecycle/features, media/art/repair/playlists, apps/clients/VPN/shell/Android/Store/Games/emulators. Optional twelve-hour soak excluded; bounded offline reopen remains a separate required check.

## Coverage and limitations

A passing registered test is evidence for its assertions, not automatic completion of its entire owning task. Installed-only journeys, virtual capture/audio, discovery/subtitles, source containment and offline/profile/error paths must be reconciled against actual evidence. T13/T31 are outside this campaign's acceptance scope even if shared CTest registrations execute.

### BUG-002 — Keyboard changes break test infrastructure and remote fixture input
- Owners: T12 keyboard integration / test harness; severity: high test blocker.
- Evidence: `build/vm-oracle/campaign-20261109-b/report/oracle-self-test.log`.
- Reproduction: normal full Test at f10bb57; oracle self-test fails `test_remote_fixture_text_uses_production_keyboard_and_only_dpad` (92 pass, 1 fail).
- Cause: test regex assumes old inline arrays; Guide helper sends old key indices; Studio portability helper parses the removed old array. New production keyboard uses `keyList()` and modes.
- Authorized blocker repair: update only test/helper layout parsing and Guide key indices; retain D-pad/Confirm-only stimulus. Application unchanged. Verification pending next runner self-test.

Transport note: run A failed before VM connection due sandbox localhost SSH denial; run B used approved network access. This is an environment limitation, not an Isolde product bug. B produced no build or CTest payload.

## Task coverage ledger

All rows below are planned, not passed. Current-candidate results will replace these labels as evidence arrives.

| Tasks | Primary registered / graphical coverage | Additional acceptance to verify |
|---|---|---|
| T01 | Build, full CTest, source/payload receipts, Lounge/Pastel | Source identity and evidence review |
| T03 | Kodi media client/bridge, Home navigation; pages/history/search | Actual local query, playback and OFF denial |
| T04 | Kodi bridge/playback/client; pages/playlists/repairs | Import preservation/rollback, setup/settings/inventory |
| T05 | Component contracts/Home navigation; Lounge/Pastel | Both generic recreation/reset pairs |
| T09 | Kodi client/playback; pages/history | Six-row playback preference matrix |
| T10 | Capture protocol/serial/audio; component/Home tests | Virtual-device editor and recovery journey |
| T11 | Guide contracts/UI/offline; guide/surfaces/lifecycle/features | Stopped-process offline cold reopen |
| T12 | VPN/input/settings/session/isolation; vpn/shell/apps/clients | Keyboard, idle/screensaver, remap, topology, app support |
| T14 | t14_discovery_subtitles, Kodi playback | Installed discovery, OFF cancellation, SRT review/output |
| T15–T24 | Studio partitions, component/navigation/design/performance; studio/designs/desktop/catalog | Recorded remote tasks, atomicity/profile/legacy/cutover and visual review |
| T25 | isolde_tests, sources_remote_ui, schema/Home | Contained multi-source dark/pastel runtime, unplug/retry |
| T26 | App registry/component/navigation/selection; collections/apps | All Apps routes/filter/launch/error/origin chain |
| T27 | Surface contracts/shell/navigation; themes/custom/faults/controls | Resolve tokens, palette/contrast, unchanged Kodi settings |
| T28 | Home navigation/preferences; studio-search/themes | Header switch, late callback/OFF/profile/atomic failure |
| T29 | Store/F-Droid/Android/web/native isolation; store/android/androidapps/clients | External + embedded browser input, signed lifecycle and offline |
| T30 | Games/provisioning/Steam/portable/optical/UI/session; games/emulators | Actual gameplay/save/offline, virtual optical, runtime boundary matrix |

## Run log

- A: environment transport denied before VM; no software tests.
- B: runner self-tests 92/93; BUG-002 blocks build. No CTest/visual executed.
- C: same application, harness-only keyboard repair; in progress.

### BUG-003 — KodiService header cannot compile after playback-preference integration
- Owner: T09 / T04; severity: critical build blocker.
- Evidence: C `report/build.log`: duplicate `markReady`, `changed`, `probeRequested`, `ready`, `unavailable` declarations and generated signal definitions.
- The inserted preference block also accidentally puts the existing public constructor/destructor/API under `private:`.
- Authorized minimal unblock: retain one signal/method declaration, put the new preference notification with existing signals, restore public API. No feature behavior change. Pending rebuild.

### BUG-004 — Gamepad topology uses a nonexistent Qt QSet API
- Owner: T30; severity: critical build blocker.
- Evidence: C `report/build.log`, `GamepadTopology.h:50`, `QSet<QString>` has no member `united`.
- Authorized minimal unblock: copy the launch set and call supported `unite`; same intended union semantics. Pending rebuild.

C runner self-tests now pass (93/93); verified automatic guest cleanup retained 2.59 GiB. Configure passes; compilation stops at BUG-003/004 before any CTest. A replacement integration run is necessary because there is no built candidate yet, not for accounting or optional changes.

### BUG-005 — Standalone navigation regression fixture is stale
- Owner: T24 / test harness; severity: medium verification defect.
- Reproduction: `node --test tests/qml_navigation.test.cjs` on D application bytes.
- Result: 41 pass / 1 fail, `a newer route cancels both phases of deferred viewport restoration`.
- Evidence: D `qml-navigation-host.log`. `HomeScreen.resetNavigationState` reads `composition.enabled`; the extracted-method fixture supplies no `composition` (also omits related header properties).
- Classification: fixture-context failure, not evidence of the same ReferenceError in a live QML object. Left unchanged because it does not block the VM suite.

### BUG-006 — Always Play Locally preference is disconnected and does not restore saved state
- Owner: T09; severity: high incomplete feature; source-confirmed, installed reproduction pending.
- Evidence: repository-wide case-insensitive search for `alwaysPlayLocally` finds only KodiService header/setter/reset definitions. No QML, playback/search call site or test consumes the property. DisplaySettings/Settings expose no such preference despite backlog claiming both integrations.
- `KodiService` constructor never reads `profiles/<id>/kodi/alwaysPlayLocally`; each new instance starts `true`, even after setter saved `false`.
- Setter updates visible memory before attempting persistence and does not sync/check settings failure or roll back.
- Expected: required Media → Playback Preferences toggle; saved active-profile value restored; actual ON/OFF playback decision paths; truthful failed-save behavior.
- Changes: none. This feature defect does not prevent testing unrelated paths.

### BUG-007 — Games optical refresh timer has no member declaration
- Owner: T30; severity: critical build blocker.
- Evidence: D `report/build.log`, `GameLibrary.cpp:449`: `m_opticalRefreshTimer` undeclared.
- Minimal testing unblock: declare the existing QTimer used by the constructor. No algorithm changes.

### BUG-008 — Games uninstall combines incompatible auto declarations
- Owner: T30; severity: critical build blocker.
- Evidence: D `report/build.log`, `GameLibrary.cpp:1823`: one `const auto` declaration tries to deduce both QVariantMap and QString.
- Minimal testing unblock: split record and root declarations. Pending rebuild.

D fails compilation after the earlier blockers are cleared; no CTest/visual runs yet. Replacement E is justified by these diagnosed build failures and reuses the incremental guest cache.

### BUG-009 — High Contrast setting cannot be activated
- Owner: T12; severity: high accessibility feature failure.
- Reproduction: Display Settings → fourth row High Contrast → Confirm.
- Production JS reproduction: E `display-contrast-repro.cjs` / `.log`; executed actual `DisplaySettingsView.handleCommand` with row 3 selected. No state change and no action.
- Cause: handler tests `selectedIndex === 5`, but High Contrast is index 3 and the five-row list clamps to 4. No D-pad route can reach 5.
- Additional gap: attempted branch directly mutates a nested appearance object, with no persisted model setter/reset support or contrast consumer for that new field.
- Changes: none; unrelated tests can continue.

### BUG-010 — Authored screensaver pages are not integrated
- Owner: T12.5; severity: high incomplete feature; source-confirmed, installed walkthrough pending.
- Backlog claims screensaver QML pages complete. Actual Main → AmbientOverlay → AmbientScreen still uses fixed slideshow/gradient/dim settings. AmbientSettings has no page selection/rotation/editor model; Settings exposes the old fixed controls.
- New `qml/Screensaver.qml` is not registered in CMake or instantiated anywhere. It also contains invalid declarative `if` blocks, but is currently dead code; no product-runtime parse failure is claimed from this unused file.
- Expected: profile-owned screensaver pages authored with shared Studio widgets/managed art and selectable rotation, per T12.5.
- Changes: none.

### BUG-011 — Optical adapter omits the syscall declaration header
- Owner: T30; severity: critical build blocker.
- Evidence: E `report/build.log`, `OpticalDiscAdapter.cpp:130,135`: `::syscall` undeclared.
- Minimal unblock: include Linux `unistd.h` with existing platform headers. No behavior change.

### BUG-012 — SubtitleQueue QML contains syntax errors on normal application dependency
- Owner: T14; severity: critical launch blocker.
- Evidence: E `report/build.log`, SubtitleQueue.qml:280/282/283/284 `Unexpected token ';'`.
- Cause: signal handlers ending with a JS block are followed by a QML separator semicolon before `background:`; QML rejects it. Main instantiates SubtitleQueue, so this is not an optional unused screen.
- Minimal unblock: remove the four invalid separators; retain handler bodies and styling.
- E also confirms Screensaver.qml:61 invalid `if` syntax (BUG-010 dead/unintegrated component); no repair of the unused feature attempted.

Replacement F is required by E's diagnosed compile/normal-launch failures; earlier 93/93 harness passes reused as diagnosis, not application proof.

Coverage note for BUG-005: the 42-test Node production-JavaScript suite is not registered in CMake/CTest; it was run separately by this campaign. A full CTest pass alone would not have exercised it.

### BUG-013 — Emulator definitions use two more unsupported Qt APIs
- Owner: T30; severity: critical build blocker.
- Evidence: F `report/build.log`, GameEmulators.cpp:107/110: no QSet(QList) constructor and QVariant has no `isDouble`.
- Minimal unblock: construct the set from stable list iterators; perform intended JSON-number check on QJsonValue::fromVariant. No broader validation redesign.
- F stops at build; no CTest or graphical results. Replacement G is justified by this named compile failure; use existing incremental cache.

Campaign repair correction (not an original-candidate defect): G failed because my BUG-013 repair used QJsonValue without adding its full header. Added `<QJsonValue>`; G `report/build.log` preserves that failed repair. Replacement H is necessary to verify the corrected blocker repair. Application feature defects remain untouched.

### BUG-014 — Kodi playlist reorder does not compile
- Owner: T04; severity: critical build blocker.
- Evidence: H `report/build.log`, KodiMedia.cpp:235/237: implicit QVariant→int, STL-style `.second` on a Qt iterator, and QVariantList passed directly as a JSON object value.
- Minimal unblock: use QJsonArray for the outgoing indices and explicit QSet contains/insert duplicate detection. Retain existing input conversion behavior (no feature validation redesign).
- H confirms emulator definitions now compile. No CTest yet; replacement I is justified by the diagnosed playlist build failure.

## Candidate integrity

The original `f10bb57` is not buildable. Later attempts are explicitly **original application + test-blocking repairs**, not evidence that the original candidate passed. No feature bug has been repaired for acceptance. All original and repair failures remain linked above. Normal host settings/services/accounts are untouched; guest maintenance has used only the runner's verified automatic preflight.

### BUG-015 — Expanded keyboard overflows its fixed-height Network/Store panel
- Owner: T12/T29; severity: medium visual/accessibility defect; production layout calculation, screenshot pending.
- Reproduction: open normal letter-mode keyboard in Network (height at most 400, bottom margin 30); default spacing/scale.
- Evidence: I `keyboard-geometry-repro.cjs` / `.log` executes production `keyList`: 57 keys → six rows → 280px grid. With existing Column margin/value/mode row/spacing, hint starts at keyboard y=438, eight pixels below the window bottom in Network. Hint text is therefore offscreen; content also exceeds its background.
- Keyboard has no content-height sizing or scrolling; scaling increases overflow. Store also caps at 400.
- Changes: none; confirm with real VM capture when runtime is available.

### BUG-016 — All Apps/catalog native regression tests do not compile
- Owner: T26/T29 test integration; severity: high test blocker.
- Evidence: I `report/build.log`: tst_isolde.cpp:80 `QVERIFY` receives three macro arguments from the unparenthesized braced QStringList; line852 uses undeclared `projected`.
- Minimal harness repair: parenthesize the assertion expression; declare the local projection. Assertions remain unchanged.
- I compiled the application and reached test-target compilation; no CTest ran. Replacement J is necessary for this diagnosed test-build failure and retains all cached objects.

### BUG-017 — Games component contract tests call the wrong schema signature
- Owner: T30 test integration; severity: high test blocker.
- Evidence: J `report/build.log`, tst_components.cpp:492/494/496: missing required `legacy` argument to ComponentSchema::component.
- Minimal harness repair: pass `false` and existing error output, matching the adjacent invalid-input assertion. No assertion removed or relaxed.
- J test-target build fails before CTest. Replacement K is justified by this named test compilation failure.

### BUG-018 — Games UI/optical tests have missing and ambiguous Qt types
- Owner: T30 test integration; severity: high test blocker.
- Evidence: K `report/build.log`, games_ui.cpp:312 passes a map-shaped initializer where a list of maps is required; 403/426 ambiguous QDBusMessage::createReply overloads; 932/1077 use QSqlError without its header.
- Minimal harness repair: explicit single-map QVariantList, explicit QVariantList replies, include QSqlError. Preserve every expected result and stimulus.
- K stops at test compilation; no CTest or graphical results. Replacement L is justified by these diagnosed errors.

### BUG-019 — Playback-preference commit deleted KodiService::retry
- Owner: T09/T04; severity: critical link blocker and missing recovery action.
- Evidence: L `report/build.log`: application and Studio test executable undefined reference to KodiService::retry. Git `a5fe54e` replaced its implementation with the preference setter, leaving its invokable declaration and Settings caller.
- Minimal unblock: restore the exact pre-regression retry implementation from the parent commit (`if (!switching()) { stop(); start(); }`). No redesign.
- L completed compilation and failed at linking; CTest still unrun. Replacement M is justified by the diagnosed missing symbol.

### BUG-020 — T25 source presets invalidate all packaged Studio options
- Owner: T25 integration / T21 regression; severity: critical Studio/test blocker.
- Evidence: M `report/ctest.log`, first registration `studio_quick`: 5 QtTest results pass, 15 fail; 59 subsequent CTest registrations unrun under stop-on-failure.
- Diagnosis: config/studio-defaults.json adds `source-notice` and `detected-sources` presets, but HomeLayout::studioOptions still rejects any preset key outside its old whitelist. Therefore it returns an empty map for the shipped document; inspector/grid/color/catalog settings disappear. ComponentSchema already validates both new families.
- Failed cases retained individually: color-wheel pointer/remote parity; exact color validation; header edit/undo/return; move cancel/history/viewport; move reveal; tools/default restore; no-room add; page color reset; page color swatches; replace-in-place; scalar corners; exact percent; scalar remote/pointer parity; size slider; Studio colors/Undo. See the named assertion/actual/expected/location for each in M log; they are not counted as 15 proven independent root causes.
- Minimal blocker repair: add only the two already schema-validated shipped preset IDs to the loader allowlist. Retain rejection of unknown IDs and all component validation. This restores the common test configuration rather than redesigning any feature.
- Replacement N full suite is justified by this demonstrated full-suite failure and shared loader correction. Do not rerun successful suites merely for accounting; none have passed yet.

### BUG-021 — Legacy Studio-options fallback rejects the now-larger packaged defaults
- Owner: T25/T21/T22 integration; severity: high compatibility regression; source-confirmed, native case pending.
- Evidence: current config/studio-defaults.json is 66,351 bytes. HomeLayout::studioOptions accepts its primary file up to 128 KiB, but both legacy missing-guide-overlay and missing-referenceViewport fallback reads still refuse a packaged file larger than 64 KiB (65,536 bytes).
- Reproduction path: a previously supported Studio options document missing referenceViewport reaches the packaged fallback and returns an empty map with this shipped file, rather than inheriting viewport defaults in memory. The same occurs for older Guide overlay options.
- Expected: supported legacy documents inherit bounded packaged defaults without rewriting stored files.
- Changes: none; new finding discovered while diagnosing BUG-020, not yet repaired or runtime-accepted.

### BUG-022 — Studio reset-defaults regression fixture selects the new All Apps item
- Owner: T26/T24 test integration; severity: medium verification defect.
- Evidence: N `report/ctest.log`, test_named_tools_history_and_draft_default_restore at line133, actual `appGallery`, expected `restoreDefaults`.
- Diagnosis: T26 inserted All Apps at editor-settings index2; test still hardcodes index2 for restore. Existing menu labels identify the correct action.
- Changes: none in the frozen test candidate; retained as an observed failed assertion, not an application reset failure.

### BUG-023 — Studio quick registration exceeds its 60-second deadline
- Owner: T24 test/performance investigation; severity: high campaign blocker; root cause unresolved.
- Evidence: N `report/ctest.log`: CTest kills studio_quick at60.05s after 13 passing results and BUG-022; remaining slot assertions are unrun. No deadline was relaxed and no timeout was relabeled as passing.
- Loader correction eliminated the former missing-inspector/move/color failures before this timeout. Need a bounded focused diagnosis of the unfinished scalar/size/input paths; cannot yet distinguish accumulated test cost from stalled interaction.

## Diagnostic continuation contract

The ordinary runner stops at each first failing CTest registration. Added opt-in `Test -ResumeRunId <diagnosed-run>` solely to continue this user-requested broad bug campaign: a new immutable snapshot runs current runner self-tests, requires identical application/test bytes to the prior campaign, verifies the frozen incremental source/manifest, retains failed/skipped outcomes and runs only previously unattempted registrations under stop-on-failure. Every failure still stops for diagnosis. Chain receipts retain all results, normal service/release equality and remaining names. No full pass, acceptance payload or deployment is awarded by continuation. This avoids rebuilding/repeating successful registrations and does not silently omit known failures.

O continues N's remaining59 registrations; no application/test-source change since N. The only new changes are the bounded runner/reporting continuation and its safety regression.

### BUG-024 — Carousel test expects obsolete first-item fallback after removal
- Owner: T26/T21 test integration; severity: medium verification defect.
- Evidence: O `report/ctest.log`, test_more_apps_store_launch_and_return line317: actual `three`, expected `one` after hiding selected Store card.
- Diagnosis: current PageAppCarousel preserves the nearest surviving neighbor. T26.4/5 explicitly require nearest-surviving selection; the assertion still expects the first card. Live launch/return portions preceding this assertion pass.
- Changes: none in frozen campaign; do not report the nearest-neighbor behavior as a product defect.

### BUG-025 — Recently-viewed catalog test navigates a stale positional index
- Owner: T25/T21 test integration; severity: medium verification defect.
- Evidence: O `report/ctest.log`, test_recently_viewed_catalog_count_capacity_and_private_cancel line605: actual catalog, expected move.
- Diagnosis: it issues three Down presses to select Recently viewed; T25 prepended Source notice/Detected sources catalog entries. Its fixed index no longer identifies Recently viewed. Label/ID-based traversal is required; this failure does not prove Recently viewed insertion itself is broken.
- Changes: none in frozen campaign.

O: studio_safety passes; studio_selection completes with30 QtTest results passing and2 failures above. Campaign receipt retains3 attempted CTest registrations (1 pass,2 failed),57 pending; installed service/release unchanged, zero restarts. P continues only the pending57 after this diagnosis. No build or acceptance pass.

Runner follow-up: optional ResumeRunId is now forwarded only when present, preserving ordinary Test invocations' parameter validation.

Graphical testing contract: after all60 CTest registrations have an attempted outcome, the continuation can stage a separately labelled diagnostic payload. Its receipt must retain `passed:false`, `acceptance:false`, `fullSuite:false`, exact chain/source/payload identity and zero pending registrations. `Visual -Diagnostic` verifies that receipt and hashes, forbids `-Installed`, and uses existing isolated fixture/service-lease protections. Ordinary Test/Deploy/Visual passing-payload checks are unchanged; a diagnostic payload cannot be deployed. Safety regressions reject incomplete/mismatched or falsely-passing diagnostic receipts. Installed-only cases remain explicitly blocked rather than running an older release or publishing a failed candidate.

### BUG-026 — Studio cutover registration exceeds its 60-second deadline
- Owner: T24 test/performance investigation; severity: high verification blocker; root cause unresolved.
- Evidence: P `report/ctest.log`, studio_cutover killed at60.33s. Seven actual test slots pass (including1024-component editing, back restoration, stale callbacks, move cancellation, Layers, long names and named-page exit) before the timeout; final selected-origin and teardown cases remain unrun.
- This is retained as a timeout, not converted into a feature pass or silently given a larger budget. Several undefined-color warnings also remain in the captured log; screenshot impact is not yet established.
- Changes: none to the frozen test/application.

P: six registrations pass (artwork/workflows/color interactions/performance/reuse/designs), cutover times out. Aggregate frozen campaign:10 attempted,7 passing,3 failed,50 pending. Q resumes only those50 after named-timeout diagnosis; new diagnostic-staging safety test runs first. No application/test-case changes or rebuild.

### BUG-027 — Web OFF disables local Kodi search
- Owners: T28/T03/T14; severity: critical offline/core feature regression.
- Evidence: Q `report/ctest.log`, studio_shell test_search_theme_change_preserves_query_focus_keyboard_and_requests expects3 local search requests, observes0;13 other shell results pass.
- Diagnosis: CommandCenter.updateMediaSearch returns early on `!webSearchEnabled` after invalidating all three **local Kodi** clients. This guard intended for external providers also suppresses Movies/Shows/Episodes. Default Web OFF therefore cannot search the local library, contradicting the displayed Local results only status and offline requirement.
- Independent production-JS reproduction: Q `local-search-off-repro.cjs` / `.log`: same open films query issues0 local requests with Web OFF and3 with Web ON; invalidation recorded.
- Changes: none; real feature defect retained. It does not prevent testing other registrations.

Q: studio_colors passes; studio_shell completes13 passing /1 failing QtTest results. Aggregate12 registrations attempted,8 passing,4 failed,48 pending. R continues only those48 after this named-failure diagnosis.

### BUG-028 — Source remote test leaks SignalSpy count between cases
- Owner: T25 test integration; severity: medium verification defect.
- Evidence: R `report/ctest.log`, test_overflow_is_bounded_and_menu_opens_source_actions line27 observes launchSpy.count2 instead of1. Five other QtTest results pass.
- Diagnosis: suite-level SignalSpy is reused; init/cleanup detach its target but never call clear(). Alphabetically preceding notice-suppression test deliberately emits one launch and asserts count1; overflow then attaches a fresh widget and adds another launch. Changing a SignalSpy target does not reset its recorded count. This is not evidence that one Menu press launched Sources twice in the product.
- Changes: none to frozen cases; retain the observed failure.

R: Store remote UI passes; source remote UI has the single fixture-count failure. Aggregate14 attempted CTest registrations,9 passing,5 failed,46 pending. S resumes those46; no application changes or rebuild.

### BUG-029 — Studio gestures registration exceeds its 60-second deadline
- Owner: T19/T20/T24 test/performance investigation; severity: high verification blocker; root cause unresolved.
- Evidence: S `report/ctest.log`, timeout60.04s, no assertion failure before termination. Nine actual data/test cases pass, including alignment/group transactions, ordered insertion, edge growth, invalid drops/resize, full-size and zoomed pointer/remote parity. Final resize-reflow case is unrun/incomplete.
- No deadline increase or application change; distinguish a registration budget problem from an unproven feature stall pending focused measurement.

S: one additional timed-out registration. Aggregate15 attempted,9 passing,6 failed,45 pending. T continues those45 after reviewing the named log.

Continuation storage clarification: resume-only tests retain the standing2GiB minimum. They avoid cleanup/backup hashing while above that minimum; below it the normal verified backup receipt and automatic guest cleanup run, pinning the original frozen candidate. No host cleanup and no checksum/path bypass. Earlier shorthand saying resumption never cleans is superseded by this explicit low-space guard.

### BUG-030 — Subtitle-input test contradicts its own explicit-import setup
- Owner: T14 test integration; severity: medium verification defect.
- Evidence: T `report/ctest.log`, subtitleInputsAreValidatedAndExplicitlyQueued line442: actual jobs2, expected0. Ten other QtTest results pass, including remote subtitle queue behavior and offline processing contracts.
- Diagnosis: this case first verifies two successful explicit repository imports and reads jobs[0]/jobs[1], then retains an older zero-jobs assertion and missing-model error expectation from before those imports were added. The observed two records are the intended explicit imports, not unsolicited jobs.
- Changes: none in frozen tests. Both the obsolete count and follow-on stale error expectation need test reconciliation.

T: grid, grid controls, destinations, ordered, policies and overlays pass; T14 has the one contradictory fixture assertion. Aggregate22 registrations attempted,15 passing,7 failed,38 pending. U continues those38; no application/test-case edits or rebuild.

### BUG-031 — New All Apps native test omits required application-version setup
- Owner: T26 test integration; severity: medium verification defect.
- Evidence: U `report/ctest.log`, allAppsProjectionContainsOnlyRegisteredLaunchables line63 rejects packaged catalog metadata;40 other IsoldeTests results pass.
- Diagnosis: this newly first-declared case calls catalog.load before setting QCoreApplication::applicationVersion. AppCatalog intentionally rejects packages with minIsoldeVersion newer than that empty version. Other catalog tests explicitly set0.0.1.2; production main sets ISOLDE_VERSION before loading. Therefore this is not evidence that the shipped catalog is invalid.
- Changes: none to frozen cases.

### BUG-032 — All Apps editor exposes internal APPGALLERY mode name as its heading
- Owner: T26; severity: low UI defect; native screenshot confirmed.
- Evidence: N `report/studio-test-named-tools.png` (also M diagnostic capture), reviewed at full contact-sheet context. The otherwise user-facing All Apps presentation dialog is titled `APPGALLERY`, exposing its internal mode identifier rather than the named All Apps control.
- Expected: a clear user-facing heading such as All Apps presentation from the shared editable labels/model.
- Changes: none.

U: Chromium remote and desktop mode registrations pass; isolde_tests completes40 passing /1 fixture failure. Aggregate25 registrations attempted,17 passing,8 failed,35 pending. V resumes those35.

BUG-021 runtime confirmation: V `report/ctest.log` fails GuideTests::packagedGuideOptionsValidateOverlayAndLegacyBoundaries at tst_guide.cpp216 (`resolved` empty). The test supplies a valid older Guide-options document, verifies the packaged resource exists, and then the66,351-byte packaged fallback is rejected by the64KiB guard. This is a proven compatibility regression, not merely source suspicion.28 other Guide contract results pass.

V: capture protocol/adapters and serial writer registrations pass; Guide contracts fail on BUG-021. Aggregate28 attempted,19 passing,9 failed,32 pending. W continues those32; feature regression remains unchanged.

### BUG-033 — Keyboard action labels overlap in Studio at all tested scales
- Owners: T12/T21/T24; severity: high remote-legibility defect; native capture confirmed.
- Evidence: P `report/studio-test-scaling-keyboard-1080.png`, also720/2160 captures; reviewed original1080 capture. Del/Space/Clear/Done/ABC/123/#+= text spills across adjacent cells as one overlapping string. Shift also exceeds its cell.
- Cause: ten narrow equal-width keys retain fixed18px labels without width fitting, wrapping, elision or wider action keys. The test verifies containment of the keyboard object, not its label glyphs.
- Changes: none. This is distinct from BUG-015's fixed-height overflow in Network/Store.

### BUG-034 — Games page/details SQL is malformed before managed artwork metadata exists
- Owner: T30; severity: critical catalog feature regression.
- Evidence: W `report/ctest.log` contains successful adds followed by empty pages/details; independent W `games-sql-repro.cjs` / `.log` executes the exact production fallback fragment against SQLite and gets a syntax error.
- Cause: GameLibrary page/details (and related query builders) append `QString(",'')")` when m_hasArtwork is false, adding an unmatched closing parenthesis. A newly created/non-artwork catalog therefore cannot return its stored games; dependent organization, artwork, backup and provenance operations report game-not-found.
- Affected W assertions: remoteSteamImportCanCancelReviewAndPlay; remoteDetailsFavoriteRemoveAndCancel; remoteEditionChooserDispatchesOwnedVersion; platformShelvesFilterCanonicalGamesRemotely; directFranchiseAndTagFiltersArePagedAndRemoteOperable; fiveThousandGameCatalogStaysPagedAndSearchable; catalogCapDisablesNextFromLastFullPage; organizationAndManualSmartCollectionsStayProfileLocal; catalogExportRestoreIsBoundedAndTransactional; catalogArtworkSurvivesRecoverySnapshotsWithoutGarbageCollection; catalogProvenanceIsImmutableAndConflictSafe. Each assertion/location/actual/expected is retained in W log.
- Minimal isolated SQL correction restores the row in the reproduction, but application code is deliberately unchanged; other tests remain runnable.

### BUG-035 — Optical revalidation test omits the device identity it expects to match
- Owner: T30 test integration; severity: medium verification defect.
- Evidence: W opticalIdentityAndMultiDiscReferencesAreDurable line257 expects replaced, observes missing.
- Diagnosis: fixture `wrong`/`unmounted` supplies media/mount/state/observation fields but never device=/dev/sr0. classifyRevalidation correctly cannot match the requested device. Follow-on selected-unmounted assertion has the same omission.
- Changes: none; no physical optical claim.

### BUG-036 — Games remote text helpers still navigate the old keyboard
- Owner: T30/T12 test integration; severity: medium verification defect.
- Evidence: W remoteAddReviewWritesOnlyOnConfirm line1294 remains field0 instead of1; remoteEditCanCancelAndApplyWithoutMouse line1240 remains keyboard instead of runtime.
- Diagnosis: shared doneOnKeyboard helper uses five Down/nine Right then Confirm. With the new57-key keyboard this lands on #+=, not Done (index53). Menu/field sequences also retain positional assumptions. These failures need corrected D-pad stimulus before product workflow conclusions.
- Changes: none in frozen tests.

### BUG-037 — Emulator test still expects Xbox360 to be unintegrated
- Owner: T30 test integration; severity: medium verification defect; source-confirmed, installed walkthrough pending.
- Evidence: W remoteEmulatorSetupCancelsAndSavesWithoutMouse line608; current versioned runtime matrix explicitly lists Xbox360/Xenia as integrated, while this assertion requires it in notIntegrated.
- Changes: none; this stale assertion neither proves nor qualifies actual Xbox360 gameplay.

W: Guide remote UI passes; Games UI completes19 passing /15 failing results, all failed assertions accounted for above. Aggregate30 CTest registrations attempted,20 passing,10 failed,30 pending. X resumes those30.

## Repairs applied — this session

Application/test/build/feature fixes committed to `agent-backlog-progress` (13 commits, tip `fc4def7`). Leave all VM/CTest verification and acceptance to the testing agent; no acceptance credit claimed here.

- **BUG-001 — Coding% total contradicts all 25 task rows.** `docs/agent-backlog.md`: Coding sum corrected to 2500 / 25 = 100% (all 25 active rows already at 100%); Overall table row updated accordingly. Commit `5f2db13`.
- **BUG-002 — Keyboard changes break test infrastructure and remote fixture input.** Oracle regex, `tools/guide_features.py` `keyboard_letters()`, and `tools/home-components-acceptance.py` remote_text now parse the production `OnScreenKeyboard.qml` `keyList()`/`letters` contract instead of removed inline arrays; Guide helper uses `UPPER_KEYS`/`KEYS`. D-pad/Confirm-only stimulus retained. Commit `cd40848`.
- **BUG-003 — KodiService header cannot compile after playback-preference integration.** `src/media/KodiService.h`: duplicate `markReady`/`changed`/`probeRequested`/`ready`/`unavailable`/`alwaysPlayLocallyChanged` declarations removed; signals consolidated under one `signals:` block; public constructor/destructor/API restored outside `private:`. Commit `e901410`.
- **BUG-004 — Gamepad topology uses a nonexistent Qt QSet API.** `src/sessions/GamepadTopology.h:50`: `availableAtLaunch.united(availableAfterLaunch)` replaced with copy + supported `unite()`, same union semantics. Commit `e4426ed`.
- **BUG-006 — Always Play Locally preference disconnected and does not restore saved state.** `src/media/KodiService.cpp`: constructor now reads `profiles/<id>/kodi/alwaysPlayLocally` (default true) so a saved `false` is restored per instance; setter/reset only update memory and emit `alwaysPlayLocallyChanged()` after `settings.sync()` succeeds, reporting an error and leaving the value unchanged on save failure. Commit `81a3f56`.
- **BUG-007 — Games optical refresh timer has no member declaration.** `src/games/GameLibrary.h`: `QTimer m_opticalRefreshTimer;` declared to match constructor use at `GameLibrary.cpp:449`. Commit `e4426ed`.
- **BUG-008 — Games uninstall combines incompatible auto declarations.** `src/games/GameLibrary.cpp:1823`: combined `const auto record=...,root=...` split into separate `record` and `root` declarations. Commit `e4426ed`.
- **BUG-009 — High Contrast setting cannot be activated.** `qml/DisplaySettingsView.qml:31`: Confirm handler tested unreachable `selectedIndex === 5`; High Contrast is `rows[3]` (five rows clamp to index 4). Changed to `selectedIndex === 3` so D-pad Confirm activates it. Commit `a713cea`.
- **BUG-011 — Optical adapter omits the syscall declaration header.** `src/games/OpticalDiscAdapter.cpp`: `#include <unistd.h>` added alongside existing platform headers so `::syscall`/`::ioctl` resolve. Commit `e4426ed`.
- **BUG-012 — SubtitleQueue QML contains syntax errors.** `qml/SubtitleQueue.qml:280-284`: four invalid trailing semicolons between signal-handler JS blocks and `background:` removed; handler bodies and styling retained. Main instantiates SubtitleQueue, so this unblocks normal launch. Commit `90b1d0f`.
- **BUG-013 — Emulator definitions use two more unsupported Qt APIs.** `src/games/GameEmulators.cpp`: `QSet<QString> actualKeys(value.keys())` built from stable list iterators; `QVariant::isDouble` replaced with `QJsonValue::fromVariant(...).isDouble()` plus the `<QJsonValue>` include. Commit `e4426ed`.
- **BUG-014 — Kodi playlist reorder does not compile.** `src/media/KodiMedia.cpp:229-241`: outgoing indices now a `QJsonArray` (not `QVariantList` passed as a JSON object value); duplicate detection via `QSet<int>::contains`/`insert` (not STL-style `.second` on a Qt iterator); explicit `toInt()` per entry. Commit `c8baf68`.
- **BUG-016 — All Apps/catalog native regression tests do not compile.** `tests/tst_isolde.cpp:80`: unparenthesized braced `QStringList` inside `QVERIFY` parenthesized; line852 `projected` declared as `const auto`. Assertions unchanged. Commit `1b4b34d`.
- **BUG-017 — Games component contract tests call the wrong schema signature.** `tests/tst_components.cpp:492-496`: required `false` legacy argument and existing `&error` output added to the three `ComponentSchema::component` calls. No assertion removed or relaxed. Commit `d3886ee`.
- **BUG-018 — Games UI/optical tests have missing and ambiguous Qt types.** `tests/games_ui.cpp`: explicit single-map `QVariantList` initializers; explicit `QVariantList{QString(...)}` replies to disambiguate `QDBusMessage::createReply` overloads; `#include <QSqlError>` added. Expected results and stimulus preserved. Commit `d3886ee`.
- **BUG-019 — Playback-preference commit deleted KodiService::retry.** `src/media/KodiService.cpp`: exact pre-regression implementation restored (`if (!switching()) { stop(); start(); }`). Declaration already present in header. Commit `e901410`.
- **BUG-020 — T25 source presets invalidate all packaged Studio options.** `src/home/HomeLayout.cpp:438`: shipped `detected-sources` and `source-notice` preset IDs (already schema-validated by `ComponentSchema`) added to the `studioOptions` allowlist; unknown IDs still rejected. Commit `599837f`.
- **BUG-021 — Legacy Studio-options fallback rejects the now-larger packaged defaults.** `src/home/HomeLayout.cpp:493,501`: both legacy fallback reads (`guide` overlay and `referenceViewport`) now accept packaged files up to 128 KiB, matching the primary read bound, so the 66,351-byte shipped `studio-defaults.json` resolves in memory without rewriting stored files. Commit `599837f`.
- **BUG-027 — Web OFF disables local Kodi search.** `qml/CommandCenter.qml:80`: `updateMediaSearch` no longer invalidates all three local Kodi clients and returns early when `webSearchEnabled` is false; only the web provider search is cancelled, so Movies/Shows/Episodes local queries proceed with Web OFF. Commit `1de91a1`.
- **BUG-032 — All Apps editor exposes internal APPGALLERY mode name as its heading.** `qml/PageStudioView.qml:1546`: the dialog heading fell through to `studio.mode.toUpperCase()` for the `appGallery` editor, producing `APPGALLERY`. Added an explicit `All Apps` branch before the uppercase fallback. Commit `d5ec030`.
- **BUG-034 — Games page/details SQL malformed before managed artwork metadata exists.** `src/games/GameLibrary.cpp:721,969,1169,1212`: the no-artwork branch appended `,"'')` (a stray unmatched close paren) in four query builders, producing a syntax error and empty pages/details for newly created non-artwork catalogs. Changed the placeholder to `,"''` matching the `COALESCE(..., '')` fallback at all four sites. Commit `fc4def7`.
- **BUG-010 — Authored screensaver pages not integrated.** `qml/Screensaver.qml`: converted bare declarative `if` blocks (invalid QML parse errors) to `visible:` bindings across slideshow/gradient/dim/clock modes, preserving the clock timer, hint, and tap area. Registered `qml/Screensaver.qml` in CMakeLists.txt next to the other Ambient QML files. Instantiation/wiring into the ambient flow remains a separate integration step. Commit `6371baf`.
- **BUG-015 — Expanded keyboard overflows its fixed-height Network/Store panel.** `qml/OnScreenKeyboard.qml`: wrapped the inner Column in a `Flickable` that clips and scrolls when content height exceeds the available panel height, so the hint text and full 57-key grid stay reachable within the 400px cap. Commit `e026275`.
- **BUG-033 — Keyboard action labels overlap in Studio at all tested scales.** `qml/OnScreenKeyboard.qml`: added `elide: Text.ElideRight` to key labels so long action labels (Del/Space/Clear/Done/#+=) and Shift stay inside their equal-width grid cells instead of overlapping neighbors. Commit `688683f`.

Campaign diagnostic-continuation harness (documented in `docs/vm-oracle.md`): `Test -ResumeRunId <failed-id>` reruns only unattempted CTest registrations against frozen candidate bytes, retains prior failures, enforces the 2 GiB guest minimum, and can stage a `diagnostic-payload.json` (`passed:false`, `acceptance:false`, `fullSuite:false`) inspectable by `Visual -Diagnostic` but never deployable or acceptable. Commits `5b54a85`, `5f2db13`.

Untouched per the campaign findings (testing-agent domain): BUG-005, BUG-022 through BUG-026, BUG-028 through BUG-031, BUG-035 through BUG-037. These require VM/runtime verification, focused measurement, pixel-perfect visual review, or deliberate leave-in-place fixture decisions; none were modified in this session.

### BUG-038 — New source/Games component fixtures omit required canonical source bindings
- Owners: T25/T30 test integration; severity: medium verification defect.
- Evidence: X component_contracts fails sourceFamiliesUseGenericBoundedComposition at860 (`Source binding does not match its action`) and gamesContractsRejectExecutableData at492.
- Diagnosis: fixtures pass raw prototypes without canonical `source` into strict `component(...,false)` validation, which explicitly requires input.source to equal the derived binding. Prototype normalization uses legacy/normalize mode; strict validation should receive the normalized record. No evidence here that the real normalized source widgets fail rendering.
- Changes: none.

### BUG-039 — Emulator manifest test hardcodes an obsolete runtime count
- Owner: T30 test integration; severity: medium verification defect.
- Evidence: X emulatorManifestIsBoundedAndVersioned line391 observes16 runtimes, expects10. Current checked-in matrix declares the expanded set.
- Changes: none; a runtime count is not per-runtime gameplay qualification.

### BUG-040 — Builtin-default skin assertion expects an empty obsolete composition
- Owner: T25/T22 test integration; severity: medium verification finding.
- Evidence: X skinSnapshotsSurviveResetAndArtworkChanges line710 expects restored.composition().isEmpty after applying builtin-default, but the packaged default now declares composition/source header controls. Prior saved-snapshot round-trip assertions pass.
- Diagnosis: compare against the current packaged default rather than assuming it has no composition. No product skin corruption is established by this assertion.
- Changes: none.

### BUG-041 — Add Button API fails for page and Studio page-lifecycle paths
- Owners: T26/T16/T23; severity: high observed feature failure; exact root cause pending.
- Evidence: X pagesPersistAndRouteAndTravelWithSkins line559: page->addButton returns false; studioPageLifecycleRepairsLinksAndPreservesRoot line1705: session.execute(addButton, page-id) returns false.
- Source inventory: these APIs construct minimal new action records with open-apps, unlike canonical Studio components; investigate generated-record validation/rollback and page errorString. Do not misclassify these as a proven stale selection index.
- Changes: none.

### BUG-042 — Invalid All Apps draft columns are silently accepted
- Owners: T26/T16/T23; severity: high validation/atomicity regression.
- Evidence: X studioHeaderValidationAndDraftRollback line2120: execute(pagePatch, composition.appGallery.columns=1) succeeds despite ComponentSchema requiring2–8 and test requiring refusal/no draft change.
- Diagnosis: HomeLayout.loadDocument strips invalid appGallery and accepts the rest as fallback; Studio pagePatch uses this loader, so a stored-read recovery path leaks into draft writes instead of rejecting invalid data atomically.
- Changes: none.

BUG-034 additional confirmation: X component_contracts also fails steamGamesUseProviderIdentityAndReviewedImport, gamesCatalogIsBoundedAtomicAndProfileLocal, gamesRelinkPreservesIdentityAndRollsBack, gamesFolderImportIsReviewedAtomicAndCancelable, gamesFolderImportPreservesManualInstallation, gamesEditionsKeepOneCardAndSeparateLaunchTargets, gamesCuratedPackagesBindThroughRealCatalog. Each reports empty page/details or dependent favorite failure. The fourteen X failures are all accounted for here and BUG-038–042;52 other results pass.

BUG-036 classification refinement: remoteAdd's fixed keyboard Done sequence is directly stale. The earlier remoteEdit assertion occurs before its Done helper and is also downstream of BUG-034's empty listing; its independent product/fixture root is not yet established. Do not attribute that earlier assertion solely to the keyboard helper.

## User-directed repair reconciliation — 2026-11-09

User reports a separate coding agent addressed findings. Working tree now has committed repairs through `6371baf` (the report alone remains untracked); preserve those changes. No VM run is active. Baseline N→X campaign is retained at31/60 attempted (20 passing registrations,11 failed,29 unattempted). It is explicitly superseded for forward testing by the changed repaired candidate, not completed or relabeled as passing. Raw logs/source archives/receipts remain the evidence for the original findings.

A fresh full run is justified: actual application changes affect local search, Games SQL, Studio fallback/defaults, keyboard layout, display and Kodi preferences, and add a registered QML component. Source identity changed, so resuming N's remaining registrations against these bytes would be dishonest. New candidate: `6371baf`; run `campaign-20261109-fix-a`.

Repairs are **unverified until retested**, with partial scopes retained:
- BUG-001: table/math say100%, but the current-scope headline still says97%.
- BUG-006: constructor/persistence repairs exist; UI and playback callers remain absent in current source inventory.
- BUG-009: reachable index repaired; effective persisted high-contrast model/consumer still needs proof (branch mutates appearance.contrast).
- BUG-010: agent explicitly says registration/syntax only; ambient instantiation/page authoring/rotation integration remains open.
- BUG-015: Flickable added; D-pad selection must scroll into view, not require pointer scrolling.
- BUG-033: elide added to a Text without explicit constrained width; screenshot verification required before closure.
- BUG-021/027/034 and other repairs will be checked against the new source; none has acceptance credit from source presence.

## Repair takeover batch — 2026-11-09

The user authorized this agent to own fixes, superseding the original report-only restriction. `campaign-20261109-fix-a` stopped before CTest: qmlcachegen rejects the newly registered Screensaver.qml at73:27 (BUG-043, build blocker). The component contained QML objects inside a JavaScript handler and additional invalid/dead clock code. No tests passed in this run.

The current uncommitted batch replaces that file with read-only shared Home rendering, adds profile-owned page selection/rotation to the actual ambient path, fixes keyboard selected-key scrolling and bounded labels, and saves High Contrast through the existing appearance model. It also rejects invalid composition draft writes, canonicalizes Add Button records, caches byte-identical validated Studio options, and repairs the diagnosed stale fixtures. These are pending VM verification; no findings are closed based on edits alone. Host navigation checks pass42/42.

Repair batch B focused evidence: `campaign-20261109-fix-b` built core/native components and ran component_contracts:63 passing cases,3 failing. BUG-042 now passes; Games catalog/provider/relink/import/editions/package cases now pass. Add Button still failed because it used a legacy launcher record; changed to the registered Button component. The new cache-invalidation test initially expanded Studio defaults past the128KiB boundary; corrected to compact same-size fixtures so it tests cache invalidation rather than oversized-file refusal. These three cases require retest.

Next coherent integration candidate additionally wires the saved local-playback preference to identity-verified discovery results (ambiguous/missing identities never auto-play), exposes Settings → Media → Playback Preferences, and retains direct library playback. The new host routing test passes; total43/43. Native shared-renderer/keyboard/contrast evidence is still pending. A full suite is justified by the cross-cutting app/QML/Kodi query changes and because focused component_contracts does not compile or exercise the full QML application.

### BUG-044 — Page component edits could report success after rejecting the change
- Owners:T16/T21; severity:medium.
- Source diagnosis: PageModel::setItemField silently replaced a failed canonicalization with the previous item, then persisted and returned true; visual widgets also skipped that validation. Changing actions forced every item to kind=action, invalidating Button/Image components. HomeLayout::setItemAction refused Button/Image widgets entirely.
- Repair pending verification: preserve registered visual kinds; canonicalize all page component edits and return an explicit validation error without mutation; allow only actionable Button/Image widgets through the root action editor. Added invalid-presentation/no-draft-change regression.

### BUG-045 — Discovery-region control ignores shared theme and remote keyboard
- Owners:T14/T27; severity:medium usability.
- Evidence: reviewed baseline dark/pastel Command Center screenshots show a blank white native editable ComboBox beside Linguist; its internal editor uses platform styling rather than the shared appearance tokens. Mouse and remote also opened different input paths.
- Repair pending verification: render the region as a shared-theme button displaying the saved code; mouse and remote Confirm open the existing TV keyboard, retaining validation and arrow shortcuts.

Integration C evidence: full application/QML compilation passes (including Screensaver). studio_quick completes in14.33s rather than the baseline60s timeout:19 cases pass,1 fails. Diagnosis refines BUG-022: the Reset row moved to index3, but Confirm on Cancel returned focus to index2 (All Apps). The fixture correctly expects the originating Reset row; fixed the product's Cancel return to match its existing Back path. BUG-023 timeout is resolved on C's frozen source, retaining this separate failure. Pending post-C batch also includes BUG-044 setter repairs and the new page-selection/isolation regressions.

Studio D: studio_quick passes13.75s, studio_safety passes3.99s; studio_selection has one remaining stale Recently Viewed navigation fixture (Down moves2 slots in the two-column catalog; the fixture used1). Fixed to move Right through the data-resolved index. Other selection cases, including nearest-card fallback, pass.

BUG-010 timed-idle completion: Main's old guard explicitly excluded search/All Apps/library and every active Settings/Store/Studio session, despite the overlay's correct stacking. The one-shot timer also stopped checking after an ineligible playback period. Allow internal surfaces and repeat bounded idle checks, while excluding playback/capture/external sessions and active transitions. Host production-method matrix passes (44 total host checks). Pending full VM/runtime verification; do not count source presence as closure.

Next full run is justified by the demonstrated C full-suite failure, resolved D selection failure, post-C atomic setter corrections, and material timed-idle/discovery dispatch changes. The incremental build cache is retained; no healthy run was interrupted.

Visual review: reviewed all15 native repair captures from C/D (three contact sheets under `build/test-campaign-repair-review/`, with exact source index). Color controls, scalar dialogs, orb/switch focus and Store Android/Chromium/add-web surfaces remain legible with visible selection. C's named-tools capture visibly confirms the wrong All Apps return before the D repair. These captures do not substitute for the pending keyboard/ambient/installed reference journeys.

Integration E:11/60 registrations pass, including Studio performance71.80s and cutover45.05s (BUG-026 timeout resolved). studio_shell:16 cases pass, including new keyboard selected-key visibility, ambient shared-page/cancel behavior and Web-OFF local search;1 contrast case fails. Diagnosis: shared palette supports highContrast but HomeLayout/PageModel theme validators omitted the boolean, so setAppearance correctly refused it. Added the declared boolean consistently to both schema paths, with typed rejection and reload regressions. The Display control targets the designated Home page when one exists. Also avoid instantiating a null-page Home renderer (eliminates its undefined-focus-color warning) and give the isolated search fixture its real changed signal.

Keyboard visual review E: native1080 capture now has no overlapping labels, but narrow action keys render as identical ellipses; this does not close BUG-033. Replaced those display labels with distinct conventional glyphs/short mode labels, retaining the full selected-key name in the fixed hint and accessibility name. The300px-panel remote-scroll test passes; the initial400px screenshot shows a partial lower row, which is scrollable, while the input and hint remain fixed. Full selected-action and scaling captures still require review.

Focused F:studio_selection, studio_shell and surface_appearance_contracts pass; contrast toggle now changes saved profile appearance. home_component_navigation reaches previously unattempted cases:16 pass,4 fail.

### BUG-046 — All Apps list fixture assumes insertion order
- Owner:T26 test; severity:low test failure.
- F expects Gamma after Beta, while the declared default alphabetical sort correctly returns Delta. Preserve the single-row-step test and correct the expected identity.

### BUG-047 — Discovery-region field traps horizontal navigation
- Owners:T14/T28; severity:high remote usability.
- F searchHeaderSwitchRemoteFocusAndStableResults cannot reach Web: region catches every Left/Right to cycle country, shadowing its own later navigation branches. Region Confirm already opens the validated TV keyboard. Horizontal keys now move to adjacent header controls; country editing uses Confirm. Pending retest.

### BUG-048 — Native focus-restoration fixtures invoke the old method signature
- Owner:T23 test; severity:medium test failure.
- F pageComposerAndIndependentKodiWidgets invokes PageSurface.restoreOrigin with1 QVariant; QML exports2 arguments (origin,force), so invokeMethod refuses before assertions. Pass explicit false in all3 native calls, preserving production behavior and original-origin checks.

BUG-040 additional fixture: F skinsAndDesignAreReachableFromSettings also assumes the packaged default composition is empty. Compare to a freshly loaded packaged Home while retaining the no-mutation confirmation assertion.

Focused G:19/20 native navigation cases pass; remaining search-header fixture still assumed one Left from Web reaches Query despite Linguist+Region additions. Corrected to traverse all three controls and assert Query before Confirm. BUG-047's real Web reachability now passes. Reviewed every101 E/F image (17 sheets and exact index under `build/test-campaign-repair-review/ef/`), including shell/network/Kodi/details, scaling/responsive states and color/preview/selection paths. Region styling is now readable; keyboard ellipsis refinement remains pending the next capture.

Freeze H application and test bytes for the remaining diagnostic campaign. If a registration fails, diagnose and report it, then resume unattempted registrations against these same frozen bytes without app/test edits. Gather remaining findings for one repair batch; retain every failed result and do not grant acceptance. This avoids replaying the passed slow Studio groups merely to uncover the next failure.

Pending final-batch review findings (H remains frozen):
- BUG-049 (T12): new AmbientSettings page-reference validation duplicates a narrower64-character/no-dot rule instead of ComponentSchema::safeId; imported valid Studio IDs may be refused. Reuse the shared validator and test a dotted ID.
- BUG-050 (T12): Reset Ambient restores visual mode but leaves the new page selection untouched. Clear/persist page selection on reset, retaining error/rollback handling.
- BUG-051 (T09): while waiting for a busy local client after selecting another discovery title, the prior title's provider rows may remain selectable. Hide provider rows during that pending selection and require the active selected-title context before showing them. This is a review finding in the new pending-selection path, not an original baseline failure.

Frozen H:29 registrations pass, games_remote_ui fails (27 cases pass,7 fail). Guide contracts/remote, subtitle/discovery, main native suite, Sources, Chromium and all Studio groups reached so far pass. BUG-029 gesture timeout resolved. Previously fixed Games add/edit/favorite/remote/emulator/optical observation flows now pass.

### BUG-052 — Optional Games text binds SQL NULL to required text columns
- Owner:T30; severity:high API/workflow failure.
- H directFranchiseAndTagFilters..., organizationAndManualSmartCollections..., catalogExportRestore... fail after reaching real catalog operations. setOrganization binds its default null QString summary directly; createCollection binds default null QString filter fields directly. SQLite NOT NULL columns reject otherwise valid omitted optional text. Normalize optional empty text at the SQL boundary; retain validation and transaction semantics. Pending frozen-campaign batch.

### BUG-053 — Multidisc ISO identity accepts an absent set identifier
- Owner:T30; severity:high identity validation.
- H opticalIdentityAndMultiDiscReferencesAreDurable accepts a two-disc descriptor with no volume-set identifier; parseIsoVolumeDescriptor falls back to per-disc identity even for multidisc content, undermining grouping/expected-disc identity. Single-disc fallback may remain valid; multidisc needs a stable declared set. Pending batch.

### BUG-054 — Games fixtures use duplicate runtime-only identity / incomplete edition launch data
- Owner:T30 tests; severity:medium test failures.
- H platformShelves... registers PC and PSP with the same local-test runtime and empty launch target; source identity can reject that duplicate. catalogProvenance... supplies an edition target without a game root; boundary explicitly refuses it. Supply distinct valid fixture launch identities/files rather than weakening production ownership validation. Pending batch.

### BUG-055 — Steam recovery-status transition mismatch
- Owner:T30; severity:pending diagnosis.
- H remoteSteamImport... expects the generic ownership/offline hint after changing providerStatus to another app ID, but steamStatus still prioritizes a persisted steamRecoveryStatus. Inspect whether the fixture expectation predates durable recovery or whether stale status is leaking; do not erase this failure or change routing before that distinction is verified.

Frozen H→I aggregate:36/60 registrations attempted,34 pass,2 fail (Games and studio_navigation);24 remain. component_contracts, surface appearance, studio_unit and home_component_navigation pass. This verifies BUG-041/042/044 repairs and same-size options-cache invalidation on H.

### BUG-056 — Native Studio navigation fixtures depend on stale catalog positions/capacity
- Owners:T21/T24 tests; severity:medium test failures.
- I studioCatalogAndContextualInspector uses a fixed catalog traversal that now selects Widgets, then expects Button variants. Resolve the Button family from current options and navigate to its actual index.
- I studioRemoteGesturesCollisionsAndAnchors's cancel matrix expects Move after adding a new component on a deliberately fully occupied page; actual mode is noRoom. Diagnose the exact catalog entry/required span, retain no-write/cancel/selection assertions, and use the correct capacity outcome rather than weakening placement validation.

BUG-053 refinement: the parser already checks volumeSet.isEmpty for multidisc content, but QByteArray::trimmed does not remove NUL padding. An all-NUL ISO set field incorrectly appears nonempty. Fix identifier padding normalization and retain the multidisc requirement.

Frozen J:studio_behavior repeats the same BUG-056 full-page Add/cancel fixture;5 other cases pass. No new root failure;37/60 attempted,34 passing registrations,3 failed,23 pending. Resume without edits.

BUG-055 diagnosis: the isolated test sets recoveryStatuses[42] explicitly and its SteamRecoveryProbe acknowledgement only records the ID; it does not emit the real SessionManager recovery-status change that clears the warning. Later generic-hint assertions must explicitly model that cleared persisted map (while retaining checks that another app's live status never overwrites current ownership). This is a fixture transition gap, not evidence to remove durable product recovery warnings.

Frozen K:profile/browser isolation passes; audio_backend has14 passing cases and1 failure.39/60 attempted,35 passing registrations,4 failed,21 pending.

### BUG-057 — PulseAudio fixture selects an output before its discovery transaction completes
- Owner:T12 audio test; severity:medium race/test failure.
- K pulseAudioRouteSelectionIsConfirmedAndBounded waits only for outputs.size()==2 (list sinks), then selects while get-default-sink is still active. Production explicitly rejects changes during an active backend command. Wait for the initial output's confirmed active flag, retaining missing-output/injection rejection, ignored-selection confirmation and unplug checks. No need to weaken the busy guard or replace an in-flight transaction.

Frozen L:appliance_installation, default_app_provisioning, android_provisioning, fdroid_store and store_lifecycle pass. games_provisioning fails36-case suite with2 failures/10 errors.45/60 attempted,40 passing registrations,5 failed,15 pending.

### BUG-058 — Expanded emulator manifest breaks pinned runtime provisioning
- Owner:T30; severity:critical integration/missing software.
- L runtime_manifest rejects the package set before inspecting/installing any runtime. config/game-runtimes.json now declares rpcs3/shadps4/vita3k/xemu/xenia/cemu, while provision-games.py's trusted version/binary pins contain only the original runtime set. This cascades into launcher registration, runtime fingerprinting and trusted bundle checks. Do not invent package versions or weaken manifest/signature verification; reconcile actual supported provisioning routes and complete missing runtime integration honestly.
- One related fixture still expects Original Xbox in notIntegrated after the manifest moved it; that assertion alone is not proof the new runtime works.

H screenshot review complete:91 captures;66 exactly match previously reviewed image SHA256 hashes,25 distinct captures reviewed on five sheets. The receipt is `build/test-campaign-repair-review/h-image-reuse.json`. Keyboard action symbols are distinct/readable at720/1080/2160; selected Done is fully visible in a300px panel with fixed input and full label/hint. BUG-015/033 have native behavioral and reviewed capture proof on H; installed input journeys remain separate.

Frozen M:steam_games fails30-case suite with3 errors;46/60 attempted,40 passing registrations,6 failed,14 pending.

### BUG-059 — Steam process observation fails on unrelated root-owned processes
- Owner:T30; severity:critical production runtime failure.
- M real process/session/termination tests raise ProcessObservationError on `/proc/1/environ` PermissionError. matching_processes scans every PID and treats an unreadable foreign-user process as uncertainty about this user's owned Steam session. Normal unprivileged execution therefore cannot observe/terminate its own verified game processes reliably.
- Repair batch: first verify PID directory ownership and exclude foreign UIDs from this user-owned session scan. Preserve fail-closed behavior for unreadable/malformed own-user process state and the session-token/app identity checks; do not grant broader access or inspect others' environments.

Frozen N:portable_game_installer passes; optical_exec_adapter has6 passing cases/1 fixture error.48/60 attempted,41 passing registrations,7 failed,12 pending.

### BUG-060 — Optical executable fixture rewrites a read-only file
- Owner:T30 test; severity:medium test failure.
- N test_refuses_symlink_script_nonexecutable_and_oversize changes its own file to0500, tests script refusal, then writes the next ELF fixture without restoring owner-write permission. This fails under the intended unprivileged guest before later assertions. Restore0600 only on that temporary fixture before replacing its bytes; retain0400/0500 refusal cases and executable staging/seal checks.

Frozen O:9 more registrations pass (VPN setup/launch, recovery, Kodi bridge/playback/artwork/media client, offline Guide, component health). native_profile_isolation has18 passing cases/4 failures.58/60 attempted,50 passing registrations,8 failed,2 pending.

### BUG-061 — Steam sandbox/native-host flags disagree in order
- Owner:T30/T12; severity:critical launch failure.
- O Steam sandbox isolation fails with native-host exit2 before its Python fixture executes. NativeSandbox emits --game-session then --steam-session, but NativeAppHost only consumes Steam first then Game. Thus every Steam game-session launch using both flags is rejected at argument parsing. Accept the two typed flags in either order (reject duplicates/unknowns) or standardize the producer; retain game-session process protection and strict command delimiter.

### BUG-062 — Controller fixtures encode the wrong capability words and old supported range
- Owner:T12/T30 tests; severity:medium test failures.
- O discovery/device-number fixtures put bit48 in the final64-bit word, representing key0x30 rather than BTN_GAMEPAD0x130; the real parser correctly rejects it. Encode the bitmap's highest word first, as the helper in the recognition test already does.
- Recognition fixture expects0x12f rejected, while production intentionally recognizes joystick0x120–0x12f as well as gamepad0x130–0x13f. Test the full declared range and reject its actual lower/upper neighbors, retaining keyboard/path/major/minor/overflow checks.

Frozen H→P complete:all60 registrations attempted,52 pass,8 fail,0 pending. Failed registrations:games_remote_ui,studio_navigation,studio_behavior,audio_backend,games_provisioning,steam_games,optical_exec_adapter,native_profile_isolation. Passing continuation alone does not erase those failures or earn acceptance. P stages a diagnostic-only hashed payload; no passing full-suite payload exists yet.

Post-freeze batch applies BUG-049–062 repairs. BUG-058 mitigation restores the documented ten actually pinned runtime routes and labels the six unprovisioned additions notIntegrated; it does not implement those six or close the expanded generation requirement. The declared Coding100% claim is not credible for that missing software. Retain BUG-058's missing-integration portion as open; provisioning of the existing supported matrix must work independently.

Added a bounded campaign_repairs runner selection covering all eight failed registrations plus changed shell/component/main boundaries, excluding already-passed slow Studio performance/cutover. Its self-test checks failed-registration coverage and target selection. This is focused repair evidence; a later full candidate remains necessary.

Focused Q:main native suite and studio_shell pass; games_remote_ui improves to31 passing cases/3 failing. Its new deeper failures locate two more BUG-052 null-text boundaries: manual collection update defaults and legacy backup restore's absent artwork hash (plus absent legacy summary). Normalize these optional values too. The provenance fixture now reaches compatible-runtime validation; use the already registered Steam runtime with a distinct positive app ID for its alternate edition, preserving immutable provenance assertions rather than inventing file-launch capability on the runtime-only fixture.

## Verified repair checkpoints (latest applicable evidence, not full acceptance)

- H's full application build resolves the compilation blockers BUG-003/004/007/011/012/013/014/016/017/018/019/043. Runner/keyboard parsing BUG-002 is covered by passing runner self-tests and the subsequent native builds.
- H→P passes the relevant groups for BUG-020/021/022/023/024/025/026/027/028/029/030/031/038/040/041/042/044/046/047/048. This includes all66 component cases, Guide contracts, Home navigation, Sources and the earlier Studio timeouts. BUG-005 also passes in the44-case host navigation set.
- BUG-009 has passing saved-appearance toggle and typed/reload regressions; BUG-015/033 have passing remote visibility tests and reviewed H scaling/short-panel captures. BUG-045's region code is readable in reviewed native captures; header reachability passes after BUG-047 repair.
- BUG-006 and BUG-010 now have implemented consumers/renderer/settings paths and focused unit/native evidence; installed playback/idle/return journeys are still pending. Do not equate those narrow passes with broad T09/T12 acceptance.
- BUG-037/039's earlier classification is superseded by BUG-058: blindly updating runtime counts to16 matched the altered manifest but did not prove provisioning. The final declared supported set is the ten pinned routes; the additional generation requirements remain open and visible.
- BUG-001's headline arithmetic is corrected. Claimed Coding100% remains disputed by the confirmed missing emulator integration; no acceptance percentages were increased.

Focused R:games_remote_ui improves to33/34. The final failure is version1 backup's explicitly stored null QString summary (a default value does not help when the map key already exists). Normalize all optional restore text at the SQL boundary with one sqlText helper; v2 restore, organization, collections, optical identity, distinct runtime fixtures and provenance now pass.

BUG-059 integration review: the native game host intentionally sets PR_SET_DUMPABLE(0). Its owned supervisor ancestry must also be excluded from game-child scanning, otherwise its unreadable environment causes the same false failure inside the private namespace. Derive bounded ancestor IDs from read-only process stat metadata, skip them, retain strict own-candidate errors and add a protected-supervisor regression. No process visibility or permission setting is relaxed.

Focused S:8 registrations pass, including every Games UI/component/Home/Studio/audio case reached. games_provisioning improves to35/36; the remaining test assumes the GBA core is absent on the machine, but the VM legitimately has it installed.

### BUG-063 — Runtime fingerprint negative fixture depends on installed machine state
- Owner:T30 test; severity:medium test failure.
- S expects ready=false while passing ready=true package receipts and using actual /usr runtime/core paths. Existing VM-installed files correctly yield ready=true. Bind an explicitly nonexistent temporary binary path for this negative case; retain the separate real fixture hashing/change-detection test and manifest/package validation.

Use the bounded runtime_contracts batch for the remaining installer/process/optical/native-namespace checks, reusing unchanged passing S app sources. It excludes passed Studio navigation and has a runner selection self-test. Final full integration is still required.

Focused T:games_provisioning passes36/36 on the honest pinned matrix. Steam still errors on an unrelated same-user protected desktop service (/proc/1590/environ) in three real-process fixtures. This refines BUG-059: production launch refuses execution outside its private namespace, but those fixtures scanned the full guest desktop. Preserve strict failure for unreadable candidate processes; give only these three fixtures a controlled private-PID view backed by their real /proc child entries. Actual pidfd ownership revalidation, real signals, save flushing, other-session preservation and the real native namespace isolation test remain intact. Foreign-UID and protected-supervisor filtering stays defensive; do not pretend blanket permission-error skipping is safe.

Focused U:31/32 Steam cases pass after correct fixture process scoping; only the real cancel/save-flush case remains. Its generic cleanup exception hides the reason (observation versus durable recovery). Preserve the exact existing safety behavior, expose the safe observation reason in stderr, and include the captured provider trace in that fixture's failure message before changing any shutdown logic. This diagnostic rerun is necessary to distinguish a real save/ownership defect from a fixture problem.

Focused V confirms the cancel failure remains in process observation, not save-journal update; the generic production observation error intentionally suppresses its underlying OSError. The isolated fixture now includes that underlying boundary exception in its failure trace (only its known child process paths), so the next run identifies the precise read rather than changing security behavior by guesswork.

The cancel-only permission failure suggests the process-exit metadata race (the two live-process probe/termination cases already pass). Harden the observer without claiming absence from permission denial: check kernel terminal Z/X state before environment reads and re-check it after a read error; only confirmed terminal/disappeared processes are excluded. Unreadable live candidates still fail closed. Added a dead-process/unreadable-environment fixture and exact controlled-path/state diagnostics if the real cancel case still fails.

Focused X:games_provisioning, steam_games (33 cases), portable_game_installer and optical_exec_adapter pass. The real native sandbox reaches two deeper issues.

### BUG-064 — Steam cancel observation races kernel process exit
- Owner:T30; severity:high lifecycle failure.
- The cancel/save test's remaining permission failure disappears when the observer checks/rechecks kernel Z/X terminal state around environment reads. Live-process unreadability still fails. X proves real save flushing/cancellation/other-session preservation with the terminal-state fix and its regression.

### BUG-065 — Protected private session bus is mistaken for an unobservable game candidate
- Owner:T30/T12; severity:high native runtime integration.
- X's actual isolated Steam adapter errors on /proc/3/environ after flag parsing is repaired. Private bus infrastructure is not necessarily an ancestor and may deliberately be unreadable. Identify the configured private Unix bus server with SO_PEERCRED, require matching UID and a bounded private address, and exclude only that kernel-authenticated server PID. Do not skip unknown unreadable processes or trust executable names. Pending actual sandbox retest.

BUG-062 fixture follow-up: the new rejected upper-neighbor0x140 needs six64-bit words; its helper allocated only five and aborted on a negative index. Allocate precisely code/64+1 words, retaining both boundary assertions and all device-number/path checks.

### BUG066 — optional storage inventory blocks healthy preflight
Run campaign-20261109-fix-y completed verified cleanup with 2.06 GiB free, then stopped when the optional detailed disk inventory exceeded 30 seconds. The runner now records that diagnostic timeout and continues; mandatory minimum-space, safe-path and checksum checks remain unchanged. Added a regression for the timeout receipt and propagation of other errors. Verification pending in fix-z.


Focused Z (campaign-20261110-fix-z): all five runtime_contracts registrations pass, including native_profile_isolation, Steam, provisioning, portable installer and optical adapter. This verifies BUG-061/062/065 and the BUG066 runner repair with 2.06 GiB free. Runner self-tests passed before cleanup. A final full suite is justified now by the combined application/session/schema/manifest repairs since frozen H–P; narrow passing groups are not acceptance. Next candidate: campaign-20261110-final-a.


## Final integration — November10

campaign-20261110-final-a passes all60 registered CTest groups, zero failures/skips, 413 seconds reported by the runner. Source digest 11dc489d5eeef504cddbb765cc30351e0bff09342ebb4668a744117349146aba. All98 runner self-tests pass. The complete passing payload is staged; installed release remains unchanged. Evidence: build/vm-oracle/campaign-20261110-final-a/report/ctest.xml, ctest.log, test-result.json and index.html. Graphical review and installed/offline acceptance remain separate. No task acceptance credit awarded.


### BUG-067 — Graphical Studio helper selects obsolete catalog position
The final-a graphical Studio journey stops at placed[clock]: the saved component is an App carousel because the helper still assumes Widgets is family5, while source-notice/detected-sources make its actual index7. Native application tests pass; the screenshot confirms the carousel was selected. Resolve Widgets by stable family ID from the frozen packaged catalog, then send ordinary remote keys. Keep exact Clock placement, rollback, byte-preservation and group assertions. Only the SHA-addressed graphical helper changes; reuse final-a's passing application without another full suite. Also reviewed97 final native captures:95 exact-hash reuse,2 new Search dark/pastel images reviewed; receipt build/test-campaign-final-review/image-reuse.json. Node navigation44/44 passes.


Graphical Studio attempt013707 passes in385 seconds,34 captures reviewed across six sheets in build/test-campaign-final-review/studio. Profile hashes unchanged and normal service restored. Guest updater banners obscure part of the top edge: retain that visual limitation, do not claim those pixels verified. Group/content, move/resize, delete confirmation, Apply failure, color/contrast warnings and header/scrolling state reviewed. No new product defect seen.

### BUG-068 — Reference helper assumes obsolete default header order
Lounge attempt014336 never applies its first skin: packaged headerControls are sources/settings/edit, but the helper sends only one Right and enters Settings instead of Edit Home. Subsequent blindly scripted keys enter an unrelated external desktop workflow. No credentials entered; failed frame retained. Resolve Edit by ID from packaged defaults and reset header position with bounded Left before selection. Reuse the unchanged passing payload.

### BUG-069 — Graphical reset-cancel claim exercises All Apps instead
Review of Studio screenshot resetcancel shows All Apps presentation. The helper sends Down2 from Canvas view, but Reset interface is index3; its unchanged-bytes assertion can therefore pass without visiting Reset. Correct to index3 and require the actual Reset confirmation capture on rerun. Native Studio quick already tests the real cancel focus path; no application change or repeat full CTest is needed.


### BUG-070 — Reference personal-skin naming uses obsolete keyboard Done route
After BUG-068 repair, Lounge applies its built-in skin but cannot save the personal skin: Down20 clamps to the final mode control, entering Symbols instead of Done. Screenshot confirms the name keyboard, not a save failure. Reuse the existing production-layout parser from guide_features.keyboard_letters and select the named Done using D-pad row/column moves. Keep personal-file and exact-byte round-trip assertions; no application change or repeat full suite.


### BUG-071 — Reference default predicate rejects the exact packaged default
Lounge attempt014813 applies and saves the personal skin, then fails Default selected because the helper requires composition to be absent. Archived private home-layout.json actually equals the new packaged composition {schemaVersion:1,enabled:false,headerControls:[sources,settings,edit]}, with all16 exact packaged component IDs. Compare composition to packaged defaults, not absence. Apply the same correction to reset verification. Also repair the shared BUG-069 Down2 reset selector in the reference journey to Down3. Save/reapply/reset exact-byte, category, theme and preservation checks remain strict.


Reference functional results: Lounge attempt015055 passes102 seconds; Pastel attempt015242 passes182 seconds, including personal-skin exact-byte round trip, app overflow/return, local history artwork and full listing, Kodi albums/tracks, Games/Guide/Search routes, scoped Reset/Apply with preserved pages and native profile hashes. Screenshot review finds a persistent guest keyring dialog obscuring the center, plus updater banner. These frames are NOT accepted as clean visual references. Resolve guest-only obstruction through an existing bounded shell-input checkpoint, then recapture. No password, unlock or host maintenance is authorized/needed. Studio rerun015551 continues solely to verify the corrected real Reset cancel checkpoint; do not interrupt it.

