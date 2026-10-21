# Oracle VM testing — use the runner

The VM is the existing **Isolde-Test** developer machine. Preserve Household, Kodi, installed releases and the graphical login. Routine testing goes through `tools/vm-test.ps1` in **PowerShell 7**. Do not assemble ad hoc SSH/build/capture loops.

## Commands

From the repository root:

```powershell
# Optional: connection, tools, disk and service check; changes no release/profile.
./tools/vm-test.ps1 Check

# Normal development gate. Prints RUN_ID and EVIDENCE immediately.
./tools/vm-test.ps1 Test

# During implementation, run only the relevant native suite.
./tools/vm-test.ps1 Test -Suite component_contracts
# Fast Studio: studio_quick, studio_unit, studio_behavior, or studio (all three).
./tools/vm-test.ps1 Test -Suite studio
# Other choices: home_component_navigation, kodi_media_client. Default: all.

# Use the exact RUN_ID printed by the passing full Test.
./tools/vm-test.ps1 Visual -RunId <id>
# Default: Studio, lounge and pastel sequentially, using the staged tested build.
# To rerun one graphical case without rebuilding:
./tools/vm-test.ps1 Visual -RunId <id> -Cases studio

# Only when deployment is requested: activate the already tested payload and restart.
./tools/vm-test.ps1 Deploy -RunId <id>
./tools/vm-test.ps1 Visual -RunId <id> -Installed

# Inspect a running/failed run. Never start a second build to get progress.
./tools/vm-test.ps1 Status -RunId <id>
# Explicitly cancel ONLY this runner's verified process; it restores the service.
./tools/vm-test.ps1 Cancel -RunId <id>
# After a crashed guardian, restore its recorded service state if no test owns either lock.
./tools/vm-test.ps1 Recover -RunId <id>
```

The `pages` case is opt-in (`-Cases pages`): it exercises real Kodi in a fresh UUID native profile and synthetic media under `~/Videos/Isolde-Acceptance/`. It is not preservation testing of an existing Kodi installation. Do not point it at Household or `~/.kodi`.

For frequent code/test iterations, use [the focused Studio suite](studio-testing.md). These offscreen tests reuse the cached build without installing a release or restarting the normal app. Run full native/graphical acceptance at integration checkpoints; deployment remains explicit and occasional. No host toolchain installation is needed.

## Agent execution rules

1. Finish a meaningful edit batch before Test. Do not cancel a healthy build to add optional polish. If a tool returns a running session, poll **that session**; do not launch another SSH build.
2. Track required new source/test files before Test. Packaging includes tracked **working-tree bytes**, including uncommitted edits, and excludes unrelated untracked files. No reset/stash/clean is part of testing.
3. Treat the first `FAIL` as a stop. Read the named stage log and `host-failure.json`/`report/status.json`, fix that cause, then run once. No blind retries, new run-number loops, package installation, VM resets or lock deletion.
4. Targeted Test is quick feedback, not completion: it cannot stage, deploy or run visual acceptance. Full Test discovers the actual CTest count and refuses failed/skipped/zero-test reports.
5. Visual uses the exact run's staged executable by default, so it cannot accidentally test an older installed release. `-Installed` requires that run's explicit successful Deploy and verifies its selected release.
6. View **every** captured screenshot and compare both recreation/reset pairs with the fixture references. `captures.json` deliberately says `visualReview: pending`. Automated pixel/blank checks and capture acknowledgements are not design acceptance. T24 still owns genuine user usability/cutover.
7. Commit only source and concise evidence records. Do not stage `build/`, generated caches, PulseAudio cookies, browser profiles or binary artwork.

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
| Less than 2 GiB free | Stop and review disk use. The runner never deletes old evidence, profiles or releases automatically. |

## Machine and limits

VM `Isolde-Test`; VirtualBox `C:/Program Files/Oracle/VirtualBox`; guest Ubuntu 26.04.1 LTS, `isolde-dev@127.0.0.1:2222`. Key location `C:/Users/kketc/VirtualBox VMs/Isolde-Test/automation/id_ed25519_automation` (never print its contents). Normal service `systemctl --user isolde.service`; release links under `~/.local/share/isolde-appliance/{current,previous}`. These links are observations, never fixed release IDs.

Keep the normal graphical session logged in. Harnesses import only DISPLAY/XAUTHORITY/XDG_RUNTIME_DIR/DBUS_SESSION_BUS_ADDRESS from the user service manager; do not hard-code a display/UID or export offscreen settings into the service. If stopped, use `tools/start-isolde-vm-visible.ps1`; do not restore snapshots, alter autologin or bypass a lock/password prompt. Physical HDMI/CEC/audio/latency and a disconnected cold-boot Generator Test remain separate gates.

Low-level scripts are implementation details. `run-visual-acceptance.ps1` requires the runner's guardian; direct unguarded launches are retired. Historical evidence remains in [the October 19 record](vm-update-2026-10-19.md) and task execution records; old `/tmp/isolde-*` trees are never a current-source default.
