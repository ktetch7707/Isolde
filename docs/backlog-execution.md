# Backlog execution contract

Approved planning response to the user's 2026-10-29 request: make “continue working on the backlog” sufficient. This is the operating procedure, not permission to reduce product scope. The backlog owns requirements, accepted percentages and evidence; `implementation-order.md` owns product sequencing. The execution queue supplies the concrete next work item. Existing machine boundaries and approval requirements remain in force.

## What “continue” means

**User revision, 2026-10-29:** “get this actually done” is an instruction to finish the selected task end to end. Queue rows, test levels, commits and evidence updates are internal checkpoints, not reasons to end the turn or seek confirmation. Continue implementation and cause-directed debugging until the task is accepted or a concrete external blocker prevents further useful authorized work. Do not spend a feature-work session rewriting this plan or building optional testing infrastructure. Repair only tooling that demonstrably blocks the selected outcome.

**Full-suite budget:** default to one full application-suite run at task closure, not one per slice, queue row, candidate or commit. Existing applicable full-suite evidence counts; current T11 already has multiple passes, so another is not the default next step. Use focused regression tests for subsequent bounded corrections and document applicability. A repeat full run requires either a failing full-suite test that needs integrated revalidation, or material cross-cutting changes (for example shared persistence/schema/profile isolation, global dispatch/lifecycle or build/runtime integration) that invalidate the prior coverage. Name the changed boundary and missing coverage before launching. No arbitrary numerical cap may turn a failure into a pass. T31 requires an exact frozen final-candidate full run. This revision overrides older I-per-row or full-run-after-every-fix wording throughout the queue and task plans.

1. Read the backlog's current authority, priority, table and selected task section, this contract, and the queue's current checkpoint. Read only the selected task's Entry docs/files and directly relevant evidence afterward. Do not reload historical ledgers unless needed to resolve a specific discrepancy.
2. Check branch, staged/unstaged/untracked changes, and the checkpoint's existing run/session. Preserve other work. Never reset/stash/clean it or commit it as your own. Do not launch a second build to discover whether a first build finished.
3. Resume an active selected item before selecting another. Otherwise select the first READY item in queue order whose dependencies are satisfied. Product priority follows the backlog, including recorded exceptions. T15.4 is deferred to T24, and T21.9 may unblock T20.5. The existing authorized T11 continuation is the current exception; it does not accept T24 or open new feature tracks.
4. Complete that item's implementation, error/recovery behavior and focused verification as one meaningful batch. If its required implementation already exists, inspect the relevant diff/evidence and verify it; do not rewrite it to create activity.
5. At a checkpoint, update evidence/status and commit the owned logical change on `agent-backlog-progress`, push and verify. Continue to the next eligible item without asking “shall I continue?” A planning-only request, such as the one creating this contract, implements documentation only.
6. An external blocker does not authorize an unrelated feature switch. Record exactly what is needed. Continue independent work within the selected task; use only the explicitly allowed fallback in the queue. Never manufacture a pass or silently skip a gate.

Do not start multiple application tracks or delegate just because multiple agents are available. There is one integration owner and one VM runner owner. Separate user-authorized agents must claim disjoint files/work items in the checkpoint before editing; they serialize VM access and shared-file integration. This document does not itself request subagents.

## Work item and checkpoint format

Each item needs the following before code changes. The queue supplies the outcome/dependencies/test family; the agent fills exact filenames and scenario names from the selected task's Entry files. This is a short implementation checklist, not another design project.

| Field | Required content |
| --- | --- |
| Identity | Stable queue ID, owning backlog gate(s), selected/ready/blocked/done |
| Outcome | A user-visible workflow or a named acceptance/tooling defect to remove |
| Boundary | Included behavior, affected files, explicit dependencies; preserve all linked requirements |
| Cases | Happy path, invalid/error + retry, cancel/no-write, exact focus return; relevant profile/security/bounds cases |
| Verification | Exact focused suite/scenario, integration trigger, graphical cases and review mode |
| Evidence | Candidate/source manifest, executable identity where relevant, run ID, logs/captures and their actual verdict |
| Resume | Last completed step, next action, existing session/lock owner if any, precise blocker |

Use four evidence states: **implemented**, **focused verified**, **runtime verified**, **accepted**. These are independent evidence claims, not invented fractional completion. A row may be runtime verified with acceptance still blocked. Official Scope boxes remain unchecked until all their requirements pass.

Keep a compact current checkpoint in `docs/execution-queue.md`; replace superseded resume details rather than appending every tool call. Detailed logs remain under `build/vm-oracle/<run>/`. Only material decisions/failures belong in task evidence docs. Every final update names the completed outcome, evidence, remaining blocker and next item; include official % only when reconciled.

## Testing ladder

Use PowerShell 7. Every guest operation goes through the bounded runner. `test-all.ps1` is an orchestration/report wrapper over it, not a new VM access path. Commands below run from the repository root. IDs represent immutable runs; choose new IDs only for a justified new Test snapshot.

| Level | When | Command / evidence | What it establishes |
| --- | --- | --- | --- |
| D: documentation/static | Plans, comments, documentation; before any costly run | `git diff --check -- <owned paths>`; check links, queue dependencies, task IDs and arithmetic | No application build claim; do not build merely because docs changed |
| F: focused | After one complete related application edit batch | `./tools/vm-test.ps1 Test -Suite <suite> -RunId <new-id>` | Changed behavior and meaningful failure cases; no staging/deploy/graphical acceptance |
| H: harness | Before a changed acceptance helper drives a long journey | Its existing self-tests + a contract regression for changed bridge APIs/markers/dependencies; Linux-only checks run via the supported runner | Helper understands the production interface; not product acceptance |
| I: integration | Once at complete task closure, or a justified repeat under the full-suite budget above | `./tools/test-all.ps1 -RunId <new-id> -NativeOnly -NoOpen` | All registered CTests, no failed/skipped/zero reports, one frozen staged payload |
| V: runtime/visual | Passing full candidate and prepared reviewed journey | `./tools/vm-test.ps1 Visual -RunId <passing-id> -Cases <affected-cases>` | Exact staged executable, actual input/runtime behavior and captures; no automatic aesthetic/semantic approval |
| A: acceptance | All mapped requirements for a Scope box are demonstrated | Review logs and every required capture, compare both reference recreation/reset pairs where required, reconcile source applicability | May close the gate; representative, installed, offline and hardware evidence still have their own requirements |

For a ready graphical integration batch, `./tools/test-all.ps1 -RunId <new-id> -Cases <cases> -NoOpen` combines I and V. Do not then run I again manually. Reopen an existing report with `./tools/test-all.ps1 -RunId <id> -ReportOnly -NoOpen`.

### Selecting focused tests

Use runner-supported suites, not guessed executable names. Verify current `ValidateSet` in `tools/vm-test.ps1` when selecting a suite absent below. A missing scenario/helper is implementation work; identify it before claiming readiness.

| Changed boundary | Focused suite(s) |
| --- | --- |
| Schema, drafts, persistence, assets, skins, tokens | `component_contracts`; add the applicable Studio suite for UI |
| Studio inspector/input | Smallest relevant `studio_quick`, `studio_selection`, `studio_gestures`, `studio_grid`, `studio_destinations`, `studio_ordered`, `studio_policies`, `studio_overlays`, `studio_unit`, `studio_behavior`; use `studio` only for cross-cutting changes |
| Home/page routing, headers, search, focus | `home_component_navigation` (runner includes relevant Studio navigation partition); plus `component_contracts` for data changes |
| Kodi API/playback/artwork | `kodi_media_client`; owning native/Python regressions are also mandatory at I |
| Guide schedule/store/API | `guide_contracts`; `guide_remote_ui` for UI or `guide` for both |
| Store/provisioning | `store`; `store_remote_ui` for QML-only edits |
| Games/session | `games`; `desktop_mode` or `profile_browser_isolation` when that boundary changes |
| VPN/audio | `vpn` / `audio_backend` |
| OS/provisioner/runner with no suitable focused choice | Existing script self-tests where supported; otherwise one justified I run. Do not install a host toolchain or invent ad hoc guest commands |

New tests exercise production behavior and realistic failures, not source-text imitation or assertions that merely restate implementation. Reversible documentation changes need no application tests. Linux-only skips on the host are not Linux passes.

### When to repeat, and when to reuse

- Finish the batch before F. Do not run F after every small edit. Correct all known related causes and run once. Do not interrupt a healthy build to add polish.
- Run I under the task-level budget above. A changed candidate alone never justifies I. Batch all known required application fixes, use F for bounded corrections and reuse demonstrably applicable broader evidence. Documentation, reporting and harness-only fixes never justify another application I run.
- Harness-only repair: test the helper, then rerun the failed V case on the same passing payload through the supported runner. Record both helper and payload identities. If the runner cannot support that separation, fix that bounded capability or report it; never modify frozen snapshots or bypass integrity checks.
- Reuse earlier runtime evidence only after comparing manifests for relevant application/config/provider/helper paths. Record what changed and why the prior result still applies. Whole-product T31 uses one final candidate regardless of reused development evidence.
- Both reference pairs remain mandatory evidence at owning task closure. Capture them once after the related shared-renderer/appearance work is ready; repeat only a pair invalidated by a relevant change or failed comparison. Do not recapture after each backend-only F run. Installed gates require explicit Deploy authorization and installed identity; staged tests never impersonate installed acceptance.
- New optional polish goes into the owning future item. Correctness, security, data loss, remote accessibility and explicit acceptance failures are required fixes. Do not defer those merely to avoid a rerun.

## Graphical readiness and token use

The current runner requires a passing full Test before staging/Visual. Reuse that payload for unchanged application journeys. If a bounded application correction needs a new graphical payload and the runner cannot stage focused-verified bytes, identify this as a tooling limitation: implement the smallest integrity-preserving runner change within the selected task, or record the specific full run needed to obtain the required payload. Do not pretend the old executable contains the fix, bypass hashes, or silently turn this runner limitation into an endless series of full runs. Final T31 retains its full-suite staging requirement.

Before V, list the exact cases, supported bridge observations, fixture/profile requirements, number of review checkpoints if known, deadlines and review mode. Confirm fixtures/helpers use allowlisted production APIs and avoid denied private fields. Verify captured paths/markers and error handling with H. Resolve known prerequisites before the long run, not halfway through it.

Distinguish **unattended assertions**, **agent-reviewed capture checkpoints**, and **user/representative actions**. The current runner contains timed instruction-file checkpoints; the HTML wrapper does not remove them. Never describe these cases as unattended. An agent running such a case must stay available, poll that session frequently enough to meet its actual review deadline, view captures and provide authorized bounded input through the existing mechanism. Never sleep through the deadline or replace review with blind acknowledgements. Routine guest test actions already authorized by standing instructions do not need renewed user consent.

If a helper improvement removes incidental per-capture waits, it must retain ordered evidence, bounded cancellation/service restoration, required review and explicit consent boundaries. This is planned tooling work, not an already available capability. Required user decisions, account secrets and representative usability remain real external dependencies.

During ordinary build/test execution poll the existing session with small output tails (about 1,000 tokens); read named logs once after completion/failure. Use the longer normal polling interval for builds, shorter intervals for live capture reviews. Do not dump complete manifests, repeated status objects, all historical logs or entire source files to obtain progress. Share a brief update at least every minute while actively working. Wall-clock soak waiting does not require model turns or repeated status checks.

## Failure protocol

1. Stop at the first named failure. Preserve logs/captures; inspect its stage and a bounded relevant log section. Classify it as product, harness, environment/transport, account/provider, or review scheduling. Record the classification and evidence; do not call a harness failure a product failure.
2. Identify a testable cause. Patch that cause and related known issues; run the smallest check capable of falsifying the diagnosis. No unchanged retry, arbitrary timeout increase, relaxed assertion, force-pass, lock deletion or clean rebuild.
3. If the same failure persists after two cause-directed repair attempts, switch to a minimal reproducer/contract diagnosis instead of another long acceptance run. This is a change in debugging method, not permission to abandon the task. A further expensive attempt needs new evidence explaining why it should differ.
4. BUSY means resume/observe its verified owner. Storage means the existing checked guest maintenance process. Host/VM security, transport and approval boundaries remain in `vm-oracle.md`; never use a second access path to bypass a rejection.
5. Long-outage tests use a real receipt and due UTC. Prepare once, record candidate/fixture identity and due time, work only on permitted independent work without touching that frozen fixture, and reopen once due. If no independent work is authorized, checkpoint and return the exact resume time; do not spend model turns waiting. A failed consumed soak requires new valid preparation, not relabelling.

## Closure and progression

Map every linked subrequirement to evidence or an explicit remaining blocker before closing a Scope box. Keep official percentages under the existing 27-task arithmetic; milestone states do not change the denominator. Detect inconsistencies such as checked T31.6 versus 0/7, inspect the actual evidence, and reconcile honestly before using the number. An unchecked required gate never becomes complete from elapsed time, lines of code, a process launch or a simulated physical result.

Before handing off, record an exact resume action and any existing run ID. The next “continue” resumes it. Ask the user only for a required unavailable decision/credential/physical action or permission outside existing authorization; state the concrete blocker. Do not repeatedly request ordinary test/guest-maintenance permission or approval of routine implementation choices.
