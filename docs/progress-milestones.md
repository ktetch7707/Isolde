# Weighted, independently verifiable milestones

## T24 — installed cutover (five original groups, weights unchanged)

Coding-only exception authorized November 2, 2026 while VM verification is stalled.
Existing evidence has not been reconciled to this changed candidate; no T24 credit
is awarded for source presence or the local auditor unit tests. Final children
retain every inherited requirement, including T22/T23 dependencies.

| Milestone | State | Independently finishable outcome / pass condition | Evidence or remaining requirement |
| --- | --- | --- | --- |
| T24.1.a | open | Core blank/existing/multiple/nested, collision/full-grid, remote/pointer and scrolling matrix passes on the candidate. | docs/studio-t24-coding-2026-11-02.md; installed tasks pending. |
| T24.1.b | open | History/profile/invalid-doc/repair/atomic-failure/recovery and all inherited T24.1 requirements pass. | docs/studio-t24-coding-2026-11-02.md; source-matched native and installed evidence pending. |
| T24.2.a | open | Both exact-source installed recreation/reset pairs and 720p/1080p/4K scaling are explicitly reviewed. | docs/studio-t24-coding-2026-11-02.md; new candidate capture/review pending. |
| T24.2.b | open | Legacy preservation/conversion, isolated real-Kodi sources and all inherited T24.2 requirements pass. | docs/studio-t24-coding-2026-11-02.md; installed evidence pending. |
| T24.3.a | open | Named color/move/size/delete-Undo/control/header/discovery tasks pass with observed input, focus and consequences. | docs/studio-t24-coding-2026-11-02.md; agent-run installed tasks pending. |
| T24.3.b | open | Declared VM performance, accessibility, disconnected Generator/network denial and all inherited T24.3 requirements pass. | docs/studio-t24-coding-2026-11-02.md; actual measurements and installed evidence pending. |
| T24.4.a | open | Production entry, Home/Back/dirty exit, named-page origin and removed-page recovery pass native/QML regression tests. | docs/studio-t24-coding-2026-11-02.md; coding prepared, Qt tests not run. |
| T24.4.b | open | Complete legacy parity map and normal shipped cutover/recovery, including all inherited T24.4 requirements, pass installed review. | docs/studio-t24-coding-2026-11-02.md; installed parity/recovery review pending. |
| T24.5.a | open | Source/release hashes, complete logs/captures, actual usability/performance results and service/profile restoration are published. | docs/studio-t24-coding-2026-11-02.md; pending evidence inventory prepared. |
| T24.5.b | open | Remaining software/hardware boundaries and dependent T15.4/T24 acceptance are reconciled against the final candidate and all inherited requirements. | docs/studio-t24-coding-2026-11-02.md; no acceptance or completion claimed. |


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
| T21.6.a | done | All-installed/selected App and Button collection membership with stable validated identities. | [Collection reconciliation](studio-collections-2026-10-30.md): unchanged G schema/native atomic bounds and focused A selection. |
| T21.6.b | done | Remote ordering, visible capacity and registry updates preserve canonical data. | [Collection reconciliation](studio-collections-2026-10-30.md): focused A real remote/pointer reorder and open-gallery registry changes. |
| T21.6.c | done | Scoped More galleries, launch/return, no truncation and remaining collection acceptance. | [Installed collection acceptance](studio-collections-2026-10-30.md): full E 41/41, six native cycles, Store removal/reinstall, 17 collection frames and both reference/reset pairs. |
| T21.7.a | done | Real bounded Kodi viewing history with independent count/capacity. | [History evidence](studio-history-2026-10-31.md): A/J Kodi native merge/date/count bounds and I independent production instance/query tests; K remote/pointer count-capacity endpoints/cancel. Actual playback/update acceptance remains .c. |
| T21.7.b | done | Explicit supported app providers merge deterministically; OFF/auth/offline/teardown isolation. | [History evidence](studio-history-2026-10-31.md): I/J native protocol/profile/failure proofs, M/Q actual source selection/cancel/Undo and installed Q local app-opening adapter; eight native input/normal-exit cycles, including two exact history reopens with Kodi unavailable. No account/service viewing-history claim. |
| T21.7.c | done | Multiple instances/profiles and actual history updates meet runtime and remaining acceptance. | [History evidence](studio-history-2026-10-31.md): installed Q real Kodi playback/lastplayed/two-instance/replay and app-history journeys pass; 62 installed captures reviewed, full L plus strict focused Q coverage. |
| T21.8.a | done | Registered shape/container/button/text and shared content/name/label semantics round-trip. | [Catalog evidence](studio-catalog-2026-10-31.md): full AB and final focused AF prove prototypes, schema, plain body/label independence, inheritance, replacement, focus and real portrait framing. |
| T21.8.b | done | Managed linked image/GIF validation, decoding bounds, reduced motion and atomic staged assets. | [Catalog evidence](studio-catalog-2026-10-31.md): native I/Y and full AB real GIF Apply/failure/reload/portable restoration/FIFO refusal; AF real decoded pixel/motion/lease/browser/framing checks and installed exact-byte GIF Apply/reopen. |
| T21.8.c | done | Remote catalog edit/layers/links/focus/inheritance and all remaining catalog acceptance. | [Catalog evidence](studio-catalog-2026-10-31.md): installed AF catalog/Studio/Lounge/Pastel pass; six persisted nodes, body/label/name separation, links/Layers/cancel/Apply/reopen, all 56 final installed captures and both original reference/reset pairs reviewed. |
| T21.9.a | done | Saved Pastel preset/fixture uses production collections and generic shapes/buttons. | [Pastel closure](studio-pastel-closure-2026-10-31.md): S01 native parity retained; full AB verifies ten editable Buttons, one App carousel, genuine history/same-source grid and no load-time write; AF saved structure inspected. |
| T21.9.b | done | Live history/apps overflow, editing and no startup rewrites work generically. | [Pastel closure](studio-pastel-closure-2026-10-31.md): S01 navigation plus accepted S03/S04 real collection/session/Kodi history evidence; unchanged Q provider bytes, current AB/AF input tests and installed generic Pastel overflow/exact return/no-write. |
| T21.9.c | done | Both reviewed recreation/reset pairs and all linked Pastel acceptance pass. | [Pastel closure](studio-pastel-closure-2026-10-31.md): full AB/current focused AF, installed AF both original reference/reset pairs and saved designs, all 56 installed captures reviewed; source applicability and completed S03–S05 dependencies reconciled. |
| T21.10.a | done | Shared named swatches/theme/recent choices and actual remote/pointer color wheel agree. | [Color completion](studio-color-completion-2026-10-31.md): existing AB wheel/swatch parity plus E actual wheel/mixed cases and F shared field/Recent matrix; D bounded canonical profile-local recent persistence. Final K installed review accepted in .c. |
| T21.10.b | done | Brightness/opacity/inheritance/reset, private preview and one accepted history entry. | [Color completion](studio-color-completion-2026-10-31.md): D atomic metadata/color Apply failure/retry/reload/Undo/profile teardown, F shared field cancel/one acceptance/mixed and E passing original brightness/opacity/reset/transaction regressions; K proves actual brightness-slider pointer/remote byte-identical transactions; H full43 and K focused4 pass. Final K installed .c accepted. |
| T21.10.c | done | Before/after contrast/readability across all owning fields and remaining picker acceptance. | [Color completion](studio-color-completion-2026-10-31.md): final K four focused suites plus H43/43, nine native fields/light/dark/pastel, actual installed color/chrome/cancel/reset/reopen and40 reviewed captures including both reference/reset pairs. |
| T21.11.a | done | Type-specific Size & layout separates outer size/card size/capacity with validated bounds. | [Combined workflow evidence](studio-workflow-completion-2026-10-31.md): F proportional size schema/actual input; existing H/K card/capacity and typed controls. Final acceptance .c remains open. |
| T21.11.b | done | Live size preview/cancel/history/remote-pointer parity and unobscured Done/Apply flow. | [Combined workflow evidence](studio-workflow-completion-2026-10-31.md): F actual pointer/remote accepted byte parity, one Undo, private cancel, impossible-ratio refusal and exact field return. .c retains installed review. |
| T21.11.c | open | Exact focus/feedback and remaining workflow acceptance; T24 discovery review remains explicit. | Agent-run VM discovery task/results; no independent participant required |
| T21.12.a | open | Runtime and Studio share authored focus/selection for every component shape. | Resolved styles and actual item bounds |
| T21.12.b | open | Art/labels/context focus/decorative selection and reduced-motion state remain legible. | Light/dark/pastel production rendering |
| T21.12.c | open | Complete selection matrix and remaining acceptance pass. | Runtime captures and review |
| T21.13.a | done | Header/title/logo/clock/date/Search/Settings share validated selectable editable model. | [Combined workflow evidence](studio-workflow-completion-2026-10-31.md): G header validation/direct-selection and H actual Top/Bottom region placement, bounds/cancel/Undo through the shared model. Final persistence/installed acceptance remains .c. |
| T21.13.b | done | Layers/Edit header recovery, actions and permanent Settings/Edit route survive hidden/empty state. | [Combined workflow evidence](studio-workflow-completion-2026-10-31.md): F hidden title/clock/controls, actual Layers Header recovery, one Undo and exact settings field return; existing H permanent recovery. Installed .c remains open. |
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
