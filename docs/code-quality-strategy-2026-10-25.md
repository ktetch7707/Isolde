# Isolde code quality improvement plan

Date: 2026-10-25. Status: proposed management strategy; no implementation or acceptance credit.

## 1. Outcome and scope

Make Isolde maintainable, consistently formatted, documented, meaningfully unit tested and verified in its real VM environment. Coders perform implementation; the project manager assigns bounded work, sets acceptance criteria, reviews evidence and controls integration. A dashboard supports these decisions; it cannot certify SOLID, DRY, usability or correctness.

This plan supplements `agent-backlog.md`, `implementation-order.md`, `vm-oracle.md` and `studio-testing.md`. It does not reorder feature work, close existing gates or authorize a rewrite. Quality infrastructure needs an explicitly scheduled slice before implementation. Thereafter, quality requirements apply to each feature slice. No new active backlog task or percentage change is introduced by this proposal.

## 2. Current position

The inspected project uses C++17, Qt 6, QML, CMake/CTest, Python and PowerShell tooling, plus JavaScript/web assets. Existing Qt Test, Qt Quick Test, provisioning and integration suites already cover substantial behavior. The bounded VM runner captures source identities, JUnit, service state and graphical evidence; reuse it.

The scoped inspection found no repository-wide formatter/analyzer configuration or `.github` workflow directory. This is an initial inventory, not a completed audit. Test registration counts are not unique behavior counts: focused registrations reuse broad-suite functions. No coverage percentage, defect count or architecture score has been measured for this plan.

The working tree contains unrelated staged and unstaged implementation work. Preserve it; baseline and later reports must identify exact source bytes, including uncommitted changes.

## 3. Free tooling decision

**Recommended starting point: free native tools plus a generated local HTML/JSON quality report.** This gives useful C++ and QML checks without operating a server. No paid license, trial, public-repository requirement or hosted quota is needed. Compute, storage and maintenance still have a cost.

SonarQube Community Build is free/open source, but the current official language matrix does not include its native C/C++ analyzer in Community Build. QML must also have Qt-specific checks. Therefore it cannot be Isolde's sole quality gate. Optionally evaluate Community Build later for its supported languages and external issue/report ingestion; validate the pinned release's import formats, licensing, branch limitations and resource requirements before promising integration. Imported diagnostics do not provide native C++ semantic analysis. Do not rely on an unmaintained community C++ plugin.

| Area | Proposed free tools | Use and qualification |
| --- | --- | --- |
| C++ formatting | clang-format | Pin version; check owned source; separate mechanical commits |
| C++ defects/design warnings | clang-tidy and Cppcheck open-source edition | Use CMake compilation database and Qt-aware configuration; start with high-confidence checks |
| QML | qmllint and qmlformat from the matching Qt installation | Supply real module/type import paths; qualify formatter changes against runtime and fixtures |
| Python | Ruff lint/format | Pin version and rules; preserve provisioning/security behavior |
| PowerShell | PSScriptAnalyzer | Run only where available in the guest/tooling environment; do not install host modules |
| Shell / standalone JS / web assets | ShellCheck; ESLint and Prettier where applicable | Inventory actual files first; keep QML JavaScript under Qt tooling; avoid redundant tools |
| C++ coverage | GCC gcov with gcovr, or LLVM coverage | Select the installed compatible compiler path; separate instrumented cache |
| Python coverage | coverage.py | Report orchestration coverage separately from native application coverage |
| Runtime memory/undefined behavior | AddressSanitizer and UndefinedBehaviorSanitizer | Separate bounded diagnostic build; qualify Qt/WebEngine behavior and suppress only evidenced external defects |
| Documentation | Markdown/link checks; Doxygen if useful for public C++ APIs | Human-reviewed contracts remain mandatory; generated pages are supplementary |

Install/pin test-only tooling inside the authorized guest through repeatable setup, with versions and licenses recorded. Keep application dependencies unchanged. Downloads happen during setup; installed checks must run offline. Source uploads and a hosted service are unnecessary. Optional server hosting must stay separate from the acceptance guest's build/display resources and requires an agreed location; never install services on the personal host.

## 4. Delivery phases and acceptance

These are proposed work packages, not existing T-task IDs. Estimates are coder effort ranges for scheduling, to be revised after measurement; VM elapsed time and existing feature work are additional.

| Phase | Coder deliverables | Manager acceptance | Initial effort |
| --- | --- | --- | --- |
| Q0: inventory and baseline | Language/tool/version inventory; reproducible full native run; warnings, coverage gaps, duplicate logic and risk register; baseline report tied to source manifest | All owned files classified; missing tools/tests visible; no invented clean score; prioritized top ten risks | 1–2 days |
| Q1: conventions and repeatable checks | Proposed coding/documentation standards, `.editorconfig`, formatter/linter configurations, exclusions, report schema and bounded runner integration | Clean checkout reproduces reports; intentionally bad sample fails; missing analyzer, timeout and malformed report cannot pass | 2–4 days |
| Q2: stop new debt | Changed-code gates, review template, baseline comparison and expiring suppression register | A representative C++, QML and tooling change demonstrate success and failure paths; pre-existing findings remain visible | 1–2 days |
| Q3: strengthen critical tests | Risk-to-test matrix; characterization and regression tests for priority boundaries; coverage report | Failure injection proves tests detect real regressions; full native and relevant VM acceptance pass on exact source | 3–5 days initially |
| Q4: incremental remediation | Small hotspot refactors, duplicated policy removal, module contracts and ADRs | Observable behavior and schema compatibility preserved; relevant tests and VM evidence; no feature regressions | Budget 15–20% of subsequent slices |
| Q5: sustained operation | Integration/release scorecard, trend report, periodic full analysis and debt review | Two consecutive integration checkpoints meet gates; documented reproducibility and recovery | Ongoing |

Complete Q0–Q2 before enforcing numerical thresholds. Q3–Q4 then follow risk and current feature sequencing. Pause promotion when a gate fails; repair the named cause rather than retrying blindly. No calendar completion promise for the whole codebase before Q0.

## 5. Acceptance gates

### Every code change

- Formatting and configured analyzers pass for changed owned code. New compiler warnings and high-confidence analyzer defects block integration after Q2. Existing findings cannot be hidden by enlarging exclusions or refreshing the baseline.
- Changed behavior has tests for normal, invalid and failure/recovery paths where applicable. A bug fix includes a regression that fails against the old behavior. Trivial mechanical edits need verification, not artificial unit tests.
- Public contracts, user-facing behavior and operational documentation are updated in the same slice. A separate review checks responsibilities, ownership, duplication and test quality.
- Relevant focused tests pass during development. Completion follows the existing full-test and task-specific VM gates; a focused pass is never reported as full acceptance.
- No unexplained disabled/skipped tests, empty reports, new flaky retries or unresolved profile/save/security regressions. Scope-bound exceptions require evidence and a follow-up owner.

### Coverage policy, introduced after a trustworthy baseline

Proposed initial gate: at least 80% changed executable C++/Python line coverage and 70% changed branch coverage where the selected tool measures it reliably. Ratchet module coverage upward; do not impose 80% across the legacy repository immediately. Report uncovered lines and missing branches, not only percentages. Small diffs and compiler-generated/Qt macro branches require explicit review rather than meaningless tests.

Require explicit tested scenarios for atomic rollback, invalid schema, profile teardown/isolation and unsafe execution regardless of numerical coverage. QML uses a behavior/scenario matrix initially: native coverage does not measure QML execution. Do not combine unlike coverage figures into a single project percentage. Exclude generated Qt/moc/resource files and third-party code with documented paths; include owned adapters and scripts.

### Integration and release

Run the full native CTest gate through the existing runner, full configured analysis, relevant diagnostic builds, and graphical journeys for application/UI changes. Inspect every captured frame and compare both reference recreation/reset pairs. Preserve current open visual/design gates; an unchanged screenshot is not automatic design approval.

Installed acceptance uses the exact tested run after an explicitly authorized Deploy. Disconnected cold boot/Generator Test and physical HDMI/CEC/audio/controller acceptance remain distinct release gates. VM automation cannot substitute for hardware proof.

## 6. What SOLID and DRY mean here

| Principle | Review expectation in Isolde |
| --- | --- |
| Single responsibility | QML presents state and emits user intentions; validated C++ domain services own rules; adapters own process/network/filesystem work. Split modules for identifiable reasons to change, supported by tests. |
| Open/closed | Extend validated declarative capabilities and existing provider contracts; avoid new destination/type switches scattered across views. Reject unknown or unsupported data safely. |
| Liskov substitution | Every provider/session implementation obeys the same failure, cancellation, bounds, ownership and teardown guarantees. Add shared contract tests where substitution is real. |
| Interface segregation | Expose small consumer-oriented capabilities; views should not receive unrestricted process/file/network authority. |
| Dependency inversion | Domain policy depends on narrow existing boundaries; inject external clients, clock or storage at testable seams when needed. Avoid framework/container introduction. |
| DRY | One authoritative schema, save transaction, navigation policy and appearance model. Consolidate duplicated knowledge after proving shared semantics; visually similar code alone does not justify abstraction. |

Use RAII and explicit QObject/thread ownership; prefer clear value types and bounded operations. Keep cancellation and error propagation explicit. Never replace simple code with speculative inheritance or generic frameworks to improve a metric. Function size, complexity and duplication reports identify review candidates; they are not automatic refactoring instructions.

Prioritize observed defect history, churn, boundary risk and missing tests. Candidate areas to assess include persistence/schema, Studio transactions/focus, session teardown/process launch, Kodi adapters and provisioning/VM recovery. These are review targets, not claims of discovered defects. Preserve Kodi ownership, declarative layouts, remote parity, managed artwork and all existing bounds.

## 7. Documentation deliverables

- A concise contributor guide: guest setup, exact check/test commands, formatting, evidence locations and failure handling.
- A coding standard: C++/Qt ownership/threading, error/cancellation contracts, QML responsibilities, Python/PowerShell conventions and practical SOLID/DRY examples.
- Per-module contracts for changed/high-risk modules: purpose, owner, dependencies, inputs/outputs, limits, state transitions, profile lifecycle and test locations.
- API comments for non-obvious public behavior: invariants, ownership/lifetime, side effects, failure semantics and units. Explain why; avoid comments that repeat statements or decorate every private getter.
- Schema/version compatibility documentation, migration/rollback rules, provider capability matrix and repeatable operational/recovery runbooks.
- Short architecture decision records for consequential changes: context, options, decision and consequences. Update user documentation when behavior changes.

The reviewer must be able to follow one representative path from remote input through validation, persistence/execution, visible result, failure and focus restoration using the documentation and tests.

## 8. Test investment order

1. Data loss and security: invalid/oversized schema, atomic write failure and rollback, original-file preservation, profile isolation/teardown, unsafe IDs/paths/commands and managed artwork.
2. Domain behavior: transaction/undo/reflow bounds, canonical remote/pointer equality, navigation/focus origins and empty-state recovery.
3. Boundary contracts: bounded Kodi requests, timeout/malformed/disconnected replies, session cancellation, failed launch and exact return. Fake external services for unit tests; retain real integration journeys.
4. Tooling: provisioning idempotence/rollback, runner locks, source hashes, timeout/cancel/service restoration and failed-report propagation.
5. Real UI/system: rendered visibility, remote-only workflows, both visual fixtures, installed launch/playback/return and offline recovery.

Reuse existing Qt Test, Qt Quick Test and Python suites. Test production rules rather than duplicated test implementations. Use isolated temporary profiles, deterministic fakes and condition/signal waits. Flaky tests remain defects with owners; do not turn off assertions or add blind retries. Sanitizers and coverage use separate bounded caches and must not silently alter the ordinary accepted executable.

## 9. Coder work and review process

The manager gives each coder a work order containing: existing task/slice ID, exact entry files/docs, observed problem, measurable outcome, compatibility constraints, tests, VM cases, evidence outputs and maximum scope. Coders return a small diff, updated contracts and evidence; they do not declare success from tool scores alone.

For significant refactors, capture characterization tests first, make the smallest behavior-preserving change, then verify. Keep formatting-only changes separate from behavior changes. Do not refactor unrelated modules while implementing a feature. An independent review pass is required for profile, persistence, process/security and test-runner boundaries; review must inspect the diff and evidence rather than accept a coder's summary.

Each finding has a stable ID, tool/rule, source location, severity, baseline/new status, rationale, owner, linked task, acceptance test and disposition. Prioritize confirmed data loss/security first, then correctness/lifecycle, then maintainability. Suppressions record reason, precise scope, reviewer and review date; broad disabling and metric-only fixes are prohibited.

Use existing task-prefixed logical commits on `agent-backlog-progress`, and the standing push/verification policy when implementation is authorized. Never stage unrelated work or generated reports/artifacts. Planning does not authorize application changes, deployment or installation.

## 10. VM and automation integration

Implement future quality stages as bounded extensions of `tools/vm-test.ps1` and its existing guardian/report machinery, with regression tests. Do not invent a second SSH/build pipeline. Proposed quality switches or stages do not exist yet; define and document them in Q1.

Existing commands remain:

```powershell
./tools/vm-test.ps1 Test -Suite studio
./tools/vm-test.ps1 Test
./tools/vm-test.ps1 Visual -RunId <passing-full-run-id>
./tools/vm-test.ps1 Status -RunId <existing-run-id>
```

The full Test runs CTest with output-on-failure inside the guest. Poll the existing tool session; serialize guest use, stop at the first named failure and preserve source identity. No healthy-build interruption, manual lock removal or host maintenance. Quality reports must retain exit codes and distinguish passed, failed, missing and not applicable. A crashed analyzer or absent coverage report is an incomplete gate.

Start with an agent-invoked repeatable gate. Hosted CI is optional later, following repository/provider availability and zero-cost limits; enforce the same scripts in either environment. Do not expose the personal host or guest SSH endpoint to public runners. No server or container platform is required for the initial plan.

## 11. Management scorecard and completion

At each integration checkpoint report: exact source manifest/run ID; analyzer versions and completion; new versus baseline findings by severity; formatter status; measured coverage by language/module; unique behavior coverage and untested risks; full/targeted test distinction; flaky/skipped tests; VM/visual/installed/offline/hardware gates; unresolved exceptions; and links to evidence. Report trends weekly during active work without equating code quality progress to feature completion.

Maintain one concise tracked scorecard and debt register; retain large HTML/JSON, JUnit and screenshots under runner evidence, outside Git. Set a guest-space retention policy during Q1 that preserves current acceptance and useful failure evidence, records cleanup and excludes host/shared mounts.

Program adoption is complete when the documented gate is reproducible, intentional violations fail, new code meets the agreed standard, critical boundaries have demonstrated regression coverage, documentation explains the system, and two consecutive integration checkpoints pass. Existing debt stays visible and declines through planned slices. No unsupported claim of universal SOLID compliance, zero bugs or 100% meaningful coverage.

## 12. Official references and decision verification

Tool capabilities were checked against official sources while drafting. Pin versions and recheck capabilities/licenses in Q0; linked latest documentation may describe a newer Qt/LLVM release than the guest.

- [SonarQube Community Build source and scope](https://github.com/SonarSource/sonarqube)
- [Sonar language/edition matrix](https://docs.sonarsource.com/sonarqube-community-build/analyzing-source-code/languages/overview)
- [clang-tidy](https://clang.llvm.org/extra/clang-tidy/index.html) and [Clang tooling/formatting](https://clang.llvm.org/docs/ClangTools.html)
- [Cppcheck open-source manual](https://cppcheck.sourceforge.io/manual.html)
- [Qt qmllint and CMake integration](https://doc.qt.io/qt-6/qtqml-tooling-qmllint.html)
- [LLVM coverage](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html)

Planning verification: checked this proposal against the scoped CMake/test inventory and VM rules. No application files changed; no tests, scans, installs or VM actions were performed. Feature completion remains unchanged.
