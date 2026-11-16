# T31 — Source-tied software risk baseline (T31.6)

Version: 1.0
Candidate: frozen at T31.2 hash freeze
Isolde version: 0.0.1.2

This document is the T31.6 source-tied software risk baseline. It records actual
tool/check availability and warnings, a boundary risk-to-test map, meaningful
failure-injection regressions, agent critical-boundary diff/evidence review,
reproducible contracts, and unresolved-defect disposition. No independent
reviewer, invented coverage score, paid analyzer, or host tool installation is
required. Final-candidate applicability remains required; Q0-Q5 rollout is
optional infrastructure.

## 1. Tool/check availability and warnings

| Check | Tool | Guest availability | Status | Warnings |
|-------|------|-------------------|--------|----------|
| C++ formatter | clang-format | pinned via Qt toolchain | available | version pin required |
| C++ defects | clang-tidy | guest toolchain | available | Qt-aware config needed |
| C++ defects | Cppcheck | guest toolchain | available | open-source edition |
| QML lint | qmllint | Qt 6 installation | available | real import paths needed |
| QML format | qmlformat | Qt 6 installation | available | qualify against fixtures |
| Python lint/format | Ruff | guest Python | available | pin version/rules |
| PowerShell analysis | PSScriptAnalyzer | guest PowerShell 7 | available | run only where present |
| Shell check | ShellCheck | guest | available | inventory standalone JS |
| C++ coverage | gcov/gcovr or LLVM | GCC 15.2 / Clang | available | select compatible path |
| CTest runner | CTest | guest Ninja build | available | --stop-on-failure |
| VM test runner | `tools/vm-test.ps1` | guest PowerShell 7 | available | bounded, self-test first |

No paid analyzer, hosted CI, or public-runner quota is required for S1. Host
tool installation is not permitted; all checks run in the guest.
## 2. Boundary risk-to-test map

| Boundary | Risk | Test | Fixture/substitution | Coverage |
|----------|------|------|---------------------|----------|
| Kodi media library | scan failure, paging injection | `kodi_playback`, `kodi_local_bridge` | real Kodi 21.3 in VM | partial |
| Layout save/rollback | atomic write failure, data loss | `component_contracts`, `tst_home_layout` | temp dir + rollback | full |
| Profile isolation | cross-profile read/write | `profile_browser_isolation` | bubblewrap + dbus-run-session | full |
| Store install/update | signed metadata, invalid signature | `store-*` 62/62 + boundary 6 | curated manifests | full |
| Offline behavior | network denial, OFF mode | provider HTTP adapter tests | loopback OFF responses | full |
| Recovery | invalid signature, missing backend | `appliance_recovery` | temp releases + lock | full |
| Display startup | early Qt before Wayland | `wait-for-display` | socket existence check | full |
## 3. Failure-injection regressions

| Regression | Injected fault | Expected behavior | Test | Status |
|------------|---------------|-------------------|------|--------|
| Write failure mid-install | incomplete payload | `RuntimeError`, current unchanged | `test_bad_payload_does_not_replace_current` | Linux guest |
| Existing directory not overwritten | `current/` already exists | `RuntimeError`, directory intact | `test_existing_directory_not_overwritten` | Linux guest |
| External symlink rejected | `current` → payload symlink | `RuntimeError` | `test_external_link_rejected` | Linux guest |
| Install lock contention | `.install.lock` held | `BlockingIOError` | `test_install_lock_blocks_recovery_mutations` | Linux guest |
| No previous release | `previous` missing | `RuntimeError` on rollback | `test_no_previous_is_not_available` | Linux guest |
| Running app blocks recovery | `ActiveState` = active | `RuntimeError`, no switch | `test_cannot_switch_under_running_app` | Linux guest |
| Display not socket | `WAYLAND_DISPLAY` = regular file | `display_ready` = false | `test_display_waits_for_socket_not_just_environment` | Linux guest |
| Environment import sanitization | `LD_PRELOAD` in display env | only display fields kept | `test_display_import_only_accepts_display_fields` | full |
## 4. Agent critical-boundary diff/evidence review

Critical boundaries reviewed for the frozen candidate:

1. **Install payload atomicity** — `install_payload` validates all required binaries,
   rejects symlinks in payload, computes SHA-256 digest before linking, rolls back
   on bad payload. Reviewed: `tools/install-appliance.py` lines 72–110.

2. **Release link safety** — `checked_release` requires symlink to `releases/`
   with `bin/isolde` present; refuses non-symlink `current`/`previous`. Reviewed:
   lines 60–69.

3. **Recovery lock** — `recover` takes `fcntl.flock` exclusive lock on
   `.install.lock`, refuses if Isolde is running. Reviewed:
   `tools/recovery-controller.py` lines 27–40.

4. **Display readiness** — `wait-for-display` waits up to 30s for Wayland/X11
   socket, not just environment variables. Reviewed: `tools/wait-for-display.py`
   lines 37–52.

5. **Profile isolation** — bubblewrap sandbox + dbus-run-session; native apps
   refused if sandbox unavailable. Reviewed: `docs/install.md` lines 93–94.

6. **Store signature verification** — signed metadata, verified payload hashes,
   explicit capability consent required before remote repository enablement.
   Reviewed: `docs/packages.md` lines 45–46.

## 5. Reproducible contracts

| Contract | Source | Reproducible | Evidence |
|----------|--------|--------------|----------|
| Atomic payload install | `tools/install-appliance.py` | yes (temp + rename) | `test_appliance_install.py` |
| Offline rollback | `tools/install-appliance.py` + `Recovery.qml` | yes | `test_recovery.py` |
| Display wait | `tools/wait-for-display.py` | yes | `test_appliance_install.py` |
| Recovery UI | `src/recovery/Recovery.qml` | yes (QML offscreen) | VM visual |
| Store boundary | curated manifests + validation | yes | `store-boundary-o` 6 tests |

## 6. Unresolved-defect disposition

| Defect | Severity | Boundary | Disposition | Blocks T31? |
|--------|----------|----------|-------------|-------------|
| BUG-125 Studio frame stall | retained, not claimed | Studio render | diagnostic only, no fix claimed | no |
| BUG-176 Chrome first-focus | after 23 passes | browser remote | regression open | no |
| BUG-179 music scan paging | generic AudioLibrary read dispatch | Kodi scan | fix branch open | no |
| Live SmartTube decoding | loading spinner only | Android playback | U01 handoff, no playback claim | no |
| T04 poster/playlist/import | complete closure open | Kodi import | held for combined verification | yes (T04 at 66%) |
| T09/T10/T12/T14/T30 | below 100% | multiple | active tasks incomplete | yes |

T31 cannot reach 100% until all active tasks are at 100%. The defects above that
block incomplete active tasks must be resolved first.

## 7. Q0-Q5 rollout (optional infrastructure)

Quality rollout stages Q0–Q5 are optional for S1. If adopted later, they extend
`tools/vm-test.ps1` with analyzer stages and coverage reports; they do not
replace the existing CTest + VM acceptance gates.

| Startup rate limit | 3 starts in 1 minute | `StartLimitBurst=3`, pause | `test_startup_is_bounded` | full |
| Enable failure not success | `systemctl` call fails | `CalledProcessError` propagated | `test_enablement_failure_is_not_reported_as_success` | full |
| Cancellation mid-scan | provider cancellation | scan stops, no false entitlement | `t14_discovery_subtitles` OFF/cancellation | partial |

| Source detection | dead tile when no handler | package manifest validation | no handler → not shown | full |
| Web kiosk | navigation outside allowedOrigins | `allowedOrigins` enforcement | packaged qrc URLs | full |
| Android/Web removal | cleanup completeness | Android provisioning tests | signed APK fixtures | partial |

