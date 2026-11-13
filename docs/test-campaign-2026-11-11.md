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

## Batched repair continuation

The user explicitly requests larger coherent repairs and fewer reruns. The
current closure batch combines the already committed Search invalidation and
Sources elevated text fixes with shared AppTile selected-text contrast,
PageAppCarousel/GuideWidget resolved palette fallbacks, truthful episode labels
when Kodi omits numbering, VPN first-row/header reveal, and managed Kodi Back
stop while VideoOSD is open. Extend existing gallery/library regressions rather
than creating a test per line. Capture/HDMI explanatory overflow remains open;
no unrelated control is silently dropped.

Storage diagnosis found641257472 bytes of synthetic TV media omitted by the
old oldest512-run inventory. The corrected bounded inventory shows newest
payloads and named fixture/cache roots. A new cleanup path hardlinks only
identical old synthetic clips within each verified native fixture directory;
all paths and bytes remain, newest three fixtures and active paths are retained,
links/mounts/unknown formats/oversize or changed bytes fail. The104 runner
self-tests pass, including path/byte preservation and idempotence. Actual guest
receipt records400222784 duplicate bytes consolidated. Configure still fails
its3GiB target honestly; the next Test retains its own2GiB minimum.

One full closure Test is justified for this coherent repair batch: it changes
a native QML-exposed client method, Kodi runtime keymap generation and shared
app/guide/shell palette rendering. The previous61/61 baseline does not cover
these new bytes. Run this once after the batch; subsequent bounded fixes use
focused regressions unless this closure run exposes an integration failure.
BUG-094's bound grows from900 to1740 seconds, matching the existing bounded
large-journey class, because the prior90-phase run was still progressing at
898 seconds. This changes no application/test assertions.

Storage continuation: repair-batch-a still stops before compilation, at
2132172800 bytes (about15MiB below the minimum) after its new source upload.
Fixture dedup records400222784 logical duplicate bytes; actual media tree usage
falls from641257472 to404606976 bytes, so logical totals are not claimed as
physical reclaimed space. A broad native-profile size inventory times out at
30 seconds; narrow named-cache inspection succeeds instead, with no deletion.

Verified immutable payload dedup retains all candidate paths and manifest
hashes, sharing only same-mode/owner/device artifacts. It refuses redirected/
mounted/modified candidates and interrupted transactions. Initial self-test
fails because its synthetic fixture lacks required bin/isolde; corrected
fixture passes105/105 in3.099 seconds. Real receipt consolidates425 identical
files (33800214 logical bytes), and verified source cleanup removes an obsolete
recoverable copy. Configure still reports failure against its3GiB target.
`round2-20261111-repair-batch-b` then passes its own preflight at2.02GiB and
starts the application closure test. A never reached app run is not a repeated
full suite; all stopped preflight and self-test failures remain evidence.

Games harness diagnosis: VirtualBox sends make/break together; an SDL game
polling held key state may miss that zero-time press. Games/emulators attended
input now holds each approved existing key100ms between its bounded make and
break halves, retaining the20-key limit and existing phase review deadlines.
No arbitrary keys or unreviewed consent are added. Actual menu/gameplay input
response must verify the hypothesis; no gameplay credit from source inspection.

Repair-batch-b reaches configure/build after passing preflight. Compilation
stops in the new subtitle regression: a braced QVariantMap argument contains
commas inside the Q_ARG macro. Move it to a named QVariant before invocation.
This is an introduced test compilation defect, not a product failure. No CTest
registration ran. Resume the unchanged production batch with incremental build;
the prior full-suite decision still applies, rather than starting a new feature.

Existing evidence reconciliation credits T25.4.a from the full round2-b native
SourceManager cases: repeated scans/roots, reconnect/dismiss, hide/restore,
handler addition/removal, repeat promotion with identical committed document
and deterministic failed-decision-write rollback. This is more than dedup alone;
sourceManager production bytes remain unchanged. T25 becomes40%; acceptance
sum1703/25 still rounds to68%. Installed remote actions remain separate and open.

Repair-batch-c builds and reaches CTest. Nine registrations pass, then
studio_cutover times out at60.26s, with only initTestCase logged. Studio performance
also grows from26.61s in round2-b to101.64s. Its actual frame latency assertions
still pass; the constructor/lifecycle portion is suspect. The new per-tile and
per-carousel SurfacePalette objects are the changed repeated-allocation boundary.
Remove those duplicate resolvers, using already-resolved selected-text roles
and the carousel's existing palette fallback; retain all contrast behavior.
The existing closure regression selection now also includes studio_cutover,
so one focused batch checks the failure plus performance, gallery and design
contracts before a justified full integration repeat. No timeout is weakened.

Focused-d stops at preflight before build/test: the completed full build has
recreated many linked test executables. Automatic cleanup reclaims776458240
bytes but leaves2008477696, still below2GiB. Extend the existing linked-output
cache cleanup to the single CMake-declared libisolde_core.a archive, requiring
its archive magic and the same native path/mount/active checks. All individual
compiled objects remain; Ninja regenerates the missing archive. Extend the
existing safety regression and named focused-closure selection check.

Focused-e repeats the cutover timeout after removing the duplicate resolvers;
that hypothesis is not confirmed. Diagnostic Status finds5.2GiB available RAM,
almost unused swap, and GNOME Shell consuming100% of a CPU core while idle.
Normal Isolde is not the busy process. Record this as guest desktop contention,
with causation still requiring the controlled run. The runner now pauses only
the verified same-user /usr/bin/gnome-shell process during the two offscreen
scale registrations (studio_performance/cutover), then resumes it before any
other registration and in finally on failure/cancel. PID/start-time/executable
identity prevents signals to a reused/foreign PID; Recover handles an unfinished
lease. All real external-app/session tests keep the live compositor. The new
mock-process safety regression proves failure restoration and reused-PID refusal.

Batch reporting gains focused Suite selection and an exact-hash capture index.
Pixel review is reused only from an explicit agent-reviewed ledger, preserving
prior exclusions and keeping semantic acceptance pending. Baseline self-check
reuses406/406 identical images with zero new pixels. Focused batches must be
NativeOnly; a focused receipt cannot silently become installed acceptance.

Focused-f confirms desktop isolation does not fix the slowdown: performance
103.27s and cutover60.22s timeout, both desktop leases resumed=true. Remove the
speculative pause/restore workaround and its mock regression; do not ship an
ineffective process-control change. The controlled negative result is retained.
The consolidated report automatically reuses19/19 reviewed identical captures,
with zero new pixel reviews. Add monotonic phase timings to the exact1024-node
cutover case (no assertion or timeout changes) and run only studio_cutover to
identify the blocking phase. No further broad test replay for this diagnosis.

Diagnostic-g locates the scale cost: reset616ms, create22175ms, layer confirm
25674ms; Find on page then exceeds the60s registration deadline. Existing
ComposedHome viewport filtering depends directly on subpixel contentY and
preloads three whole screens. Replace this with stable integer-row bounds,
two buffer rows on each side, and no card construction for hidden previews.
Data, declared layout, placement and focus state remain in their existing models.
Extend the same1024-node regression with rendered-row bounds and actual last-
component visibility after Find on page, retaining its unchanged-disk assertion.
This is a product rendering repair; no test timeout is raised.

Render-h reaches Find on page at25.2s, then times out at60.5s. Row buffering
alone is not a proven repair. Find on page only changes mode and relies on an
earlier deferred reveal; make the action explicitly reveal the current placement.
Add selected-visible/disk-unchanged phases to separate navigation from teardown.
Render-i never builds: concurrent queue edits correctly invalidate the source
snapshot. This packaging error is agent-induced, not an application failure.
Finish all edits before packaging the next candidate.

Accounting CheckTable confirms1703/25=68%,122/154 milestones. The report/index
self-check reuses406/406 reviewed identical captures. Baseline ReportOnly still
returns incomplete because its retained visual failure is not waived by61 native
passes. The changed-capture gallery is now linked from the consolidated report.

Render-j confirms Find on page and actual selected visibility pass by29.7s,
with saved bytes unchanged. Timeout follows the test body, exposing lifecycle
cost rather than failed navigation. Editor pointer targets still instantiate
all1024 delegates independently of card virtualization. Bound root scrolling
pointer targets to visible grid rows plus two-row buffers, retaining the selected
target for drag continuity. Closed editors construct no pointer targets. Extend
the1024 regression with an actual pointer Repeater count bound. No deadlines or
existing assertions are removed. Validate this focused before broad integration.

Render-k PASSES studio_cutover, zero failures/skips, including the1024-node
visibility, bounded card/pointer delegates and unchanged saved data checks.
The automatic report and capture index complete without separate capture work.
A repeat full integration run is now justified: repair-batch-c failed cutover,
and the shared editor renderer/pointer lifecycle repair plus Kodi keymap and
UI/media changes need cross-boundary revalidation together. Run one full batch
round2-20261111-integration-l; do not rerun full per individual repaired widget.

Integration-l passes61/61 with zero skips. All accumulated UI/media repairs and
bounded Studio pointer rendering pass together. Native evidence reuses122 exact
reviewed PNGs; five changed originals are reviewed (icon cleanup preview, three
Kodi details palettes, Sources custom contrast). Guide/carousel invalid QColor
warnings are absent from the affected paths. BUG-082 cancellation now passes
through the actual QML meta-object call; BUG-090 contrast regression passes.
Deploy succeeds as0838e68c4de00a86ee395580; normal service active/zero restarts.
Installed studio-search passes network denial and exact Settings recovery with
normal layout hashes unchanged. References/history are in the same serialized
visual batch. The new evidence reconciliation credits existing independent
native outcomes under unchanged Scope weights:1974/25 rounds79%,155/212 children.
The complete mapping is docs/acceptance-reconciliation-2026-11-11.md. Runtime,
common gameplay, Power and combined installed gaps remain explicit.

### BUG-095 — Unbounded scrolling editor pointer delegates

T24: the1024-node cutover registration times out after the navigation assertions,
with all pointer hit-target delegates instantiated despite virtualized cards.
Fixed by row-bounding pointer delegates and hidden previews, retaining the
selected drag target and explicit Find on page reveal. Focused render-k and
integration-l61/61 pass. No deadline was raised.

### BUG-096 — Guest desktop prompts invalidate installed capture evidence

The current Search/Lounge helper flags pass, but agent review of original PNGs
reveals a GNOME keyring unlock modal and low-disk desktop banner obscuring the
application. This is a harness/environment failure, not accepted product visuals.
Cancel the remaining Pastel batch; history is unattempted. Normal service and
layout hashes restore unchanged. Exclude these installed frames from acceptance;
the61/61 native pass and five reviewed native changes remain valid.

Repair the runner preflight: only inside the native Isolde-Test oracle, cancel
outstanding Secret Service Prompt.Dismiss requests (never unlock/read credentials),
and turn off guest desktop banners that cover owned-window captures. This changes
no host settings, Isolde permission/notification policy, or2GiB test threshold.
Use a bounded object inventory and verified runner self-tests. No application
full rerun is needed for this harness-only correction. Replay only affected
installed journeys against the exact existing passing payload after checking
that the desktop is clear. Desktop-probe run: round2-20261111-desktop-preflight-m.

Desktop preflight-o passes with107 runner checks after m identifies the old
visual-service test fixture's missing preflight stub and n demonstrates D-Bus
Dismiss rejects a different caller. The bounded fallback restarts only the guest
user gnome-keyring-daemon service to cancel pending operations; credential files
are preserved, no unlock/password is supplied, and prompts must be absent after.
Guest-only notification banners are disabled. No host setting changes occur.
Replay installed Search/history/references using the existing integration-l
payload, automatically preparing the guest desktop before each case.

Desktop preflight-s PASSES107 runner checks and actual targeted banner dismissal.
The GNOME accessible Ignore button exposes zero actions, so the runner verifies
its exact shared low-disk notification container, focuses it through accessibility,
confirms FOCUSED, emits a bounded named checkpoint, and sends Space make/hold/break
through the existing VM input path. It requires the button to become defunct.
No guessed coordinates, password/Enter input, host settings or credential reads.
The ineffective notification-tray shortcut workaround is removed. The preflight
runs automatically for later Visual calls; pending/undismissable warnings fail
before the expensive product journey. Replace affected Search/reference images
only; real episode stop/history/replay/exact-return assertions already passed
on integration-l twice, with no new application full run.

Final checkpoint: integration-l clean Search142420-318, Lounge142504-240 and
Pastel142807-740 pass; all51 frames reviewed, both generic recreation/reset pairs
compared, normal layout hashes unchanged and service restored active/zero restarts.
T28.1 genuine switch and T25.5 profile/persistence isolation close with their
scoped native/runtime evidence. Acceptance1994/25=79.76% rounds80%,157/212 child
milestones. No task weight changed, no missing Power/gameplay/source-default
requirement was waived, and no native full rerun was added for harness recovery.
No runner remains active. Next: remaining T25 source presentation/action/default
slot acceptance, then gallery and combined Web-switch installed gates.

Final review ledger/index:185 reviewed scoped captures,58 explicitly excluded
obstructed/cancelled captures, zero pending pixel reviews. Excluded attempts were
not individually certified or turned into acceptance. The index now preserves
explicit exclusions instead of placing rejected attempts back in the review queue.

### BUG-097 — Legacy source banner bypasses declared containment (T25)

The installed populated-source walkthrough exposed a legacy Home Text banner
outside the declared Sources header. It reflows content, survives policy notice
suppression and advertises desktop-only K/Delete actions. Removed that obsolete
banner; actual Home navigation regression now asserts unchanged content position
when discovery changes. The compact reserved header remains data-driven.

New bounded `sources` journey batches packaged-header entry, policy cancel,
persisted hide/restore, Keep cancellation, handler cancellation, unplug/retry,
all policy fields with one atomic Save, and both source families in dark/pastel.
Its first installed attempt175158-265 proves default/header and hide/restore, but
four component frames are EXCLUDED: harness used an invalid shell source binding
and the application correctly preserved it while falling back to defaults.
Corrected to the registered sources capability binding; no component pass claimed.
Harness maintenance-a passes108 runner contracts. Product closure requires one
full task-level run because HomeScreen changes must be in the next staged and
installed artifact; integration-l still covers unchanged production boundaries.
No additional full run is authorized merely for helper/document changes.

Closure-a stops before build: automatic cleanup recovered only1.81GiB. The bounded
inventory identifies obsolete report transport bundles as duplicate storage.
Extend preflight to remove only terminal, non-current/non-pinned guest report
archives whose exact SHA256 matches a verified workspace backup. Retain newest
three bundles, raw guest evidence, source/payload bytes and all host files.
Regression rejects checksum/mount failures and preserves active/protected/recent
runs and raw evidence. This repair addresses the diagnosed capacity failure;
closure-b resumes the same application batch, with no unchanged test retry.

### BUG-098 — Compact source details ignore the configured visibility (T25)

Source review while closure-b runs finds the compact card always populates its
first-source subtitle even with sourceDetails=false. Respect the declared flag;
actual QML regression checks visible text while toggling details and count badge,
including returning to the original setting with unchanged layout bytes.
A strict Sources-only QML reuse boundary rebuilds/stages the correction and runs
isolde_tests, component_contracts and sources_remote_ui; only PageSourcesWidget
and named regression/harness paths may change from the original full candidate.
Contract regressions reject changed core/config and tampered base executables.
Closure-b remains the original full proof; no second full run for this local fix.

Closure-b reaches34 passing registrations then stops at the new banner regression.
The fixture changes brand visibility then samples child y after only10ms; its
first sample is the old58px layout, second the correct190px branded layout.
This is a new test synchronization error, not a source-driven reflow. Invoke the
parent Column's forceLayout before each measurement, so the comparison isolates
discovery without a timing guess. Sources focused selection now includes actual
Home navigation (four registrations). Verify both local repairs there first,
then repeat the failed integration for the final payload; b remains failed.

Sources-focused-c stopped in runner self-tests before application build: the
suite-registration expectation still listed three targets after adding Home
navigation. Update that explicit expectation to four, including the exact new
registration; preserve the failure. BUG-098's visible-text regression explicitly
uses the supported compact-summary presentation (the default icon-card variant
does not paint the subtitle). No claim that default icon cards exposed the name.

Final verification budget: after the focused repairs pass, use original passing
integration-l61/61 as the full-origin proof and rerun the four Sources/Home
registrations with strict payload/source-manifest reuse. The entire product
delta is HomeScreen's removed legacy-only banner and PageSourcesWidget's compact
subtitle flag; no C++, schema, registry, persistence or provider bytes changed.
The explicit allowlist covers those two QML files and named tests/helpers only;
changed core/config and base-payload tampering are rejected. This resolves the
failed new fixture and rebuilds/stages the actual fixes without replaying the
unchanged slow Studio registrations. Failed closure-b remains failed and no new
full61/61 pass is claimed from four focused registrations.

Sources-focused-d passes4/4 and110 runner checks. Staged-e stops before build:
verified cleanup leaves2143973376 bytes,3.35MiB below the unchanged2GiB floor.
Selected test executables were retained only as a linking-speed optimization.
When all safer cleanup remains insufficient, permit the same checked linked-test
cleanup to remove those regenerable selected executables too, preserving compiled
objects, active processes, payloads and evidence. Existing safety contracts cover
this cleanup function. Staged-f batches Test with strict61-test-origin reuse,
Deploy, and installed Sources/collections/Search using the same corrected payload;
any runner failure stops the batch. No hidden full-suite rerun or threshold change.

Staged-f refuses reuse before building: the manifest also includes the already
committed review-index exclusion repair from the previous checkpoint. It changes
only tools/index-vm-review.ps1, not the installed application. The complete local
manifest comparison confirms exactly the two declared QML changes plus named
regressions/helpers/docs, with no other product/config delta. Include this exact
review-only helper in the harness allowlist; keep core/config refusals intact.
Batch-g repeats only the focused selection and intended installed workflows.

Staged-g passes the product-path allowlist, then correctly rejects a new evidence
file absent from the original candidate: docs/acceptance-reconciliation-2026-11-11.md.
Permit only that named added document; the same guard still rejects application
file additions/removals. Added a contract for this exact documentation case.
This checkpoint's full local manifest inventory now reconciles both changed and
added paths against integration-l before batch-h. No application check ran in
f/g and neither failed attempt is represented as test completion.

### BUG-099 — Outer Home Menu routing ignores focused source components (T25)

Installed h contains valid bounded dark/pastel sources, but both sourcesactions
frames show Search. Main routes Menu to Home only for an already active widget
or group, so the first Menu bypasses the focused source. Exclude these two frames
from source-action acceptance. Home now exposes a contextual-menu eligibility
property including focused source families outside editing/header recovery;
Main uses that property. New Home-level actual-input regression verifies first
Menu dispatch, Settings/Sources destination, source identity, no write and the
editing/header guards. Explicit Search remains separate.

### BUG-100 — Source handler/Keep instructions falsely promise deferred policy Save

Installed handler frame says changes remain private until Save, but handler
Confirm immediately persists the choice. SourceSettings now explains immediate
handler Save, permanent-tile confirmation and Back cancellation separately from
policy drafts. Actual QML instruction tests cover both screens. Final Sources
journey also persists a handler and explicitly confirms exactly one durable tile.
The receipt now says noUnintendedLayoutWrites, not layoutBytesUnchanged: policy
Save, explicit Keep and fixture preparation intentionally write the private file.
The older h flag is not evidence of total byte preservation; only its named
cancel/open/hide/restore/no-write assertions are retained.

Final bounded Sources reuse adds only Main's one contextual routing condition
and SourceSettings' instructions to the earlier two QML paths. Core/schema/
provider/profile/storage bytes remain identical to integration-l. Four focused
registrations plus the actual installed Menu/positive-action journey verify the
repair; no new full-suite run is needed for this local source boundary.

### BUG-101 — Collections helper assumes the obsolete gallery's app order (harness)

Installed h collections fails waiting for the native client. Reviewed three
frames: dedicated All Apps correctly has FreeTube first, YouTube second; the
helper presses Right and actually opens YouTube. No native-launch pass claimed.
Select FreeTube through the real local Filter control/OSK instead, verify filter
return/clear and unchanged layout, and keep the expected native input receipt.
After uninstall, the dedicated gallery removes the app and selects a survivor;
never Confirm that different app while asserting the removed app was refused.
The explicit button-carousel remains ordered and retains its existing steps.
Search was not attempted after this failure. Normal service/layouts restored.

Staged-i stops at the new Home-level source test: the fixture supplies components
but omits HomeLayout's separate placements/grid projection, so no widget can be
created. Supply the same three inputs as production Main and the existing
page-menu Home regression. This is a fixture setup failure; action guidance and
compact-details tests pass. No deployment or installed i pass occurred. Stage-j
verifies the complete corrected fixture before repeating the Sources journey.

T25 ACCEPTED: sources-staged-j passes4/4 (original integration-l61/61 reused by
strict changed-file/payload proof), installs03ff1010746b189b470ed74f, and passes
18 installed Sources checkpoints. All18 frames are agent-reviewed, including
both palette action screens, both bounded source families, policy draft/save,
explicit handler/Keep confirmation and unplug/retry. The custom native contrast
frame is reviewed and27 unchanged native pixel reviews reuse exact hashes.
BUG-097–100 are repaired and verified. No user media/grants/Kodi writes or real
playback claims arise from tile promotion. Normal profile layout hashes remain
unchanged and the service restores active/running/zero restarts after Sources.
Both reference pairs reuse unchanged composed code paths as documented above.
T25 becomes100%; total2034/25=81.36%, rounded81%,161/212 milestones. Gallery
continues in the same runner; its acceptance is not inferred from Sources.

Gallery installed acceptance: sources-staged-j collections attempt185044-623
passes24 reviewed checkpoints,8 real isolated native X11 input/normal-exit cycles,
local filter/clear, stable return, actual Store removal/reinstall and app-opening
history replay. The dedicated gallery contains3 registered apps, then2 after
removal, then3 after reinstall; package offers/internal routes are excluded.
No gameplay or real FreeTube/provider claim: the lifecycle client is an explicit
production-boundary fixture. BUG-101's positional helper repair is verified.
Three reviewed installed Search frames also pass Settings query, genuine OFF
switch, denied IPv4/IPv6 sockets and exact origin return. No real Kodi result or
ON/profile lifecycle proof is inferred from those frames.

Credit T26.1.b/.2.b/.4.b/.5.b with the unchanged full-origin native/legacy/state/
profile proofs and this installed renderer/lifecycle matrix. T26 is90%; its
remote gallery presentation edit/Preview/Cancel/Apply milestone remains open.
Total2074/25=82.96%, rounded83%,165/212 milestones. Source/reference applicability
and normal-layout/service preservation are unchanged. No runner remains active.

### BUG-102 — All Apps Done/Reset strand Studio focus on an absent row (T26)

Source review finds both Done and Reset return to editorSettings choice2, but
that dialog now has only choices0/1. Return to the owning pageContext choice8,
matching Back. New actual QtQuick Studio input covers all eight gallery controls,
private Preview, discard/no-write, atomic Apply/reopen, Reset and Done origins.
A bounded native evidence writer exports the actual applied presentation; its
SHA256 is bound to the passing candidate. The installed collections journey
loads that exact saved presentation, then repeats the live registry/filter/native
input/return matrix. Modified/missing/unpassed evidence is refused.

The existing Sources focused suite now also runs studio_quick (five registrations)
for this local Studio handler change. Strict original full61/61 reuse allows only
the earlier four source QML files plus PageStudioView and named regression/helper
paths; production C++/schema/provider bytes cannot change. No new full run.

Search matrix version2 adds sixteen installed checkpoints: All Apps -> Search ->
local controlled UNIX Kodi metadata, genuine Web On/Off without arrow toggling,
cold saved state, real in-app profile switch/new owned windows/default-OFF
isolation/restored default state, read-only private preference save failure and
retry, then exact Gallery -> Home origin recovery. IPv4/IPv6 remain denied for
the actual child, UNIX works, both layouts and native Kodi XML are unchanged.
The local metadata server is an explicit production-client boundary fixture;
no real Kodi-server or online-provider/account acceptance is fabricated.

Prepared surfaces-k batches the five focused source/Home/Studio registrations,
exact Deploy, installed collections using the checksum-bound actual Studio output,
and the expanded Search/profile matrix. Source18/reference evidence reuses j's
unchanged source behaviors; no duplicate source tour or full CTest is scheduled.
The native helper parse check, evidence identity/negative contracts and complete
Search-matrix receipt tests run before any installed journey. X11 identity is
PID plus window ID: a cold process may legitimately reuse the old X11 number.

Surfaces-k passes113 runner checks but stops before build at storage: cleanup
reclaims523702272 bytes and leaves2121641984, still below2GiB. Installed per-user
releases duplicate already verified staged payloads. Extend existing dedup to
use a release only when its deployment receipt, canonical24-hex native path,
whole payload hash, mode/uid/gid/device all match the passing staged candidate.
Replace staged duplicate paths only; never replace live installed files. Paths,
bytes, current/pinned candidates and evidence remain. Regression refuses mounted
or modified installed data and verifies the installed inode itself is unchanged.
This repairs the diagnosed space cause; surfaces-l resumes the prepared batch.

### BUG-103 — All Apps list ignores artwork presentation (T26)
Installed surfaces-l loads the exact Studio-applied list configuration, but every
row discards its logo and forces showLogo=false. Fix passes the registered app
artwork and declarative visibility into HomeCard, whose generic summary launcher
art now shares the icon position. Native regression checks visible source,
non-overlap, artwork OFF and labels OFF. Acceptance remains pending.

### BUG-104 — Installed Search preferences always roll back (T28)
Surfaces-l expanded installed Search stops at Web On. The reviewed diagnostic
frame shows the production save-error message; the preference returns to false.
SearchSettings uses default QSettings with no application organization name,
which reports AccessError despite a writable fallback preference file. Existing
native tests supplied an explicit path and missed this production constructor.
Use explicit INI access to the same resolved legacy file, preserving profile
keys and read-only load behavior. Add an isolated default-constructor/cold-read
regression with an empty organization. No migration or normal-profile writes.

The next candidate surfaces-m runs one full suite because the batch now changes
shared HomeCard rendering and production preference storage access beyond the
bounded Sources reuse coverage. This is the task-closure integration run; no
additional full run is scheduled for helper-only fixes. It then deploys once,
runs installed collections/Search and both generic reference pairs, stopping at
the first failure. All captures still require review; current acceptance83%.

### BUG-105 — VM Studio frame deadline failure, reproduction pending
Surfaces-m full attempt stops at studio_performance after six registrations pass.
1080p sample7 takes659.953ms input-to-visible, violating the unchanged250ms bound.
The corresponding production command takes5.086ms; adjacent samples take7.579
and13.863ms; p95 is33.486ms and maximum command32.434ms. 720p max44.517ms.
This isolates a single render/event-loop stall, not slow gesture validation.
No passing full receipt or deployment was produced. Keep the failed evidence;
reproduce on the next integration run without relaxing timing requirements.
Search boundary fixture also now honors title/contains filtering, with negative,
case-folding and immutable-response contracts. This corrects the test server;
it is not an application search defect. No unfiltered fixture result is accepted
as a title match. The next full attempt is justified by this named full-suite
failure and includes all pending native Search/artwork checks.

### BUG-106 — All Apps list ignores density (T26)
Source review of BUG-103 finds that the same list branch fixes row height at82,
ignoring Compact/Spacious even though Studio persists those choices. Apply the
existing validated density multiplier to both grid and list, and assert real
list cell geometry for all three densities. Prepared while the frozen n suite
runs; n does NOT include this last local fix. After n, use five focused Sources/
Home/Studio checks with exact n full-origin reuse, then installed collections.
The reuse allowlist gains AppGallery.qml only; no C++/shared HomeCard allowance.
No third full run is needed for this isolated gallery geometry correction.

## User VM feedback priority — Home navigation and Store discovery
User November11 reports lost/invisible Home Settings focus, rejects the automatic
Edit page button on other pages, and requests discoverable online app browsing/
search rather than a preloaded-only catalog. This explicitly prioritizes the
related Home/PageSurface and T29 Store changes within the active batch.
BUG-107: entering recovery header after Sources was added selects Sources first;
Up must select Settings when present. Library refresh repairFocus also clears an
explicit header selection. Fix both and assert actual Settings signal/focus ring.
BUG-108: named-page runtime injects Edit page. Hide that control on named browsing
surfaces without rewriting stored pages; Home's existing Edit Home and Settings
Studio entry remain available. Tests cover effective controls and visible focus.
BUG-109: online Store provider is hidden behind F-Droid opt-in; Search silently
filters local entries while OFF. Add explicit Explore apps online and submitted
Android Search as one-request opt-in actions, preserve cancellation/no requests,
keep Device apps/Web filtering local, retain explicit Menu disconnect. This is
real signed F-Droid catalog discovery, not a claim of Google Play/Chrome support;
user's must-have sources/apps question is pending and independent work continues.
Store D-pad follows visual header -> tools -> grid order. Native catalog tests
check one request with the submitted query and reject overlong input before opt-in.
QML regression covers Explore, accepted/cancelled search and local platform filters.

Surfaces-n passes34 registrations including unchanged Studio performance and the
new production default-path Search save/cold regression, but navigation stops at
an empty background ArtworkImage still marked visible after BUG-103. Hide that
empty launcher background explicitly; keep the actual launcher icon test. Failed
m/n receipts are retained and neither candidate is deployed. The next focused
app_icons family exercises actual Store QML, catalog, artwork and navigation;
then one full integration run is justified by the named full failure plus user-
requested shared navigation/AppCatalog changes. Preserve the six unrelated tests.

Feedback-focus-p stops at old Store test mock missing browseOnlineStore. Update
that owned test double and its header -> tools -> grid sequence. Focus-q passes
Store discovery/old lifecycle UI, artwork, component contracts and the new Home
Settings/refresh/hidden-edit regression. Navigation then finds two test mistakes:
the artwork assertion picked a hidden source label instead of visible summary;
Pastel still expects Up -> Search. Select the visible label and assert new
Up -> Settings, Left -> Search, Right -> Settings behavior. No limit is loosened.
Viewport callbacks also move from free queued calls to a Home-owned zero-delay
Timer, cancelling pending work on teardown (BUG-081 viewport branch). Explicit
boolean conversion removes the undefined composed-menu warning. Remaining
scroll.reveal/isolated-context warnings remain open unless new evidence closes.

The installed store-discovery case uses the normal production Store/provider,
fresh profile and actual remote input. It verifies the live signed entry/index,
browses and searches NewPipe, captures the actual results, checks no installation
was committed, and returns through Home's visible Settings focus into Settings.
This is online listing/search evidence only, not APK install or Google Play.
Prepared final feedback-r runs full61 once, deploys, then sources/collections/
Search/Store discovery and both reference pairs under stop-on-first-failure.

Agent review of focused-q: studio-test-home-settings-focus.png shows the actual
Settings ring, Sources to its left and no Edit page control; accepted for that
narrow native regression. studio-test-store-discovery.png is excluded: the
Studio fixture was above the newly created Store, although its logic assertions
passed. Hide the fixture editor and raise the Store before capture in that test.
This screenshot-only correction is after r's frozen snapshot; no application
bytes change and no full rerun is warranted. Installed live Store captures, not
the obscured mock image, will supply final visual evidence.

Feedback-r full61/61 passes, sourceDigest a2017aaa8f8f60605b52eb961bd393c69fb26ec14dfca8e79a950f7e5538ff2a,
and deploys release9e33a01181c41ea8d571f932. Native Settings/visible list artwork/
controls/contrast/import/Store Explore images are reviewed; obscured mock Store
image excluded. Installed sources passes18 actions with unchanged normal layout
hashes. Its last six dark/pastel captures contain a guest low-disk dialog that
appeared mid-case (BUG-096 recurrence); exclude those for final clean UI evidence.
Reuse accepted j's clean source component/action views: PageSourcesWidget and
SourceSettingsView bytes are unchanged, source actions are not launch-app summary
artwork, and the new global header default is independently exercised by actual
r inputs/native Settings-ring proof. Do not infer a fresh clean18-image review.
The next-case bounded desktop preparation dismisses the identified guest warning;
r collections-selectedgallery.png is reviewed clean with real app logos,
spacious rows, visible label/state and restored selected FreeTube. No host files,
storage thresholds or unsafe-path/checksum safeguards changed.

### BUG-111 — Collections helper uses obsolete Store focus order
r installed collections completes four actual native input/normal-exit launches,
then its Store shortcut sends Up/Left/Confirm assuming grid -> header. New visual
navigation is grid -> tools -> header, so it chooses Explore and then SmartTube
Install confirmation instead of Device apps/FreeTube. Reviewed failure frame
shows Cancel selected; no Android install was confirmed. Update this helper to
Up twice before choosing Device apps. Product source/full61 and installed release
remain unchanged. Reuse r's passing candidate/helper overlay, with no rebuild,
redeploy or full rerun; resume collections/Search/live discovery/references.

### BUG-112 — Live Store cold index times out without compression
The new installed r Store check reaches real Explore, but fails to fetch its
signed index within the existing120-second download budget. Reviewed failure UI
reports the time/size limit. Official entry metadata on November11 declares
72930200 bytes,4987 packages, below the128MiB size bound: this is the transfer
time path, not a license/account/entitlement gate. Native Store opening/navigation
and the no-install-before-confirmation boundary work; no live listing pass yet.

Add Accept-Encoding:gzip and bounded streaming decompression with separate wire
and expanded size enforcement, CRC/truncation/unsupported-encoding rejection,
and the unchanged120-second deadline and pinned signer/index hash validation.
New transport tests verify identical signed index bytes after decompression and
refuse expansion bombs, raw overflow, truncated gzip, unknown coding and timeout.
No unsigned search/display API replaces trusted installation metadata.

Store-s uses six focused Store/catalog/QML/Android/Web/F-Droid/lifecycle checks
with exact original r61/61/payload reuse. Its sole production delta is the one
fdroid-store.py transport helper; C++/QML/catalog/unknown changes and modified
base bytes are explicitly refused by regression. This is no new full-suite pass.
The post-r Store test screenshot-layering fix is also exercised. Deploy once
then rerun the actual live Store journey; unchanged gallery/Search/reference
runtime evidence remains attributable to r's matching C++/QML/config payload.

### BUG-113 — Store hides valid provider matches and draws blank fallback icons
Review of store-s live NewPipe search shows provider5 matches but only4 cards:
the view applies a second narrower filter that omits package identity and uses
lowercase instead of the provider's Unicode case-folding. Honor IDs in the
current active provider result generation while retaining local filtering for
local entries and clearing authority on OFF. Regression covers a Unicode-only
provider match, an independent local match and OFF behavior. Missing online
symbol/accent also draws a white blank square; derive the app initial with the
shared selected-surface/text fallback. No unsigned artwork download is added.

### BUG-114 — New Store discovery receipt omitted from host report
The guest validates its passing signed live result, but write_report's explicit
case archive allowlist omitted store-discovery. Add only that named case and
extend the report isolation regression (unknown archives/cookies/source remain
excluded). The next bounded Store run exports the complete receipt. Native r
reference frames14/34 and collections24/Search16 are now agent-reviewed; source
six guest-dialog frames and the old Store mock/obsolete helper failure retain
explicit exclusions. T26/T28 close; calculator confirms2124/25=85%,170/212.

Store-t retains exact r full-origin reuse for only fdroid-store.py plus
AppStoreView.qml and named harness/tests. Six Store checks cover both; no global
C++/configuration changes or new full run. Actual installed live discovery is
repeated for this meaningful query/presentation correction, not for accounting.

Store-t self-tests pass but preflight stops before build: verified cleanup
reclaims436326400 bytes, leaving2144784384,2699264 bytes below the2GiB floor.
One supported Status/Storage inventory confirms terminal duplicate report
transports remain. Their latest-three retention is a speed optimization; only
the outer report.tar.gz copies may be reclaimed when the floor still fails,
after exact host-backup verification. Raw per-case archives/logs/screenshots,
source, pinned payloads, compiled objects, running/soak receipts and the current
run remain. Extend the existing report-cleanup fallback; checksum/mount/path
failures still stop it. Regression proves recent transport cleanup preserves a
pinned executable, raw evidence and the running report. No host deletion and no
lowered threshold. Store-u resumes the same bounded six-check/Deploy/live batch.

Store-u self-tests pass, but archived recent failed source copies leave free
space2124455936 bytes after10.8MB of verified transport cleanup. Store-v adds a
narrow final fallback: only ended failed/cancelled runs with no tested payload,
no test-result/Guide receipt, no active process and no current/pinned identity
may lose their extracted source copy. The existing primitive verifies the
recovery archive, every manifest/member and source byte, links and mounts before
removal. Recovery archives, raw evidence and compiled objects remain. Regression
covers current/pinned/active/running/payload/tested/Guide preservation, successful
re-extraction and tampered-archive refusal. No test-space floor is lowered.
Store-v stops at the new harness self-test: missing shutil name in its fixture copy. Corrected to the imported oracle.shutil; no application build/test was attempted. Store-w resumes the same bounded batch.
Store-w self-tests pass116, safely removes failed u/v source copies and retains their archives; free2133618688 still misses2GiB. Diagnosis: pruning only fullSuite payloads permanently retains obsolete passing focused payloads. Accept focused payloads only after revalidating their original complete coverage proof and exact payload hashes; existing newest-three/base/Guide/current pins stay. Regression rejects changed proof and then prunes only the obsolete focused payload while retaining pinned/newest payloads and raw evidence. Store-x repeats focused Store only.
Store-x passes116 runner/six Store tests, deploys d5aa5b72a5a662642f1b696d and passes91s live journey; named raw receipt now exported. Visual review catches BUG-113 still showing4/5: merged provider IDs have the production provider: prefix, absent from the mock. Correct the lookup and regression IDs to actual AppCatalog merged format. No percentage credit for the incomplete correction; Store-y repeats bounded Store/installed journey only.
Store-y self-tests pass116; cleanup reclaims487305216 but free2091540480 misses2GiB. Generated build/isolde was omitted from linked-binary cleanup even though its staged and installed copies remain. Extend the existing exact declared-target ELF/mount/link/active-process checks to only isolde, isolde-recovery and isolde-native-host in work/build; retain objects, staged and installed copies. Regression proves active executable preservation and independent installed bytes. Store-z verifies unchanged product correction.
Store-z passes116 runner/six Store tests but staging fails because focused builds only named isolde and assumed recovery already existed. Cleanup exposed that dependency. Explicitly rebuild all three already-declared shipped executable targets for every reuse payload (incremental relink only); no new application registration or full run. Store-za retries this diagnosed packaging failure.
Store-za again stops at2GiB before build after retaining z partial-stage bytes. No unknown payload deletion is allowed. Add a final bounded cache fallback for declared unselected Isolde test-target ELF .o files only, largest first. Preserve main/core/production objects, selected test objects, active files, generated sources/metadata, staged/installed data and all evidence. Reject redirected/mounted/unknown trees before mutation. Regression covers each preservation boundary and unknown bytes. This permits later tests to recompile affected test objects instead of blocking; no app full rerun. Store-zb completes the same correction.

### BUG-115 — Guest artifact retention prevents focused Store verification
Store-t/u/w/y/za stopped at the unchanged2GiB minimum. Exact terminal transport
backups and archived failed source copies were insufficient because obsolete
focused payloads were never eligible for existing passed-payload pruning and
generated application/test artifacts remained. The diagnosed fixes above retain
current/newest/pinned/active candidates and reject checksum/mount/path failures.
Final fallback may regenerate only unrelated declared test-object cache; core,
application and selected-test objects remain. No acceptance pass is awarded to a
space-blocked attempt. Store-x demonstrates safe obsolete focused pruning;
store-z demonstrates declared inactive build-binary cleanup. Final zb tests the
unselected-object fallback and packaging correction together.

### BUG-116 — Focused staging assumes cached recovery executable exists
Store-z passes all six selected registrations but cmake install cannot find the
recovery executable after verified generated-cache cleanup. Focused reuse builds
now explicitly include all three shipped executables, instead of assuming a
previous build left them behind. This is an incremental packaging dependency
correction; original r full-suite/payload proof remains required.
Store-zb passes117 runner tests; unrelated test objects recover93MB but leave2122838016 bytes, still24.6MB below floor. Add source dedup before object fallback: terminal inactive snapshot files only, exact whole-source manifests, matching checksum/size/mode/uid/gid/device, safe atomic hardlinks. Every source path/archive/manifest stays and candidate bytes do not change; current and active snapshots are skipped, mounts/links/modified source fail. Linux regression verifies preserved/revalidated paths and shared identical inode, refuses modified/mounted source. Store-zc resumes the same tested product correction.

### Installed feedback closure — Store-zc, 2026-11-12
All118 runner checks and six focused Store registrations pass, zero skips,
with strict full feedback-r61/61/source/payload reuse. Only AppStoreView.qml and
fdroid-store.py are production deltas from r; unknown changes remain refused.
Source digest9b58ffcc1a3480db09c1dadd98daaf0d0ac75bc0b3e47b2ce5490ccb3c5f7d16;
installed release ceb971fc65a09dd9a9973336. Live signed discovery passes91s and
81 actual input actions: fresh local-only entry, explicit Explore,4891 eligible
browse matches, OSK NewPipe search showing all five returned packages including
PipePipe, readable initial fallback, Home return/visible Settings/activation.
The exported named case archive retains raw result.json and shell.log. Both
index/entry hashes and package IDs are recorded there; no app is installed by
browsing. Android runtime availability/setup remains an actual open limitation,
not a successful APK install. Both normal layout hashes remain unchanged;
service restored active/running/zero restarts.

Agent reviewed all eight live frames (store-zc-discovery-00/01 contact sheets
and full search-result frame), reuses15 identical native pixels from s, and
records23 reviewed/zero pending in the explicit ledger. BUG-113 result count and
BUG-114 export are fixed. BUG-115/116 retention and recovery-target packaging fixes
pass118 runner checks; source dedup retains every original path and byte.
T29.1 provider/local/default-OFF/result-model audit now closes using production
native contracts plus installed fresh opt-in/live browse/search. Remaining actual
artifact download/install/update/remove and runtime matrices retain their own
open milestones. T29=63%; sum2131/25 rounds85%,171/212 milestones. No new full or
reference run is required solely for these bookkeeping updates.

## T29 continuation — idle runtime compatibility and independent Web lifecycle

BUG-117: installed Android is configured, but Status proves every mounted
rootfs build.prop absent while the idle container service is healthy. Existing
runtime_target therefore disables all online app installation. Read actual
metadata from initialized root-owned images during explicit setup/package
configuration, in private read-only ext4 loop mounts; preserve live runtime and
profiles. Cache SDK/ABI/features with exact image dev/inode/size/mtime identity,
root ownership and non-writable permissions. Idle Store reads that bounded cache;
image replacement/malformed/untrusted metadata refuses compatibility. No host
ABI guessing, extra dependency or bypassed APK trust.

BUG-118: the earlier terminal runtime setup never installed the newer protected
Store transaction support. Factor its existing normal .deb builder for reuse;
package the metadata reader and refresh initialized-image metadata in postinst.
The VM Configure android-store-support target uses only existing authorized
sudo -n dpkg, the tested deployed payload and its fixed package. No user-writable
Python elevates and no authorization policy is included in this package.

Automatic approval review rejected a proposed temporary profile-restricted
polkit test rule: it would grant privileged authorization without explicit
approval. The patch was rejected and no such rule was written or installed.
Do not work around that rejection. Protected Android positive transaction
acceptance remains pending approved authorization; independent Web Store
lifecycle runs with its own store-web receipt and makes no Android claim.

Verification batch store-runtime-a: six focused Store registrations, strict
original feedback-r61/61/payload reuse extended only to android-setup.py and
provision-android.py plus their tests, deploy, fixed support package configuration,
installed Web lifecycle and signed discovery with actual idle compatibility.
No full/reference reruns for the bounded helper correction. Percentages remain
85%/T29 63% until the corresponding independent evidence is reviewed.
Store-runtime-a passes118 runner checks but stops at preflight:602849280 bytes reclaimed,2118103040 free,29MB below2GiB. Extend only duplicate source-archive transport cleanup when a matching exact host backup and a complete manifest-verified extracted source tree both remain. Source paths/payloads/logs/pins/current/Guide evidence are preserved; changed source or archive checksum refuses cleanup. Regression proves default retention, modified-source refusal and preserved complete source/staged bytes. No floor reduction or installed-release deletion.
Milestone reconciliation: T29.2/.3/.5 native children retain exact0.5 weights; their original remaining0.5 splits into two0.25 outcomes. All three new outcomes start open; calculator CheckTable preserves2131/25=85%,171/215 before credit. T29.3.b now credits the already reviewed store-zc live signed-provider UI, unchanged by this helper-only batch: loading, all5 results/provenance/initials/focus and exact return. No package installation is inferred. T29=66%, sum2134/25 still85%,172/215.
Store-runtime-b passes119 runner checks and5/6 Store registrations, including18 F-Droid tests/new cache cases; first failure is store_lifecycle test looking for package postinst under the source fixture after install_support had cleaned its separate temporary build directory. Capture/assert the actual postinst at the mocked dpkg-deb boundary before cleanup. No production packaging failure or system mutation occurred. Accounting-only script is a named non-production reuse delta; no app full run.
BUG-119: runtime-c passes119 runner/six Store tests and deploys47f8c6f0fe843a0dbac1b208. Actual support package installation reaches root postinst and correctly refuses legacy /var/lib/isolde mode0775, uid0; supported Status confirms exact path/owner/mode and idle rootfs absence. Explicit setup now seals only this verified root-owned application directory to0755. No parent/personal path changes. Extend actual-image parsing to Android namespaced SDK/ABI properties and root sysconfig, with regression. The half-configured support package will be repaired by the next exact fixed-package install; no success credit yet.
BUG-120: runtime-d self-tests pass119 but integrity preflight refuses installed payload mismatch. Diagnosis: Configure imported android-setup from the installed release, creating an extra __pycache__ file. Prevent with python -B; extend existing strict generated-cache recovery to passing focused/installed copies only after every tested file still matches and every extra is a named bounded CPython cache for a verified source. Unknown extras/changed code still fail; restoration must equal the exact tested manifest. Regression covers focused installed cache recovery alongside existing unknown/code-corruption refusal.
Storage inventory also identifies2.57GB managed .cache/isolde-emulators cache. Add last-resort removal of only PPSSPPs generated build directory, after exact pinned sourceCommit/offline .deb checksum/CMake source identity verification, no active process and native path/mount checks. Source checkout, offline .deb/receipt, installed runtime and all evidence remain. This avoids repeatedly retaining a completed runtime build while Store testing is blocked. Regression rejects modified payload/mounts and preserves active build/source/offline recovery.
Store-runtime-e passes120 runner checks, restores only recognized installed CPython cache and reclaims355667968 bytes, but2040565760 free remains below2GiB. Supported Storage proves the PPSSPP generated build was already removed;2.547GB is its managed upstream checkout. Last-resort cleanup now may remove that checkout only with verified pinned offline .deb/receipt, exact pinned Git HEAD, empty tracked/untracked/ignored status including recursive submodules, no active cache process and no redirected/mounted path. Source is re-fetchable upstream code, not authored workspace snapshots; offline reinstall package and installed runtime remain. Dirty/unknown checks fail and preserve the checkout. Regression proves dirty refusal and exact clean-only removal.
Store-runtime-f passes120 runner/six focused Store tests, strict original r61/61 payload reuse, deploy4b8acad73bafd1cc61ae3c77. Fixed support package configuration now succeeds: SDK33, x86_64/x86 and exact image identities/features; real read-only mounts return cleanly. Web lifecycle passes7 reviewed frames/50s: actual Plex interface with bridge highlight, same-card Home return, Cancel-first confirmations and package/manifest removal. Signed discovery passes8 reviewed frames/101s,4891 browse matches/all5 NewPipe results, compatible Install action enabled while idle, Settings return, normal layout hashes unchanged, service active/0 restarts. No Android protected install or playback claim. Continue current-candidate installed clients case for external Chrome remote/owned return; no rebuild/full or reference rerun.
Current-f clients attempt044526 times out at its legacy attended freetubepermission checkpoint; reviewed capture shows FreeTube already open/loading, no Isolde permission dialog. No consent key was sent; service/layouts restored unchanged. This is not an app failure or a pass for external Chrome. Retry the same frozen candidate, review each ordinary app checkpoint and use the existing bounded instruction files (empty keys when no dialog); no OS-auth fixture or polkit change.
Installed clients retry044933 passes8 reviewed frames/192s. freetubepermission: actual already-open app, empty keys (no consent); streamingpermission: actual CBC Gem, two physical Right keys, next frame visibly selects Browse, physical Home returns to Isolde. Normal layouts unchanged/service active/zero restarts, no account/subscription playback. appsoverflow now genuinely shows All Apps in this attempt. Native+client scope only; broader external/embedded text/Menu/scroll installed matrix stays open. T29.6 retains original native0.5 and splits remaining0.5 into external directional/return0.25 done and all remaining0.25 open. T29=75%, sum2143/25 still86%,175/216; no erased credit or original group weight change.

### Current reviewed closure — 2026-11-12

Installed4b8acad73bafd1cc61ae3c77, sourceDigest
b2ff543b99bf3f955b9631cd9554b6aaec5e5a4c3ab862f00e593c2068d2e543.
Store-runtime-f passes120 runner checks and all6 Store registrations, zero
skips, with strict original feedback-r61/61/source/payload reuse. Production
deltas remain bounded to AppStoreView and the three named Store/setup helpers;
no C++/Main/schema/renderer change or new full-suite claim. Unchanged r Gallery,
Search/Sources/reference evidence remains applicable. Package support installs
and configures successfully, clearing the earlier half-configured package:
read-only ext4 image metadata33/x86_64+x86, trusted image identity and root-owned
cache, sealed application metadata directory. No policy/normal-profile grant
change. Both Web50s and signed discovery101s pass with15 reviewed captures.
Clients192s adds8 reviewed captures; first attended-timeout attempt3 captures
excluded. Explicit ledger38 reviewed/3 excluded/0 pending. All three journeys
preserve both normal layout hashes and restore active/running/zero-restart service.

T29 is75%; sum2143/25 rounds86%,175/216 independently evidenced milestones.
Accounting checks preserve old native0.5 weights when splitting original
remaining0.5; invalid total weights are rejected. No tests are rerun for accounting.
Remaining Android protected positive lifecycle and installed browser text/Menu/
scroll/full-origin/software matrices stay open. The authorization proposal is
pending, not installed or counted as acceptance; do not bypass automatic review.
If that policy action is not approved, choose a safer non-persistent OS-boundary
fixture rather than asking to waive a software requirement. Ordinary native
administrator authentication remains available, never through chat credentials.

### Browser controls batch — 2026-11-12 (verification in progress)

BUG-121 (T29.6): real external Chrome inherits the page's ordinary `button`
margin/content-box rules in Isolde's injected page actions. In web-controls-k,
all controls work, but the reviewed Menu image shows its buttons extending
past the panel's right edge. The shared bridge now resets its own boxes, rows,
caption and buttons and uses bounded border-box/flex geometry. The controlled
page keeps its interfering CSS; keyboard/menu containment is now an actual DOM
assertion, alongside physical-key text Cancel/Done, explicit scrolling, history
Back and owned sandbox exit. No pointer, external debugger listener, account,
normal-profile data or authorization-policy change is involved.

The new repeatable fixture uses a private guest Xvfb display and production
NativeSandbox/native-host/pipe bridge. Existing guest apt delegation installs
only xvfb through Configure browser-test-tools; the host is unchanged. The page
is a declared loopback service boundary, not a claim of subscription playback.
The embedded WebEngine tests remain actual engine/input tests. Previous installed
CBC Gem/Home and Plex/Store origin evidence is retained by source applicability.

Cause-specific preparation failures are retained: a is host sandbox-denied Git/
SSH (no VM run); b rejects the new helper; c additionally identifies the newly
committed authorization-proposal document as absent from the old full manifest;
d's self-test rejects broad documentation additions. The final guard preserves
that restriction and names only those two additions for Store. e's diagnostic
was consumed by the test assertion; f retains the exact missing-Xvfb traceback.
Guest-only xvfb setup passes. g/h identify private display/window naming; i
identifies an unmapped Chrome helper (BadMatch); j lacks the actual page-focus
readiness barrier. k passes six focused registrations/120 runner checks and25
actual X keys with five reviewed external frames, then reveals BUG-121 during
visual review. No failed attempt earns acceptance. l verifies the repaired
shared JS, with no full-suite or unrelated reference rerun.

BUG-122 (T29.6): external Chrome had no owned-session return when remote Back
reached its first browser-history entry. The shared bridge also used total
history length, which includes forward entries after Back. A source review of
that final route kept T29.6.c open despite l's passing text/Menu/scroll/history
journey. The fix uses the browser Navigation API's actual canGoBack capability
(with the existing legacy fallback) and one fixed `return` binding restricted
to the IsoldeRemote isolated world and attached owned sessions. It exposes no
arbitrary command/data interface or listener. Native-host return reuses the
existing graceful stop/owned-group cleanup. m's extended actual Escape test
correctly fails on missing Runtime event delivery; n enables that delivery and
retains actual history/focus/key observations and failure screenshots. Empty
appearance still fails closed. No missing Back behavior is labelled external.

l itself passes120 runner checks/all6 Store registrations, sourceDigest
120223c7b2595ac9cec99cafcbd63b45f4ec078d2077ad9e45b5ea85ea3165a7;
installed67e84ef799df04ef880721b0. Seven installed Web lifecycle captures pass
and are reviewed: actual Plex interface, physical Home return to Open on Plex,
Cancel-first remove and verified absent manifest. Normal layouts unchanged;
service active/running/zero restarts. Twenty native captures are reviewed by
new images or exact prior pixels. This is the intermediate overflow fix, not
acceptance of the as-yet-unverified root-Back addition. T29.6.c remains open
until the complete corrected production route and source applicability pass.

### Browser batch accepted — final web-controls-s

S passes120 runner/all6 focused Store tests with strict original feedback-r61/61
coverage reuse. SourceDigest b05da56a1c67e69ca3b8a9973a31e38221b0fd2770be7b1855befb1cf4106bc4;
installed c2b0ac24fadcf2f7652d986b. Actual external Chrome records26 physical X
keys, text Cancel/no-write and Done/1, contained keyboard/Menu under interfering
page CSS, explicit scroll, real second-page navigation/history Back, then an
additional Escape closes the owned sandbox with rootBackExitVerified=true.
Actual embedded WebEngine covers controls/dialog keyboard/top-layer/shelf.
Shared Main/SessionManager/DesktopSession/NativeSandbox origin restoration is
unchanged from accepted installed f CBC/Home and r collections native exits.

Final installed Plex/Store journey passes7 reviewed captures after a diagnosed
guest PipeWire timeout: install, real public interface, physical Home returns to
Plex/Open, confirmed remove and manifests absent. The first attempt creates no
application window and earns nothing; its log retains pw_core_sync timed out.
Guest-only, locked audio-test-recover stops the shell, restarts/verifies PipeWire,
PipeWire-Pulse/WirePlumber and restores prior shell-active state without changing
settings. The same candidate then passes. Audio-service-denial startup resilience
remains broader T12 work, not new credit here. Both normal layout hashes unchanged;
service restored active/running, PID341290, zero restarts. Explicit ledger27
reviewed/0 pending; failed attempt retained separately, no failed PNG credit.

n's regression catches an independent key listener closing the session during
keyboard Cancel. The final single shared handler handles keyboard/Menu/history
before invoking root return. o stops before VM tests on concurrent new input
files. A detached workspace-only test checkout preserves that coder's work;
p/q refuse checkout byte/EOL differences, corrected by restoring frozen l bytes
and applying only this batch. r meets another healthy run's lock; s waits through
the bounded runner and passes. No healthy run is interrupted. Its unused
remote_bindings runner registration is harness-only and not selected; the
concurrent input application/QML/config/CMake changes are excluded from the
installed candidate and this batch's commits. Preserve their staged files.

T29.6.c and its parent accepted under unchanged weights; Store75→78%. Combined
working-ledger accounting (including the independently recorded T12 binding
milestone) is2148/25=85.92%, rounded86%,177/218. No Android install/update/
protected rollback, combined disconnected Generator or subscription playback
claim. No policy was installed or approved. Next independent Store work remains
offline/profile/error recovery, alongside the protected Android lifecycle.

### BUG-123 — Failed Store activation leaves a new package manifest (T29.4)

store-close-20261112-b: installed offline UI reports Unable to save app
installation state and restores Install, but the new Plex manifest remains.
The actual config directory was read-only; the previous activation and registry
were preserved. AppCatalog now records whether it created the package and removes
only that new artifact after registry/activation failure. Pre-existing staged
packages remain unchanged; cleanup failure is surfaced. Native regression checks
new-manifest removal, byte-preserved existing package, unchanged failed activation,
registry rollback and successful retry. Failed attempt/captures retained.

The new offline journey uses three actual process starts/two profiles, inherited
kernel IPv4/IPv6 socket denial and unchanged Unix/display ownership. Default Cancel,
write failure/retry, isolation, cold persisted app, Web error/return, removal and
explicit failed online opt-in/OFF recovery are one bounded automatic batch.
Initial a self-test accidentally included concurrent new keyboard expectations in
the isolated old keyboard tree; b restores the frozen test bytes, retaining the
receipt guard. c correctly refuses AppCatalog.cpp under old bounded reuse. Keep
that guard strict. One full integrated candidate now covers the new package
activation fix plus the coder's committed InputManager/physical keyboard changes.
No new full run for each subsequent helper correction/accounting step.

### Store offline/profile closure — store-close-20261112-full-d

Full62/62 passes (source c84d65c27c28f1ea7dadd0e9d36e7c2ff238f1647f5eccf4f4ad2ebcb956fcee),
deployed638b0fd8106ac8fd9c54ccf0. BUG-123 fixed: installed write-failure now
leaves no new manifest; native test additionally preserves a pre-existing package
byte-for-byte. No application suite repeated after the passing full candidate.
First installed attempt101242 fails because fixture profile B omitted Kodi
AutoStart=false and Kodi covers the Store; not an accepted profile matrix.
Corrected both disposable profile preferences. Exact SHA-addressed Store helper
overlay preserves frozen source/dependencies and tested executable. Added path,
arguments, frozen dependency and tamper refusal coverage. Maintenance bundle's
first self-test failed because Store helper was omitted; added that exact file to
both manifest allowlists. Current self-test log guest-maintenance-self-tests-
1794507692268125399.log passes121/121. Guest-only verified maintenance succeeded.

Installed attempt101748 passes15/15 captures, all agent-reviewed in contact
sheets store-close-d-pass-00/01/02.jpg. Raw receipt:
build/vm-oracle/store-close-20261112-full-d/report/evidence-store-offline-1794507612601914050/isolde-components-acceptance-store-ziw8rzv6/result.json.
Captured frames: same run visual/store-offline/attempt-20261112-101748-723/.

Semantic review: entry Android local list, focused default Cancel install/remove,
visible save failure and retry to Open app, profile B initially uninstalled then
installed/removed, profile A cold saved Open app, real embedded Plex offline
ERR_INTERNET_DISCONNECTED with recovery controls, physical Home restores exact
Plex/Open app, both confirmed removals restore Install, explicit repository
resolution failure remains navigable, access OFF returns local list and visible
Settings network category. No obscuring window in accepted15. All reviewed;
raw machine visualReview=pending is preserved, with this separate agent ledger.
Three PIDs369389/369796/370146 each prove IPv4+IPv6 errno101, Unix available,
root ownership preserved;84 actual remote actions. Activation and manifest bytes
remain isolated through A→B→A. Normal layout hashes match and service restored
active/running/zero restarts. No live playback or Android transaction claim.

Under original fixed weights, T29.4 remaining half splits equally into Android
write/crash/network acceptance and evidenced Web write/profile/offline recovery;
T29.5 remaining quarter splits equally into Android package lifecycle and the
Store portion of disconnected Generator. Native/previous children keep weights.
T29.4.c/.5.d done: Store78→83%, project2155/25=86.20% rounded86%,180/221.
Protected Android .2.b/.3.c/.4.b/.5.c/.8.b remain open. Explicit scoped
VM authorization question is unanswered; no rule installed or bypass attempted.
### Android automation authorization repaired; BUG-124 stopped IPC snapshot

User explicitly selected I approve this temporary VM polkit rule after the full
single-profile prepare/recover/15-minute/removal scope was stated. Earlier policy
patches were rejected and wrote nothing. The approved fixture now installs only
inside Isolde-Test via a separate test package, validates the active local user,
root-owned production helper and exact private UUID/path commands; independent
root monotonic expiry precedes rule publication, with wall-clock expiry too.
Helper and runner both verify removal before ordinary service restoration.
No normal-profile grant or host policy change. Native terminal fallback also
reaches the actual GNOME administrator dialog; its unapproved attempt expired.

Fixture fixes: create Android parent directory0700; keep disposable data under
guest /var/tmp (same filesystem as protected /var/lib backups), config/evidence
under /tmp. Kernel mount separation is not waived. Authorization remains bound
to exact paths for one UUID. Operations now detect early helper exit instead of
waiting another150 seconds after a displayed failure.

Installed full-d attempt105421 verifies protected Android install, actual
SmartTube launch and owned return, then fails removal because its stopped Android
data contains an IPC special inode. Retained failure screenshot shows unsupported
live file even though the owned session has stopped. BUG-124: snapshot preparation
rejects persistent Unix sockets/FIFOs and would block opening a FIFO during fsync.
Allow stopped IPC inode types through cp --archive (no stream read); fsync only
regular files. Devices/unknown types remain rejected and root entrypoint still
requires idle runtime. New Linux regression creates an actual Unix socket/FIFO,
uses actual cp rather than the old copy mock, verifies preserved types and exact
save/receipt rollback. Focused Store candidate store-android-20261112-e reuses
full-d62/62; no new full-suite run. No lifecycle acceptance awarded yet.
Focused e correctly refused the previously unlisted transaction module before building. Add only tools/android-store-transaction.py to the Store boundary, alongside its required store_lifecycle actual-IPC regression. This isolated Python inode/snapshot fix changes no shared AppCatalog, schema, input or renderer. Self-tests still reject unrelated AppCatalog/Main/config changes and changed base payload. Use f with the same original full-d62/62; this is an explicit tested boundary extension, not a full-suite claim.

Focused f stops at web_remote: the real Chrome mutation observer already selected the first control before the fixture's initial Right, so Right correctly moved to the entry field. Embedded matrix passes. Normalize initial origin with physical Up (unset or first control both yield first), retaining all actual text/Menu/scroll/Back assertions. This is a diagnosed fixture race, not a browser pass or a production change. Retry g once with both IPC and origin fixes.

### Installed Android lifecycle now automated and accepted — g / attempt111140

Releasea9ee9953e115ed5a20d970ff; six affected Store registrations pass, including
the actual Linux archive/socket/FIFO rollback test. Original full-d62/62 reuse
is verified under source079a8c6bb023a27af22e0276cdfaf38cbee7df768866e09a8d49672e94f5d6ad.
No additional full run. Root Android support updated from that tested payload.
Raw installed receipt:
build/vm-oracle/store-android-20261112-g/report/evidence-store-1794510941294490934/isolde-components-acceptance-store-x_i22lnw/result.json.
All14 frames in visual/store/attempt-20261112-111140-469 reviewed via
store-android-g-00/01/02.jpg; five new real external Chrome frames reviewed via
store-android-g-browser-00.jpg. No pending semantic review for these19 images;
raw machine visualReview=pending remains unchanged, this is the agent ledger.

Observed: SmartTube local card, Cancel-first install consent, installed badge and
Open app focus, actual SmartTube public menu, physical Home returns exact card,
Cancel-first removal and verified Install state; Plex consent/Open/public page/
physical Home/exact Store card/confirmed removal. Both Android confirmation
cancels preserve exact activation bytes.27 ordinary remote inputs. Actual
package presence/absence is verified before activation by production helper.
SmartTube menu is not playback proof. Guest desktop chrome is visible around
SmartTube and remains an open full-containment item. Long details scroll when
Uninstall is selected; selected Open is fully visible in reviewed return frames.

Permission receipts: exact UUID7e8680f9-7fdd-43b6-a15b-985634d1ee37, uid1000,
active local session1, created1794510713/expires1794511613, only exact prepare/
recover commands, independent monotonic root timer. Two positive and two negative
scope probes allpass. Probes run from trusted installer against the live test
PID/start/UID because current polkit refuses unprivileged CheckAuthorization
requests with action details; first probe failure was correctly diagnosed and
retained, not waived. Normal profile and root-job probes are denied. Rule removal
verified at1794510938, again by finally and runner at1794510940, before service
restore. Cleanup also identifies actual stale Unix sockets49152:
waydroid/data/system/unsolzygotesocket and ndebugsocket. This confirms BUG-124's
cause and the real installed fix. No persistent authorization left enabled.
Normal layouts byte-identical; ordinary service active/running/zero restarts.

T29.5.c accepted and parent complete. T29.8 original remaining half splits into
independent curated installed lifecycle (.c,quarter) and remaining downloaded
updates/fault/profile/setup/containment (.b,quarter), native .a retains half.
T29.8.c accepted. Store83→88%, project2160/25=86.40% rounded86%,182/222. No claim
that the rest of Store or backlog is100%; next work is downloaded APK update and
fault/profile/containment acceptance. One final runner assertion initially
misclassified the deliberately denied --root-job probe as a grant; it now checks
that the grant list contains only prepare/recover and root-job is a false probe.
Final runner self-tests pass121/121 in guest-maintenance-self-tests-1794511182821065611.log (first false assertion retained separately). Maintenance itself reports unable to reach its3 GiB cleanup target; do not label Configure as passed. Free space remains above the2 GiB test minimum. No host cleanup, lowered gate or extra application run. All temporary rules removed; installed lifecycle evidence remains valid.

### T29 installed signed updates and crash recovery — 2026-11-12
Installed g attempt20261112-113406-693 passes store-updates. Evidence:
`build/vm-oracle/store-android-20261112-g/report/evidence-store-updates-1794512334084308841/isolde-components-acceptance-store-3nbwe4zh/result.json`
and adjacent `store-update-evidence.json`. All ten captures reviewed in contact
sheets store-updates-g-00/01.jpg. Actual signed NewPipe1014 fixture baseline,
prepared transaction SIGKILL before activation, production recover preserving
save and removing uncommitted package/runtime; actual Store1015 update, visible
progress, cancel-first consent, subscription persistence, cold reopening,
second-profile absence/stale request refusal and verified removal pass.
Temporary exact-profile rule removed before normal service restoration. Normal
layout hashes unchanged; service active/zero restarts. Fixture baseline is not
claimed as UI install; g's prior real SmartTube install/open/remove remains
applicable. No playback or actual Android ENOSPC claim. T29.2.b/.3.c/.4.b credited:
T29=97%, overall2169/25=86.76% rounded87%,185/222 milestones.

Fullscreen containment batch introduces a typed per-profile Store preference,
owned nested Wayland kiosk compositor and guest Weston setup dependency. One
new full CTest run is required because shared AppCatalog persistence and the
runtime/display boundary changed; narrow g reuse is insufficient. Subsequent
harness-only installed retries reuse that passing candidate.

Full h stopped at studio_performance after six passes:1080p one input498.67ms,
limit250ms; remaining inputs/commands mostly single-digit ms. Store settings
selection passed. No Studio movement/render source changed in this batch.
Retain failure as BUG-125 pending diagnosis; isolated studio_performance rerun
uses unchanged250ms gate to distinguish repeatable regression from VM stall.
Isolated i passes all3 registrations (performance/component contracts), unchanged
application and250ms gate. BUG-125 remains retained as non-reproduced timing
failure, not an implementation fix. Full integration j is justified by h's named
full-suite failure; no threshold change, Studio code change or suppressed test.
Focused k refused reuse before compilation because no application QML delta;
not a test failure. Direct l stopped at externalChromeRemoteBoundary: initial
physical Up was not observed (selected/lastKey empty after native window startup).
Embedded control test passed. BUG-126 fixture X focus readiness race: establish
owned window focus after visible bridge readiness and XSync before first input;
no product input change, command retry or weakened semantic assertion. Focused m
reuses original full j applicability manually; no repeated full suite required
for this harness/test-only batch. New Android runtime fixture not yet executed.
m reproduces initial no-key observation despite X focus barrier: BUG-126 remains
open. Capture shows actual page/bridge, no focus ring. Physical fixture now
checks XTest press/release success, synchronizes each edge and holds30ms instead
of sending both edges before any server/client processing. Semantic expectations
unchanged; this is not a successful diagnosis until the focused run passes.
n passes actual external Chrome and the other four Store registrations; BUG-126
physical press/release synchronization now verified. New lifecycle fixture fails
because its pre-existing cross-platform setUp replaces Linux fcntl with an empty
stub. Use actual fcntl on Linux and keep the import stub only on other platforms;
this exercises real private lock ownership rather than mocking it away.
Final o passes all6 Store registrations, including lifecycle22 and real external
Chrome. j original62/62 and installed14+16-frame reviews remain applicable to
unchanged application bytes. T29 accepted100%, project2172/25=86.88% rounded87%,
186/222 milestones. Exact scope/matrix/fixture limits in
`store-final-acceptance-2026-11-12.md`. BUG-126 fixed; BUG-125 not reproduced and
retained. Live-content spinner is not playback evidence; U01 handoff explicit.

T03 real Kodi installed local-search attempt153522 passes with4 reviewed captures
on full-tested installed j. Actual one show/three episode matches, kernel
IPv4+IPv6 socket denial, AF_UNIX bridge and exact Home clock focus restoration.
Normal layouts unchanged, service restored. T03.4.b credited: T03=92%,
overall2181/25=87.24% rounded87%,187/222. Expanded56-episode paging/play/return
journey now pending; no full app rerun for harness-only changes.
Expanded real-Kodi local-search attempt153805 automated receipt passed, nine
captures reviewed:56 actual episodes,52 first-page rows,8 second-page rows,
Previous returns first page; selected Episode1x50 actually plays. Agent review
rejects closure: BUG-127 stop returns Home rather than original Search. No .4.c
credit. Fix search-origin tracking independently of previously open library;
KodiLibrary emits a dedicated return signal to reopen existing Search without
resetting query/page/selection, while physical Home stays Home. Native QML
regression covers stop and explicit Home distinction. One full T03 closure Test
is justified by Main/KodiLibrary global return routing changes; prior Store full
cannot cover this delta. Repeat installed matrix on the passing new candidate.
BUG-128 in the same reviewed expanded journey: selected More Episodes is outside
viewport. contentY used index*82 but actual row76+spacing10=86. CommandCenter
now derives scroll from selected delegate geometry; native60-row visibility
regression added. BUG-127 also requires retaining local search clients/page and
selected row across playback: CommandCenter snapshots origin before close and
resumes without restarting/resetting the query. p is already a frozen healthy
full run, so do not interrupt it. After p passes use supported strict
studio_visuals focused reuse for this CommandCenter-only follow-up plus native
regressions (Main/KodiLibrary bytes unchanged from p), then deploy that final
candidate and repeat real56-episode installed acceptance. No additional full
run for this bounded follow-up or harness evidence.
q focused run passes return-state preservation but long-list regression cannot
find a Repeater delegate through QObject ownership. Native harness now asserts
60+ results and traverses the real QQuickItem visual tree using existing
findItem, then checks full selected-row containment. Retain q failure; geometry
acceptance stays open until native and installed review pass.
r fails build at the diagnosed test-only findItem scope error; no tests run.
Corrected local recursive visual-tree traversal is now self-contained. Retain
failed evidence. No application change or new full-suite requirement.
s passes universal Search origin/long-list tests, but existing TV theme repaint
traffic assertion catches an asynchronous artwork RPC arriving after arbitrary
200ms sleep (requests7 vs6). Replace that delay with actual KodiArtwork terminal
no-cache response before counting; keep zero additional repaint requests.
Attach long-list regression to an actual offscreen QQuickWindow and require
selected row y>4000 as well as complete viewport containment, ensuring real
layout rather than an unpositioned tree can never satisfy geometry acceptance.
t focused5/5 passes with strict p62/62 reuse, deployedc0c5f1fa241bf1b2c11f93cc.
First installed retry stops before captures: Kodi publishes the show before
all56 asynchronous XML-import episode records exist. Fixture previously asserted
count immediately; now wait bounded60s for exactly56 across two50-bounded RPC
pages before search input. Preserve failed attempt160955. No product change,
no full/focused rerun needed for this import-readiness repair.
Final t attempt161123 passes automation; nine captures reviewed. BUG-128 actual
More row visible, BUG-127 Search query/page2/Episode1x50 exact selection restored
following real playback. Closure still withheld: backing out of restored Search
loses underlying library origin (origin frame is Library; final is Home). Main
now captures/restores the underlying library visibility separately from Search
state. Physical Home still bypasses return signal. This shared origin-routing
change exceeds existing Main-excluding focused whitelist, so another full run is
required by the current runner. Earlier full p was launched before complete
origin review; retain it but do not misrepresent its coverage of this later fix.
u full stops after35 passes at existing pageComposerAndIndependentKodiWidgets:
second history widget observed0 during its independent asynchronous refresh,
expected16. Search/geometry/return native regressions pass. BUG-129 fixture:
app session invalidates all history clients but test awaited only appHistory,
then synchronously compared otherHistory. Await exact2/16 rows for both clients;
no count relaxed. Full v repeat is justified by this named integrated failure,
not accounting or the unchanged application. Preserve u logs and no .4.c credit.
v full stops at recurring BUG-1251080p frame max514.11ms (720p59.04ms), unchanged
Studio code. Retain failure; no threshold alteration or claimed Studio fix.
Redesign the overly broad Main reuse prohibition for the exact selected-task
origin correction only: compare checksum-verified frozen Main texts; apply three
unique literal edits to p's original text and require byte-for-text equality to
the new text. Reject any extra/missing/unknown routing edit. Unit positive and
negative guard tests added. studio_visuals then verifies native Search/window
geometry/return contracts, full p62 unaffected coverage remains source-bound,
and installed actual return review remains mandatory. No general Main whitelist,
no removal/skipping of performance tests or claim that v fully passed. Existing
p/j full performance passes cover identical PageStudio/performance bytes; keep
intermittent frame stall noted as unresolved diagnostic, separate from T03 route.
w confirms scoped Main guard passes but reuse rejects the later TV import-wait
harness correction, absent from StudioVisuals harness list. Add only that named
already SHA-overlay-controlled fixture helper; application allowlist unchanged.
Coverage errors now name up to16 out-of-bound relative files for direct diagnosis.
No test failure or new acceptance credit from w; self-tests pass before refusal.
Final x passes5/5 with strict p62/62 reuse and scoped Main guard,123 runner tests.
Installed releasef21c113eb646a6997202177b attempt163901 passes81s journey; all9
frames reviewed. BUG-127/128 fixed: visible More, exact Search query/page2/
Episode1x50 and underlying library/toolbar restored after actual Kodi playback.
Normal layout hashes unchanged/service active/zero restarts. T03.4.c accepted,
T03=100%, overall2189/25=87.56% rounded88%,188/222. Evidence and retained BUG-125
intermittent offscreen frame-stall diagnostic in t03-search-acceptance-2026-11-12.md.
No claimed fix/pass for the retained Studio timing failures. Next T04 by priority.

T04.1 batch in progress: BUG-130 no-op import and BUG-131 shallow/ineffective
original-source rollback replaced by unprivileged snapshot/transaction helper,
async bounded C++ adapter, own-backend suspension/shared native lock, startup
journal recovery and teardown cancellation. Source desktop .kodi is read-only;
private profile replacement/rollback uses rename+fsync/journal, checksums,
profile/source binding, no links/shared mounts/special files,4GiB/20000 entries.
Native actual SQLite/nested-addons fixtures, deterministic write/cancel boundary
failures and real subprocess crash recovery added. No new acceptance credit yet.
T04.1 split adds4 open milestones under unchanged parent weight (188/226).
Focused Kodi batch builds app and runs media client + new import contracts before
any task-level full run. Installed workflow and remaining T04 groups stay open.
T04 a application build passes; import-contracts fail at destination group-write
refusal because Qt/test roots inherit VM umask002. Harden only verified
user-owned Isolde destination/config/backup directories0700, never original
source; source-mode preservation and destination-private assertions added.
C++ integration now exercises real helper scan/backup/preview/import/cold read/
rollback, complete nested files, backend guard refusal and original source bytes.
A failure retained; run b focused after this complete permission/integration batch.
T04 b stops at QML cache compilation after compressed confirmation callback
changes (reserved import property/statement boundary). Use explicit bracket
method access and statement terminators. The same batch now keeps the owned
Kodi backend paused/locked through Backup→Preview→Import so automatic restart
cannot stale the prior-profile snapshot; Back/hidden panel resumes without
import, active copy Back requests safe cancellation. Complete native C++ guard/
copy/cold/rollback test remains pending. No extra full test for this focused fix.
T04 c native tests actually pass: import-contracts13/13 and media-client35/35,
including real C++ scan/backup/preview/import/cold/rollback and guard refusal.
Runner rejects count because its expected-suite set still held only the original
media-client registration; update it to exact two names, no skip allowed.
Final d batch adds actual stopped bridge socket/FIFO exclusion (generated IPC
only; full config/DB data stays hashed), held backup review lock, and simulated
private backend writes on resume so premature restart invalidates the test.
Native tests first; installed workflow and whole-task full run still pending.

BUG-132: existing playlist acceptance captures management controls during claimed playback; UI lacks Open playlist. Add actual Open action and use physical UI launch/pause/stop in the existing journey. BUG-133: management drops catalog during rename typing and refresh races async mutation; retain catalog and defer refresh to completion. BUG-134: partial reorder can index past submitted order; reject any incomplete permutation atomically. Batch fixes pending full integration and installed acceptance; no playlist credit yet.
T04 integration full run justified by material shared backend lifecycle/import, helper packaging and playlist UI/bridge changes; prior T03 full cannot cover those boundaries. Run t04-integration-20261112-e once, then reuse its payload for existing acceptance journeys.

T04 e full63/63 passed and deployed release b1acaa32277bf53597c4a145, source5137b5ffdeef379fae1681417a11a0ae06c50c73bb7c690feb849dd55c3e71af. Installed playlists attempt200832 shows actual episode list but fails physical Confirm playback. BUG-135: KodiMedia activate/playSelected dispatch by container shows rather than validated episode member type. Fix episode playlist dispatch and extend existing savedKodiPlaylistCatalogAndShelf case with exact origin return. Also route importer Back to cancellation/confirmation handling rather than closing parent immediately (BUG-136). Repeat full justified by shared playback dispatch invalidating e's movie/episode/navigation coverage; no harness/reuse expansion. Desktop/movieart were unattempted after first failure. No acceptance credit.

T04 f full63/63 passed, installed58a7c894d6ffc1b1e158d68e, sourced0b2e32ed2689dd2728ce188cece041a974dd7e644ff7e8f35673f75ec10bf9b. Attempt202743 shows actual fullscreen color bars and paused OSD; episode dispatch fixed. Existing helper incorrectly used Kodi Escape as Stop (Escape dismisses OSD); use actual focused Pause and adjacent Stop via Confirm/Right/Confirm. Helper-only correction reuses f installed payload, no application suite rerun. Reviewed playing+paused frames; return still pending.

Correction to Stop diagnosis: managed KodiService explicitly maps Escape/Back to Isolde Stop. Both failed logs show CGUIWindowManager ignoring input while OSD closing animation runs (action122 in first, action2 in second). Existing helper now waits3s after paused capture then sends managed Back. Do not claim default Escape behavior as root cause, or mark Stop accepted yet. No application change/full rerun.

Installed playlist attempt203323-181 passes all7 reviewed frames and exact Episode1x1 return; .4.c credited under preserved original weight. Desktop selected case is Studio pointer parity, not Kodi DesktopSession: BUG-137 fixture reopen_clock stops at Page item panel, so its following move sequence never enters movement. Normalize through Find on page before repeating existing pointer journey; no application change/full run. Movieart remains unattempted after first desktop failure. Current acceptance2198/25=87.92% rounded88,191/228 milestones.

BUG-138 retained: desktop Studio journey now proves top-level pointer/remote byte equality and cancel, then fails its old positional catalog path while authoring the packaged nested input group (component count never reaches11). This is an obsolete authoring-path assumption requiring named current catalog/group flow; no nested-pointer pass and no Kodi DesktopSession claim. Keep failed attempts; do not rerun blindly. This was an extra Studio check, not T04 import/desktop acceptance. Return to selected T04 movieart existing journey; all T04 unresolved requirements remain open.

BUG-139 movieart failure is local fixture readiness, not yet a provider/network failure: xbmcaddon.Addon raises Unknown addon id before asynchronous registration completes (later log shows installation). Existing45s settings loop now retries that readiness exception within its original deadline. Extend the existing SHA-verified library-helper route to existing movieart case so frozen f application can be reused without rebuilding. No new acceptance scenario, no credit yet.

Movieart attempt204525 passes live Kodi TMDB identity/artwork decoding and original media hashes, but reviewed capture is playback instead of the promised Movies poster view. BUG-140: helper sends Confirm after the Movies shortcut, launching the selected item despite playbackVerified=false. Remove that Confirm and repeat only this existing case. No Isolde poster/editor UI credit from the playback capture.

Movieart attempt204701-920 passes actual Kodi provider matching, managed image decode, unchanged media and reviewed Movies poster/title/details. No title-art editing/source-management credit; no new percentage. Summary: docs/t04-kodi-verification-2026-11-12.md. Current f application full63/63 remains exact installed release; helper-only fixes do not require application full rerun.

T04 management batch BUG-141 undefined name validator, BUG-142 destructive collision rename, BUG-143 M3U reorder detaches EXTINF metadata, BUG-144 mutation errors disconnect the entire library and disappear from the controls. Implemented owned/regular/1MiB bounded reads, directory-locked recovery journal, collision refusal, XML rename label, full member permutations plus stale revision refusal, remote D-pad order draft, smart sort, rule preview/confirmed replace. Extended existing bridge case with real filesystem collision/write-failure and actual child-process crash recovery; no new test suite. Existing focused kodi_media_client now also selects its owning kodi_local_bridge registration alongside existing import contracts. Source/installed acceptance pending; no credit yet.

Management a stops before build: automatic verified cleanup leaves1.81GiB, below mandatory2GiB. Status confirms no live run. Existing source/payload pins retained. Last-resort cleanup now permits only declared isolde_core ELF .o cache files after all existing safe cleanup paths fail, preserving production source/application objects/selected-test objects and installed/staged evidence. Largest objects regenerated by Ninja until2GiB minimum only. Existing cleanup regression extended for exact core allowance, app/selected/active/unknown preservation and invalid policy refusal. This is required guest build-cache recovery, no host maintenance or acceptance change.

Management b still stops before build, no test failure: recovered176,934,912 bytes to2,093,473,792 bytes,54MB short of minimum. Core cache has no objects left. Extend same declared-target ELF-object last resort to application objects too (not staged/installed executable/source); selected test objects stay. Regeneration is required guest setup because only this reproducible cache can meet the2GiB floor while preserving all pinned proofs. Current cleanup self-test verifies production allowance only on opt-in. No further standalone Status/Configure loop.

## Combined repair source batch — 2026-11-12, verification held

Latest user direction holds additional application/VM runs until the larger repair batch is ready. No VM test/build/deploy has run after focused t04-management-20261112-c. Its three passing registrations do not verify the later Power, poster, browser-binding or added C++ assertions below. Current earned acceptance remains88% (2202/25),192/228 milestones; no source-only credit.

Implemented pending combined verification:
- BUG-078: real guarded login1 Power capability refresh and confirmed suspend/restart/shutdown, bounded asynchronous requests, denial/cancel status and a private-bus regression that never powers the VM off.
- BUG-091: Capture Settings measures wrapped header/row heights, contains the preview and offers remote details scrolling with exact setting return; custom gap/text scale no longer overlaps columns.
- BUG-138: existing desktop journey now locates the Widgets family by current declarative catalog identity before adding its nested input group; no nested acceptance claimed.
- BUG-141–144: journaled playlist mutations, collision refusal, EXTINF-preserving complete member reorder, stale revision refusal, smart-rule preview/confirmed replacement, smart sort and retained library/status on logical errors. Existing installed playlist journey covers the whole management batch before its playback/return check.
- T04 title poster authoring: actual title-details managed-art picker, no-write preview/confirmation cancellation, explicit apply, bounded PNG publication and Kodi SetMovie/TVShowDetails metadata handling with failed-ack rollback. Existing native bridge/media and installed movieart journeys cover this; no direct Kodi database writes or component-decoration substitution.
- BUG-145: source/content setup is genuinely incomplete: folder access still does not set a Kodi source's scraper/content rules. Removed the unnecessary frontend switch for stopped-backend grants, but did not claim that grants are a full source editor. Kodi's sources.xml stores locations; current public JSON-RPC has no source-content setter, and the native library builtins have no parameterized import. A real supported implementation is still required. Do not replace it with direct DB writes or count hidden Kodi-dialog fixture setup as product UI. Primary references: https://kodi.wiki/view/JSON-RPC_API/v13.5 and https://github.com/xbmc/xbmc/blob/Omega/xbmc/interfaces/builtins/LibraryBuiltins.cpp . This is missing software, not an external-account/hardware exception.
- BUG-146: custom shell bindings were not sent to external Chrome's remote bridge. Supported function-key/ASCII navigation/Confirm/Back/Home/Menu/Search bindings now travel through the existing private appearance/options payload; the existing Chrome boundary journey uses custom buttons alongside standard keys. This does not claim arbitrary raw IR/CEC or every native-window mapping.
- T09 preference failed-save path restores the prior cached QSettings value before returning; stale always-no-provider accessibility/Settings text now reflects the actual discovery status.

Runner change directly addresses the observed2 GiB storage blocker: opt-in pruning only declared rebuildable production objects after safer cleanup is exhausted, plus Debug -g1 line tables to reduce repeated object/binary storage pressure while preserving Debug/O0/assertions. The next combined candidate needs one full CTest, because application changes cross media/client/settings/input and invalidate scoped full reuse. Record the candidate once, then use it for affected installed workflows and reuse unchanged applicable evidence; no full run per small repair, screenshot or accounting row.

Static review: git diff --check and node --check src/input/web-remote.js pass. Power source/QML, later C++ and Python changes remain uncompiled/unexecuted. Remaining source work includes BUG-145 and the complete Kodi capability/setup UI; remaining T09/T10/T12/T14/T30 installed/virtual/offline matrices stay open. Preserve the six unrelated Studio test files. No runner is active.

BUG-147, subsequent playback API review: subtitle discovery used unsupported Player.GetProperties property `subtitle` instead of Kodi's declared `currentsubtitle`; its mock duplicated the invalid contract and concealed the production failure. Corrected both, explicitly pass SetSubtitle enable=true for a chosen track (false for Off), and extend the existing track test to start disabled before selecting. The picker now measures wrapped heading/status/rows, reveals the selected track and uses shared selected-text contrast/radii. Primary schema: https://github.com/xbmc/xbmc/blob/Omega/xbmc/interfaces/json-rpc/schema/types.json . No VM run; include in the same combined candidate.

Additional T04.6 integration gap retained: the existing Isolde subtitle picker does not itself prove the physical Menu handoff from the foreground native Kodi playback window, nor complete audio/viewing controls. Review native keymap/notification/focus handoff with the capability UI work; an API-only pass cannot close this advertised workflow. This is known software work, not a hardware handoff exception.

## T04 source/setup and playback completion batch — November13

BUG-145 source repair now includes a Kodi-native managed source builtin, typed
private broker, draft/preview/confirmed apply/remove UI, exact source/provider
backup and rollback, stale-preview refusal, video/music scanner controls and
safe idle-backend folder-access lease. Declared matrix and package provenance:
[kodi-managed-source-interface.md](kodi-managed-source-interface.md). Existing
network/multipath definitions remain preserved/read-only; this is explicit scope,
not a claim that arbitrary Kodi settings or add-on configuration are implemented.

Native Menu now opens an owned Isolde controls window, with common transport,
reported audio/subtitle tracks, picture size and current-list previous/next across
pages. Music visualization has managed transport/Menu keys. The capability guide
is validated packaged data, with setup and recovery routes. Existing bridge,
playback and Qt media registrations now contain the direct boundary assertions;
existing movieart/playlists installed journeys exercise fresh Isolde-only source
setup, poster edits and actual Menu/next/previous/picture/subtitle/focus return.
They have not run yet; no new registration or acceptance credit is claimed.

BUG-148 (source review): both new source-save and Power guards treated Isolde's
own Settings session as an external app, blocking their normal UI. Allow Settings
explicitly while preserving profile, external app, desktop, import and playback
restrictions. Repaired in source; installed verification remains pending.

Dependency setup attempts a–g are retained under build/vm-oracle. g compiled the
native source builtin and reached1166/1697 before the distribution-patched
SystemInfo.cpp failed on missing DEB_VERSION. debian/rules supplies that macro;
plain dpkg-buildflags does not. h supplies the exact installed version to its
consuming translation unit and reuses cached objects. Earlier DFSG dependency,
render-system and CMake target errors were diagnosed before retry. These are
native dependency setup failures, not application test passes. Guest build deps,
verified generated legacy cache cleanup and reproducible desktop index-cache
cleanup are recorded; an indexer restoration lease remains until setup succeeds.
No host cleanup or host toolchain installation occurred. Full application and
installed batch still pending; acceptance remains88%, T04=49%.

BUG-149 (source review): desktop-import preparation called KodiService.suspend,
whose legacy stop path kills the backend after four seconds if stdin quit does
not finish. That bypassed idle scan/save ownership. Import now uses the same
asynchronous clean-exit lease as folder editing, waits within the existing20s
backend deadline, and cancels preparation without copying/writing configuration.
Existing Qt desktop import case covers deferred wait, cancel, resumed operation
and source preservation. Import rows now size to wrapped shared-scale text.
Pending combined verification; no extra full test or acceptance credit.

BUG-150 native dependency packaging is now repaired by l: generated package
modes are explicit, fixed root-owned directories are corrected at dpkg configure,
and installed binary/manifest ownership and hashes remain required. Read-only k
storage evidence showed directories0775 while binaries were0755 and manifest0644;
reinstall alone retained existing directory modes. l passes installation and
restores the guest indexer; no application acceptance is inferred.

The repeated remaining disk blocker was desktop indexing of generated build
artifacts. Test now uses the same exact-path cache cleaner already covered by
oracle safety checks, pauses only initially active guest indexer units during
low-space builds, and restores them in finally under the existing runner lock.
A crash leaves a validated per-run Recover lease. Ninja-declared Kodi link
archives/staging duplicates can be discarded only after the installed dependency
passes ownership/permission/hash checks; native objects/source/package/evidence
remain. d stopped before application compilation after ordinary caches exhausted;
e is the combined application candidate's first actual build/full attempt.

Milestone reconciliation splits T04.5/.6/.7 under unchanged original group
weights, preserving every previously earned weight. No new evidence credit:
192/235 milestones, task49%, project2202/25=88%. Existing installed helpers now
also exercise real native source publication denial/recovery and the capability
guide's named setup route. These assertions remain pending execution/review.

BUG-152, e full-run failure: invalidate/retire cleared m_busy before generic
failure handling. This avoided per-operation nonfatal error branches, but also
lost the active scan/quit context; the existing cancelsScanMonitoring assertion
failed. Keep the context and explicitly request a forced disconnect instead,
while ordinary source/options/subtitle RPC errors remain nonfatal. Retirement
also clears newly added source/options data. Existing media tests retain scan
cancellation and assert source/options teardown; no new registration.

BUG-151 repairs restore the opening playback row after nested Back, make Menu
close controls consistently, restore Remove's own row after preview Back, and
hide Remove on a never-saved source. Existing Home navigation theme case now
exercises these actual QML methods. GUI helper navigation resets to known list
bounds rather than assuming the previous focus bug. The e run compiled the full
app and passed59/60 attempted registrations; Native profile/browser isolation
and Home layout were unattempted after its required stop at#60. Indexer finally
restoration passes. No passing full receipt or deployment is claimed.

f repeats full integration because e has a demonstrated full-suite failure and
the fix touches shared media disconnection/lifecycle plus remote focus. This is
the combined correction batch, not a rerun for a screenshot, commit or percent.
Keep all failures; same candidate then goes through installed/offline/standing
visual journeys. Current app changes remain unaccepted until that evidence.

November13 media batch: t04-combined-20261113-f passes63/63 existing
registrations, including the prior scan-cancellation failure, and stages its
exact candidate. Native poster publication/failed-ack rollback, managed-art
bounds and client cancellation now credit T04.5.c. Ledger193/235, T04=51%,
project2204/25=88.16% (rounded88%). Installed source/controls acceptance stays
open. BUG-153: stale source errors obscured successful retry, and the fixed
Music embedded-tag provider emitted an erroneous missing-provider message.
BUG-154: ordinary default library navigation lacked Music despite existing
album/song APIs. Both repaired in the bounded media follow-up; existing Home
navigation/media cases extended, no additional CTest registration. g attempted
zero application tests due to a runner indentation error; corrected h is the
active focused run reusing the exact original f full evidence. No failed run
is relabelled passed. Installed movieart now also covers a real tagged WAV
music source, Kodi scan, remote album/song browse and native Music controls.

BUG-155: new libraryViews getter deduced QJsonValueRef from a temporary JSON object; h aborts with heap corruption. Own a QJsonValue through value() instead. i cancelled after this concrete diagnosis, with guest indexer/service restoration. j verifies the fix; no acceptance credit for h/i.

BUG-156 runner notification inventory exceeded its fixed bound by traversing
application trees; k passes all126 runner checks and actual guest preparation
after restricting traversal to independently verified notification process IDs.
BUG-157 installed preview scrolling hides review context; read-only focusable
review fields and long-row remote paging implemented, awaiting verification.
BUG-158 installed native recovery loses pathless Video/Music add-on sources on
XML Load. Both strict order and full-record multiset checks fail, preserving
the finding. Native restore now retains those complete objects in all source
categories, including failed-publication retries; local source flags were
unchanged. No credit for the failed installed runs.

November13 storage repair: historical installed releases and focused receipt
pins grew without bounds. Verified retention c removes148 retired releases and
six staged payloads, restoring13.1 GiB. Repeat e passes129 runner checks,
removes nothing and retains12.8 GiB free. Current/previous/recovery/live and
reachable candidate dependencies are protected; all test receipts, profiles
and incremental cache remain. Ten simulated deployment cycles stay bounded.
Tracker exclusions use existing guest policy; native failure restoration now
closes the indexer lease. Repair pushed as f7e7ce5. Acceptance remains88%.

Managed Kodi p builds/installs corrected extension5349a0b41c76, binary
e6d262fbc356807927da370e98cac9e9e331949cfd168163c112f2768856fc96;
normal service remains active/zero restarts. Dependency evidence alone is not
installed recovery acceptance. q attempts zero application tests: exact reuse
guard rejects the new storage-policy document. Add only that named document
to the media harness addition allowlist; r resumes the same correction batch.

r passes7/7 and deploys cfc446352b79587dfc5c060a. Six installed captures
reviewed: preview is readable and remotely selected. Recovery still fails;
bridge masks nonlocal URLs as empty paths, so preserve nonlocal native objects
instead of only empty paths. s compiles that correction but apt rejects its
hash-derived package version as a downgrade. BUG-159: content hashes have no
chronological Debian ordering; allow replacement of the exact locally built
package. BUG-160: error status scrolls offscreen at Apply; visibility fix open.

t installs corrected7d9bac4fd7d9. Installed r attempt123227-354 proves exact
source inventory preservation and XML restore, closing the reproduced native
BUG-158. BUG-161 helper Up8 targets Frontend after returning from capability
source setup, shutting Kodi down rather than scanning. Movie lookup timeout
is retained. Correct helper to Up30/Down2/Confirm; no application/full-suite
rerun for this input-only correction. Artwork/playlists acceptance stays open.
