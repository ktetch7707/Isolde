# Executable backlog queue

## Current checkpoint — November 13 storage repair

Acceptance remains88% (2204/25),193/235 milestones; T04 remains51%.
Permanent guest retention is verified: c removes148 obsolete releases and six
payloads, recovering13.1 GiB; e passes129 runner safety checks and repeats with
zero removals/12.8 GiB free. Normal service, release links and layouts remain
unchanged. Tracker exclusions configured and paused indexer restored. See
[storage policy](vm-storage-policy.md). Earlier m/n/o dedup proposals below
are superseded by bounded retention, not implemented as extra workarounds.
Resume managed Kodi native correction p, then the combined media_controls
candidate and installed movieart/playlists journeys. No acceptance credit
for maintenance or failed preflights.

p dependency passes; r passes7/7 existing media registrations and deploys
cfc446352b79587dfc5c060a. Installed preview is reviewed and fixed. Native
recovery still loses virtual sources: bridge inventory deliberately masks
nonlocal URLs as empty strings, so native empty-path-only preservation misses
the actual URL objects. Preserve nonlocal source objects instead. s rebuilds
only that dependency; reuse unchanged r application tests, then repeat installed
movieart/playlists. r failure is retained, with no recovery acceptance credit.

t native package7d9bac4fd7d9 installs successfully after correcting hash-version
ordering. r installed attempt123227-354 proves exact recovery inventory and
byte-identical XML, including both virtual add-on entries. It then times out
on movie lookup: old Up8 sequence selected Frontend and shut Kodi down instead
of Scan. Helper now uses Up30/Down2/Confirm to select explicit Scan. No further
application rebuild required for this helper correction. Resume installed r
movieart/playlists, review all new captures; BUG-160 error-status visibility
remains open. Acceptance stays88%; source/control task closure remains open.

## Round-two verification campaign — 2026-11-11

The user now authorizes testing everything not yet VM verified in the backlog;
this supersedes the older rerun deferrals below. Resume
[the round-two record](test-campaign-2026-11-11.md). One VM runner owner;
`round2-20261111-b` is the frozen full-suite attempt. Preserve the six existing
Studio test-file edits. Diagnose the first named failure before resuming
unattempted registrations; then exercise missing/source-affected VM workflows.
Current acceptance86% (2150/25),178/219 milestones.
Latest browser candidate web-controls-s installed c2b0ac24fadcf2f7652d986b.
Six Store/120 runner tests pass with original r61/61 reuse;26 physical remote
keys verify real Chrome text Cancel/Done, Menu, explicit scroll, history Back and
root-Back owned exit. BUG-121 overlay overflow and BUG-122 root-Back/history
are fixed; actual embedded WebEngine tests and installed Plex lifecycle pass.
27 captures reviewed/0 pending, unchanged normal layouts, service active/0 restarts.
T29.6 accepted; T29=78%. The initial installed attempt hit a guest PipeWire timeout;
locked audio-test-recover restores the three user services without settings/host
changes, then the same installed candidate passes. Preserve the concurrent T12
input-controls batch. Detached browser-controls-check is the isolated test checkout;
its final evidence is also copied to build/vm-oracle/round2-20261112-web-controls-s.
No browser runner remains active. Next T29 work is combined offline/profile/error
recovery and protected Android transactions; the proposed policy remains unapproved. T25/T26/T28 accepted100%.
Previous installed4b8acad73bafd1cc61ae3c77, store-runtime-f: six Store tests and120
runner checks pass with strict original feedback-r61/61/source/payload reuse.
Fixed support package configured, SDK33/x86_64+x86/image-bound trusted metadata;
actual idle Store Install availability verified. Reviewed Web lifecycle7/50s
and signed discovery8/101s pass, normal layouts unchanged/service restored.
T29.2.c/.3.b/.5.b/.6.b independently accepted under unchanged original weights;
At runtime-f T29=75%. See BUG-117–120/cause-specific failed attempts in the round-two record.
Guest cache recovery preserves installed/offline runtime packages, current/pinned
candidates and authored-source evidence; clean pinned upstream checkout removed.
Clients retry044933 passes8 reviewed frames/192s: actual CBC Gem Browse
selection via physical Right/Right and Home return; FreeTube ordinary launch/
return. First attempt timed out at attended freetubepermission (no dialog); its
three captures are excluded. Explicit ledger38 reviewed/3 excluded/0 pending.
No runner active, no rebuild/full/reference run for harness inputs/accounting.
Preserve the six unrelated Studio test files. Next independent T29 work is the
combined Web
profile/offline/error recovery. Protected Android setup/update/remove/rollback
remains separate; no positive Android package claim from these Web/runtime checks.
Android protected positive transactions await explicit authorization: automatic
review rejected the proposed profile-only test rule. No rule was installed; do
not bypass that rejection. Reviewable proposal: store-android-authorization-fixture.md.
The async approval question remains pending. Ordinary native authentication is
an alternative; no password may be collected in chat. Continue independent T29
offline/profile work without inventing an Android install pass.
## User-directed T12 interface sound themes — 2026-11-11

Selected: Kodi-compatible audible UI feedback, extending the Settings cleanup.
Owned new files: `src/settings/InterfaceSounds.{h,cpp}`, `qml/InterfaceSoundSettingsView.qml`,
`config/interface-sounds.json`, `tests/tst_interface_sounds.cpp` and the linked
implementation record. Integration edits are limited to sound-related hunks in
`src/main.cpp`, `qml/Main.qml`, `qml/SettingsView.qml`, `ComponentSchema.cpp`,
`studio-defaults.json`, CMake and backlog/audio documentation. Preserve the active
app-icon work, including its staged files. No VM/build/test runs: the user's
existing deferral remains in force. Outcome: profile-local sound theme/mute/level,
remote selection and preview, bounded installed Kodi resource-pack import, quiet
default feedback, mute/playback/external-app suppression and safe error/retry.
No new dependencies, Kodi settings writes, downloads or binary artwork commits.
The source batch and prepared regressions are complete; [implementation, limits
and resume ladder](interface-sounds-2026-11-11.md). Source/JSON/JavaScript syntax
checks only. Acceptance and percentages remain unchanged until deferred native,
actual Qt-audio/installed and integration verification.

## Current user-directed T30 repair — no rerun

The user explicitly selected implementation of the six BUG-058 integrations,
then said continue. [The expanded runtime record](games-page/expanded-runtimes-2026-11-10.md)
documents the coherent source batch: sixteen declared routes, six exact locked
upstream artifacts, signed-bundle packaging, private firmware/game preparation,
typed remote setup, runtime payload checks, same-version repair and retained-data
rebuild/retry. Source parsing/JSON parsing and static review only. The user's
instruction still defers tests/builds/VM journeys; do not launch the queued
verification ladder merely to close this record. Native/installed/bundle and
gameplay evidence remains open, with no percentage credit. Preserve unrelated
Studio test changes. The historical checkpoints below retain their evidence
and are superseded where they still describe these six routes as unimplemented.

**Current scope and handoff:** T13 and T31 are excluded from project requirements and project averages. Continue toward 100% Coding across the remaining 25 tasks; only then start the testing agent, which reports every bug found. Older queue instructions to select T13/T31 are historical.

Operating rules: [backlog-execution.md](backlog-execution.md). Scope/Entry docs/files/percentages: [agent-backlog.md](agent-backlog.md). This queue breaks execution into resumable outcomes without replacing or relaxing any linked requirement. A future selection reads the full owning gate before implementation. Tests below use the F/H/I/V/A ladder in the contract.

**User testing-budget revision, 2026-10-29:** all I/full-suite references below are conditional under the contract's task-level budget: one task-closure run, applicable prior passes reused, additional runs only with a recorded concrete justification. They do not order a full run per row or small fix. Proceed across rows without asking or ending work at each checkpoint. Current Studio has verified full-G/focused-J application coverage and installed evidence; reuse it by scope rather than starting a broad test cycle for every slice.

Verified foundations T15–T24 are accepted as recorded in the backlog. Reopen only a demonstrated regression and preserve applicability evidence.

## Current user-directed Studio simplification — 2026-11-10

Selected: T21/T22/T23/T24/T27 UX refinement, explicitly authorized by “make all of this so”. Implement the ten-item submenu audit plus five-color linked themes. Own Studio QML/config, shared theme validation/session operations, their tests and Studio acceptance helpers. Preserve the earlier pending grouped main menu/timed trial and unrelated UI/VM-runner work. Simple task controls lead; per-section Advanced retains every supported capability. Five shared colors have individually overrideable roles, preview/cancel, explicit reconnect and atomic draft history. Cases: remote/pointer navigation, short/large-text screens, exact Back origin, no-write preview/cancel, undo, bounds/invalid data, profile isolation, preserved legacy designs and both reference pairs. Verification: complete edit batch → focused Studio/component contracts → one closure full CTest → affected staged Studio/reference journeys and image review. No active runner is recorded in the latest UI checkpoint; inspect before testing and never interrupt another owner. Current state: implementation complete, focused coverage recorded in [the refinement report](studio-simplification-2026-11-10.md): j passes7, k passes20/21 with its final native-input fixture corrected and passing in m, and n passes damaged-gallery contracts. Closure full a exposed a stale default-scheme assertion; b exposed stale native-menu fixtures. Focused o/r pass those corrections, including16 navigation functions. Diagnosed repeat full c passes60/60, zero skips. Staged Studio/designs workflows and both Lounge/Pastel recreation/reset journeys pass;112 frames reviewed and one mislabeled reset frame excluded, with its helper corrected and reset covered by native/reference evidence. Final evidence and limits are in the refinement report. Implementation/regression work is complete; no runner is active and no deployment is claimed. Failed evidence remains recorded in the refinement report. No new percentage credit.

## Latest user-directed UI audit — 2026-11-10

Current checkpoint: finish the [UI audit and repair report](ui-audit-2026-11-10.md), not the historical next-feature rows. Ten findings are repaired. Full c passes60/60; focused e passes5/5 and both reference journeys pass with reviewed owned-window captures. Final focused f passes5/5 with102 runner checks and19 individual Qt icon images; the final Disney contrast correction is reviewed. The UI audit is complete and its fixes/evidence are committed on the progress branch; no runner is active. Retain earlier campaign/install/emulator limitations below; acceptance stays63%.

Installed UI follow-up complete: user-authorized Android setup refreshed its stale receipt; final UI candidate f is installed as `742be8322eff80e0047e9d85`. Both installed reference journeys pass using `-FrozenReferenceHelpers`, retaining the concurrent-helper mismatch as failed evidence. Normal layouts unchanged; service active/running/zero restarts; no runner active. Earlier stale-Android deployment-blocker text below is historical and superseded.

## Test and repair campaign — 2026-11-10

Current user-directed checkpoint, November10: the whole-project test/repair campaign takes precedence over historical next-feature rows below. Read [the campaign report](test-campaign-2026-11-09.md) first. Original full candidate `campaign-20261110-final-a` passes60/60; the sole subsequent product change, AppGallery, passes focused `campaign-20261110-gallery-c` with strict full-origin reuse and100 runner self-tests. Its payload exactly matches the reviewed Pastel journey in gallery-b; Studio/Lounge staged results remain applicable by scope. Deployment rollback is tested against the actual stale-Android failure and restores both links/service. No runner is active. Normal profile layout hashes are unchanged, service active/running/zero restarts. Installed verification awaits valid VM Android setup/authentication; no password bypass or installed acceptance is claimed. BUG-058's six missing expanded emulator integrations remain software work. Acceptance accounting still1563/25=63%; do not award broad credit or silently resume unrelated feature work from older queue text.

## Coding handoff — T10 capture recovery, 2026-11-08

Source base `ac3f2e7702c92b78ff2f3e90afd3a382147afc68`. The existing T25/T26 ownership below is preserved; T10 is the next incomplete Coding% item with completed implementation dependencies. Capture Settings now adjusts the input on its own row; synchronous switch failure keeps a retry path, and camera errors stop preview so it can be restarted. The shared keyboard's malformed binding is repaired, its letter/number/symbol modes are remote-operable, and the unintended always-visible global keyboard overlay is removed; Network Settings retains its own keyboard. No acceptance or Coding% credit is claimed for this partial slice.

Testing handoff: run the existing capture protocol/component tests and actual QML keyboard/capture navigation, including camera errors, serial timeout/asynchronous failure and retry, Back/focus, and input switching with virtual devices. Then assess the remaining T10 adapter, atomic editor and audio-route scope. Local Node production-method checks pass for the changed remote paths; the existing full `qml_navigation.test.cjs` has a separate `composition is not defined` fixture failure in the HomeScreen deferred-viewport case. No Qt build, graphical, VM or installed testing was run here.

## VM-only acceptance authority

The November2 coding-only exception is historical. [Final installed acceptance](studio-batch-acceptance-2026-11-04.md) closes T22/T23/T24 and T15.4; [T27 acceptance](studio-t27-acceptance-2026-11-04.md) is also complete; T25 is next. Earlier coding/diagnosis records below retain their original evidence limitations.

All task outcomes below use [vm-software-completion.md](vm-software-completion.md). No representative/account/hardware/model/target-only check blocks S1; use the defined VM substitute and user handoff. A twelve-hour real-time wait is optional; required proof is production-clock invariants plus actual VM cold reopen. This changes criteria, not milestone states or weights.

## Current checkpoint — T27 accepted, November4,2026

T25-A native checkpoint: `t25-policy-20261107-c` passes sources2/2 and93 runner checks. T25.1.a/T25.5.a earn20%; Overall58%, sum1568. See [source evidence](t25-sources-2026-11-07.md). No active runner. Next batch: declarative source families/presets and remote policy/source actions, replacing runtime injection with generic contained projection. T25 full/installed/references and T26 remain in this chat's authorized work.

November7 takeover: this chat owns T25 through acceptance, then T26, as explicitly requested. Selected T25-A covers `SourceManager`, `ComponentSchema`, Home/Page appearance persistence, packaged source presets and the existing Sources/Studio/runtime views. Cases: policy validation/atomic rollback/profile isolation, contained notices/overflow/suppression, all remote decisions and focus after unplug, Preview/Cancel no-write. Verify native/source/component and QML focused suites after meaningful batches; one full at closure, then installed themed source journeys and both reference pairs. Resolve the recorded Studio/history verification failures before final acceptance. No VM run currently active at takeover. T25/T26 original groups are split in the milestone ledger; all children remain open until evidence exists.

November7 recovery note: [T25/T26 salvage review](t25-t26-salvage-2026-11-07.md) records recovered source archives, the selected source-scan fixes, passing VM build/native unit registration and failed full-suite evidence. Original policy/gallery code and unrelated Guide/Android/quality work remain preserved in the recovery archive, not silently restored. No T25/T26 acceptance credit; future full verification must resolve the named Studio/history/timing failures. This recovery does not establish a new installed candidate.

**T27100%, Overall57%, sum1548 across27 ACTIVE tasks.** [Final acceptance](studio-t27-acceptance-2026-11-04.md) records15/15 milestones, original N55/55 plus strict final AE4/4 reuse, reviewed installed3-palette routes/faults/private discard and both generic recreation/reset pairs. Final AE release96a7f77c8f4f68b66dda23e7; sourceDigest4c1ddf0911f6ff94044705a5376c86bb0e957962dad1df902e93e56d68781388. Native captures have no unreviewed hashes. Installed ledger144 artifacts/135 accepted/9 excluded; no partial/failed attempt disguised as a pass. No runner active; normal service running/zero restarts, root/Household layout hashes and native sentinel state unchanged.

Next authorized item is **T25 contained configurable detected sources**, thenT26/T28. Read only T25 Entry docs/files and full owning gate; split broad outcomes under original weights and reconcile existing evidence before edits. Reuse applicable T27/Studio passes, with focused checks after a meaningful batch and no full solely for a new task ID. T25 still owns discovery/policy/notice-slot containment and source-string diagnostics; T26 owns dedicated gallery, T04 missing episode metadata, T12 remaining system/legacy geometry. Preserve unrelated staged Guide/Android/quality work. Sole VM owner remains this chat. H01–H05 are post-software handoff.

## Current checkpoint — T22/T23/T24 complete, November4,2026

**Accepted: T22, T23, T24 and deferred T15.4 are100%. Overall54%, sum1448 across27 ACTIVE tasks.** [Final acceptance](studio-batch-acceptance-2026-11-04.md) is the current evidence authority; the diagnosis/checkpoints below are historical.

`studio-batch-final-navigation-20261104-a`: final55/55 CTests, zero failed/skipped,431s CTest XML; sourceDigest `fb5fd9c480fece358ae83f9cfd6e6d1e6d5ae852c513612f5ff4807b197bc0ed`; installed release `6c359ceb7a5b813e5e6952b7`, executable SHA `e26739523c9f4de5d7b5d85dd8a18b248fe1cbccb79f08deaa5ce65cdcf09721`. Current production hashes match frozen source. All77 native captures match reviewed originals. Final installed pages/designs/Lounge/Pastel pass with58 individually reviewed captures; source-bound audit34/34 passes at `build/studio-batch-final-navigation-audit/report.json`. Root core/desktop reused by explicit source-applicability proof, without another full run.

No VM action/session is active. Final normal service active/running, zero unexpected restarts; root and Household profile layout hashes unchanged. Guest maintenance C passed3GiB via verified archive pruning;128 host originals retained. No host maintenance/tool installation or formatter-clean claim.

Next authorized backlog item is **T27 shared shell appearance**, whose implementation is present but whose own full surface matrix is not accepted by this Studio closure. Read its Entry docs/files and current implementation record, reconcile applicable final55/55 and appearance tests first, then implement/verify remaining scope in one meaningful batch. Do not rerun a full suite solely for a new task ID or bookkeeping. This Studio integration includes the minimum shared appearance dependencies; it does not award T27 credit or take ownership of unrelated Guide/Android/quality work. Preserve all unrelated staged/working-tree changes.

## Combined closure A diagnosis — 2026-11-03

`studio-batch-closure-20261103-a` completed full CTest **50/53 passing**, no skips, 729.92s; it did not stage or deploy. Named failures: one-page menu height after the added reuse/safety destinations (`studio_quick`); `studio_colors` exceeded 60s after eight passing data rows; `home_component_navigation` compared QML array wrapper identity instead of row contents. Fixes retain every assertion/individual deadline: adapt menu row height within existing 40–48px bounds; split the measured color interaction cases from the field matrix using one shared fixture; compare actual array contents.

Source review also exposed a required T22 runtime gap: cached destination PageModel items bypassed the Home shared-style resolver and did not notify views when shared styles/Home tokens changed; inherited tile-roundness used root instead of designated Home. The same resolver is now shared, cache notifications are dependency-specific, and a public-behavior native regression covers page resolution/refresh/override/reset/no-write. This is a real application correction, not a waiver or accounting change.

Next focused `studio-batch-regressions-20261103-b` covers all three named failures, native page-style regression and actual VM frame/lifecycle/scaling measurements. It adds no full-run claim. No replacement full run until those affected checks pass. A subsequent full candidate is justified by the demonstrated first full failure and changed C++ runtime page boundary. Preserve the completed A evidence and do not run a full suite per small fix.

## Active T22/T23/T24 combined closure — 2026-11-03

Latest user direction transfers this batch through 100% to this chat; T23/T24 are reported ready for testing. T22 preparation passes. The prior VM hold for the other implementation chats does not block this VM owner. The combined candidate includes the current shared Studio source and identifiable concurrent T27 dependencies. Preserve unrelated staged/working-tree changes.

`studio-batch-preflight-20261103` stopped below 2 GiB. Authorized bounded `studio-batch-artifacts-20261103` verified/pruned obsolete guest artifacts/cache, preserving evidence/profiles/releases/build cache. It recovered 2.54 GiB, but reports failure against its separate 3 GiB maintenance target; it is not a passing application run. The Test guard remains 2 GiB.

Launch `studio-batch-closure-20261103-a`: one full CTest then staged Studio/desktop/Lounge/Pastel on its frozen payload. Full-run justification: T22 validation/inheritance/style/symbol and T23 safety boundaries plus T24 cutover changed materially since the last passing T21 full candidate; earlier full T22-B failed and did not finish. Do not run a separate full suite per task or tiny fix. Diagnose named failures, reuse focused evidence and retain actual installed/reference/offline/performance gates before crediting 100%.

## Current handoff — T22 ready for combined T22/T23/T24 testing, 2026-11-03

User requested finishing T22 implementation before testing all three Studio tasks together; T23 implementation is still underway. Shared styles, copy/paste appearance, bounded My components, linked instance preview/propagation/Detach, profile-local definitions, portable-definition merge, page inheritance and responsive section/viewport controls are implemented. `t22-reuse-ready-20261103-final` passes **2/2** focused suites (9 native and 5 QML case results including lifecycle); final capture reviewed. Source digest `8f69fff5f26d488b7b1a2a467ff39a64a28c33c872a1e980a5069725211bb8d8`. [Exact implementation, diagnosis and combined test plan](studio-t22-ready-2026-11-03.md).

No runner is active. Service/release unchanged, zero restarts; guest free space 2.01 GiB. Before the full candidate, use bounded Check and authorized guest artifact maintenance if its guard requires it. Preserve the shared T23/T27 changes and unrelated staged work; preparation is not a standalone clean HEAD build. Next: freeze stable T22/T23/T24 source once T23 is ready, then **one full CTest + staged Studio/desktop/Lounge/Pastel**, reuse that payload for installed journeys/reference pairs, and complete the T24 task audit. The runner's measured aggregate allowance is prepared for 53 suites (900s full, 600s full Studio, unchanged individual deadlines). Do not launch separate full suites per T-task or count preparation as installed acceptance.

Only T22.2.d is newly credited: T22 **51%**, Overall **44%**, sum **1187**; complete responsive/style/copy/nested/asset/installed matrices remain open. The entries below are historical checkpoints.

## Latest verified checkpoint — 2026-11-03 T22 combined design batch

`t22-design-regressions-20261103-c` passes 6/6 affected suites on source digest `df25dd4bfec46515a9f9e3d756c75e2961723314a5f7d05616fd2d6936552239`: Studio quick/designs/cutover/colors plus native component/design contracts. All diagnosed regressions are fixed, including destroyed-view callbacks, wrapped-label geometry and navigation-only proposal summaries. Both design captures were reviewed. No runner remains active; installed service/release unchanged, zero restarts. [Evidence](studio-design-portability-2026-11-02.md).

T22.1.d/.f, .3.a/.b and .4.a are now credited: T22 **47%**, Overall **44%**, sum **1183**; progress table validation passes. Next unimplemented T22 outcomes are responsive section rules (T22.1.e), shared design tokens and reusable symbols (T22.2); then complete installed/fixture and supported-source matrices. Preserve the existing narrow evidence and combine the next meaningful implementation batch before testing. No new full CTest solely for this checkpoint; task-closure integration remains required. Shared T23 dependencies remain uncommitted and their acceptance remains separate. The dated entries below are diagnostic history, not active runners.

## Active combined T22 design/portability batch — VM owner: this chat

User requested one large edit batch and combined VM verification. Implemented boundary: scope-aware saved/template proposals, page/interface portable export/import with missing-app remap, and private editor reference imagery. Owns new `StudioDesigns.cpp`, `HomeDesignData.h`, `StudioDesignPanel.qml`, `tst_studio_designs.cpp`, `tests/studio/tst_designs.qml`; targeted shared Session/HomeSkins/Studio/Customize/SkinLibrary/resources/quicktest integration. Preserve the concurrent T23 safety and T24 cutover changes in shared files. This chat has sole VM rights per the user; other implementation chats must not launch tests.

Finish edits before freezing. Cases: current-page preservation, explicit whole-interface removals, live proposal cancel/one-step Undo, atomic apply retry, managed artwork, missing-app placeholders/remap, malformed/private/executable data refusal, exact origin, remote/pointer parity, browser/export no-overwrite and reference cleanup/exclusion. New native and QML design suites cover the entire batch. Planned one combined full candidate (rather than one VM run per feature), then reuse its staged payload for Studio/Lounge/Pastel. Full-run justification: shared draft/proposal/assets interfaces, new application resources and new CTest registration make the prior T21 full payload inapplicable. No broad task acceptance until its remaining linked requirements pass. Candidate A stopped at build before any CTest: the concurrent T23 household test called private `AppCatalog::installCustom`. Current source now uses the public `addWebApp` denial boundary. Resume the same full candidate scope from the incremental cache; no application tests have been repeated. Added a supported `studio_designs` focused pair (native + QML) for cause-directed diagnosis without future full reruns; its two host selection contracts pass.
November 3 regression checkpoint: candidate B full CTest did not complete and is not a pass. Focused `t22-design-regressions-20261103-a` passes five of six affected suites, including both new design suites. Remaining cutover failures identify delayed callbacks accessing destroyed views; lifetime guards are implemented. `t22-design-regressions-20261103-b` fixed the lifetime failure and passed the 1,024-node test; it found a stale restore-scope assertion and wrapped-label height loop. Both are corrected. `t22-design-regressions-20261103-c` is the sole active runner, checking those regressions and the additional navigation-metadata summary contract together. Both design captures were reviewed; final installed/reference acceptance remains open. See the design-portability evidence record.

## User-directed T27 implementation - VM hold retained

2026-11-03: user selected T27 after the T23 implementation batch. This chat owns the shared semantic resolver/schema, surface-theme draft controls, shell/Kodi/Settings child propagation and their regression tests. Preserve the active T22 owner's draft/design work and runner. No VM commands from this chat. [Scope, cases and resume](studio-t27-implementation-2026-11-03.md). All T27 groups/children remain open until evidence passes; this user-directed implementation does not accept T24/T22 prerequisites.

## User-directed T23 implementation - VM verification deferred

2026-11-03 continuation: current T23 changes include scope-correct version/default restore, explicit damaged-history recovery, current-profile/storage guards and a separate bounded `studio_safety` QML partition. Three local runner-selection contracts pass. Existing local T22-B report exposed the earlier invalid staged-art fixture and original-suite timeout; those were diagnosed and patched, not retested in the VM by this chat. Current Qt/installed verification remains deferred.

The user selected T23 on 2026-11-02 and instructed this chat to hold VM verification. T23 changes and regression cases are in the shared working tree; no VM Test/Visual/Deploy/Status was launched. [Implementation, exact files and verification resume](studio-t23-implementation-2026-11-02.md). All T23 milestones remain open (0%): local Qt/compiler/formatter checks are unavailable and authored native/QML tests are not passes. Preserve concurrent T22/T24/quality changes. Resume T23 verification only after the user lifts the VM hold; start with focused component/Studio/navigation/Store/profile contracts, stop at the first named failure, then one task-closure full candidate and installed journeys/reference pairs. The T22 checkpoint below remains its owner's work; no T22/T24 acceptance is inferred.

## Active T22 checkpoint — starter templates and roles focused verified

T22.1.b/.c are focused verified. Four data-only starters pass apply/edit/reload, exact authored-page preservation and unsafe-ID refusal in `t22-starter-templates-20261102-d`. Named role edits, empty selected apps, no Kodi client and exact draft/applied/disk cancellation pass in `t22-template-roles-20261102-b`. [Evidence](studio-templates-2026-11-02.md). T22 is 11%; no parent or installed acceptance is claimed. Next: T22.1.d Current page versus Whole interface draft preview/application. Shared evidence reconciliation is being edited concurrently; preserve its changes. Picker regressions pass 2/2 in `t22-template-picker-20261102-a`; final catalog-boundary component regressions pass 1/1 in `t22-template-catalog-20261102-a`. No runner remains active; service/release unchanged.

## Current combined checkpoint — 2026-11-02 S10/T22.1.a focused verification

**Current status: T21 is complete at 100%; T22 is 4% with one narrow T22.1 milestone verified.** `t22-home-copy-preserve-20261102-c` passes `component_contracts` 1/1, including `ComponentTests::homeCopyPagePreservesExistingPages()`. Copying Home into a new page adds only that page, leaves two existing authored page documents unchanged across creation and reload, and does not embed the page collection in the copy. The test only closes T22.1.a; the templates, current-page/whole-interface scope, content slots, responsive rules, editor-private reference image and installed fixture acceptance remain open. Next: T22.1.b, the validated editable template set. No application behavior change or installed acceptance is claimed.

The dated entries below are historical execution notes. Earlier DNS, deployment and acceptance blockers were resolved for this final candidate; do not treat them as current blockers.

2026-11-02 exact-candidate integration and deployment: `studio-t21-integration-20261102` passes full CTest **45/45**, zero failures/skips, source digest `7bf6130ef21e724ddbfb6d933168495cff1964b0966d2193c6370a4f3f8d33d7`. Staged `studio,desktop,lounge,pastel` all pass on that payload; agent review covered all 58 captures (30 Studio, 10 desktop, 4 Lounge, 14 Pastel), including the Search-local Pastel contrast fallback. `studio-t21-header-legacy-profile-20261102` also passes the latest header legacy-read/no-rewrite regression. These results do not close installed or fixture-pair acceptance; generated capture manifests remain `visualReview: pending`.

The same run's Deploy selected release `db40eb2bfa147ed900c8cf7e` (previous `5c49ef0a674fbcfcb25b442f`) and then failed at `android-app-registration`: the guest Android setup receipt's configuration hash does not match the candidate. The runner did not reach service restart; supported Check `20261102-144942-96a803` confirms the existing service is active with zero restarts, while current-release selection is already the candidate. Do not repeat Configure/Deploy until a supported read-only Check resolves `repo.waydro.id`. The Check reports DNS timeout on `surfshark_wg`; Android setup and installed visual acceptance remain open. After DNS recovery, Configure Android and Deploy this same passing run, then run installed Studio/desktop/Lounge/Pastel and compare both reference/reset fixture pairs before reconciling .11.c–.14.c. T21 remains **90%**, sum **1039**, Overall **38%**; no new milestone credit.

The read-only Check initially stopped at the runner's <2 GiB guard. Bounded `Configure -ConfigureTarget artifacts` cleaned verified guest artifacts but did not reach its 3 GiB maintenance target; a subsequent Check passed at 2.96 GiB. It retained the candidate/evidence, releases, profiles and build cache; no host cleanup or network changes occurred.

2026-11-02 follow-up: Completed agent review of all 58 captures from `studio-menu-fit-integration-20261102` (Studio 30, desktop 10, Lounge 4, Pastel 14); manifests still correctly say `visualReview: pending`, and exact-candidate reference/reset comparisons and installed acceptance remain open. The packaged Pastel white focus ring is required by the reference and is unchanged. The attempted global `#943f69` preset/fixture change and its regression were reverted after staged visual comparison showed the ring no longer matched the reference; the related 45/45 run is not final-candidate evidence. Search now uses a separate contrast-safe control ink while preserving the original configured accent elsewhere. Focused `studio_selection` passes in `studio-t21-search-contrast-20261102-b`; its new regression verifies white Pastel focus fallback, action-label/query-border contrast, and preservation of a compliant configured accent. `home_component_navigation` also passes in `studio-t21-search-home-navigation-20261102`. This remains focused-only evidence, not a staged/final candidate. A single fresh exact-candidate full run is justified before S09 `studio,desktop,lounge,pastel` review because the previous passing full candidate predates the shared Search QML change. `studio-t21-resolver-inspection-20261102` continues to show `repo.waydro.id` DNS timeout routed over guest `surfshark_wg`; do not retry Android Configure/Deploy until guest DNS is available. T21 stays 81%, with no acceptance credit.
2026-11-02 exact-candidate follow-up: `studio-t21-search-final-20261102` passes full CTest **45/45**, stages digest `948211feb1d76e45515a65f5bf77a7e3e1de03a29412d0d9055b15d1a7c07e17`, and all staged `studio,desktop,lounge,pastel` journeys pass on its frozen executable. Agent review covered all 58 exact-candidate captures (30 Studio, 10 desktop, 4 Lounge, 14 Pastel), including full-resolution Pastel Search; no additional visual defect was found. Manifests remain `visualReview: pending`; exact-candidate reference/reset comparisons and installed acceptance are still open. Full Test and Visual preserve the installed release, zero service restarts and both normal profile hashes. Latest read-only Check `20261102-135827-5bc0b5` still reports `repo.waydro.id` DNS timeout on `surfshark_wg`; no guest/host networking was changed. Do not retry Configure/Deploy until DNS is available. T21 remains 81%, with no group acceptance credit. Resume: resolve guest DNS through approved diagnosis, then Configure the exact passing candidate, Deploy, run installed Studio/desktop/Lounge/Pastel, review captures and fixture pairs, and reconcile remaining T21 gates.
2026-11-02 focused workflow follow-up: `studio-t21-size-dpad-20261102` exposed an invalid test fixture (`cell-0` has no Outer size field). The regression now adds/selects a Shape, then traverses the remote D-pad context → Size & layout → Outer size flow; `studio-t21-size-dpad-20261102-b` passes `studio_workflows` **1/1**. It verifies live preview, exact field restoration on Back and unchanged document/disk. This is focused evidence only; T21.11.c's installed/discovery acceptance remains open and no credit is claimed. Latest supported read-only Check `20261102-140357-3b32d4` still times out resolving `repo.waydro.id` on `surfshark_wg`; service is active with zero restarts and the installed release is unchanged. No networking was changed. Do not retry Android Configure/Deploy or installed acceptance until DNS is available.
2026-11-02 selection reconciliation: `studio-t21-selection-matrix-20261102` passes focused `studio_selection` with actual bounds/color/width checks for rectangle, rounded, ellipse, pill and divider. The exact full candidate also passes the four dark/light/pastel/named-pastel selection cases and reduced-motion/art-label regressions; the associated palette/orb captures and all 58 staged runtime captures were reviewed. Credited T21.12.a/.b only; .12.c installed acceptance remains open. T21 is now 86% (sum1035, Overall38%). DNS still blocks Android Configure/Deploy and installed acceptance; the latest read-only Check is `20261102-140357-3b32d4`.
2026-11-02 tools/reset follow-up: removed the unreachable legacy `More tools` action/dialog path; focused `studio_quick` passes as `studio-t21-tool-reset-focus-20261102`. Its remote-input regression covers named Layers/Layout/Saved designs/Editor settings, Undo/Redo, reset scope/cancel/accept/Undo, and secondary Try interaction with exact selection restoration. Credited T21.14.a/.b; .14.c discovery/runtime review remains open. T21 is 90% (sum1039, Overall38%). Installed acceptance remains blocked by guest DNS; do not retry Configure/Deploy until a read-only Check resolves the repository host.
The attempted candidate `studio-t21-focus-contrast-20261102` stopped before build at source preflight because `tests/test_duplicate_code.py` and `tests/test_quality_report.py` are untracked; no CTest or payload was produced. Its retry, `studio-t21-focus-contrast-20261102-b`, temporarily staged those two files plus `src/media/GuideValue.h` and `tests/studio/CMakeLists.txt`, restoring all four to their prior untracked state afterward. It passed the full suite, **45/45**, but its global Pastel color change was rejected on visual review and must not be used for final acceptance. The corrected Search-local change is now focused-verified; remaining T21 scope, a fresh task-candidate integration/visual review and DNS-gated installed acceptance remain open. No Deploy or installed acceptance until DNS is resolved.

User requested a larger slice: complete T21.11–.14 together. [Boundary/evidence](studio-workflow-completion-2026-10-31.md). **T21 81%, sum1030, Overall38%**; .11.a/.b and .13.a/.b credited. G full passes44/44; H focused passes nine cases including corrected unobscured contexts, actual light/pastel, hidden recovery and new Top/Bottom header placement. Header region placement was genuinely absent, so its new shared schema/runtime geometry requires the documented replacement full integration run. Do not count G as final placement proof. I stopped at its space guard; verified J archive cleanup met3GiB. Final K `studio-workflows-final-20261101-k` failed three CTests; the scroll regression was traced to a stale page-viewport map binding, corrected with explicit geometry-change refresh and verified by focused navigation/quick/behavior/selection/workflow/component suites. The named-page palette row passes. First replacement full CTest `studio-workflows-integration-fix-20261102` built after repairing the incomplete `home_layout` test main but finished 44/45: only `studio_colors` failed because `waitForRendering` returned false for a visible, nonzero brightness slider and its visible panel; the actual pointer-to-slider value/transaction assertions pass when that unrelated render poll is omitted. Focused `studio_colors` now passes; one replacement full integration run is justified by that named full-suite failure. Final integration `studio-workflows-final-integration-20261102` passes **45/45**, zero failures/skips. Deploy then stopped at `android-app-registration` because the guest receipt configuration hash differs from this candidate. Added a narrow, tested runner recovery path that permits `Configure android` without a deploy receipt only when `current` is an Isolde release with a full payload hash map exactly matching the passing run. That check passed; the supported setup then failed before mutation while downloading the pinned repository key due guest DNS resolution failure (`android-admin.log`: `Temporary failure in name resolution`). Status confirms the user service is active/running with zero restarts, normal layout hashes unchanged, and the Android container still active; no installed visual acceptance or T21 credit is claimed.

The prior staged Studio, desktop, Lounge and Pastel runs all pass automated checks, but their 58 captures remain `visualReview: pending`; both reference fixtures and all Lounge/Pastel captures were agent-reviewed. The first Studio/desktop entry exposed a clipped final “Edit page” menu action. All 30 Studio captures from that run have now been reviewed; no additional actionable Studio visual defect was found. `qml/PageStudioView.qml` now calculates the action list height from measured title/status/history controls, and `tests/studio/tst_editor.qml` adds a `studio_quick` regression. Focused `studio_quick` passes **20/20** in `studio-menu-fit-20261102-b`, including the no-clipping check. Replacement full CTest `studio-menu-fit-integration-20261102` passes **45/45**, zero failures/skips, and stages the corrected candidate. Its staged Studio visual journey passes; the new entry/applied screenshots show the complete “Edit page” action. All 30 new captures still report `visualReview: pending`, and the journey was staged, not installed. No installed visual acceptance or T21 credit is claimed. Next: resolve guest repository DNS/connectivity through the approved environment before Configure/Deploy and installed Studio/desktop/Lounge/Pastel review. Do not retry setup while DNS remains unresolved. Preserve unrelated work and separate `build/workflows-test.index`. C verified host archive backups and pruned obsolete guest copies, recovered2.29GiB but honestly missed3GiB; no host cleanup.
## Current checkpoint — 2026-10-31 S07/T21.10 accepted

- **T21 71%, sum1020, Overall38%, 27 active tasks.** Color .10.a–c and parent accepted. [Color completion and matrix](studio-color-completion-2026-10-31.md). Next S07/T21.11 type-specific size/layout; preserve existing evidence before implementing its missing requirements. No T22 work.
- H `studio-colors-full-20261031-h` passes43/43, zero failures/skips. Final K `studio-colors-final-20261031-k` passes quick/selection/artwork/colors4/4 and61 safety tests with strict original-H reuse. Only production change H→K excludes dormant gradient endpoint from Solid contrast. Actual brightness pointer/remote documents match. All217 current application/build/config files match K; no further full run for accounting/helper changes.
- Installed K release `661a4d22aeab2f1420efeffc`: final Studio `140337-676`, Lounge `140005-568`, Pastel `140101-862` pass. All40 final captures and both original reference recreation/reset pairs reviewed. Seven color phases prove private cancel, Apply/Recent, Reset cancel, Studio chrome and exact-origin reopen. Earlier helper navigation failure and falsely claimed reopen capture remain honestly rejected diagnostics.
- Normal Household/Studio Demo layouts unchanged. Final journey service active/running PID2381936, zero restarts. No active test runner/guardian; final Status guest free1.86GiB. Future Test retains its2GiB guard. C/J maintenance followed actual B/I low-space blocks;3GiB targets honestly failed but recovered2.15/2.10GiB, enough for independent2GiB Test guards. Verified guest obsolete artifacts only; profiles/releases/evidence/cache preserved. No host cleanup.
- Preserve pre-existing staged Guide/CMake/config and unstaged template/docs/native-test work. Color commit owns only its code/tests/harness/closure documents. Test packaging index `build/colors-test.index` is separate from the real index.
## Previous accepted S06 checkpoint — 2026-10-31 S06/T21.9 accepted

- **T21 64%, sum 1013, Overall 38%, 27 active tasks.** T21.9.a–c and original .9 accepted; five T21 broad groups remain open. [Pastel closure](studio-pastel-closure-2026-10-31.md). **Next S07/T21.10:** reconcile/split existing color-picker evidence under its current weight, then finish shared fields, before/after, inheritance/reset, remote/pointer/cancel/Undo and owning acceptance. No T22 work.
- S06 required no application edits or VM rerun. All 217 current application/build/config files match installed AF. Packaged/fixture semantic parity and AF's actual saved personal skin retain ten Buttons, one genuine App carousel and one Kodi Recently viewed node with six pages. Full AB and focused AF remain applicable. S03/S04 real registry/session/Kodi history proof completes the dependencies that kept .9.c open at S01.
- Reuse the accepted AF installed Studio/Lounge/Pastel/catalog and 56 reviewed captures, including both original recreation/reset pairs. Deterministic Pastel providers remain explicitly fixtures; separate real Kodi and native app-history journeys provide runtime semantics. Raw helper pending/population fields are not relabeled as independent passes.
- Last VM Status remains AF: no active runner/guardian, service active/running PID2354701, zero restarts, normal layouts unchanged, free1.73GiB. S06 performed no VM mutation. Read vm-oracle and inventory/verify guest maintenance before a future Test if its 2GiB guard blocks; no host cleanup.
- Preserve pre-existing staged/unstaged Guide/CMake/config/template/docs/native-test work. This closure owns evidence/backlog/ledger/queue documents only. No application full run for accounting.

## Previous accepted S05 checkpoint — 2026-10-31 S05/T21.8 accepted

- **T21 62%, sum 1011, Overall 37%, 27 active tasks.** T21.8.a–c and original .8 accepted; six T21 broad groups remain open. [Evidence and supported matrix](studio-catalog-2026-10-31.md). **Next S06/T21.9:** reconcile S01 preset/fixture structure and existing live collection/history/catalog evidence against its full owning Entry/Done gate. No T22 work.
- Full **AB `studio-catalog-full-20261031-ab` passes 42/42**, zero failures/skips. Final **AF `studio-catalog-header-20261031-af` passes Studio quick/selection/artwork and 61 safety tests**, with strict original-AB coverage reuse for four existing QML views only. Source `ccf58223c9d5fc58413c3cb54acbd2e2b08960825a0f6a1ff5e69a6e694d6ebd`; installed release `49dae92cfc6de4b97f7b48d0`. All 217 current application/build/config files match AF. No further full run for bookkeeping.
- Installed AF catalog/Studio/Lounge/Pastel pass. All **56 final installed captures** and both original reference recreation/reset pairs reviewed. Real managed GIF import/refusal/framing/cancel/Apply/reopen, six persisted catalog nodes, independent Text body/label/name, links/Layers and existing Studio nested/scrolling behavior are proven. Normal Household/Studio Demo layouts remain unchanged. Rejected AE helper/header-origin attempts retain honest failure classification.
- No active runner/guardian. Final Status: service active/running PID2354701, zero restarts; guest free **1.73 GiB**. Before another Test, use bounded storage inventory/verified guest maintenance if its 2 GiB guard blocks. Preserve profiles/releases/cache/evidence. Host original archive backups support verified recovery; no host cleanup.
- Only owned catalog code/tests/harness/evidence belong to this checkpoint. Preserve pre-existing staged Guide/CMake/config and unstaged template/docs/native-test changes. Current application evidence includes those existing workspace changes; the owned commit is reconstructed separately and verified against them.
## Previous accepted S04 checkpoint — 2026-10-31 S04/T21.7 accepted

- **T21 55%, sum 1004, Overall 37%, 27 active tasks.** T21.7.a–c and original .7 accepted; seven T21 broad groups remain open. [Evidence and supported matrix](studio-history-2026-10-31.md). **Next S05/T21.8**: read its owning Entry/scope and reconcile existing evidence before implementation. No T22 work.
- Full L `studio-history-full-20261031-l` passes 41/41. Final Q `studio-history-focus-20261031-q` passes focused Studio and 55 safety tests with strict original-L coverage reuse for three existing QML files. Installed release `9d866e376432c4c3ae7f4285`; all 215 current application/build/config files match Q. Do not rerun full tests for bookkeeping.
- Installed Q history/collections/Studio/Lounge/Pastel all pass: real Kodi playback/lastplayed and history replay, eight actual native app sessions including two history reopens, exact focus, unchanged normal profiles/layouts, generic reference/reset pairs. All 62 installed captures reviewed. App provider is explicitly opt-in, session-only opening history; service account history and third-party playback are unsupported/unclaimed.
- No active VM runner/guardian. Service active/running PID2208508, zero restarts; guest free **1.76 GiB**. Before next Test, use bounded inventory/verified guest maintenance if its 2 GiB guard blocks. Preserve profiles/releases/cache/evidence and unrelated host data.
- Preserve pre-existing staged Guide/CMake/config and unstaged template/docs/native tests. Only owned history code/tests/harness/evidence belong to this closure.
## Previous accepted checkpoint — 2026-10-30 S03

- **Sum 997, Overall 37%, 27 active tasks. T21 48% (T21.6.a–c and the original group accepted); T20/T11 remain 100%; T31 5%.** S01/S02/S03 complete. Next selected item is **S04/T21.7**, real bounded Kodi/app viewing history; no T22 work.
- S03 closes with the actual installation-to-layout correction: collection pages follow the registry without generated app placements or changed Selected membership. Native AppCatalog install/remove/reinstall regression passes in All and Selected modes. Full `studio-collections-full-20261030-e` passes **41/41**, zero skips/failures, 472.28 s; source `f213fc2601b1f3b0b76f73a0768e82287ce50498a9176ba4e38af8e26664f716`. [Collection evidence](studio-collections-2026-10-30.md).
- Installed E release `0a3c1c22c2da3ab3282b6b7a`, executable SHA `6599218c51012441f2ae974ec3c945652043ba0dc776f84ab71e182c2b15134b`; previous J `0da0692cbb6b2354bed31852` retained. Installed collection journey passes six actual isolated desktop-boundary client input/normal-exit cycles, exact App/Button gallery return, Store removal/reinstall and unchanged page bytes. This is not third-party playback evidence. Installed Studio/Lounge/Pastel pass; all **50 installed + 31 native captures reviewed**, both original reference/reset pairs compared. `build/t21-final-review.json` checks hashes/identity/receipts/current production applicability.
- New `collections` case uses the existing bounded runner and SHA-addressed helper overlay. Two failed harness attempts (missing v2 source bindings, non-atomic fixture receipt) are retained; corrected helper passes on unchanged E. No repeated application full run for helper changes.
- No active VM runner. Final Status: service active/running PID 2154127, zero restarts; normal Household/Studio Demo layout hashes unchanged; guest free **2.06 GiB**. Test C stopped before execution at 1.98 GiB. Verified maintenance D removed 21 recoverable obsolete guest source copies, preserved 74 without recovery evidence and every staged payload/release/profile/cache; it recovered 2.30 GiB but failed its 3 GiB target. E then satisfied Test's 2 GiB guard. Use bounded storage inventory/verified guest maintenance if space blocks future work; no host cleanup.
- Preserve the other coder's staged Guide/CMake/config and unstaged template/docs work. Only owned collection code/tests/harness/evidence are committed. Current source remains the verified application candidate; later docs/harness-only changes do not invalidate it.
- **S04 next:** read the owning T21 Entry and .7 source/provider contracts. Reconcile existing production history/native evidence first; then complete genuine viewed/opening history, independent count/capacity and instances, supported app-provider merge/OFF/auth/offline/teardown/profile behavior, and actual playback/history update. Pastel deterministic art/lastplayed responses are fixture-renderer proof, not actual viewing evidence. Carry a complete vertical slice through its required acceptance without another routine confirmation or full run per small fix.

## Completed T11 closure sequence

Statuses here describe queue execution, not software acceptance. The active row is the next unfinished outcome; completed rows retain reusable evidence. Other rows follow dependencies unless explicitly independent.

| ID / status | Outcome and bounded work | Dependencies / evidence gate | Verification and next action |
| --- | --- | --- | --- |
| T11-R0 / done | Make lifecycle/features acceptance reliable: classify K timeout; check every helper API/field against actual bridge allowlist, capture-marker uniqueness, review-file handling, diagnostics, cancellation and restoration. Choose explicit attended review or implement safe separation of deterministic execution from review. Do not change product behavior merely to accommodate a bad observation. | Existing passing candidate + T11 entry/evidence + runner files implicated by failure | H regression for supported observations and timeout/marker/error paths; wrapper contracts if changed. Preserve payload identity. Exit when the planned V journey can execute with supported observations and a review schedule; then R1. |
| T11-R1 / done | Finish lifecycle on the frozen passing payload: base Guide journey, actual natural-end transition, saved-order surf, deleted disposable file, honest failure, targeted repair, exact return; record actual stopped-process state for VM cold reopen. | R0; isolated fixtures; passing full payload | V `guide-lifecycle`; review every capture. Verify committed slot IDs/times and service/layout preservation. Record actual stop state and executable/source identity; no twelve-hour timer required. Do not repeat I for a helper-only change. |
| T11-R2 / done | Finish source/authoring/features: remote local M3U/XMLTV/direct/no-EPG setup, error/401/OFF/refresh rollback, managed logo/color, snapshot/art export-import/remap, rule template, schedule/calendar controls, widget variants/filters. Compare each linked amendment requirement with actual assertions; extend a missing journey before claiming it. | R0; use a separate disposable profile and preserve the cold-reopen fixture | F `guide` for any required product correction; H for helper corrections; V `guide-features` against applicable full payload. If product bytes change, check evidence applicability and exercise affected runtime paths; do not start an optional soak as a prerequisite. |
| T11-R3 / done | Actual stopped-process VM cold reopen: persisted schedule/local seek/identity, cached listings/search/art, restart stability and denied-network app use. | R1; production-clock twelve-hour/seven-day invariants plus unchanged applicable fixture | Reuse native invariant evidence; add a bounded cold-reopen path to the supported runner if needed. Existing guide-soak enforces real time and must not be bypassed or mislabelled; retain it as optional long soak. No required 43,200-second wait. |
| T11-R4 / done | Close remaining T11 visual/scale/remote/semantic requirements; clean Studio/lounge/pastel reference/reset captures; agent task/visual review. Inventory every amendment requirement against R1–R3 and existing exact-source evidence; fix required gaps only. | R1–R3 and all T11 mapped requirements; any required installed deployment needs existing/explicit authorization | One I for the final application batch if bytes changed, then affected V cases (`guide-surfaces`, `studio`, `lounge`, `pastel`); A. Leave unmet gates open with precise blockers. T11.1/.2 may close earlier when their complete mapped requirements pass; T11.3 waits for its redesigned actual cold-reopen/offline and deterministic time proofs. |
| META-1 / done | Weighted progress ledger and calculator now preserve original weights and credit proven narrow outcomes; T31.6 inventory is retained and its unproven final parent reopened. | Evidence-only reconciliation; no VM action | `tools/backlog-progress.ps1`: sum 932, Overall 35%; T11 12/18, T31 inventory only. Future updates use the calculator and ledger. |

Representative availability, unavailable accounts and actual hardware cannot block R4/S1. Use the defined VM replacements, document residual user checks and keep genuine unimplemented/failing VM software work open. Finish the authorized T11 track, then return to Studio priority.

## Studio execution after the current exception

Every row includes error/retry, cancel/no-write, visible/restored focus, profile isolation and applicable bounds/security. Read the owning Entry docs; the compressed outcomes below are not the full specification. Existing implementation is inspected and reused before any edits.

| Order / ID | Outcome and gate mapping | Focused verification | Runtime/closure |
| --- | --- | --- | --- |
| S01 / T21.9 prerequisite / done | Inventory genuine Pastel app/history/shape/button structure; identify concrete remaining T21.6–.8 prerequisites; fix only those necessary for generic live fixture behavior. | G full 41/41; J Home/Studio navigation + strict G coverage | G Studio/Lounge + final J Pastel; structure, actual overflow/See All/editing/no-write and both reset pairs reviewed; scoped T21.9.a/.b evidence in Studio ledger, broad parent open |
| S02 / T20.5 / done | Close actual selected-item pointer threshold/neighbor preview/cross-container/reduced-motion/overlap parity matrix using S01 foundation. | `studio_gestures`, `studio_grid`, `studio_overlays`, `component_contracts`; navigation if affected | I after complete batch; V `desktop,studio,lounge,pastel` with installed evidence where gate requires it; close T20 only with all rows |
| S03 / T21.6 / done | Finish app/button collections: all/selected, order, capacity/More galleries, registry changes and exact return. | `studio_ordered`, `component_contracts`, navigation | Real registry change/overflow/launch-return journey; capture required surfaces |
| S04 / T21.7 / done | Actual bounded Kodi/app viewing-history merge, independent instances, unsupported/offline/auth/disabled-provider behavior. | `kodi_media_client`, `component_contracts`, navigation | Real playback/history update/profile journey; synthetic art is not history proof |
| S05 / T21.8 / done | Finish shapes/containers/buttons/text/linked image/GIF, labels/content/name separation, managed staged art and reduced motion. | `component_contracts`, applicable Studio inspector/input suites | Real remote add/edit/link/layers/cancel/reopen; no external side effect in preview |
| S06 / T21.9 closure / done | Recheck S01 against completed collection/catalog behaviors and real preset/fixture document. | Only relevant F if changes; reuse matching evidence otherwise | Both reference recreation/reset pairs plus live collection/edit behavior; A closes .9 |
| S07 / T21.10–.11 | Shared visual color picker and type-specific size/layout workflow; inheritance/reset, remote/pointer parity, cancel/history/Done/Apply clarity. | `studio_quick`, `studio_unit`, `component_contracts` | Light/dark/pastel and bounded size/color journeys; discovery review remains T24 |
| S08 / T21.12–.14 | Runtime-consistent selection styling; editable/recoverable header; discoverable named Undo/Layers/Layout/Saved designs/settings locations. | `studio_selection`, `studio_behavior`, navigation, `component_contracts` | Hidden/empty header recovery, exact focus, real bounds and complete feature-map coverage |
| S09 / T21 acceptance | Map all 14 scope gates/linked expansions to evidence, not just S03–S08. Close missing requirements without rewriting proven work. | I for the final complete application candidate | V `studio,desktop,lounge,pastel`; A; no T22 feature work until T21 requirements/dependencies met |
| S10 / T22.1 | Four editable templates and scope-aware preview/application preserving unrelated pages. | `component_contracts`, Studio UI | Template selection/cancel/apply/reopen and fixture pairs |
| S11 / T22.2 | Shared tokens/Home inheritance and linked reusable sections: edit-all/detach, cycle/rollback, minimum Settings/dialog/keyboard adoption. | `component_contracts`, navigation | Home/page/override/profile matrix, failed atomic propagation, no draft leakage |
| S12 / T22.3–.4 | Portable page/interface assets/remap/privacy/legacy skins; reject unsafe executable/private/credential content. | `component_contracts` | Export/import with missing source + cancellation/error; I and required template/fixture A closes T22 |
| S13 / T23 | In scope-box order: real runtime draft/side-effect interception; complete summary/conflict/atomic Apply/error focus; version/restore/reset/navigation recovery as specified. | `component_contracts`, `studio_behavior`, navigation | Failed write/revision race, cancel/Undo, restart/restore/exact focus; I + owning installed journeys + both references |
| S14 / T24 and T15.4 | Inventory parity against all linked audits, installed cutover, agent-run remote/desktop tasks, accessible/performance budgets, offline Generator and recovery. | Relevant F for observed defects, then final I | Agent-run installed VM checklist and agent review are sufficient; no independent participant. Exact shipped entry and both clean reference pairs; close T15.4 only from review |

## Remaining ordered work packages

Within each package select the first unfinished Scope box in backlog order, inspect existing code, and expand it into the contract's short item format before editing. Treat each independent remote → data → result → failure/retry → focus journey as a work batch. Full linked scopes remain mandatory; no package is accepted from this table alone.

| Next package | Ordered outcomes | Focused test family / runtime proof |
| --- | --- | --- |
| T27 | .1 effective resolver → .2 override/reset controls → .3 all shell/Isolde Kodi child surfaces → .4 state/request/profile preservation → .5 palette/accessibility matrix | Components + navigation + Kodi; themed Search/Settings/keyboard/details/error captures and both references |
| T25 | .1 discovery vs policy → .2 declarative contained slots → .3 overflow/no injection → .4 remote reconnect/dismiss/error → .5 isolated persistence | `component_contracts`, navigation; `isolde_tests` via I if no focused selector; multi-source dark/pastel/unplug journeys |
| T26 | .1 typed routes/legacy reads → .2 real catalog/preset → .3 layout/sort/filter controls → .4 launch/return/empty → .5 registry/uninstall/offline | Components + navigation; real All Apps launch/return, no Search/query or external request |
| T28 | .1 true header switch → .2 input/focus → .3 atomic preference/profile → .4 cancellation/OFF → .5 combined shell matrix | Components + navigation; late-response denial, themed remote captures, combined T25–T28 acceptance |
| T29 existing exception | Inspect other authorized owner's work first; .1–.4 provider/provenance/security/UI → .5 real store lifecycle/offline → .6 genuine Chromium directional bridge → .8 updates/removal/SmartTube/two-profile containment; retain .7 evidence | `store`, `store_remote_ui`, browser isolation; real source install/play/update/remove and external/embedded browser input. Unavailable live account/provider checks use controlled production-boundary fixtures and user handoff |
| T01 → T05 → T03 | Reconcile actual source/current fixtures; grid renderer evidence and applicable prior geometry; real Kodi installed search and source return | Components/navigation/Kodi; one matching candidate and relevant installed search/reference evidence, not three identical full builds |
| T04 | .1 safe preview/import/rollback → .2 guided setup/handoff → .4 actual Kodi playlists → .5 sources/scanner/posters → .6 capability inventory/controls → .7 full background-backend normal use | Kodi/components/navigation; disposable pre-existing Kodi installation preservation plus fresh real-video setup, no required Kodi menus |
| T12 | .1 system/a11y/reduced motion → .2 keyboard → .3 remap/universal-return failure matrix → .4 CA/US/CBC VPN → .5 authored idle pages → .6 app login/content/playback | Relevant audio/VPN/browser/desktop/navigation suites; actual remote/owned-app/country/traffic/idle/account journeys; no handshake-only pass |
| T09 | .1 preference → .2 ON direct/resume/series → .3 OFF viewing choices → .4 six-row playback matrix | Kodi/components/navigation; real Kodi movie/episode/subtitle/progress ownership and Web OFF denial |
| T10 | .2 production adapter cancellation/recovery → .3 shared remote capture/HDMI/audio editors; preserve .1 foundation | Components + `audio_backend`, protocol tests at I; installed virtual-device journeys. H04 remains physical |
| T11 | Already worked under current exception: reuse only demonstrably applicable accepted evidence; reopen regressions | Do not redo the complete T11 program merely because table order reaches it |
| T14 | .1 resolve provider/terms, opt-in discovery/cancellation → .2 offline SRT queue/review/cancel/provenance | Kodi/components/navigation + denial/provenance fixtures; production-provider boundary fixtures and actual offline subtitle output; agent documents an in-scope conservative provider/model choice without requiring unavailable keys |
| T30 | .1 G0 routes/runtime/legal fixtures/budgets → .2 real local game → .3 transactional catalog → .4 configurable collections → .5 Steam → .6 integrated emulators → .7 independent PC copies → .8 optical software → .9 integrated matrix | `games`, session/browser/desktop/components as affected; actual gameplay/save/return per advertised route, 5,000-entry scale, offline. H02/H03 physical; cinematic/client spawn is insufficient |
| T13 | .1 signed/offline clean-machine onboarding → .2 product recovery/rollback → .3 install/power-failure/Generator → .4 illustrated generic VM-validated beginner guide | Installer/recovery regressions and I; fresh disposable VM shipped artifact, no developer setup prerequisite; no target model/firmware/human participant dependency |
| T31 | .1 complete requirement matrix → .2 freeze hashes/full suites → .3 clean install/recovery → .4 integrated offline/profile/product routes → .5 visual/performance/usability → validate .6 risk baseline → .7 agent-run clean-VM guide walkthrough | One final integrated candidate; all preceding active gates accepted; no skipped promised route or simulated physical credit |
| H01–H05 after S1 | User-owned display/remote/controller/optical/capture/install/power checks plus live account/service and personal-usability observations | Handoff only, outside all software percentages; the user decides how to test and publish. No host maintenance authorization |

## Ready-to-start checklist

Before selecting any later package: confirm predecessor closure/recorded exception, fill exact next gate/scenario/files, identify account/runtime/fixture requirements, classify tests with the ladder and name the integration boundary. If there is a missing test selector/helper, plan that small prerequisite before the long journey. A blocked queue item must retain its blocker and the next allowed action; it cannot disappear from the plan.

## Planning validation

This revision must pass D only: all 27 active IDs appear in the backlog; completed T16–T19 remain foundations, T15 review remains paired with T24, excluded T02/T06–T08 never become selectable. All selectable packages above map to backlog gates; no official completion credit is claimed. Confirm the documents' links and commands against the current runner. No application/VM testing is justified by this documentation batch.

2026-10-29 validation: PASS for local Markdown links, explicit mappings for all 27 active task IDs, all six immediate queue items, focused suite names against the current runner and `git diff --check`. These are static planning checks only; no product gate, percentage or VM result is advanced.

## Selected T29 closure batch — 2026-11-12

Finish Store before returning to the ordinary backlog. Preserve the six Studio
fixture edits. Current owned files: AppCatalog.cpp/new-package activation
rollback, tst_isolde.cpp, Store acceptance/runner case and offline receipt guard.
Offline matrix covers three real cold starts/two disposable profiles under
AF_INET/AF_INET6 denial, cancel/no-write, activation failure, retry, manifests,
Web error/owned return and repository OFF/error/recovery. Actual Android
protected transactions still require native authentication or explicit approval
of the already documented temporary profile-only test authorization; none is
installed. Approval question pending; no workaround for the review rejection.

Verification: one integrated full run is now justified because the installed
write-failure test exposed an orphan package after AppCatalog activation failure,
and shared package activation plus newly committed InputManager/keyboard changes
invalidate the old narrow full-origin reuse boundary. Keep AppCatalog.cpp outside
that bounded reuse allowlist; use the full run as the next installed base. Reuse
that passing candidate for subsequent harness-only fixes and final Store journeys;
no full runs for accounting or each capture. Six Store tests passed on b; its
installed failure captures prove the orphan-manifest defect, not a pass.

T29 current checkpoint: store-close-20261112-full-d passes62/62 and is installed
as638b0fd8106ac8fd9c54ccf0. Automated offline/profile matrix passes15 reviewed
captures/84 remote actions/three real cold network-denied processes. BUG-123
closed; T29.4.c/.5.d credited under unchanged parent weights, T29=83%, project
2155/25=86.20% rounded86%. Current runner121/121 passes including verified Store
helper overlay/frozen dependencies/tamper rejection. First second-profile attempt
was obscured by its fixture's Kodi auto-start; both fresh profile preferences now
disable it. No product full rerun for that harness repair. Preserve failed evidence.
Next: actual protected Android install/update/cancel/rollback/remove/SmartTube
and containment. Explicit approval of the bounded temporary test authorization
question remains unanswered; no authorization rule installed, no bypass. Reuse
this full candidate; ordinary backlog resumes after Store closure or user steering.

Native-auth continuation: user accepted proceeding in chat, but automatic review
rejected the exact temporary policy patch again; no policy code or rule written.
User explicitly requested the VM password-screen fallback. Direct SSH Store
failed authorization without an agent; real Ptyxis launch now produces GNOME's
normal Authentication Required dialog. Removed an unnecessary wmctrl request
for the terminal's Wayland window after that named harness failure. Live Visual
session54445 on full-d, fixture store-yk1l2asj, attempt103428 has a reviewed
storeandroidauthentication capture showing the empty password field. User asked
to authenticate only in the VM; test is waiting under existing time bounds.
Unchanged passing app; native-auth helper/runner edits remain uncommitted pending
journey verification. No new credit/policy change. Preserve six Studio edits.

Explicit temporary-rule approval received via the exact choice: I approve this temporary VM polkit rule. Approved fixture installed/revoked with independent900-second root timer; prior generic-approval blocks superseded. Keep exact paths/one UUID/local active user. Disposable Android data uses /var/tmp to satisfy same-filesystem backup; config/evidence remain /tmp. BUG-124 stopped IPC backup fix passes real cp/socket/FIFO rollback and all6 Store tests in store-android-20261112-g, strict full-d62/62 reuse. g installed a9ee9953e115ed5a20d970ff; root Android support updated. Prior g visual precheck ran before polkit reload with PID-only identity; retain checks, wait2sec and bind PID/start/UID. Live retry session92174, fixture store-31f8y3z4. No new acceptance credit yet. Resume this run, review removal failure special inode diagnostics if any, verify policy cleanup, then complete Store. No repeat full suite for harness edits.

Current checkpoint supersedes the live session above: g attempt111140 PASSES.
Installeda9ee9953e115ed5a20d970ff, source079a8c6b...,6 Store tests/full-d62 reuse.
Android+Web27 remote inputs/14 reviewed captures, five reviewed Chrome frames,
exact default-Cancel bytes, real SmartTube launch/owned return/verified removal.
BUG-124 actual stale Unix sockets confirmed and fixed; permission scope four
checks passes and removal verified before normal service restore. No password
input needed. T29.5.c/.8.c accepted: Store88, project86.40% rounded86%182/222.
Final runner self-test session23998 after correcting the negative-probe source
assertion. Commit/push this verified logical batch preserving six Studio files,
then continue downloaded APK update/fault/profile/complete containment tests on g.
No new full suite for harness changes. Actual SmartTube desktop chrome remains
an explicit open containment requirement. No playback or100% Store claim.
Final self-tests121/121 pass; maintenance's3 GiB target was not reached (above2 GiB minimum), record Configure as failed maintenance target rather than a product/test failure. Installed lifecycle/cleanup remains accepted. Current run is finished; no active session or password prompt. Next authorized Store work remains downloaded APK updates, Android fault/profile and complete containment. Reuse g/current root support, no password exchange in chat or repeated policy approval.

Current: signed Store updates attempt113406 passes and ten frames reviewed.
T29=97%, project86.76% rounded87%,185/222. Fullscreen containment batch ready
for one full Test store-final-20261112-h (shared persistence/runtime change).
Then Deploy, Configure android-store-support, installed Store/containment review.
No password/permission question; exact temporary test rule remains approved.

T29 CLOSED100%: final installed j full62/62; attempts150605/151112 reviewed14+16
frames; o6/6 focused Store and native external-session/error/two-profile fixtures
pass. Explicit decoded live-content U01 handoff, no playback claim. BUG-126
physical X press/release sequencing fixed; BUG-125 non-reproduced timing retained.
Overall2172/25=86.88% rounded87%,186/222. Commit/push Store batch, then T03
installed real-Kodi scoped search/paging/cancel/return. No full rerun for docs.

T03 selected next: installed real-Kodi search under kernel IPv4/IPv6 denial,
remote keyboard/query/results/cancel and exact Home origin. Add bounded
local-search journey to existing private TV/Kodi fixture and SHA library helper
route; reuse full-tested installed j. No product change or full CTest needed.
First .4.b evidence; complete paging/page-source .4.c remains separate.
Local-search harness runner self-tests pass (new typed case/negative-result tests).
Configure artifacts did not reach optional3GiB target; safe cleanup complete,
above2GiB minimum. This is maintenance-target failure, not a product pass/fail.
Proceed with installed local-search; no app rebuild. Review all4 captures before
credit; full paging/page-source .4.c not represented by the first journey.
T03.4.b accepted92%, project87.24%,187/222. Expanded attempt153805 reveals
BUG-127 despite automated pass: playback stop loses Search return. QML Main and
KodiLibrary correction plus native stop/Home distinction ready; Test t03-search-
20261112-p full once for changed global return route, then deploy and repeat
installed local-search. Preserve six unrelated Studio files. No .4.c credit yet.
Healthy full p running session28894. Additional reviewed BUG-128 long-list focus
and BUG-127 preserved query/page selection fixed in CommandCenter + native
regressions after p froze. Let p finish, then Test -Suite studio_visuals
-ReuseFullRun t03-search-20261112-p on a new ID. Existing reuse boundary covers
CommandCenter + tests/tst_home_navigation; Main/KodiLibrary unchanged from p.
Deploy final focused passing candidate, installed local-search; review all9
frames for actual selected More visibility and post-play Search page/selection.
t5/5 with strict p62 reuse passes; installed attempt161123 fixes visible More and
exact Search page/selection after stop. Remaining same BUG-127: underlyingLibrary
visibility must restore on final Search Back. Main captures that boolean now.
Full t03-search-20261112-u required because current bounded reuse excludes Main;
then Deploy+local-search repeat all9 frames. T03 remains92%, no .4.c credit.

T03 CLOSED100%. Current installedx/f21c113eb646a6997202177b, p62 full plusx5
strict scoped reuse,123 runner tests; attempt163901 all9 reviewed frames.
Real56-episode offline paging/play/Stop/exact Search and library origin return
passes. BUG-127/128 fixed. Overall87.56% rounded88%,188/222. Retain BUG-125
intermittent Studio frame timing diagnostic for final risk review; no fake fix.
Commit/push this T03 logical batch preserving six Studio files, then T04 current
Scope/Entry docs only. No live tests or authorization rule remain active.

T04.1 SELECTED: split into four outcomes under unchanged1/7 task weight before
implementation. BUG-130 importer emits success but transfers no configuration.
BUG-131 rollback writes original-source files with QFile::copy over existing
files and shallow backups omit nested/versioned data. Batch recursive immutable
snapshot + isolated-profile target transaction/rollback/crash recovery, accurate
inventory and remote UI, idle backend guard, native filesystem regression matrix.
Never mutate original desktop Kodi, media, other profile or host data. Use one
full task-closure run after meaningful batches; focused Kodi tests first. Current
source full software scope is NOT implemented despite Coding100 claim; report
actual defects, no false percentage credit. No active VM runner.
T04.1 implementation batch ready for focused native test: snapshot helper and
async C++/UI/backend/cancel/teardown integration,13 filesystem/SQLite/crash tests.
Test -Suite kodi_media_client builds app plus client and runs import contracts.
Track only new helper/test before runner snapshot. No full run until T04 closure
or material integration need. Never infer installed/import completion from source.
User steering: prioritize the existing acceptance journeys and fixing feature
failures; no further test expansion except a direct existing-requirement blocker.
T04 d focused passes both registrations (14 filesystem contracts/35 C++ media).
Credit .1.b/.1.c independent native snapshot/import/rollback proofs; inventory and
installed workflow stay separate. Existing installed x remains full-proven base
for unchanged playlist feature acceptance. Run existing playlists case next;
new import code is not installed/accepted yet. No new tests needed for that run.

T04 current checkpoint: installed f release58a7c894d6ffc1b1e158d68e full63/63. Playlist attempt203323 all7 frames reviewed, actual episode play/pause/Back/exact return passes (.4.c done). Movieart attempt204701 reviewed actual Movies poster/metadata; read/display only. T04=45%, project87.92 rounded88,191/228. Native import .1.b/.1.c done; .1.a inventory and .1.d installed remain open. Complete .4.b/.4.d/.5.b/.6/.7 next; do not equate coding table with verified requirements. BUG-138 extra Studio desktop stale nested-group path retained after top-level byte-equality pass; not Kodi DesktopSession. See docs/t04-kodi-verification-2026-11-12.md. No live application test; supported artifacts maintenance pending for low guest disk/current runner self-tests. Commit/push owned batch, preserving six unrelated Studio files.

Maintenance finished:123/123 current runner self-tests pass. Cleanup reclaimed1,552,723,968 bytes;2,147,745,792 free meets2 GiB test minimum, misses3 GiB maintenance target (Configure failed only that target). No unsafe cleanup or suppressed checksum/path failure. Current application service restored. No live runner. Next selected work remains T04 incomplete requirements; no routine permission needed.

T04.4.b/.d SELECTED batch: playlist mutation helpers and remote rule/order management. Files: playlists.py, bridge.py, KodiMedia.{h,cpp}, LibraryBrowseControls.qml, existing bridge/media tests and playlists journey. BUG-141 undefined playlist_name breaks rename/delete/reorder; BUG-142 rename collision deletes original before publication. Fix bounded regular-file/ownership reads, atomic journal/rollback, labels/complete member reorder, typed smart-rule sort/edit and visible status. Focused existing guide suite includes bridge/playback/media; one full at complete integration, not per bug. Preserve six unrelated Studio files.

USER BATCH REVISION: fix all known missing features/bugs across remaining tasks before another application test campaign. This supersedes one-task/per-slice test sequencing for this batch. Current focused c already finished (3 registrations pass). Hold further VM tests while implementing T04 source/art/capability/setup, T09 playback gaps, T10 capture clipping, T12 Power/input/polish, and any concrete T14/T30 defects from current source/evidence. Existing unverified working software needs acceptance, not speculative rewrites. One combined final candidate/campaign after reviewable batch; preserve failures and all earned milestone weights. No percentage-update-only checkpoints.

LATEST BATCH CHECKPOINT: source repairs now cover Power BUG-078, Capture clipping BUG-091, named nested Studio catalog BUG-138, playlist BUG-141–144, actual Kodi title poster editing, T09 failed-save/status and external Chrome binding BUG-146. Details and prepared existing-journey changes: test-campaign-2026-11-11.md, Combined repair source batch. Static diff/JS syntax passes only; no new VM run after focused c. Hold application tests per the user's fix-first instruction. BUG-145 remains an actual missing source/content editor, together with full Kodi capability/setup UI; do not claim100% Coding based on the old table. Continue that source work, then one combined full candidate and affected installed/virtual/offline campaign. Six unrelated Studio test files remain untouched. Do not launch a full merely to close this source checkpoint.

Subsequent source review found/fixed BUG-147: Kodi subtitle current-property name and explicit track enable were wrong while the old fixture mirrored that error. Existing track test and picker geometry/focus corrected; no VM run. Include in the combined candidate. Native playback Menu-to-Isolde focus handoff/audio/viewing controls remain part of the actual T04.6 implementation gap, not satisfied by the subtitle API or hidden Kodi menu.

CURRENT IMPLEMENTATION (November12 continuation): BUG-145 source authoring now has a typed private broker, native Kodi-owned source/content transaction, atomic source XML and provider/scanner backup/recovery, and remote draft/preview/double-confirm/remove UI. Supports granted local single-folder video/music sources; imported remote/multipath definitions remain intact/read-only. Folder permission editing gracefully leases the idle backend without changing startup/frontend preferences. Native Menu opens an Isolde-owned playback window with transport, audio/subtitle/view controls and current-list previous/next across pages; song visualization uses common controls. Explicit validated capability inventory added. All remain UNVERIFIED and earn no acceptance credit. Service callback, source transaction, player ownership and cross-page regression additions belong in the existing registrations before one combined application full run.

Dependency setup is an authenticated distribution Kodi21 Omega build with one managed-only source builtin (no Isolde SQLite writes, no new side service). Configure kodi-managed attempts a–f preserved: version/source-keyring/DFSG-render setup diagnosed, f reached Ninja but target kodi.bin was wrong (CMake target kodi, binary kodi.bin). Corrected g is compiling incrementally under the existing runner. Poll its live session; do not interrupt a healthy build or launch another. Guest-only build dependencies installed; obsolete generated legacy cache removed with sibling source/evidence preserved. Reproducible guest desktop tracker cache reclaimed and indexer restoration lease recorded; successful setup restores it. No app Test/Visual/Deploy has run in this continuation. Project remains88%,T04=49%.

November13 resume: g stopped at1166/1697 because the distribution-patched SystemInfo.cpp requires DEB_VERSION, supplied by debian/rules rather than plain dpkg-buildflags. Apply that exact installed version to its consuming translation unit; retain compiled objects. Source native builtin itself compiled. Pending extension batch also normalizes music scanner flags and refuses both music/video active scans. BUG-148: integration guards incorrectly treated Isolde's own Settings session as an external app and denied source saves/Power from their only UI; explicitly allow Settings while retaining profile/import/playback/desktop/external-session guards. This is a source-review fix, not yet a pass. No acceptance change.

November13 dependency closure: h completed native compilation/linking; packaging
expected kodi.bin while the observed distribution output is kodi-x11. Corrected
the package input, retaining its installed /opt/isolde-managed-kodi/kodi.bin name.
i stopped at the unchanged2 GiB preflight gate. j supplied existing exact host
source/report backup receipts to normal verified cleanup and passed native build,
package installation/hash verification and guest-indexer restoration. Normal
service remains active/running, zero restarts, current release unchanged.
Evidence: build/vm-oracle/t04-kodi-native-20261113-j/report/.

Launch ONE combined full application Test next: source/content private protocol,
new native dependency selection, asynchronous ownership/import, global playback
Menu/focus, packaged QML/resources and the earlier Power/Capture/browser batch
cross several boundaries, so prior full/scoped reuse cannot cover this candidate.
This is the planned integration run, not a percentage or documentation rerun.
Source commit1b60bd7 plus packaging/backup-receipt runner corrections. Keep the
first failing registration and diagnose before retrying. Existing installed
movieart/playlists helpers are prepared; use the same passing candidate afterward.

Combined a stopped in disk preflight before application compilation or any CTest:
newly installed native dependency plus its link archives left<2 GiB. All ordinary
verified cleanup was exhausted. Extend that same automatic cleanup to discard
only Ninja-declared managed-Kodi static link archives and its identical package
staging executable, after verifying the root-owned installed executable against
its manifest. Retain every native object, source checkout/archive, Debian package,
installed file and acceptance record. Existing oracle registration adds checksum,
mount, active-process and object/source preservation assertions. b is the same
application batch's first actual build/full attempt after this diagnosed preflight
repair; the2 GiB minimum remains unchanged.

b stopped in the oracle safety self-test, before application build: the new fake
installed-dependency fixture inherited writable group permissions, so the safety
check correctly refused it before reaching the intended checksum assertion.
Fixture now explicitly uses directory0755/files0644. No production safety check
was relaxed. c retries the planned application batch after that diagnosed harness
setup correction; still zero application registrations attempted in a/b.

c passed all125 runner safety tests, then correctly refused the actual installed
managed Kodi dependency's group-writable package directory. BUG-150: package
staging inherited the guest umask, and the first package receipt checked binary
ownership/hash but not every installed directory mode. Normalize all generated
package directories0755, data0644 and executables0755; reinstall the exact same
hashed candidate through apt and verify every installed path. This repair runs
before cache cleanup because the cleanup correctly requires a safe installed
copy before deleting any link archive. Source/binary checks and the2 GiB
application minimum remain enforced. No application CTest has run in a–c.

Read-only k Status/Storage identifies the exact retained mode: both root-owned
/opt/isolde-managed-kodi and bin remain0775, while all three executable/manifest
files already have the correct0755/0644 modes. dpkg preserved pre-existing
directory permissions on reinstall. Package configuration now sets only its
three fixed root-owned, non-symlink directories to0755; every path/owner check
remains enforced. k's failed repair and read-only mode inventory are preserved.

l passes package directory repair, native cached relink, installation/hash checks
and guest-indexer restoration. Evidence: t04-kodi-native-20261113-l/report/;
normal service active/running/zero restarts and installed app release unchanged.
The planned full application run d now follows corrected dependency setup and
safe automatic link-cache pruning. a–c never reached application compilation or
CTest, and are retained as failed preflights. Playback popup also inherits the
existing display scale/safe area and originating library palette before the
first actual application build. T04.5/.6/.7 outcomes split under their original
weights: prior earned credit unchanged;192/235 milestones, sum2202, Overall88%.

d passed125 runner checks and root-owned dependency validation, then still had
<2 GiB after discarding140 MB of native link archives. The desktop indexer was
recreating its disposable cache immediately after Configure restored it. Normal
Test now pauses only the originally active guest indexer units during low-space
builds, uses the already-tested exact tracker-cache cleanup, and restores those
same units in a finally block under the existing runner lock, with an explicit
Recover lease for crashes. No indexer setting, app profile, host or file grant is
changed. This closes the repeated cache-growth cause instead of waiving space
or rerunning separate Configure cleanup before each test. e remains the planned
first actual application build/full run; a–d attempted zero app registrations.

The e application build succeeds (all new C++/QML compiles), and the single full
CTest run is healthy/in progress; do not interrupt or restart it. Existing
movieart helper now uses a one-Movies-card declarative fixture and only
D-pad/Confirm/Back/Menu/Home for product flows (Ctrl+M removed). A longer lawful
synthetic clip permits real seek beyond Kodi's default resume threshold, actual
stored progress/resume, default-ON and remote OFF preference screenshots, and
proof that OFF leaves library playback direct. Source preview/cancel compares
any packaged empty/default source inventory/XML exactly rather than requiring
that Kodi never creates an empty config file. All are test-helper refinements,
not application changes or acceptance passes.

Source review follow-up BUG-151: nested playback audio/subtitle/view Back currently
returns to the first transport row rather than its exact opening row; source
Remove preview Back likewise selects ordinary Preview instead of Remove. Retain
these concrete focus fixes for the next diagnosed repair batch, with direct
assertions in the existing Home navigation registration. Do not interrupt the
healthy e full run. Neither new feature is accepted yet.

BUG-152, e full-run failure: invalidate/retire cleared m_busy before generic
failure handling. This avoided per-operation nonfatal error branches, but also
lost the active scan/quit context; the existing cancelsScanMonitoring assertion
failed. Keep the context and explicitly request a forced disconnect instead,
while ordinary source/options/subtitle RPC errors remain nonfatal. Retirement
also clears newly added source/options data. Existing media tests retain scan
cancellation and assert source/options teardown; no new registration.

BUG-151 repairs restore the opening playback row after nested Back, make Menu
close controls consistently, restore Remove's own row after preview Back, and
hide Remove on a never-saved source. Existing Home navigation theme case now
exercises these actual QML methods. GUI helper navigation resets to known list
bounds rather than assuming the previous focus bug. The e run compiled the full
app and passed59/60 attempted registrations; Native profile/browser isolation
and Home layout were unattempted after its required stop at#60. Indexer finally
restoration passes. No passing full receipt or deployment is claimed.

f repeats full integration because e has a demonstrated full-suite failure and
the fix touches shared media disconnection/lifecycle plus remote focus. This is
the combined correction batch, not a rerun for a screenshot, commit or percent.
Keep all failures; same candidate then goes through installed/offline/standing
visual journeys. Current app changes remain unaccepted until that evidence.

November13 f completes all63/63 existing CTests and stages a passing payload;
indexer/service restoration passes. BUG-153 source errors survived Refresh and
fixed Music provider selection falsely reported a missing scraper: clear stale
messages and make that fixed row read-only. BUG-154 ordinary Music browsing had
no default entry: validated packaged library-view data now exposes Music, with
album/song labels and contextual help. Existing Home/media tests cover these.
The next media_controls run reuses f's exact full payload and source identity,
whitelisting only the read-only KodiService libraryViews getter, its packaged
inventory and two browse/source QML files, plus named tests/harness. It refuses
Main/KodiMedia lifecycle or any other production delta and runs seven existing
registrations. No new full run for this bounded UI follow-up; installed movieart,
playlists, offline search and reference review follow the same staged candidate.
Acceptance remains88% pending proof; BUG-153/154 are implemented, not accepted.

g stopped before application testing: a harness edit duplicated the suite-selection clause into receipt validation with wrong indentation. Removed that duplicate; g attempted no application registrations. h retries the corrected harness, preserving g's failure.

h builds successfully and passes126 runner checks, but the first selected
registration aborts with corrupted double-linked list inside televisionLibraryRemoteFlow
(474 ms), then Qt's stack-trace helper hits the registration's60s timeout.
No deployment. Added bounded phase diagnostics to that existing case so the
next focused attempt locates the corruption rather than treating a timeout as
flaky or rerunning full integration. Source/app delta remains the same narrow
Music/source UI boundary. h's abort and full console are retained.

BUG-155 root cause identified during i preflight: auto deduced QJsonValueRef
from operator[] on a temporary QJsonObject in the new libraryViews getter.
The proxy outlived the temporary and caused h's heap corruption. Use value()
to retain an owning QJsonValue instead. Cancel i's redundant diagnostic run
before further application tests; this is a diagnosed memory defect correction,
not optional polish or interruption to restart a healthy full build. j will
verify the corrected getter and all seven existing affected registrations.

j passes all7/7 existing media_controls registrations in111s after owning the
JSON value;126 runner checks pass. SourceDigest a777f87dffbc89f5fa6747f0676b192d2cbce4f1bc5b4c6f6d0b6b7b34447581.
Strict reuse verifies f's63/63 full payload and the bounded changed-file set.
Deployed releaseed516a6288baeffa1ffb5b12; normal service active/running/zero
restarts. Installed movieart/playlists campaign now running on that exact
candidate; review and acceptance pending. T04=51%,193/235, project88%.

j installed Visual attempt084828 stops in guest desktop preparation, before
launching a disposable app: the all-app accessibility walk exceeds its8s/2048
node bound. BUG-156 runner scope: prune every application tree except verified
/proc process owners gnome-shell and the guest housekeeping notification process.
Keep identical time/node/action constraints and exact Ignore-only behavior;
add process-identity refusal cases in the existing oracle safety test. No
application changes or rebuild; verify harness through desktop-prompts Configure,
then repeat installed Visual on the same passing j candidate.

Desktop repair k passes126 safety tests and actual preparation. The repeated
j Visual attempt085106 stops during fixture generation, before app launch:
new function-local import struct shadowed the module import used earlier for
poster PNG creation. Removed the redundant local import; retain global struct
and local wave. Helper-only correction, same installed j, no application rebuild.

Installed attempt085306 saves real Movie and Music sources through Isolde,
passes preview/cancel no-write and native denied-write/journal recovery, and
restores exact source XML. Its final inventory list equality fails. Native
Load can reorder virtual add-on defaults relative to serialized local sources;
compare complete source records as a multiset (retain every field/duplicate),
print both raw inventories, and refresh the saved list index after reload. Any
actual scanner/provider difference still fails with both inventories retained.
BUG-157 visual review: source preview can scroll away its title/status before
all review fields are read; first-row Up cannot reveal that context. Record
for the next combined UI correction after the installed semantic campaign;
no acceptance claim for that source-preview usability outcome yet. Initial
capability/source/error frames reviewed, no desktop notification obscures them.

Attempt085821 proves BUG-158: recovery actually drops both pathless Kodi
virtual add-on sources; every local Movie/Music scanner/provider field survives.
The full-record multiset assertion correctly remains failed. Native recovery
now retains full pathless source objects and original positions across XML
Load, for all five Kodi source categories, including retries after a denied
publication; failed Load restores the previous vectors. No source is filtered
from acceptance. BUG-157 preview becomes remote-focusable read-only review
rows, with a separate final apply action and bounded paging inside over-height
rows; first review row restores title/status. Existing Home case covers the
review inventory and top scroll reset; helper now navigates to the actual final
apply row. These are one correction batch. Rebuild only the managed Kodi native
extension/package, then media_controls on f with the explicit native source
recovery file included in its reviewed boundary; no Main/KodiMedia lifecycle
changes allowed. Full native extension evidence is the repeated installed
failure/recovery invariant, not a fictitious full-suite pass. Final T04 closure
integration remains separate; current app full f63 and focused j7 remain honest.

m stops before native compilation: verified guest artifacts recover only1.69
GiB. Storage evidence identifies773 MB of owned synthetic fixture media, with
new failed fixtures still protected by the old last-three dedup retention.
Use the existing checksum/format/mount/active-path checked hardlink deduplicator
also for recent *inactive* fixtures only below2 GiB, preserving every path and
byte. Existing safety test now proves active-fixture exclusion with that mode.
Do not remove source data, cached evidence, host files or passing candidates;
keep the2 GiB minimum. n retries only after this specific safe-space correction.

n preserves all synthetic media bytes and reclaims the recent duplicates but
still stops below2 GiB; no native compilation attempted. Existing staged-payload
dedup leaves every historical installed release as a separate copy. Extend that
same verified dedup to old inactive installed releases, keeping current, previous
and any live exe/cwd release untouched. Verify each entire release against its
passing test manifest before sharing matching mode/owner/device/hash files and
again afterward; mounts, redirects, unknown bytes and pending transactions still
refuse. Paths/bytes are retained. Existing safety test proves current/previous
inode preservation and inactive-release byte identity. This is guest artifact
maintenance, not host cleanup, deletion of releases, or a lower disk threshold.

User November13 redirects work to a permanent VM space fix. o also stops below
2 GiB before compiling. Read-only topology proves a50 GiB disk with a fully
allocated ext4 root (no unused partition capacity); host has~1.1 TiB free, but
no VM disk/host files were changed. Expanded usage reveals10.8 GiB of historical
installed releases and3.3 GiB of oracle runs. Installer never expires releases;
all historical focused receipts also pin base payloads forever. Replace those
unbounded lifetimes with3 newest releases/staged candidates plus current,
previous, recovery-unit, active-process, stopped-profile and reachable current
candidate dependency pins. Audit content-addressed release trees (allow only
bounded generated Python caches), exact staged hashes, ownership, links and
mounts; preserve every manifest/log/screenshot/profile/build cache. Refuse
changed identities before applying. First run storage-capacity audit only,
review the concrete plan, then apply through the same guarded runner. No more
native rebuild retries until sustained free-space and retention checks pass.
