# Isolde Quality Debt Register — Q0 (2026-10-25)

Ranked top-10 risks extracted from the Q0 baseline (`docs/quality/baseline.md`).
Each entry is **measurable** (evidence path cited) and **actionable** (Q1 slice
target). No application code was modified in producing this register.

---

## Scoring rubric

| Column | Meaning | Description |
|---|---|---|
| **ID** | Short reference code for tracking in Q1. |
| **Risk** | One-sentence description of the quality gap. |
| **Evidence** | Section and line number from `baseline.md` where the risk is documented. |
| **Q1 entry file** | Proposed new/edited file for the Q1 implementation slice. |
| **Acceptance check** | Minimal gate that must pass for this risk to be resolved. |

---

| ID | Risk | Evidence | Q1 entry file | Acceptance check |
|---|---|---|---|---|
| R01 | **Gate not invocable** — the quality gate (`tools/vm-test.ps1 Test`) cannot run on the host because `pwsh` (PowerShell 7) is not installed. | §7 (full native test gate not executed, lines 167–177) | `tools/vm-test.ps1` — add host `pwsh` discovery/fallback or CI wrapper | Host has `pwsh` available OR the script runs successfully under PowerShell 5.1 with a compatible substitute |
| R02 | **Coverage unmeasured** — no baseline % for changed-C++/Python; strategy gates (≥80%/≥70% branch) cannot be enforced or trended. | §5.4 (coverage tooling status, lines 106–112) | `CMakeLists.txt` — add `CMAKE_CXX_FLAGS "--coverage"` and `find_package(lcov)` options (default-off) | `ctest --output-on-failure` produces a `.info` file; `lcov --summary` runs without error |
| R03 | **No static analysis** — C++/QML defect-detection defaults to review-only; no clang-configured analyzer gate exists. | §4 (inventory, lines 33–47) | `tools/vm-test.ps1` — add `clang-tidy` / `cppcheck` step (optional, behind feature flag) | `clang-tidy` or `cppcheck` runs on `src/` and emits a non-fatal report |
| R04 | **No QML linter/formatter** — presentation regressions are not machine-checked; `qmllint`/`qmlformat` absent. | §5.4 (coverage tooling, lines 106–112) | `tools/vm-test.ps1` — add `qmllint` and `qmlformat` steps (default-off) | `qmllint` checks all `qml/*.qml` without fatal errors; `qmlformat` runs in-check mode |
| R05 | **Tools absent on guest** — recommended free toolchain (clang-tidy, cppcheck, qmllint/qmlformat, Ruff) not installed on the Isolde-Test guest. | §6 (guest tool inventory, lines 278–306) | `tools/vm-oracle.py` — add package-install install step (apt-get/brew) behind `install-toolchain` guard | Guest has `clang-tidy`, `cppcheck`, `qmllint`, `qmlformat`, `ruff` after running the install step |
| R06 | **Test plumbing drift** — Studio QML live outside the CTest contract; test registration counts ≠ behaviour counts. | §5.3 (QML/integration coverage, lines 97–102) | `tests/studio/CMakeLists.txt` — register Studio QML tests with `add_test()` (or document by-design) | `ctest --list-tests` lists Studio QML test names matching `tests/studio/` directory |
| R07 | **Co-located unrelated work** — 29 modified files across T13–T29 in one tree; risk of cross-feature contamination in any future baseline. | §2.1 (working-tree changes, lines 39–52) | `docs/agent-backlog.md` — add "baseline hygiene" note: new slices must not co-mingle unrelated modifications | `git stash` preserves baseline slice isolation; `git diff --name-only` between baseline and Q1 shows only Q1-scoped changes |
| R08 | **Offscreen/SVG heap fragmentation** — no memory-diagnostic tooling (valgrind/gdb present but no baseline measured). | §9 (testing gaps, lines 180–188) | `tools/vm-test.ps1` — add valgrind/gdb offscreen memory check (behind feature flag) | `valgrind --leak-check=full` or `gdb batch` on an offscreen test run emits a non-fatal summary |
| R09 | **Runner is not CI source of truth** — runner is host-driven and host-dependent; reproducible CI would require a hosted workflow. | §11 risk 9 (top ten risks, lines 208–239) | `tools/vm-oracle.py` — add CI-config generation (GitHub Actions template) that does not depend on host `pwsh` | A minimal GitHub Actions workflow `.yml` exists in `.github/` that invokes the test gate without requiring local `pwsh` |
| R10 | **No evidence for pass/fail** — decisions today rest on inventory, not on measured gate results. | §11 risk 10 (top ten risks, lines 208–239) | `docs/quality/baseline.md` — expand §7 with actual CTest run evidence (to be filled in Q1) | After Q1 gate runs, §7 records exit code, stdout/stderr, and `oracle.log` artifacts |

---

## Notes for Q1 implementation

- Each row above maps to a **single logical change** (one file edit, one small feature add, one doc update).
- No row should be considered "done" until its **acceptance check** passes.
- The `Q1 entry file` column indicates where the change should live; some entries may require new files (e.g. `.github/workflows.yml`) — those are new files, not modifications to existing code.
- The acceptance checks are **minimal gates**; they must not block other Q1 work, but they must be demonst-checked before the baseline can be upgraded to a Q1-pass.