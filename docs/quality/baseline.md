# Isolde Code-Quality Baseline — Q0 (2026-10-25)

Single, evidence-bounded baseline for the quality improvement strategy in
`docs/code-quality-strategy-2026-10-25.md`. This slice is **measurement,
inventory, and risk scoping only**: no application code was changed, no file was
reformatted, no tool was installed, no feature work was touched, and no commits
were made. It is an *honest baseline*, not a pre-emptive fix.

---

## 1. Scope and intent

- Purpose: record what exists, what is measurable, and what is **not** known yet.
- Distinguish **measured findings** (observed during this run) from **review
  candidates** (static inspection signals requiring human judgement).
- Produce two consumables:
  - `docs/quality/baseline.md` (this file)
  - `docs/quality/debt-register.md`
- Recommend the smallest Q1 implementation slice (entry files, tools, changes,
  acceptance checks) in the final report — **do not implement it**.

---

## 2. Source and run identities

| Identity | Value |
|---|---|
| Repository root | `C:\Users\kketc\Documents\ChatGPT\Isolde` |
| Branch | `agent-backlog-progress` |
| HEAD commit | `a6637e702160abce7b3805bce63a6897c215b5ac` |
| HEAD subject | `T29: Add Online Store section to SettingsView.qml` |
| Working tree state | 29 tracked files modified (staged + unstaged); 2 untracked directories; 12 untracked files (see §2.1) |
| Runner (host) | `tools/vm-test.ps1` — PowerShell 5.1 on the host (`COMSPEC=C:\WINDOWS\system32\cmd.exe`) |
| Runner (guest) | `Isolde-Test` (Ubuntu 26.04.1 LTS), `isolde-dev@127.0.0.1:2222`, key `C:/Users/kketc/VirtualBox VMs/Isolde-Test/automation/id_ed25519_automation` |
| Guest service | `systemctl --user isolde.service` (not inspected as part of Q0) |

### 2.1 Working-tree changes preserved for the baseline

**Modified (29):**

`AGENTS.md`, `CMakeLists.txt`, `config/catalog.json`, `docs/agent-backlog.md`,
`docs/components.md`, `docs/design/page-studio/feature-map.md`,
`docs/vm-oracle.md`, `qml/AppStoreView.qml`, `qml/Main.qml`,
`qml/SettingsView.qml`, `qml/WebKioskView.qml`, `src/app/AppCatalog.cpp`,
`src/app/AppCatalog.h`, `src/app/StoreProvider.h`, `src/home/PageData.h`,
`src/input/InputManager.cpp`, `src/input/InputManager.h`, `src/main.cpp`,
`src/sessions/DesktopSession.cpp`, `src/sessions/NativeAppHost.cpp`,
`src/sessions/NativeSandbox.cpp`, `tests/studio/tst_selection.qml`,
`tests/test_android_provisioning.py`, `tests/tst_isolde.cpp`,
`tools/android-session.py`, `tools/provision-android.py`,
`tools/run-visual-acceptance.ps1`, `tools/vm-oracle.py`,

## 3. Languages, build system and test stack

| Layer | Reality in the repo |
|---|---|
| Language | C++17 (`CMAKE_CXX_STANDARD 17`, `LANGUAGES CXX`), Qt 6, QML, C++/Qt, Python 3, PowerShell, JavaScript/web assets |
| CMake | `cmake_minimum_required(VERSION 3.21)`, `project(Isolde VERSION 0.0.1.2 LANGUAGES CXX)`, `find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network DBus Qml Quick Test WebEngineQuick WebChannel Multimedia SerialPort)` |
| Build | CMake + (guest) Ninja 1.13.2 |
| Testing | CTest + Qt Test (`qt_add_executable` / `target_link_libraries(Qt6::Test)`) + Qt Quick Test (QML `.qml`) + Python scripts wired via `add_test(NAME ... COMMAND python3 ...)` |
| Guest tooling | `git 2.53.0`, `cmake 4.2.3`, `g++ 15.2.0`, `gcc 15.2.0`, `ninja 1.13.2`, `python3 3.14.4`, `gcov 15.2.0`, `gdb 17.1` |

---

## 4. Formatting / static-analysis configuration inventory

**Findings (measured):**

| Tool / config | Present? | Evidence |
|---|---|---|
| `.clang-format` / `clang-format-config.json` | **Absent** | repo-wide search → zero matches |
| `clang-tidy.yaml` / `clang-tidy.cfg` | **Absent** | repo-wide search → zero matches |
| `.cppcheck.ini` / `cppcheck` config | **Absent** | repo-wide search → zero matches |
| `ruff.toml` / `pyproject.toml` (tool.ruff) | **Absent** | repo-wide search → zero matches |
## 5. Test suite inventory (measured file counts)

### 5.1 C++/Qt-Test native (`tests/*.cpp`, 20 files)

`tst.cpp`, `tst_audio.cpp`, `tst_capture.cpp`,
`tst_desktop_mode.cpp`, `tst_home_navigation.cpp`, `tst_isolde.cpp`,
`tst_kodi_media.cpp`, `tst_native_sandbox.cpp`, `tst_vpn.cpp`,
`tst_destinations.qml`, `tst_editor.qml`, `tst_gestures.qml`,
`tst_grid.qml`, `tst_grid_controls.qml`, `tst_ordered.qml`,
`tst_overlays.qml`, `tst_policies.qml`, `tst_selection.qml`,
`ProfileBrowserTest.qml`, `quicktest.cpp`

(Plus `tests/studio/tst_selection.qml` — the Qt Quick Test suite.)

### 5.2 Python test scripts (11 files)

`test_android_provisioning.py`, `test_appliance_install.py`,
`test_health.py`, `test_default_apps.py`,
`test_kodi_artwork.py`, `test_kodi_bridge.py`,
`test_kodi_back.py`, `test_recovery.py`, `test_vpn.py`,
`test_vm_oracle.py`, plus `fake-audio-player.py`, `fake-mixer.py`.

### 5.3 QML / integration test coverage

- Qt Quick Test (`tst_*.qml`): destinations, editor, gestures, grid,
  grid_controls, ordered, overlays, policies, selection, ProfileBrowserTest.
- Studio QML are **not** registered in the CTest contract as of Q0 (they
  exist under `tests/studio/` as artifacts; see `docs/studio-testing.md`).

### 5.4 Coverage tooling status (measured)

- **Present:** `gcov 15.2.0` (GCC coverage runtime, from `gcc 15.2.0`).
- **Absent:** `lcov`, `ccache`, `valgrind`, `qmllint`, `qmlformat`, `npm`,
  `node`, `clang-tidy`, `cppcheck`, `pwsh`.
- **Consequence:** no coverage percentage, no unified report, no branch
  coverage, no QML coverage has been **measured** for this baseline. Coverage is
  reported as **unmeasured**. gcov may generate raw `.gcno`/`.gcda` files only if
  a toolchain/CMake flags provide them.
| `.editorconfig` | **Absent** | repo-wide search → zero matches |
| `.github` workflows | **Absent** | directory listing → no `.github` |
| CMake coverage flags (`--coverage`, gcov) | **Not configured** (see §6) | no coverage CMake option present |

The strategy document's scoped inspection is confirmed: **there is no
repository-wide formatter/analyzer config and no CI workflow directory.** This is
an *inventory result*, not a judgement of code health.

`tools/vm-test.ps1`

**Untracked (14 entries — 2 dirs, 12 files):**

`docs/code-quality-strategy-2026-10-25.md`, `docs/first-product-software-scope.md`,
`docs/games-page/`, `evidence/`,
`fix_settings.py`, `list_tests.py`, `out.txt`, `output.txt`,
`show_tests.py`, `src/input/WebRemoteScript.h`, `src/input/web-remote.js`,
`src/sessions/ChromiumRemotePipe.h`, `test.py`, `tst_isolde.txt`
## 7. Full native test gate — execution result

**Command attempted:** `./tools/vm-test.ps1 Test` (full native CTest gate,
`--suite` default `all` run from the repository root.

**Result: NOT EXECUTED — first named failure.**

Exact failure (host, PowerShell 5.1):

```
powershell.exe : Use PowerShell 7 (pwsh), not Windows PowerShell.
...
+ ... or -lt 7) { throw 'Use PowerShell 7 (pwsh), not Windows PowerShell.' ...
+                 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo : OperationStopped: (Use PowerShell 7 (pwsh), not Windows PowerShell.:String) [], RuntimeException
    + FullyQualifiedErrorId : Use PowerShell 7 (pwsh), not Windows PowerShell.
```

Cause: the bounded runner requires **PowerShell 7** on the host; the host
currently provides **Windows PowerShell 5.1** (`$PSVersionTable.PSVersion` = 5.1,
`COMSPEC = C:\WINDOWS\system32\cmd.exe`). Executing the script under PowerShell
5.1 aborts at the PS7 guard before any guest operation. No installation or host
configuration was performed (held by explicit constraint).

**Run-state (to satisfy "if a run is already active, inspect instead of starting"):**

- Latest timestamped runner run directory in `build/vm-oracle/`:
  `20261024-183219-07e411` (10/24) — the most recent *Test-style* run.
## 8. Measured findings vs review candidates

### 8.1 Measured findings (this Q0 run)

- Repo-wide config/CI absence (confirmed, §4).
- 84 C++ files under `src/`; 61 QML files; 77 test files; 51 tool files.
- No formatter, analyzer, or coverage-aggregator tools installed on guest (§6).
- Runner cannot execute on host (§7).
- Guest skill set = cmake/g++/ninja/python3/gcov/gdb; **no** coverage report,
  **no** QML coverage, **no** static analysis baseline (§6, §5.4).

### 8.2 Review candidates (static inspection, **not** measured)

These are signals for Q1 review, not evidence of defects:

- **Source**: recent task slices (T13–T29) touch many files across
  `src`, `qml`, `tools` and `tests` in one working tree; mixed feature work
  (store, Android provisioning, VPN, settings, Studio refs) coexists without
  separate configuration hygiene conventions.
- **Tests**: no per-language coverage configuration; test-timeout and
## 9. Testing gaps

Gaps are stated as **gaps**, not as proven failures:

1. No coverage tooling/aggregation (gcov present, lcov absent) → no measured %
   for the baseline.
2. No C++ static analyzer (clang-tidy/cppcheck) on the guest.
3. No QML linter/formatter (qmllint/qmlformat) on the guest.
4. No Python linter/formatter pinned (ruff/black availability not probed; absent
   tooling is recorded as not installed).
5. Studio QML outside the CTest contract (not registered or behaviour counted).
6. Full native gate not executed (runner PS7 blocker; no evidence for pass/fail).
7. No `lcov`/`gcovr` to aggregate gcov data.
## 10. Maintainability and debt signals

Measured (structural), not measured (behavioural):

- **Formatter/analyzer** configuration absent → mechanical consistency depends
  on individual editors, not a pinned gate.
- **No coverage** → the strategy's initial ≥80% changed-C++/Python gate cannot
  be enforced or trended.
- **Runner prerequisite** (`pwsh`) is not installed on the host → the quality
  gate itself is not reproducibly invocable from the host.
- **Tooling mismatch**: strategy recommends clang-tidy, cppcheck,
  qmllint/qmlformat, Ruff, PSScriptAnalyzer; none are installed anywhere in
  the current VM image.
- **Co-located unrelated work** — 29 modified files across T13–T29 in one tree;
  risk of cross-feature contamination in any future baseline.
8. No `valgrind`/`gdb` → no measured memory-diagnostic baseline (absent tools).
9. Background/long-running graphical journeys are not covered by native tests.
## 11. Top ten risks (Q1 + beyond)

1. **Gate not invocable** — quality gate needs pwsh on host; any host without
   pwsh cannot run `tools/vm-test.ps1 Test`.
2. **Coverage unmeasured** — no baseline %; strategy gates (≥80% changed,
   ≥70% changed branch) cannot be enforced or trended.
3. **No static analysis** — C++/QML defect-detection defaults to review only.
4. **No QML linter** — presentation regressions are not machine-checked.
5. **Tools absent on guest** — recommended free toolchain is not installed;
   any future gate must install and pin tools first (isolated to the guest,
   not the host).
6. **Test plumbing drift** — Studio QML not in CTest contract; test
   registration counts ≠ behaviour counts (documented in the strategy).
7. **Co-located unrelated work** — 29 modified files across T13–T29 in one
   tree; risk of cross-feature contamination in any future baseline.
8. **Offscreen/SVG heap fragmentation** — no memory-diagnostic tooling
   (valgrind/gdb present but no baseline).
9. **Runner is not CI source of truth** — runner is host-driven and
   host-dependent; reproducible CI would require a hosted workflow.
10. **No evidence for pass/fail** — decisions today rest on inventory, not on
    measured gate results.
  offscreen-environment conventions (`QT_QPA_FORMAT=offscreen;
  QT_QUICK_BACKEND=software`) are present but not uniform; Studio QML
  live outside the CTest contract.
- **Infrastructure**: the bounded runner is documented as the single gate but
  itself depends on a host tool (`pwsh`) that is not guaranteed installed — a
  risk for the planned quality gate.
- **Documentation**: `docs/` is large and growing; the file- and section-level
  housekeeping (e.g. vs-code, changelog, baseline output) is not yet machine
  consistent.

No claim of a defect, no defect count, and no architecture score is made.
## 12. Evidence paths

| Evidence | Location |
|---|---|
| Strategy doc | `docs/code-quality-strategy-2026-10-25.md` (full read, 104 lines) |
| Backlog | `docs/agent-backlog.md` |
| VM oracle | `docs/vm-oracle.md` |
| VM runner | `tools/vm-test.ps1`, `tools/vm-process.ps1`, `tools/vm-oracle.py`,
  `tools/package-vm-source.ps1` |
| Guest tool versions | captured via SSH (`git/cmake/gcc/g++/ninja/python3/gcov/gdb` §6 |
| Runner failure | this Q0 run, exact error text in §7 |
| Worktree state | `git status --short` (29 modified / 14 untracked entries) |
| Repo file inventory | `gci` counts: src 84 C++, 61 QML, 77 tests, 51 tools |
| Coverage tooling | gcov present; lcov/ccache/valgrind absent |

None of the evidence requires the full Test gate to have run, by design:
Q0 establishes the baseline so a later Q1 gate can be compared against it.

---

*Report generated 2026-10-25. No application files were modified, reformatted,
refactored, installed, deployed, or serviced by this slice. Feature completion
percentages are unchanged.*
- All 10/25 `build/vm-oracle/` subdirectories are descriptive names
  (`feedback-*`, `home-*`, `vpn-*`, `android-*`, `default-apps-*`,
  `t21-*`, `t28-*`, `studio-*`, etc.), i.e. Visual/other-action runs, **not**
  a full native CTest run.
- Guest-side `isolde-oracle/oracle.lock` exists but is **stale** (mtime
  `Oct 20 18:24`); no `isolde`/`oracle`/`driver` processes were observed in the
  guest.

**Conclusion:** full native test gate = **executed 0 / 1**; status **unknown**;
no CTest evidence collected under this Q0. Reported as unmeasured, not as a
pass/fail.

These are the unrelated staged/unstaged implementation work the strategy
document requires be preserved. Q0 does not classify, migrate, or alter any of
them.
## 6. Guest tool inventory (exact versions)

Measured remotely on guest `Isolde-Test` over SSH.

| Tool | Version | Status |
|---|---|---|
| git | 2.53.0 | present |
| cmake | 4.2.3 | present |
| g++ | (gcc 15.2.0-16ubuntu1) 15.2.0 | present |
| g++ | (gcc 15.2.0) 15.2.0 | present |
| ninja | 1.13.2 | present |
| python3 | 3.14.4 | present |
| gcov | (gcc 15.2.0) 15.2.0 | present |
| gdb | GNU gdb (gcc 17.1) 17.1 | present |
| clang | — | **NOT FOUND** |
| cppcheck | — | **NOT FOUND** |
| lcov | — | **NOT FOUND** |
| qmllint | — | **NOT FOUND** |
| qmlformat | — | **NOT FOUND** |
| npm | — | **NOT FOUND** |
| node | — | **NOT FOUND** |
| pwsh | — | **NOT FOUND** (host runs PowerShell 5.1) |
| ccache | — | **NOT FOUND** |
| valgrind | — | **NOT FOUND** |

Interpretation: the guest has the core CMake/Qt build and gcov, but **none**
of the free static-analysis/formatting/coverage-aggregator tools recommended by
the strategy document are installed on the guest. No toolchain maintenance was
performed in this slice.
