# T31 — Feature/route-to-test matrix (T31.1)

Version: 1.0
Candidate: frozen at T31.2 hash freeze
Status: matrix defined; evidence pending VM gates
Isolde version: 0.0.1.2

This matrix maps every active software gate and new planning requirement to its
registered test(s), pass condition, and evidence path. It is the T31.1
requirement matrix: no hidden skips, no "Unsupported" substitute for a promised
route. H01–H05 and already-deferred features are retained as handoff-only,
outside S1.

## Active task gate matrix

| Task | Gate | Registered test(s) | Pass condition | Evidence | Status |
|------|------|-------------------|----------------|----------|--------|
| T01 | core foundation | `ctest` full suite (14/14) | all registrations pass | `build/vm-ctest.txt` | VM-passed |
| T03 | Kodi search/playback | `kodi_local_bridge`, `kodi_playback` | real search/page/playback/return | VM installed acceptance | pending |
| T04 | native poster boundary | `ctest` + installed workflow | poster edit/save/rollback | `t04-kodi-verification-2026-11-12.md` | partial |
| T05 | layout validation | `component_contracts`, `home_component_navigation` | geometry/focus at boundaries | studio evidence | pending |
| T09 | provider discovery | provider HTTP adapter tests | opt-in/attribution/cancellation | `test-campaign-2026-11-11.md` | partial |
| T10 | subtitle queue | subtitle queue/review tests | bounded SRT/provenance | `test-campaign-2026-11-11.md` | partial |
| T11 | page lifecycle | `ctest` + installed acceptance | page create/edit/reopen | VM evidence | passed |
| T12 | native return | `native-return-*` VM logs | exact app return | build logs | partial |
| T14 | discovery/subtitle boundary | `t14_discovery_subtitles` | OFF/cancellation assertions | test-campaign | partial |
| T15 | Studio foundations | Studio QML suites | wireframe/feature map | studio evidence | passed |
| T16 | transactions/history | `studio_quick`, `studio_workflows` | Apply/Undo/restore | studio evidence | passed |
| T17 | canvas geometry | `studio_*` + visual fixtures | cell boundaries/scaling | studio evidence | passed |
| T18 | Studio workspace | Studio QML + installed entry | page selection/create/edit | studio evidence | passed |
| T19 | appearance controls | Studio QML suites | precision appearance | studio evidence | passed |
| T20 | Kodi/app viewing history | history tests | bounded viewing history | VM evidence | pending |
| T21 | Studio editor chrome | `studio_*` full 45/45 | installed acceptance | studio-t21-acceptance | passed |
| T22 | templates/starters | `component_contracts` | 4 starters apply/editable | docs/studio-templates | pending |
| T23 | Apply/recovery | `studio_*` + recovery tests | atomic save/rollback | VM evidence | pending |
| T24 | editor parity/usability | `studio_*` + focus/contrast | feature parity | VM evidence | pending |
| T25 | source gallery | source action tests | installed Sources actions | feedback-r 61/61 | passed |
| T26 | search flow | search tests | cold/profile/save-failure | feedback-r evidence | passed |
| T27 | quality baseline | analyzer reports | clang-tidy/cppcheck/qmllint | quality strategy | pending |
| T28 | default apps | provisioning tests | default-app install | feedback-r | passed |
| T29 | Store lifecycle | `store-*` 62/62 + boundary 6 | install/open/return/remove/update | store-final-acceptance | passed |
| T30 | Games routes | emulator provisioning tests | 10 routes + playable positive | emulator contract | pending |
| T31 | final acceptance | full CTest + VM walkthrough | all 7 gates pass | this doc + VM evidence | 0/7 |

## New planning requirements (post-2026-10-29)

| Requirement | Source | Mapped test | Pass condition | Evidence |
|-------------|--------|-------------|----------------|----------|
| VM-proven software confidence | `vm-software-completion.md` | full CTest + clean VM install | 100% tests pass in clean VM | VM runner |
| Beginner guide walkthrough | `first-product-software-scope.md` §8 | `docs/user-setup.md` + T31.7 | agent completes walkthrough | `docs/user-setup.md` |
| Code quality gates | `code-quality-strategy-2026-10-25.md` | analyzer availability report | tools present, warnings bounded | T31.6 risk baseline |
| No hidden skips | backlog T31.1 scope | matrix review | every promised route has a test | this matrix |

## Explicit boundaries (fixtures, not live services)

| Boundary | Fixture/substitution | Not allowed to block S1 |
|----------|---------------------|------------------------|
| Live account/region | controlled loopback HTTP responses | paid account/entitlement |
| DRM/target runtime | explicit controlled fixtures | target-only GPU/controller/media |
| Physical hardware | VM input simulation + virtual discs | actual TV/remote/drive/capture |
| User approval/representative | agent task walkthrough | uncoached human participant |
| Twelve-hour soak | deterministic clock test + cold reopen | real elapsed-time receipt (optional) |

## Evidence rules

- Every test name must match a registered CTest or Python test name.
- Pass condition must be an observable result, not a process launch or acknowledgment.
- Evidence paths must exist in the repo or under `build/` VM evidence.
- Reused evidence requires an applicability check recorded alongside it.
- Missing software or a failing VM production workflow is a blocker; it cannot be
  moved to the handoff list to make progress green.
