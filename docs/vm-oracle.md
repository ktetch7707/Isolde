# Oracle VirtualBox test runbook

Verified by deployment, restart, full CTest and both visual fixture runs on 2026-10-19; see the linked execution record. This is the existing developer oracle, not a disposable VM or a hardware certification environment. Preserve its normal Household profile, Kodi data, service and release links. Read this before any guest test. Run only one graphical acceptance at a time; all three Python harnesses share `/tmp/isolde-home-components-acceptance.lock`. Do not delete that lock to bypass an active test.

## Known machine and preflight

| Item | Verified value |
| --- | --- |
| Windows VM name | `Isolde-Test` |
| VirtualBox directory | `C:/Program Files/Oracle/VirtualBox` |
| Guest | `isolde-test`, Ubuntu 26.04.1 LTS |
| SSH | `isolde-dev@127.0.0.1`, TCP 2222 forwarded to guest 22, loopback only |
| Private key location | `C:/Users/kketc/VirtualBox VMs/Isolde-Test/automation/id_ed25519_automation` |
| Normal executable | `/home/isolde-dev/.local/share/isolde-appliance/current/bin/isolde` |
| Normal service | `systemctl --user`, `isolde.service` |
| Observed baseline | updated release `deacfc0b6d02d96d759303da`; previous `161ea2e866cae3d1f982416e`; active/running, NRestarts=0 |

The release is an observation, not a permanently approved baseline. Record it afresh for each run. Never print/copy the key contents. In PowerShell:

```powershell
$vmTool = 'C:/Program Files/Oracle/VirtualBox/VBoxManage.exe'
$vmKey = 'C:/Users/kketc/VirtualBox VMs/Isolde-Test/automation/id_ed25519_automation'
$vmSsh = @('-i', $vmKey, '-p', '2222', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=10', 'isolde-dev@127.0.0.1')
& $vmTool showvminfo Isolde-Test --machinereadable
ssh.exe @vmSsh 'hostname; systemctl --user show isolde.service -p ActiveState -p SubState -p NRestarts; readlink -f ~/.local/share/isolde-appliance/current; readlink -f ~/.local/share/isolde-appliance/previous'
ssh.exe @vmSsh 'systemctl --user show-environment; command -v cmake ninja ctest python3'
```

Do not paste the entire service environment into reports; retain only display/runtime entries. Verify SSH's host key normally; never add `StrictHostKeyChecking=no`. A sandbox permission error is not a broken VM: request the permitted local VM/SSH tool access before changing anything. If stopped, use `tools/start-isolde-vm-visible.ps1` for an intentionally visible test display. Do not create another machine, change NAT, reset, power off, restore snapshots, alter login/compositor, or install packages to fix a failed test without diagnosing and obtaining authorization for that change. A named snapshot is not evidence of a current backup.

## Freeze the source being tested

Use `./tools/package-vm-source.ps1 -OutputDirectory <NEW-absolute-host-directory>` from the repository root. It packages tracked working-tree bytes (including staged/unstaged edits), records HEAD/status/diff, produces `files.sha256` and `archive.sha256`, and refuses an existing output directory or missing tracked file. Review untracked files first: required new code/assets must be explicitly tracked; scratch files stay excluded. Stop other writers while packaging. Transfer archive and manifests; verify `sha256sum -c archive.sha256` before extraction and `sha256sum -c ../files.sha256` inside the extracted source. A failed check means stop and take a new snapshot.

For an executed example with the failures and corrections retained, see [the October 19 update record](vm-update-2026-10-19.md).

Record Windows `git rev-parse HEAD`, `git status --short` and the tracked diff. A commit ID alone does not identify a dirty tree. Preserve existing edits; do not reset/stash/clean them. Transfer the chosen source to a NEW guest directory with a unique run ID. Exclude `.git`, build directories, temporary dot-files, credentials and unrelated untracked files. Use an explicit reviewed file list/archive and retain its SHA-256; include required fixture artwork. A `git archive HEAD` omits uncommitted changes and cannot test them. Do not synchronize over an old source tree or the installed payload. Record archive hash, guest source path, diff and build path in the evidence manifest.

`/tmp/isolde-order-vsDgGY` is a stale source tree from earlier runs. It is NOT the current source. The PowerShell screenshot wrapper now requires `-SourceRoot` explicitly; its unsafe historical default was removed. Guest `/tmp` evidence can disappear on reboot; copy results/logs/screenshots to a durable per-run location before closing a gate.

## Linux automated oracle (does not deploy)

In a guest Bash shell, substitute the newly transferred source directory. Use a fresh build directory; never reuse a Windows CMake cache:

```bash
set -euo pipefail
source_dir=/tmp/isolde-YOUR_RUN/source
evidence_dir=$(mktemp -d /tmp/isolde-oracle-XXXXXX)
cd "$source_dir"
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug 2>&1 | tee "$evidence_dir/configure.log"
cmake --build build --parallel 2 2>&1 | tee "$evidence_dir/build.log"
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ctest --test-dir build --output-on-failure 2>&1 | tee "$evidence_dir/ctest.log"
cp build/Testing/Temporary/LastTest.log "$evidence_dir/LastTest.log"
python3 tools/verify-t03-search.py 2>&1 | tee "$evidence_dir/t03-static.log"
```

The current CMake file registers 14 Linux suites (including `capture_protocol`), not a permanently fixed 13. Record CTest's actual discovery/count, failures, skips and source identity. The static T03 script checks strings/patterns; it does not prove runtime cancellation, Web OFF network silence, or real playback. Offscreen success does not establish installed UI appearance, GPU effects, capture/audio/CEC, or cold-boot offline readiness.

## Choose the executable deliberately

For staged acceptance without changing `current`, use CMake install into a new run-specific prefix after successful tests:

```bash
cmake --install build --prefix /tmp/isolde-YOUR_RUN/payload
python3 tools/home-components-acceptance.py /tmp/isolde-YOUR_RUN/payload/bin/isolde
```

This is staged acceptance, not installed-release acceptance. Keep the complete payload (native host, config, bridge, support files); do not copy only the Isolde binary into the installed release. Python reference and TV harnesses likewise take an explicit executable. The PowerShell wrapper instead hardcodes the normal `current/bin/isolde`, so it cannot prove an undeployed source build passes.

Actual deployment is a separate, intentional operation. `python3 tools/install-appliance.py install --build-dir <linux-build-dir>` builds/tests/stages and selects a content-addressed release; `--startup` also rewrites/enables startup configuration. It changes `current`/`previous` but does not restart the running UI. Do not deploy merely to obtain unit-test results. Follow `docs/install.md` when deployment is in scope; record old/new links and explicitly restart the user service to test the selected release. Rollback switches binaries, not profile data, packages or OS state.

## Graphical acceptance and capture handshake

Keep the guest logged into its normal graphical session. Harnesses import `DISPLAY`, `XAUTHORITY`, `XDG_RUNTIME_DIR`, `DBUS_SESSION_BUS_ADDRESS` from the user service manager and use X11 (`xcb`); do not hardcode `:0`, UID 1000 or the observed temporary Xauthority filename. Do not export offscreen/software variables into the normal service. If a display is unavailable, inspect graphical login and the allowlisted environment before testing.

For the installed executable, from PowerShell 7 (the wrapper uses `ProcessStartInfo.ArgumentList`):

```powershell
./tools/run-visual-acceptance.ps1 -Case lounge -SourceRoot /tmp/isolde-YOUR_RUN/source -OutputDirectory ./evidence/YOUR_RUN/lounge
./tools/run-visual-acceptance.ps1 -Case pastel -SourceRoot /tmp/isolde-YOUR_RUN/source -OutputDirectory ./evidence/YOUR_RUN/pastel
./tools/run-visual-acceptance.ps1 -Case pages -SourceRoot /tmp/isolde-YOUR_RUN/source -OutputDirectory ./evidence/YOUR_RUN/pages
```

Run sequentially. The wrapper runs the appropriate guest Python script, waits for `PHASE_READY /tmp/isolde-...`, waits two seconds, captures the actual framebuffer using `VBoxManage controlvm Isolde-Test screenshotpng <host-path>`, checks the file, then acknowledges exactly that phase using `touch <guest-evidence>/<phase>-captured`. Reference phases include CUSTOM and DEFAULT; page phases include PAGE, TAB, TOP, STANDALONE and DIRECT. Never acknowledge before capturing or fabricate an image from QML/rendered source. Harness waits are bounded. `--inspect` is a failure-inspection pause, not an automatic visual verdict. A reference run needs both recreation and reset screenshots and explicit review against its reference README; exit zero alone is insufficient.

The wrapper saves host screenshots/stdout, but does NOT retrieve the guest `result.json`, shell logs or fixture config. Copy the exact printed evidence directory with `scp.exe -i $vmKey -P 2222 -r 'isolde-dev@127.0.0.1:/tmp/EXACT_PRINTED_DIRECTORY' ./evidence/YOUR_RUN/` after the run. Retain the source/executable identity with it. Do not commit binary artwork or build artifacts.

## Isolation boundaries and mandatory restoration

- `home-reference-acceptance.py` and `home-components-acceptance.py` use disposable config/data/cache roots. They temporarily stop an initially active normal service and restore it in `finally`. Never point their data at Household.
- `tv-library-acceptance.py` isolates config/cache but intentionally keeps the standard data root so the native sandbox can use a fresh UUID profile. It creates `~/.local/share/Isolde/native-profiles/<fresh-uuid>/kodi/...` and synthetic media under `~/Videos/Isolde-Acceptance/Isolde-tv-test-<uuid>`. It imports through Kodi's native UI, hashes fixture files, and uses private bridge calls. This is not a fully disposable XDG data root and not preservation testing of an existing non-default Kodi setup. Keep the printed UUID and paths; never substitute a real profile or media folder.
- Do not run the importer/rollback against `~/.kodi` as an oracle smoke test. See the T04 blockers in `planning-audit.md`. Existing reference/TV harnesses do not prove import rollback safe.
- Harness `finally` restoration is best effort, not protection against kill, lost session or host failure. Do not use broad `pkill`, kill all Kodi/Isolde processes, or force shutdown to escape a timeout. Inspect the exact test PID, lock owner, evidence and logs; terminate only owned test processes when necessary.

After every graphical run (including failure), check the previously recorded service state, release links and restart count. For the normal initially active machine:

```powershell
ssh.exe @vmSsh 'systemctl --user show isolde.service -p ActiveState -p SubState -p NRestarts; readlink -f ~/.local/share/isolde-appliance/current; journalctl --user -u isolde.service -n 40 --no-pager'
```

If the harness left the previously active service stopped, confirm its test child has exited, then `systemctl --user start isolde.service`. Do not start a duplicate executable. If it fails, retain journal/recovery logs and diagnose before `reset-failed` or rollback. Capture and inspect the normal Household Home and focus after restoration; active/running alone is insufficient. Restore an initially inactive service to inactive. Record any intentionally changed release separately. Never delete profiles, evidence, guest source trees or restore snapshots as routine cleanup.

## Evidence required to close a gate

Keep exact source/archive hash and dirty diff, executable/release path, build/test commands and exit codes, test counts, guest result JSON/config/logs, both fixture screenshot pairs, human visual verdict with limitations, fixture hashes and service restoration evidence. Offline Generator Test needs a separately recorded disconnected cold boot; this runbook's online SSH preflight is not that test. Physical RS-232, capture, audio sync, latency, hotplug and CEC still require target hardware. A VM pass never closes those gates.

Display troubleshooting verified during this run: a black framebuffer became GDM after a Shift press; the already-running Isolde desktop was on a different VT. Inspect `loginctl list-sessions` and each session's Name/Type/Class/Active/VTNr/LockedHint. Use the normal console Ctrl+Alt+F-key for the observed existing desktop VT when unlocked; do not change autologin or bypass a lock/password prompt. Over SSH, `loginctl activate` can require interactive authorization even for an existing session. Check its own exit code immediately. Source paths, VTs, session IDs and Xauthority filenames are observations, not constants.
