# Round-two VM verification — 2026-11-11

User scope: test everything in the backlog not yet VM verified. This supersedes
the previous rerun deferrals for Settings, interface sounds and expanded Games
runtimes. T13/T31 remain excluded; existing earned evidence is preserved.

Status: in progress. No acceptance credit awarded at campaign start.
Branch: `agent-backlog-progress`. Preserve the six pre-existing modified Studio
test files (destinations, grid, ordered, overlays, policies and store).

## Evidence

- `round2-20261111-a`: transport blocked by sandbox before guest connection;
  localhost SSH port 2222 returned Permission denied. No compilation/test ran.
- `round2-20261111-b`: same supported full Test launched with network access.
  Initial connection succeeds; guest free bytes 1760636928 trigger the runner's
  verified automatic cleanup. Full Test passes **61/61**, zero failures/skips,
  in 234.29 seconds; runner self-tests pass **102/102**. Cleanup reached
  2.04 GiB, honestly below its 3 GiB target but above the 2 GiB test minimum.
  HEAD `22c2d46b425710f756b2c5eed8a29fa7b0e7906c` plus preserved working-tree
  test edits; runner source digest
  `83bfed436030657839cf4f4738a0f1a6901a4ff407a7b83123c7204c8bb47fbe`.
  The snapshot archive SHA256 is a different identity,
  `24c5f08ef7700f1483b659a640be0d00441b1b6792686b46f37b56cdc7b31a91`.
  No deployment occurred. Native result: service active/running, zero restarts.
- Same payload `studio-search` passes the denied-network local-search/Back
  journey. All three frames reviewed; Web Off, local results and exact Settings
  focus after Back are visible. Both normal layout hashes are unchanged.
- All **126** native PNGs reviewed via `review/native-00.jpg` through
  `native-20.jpg` (six labelled images per sheet). These cover icon rendering,
  editor/color/framing/movement/scaling, search, gallery, shell appearance,
  Store and browser-remote fixtures. No new actionable visual defect identified
  from these frames. Deliberately low-contrast edited colors have warnings;
  fixture-only images do not prove installed services or playback.
- Initial staged Lounge/Pastel functional journeys pass; raw helper review
  status stays pending. Agent review and exclusions are recorded below.

Installed follow-up: Deploy succeeds as release
`8035d4d9a66f50808dd9ec30`. Corrected Lounge
`attempt-20261111-060652-258` and Pastel `attempt-20261111-060944-272` pass;
all48 screenshots reviewed, including actual default Home reset frames.
BUG-079 is fixed/verified against this same payload by the helper's explicit
Home route. Both normal layout hashes remain unchanged. Earlier staged14+34
frames are also reviewed; their two default Settings frames are excluded.

Five weighted milestones are independently credited from the full native/QML
evidence: T25.3.a and T26.1.a/.2.a/.3.a/.4.a. T25 becomes30%, T26 becomes40%;
sum1613/25 rounds to65%. `backlog-progress.ps1 -CheckTable` passes.

T14 source/evidence reconciliation then splits each of its two original groups
into native boundary proof and remaining combined installed acceptance, with
unchanged original weights. T14.1.a/.2.a are verified by the nine passing
`T14Tests` cases: controlled loopback HTTP with explicit fake credentials,
region/identity/attribution/OFF/cancellation and credential-limited downloads;
actual managed Unicode SRT/provenance/review writes; deterministic translator/
generation/model process fixtures; QML queue and track controls. Real model
quality and combined installed provider/Linguist journeys are not inferred.
T14 becomes50%; acceptance sum1663/25 rounds to67%. Other new credit remains
T25 30% and T26 40%; no broad group is marked complete.

Store `attempt-20261111-061410-276` fails before SmartTube installation.
All three captured frames reviewed. Its explicit error is missing/untrusted
privileged Android transaction support, with Store actions → Set up / repair
recovery. This is a setup prerequisite failure, not proof Android registration
or playback passed. Source boundary: `android-session.py::administrator_transaction`
checks the root-owned `/usr/lib/isolde-android-setup/android-setup.py` and its
ancestors. No authentication bypass or package-data mutation was attempted.
Pages/playlists behind the failed batch were not attempted.

Harness verification: `round2-20261111-harness` passes103 runner self-tests.
Verified guest cleanup fails its3GiB target; retains the failure and cleanup
receipts. Later transport measures2054688768 free bytes (1.91GiB), still below
the ordinary2GiB Test minimum. No host cleanup or extra application full run.
The first extended `themecontrols` attempt stops before shell creation because
the guest lacks `pactl`; this is test setup, not a sound playback failure.
The bounded `audio-test-tools` Configure target installs only
`pulseaudio-utils` through existing guest delegated apt permission. Result
successful (`report/audio-test-tools.json`, bounded95-second install). Actual
audio consumption remains unverified until the PCM checks run.

Actual sound follow-up: the temporary default-output correction produces a
dark preview with17742 nonzero samples (peak98). Its first disabled recording
has only65536 bytes and correctly fails the minimum duration; no silence pass
is inferred. Low-latency recording plus bounded sample collection resolves
that measurement defect. `attempt-20261111-063641-779` passes dark preview and
disabled silence, then encounters BUG-086 during palette restart. Its six
frames are reviewed and exact audio-output restoration is recorded. The
complete dark/pastel/enlarged-custom run passes as detailed below.

Playlist overlay rerun `attempt-20261111-063921-655` passes; all seven frames
reviewed. UI playlist creation/cancel and actual Kodi file/content checks pass.
Playback/pause/stop are deliberately invoked through the production bridge,
not through a remote playlist-item picker; video is not foregrounded in those
frames. The receipt explicitly retains `picker_ui_verified:false`. This is
backend playback evidence, not full remote playback acceptance. BUG-085's
keyboard repair is verified; remaining T04/T09 workflows are open.

## Required coverage

1. Frozen current-source registered native/QML/Python suite, including prepared
   Settings, interface-sound and emulator regressions. Diagnose each named
   failure before campaign resume; retain every failed result.
2. Missing/source-affected staged graphical workflows and both reference pairs,
   with review of every capture. Passing automation alone is not acceptance.
3. Installed software workflows for sources, All Apps, search, Store/Android,
   Kodi/library/playback, system/input/audio/idle/VPN, capture, discovery/
   subtitles and Games; production-boundary fixtures where prescribed by
   `vm-software-completion.md`.
4. Actual VM sound output/loopback, offline cold-reopen, profile isolation,
   atomicity and supported runtime/error/recovery contracts. Reuse applicable
   prior evidence explicitly; do not count missing routes as verified.

Report each newly observed bug with owner, stimulus, expected/actual result,
immutable evidence and verification state. Transport/infrastructure blockers
are distinct from product assertion failures. Acceptance accounting changes
only for completed owning milestones with evidence.

## Findings

### BUG-078 — Power controls have no implementation (T12.1)

- Actual VM stimulus: Lounge reference journey enters Settings → System &
  Support and selects the last Power row.
- Expected: T12.1's power software workflow is implemented and testable through
  a controlled production boundary; actual VM shutdown is unnecessary.
- Actual: row reports Unavailable and says OS power/restart controls are not
  available in this build. `qml/SystemSettingsView.qml` declares that status
  unconditionally and its page is explanatory text, with no power adapter.
- Evidence: `visual/lounge/attempt-20261111-055848-348/` owned-window images
  `lounge-settingssystem-window.png` and `lounge-settingslast-window.png` under
  the round2-b evidence directory. Both agent-reviewed at full resolution.
- State: confirmed missing software, open; no shutdown/restart requested or
  attempted. Native green tests do not cover this advertised scope.

### BUG-079 — Reference reset screenshot records Settings (test harness)

- Lounge's reset transaction verifies the persisted default layout, but its
  final `lounge-default-window.png` visibly shows Picture & Sound Settings.
- The helper returns through two Back presses and then Up/Down after Apply;
  that sequence does not guarantee Home after a Settings-origin Studio session.
- Evidence: `visual/lounge/attempt-20261111-055848-348/`. Reset dialog and data
  assertions pass, but exclude the final default frame from Home-reset visual
  acceptance. This is distinct from the older Studio `resetcancel` mismatch.
- State: confirmed harness defect, open. Fix the route and rerun the reference
  helper against this same immutable payload; no application rebuild needed.

### BUG-080 — Home navigation fixtures omit version context

`home_component_navigation` passes but four legacy/appearance functions emit
`HomeScreen.qml:463 ReferenceError: isoldeVersion is not defined`. Production
`src/main.cpp` supplies that context; affected isolated engines do not. This is
a fixture defect, not evidence that the installed version label is broken.
Evidence: round2-b `report/LastTest.log` lines 1200–1211. Open.

### BUG-081 — Queued viewport callbacks survive object teardown

`pageComposerAndIndependentKodiWidgets` emits delayed-function TypeErrors from
`StudioGridEditor.qml:196` (`scroll.reveal`) and `HomeScreen.qml:145`
(`composed.viewportRect`). Both functions exist on live objects; the queued
callbacks encounter objects after their QML methods have gone away. Evidence:
round2-b `report/LastTest.log` lines 1207/1209. Reproduced in VM lifecycle
testing; installed user-visible impact remains unproven. Open.

### Additional warning inventory requiring classification

- Hosted More Apps fixture: two undefined QColor assignments at
  PageAppCarousel.qml:108–109.
- StoreRemote add-web fixture: incomplete generated entry lacks publisher and
  description; palette/model bindings emit undefined QColor/QString warnings
  at AppStoreView.qml:233/237/249/252.
- StudioAppPicker in `studioRemoteGesturesCollisionsAndAnchors`: repeated
  double-to-int assignments for font sizes at lines 78/87/92/93. Inspect
  startup/teardown geometry before attributing a visible product failure.

All remain visible in the immutable LastTest log despite 61/61 success. Do not
silently call warning-free or infer a runtime screenshot defect from them.

### BUG-082 — Closing Search cannot cancel its Kodi clients (T03/T28)

- Actual staged offline-search and staged/installed Pastel `shell.log` files
  repeatedly report `CommandCenter.qml:107 TypeError: Property 'invalidate'
  of object KodiMedia is not a function`.
- `invalidateMediaSearch()` calls the movie, show and episode clients in order;
  the first call throws, so the intended invalidation chain does not execute.
- `KodiMedia.h:108` declares `invalidate` as an ordinary public C++ method,
  without `Q_INVOKABLE` or slot exposure. C++ tests invoking it directly cannot
  establish the QML bridge contract.
- Evidence: round2-b `report/evidence-studio-search-1794405527598897875/`
  and `report/evidence-pastel-1794406449655215884/`, each disposable fixture's
  `shell.log`, plus the exact frozen source. Focus return visibly succeeds;
  late-result/cancellation behavior is defective and requires regression proof.
- State: confirmed installed product defect, open. No product code changed.

### BUG-083 — Installed shared widgets receive undefined colors (T21/T27)

Installed Pastel logs reproduce undefined QColor assignments in
`PageAppCarousel.qml:108/109` and `GuideWidget.qml:53/58/65/66/106`. The same
carousel warning appears in a hosted More Apps native fixture. This is an
actual installed binding defect, not solely an incomplete isolated test map.
Evidence: round2-b installed Pastel shell log in
`report/evidence-pastel-1794406449655215884/`. Reviewed frames remain legible;
timing, originating appearance map and any transient visual impact need
diagnosis. Open; do not infer a blank or unusable screen from this warning.

### BUG-084 — Pages acceptance replays obsolete Settings/Studio menus (harness)

- `pages/attempt-20261111-063030-186` times out waiting for the authored page
  file. All four frames reviewed: blank/shelfplacement/blankcomponents are
  actually Network Settings/dialogs; the failure frame is the setup guide.
- `tv-library-acceptance.py::enter_editor` enters Settings but never selects
  the reorganized Home category and Studio row. Its subsequent dynamic menu
  offsets and catalog/keyboard sequences also predate Studio simplification.
- Real disposable Kodi import, seasons, episodes and local artwork checks pass
  before this harness failure. No page creation/playback pass is claimed.
- Supported Status after failure confirms no active runner, service running
  with zero restarts, both normal layout hashes unchanged, identical three
  WireGuard connection UUIDs still inactive and identical IPv4/IPv6 rules.
  No corrective network mutation was needed or performed.
- Open harness repair; preserve failure, do not repeat its blind key sequence.

### BUG-085 — Playlist helper selects keyboard mode instead of Done (harness)

`playlists/attempt-20261111-063436-251` reaches the real playlist keyboard and
passes cancel/no-file, then times out waiting for the saved playlist. Its
Right80/Confirm sequence selects the keyboard's final mode control rather than
Done. The name frame and failed log are retained; this helper produced no
failure screenshot. No playlist or playback pass.
Repair uses the shared production-layout-derived `remote_text` helper and a
SHA-addressed playlist overlay with frozen dependencies/application unchanged.
Rerun pending; keyboard expectations and saved-name assertion stay intact.

### BUG-086 — Palette restart treats a stale X11 property as fatal (harness)

`themecontrols/attempt-20261111-063641-779` completes the dark sound positive,
disabled-output silence, Audio/Ambient and Home phases, then the next private
window discovery encounters wmctrl BadWindow/X_GetProperty on the just-closed
window. The new child is cleaned up and default output/service restored.
The bounded existing startup wait now recognizes only this exact transient
alongside the prior empty-client-list case; genuine display/other errors still
fail. Regression extended, rerun pending. No deadline increase or app change.

### BUG-087 — New enlarged-text fixture exceeds the supported maximum (harness)

The campaign's new custom audio fixture used typographyScale1.35, exceeding
HomeLayout's declared1.3 maximum. Isolde correctly falls back without rewriting
the invalid document; the helper's later custom-sound captures are consequently
Sources screens, not sound settings. This was introduced by this campaign,
not a product regression. `attempt-20261111-064020-874` verifies dark and pastel
preview/nonzero and disabled/all-zero output, then rejects the invalid custom
route. Those custom frames and their audio are excluded. Fixed to1.3; rerun
pending. Do not weaken the schema or the audio-positive assertion.

The subsequent `attempt-20261111-064556-986` accidentally repeats the same
invalid custom fixture because the combined patch had failed atomically.
It is retained as a duplicate failed attempt, not additional product evidence.
The one-line1.3 correction was applied and verified separately before
`attempt-20261111-065145-406`. No application change or full CTest repeat.

### BUG-088 — Games helper hard-codes an obsolete keyboard and ten runtimes

Source review before the Games rerun finds its private keyboard map differs
from current OnScreenKeyboard and its Down10 install-row selection is stale
for the sixteen declared routes. Prepared harness repair uses production
keyboard definitions and the bounded frozen runtime manifest for both setup
row position and activation identities. SHA-addressed library overlays retain
the tested application and dependencies. Actual rerun is pending; no expanded
runtime installation or gameplay pass is inferred from this source repair.

## Installed sound-output result

`themecontrols/attempt-20261111-065145-406` passes all six PCM assertions.
Dark/pastel/custom previews contain18086/68593/18086 nonzero samples;
corresponding disabled previews contain zero nonzero samples. Recordings are
528000/566400/499200 bytes for enabled and528000/595200/508800 for disabled,
48kHz stereo signed16-bit. These are actual Qt output captured through a
guest-only null-sink monitor, not simulated player callbacks. All21 frames
reviewed. The supported custom scale is1.3. Original output sink is restored,
module unloaded, service active/running/zero restarts and normal layout hashes
unchanged. Native Kodi XML settings are unchanged.

Legacy `custompolicy`/`customreset` frames both show the component color editor;
`customcancel` shows Edit component. They do not prove the old policy/reset/
final-exit labels. Exclude those three semantic claims; retain the verified
no-write layout assertion. Sound, Audio and Ambient frames show their named
surfaces with visible focus in all three palettes. No broad T12/T27 gate is
closed from this narrow audio proof.

## Additional attempts and coverage limits

Games `attempt-20261111-065656-643` reaches actual SuperTux0.6.3 Start Game.
The attended review deadline expires before input is consumed; runner cancels,
restores service and preserves both normal layouts. No permission denial,
gameplay failure or gameplay acceptance is inferred. A diagnosed attended
rerun follows; the late JSON instruction cannot affect the ended attempt.

### BUG-089 — History helper assumes Kodi Escape stops playback

Installed `history/attempt-20261111-065945-035` imports the real three-episode
fixture and starts actual episode playback, then fails `Actual episode stop`
after sending Escape directly to Kodi. Kodi logs show VideoOSD loading; the
helper's assumed Escape-to-ended contract is not established. Keep this as
an open input/adapter diagnosis, not a false product playback failure or a
history pass. The fixture must use the declared supported remote stop path
and verify lastplayed and exact history return independently. Two captured
frames require review. Cleanup restores the normal service/layouts. Later
history phases were not reached.

History follow-up source diagnosis: KodiService.cpp installs Escape/Backspace
stop notifications only in `FullscreenVideo`; the captured playback frame has
VideoOSD open. Thus a supported managed key exists, but the helper sends it in
an unverified OSD context. Retain the actual stop failure as a possible T09/T12
context-coverage defect as well as a helper defect; do not replace it with RPC
and claim remote acceptance. Both history frames are now agent-reviewed.

Games attended rerun `attempt-20261111-070142-144` passes its automation; all
nine frames reviewed. Real registration uses the current remote keyboard,
keeps the catalog empty until explicit approval, launches SuperTux, returns
through Home and reopens the same entry with Confirm. The frame labelled
`gamesplay` is the Story Mode menu, not playable execution. Keep
`gameplayVerified:false`; no T30.2 milestone closes. BUG-088's keyboard repair
is verified; its sixteen-runtime setup portion remains unverified.

Final harness check `round2-20261111-harness-final` passes103/103 self-tests
in1.858 seconds, including sound-receipt validation, helper isolation, real
release rollback and transient window discovery. Subsequent verified artifact
cleanup again cannot recover its3GiB target; Configure is correctly failed,
not reported successful. It preserves evidence and current/pinned payloads.
This is a capacity blocker for additional build/provisioning, not an app test
failure. No host cleanup, unsafe-path suppression or repeated app full suite.

## November11 continuation — user requests repairs and further progress

T28 reconciliation credits three native milestones from this campaign's
already-passing exact frozen suite: T28.1.a/.2.a/.3.a. The original five groups
keep equal weight; each has one native and one integrated child. Evidence is
`searchHeaderSwitchRemoteFocusAndStableResults` and
`searchPreferenceProfileIsolationAndWriteFailure`, with real QML input,
mouse/remote toggling, unchanged results, empty/late results, keyboard, default
OFF, profile persistence and failed-write rollback. Native dark/pastel frames
were reviewed. T28 becomes30%; sum1693/25 rounds to **68%** (121/154 milestones).
No credit is awarded for BUG-082's unverified repair, OFF cancellation or the
unfinished combined installed journey.

The combined installed `themes/attempt-20261111-070457-815` reaches90 captured
phases, then fails its900-second runner deadline at custom Import. All90 PNGs
are agent-reviewed in `review/themes-round2-00.jpg` through14. Dark and pastel
surfaces complete, custom does not reach every terminal phase. Preserve this
partial evidence; it is not a passing complete journey. Fixtures use production
Kodi client boundaries with controlled replies; no real library/entitlement
claim. The arbitrary palette deliberately differs between Home and origin;
Settings follows Home and destination/gallery media follows its route context.

The VM later reports state `aborted` (VirtualBox time2026-11-11T19:10:06Z).
The cause is unknown; do not blame Isolde without evidence. User-authorized
continuation restarts only Isolde-Test headlessly. First recovery connection
precedes completed boot and times out; subsequent supported Recover succeeds.
No host maintenance or personal-data cleanup. Status confirms no active runner.

BUG-082 repair: expose KodiMedia::invalidate through Q_INVOKABLE. Extend the
existing pending-request/retry regression to invoke the meta-object boundary
used by QML, so a direct C++ call cannot hide this defect again. Source repair
is prepared, not VM-verified. Full61/61 evidence predates this repair.

Focused `round2-20261111-search-fix-a` passes runner self-tests but automatic
safe cleanup cannot reach the2GiB application-test minimum; no compilation or
application test ran. `Status -Storage` reports1.79GiB, no active owner and no
additional linked test executables to reclaim. Current/pinned payloads,
compiled objects, source receipts and evidence are preserved. Do not lower the
space threshold, delete host data or describe this as a regression pass.

### BUG-090 — Unselected Sources rows disappear in a custom palette (T25/T27)

`themes-customsources.png` visibly renders unselected row backgrounds with
unreadable labels. The source delegate paints elevated backgrounds but uses
the ordinary surface text/muted roles. Dark/pastel happen to mask the mismatch.
Repair uses shared elevatedText/elevatedMuted roles and adds a regression that
measures actual unselected-row foreground/background contrast, retaining a
capture. Prepared source only; VM verification is blocked by guest capacity.

### Additional visual findings retained for diagnosis

- BUG-091 (T10/T12/T27): `themes-darkinputs.png` and pastel counterpart show
  HDMI input explanatory/status text running below the viewport and overlapping
  the bottom help. VPN title is clipped at the top in all three Settings
  palettes. Inspect scrolling/layout before claiming TV-distance acceptance.
- BUG-092 (fixture/T04): episode rows show `SundefinedEundefined` in every
  themed fixture. Diagnose missing fixture fields versus renderer fallback;
  supplied controlled metadata is not a real-Kodi failure proof.
- BUG-093 (T26/T27): selected CBC Gem gallery card under the green/custom
  origin has an ivory background with its label/status effectively invisible
  (`themes-pastelgallery.png`). Other cards are readable. Review selected AppTile
  contrast roles; no speculative repair or acceptance credit yet.
- BUG-094 (harness): the monolithic themes inventory exceeds its900-second
  bound with host capture acknowledgements, despite progress. Split/resume
  palette inventories or revise the explicit bounded budget with evidence;
  do not rerun the healthy application full suite for this harness issue.

Remaining software coverage is explicit: source action/reconnect and editor
installed closure; complete gallery launch/state/profile lifecycle; real-client
Search/OFF cancellation; Store privileged setup/transaction and Android app
journeys; complete Kodi import/playlist/picker/history and viewing controls;
virtual capture/HDMI and isolated VPN topology; actual Linguist model/UI;
expanded runtime provisioning/offline/rollback and meaningful game execution.
Native61/61 and prior accepted Studio/Guide evidence remain applicable only to
unchanged scopes. No paid account, physical device or target GPU requirement
is introduced as a software blocker. Capacity, confirmed bugs and missing
combined evidence remain honest unresolved work.

Agent visual ledger: `build/vm-oracle/round2-20261111-b/review/agent-visual-review.json`
records SHA256 and review disposition for406 original PNGs (native126 and
visual280). Raw generated capture receipts remain unmodified/pending; the
agent ledger and this report state semantic exclusions explicitly. All
review sheets are ignored workspace artifacts, not committed artwork.
