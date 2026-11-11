# Oracle VM testing — use the runner

Latest acceptance scope: [vm-software-completion.md](vm-software-completion.md). All S1 requirements are VM-provable; agent review and defined external-boundary fixtures suffice without user/representative/hardware/account prerequisites. A real twelve-hour `guide-soak` is optional extra evidence. Its existing receipt/time/identity checks remain mandatory **if that optional command is used**; do not bypass them to run a shorter test. Required Guide outage acceptance instead needs native elapsed-time invariants plus an actual bounded stopped-process/offline cold-reopen journey through the supported runner; implement that distinct path if unavailable. Hardware/service/user checks are [handoff only](hardware-handoff.md).

Testing frequency, readiness, evidence reuse and failure diagnosis follow [the execution contract](backlog-execution.md); current work resumes from [the queue](execution-queue.md). Focused tests follow complete batches, full Test is an integration gate, and harness-only corrections reuse a matching passing staged application through the supported runner. The `test-all.ps1` report does not remove existing timed review/input checkpoints: classify a case as attended until the helper actually supports unattended execution. All screenshot/semantic review and installed/offline gates below remain required.

## Standing machine-boundary permission (user, 2026-10-21)

Isolde-Test is a disposable Isolde test machine. Agents may perform necessary **guest-only** cleanup and testing/setup autonomously; do not repeatedly ask permission for obsolete guest build artifacts or ordinary test setup. User revision, November7: normal `Test` now automatically cleans verified obsolete guest artifacts below 3 GiB, after runner self-tests and under the existing guest lock. It uses the same checked maintenance implementation as explicit Configure. Prefer this single invocation over separate Status/Configure calls. A cleanup receipt records reclaimed bytes and whether the 3 GiB target and 2 GiB test minimum were met. Unsafe paths/checksums still fail; a missed 3 GiB target alone does not block testing above 2 GiB.

The Windows host is the user's personal machine. Keep host changes within the authorized project workspace/requested outputs; no unrelated cleanup, host packages/toolchains, services, security/settings or personal-data changes. Guest authorization does not extend to the host. This preference is also saved in the root `AGENTS.md` for future sessions.

The VM is the existing **Isolde-Test** developer machine. Preserve Household, Kodi, installed releases and the graphical login. Routine testing goes through `tools/vm-test.ps1` in **PowerShell 7**. Do not assemble ad hoc SSH/build/capture loops.

## Verified artifact maintenance

Normal `Test` needs no preparatory cleanup call. On low space, its existing connection also measures free bytes, automatically verifies up to 1,024 saved workspace archive hashes and sends one bounded receipt. No host archive is deleted. The guest retains the newest runs/three staged payloads, reuse and stopped-profile pins, logs/screenshots/manifests, installed releases, normal profiles and the incremental cache. It prunes verified older payloads, reproducible source copies, guest package/fixture caches and obsolete archives with exact host backups. Healthy disks skip hashing and cleanup. Extremely full disks that cannot receive/extract a source snapshot, or disks with no safely reclaimable artifacts, still fail with evidence. CTest now uses `--stop-on-failure` so one named failure does not consume the rest of the suite budget.

After reviewing `Status -Storage`, use `./tools/vm-test.ps1 Configure -ConfigureTarget artifacts -RunId <unique-id>` to recover guest space without a passing application payload. This runs the matching, hash-verified runner self-tests before cleanup. It retains the current/newest runs, three newest actual staged payloads, payloads pinned by stopped-profile receipts, archives/manifests, evidence, installed releases, profiles and the incremental build cache. Older generated payloads are removed only after verifying their passing full-test payload manifest; unknown/modified bytes stop maintenance. Obsolete extracted source copies are removed only after archive/member/source checksum and link/mount checks. The receipts record checked guest paths. This action requires the native Isolde-Test oracle directory, aims for 3 GiB free, and never performs host cleanup or deployment. Failure to reach the maintenance target is reported honestly; a later Test still enforces its own 2 GiB minimum.

Optional bounded archive recovery: `Configure -ConfigureTarget artifacts -RestoreSourceRunIds <up to 32 IDs>` restores saved workspace `build/vm-oracle/<id>/snapshot/source.tar.gz` files only after original archive SHA256 verification. Guest source cleanup then retains the exact originals and verifies extracted bytes before removal. `-BackedUpSourceRunIds <up to 128 IDs>` verifies those workspace originals and sends a hash receipt; guest pruning removes only matching obsolete archives with no source/payload/stopped-profile receipt, preserving logs/manifests. Recover such an archive through the first option when needed. No host archive is removed. Native `/tmp` fixture-cache maintenance preserves data/config/results, recent and active fixtures, and refuses links/shared mounts; this may recover only tmpfs rather than root-disk space. `Status -Storage` reports both usage and recovery candidates.
## Commands

Round-two sound verification extends `Visual -Installed -Cases themecontrols`
with actual Qt preview and disabled-output PCM recordings under dark, pastel
and enlarged custom palettes. It uses a temporary guest null sink, restores
the exact previous guest default output before restarting the ordinary service,
and archives sample hashes/levels plus the restoration receipt. Only streams
owned by the disposable Isolde process may be rerouted. Mock cue requests do
not satisfy this output check. Missing `pactl`/`parec` can be provisioned through
`Configure -ConfigureTarget audio-test-tools -RunId <deployed-full-id>`: this
installs only guest `pulseaudio-utils` through existing delegated apt permission,
without changing repositories, the host or application dependencies.

### One-action suite and results

Run `pwsh -NoProfile -File tools/test-all.ps1` from the repository root. It runs **all registered CTests**, then Studio and both reference fixture journeys against that same frozen build, and opens `build/vm-oracle/<id>/index.html`. The report includes individual test results/output, diagnostic links and every collected screenshot. It stops on the first failure, writes the report even on failure, and returns failure to the caller. No deployment or retry is performed.

Use `-NativeOnly` for all CTests without graphical acceptance, `-NoOpen` for agent/terminal execution, or `-RunId <id> -ReportOnly` to reopen existing evidence without running anything. Agents should make one invocation with `-NoOpen`, poll only that tool session until completion, then read the report and the named failure log once; no separate Status loop is needed during a healthy invocation.

For an explicitly requested broad bug-finding campaign, diagnose each named failure first, then use `Test -RunId <new-id> -ResumeRunId <failed-id>` (or the same options on `test-all.ps1 -NativeOnly -NoOpen`) to attempt only unattempted registrations. The runner verifies unchanged application/test bytes and the original incremental source, runs its current self-tests, retains every prior failed/skipped result in `campaign-progress.json`, and still stops at the next failure. This does not rebuild, produce a passing full-suite receipt or earn acceptance. Resume-only calls retain the 2 GiB test minimum; below it, the same verified automatic guest cleanup runs with the original candidate pinned. Above it they avoid cleanup and unnecessary backup hashing. An exhausted campaign may stage `diagnostic-payload.json`, explicitly `passed:false`, `fullSuite:false`, `acceptance:false`; `Visual -Diagnostic` can inspect that exact hashed payload only in staged disposable-profile cases. It cannot deploy it, use `-Installed`, or satisfy installed/software acceptance. Ordinary passing-payload and deployment gates remain unchanged.

Additional graphical journeys can be selected with `-Cases studio,desktop,lounge,pastel,pages,guide,guide-surfaces,guide-lifecycle,guide-features,movieart,repairs,apps,clients,vpn,shell,playlists,android,androidapps,store,games,emulators` from PowerShell. These require their existing runtime/account prerequisites; the list is explicit so unavailable provider journeys are never quietly skipped. The twelve-hour `guide-soak` still requires its existing same-run preparation and elapsed-time gate. Automated completion does not replace screenshot review, offline cold boot, physical qualification or full-product acceptance.

Collection acceptance: `Visual -Cases collections -Installed` uses the verified SHA-addressed Home/Studio helper overlay against the same tested executable. It creates a disposable profile and an explicitly labelled interactive desktop-client fixture, uses ordinary Store removal/reinstall and App/Button galleries, verifies native input/normal exit/exact repeated launch return, and refuses page-byte changes. It is not third-party FreeTube playback evidence. Seventeen captured phases still require agent review. `collections` is a distinct case; desktop/Studio cases retain their existing routes and frozen dependency paths.

Reference journeys upload home-reference-acceptance.py and reference_collections.py as independently SHA-addressed, verified helper overlays. Only Lounge/Pastel may use the collection overlay; frozen source/dependencies and the installed application remain intact. The receipt records both helper digests. Zero-byte idle socket timeouts follow the production bridge protocol and are retained separately as idleConnections; partial requests, reply errors and missing history/artwork calls still fail.

Shared-shell acceptance: `Visual -Installed -Cases themes` exercises three distinct Home/origin/root palettes in disposable profiles. `themecustom` covers only the custom inventory; `themefaults` covers gallery/loading/error/retry/return under all three; `themecontrols` covers Audio/Ambient/return and explicit private source/reset/discard. Select only missing or source-affected cases, preserving reviewed applicable evidence. Their receipts require all declared phases, unchanged layout bytes and nonempty unchanged native Kodi settings hashes. Fault loading is captured from the actual installed X11 window before releasing the controlled RPC reply; verify the three archived loading-frame SHA256 values and review those images. Later host loading-phase screenshots are transport diagnostics after recovery.

`Test -Suite shell_appearance -ReuseFullRun <original-full-id>` verifies the original passing full payload and refuses all unlisted production/config changes. Its local QML delta is limited to NetworkSettingsView, PageAppCarousel/OrbCard and Audio/AmbientSettingsView, with named test/harness files and one declarative fixture. If guest maintenance already pruned the original staged copy, the full base may be verified against its original retained installed release, requiring exact complete payload hashes and a safe release path; modified staged copies never fall back. Maintenance retains base payloads referenced by focused receipts. It rebuilds/stages the app and runs all four affected registrations; a reuse receipt is not another full-suite pass. Record the source applicability and review every newly changed capture before acceptance.

The separate code-quality/refactoring initiative runs locally and does not
invoke this VM runner.

T11 extended cases: `Visual -Cases guide-lifecycle` adds real natural-end/surf/deleted-file repair to the existing Guide surfaces journey; `guide-features` adds remote source/artwork/portability/template/calendar workflows. Their distinct additional scope is bounded to 1,740 guest seconds / 1,800 host seconds, with unchanged 110-second capture reviews. A successful lifecycle run records an off-time fixture receipt. `guide-soak` on the **same run ID** refuses less than 43,200 real off-time seconds, verifies the frozen executable and committed slots, and cold-starts only that disposable profile in a denied network namespace. Never count its preparation or a simulated clock jump as a twelve-hour/offline pass. Existing case budgets and normal-profile preservation remain unchanged.

From the repository root:

```powershell
# Optional: connection, tools, disk and service check; changes no release/profile.
./tools/vm-test.ps1 Check
# Android preflight reports bounded DNS resolution for the current release's
# pinned repository host plus read-only resolvectl status; it never changes networking.

# Normal development gate. Prints RUN_ID and EVIDENCE immediately.
./tools/vm-test.ps1 Test

# During implementation, run only the relevant native suite.
./tools/vm-test.ps1 Test -Suite component_contracts
# Fast Studio: studio_quick, studio_selection, studio_unit, studio_behavior, studio_colors, or studio (all registered partitions).
./tools/vm-test.ps1 Test -Suite studio

# Bounded Guide QML-only correction: reuse a verified original full pass.
# Current guide_remote_ui tests and a rebuilt/staged application are required.
./tools/vm-test.ps1 Test -Suite guide_remote_ui -ReuseFullRun <full-run-id> -RunId <new-id>
# Bounded section/placed-component focus correction; also runs Studio navigation.
./tools/vm-test.ps1 Test -Suite home_component_navigation -ReuseFullRun <full-run-id> -RunId <new-id>
# Other choices: home_component_navigation, kodi_media_client. Default: all.

# Use the exact RUN_ID printed by the passing full Test.
./tools/vm-test.ps1 Visual -RunId <id>
# Default: Studio, lounge and pastel sequentially, using the staged tested build.
# To rerun one graphical case without rebuilding:
./tools/vm-test.ps1 Visual -RunId <id> -Cases studio

# Only when deployment is requested: activate the already tested payload and restart.
./tools/vm-test.ps1 Deploy -RunId <id>
./tools/vm-test.ps1 Visual -RunId <id> -Installed

# If deployment stops at Android app registration because its setup receipt is stale,
# Configure android may refresh setup only when current exactly matches the full tested payload.
./tools/vm-test.ps1 Configure -ConfigureTarget android -RunId <same-id>

# Inspect a running/failed run. Never start a second build to get progress.
./tools/vm-test.ps1 Status -RunId <id>
# Explicitly cancel ONLY this runner's verified process; it restores the service.
./tools/vm-test.ps1 Cancel -RunId <id>
# After a crashed guardian, restore its recorded service state if no test owns either lock.
./tools/vm-test.ps1 Recover -RunId <id>
```

Deployment now holds the installation lock through registration and startup verification. On failure it restores both prior release links and the prior service state, retaining `deploy-failure.json`; a failed registration is never an installation success. Android setup authentication remains a real guest administration boundary, not something the runner bypasses. Reference journeys retain both full-desktop frames and bounded readbacks of the owned X11 application surface; label the latter as renderer evidence when guest desktop dialogs obscure the former.

`-ReuseFullRun` has four explicit boundaries: `guide_remote_ui` permits existing `GuideWidget.qml` / `GuideView.qml`; `home_component_navigation` permits existing `ComposedHome.qml` / `PlacedComponents.qml` / `AppGallery.qml` and runs both Home and Studio navigation registrations, including actual app-art/name geometry and gallery layout switching; `studio_selection` permits only existing `StudioInspector.qml` / `StudioAppPicker.qml` / `PageLibraryWidget.qml`, with actual-input selection and empty-update/frozen-renderer regressions. `studio_artwork` permits only existing `StudioImageFraming.qml` / `StudioArtworkPicker.qml` / `StudioInspector.qml` / `PageStudioView.qml` and the existing tests/studio/tst_colors.qml regression; it runs artwork, Studio selection, Studio quick and Studio colors regressions together; this covers actual component geometry in the framing preview. Each permits only its named regression/runner files and existing documentation alongside those QML changes. C++/configuration/build/unknown runtime changes, new/deleted files, another focused suite, a focused-only base or a modified base payload are refused. The runner verifies both manifests and the original full payload, runs current focused tests, builds/stages the current application, and records `fullSuite: false` with an explicit coverage-reuse proof. Visual rechecks that proof and both payload identities. This implements reuse for a bounded visual correction; T31 still requires one exact final-candidate full run.

The `pages` case is opt-in (`-Cases pages`): it exercises real Kodi in a fresh UUID native profile and synthetic media under `~/Videos/Isolde-Acceptance/`. It is not preservation testing of an existing Kodi installation. Do not point it at Household or `~/.kodi`.

For frequent code/test iterations, use [the focused Studio suite](studio-testing.md). These offscreen tests reuse the cached build without installing a release or restarting the normal app. Run full native/graphical acceptance at integration checkpoints; deployment remains explicit and occasional. No host toolchain installation is needed.

Studio and desktop graphical cases upload the current SHA-addressed `home-components-acceptance.py` helper while preserving the passing candidate's frozen source/dependency paths. Desktop retains its explicit `--studio --desktop --inspect` route. The runner records the helper hash; changed helpers never change the tested application/payload or earn a new application full-suite claim.

## Agent execution rules

1. Finish a meaningful edit batch before Test. Do not cancel a healthy build to add optional polish. If a tool returns a running session, poll **that session**; do not launch another SSH build.
2. Track required new source/test files before Test. Packaging includes tracked **working-tree bytes**, including uncommitted edits, and excludes unrelated untracked files. No reset/stash/clean is part of testing.
3. Treat the first `FAIL` as a stop. Read the named stage log and `host-failure.json`/`report/status.json`, fix that cause, then run once. No blind retries, new run-number loops, package installation, VM resets or lock deletion.
4. Targeted Test is quick feedback, not completion: it cannot stage, deploy or run visual acceptance. Full Test discovers the actual CTest count and refuses failed/skipped/zero-test reports.
5. Visual uses the exact run's staged executable by default, so it cannot accidentally test an older installed release. `-Installed` requires that run's explicit successful Deploy and verifies its selected release.
6. View **every** captured screenshot and compare both recreation/reset pairs with the fixture references. `captures.json` deliberately says `visualReview: pending`. Automated pixel/blank checks and capture acknowledgements are not design acceptance. T24 still owns genuine user usability/cutover.
7. Commit only source and concise evidence records. Do not stage `build/`, generated caches, PulseAudio cookies, browser profiles or binary artwork.

For installed native-app checks, verify the guest's graphical login is active (`loginctl show-session` and `/sys/class/tty/tty0/active`). A virtual-terminal switch can leave an old application frame in the VM capture after its process has exited; check process/session evidence before calling it an orphan or restarting repeatedly. Release synthetic modifiers and send chord events separately. Restore only the confirmed graphical VT (this VM's session was tty2 during the 2026-10-22 check), then recheck `Active=yes`. Do not assume a captured frame proves the app is alive, or count an unverified native Home-key return as passing.

## What the runner does

- Bounded SSH/SCP with BatchMode, ConnectTimeout and keepalive; no host-key bypass. One connection attempt, no automatic retries.
- Unique immutable source archive and per-file SHA-256 manifests per run. Archive extraction rejects traversal, links, unexpected files and hash mismatches.
- One guest lock serializes builds, deployment and graphical runs. BUSY is diagnostic, not permission to remove a lock.
- A persistent **Linux** Ninja cache at `~/isolde-oracle/work/` retains unchanged source timestamps and compiled objects. New run snapshots stay immutable under `~/isolde-oracle/runs/<id>/source`. Tests run under the lock against verified matching workspace bytes. A changed file rebuilds normally; a new evidence run does not force a clean compilation. Never put a Windows CMake cache there or edit the workspace manually.
- The first Test populates the cache. Later tests reuse it. Full Test stages a complete payload per run; no installation is needed for graphical verification. Deploy rechecks the passing full-suite/payload hashes and reuses the existing appliance installer's atomic release selection. It does not rebuild or run the same successful CTest again, change startup configuration, or reboot.
- Stage deadlines: configure 120s, build 1200s, CTest 360s (120s default per test), staging 120s; graphical 480s, real-Kodi pages 900s. Host transports/captures/acknowledgements have separate deadlines. Progress/status updates every 10s, with stage/log/error paths.
- On a graphical failure, timeout or cancellation, the guardian terminates its owned test process group and restores the initially recorded service state. Release links and normal layout hashes must remain unchanged. Recover refuses while either test lock is held. This is bounded recovery, not protection against power loss or SIGKILL of every guardian; inspect status after a host/guest crash.
- Logs, JUnit, passing/failed status, executable identity and allowlisted guest result/config/shell evidence are fetched automatically to `build/vm-oracle/<id>/report/`. Framebuffer PNGs are under `visual/<case>/attempt-*/`. Reruns retain earlier attempts. No cache or authentication-cookie trees are collected.

## Failure decisions

| Failure | Action |
| --- | --- |
| Approval review / usage limit | Stop; report the blocked action and reason. Do not try another access path. |
| SSH / host key / VM unavailable | Inspect `transport.log`. Do not recreate the VM or change NAT/login. |
| BUSY | Use Status for the existing run; wait or explicitly Cancel its verified owner. |
| Hash / unsafe archive / changed payload | Stop; package a new run after fixing the input. Never disable verification. |
| Build / CTest failure | Read the stage log, fix the cause, then one new Test. No repeated clean builds. |
| Capture/acknowledgement failure | Runner cancels its guardian and preserves diagnostics. Correct the cause, rerun Visual on the same tested run. |
| Missing display / failed service restoration | Inspect status and the user journal. Recover only when no test owns the locks. No duplicate manual Isolde launch. |
| Less than 2 GiB after automatic preflight | Inspect the cleanup receipt and storage failure. Safe cleanup already ran; no blind retries or manual deletion of evidence/profiles/releases. |

## Machine and limits

VM `Isolde-Test`; VirtualBox `C:/Program Files/Oracle/VirtualBox`; guest Ubuntu 26.04.1 LTS, `isolde-dev@127.0.0.1:2222`. Key location `C:/Users/kketc/VirtualBox VMs/Isolde-Test/automation/id_ed25519_automation` (never print its contents). Normal service `systemctl --user isolde.service`; release links under `~/.local/share/isolde-appliance/{current,previous}`. These links are observations, never fixed release IDs.

Keep the normal graphical session logged in. Harnesses import only DISPLAY/XAUTHORITY/XDG_RUNTIME_DIR/DBUS_SESSION_BUS_ADDRESS from the user service manager; do not hard-code a display/UID or export offscreen settings into the service. If stopped, use `tools/start-isolde-vm-visible.ps1`; do not restore snapshots, alter autologin or bypass a lock/password prompt. Physical HDMI/CEC/audio/latency and a disconnected cold-boot Generator Test remain separate gates.

Low-level scripts are implementation details. `run-visual-acceptance.ps1` requires the runner's guardian; direct unguarded launches are retired. Historical evidence remains in [the October 19 record](vm-update-2026-10-19.md) and task execution records; old `/tmp/isolde-*` trees are never a current-source default.

### Reviewed Guide fixture inputs

`Visual -RunId <passing-id> -Cases guide-lifecycle -GuideReplayPlan tools/guide-lifecycle-inputs.json` replays only bounded remote keys for the disposable Guide fixture. Source/authoring uses `guide-features` and `tools/guide-features-inputs.json`. Plans are schema-validated, copied to evidence and limited to 64 phases / 20 remote keys per phase. Captures retain pending review; replay never awards acceptance. Unknown phases remain attended. See `docs/t11-runtime-2026-10-29.md` for current evidence and diagnosed failures.

`guide-reopen` with `tools/guide-reopen-inputs.json` is a distinct bounded actual-cold-start check, using a separately hash-verified helper with the unchanged passing app. It reports no twelve-hour claim and currently inherits normal networking. It cannot satisfy disconnected acceptance. `Status -GuideDiagnostics` adds bounded readonly disposable-fixture slot comparison and native-sandbox probes; normal Status stays small.

`guide-features` likewise uploads the current `tools/guide_features.py` under its SHA-256 identity and verifies that identity before loading it with the frozen TV harness/dependencies and passing application payload. It never edits a frozen source tree or changes payload hashes. The run records `acceptanceHelperSha256`; the helper result/capture contract remains mandatory. This permits cause-directed feature-harness diagnosis without another application full suite.

### Bounded Studio visual corrections

`Test -Suite studio_visuals -ReuseFullRun <original-full-run>` permits only existing `PageStudioView.qml` and `CommandCenter.qml`, their artwork/shell regressions and named harness/docs files. It runs Studio quick/artwork/shell plus Home and Studio navigation registrations. The full-origin manifest and payload must match; C++/configuration/other QML changes are refused. This covers the observed composite-heading annotation and Search fallback-symbol overflow without repeating an applicable full suite. `Visual -Cases studio-search -Installed` uses a fresh private profile, the frozen socket-denial fixture, actual remote keyboard query, Search/recovery captures and a strict input/network receipt. It does not replay template/style/export/version workflows that are unchanged by a Search icon correction.

Low-space fallback: if verified artifact/runner-cache cleanup still leaves less than2GiB, routine Test may unlink obsolete generated ELF test executables declared by the native incremental build's CMake target list. It preserves the requested suite's executable targets, active processes, all compiled objects, application payloads, profiles and evidence. Redirected/mounted/unknown targets fail safely. Ninja relinks missing test programs when a future suite needs them. `guest-linked-tests-cleanup.json` records the removed guest paths; no host cleanup is performed.

When verifying an older tested or installed Lounge/Pastel candidate while the working folder has newer menu/helper changes, use `Visual -RunId <id> -Installed -Cases lounge,pastel -FrozenReferenceHelpers`. This selects the source-verified helpers frozen with that candidate instead of current working-folder overlays. It is restricted to those reference cases; the existing payload/source/hash checks still apply. Keep failed mismatched-helper attempts as evidence.

Low-space fallback now consolidates identical synthetic clips and immutable
verified payload artifacts through hardlinks. It preserves every path and byte,
retains the three newest synthetic fixtures, checks hashes/modes/ownership and
same-filesystem boundaries, and refuses redirected/mounted/changed candidates.
Receipts are guest-fixture-media-dedup.json and guest-payload-dedup.json. Logical
dedup byte totals are not physical reclaimed-space claims; use freeBefore/After.
All runner self-tests must pass before maintenance. The2GiB Test floor remains.
Storage inventory reports newest candidates first, plus the named synthetic
media/runtime-bundle and Kodi cache roots; it does not traverse entire profiles.
