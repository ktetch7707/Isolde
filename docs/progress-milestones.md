# Weighted, independently verifiable milestones

Latest user authority: [vm-software-completion.md](vm-software-completion.md). Every child is judged by VM-only software pass conditions; non-VM hardware, account/region, participant/approval and actual-computer checks move to the user's post-100% handoff. Existing weights and done/open states are preserved; new VM replacements must pass before earning credit. References below to installed mean installed in the guest, and visual review means agent review.

Current progress authority, adopted at user request 2026-10-29. Original backlog Scope IDs remain requirement groups and preserve their original equal weight within each task. A split group's children divide only that group's existing weight equally. All 27 tasks still have equal weight in Overall. Adding rows cannot increase a group's weight.

`done` means the narrowly stated outcome is proven at the stated evidence level; it does not imply installed, physical, uncoached or whole-feature acceptance. `open` means missing or not yet audited evidence, not necessarily missing code. Original Scope boxes indicate **final group acceptance**; they remain open until every child and all inherited requirements pass. Children inherit the group's full linked requirements; the final acceptance child catches unmapped requirements and cannot be removed to manufacture completion.

Task progress = half-up round(100 × mean(original group earned fractions)). Group earned fraction = done children / all children; unsplit groups retain their original checked/unchecked value. Final task acceptance requires every original group, Done when and Verify requirement, regardless of the displayed percentage. Incomplete tasks cannot round to 100%. Overall = half-up round(mean of the 27 rounded task progress values), capped below 100 until S1 is accepted. Report accepted tasks separately from weighted progress.

Run `pwsh -NoProfile -File tools/backlog-progress.ps1 -CheckTable` to validate IDs, evidence-reference format, parent checkboxes, table consistency and calculate percentages. It reads the backlog and this ledger; it never connects to the VM or rewrites evidence. Evidence content must still be reviewed by the agent; the calculator does not certify it. Required evidence can reside in the workspace or a durable document describing guest evidence. No rerun is needed solely to award credit for an already-proven child.

## Evidence keys

- **J-native**: `build/vm-oracle/t11-complete-app-20261029-j/report/LastTest.log`; source digest `860b4f0a02638c0b64388a288ea240b0e96ca1a5c8815b8bbc10cfb1f715e1b3`, recorded in `docs/live-tv-completion-2026-10-29.md`; snapshot manifest `build/vm-oracle/t11-complete-app-20261029-j/snapshot/files.sha256`. These are native/offscreen software proofs, not installed acceptance.
- **J-runtime**: `build/vm-oracle/t11-complete-app-20261029-j/report/visual-guide-lifecycle.log` plus the candidate J paragraph in `docs/live-tv-completion-2026-10-29.md`. The enclosing journey failed at an unsupported harness observation; only completed preceding assertions earn credit.
- **C-runtime**: candidate C's 23 reviewed Guide checkpoints in `docs/live-tv-completion-2026-10-29.md`, `build/vm-oracle/t11-completion-20261029-c/visual/guide-surfaces/attempt-20261029-064548-306/`. J's completed base journey corroborates the actual PVR/playback/surface behavior; do not reuse C to claim later feature UI.
- **Q-inventory**: `docs/quality/baseline.md`, actual tool/version/availability and warning inventory, tied to its recorded October 25 source. This proves the inventory substep only, not a final-candidate risk baseline.

## T11 — Guide (three original groups, each still one third of T11)

Each of the six children within a group is one sixth of that group's weight. The 18 children are discrete deliverables/evidence contracts, not six mandatory full-suite runs. All source/portability/widget/remote/semantic amendments remain assigned below.

| Milestone | State | Independently finishable outcome / pass condition | Evidence or remaining requirement |
| --- | --- | --- | --- |
| T11.1.a | done | Bounded PVR/native feed generation, timestamp/source mapping, OFF state and failed-import rollback pass production contract tests. | J-native: pvrGenerationIsAtomicAndPreservesUserOverrides; feedImportPreservesSourceTimesAndRollsBackFailures |
| T11.1.b | done | Actual PVR discovery/import, visible local fixture video, Stop and exact Guide return. | C-runtime and J-runtime completed base journey; synthetic fixture, no provider entitlement claim |
| T11.1.c | done | Compact, overlay and full Guide share actual channel selection/progress and remote Back return. | C-runtime; J-native: overlayAndFocusedEntryPreserveGuideStateWithoutSaving; compactPvrProgressAndAccessiblePressUseTheSameActions |
| T11.1.d | done | Station artwork/clock/XSP validation and bounded widget variants/filter selection/focus protection pass native/offscreen tests. | J-native: artworkClockAndPlaylistBoundaries; widgetVariantsFilterBeyondFirstFiveAndKeepFutureSelectionSafe; previewArtworkCannotCoverRemoteFocus |
| T11.1.e | done | Complete actual remote native-feed/no-EPG/error/401/OFF/refresh setup and source-presentation journey. | docs/t11-runtime-2026-10-29.md: F attempt-20261030-133755-262 passes, all 51 captures reviewed; native video/auth/OFF, direct URL with zero listings, exact malformed-refresh rollback and persistent visible error |
| T11.1.f | done | Final surface/art/filter/scale/accessibility/semantic review, clean reference/reset pairs and all remaining T11.1 linked requirements. | docs/t11-runtime-2026-10-29.md: G Guide surfaces/cold-art and Studio/lounge/pastel pass; all 60 captures reviewed, both reference/reset pairs checked; F full 41/41 plus G focused Guide UI/full-coverage reuse; 100-channel VM first frame 13 ms. Broader Studio feature/fidelity acceptance remains with T21/T24 |
| T11.2.a | done | WAL persistence refuses unknown/corrupt databases/overlap; write/cancel failure and real killed-writer recovery preserve committed slots. | J-native: channelAndExtensionFailuresRollback; unknownDatabaseIsNeverRewritten; corruptKnownDatabaseIsNeverRewritten; intervalOverlapRejectedAtDatabaseBoundary; killedScheduleWriterRecoversCommittedSlots |
| T11.2.b | done | Real-time/paused/adjusted clocks, calendar/DST and changed rules preserve committed schedules under native tests. | J-native: virtualClockPausesWhileProfileIsOff; reviewedClockAdjustmentPreservesSchedulesAndSurvivesRestart; calendarPolicySurvivesDstAndPreservesCommittedSlots; rulesOnlyAffectUncommittedSlots |
| T11.2.c | done | Rolling/expired-schedule maintenance uses cached state without Kodi reindex; profile teardown invalidates clients. | J-native: rollingMaintenancePreservesSlotsAndCatchesUpAfterLongOutage; profileIsolationAndInvalidation; startupMaintainsExpiredSchedulesWithoutKodiReads |
| T11.2.d | done | Selected-slot repair refuses stale/foreign choices, supports cancel and preserves other intervals; native shorter-repair rollback passes. | J-native: selectedSlotRepairPreservesIntervalsAndRejectsStaleOrForeignChoices; remoteSelectedSlotRepairRequiresReviewAndHonorsCancelAndStaleRevision; shorterRepairAndPortableSnapshotsAreAtomicAndRemapped |
| T11.2.e | done | Actual playback naturally transitions and saved visible-order surfing selects the expected program. | J-runtime and ledger: assertions completed before unsupported GetEpisodeDetails observation |
| T11.2.f | done | Actual deleted-file failure → targeted repair and exact return; unrelated slot IDs/times preserved, completing the durability/repair group with existing native proofs. | docs/t11-runtime-2026-10-29.md; K lifecycle attempt-20261029-234239-370 passed, all 36 captures agent-reviewed |
| T11.3.a | done | Local channel draft preview/save/cancel/error recovery and profile teardown pass production offscreen remote tests. | J-native: asyncPreviewCreateCancelAndProfileTeardown; remoteCreatePreviewSaveAndFutureSelection; remoteCreationRetainsVisibleFocusAndRecoversFailedSave |
| T11.3.b | done | Portable snapshot remap/rollback and template isolation pass contract/offscreen tests. | J-native: shorterRepairAndPortableSnapshotsAreAtomicAndRemapped; feedRemotePreviewMappingCancelImportAndTemplateIsolation; actual portable artwork journey remains in .d |
| T11.3.c | done | Deterministic seven-day schedule persistence and restart/offset invariants pass native tests. | J-native: persistsSevenDaysAndTwelveHourOffset; simulated offset is not actual twelve-hour outage |
| T11.3.d | done | Actual Cartoon-Network/mixed-rule one-save, calendar/template and portable artwork/remap remote workflows pass. | docs/t11-runtime-2026-10-29.md: C attempt-20261030-083420-219 passes; all 47 captures reviewed; actual portable art/snapshot remap, template reuse, calendar one-save and four mixed movie/episode matches |
| T11.3.e | done | Actual stopped-process VM cold reopen/seek and committed schedule stability, alongside existing production-clock invariants. | docs/t11-runtime-2026-10-29.md: actual cold playback/seek prefix of attempt-20261030-000659-062 plus read-only comparison of all 10,080 committed slots after restarts; combined search/offline journey is not claimed passed |
| T11.3.f | done | Actual denied-network application listings/search/art/local playback/return and remaining T11.3 requirements pass. | docs/t11-runtime-2026-10-29.md: F attempt-20261030-135011-264 passes, all seven captures reviewed; actual cached PNG, five local Search results, elapsed seek, explicit Stop with naturalEnd=false, exact Guide/Home recovery and unchanged committed slots |

## T20.5 — pointer/reflow closure (one fifth of T20)

| Milestone | State | Independently finishable outcome / pass condition | Evidence or remaining requirement |
| --- | --- | --- | --- |
| T20.5.a | done | Selected-item pointer threshold and complete neighbor/insertion previews match remote movement. | docs/studio-movement-acceptance-2026-10-30.md: full G actual-input gesture/ordered suites; full/half zoom, refusal/recovery, insertion/reflow, complete cancel/one-Undo; unchanged movement/test bytes reconciled to J |
| T20.5.b | done | Explicit cross-container targeting, mixed/full/locked layouts and decorative overlap refuse invalid operations. | docs/studio-movement-acceptance-2026-10-30.md: full G destination/policy/overlay/grid/component suites, named targets and complete validated/refused results; current production matches J |
| T20.5.c | done | Remote/pointer/Undo/Back produce byte-identical complete transactions with reduced-motion feedback. | docs/studio-movement-acceptance-2026-10-30.md: full G canonical/bounded-edge proofs plus focused A global DisplaySettings policy matrix (7 functions, no skip), actual stationary origin/invalid-target outlines and mid-proposal no-write toggle; gesture fixture regression B passes; four new native frames reviewed |
| T20.5.d | done | Actual desktop/TV movement, clean references and all linked movement acceptance requirements pass. | docs/studio-movement-acceptance-2026-10-30.md: installed J release 0da0692cbb6b2354bed31852 desktop/Studio/Lounge/Pastel pass, all 43 installed + 10 staged + 4 focused frames reviewed and both reference/reset pairs compared; exact root/nested bytes/Undo/Back/focus, profiles and service preserved; all T20.5 linked requirements mapped |

## T21 — open inspector/catalog groups (each retains one fourteenth of T21)

These rows are open pending exact evidence reconciliation; code presence and historical broad claims are not automatic credit. Reconcile the next selected child's existing logs before implementing or rerunning it. The five accepted original T21 groups keep their prior weight and credit.

| Milestone | State | Independently finishable outcome / pass condition | Evidence or remaining requirement |
| --- | --- | --- | --- |
| T21.6.a | open | All-installed/selected App and Button collection membership with stable validated identities. | Existing source/model contract audit |
| T21.6.b | open | Remote ordering, visible capacity and registry updates preserve canonical data. | Input/parity and dynamic registry evidence |
| T21.6.c | open | Scoped More galleries, launch/return, no truncation and remaining collection acceptance. | Real overflow/runtime/focus journey |
| T21.7.a | open | Real bounded Kodi viewing history with independent count/capacity. | Actual viewing history and request bounds |
| T21.7.b | open | Explicit supported app providers merge deterministically; OFF/auth/offline/teardown isolation. | Provider contract and zero-disabled-request evidence |
| T21.7.c | open | Multiple instances/profiles and actual history updates meet runtime and remaining acceptance. | Real playback/history, not sample/recently-added art |
| T21.8.a | open | Registered shape/container/button/text and shared content/name/label semantics round-trip. | Schema/renderer/data contract |
| T21.8.b | open | Managed linked image/GIF validation, decoding bounds, reduced motion and atomic staged assets. | Native decoder/runtime asset evidence |
| T21.8.c | open | Remote catalog edit/layers/links/focus/inheritance and all remaining catalog acceptance. | Runtime journey plus failure/cancel |
| T21.9.a | done | Saved Pastel preset/fixture uses production collections and generic shapes/buttons. | docs/studio-pastel-see-all-2026-10-30.md: VM studio_unit A/B verifies packaged/fixture parity, ten registered editable buttons, one app collection, genuine history source and no load-time write; broader runtime/reference children remain open |
| T21.9.b | done | Live history/apps overflow, editing and no startup rewrites work generically. | docs/studio-pastel-see-all-2026-10-30.md: G full 41/41, actual Studio/app-gallery/history-source journeys and current J Home/Studio navigation plus 14 reviewed Pastel captures; real production components fed by deterministic boundary providers, exact heading return and unchanged layout bytes; actual external-provider/playback-history acceptance remains T21.7 |
| T21.9.c | open | Both reviewed recreation/reset pairs and all linked Pastel acceptance pass. | Current applicable clean reference evidence |
| T21.10.a | open | Shared named swatches/theme/recent choices and actual remote/pointer color wheel agree. | Production picker input/parity |
| T21.10.b | open | Brightness/opacity/inheritance/reset, private preview and one accepted history entry. | Cancel/write/undo data proofs |
| T21.10.c | open | Before/after contrast/readability across all owning fields and remaining picker acceptance. | Runtime field/palette review |
| T21.11.a | open | Type-specific Size & layout separates outer size/card size/capacity with validated bounds. | Inspector/schema coverage |
| T21.11.b | open | Live size preview/cancel/history/remote-pointer parity and unobscured Done/Apply flow. | Production UI/transaction evidence |
| T21.11.c | open | Exact focus/feedback and remaining workflow acceptance; T24 discovery review remains explicit. | Agent-run VM discovery task/results; no independent participant required |
| T21.12.a | open | Runtime and Studio share authored focus/selection for every component shape. | Resolved styles and actual item bounds |
| T21.12.b | open | Art/labels/context focus/decorative selection and reduced-motion state remain legible. | Light/dark/pastel production rendering |
| T21.12.c | open | Complete selection matrix and remaining acceptance pass. | Runtime captures and review |
| T21.13.a | open | Header/title/logo/clock/date/Search/Settings share validated selectable editable model. | Header schema/draft/model tests |
| T21.13.b | open | Layers/Edit header recovery, actions and permanent Settings/Edit route survive hidden/empty state. | Actual remote recovery and focus |
| T21.13.c | open | Legacy/profile/undo/Apply/export and all remaining header acceptance pass. | Persistence/runtime failure and round-trip evidence |
| T21.14.a | open | Undo/Redo/Layers/Layout/header/Saved designs/editor settings have named locations. | Feature-map inventory and actual routes |
| T21.14.b | open | Destructive reset scope and secondary interaction remain explicit with exact focus. | Remote cancel/restore/error journey |
| T21.14.c | open | Complete capability/discovery/context visibility and remaining usability acceptance pass. | Runtime review; genuine discovery stays with T24 where required |

## T31.6 — baseline reconciliation (one seventh of T31)

The old checked parent conflated an inventory with the complete final-candidate quality/risk gate. Reopen the parent, retain the proven inventory child, and leave unproven requirements open.

| Milestone | State | Independently finishable outcome / pass condition | Evidence or remaining requirement |
| --- | --- | --- | --- |
| T31.6.a | done | Actual source-tied tool/check availability, versions and warnings inventory records unavailable tools honestly. | Q-inventory |
| T31.6.b | open | Complete boundary risk-to-test map, meaningful failure-injection proof and agent critical-boundary review. | Baseline inventory alone does not prove all three outcomes; split further before implementation if needed |
| T31.6.c | open | Final candidate applicability, reproducible contracts and unresolved-defect disposition satisfy all remaining T31.6 requirements. | Frozen final candidate not yet available |

## Remaining work and selection rules

Unsplit groups keep their prior credit; this ledger does not erase older accepted evidence or award unaudited other-task credit. Before selecting any remaining broad group, split it into concrete independently verifiable outcomes under its fixed weight, including an explicit final-acceptance child. Do this as a short checklist, not a separate planning session. Never leave an active group's verified subwork hidden at zero until the entire feature closes. If a child still requires several unrelated workflows, subdivide it without changing its inherited weight; extend the calculator's hierarchy before introducing nested IDs.

Implementation effort alone earns no percentage. Focused native/offscreen proof can complete a child explicitly defined at that level. A runtime child needs real runtime evidence; a final-acceptance child needs its reviews. A blocked human/soak/provider child does not revoke unrelated earned credit. Regressions reopen only affected children and invalidate final acceptance where relevant.

Historical dated percentages in older evidence ledgers remain historical. The current backlog table and calculator supersede them. This accounting revision is recognition of prior verified work, not new application implementation or a claim of additional tests.

Validation: `tools/test-backlog-progress.ps1` checks fixed original group weight, no credit from splitting an open group, half-up rounding, duplicate/unknown-ID rejection and current table consistency. The retained J log was read to check the cited native scenario names. No application suite was rerun for this reconciliation.
