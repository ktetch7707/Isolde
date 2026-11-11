# Native and runtime acceptance reconciliation

This reconciliation applies the standing independently creditable milestone rule.
It adds no task weight and does not certify installed workflows from unit tests.
Each newly split group has two equal children: the named automated production
contract, and all remaining original requirements including integrated runtime
acceptance. Known defects stay in the second child. Existing splits are preserved.

Evidence: `build/vm-oracle/round2-20261111-b/report/LastTest.log` and `ctest.xml`:
61 registrations passed with zero skips. The current integration candidate is
`round2-20261111-integration-l`; only its completed results can supersede baseline
evidence. Production boundaries below are unchanged except the explicitly pending
Kodi keymap, visual color/fallback repairs and shared Studio viewport repair.
Those changes require the current integration and affected installed review;
they do not invalidate independent catalog, provisioning, storage or protocol tests.

| Group | Completed automated contract | Remaining acceptance, explicitly not credited |
| --- | --- | --- |
| T04.2 | desktop_mode: identity/permission refusal and owned Desktop session lifecycle; native_profile_isolation: owned process cleanup and graceful state flush. | Guided first-library setup, full Kodi ownership handoff and installed workflow. |
| T04.4 | kodi_local_bridge21 tests: atomic Kodi-owned playlist creation, bounded catalog/members, existing-file preservation; kodi_media_client saved playlist/shelf; Studio remote draft picker. | Complete edit/reorder/delete, real picker and playback presentation. |
| T04.5 | kodi_media_client: configured scan/cancel/observe/rejection and artwork cancellation; kodi_cached_artwork4 tests: managed bounded read boundary. | Complete title-art/source/content editing and installed cancel/rollback workflow. |
| T09.1 | kodi_media_client localPlaybackPreferenceIsPersistedAndProfileScoped proves default ON, restart, separate profiles and reset. | Complete Settings route and installed remote toggle/failed-save behavior. |
| T09.2 | kodi_media_client playbackLifecycle, televisionHierarchyAndEpisodePlayback, episodeSearchAndShowResult and actual QML televisionLibraryRemoteFlow prove typed local actions and hierarchy. | Installed preference-ON shelf/search/resume/context matrix. |
| T09.3 | kodi_playback32 tests exercise owned audio/subtitle operations and local-video identity; library playback remains Kodi-owned. | Complete preference-OFF/discovery/viewing-choice integration and real resume/watched behavior. |
| T10.2 | capture_serial_writer10 tests plus capture_protocol7 and audio_backend15 exercise serial commands, injected hotplug/preview, confirmed route selection, timeout and malformed responses. | Complete virtual-device integration, reconnect matrix and published physical measurement handoff. |
| T10.3 | Component schema/dispatch and shared QML shell settings tests exercise validated data and editor appearance; capture labels and discovered audio-output refusal pass. | Complete remote editors, no-write preview/cancel, installed success/error/focus; known Capture clipping remains open. |
| T12.1 | audio_backend15, interface_sounds8 and shell appearance tests prove audio confirmation, atomic profile preferences, policy/teardown, contrast and saved-display behavior; actual themecontrols PCM verification is recorded in the campaign. | Power is a known missing software route (BUG-078); complete onboarding/accessibility/localization review. |
| T12.2 | ShellAppearance actual QML Network/keyboard tests prove remote focus/cancel, password/cursor preservation and visible selection in short panels; Store remote form/keyboard tests pass. | Complete field/shift/symbol/Back/unobscured installed keyboard matrix. |
| T12.3 | native_profile_isolation22, desktop_mode12, web_remote and inputManagerMapsRemoteAndMediaKeys prove bounded owned return, denial, exact input mapping and process cleanup. | Complete remapping/hang/retry/offline/external-window installed matrix. |
| T12.4 | vpn_setup34 plus vpn_launch prove profile-scoped capture/switch rules, authorization, route handoff, failed handoff preservation and cancel/no-write. | Actual isolated virtual topology TCP/DNS/IPv4/IPv6 leak and complete themed installed matrix. |
| T12.5 | actual QML ambient-pages/shared-model/private-cancel and screensaver-covers-gallery/restores-selection tests pass. | Complete timed-idle/rotation/content/dismissal/profile installed matrix. |
| T12.6 | default_app_provisioning18, android_provisioning11 and native/profile browser isolation prove pinned setup refusal/rollback, scoped app roots, URL boundaries and retained browser state. | Published per-app supported matrix and actual available content/playback/session/return integration. |
| T29.1 | IsoldeTests onlineStoreRefusesUnknownSourcesAndRequiresOptInAndHash plus fdroid_store14 prove explicit opt-in, bounded metadata and cryptographic repository trust. | Complete provider abstraction/local catalog/OFF integration audit and installed acceptance. |
| T29.2 | fdroid_store14 and store_lifecycle18 exercise signer/index/hash/ABI/runtime rejection and transaction rollback. | Complete installed download/verify/install/update lifecycle. |
| T29.3 | StoreRemote and StudioSelection actual QML form, menu, platform install/open/remove/cancel and large-text action tests pass against explicit catalog boundary fixtures. | Real package presence/provenance/progress/focus through installed Store; fixtures do not prove actual installation. |
| T29.4 | fdroid_store14, store_lifecycle18 and storeChangesOnlyTheActiveProfile cover rejection, hash/signature corruption, rollback and active-profile isolation. | Final integrated write/crash/network-denial review across actual installed package paths. |
| T29.5 | profile_browser_isolation performs actual embedded-browser persistence/isolation cycles; customWebAppsPersistAndRejectUnsafeInput and webKioskSessionExposesSafeLaunchUrl pass. | Actual Store package install/open/remove plus disconnected Generator journey. |
| T29.6 | web_remote drives actual WebEngine controls with direction/activation/text cancel/page actions; native focus/origin contracts pass. | Complete external Chrome and embedded installed full-origin matrix. |
| T29.8 | store_lifecycle18 and android_provisioning11 exercise package transaction, consent, verification/rollback/repeat boundaries and private setup. | Actual shared Store/setup/update/remove/SmartTube workflow; privileged Android support remains unresolved. |
| T30.1 | games_provisioning47 validates explicit versioned runtime matrix, locked artifacts, formats, executable/core fingerprints and tamper refusal; gamesContractsRejectExecutableData passes. | Full supported/licensing/storage/save/budget inventory and common installed playable route. |
| T30.2 | GamesUiTests actual remote add/review/details/edition dispatch/cancel and canonical manual registration pass. | Meaningful actual gameplay and exact offline return; a menu or process spawn is insufficient. |
| T30.3 | ComponentTests Games bounds/atomic/profile/relink/import/edition contracts and GamesUiTests export/restore/provenance/art-recovery/manual-edit tests pass. | Combined installed import/relink/export/restore and transactional recovery review. |
| T30.4 | GamesUiTests real renderer/input verifies platform/franchise/tag filters, smart/manual collections and5000-entry bounded paging/local search. | Installed shared-page/Studio presentation and complete canonical shelf/input-parity review. |
| T30.5 | steam_games34 drives production discovery, identity, denial/offline/cancel/retry, journal recovery and actual tagged-process graceful save flush without touching another session. | Installed Steam common game lifecycle and actual meaningful gameplay/return. |
| T30.7 | portable_game_installer49 exercises real CLI preflight/install/recovery/uninstall and bounded archives/native ELF, source mutations, race refusal and safe save preservation. | Remote Isolde install/consent/error/recovery plus actual playable installed positive route. |
| T30.8 | optical_exec_adapter7 plus GamesUiTests virtual optical identity/approval/revalidation/replacement/removal/busy-eject tests exercise actual adapter contracts. | Combined installed virtual optical UI/Install/Play/retry and disc-required gameplay. |
| T30.9 | native_profile_isolation22, Steam owned tagged-process tests, optical contracts and actual5000-game UI scale cover security/session/input/save boundaries. | Final disconnected cold boot, common gameplay, runtime integration matrix and current reference pairs. |

T04.1 import preservation, T04.6 complete capability inventory, T04.7 Kodi-free
setup, T09.4 installed six-row matrix, and T30.6 complete runtime routes receive
no new credit here. Their existing broad gates lack sufficient complete evidence.
Passing a command builder does not prove runtime provisioning or gameplay.

Current integration-l completes61/61, zero failures/skips. Installed release
0838e68c4de00a86ee395580 starts active/running with zero restarts. Its native
capture index reuses122 exact reviewed images. All five changed originals were
agent-reviewed: temporary-path icon import preview, three real Kodi details
palettes, and Sources green/ivory custom contrast. Text/focus are readable and
no new clipping defect is observed. These are labelled native fixture images.

T25.2.a earns existing schema proof from sourceFamiliesUseGenericBoundedComposition;
T26.5.a earns legacy/skin/registered Android/Store/VPN/registry-removal input proofs.
T28.4.a now earns actual QML meta-object cancellation/retry/deadline evidence from
integration-l, together with T14 OFF/cancellation and Search no-provider tests.
T28.5.a earns the audited native state/palette/focus/no-provider matrix: actual
QQuickWindow directional and mouse input, empty/late results, failed-write state,
profile isolation, long labels and theme-change stability. Generic Qt Switch
checked accessibility is inherited, not replaced by painted pseudo-results.
The combined installed T25-T28 matrix stays open.

Installed replay correction: helper pass flags for Search/Lounge did not establish
visual acceptance. Original captures show a guest keyring modal and desktop disk
banner. The batch is rejected/cancelled under BUG-096; no installed milestone is
credited from it. Native integration and its reviewed images remain valid.
Current79% is independently credited native contract evidence, not these captures.

Clean installed Search attempt20261111-142420-318 supersedes the obstructed
attempts. Both original Search/results and recovery images are agent-reviewed:
no guest overlays, genuine labelled top-right Web Off Switch, visible results
focus, readable local results and exact Settings focus recovery. The helper
proves socketErrnos101/101 with local UNIX sockets available and unchanged layout
hashes. T28.1.b is complete with this installed presentation, the current native
switch input/state/result-count/palette tests and generic checked-accessibility
audit. Combined request/profile/provider acceptance remains in the other groups.
The initial Search-home image remains separately reviewable, not implied by these
two frames. No real-Kodi search-result proof is inferred from Settings results.

Final clean installed reference evidence: Lounge attempt20261111-142504-240
(14 PNGs) and Pastel attempt20261111-142807-740 (34 PNGs) pass recreation,
saved-design round trip and reset with unrelated pages preserved. All48 frames
are agent-reviewed through contact sheets; both reference recreation/reset pairs
retain the generic renderer, normal profile hashes are unchanged and service
returns active/running with zero restarts. No desktop prompt obscures these frames.

T25.5.b closes the persistence/isolation group's integrated-candidate gate:
current61/61 contains source injected-root/profile/no-media-mutation/atomic
rollback proofs, the reviewed custom Sources contrast regression and the repaired
Studio cutover; installed clean reference pairs pass and actual Kodi stop/history
replay/return assertions pass. SourceManager production code is unchanged from
the independently proven native policy/isolation evidence. The combined source
presentation/action/default-slot requirements remain in T25.1-.4; none is inferred
from references. T25 is60%, not100%. T28 is60%, with its real installed header
switch verified separately. Sum1994/25=79.76%, rounded80% under standing rules.

History attempt20261111-141858-974's seven frames were also reviewed: actual
synthetic color-bar video, history update, focused history replay and repeated
exact return are visible. The old disk banner limits header/presentation claims
for that attempt; functional video/ownership/Stop assertions remain evidence for
BUG-089. No complete media/setup acceptance is inferred from backend fixture setup.
