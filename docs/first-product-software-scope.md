# First product: software completion before hardware qualification

## Current S1 authority — user revision 2026-10-29

**100% means reasonable belief that the software is done, proven entirely in the VM.** [VM software completion](vm-software-completion.md) replaces every conflicting gate below and in referenced plans. All 27 task weights remain, but human-participant/approval, actual-device/model/USB/firmware, paid-account/entitlement/geo-success and target-only runtime/performance gates are redesigned as production-boundary/virtual-device tests and recorded agent walkthroughs. Actual hardware, account/service and personal-usability checks are the user's post-100% handoff, outside percentages. Long elapsed-time durability uses deterministic clock tests plus actual cold reopen; a real twelve-hour soak is optional.

T13.4/T31.7 now require a complete illustrated generic installation guide and an agent-run clean-VM artifact/install/playback/recovery walkthrough. No independent reader or actual-computer details are required for S1. Final T31 still uses one frozen shipped artifact and clean-VM integration, relevant failure/security/isolation checks and agent-reviewed visuals. Existing defects and missing software remain blockers; no mock may stand in for the production feature itself. Public release is a separate user decision, not another software-completion gate.

Current percentages are in the backlog and weighted milestone calculator; none is increased merely by changing this acceptance definition. The October 25 material below is a historical scope/evidence audit. Its statements that outside accounts, representatives or actual-computer details block 100% are superseded, not unresolved requirements.

## Historical October 25 audit — subject to the current VM contract

Reconciled 2026-10-25 at the user's request. This is a planning/evidence audit, not a new application test or implementation claim. The [backlog](agent-backlog.md) owns task status and execution order; this document defines what its 100% means.

## Milestones and scope

**S1 — first-product software complete:** every active T-task is 100%, all required software journeys have evidence, and T31 accepts one frozen installed candidate. This means ready for physical hardware qualification, not publicly released or hardware certified. Complete software behavior, UI, provisioning and recovery before starting that qualification campaign. Existing hardware observations remain evidence, but are not prerequisites for implementing software editors.

**H1 — hardware qualified:** the separate H01–H05 ledger passes on intended equipment, including the witnessed TV-remote → Xbox gameplay requirement. VM input simulation and virtual discs cannot satisfy H1.

**R1 — release accepted:** S1 and H1 pass, hardware findings are repaired and reverified, and the final distributed artifact passes installation/recovery on target hardware. S1 never waives release safety.

S1 includes the existing active scope, the current Store/Chromium work, and all required Games software from the new plan. Nothing is removed merely to improve the percentage. T14 was previously optional before release; it remains in the active denominator here rather than silently deferring discovery/subtitles. Changing that product scope requires an explicit recorded user decision. The Games plan's already deferred cloud/subscription discovery, DRM workarounds, BIOS/ROM acquisition and save synchronization stay deferred. Play Store credentials/GAPPS and Chrome extensions are superseded proposals, not S1 requirements.

## What the audit established

| Area | Evidence reviewed | Current interpretation |
| --- | --- | --- |
| Studio foundations | Backlog T16–T19 evidence; `studio-geometry-2026-10-20.md`, `studio-page-lifecycle-2026-10-20.md`, `studio-remote-editing-2026-10-20.md` | Retain completed foundation credit; full-product usability/visual acceptance remains separate. |
| Pointer/nested editing | `studio-t20-completion-audit-2026-10-23.md`, `studio-history-prerequisite-2026-10-23.md` | Much of T20.5's input/policy matrix is covered. Populated Pastel semantics/design and integrated acceptance still prevent closure. The T21.9 prerequisite exception is already authorized. |
| Collections/history and controls | `studio-history-prerequisite-2026-10-23.md`, `studio-live-carousels-2026-10-21.md`, colors/sliders/selection evidence | These features are partially implemented, not simply unstarted. Keep broad T21 gates open for their remaining provider, visual and installed cases. |
| Reported repairs | `product-feedback-2026-10-25.md`, `studio-six-fixes-2026-10-23.md`, `home-feedback-2026-10-24.md` | Sidebar removal, Store link/return and ambient layering have narrow verified evidence. Full header, screensaver authoring, genre shelves, artwork quality and theme requirements remain open. Ambient preview is not timed-idle acceptance. |
| Kodi | `kodi-complete-interface-2026-10-25.md`; `media-controls-20261025-i` 31/31, seven playlist captures and 28 standing captures reviewed | Real playlist name/cancel/create/catalog works. Full management, installed shelf-picker, friendly headings, Kodi-free setup and actual video presentation in that journey remain open. API play/pause/stop state is not visible video evidence. |
| Native apps | `default-apps-2026-10-23.md`, native-return full C | Actual FreeTube/CBC/Steam launch and Home return are evidenced; CBC reports outside Canada. FreeTube is historical proof, superseded by SmartTube as the preferred YouTube client. Do not generalize three apps to all advertised providers or subscription playback. |
| Android | `android-apps-2026-10-25.md`, Android apps B 31/31 and seven reviewed app frames | Repeatable runtime foundation plus actual catalog launch/menu/Home return earns narrow T29.7 credit. Content/playback, account isolation, setup UI, containment and comprehensive lifecycle remain open. |
| Store baseline | `store-lifecycle-2026-10-25.md`; `build/vm-oracle/store-20261025-g/report/{ctest.log,status.json,deploy-result.json,visual-store.log}` | G has 31/31, deployed `014674ec29d7dfb787fd8ca9`, functional install/open/close/remove phases for both platforms, unchanged layout hashes and zero service restarts. Its retained status still says visual review/acceptance pending and semantic acceptance not verified. Do not convert functional success into reviewed acceptance. |
| Live Store work | `actual-stores-integration-plan.md`; `build/vm-oracle/store-online-20261025-c/report/{status.json,ctest.log}` | Accepted sources are verified account-free F-Droid and web apps/Add by URL. C passes three focused Store registrations; `fullSuite: false`. No new full-suite, repository download or remote-bridge acceptance is inferred. Worktree implementation continues independently. |
| VPN | `vpn-setup-2026-10-24.md`, `vpn-switch-permission-2026-10-25.md`, feedback's AQ checkpoint | Approved helper/provisioning and handshake diagnostics exist. Actual country/traffic, saved CA/US and CBC automatic switching remain open. |
| Games | `games-page/GAMES_PAGE_PLAN.md` §§1–10 | Approved requirements/planning, not delivered library/gameplay. T30 now owns G0–G6, local copies/installers, disc software and integrated emulator software. Physical gamepad/optical acceptance stays required under H02/H03. |
| Code quality | `code-quality-strategy-2026-10-25.md` | Proposed management/tooling program, with no measured clean score or adopted coverage threshold. T31.6 requires a real candidate risk/test/review baseline before hardware. Q0–Q5's complete formatter/analyzer/dashboard/ratchet rollout remains a separately schedulable proposal, not silently accepted or marked delivered. |
| Guide/capture/appliance/online | Existing T10/T11/T13/T14 scopes and evidence | Guide and online/subtitle capabilities remain open. Capture foundation and installer/recovery code are partial software foundations, not qualified hardware or a finished appliance. |

This review read retained reports and dated verdicts; it did not newly inspect every historical image, run CTest, contact providers, or certify the dirty current source. A missing/stale review is a closure blocker, not an automatic failure of the implementation or a pass. Historical evidence can be reused with an explicit unchanged-code/data comparison; T31 must still test the final integrated candidate.

## Requirement-to-task coverage

| Requirements / planning authority | Software owner and remaining closure |
| --- | --- |
| Page-first design, reference audit, component contracts, usability audit, control audit, approved 15-feature expansion and scrolling contract under `design/page-studio/` | T20–T24. Their owning acceptance tables remain mandatory, including presets, My components, linked styles, copy appearance, replacement, responsive rules, stress scenarios, reference overlay, named versions, editor permissions and health checks. Declined ideas remain excluded. |
| Complete Kodi interface | T04/T09, source bindings T21, themes T27, final journeys T31. Supersedes the old T02 curation prohibition for Kodi-owned authoring; T02 remains archived. |
| Feedback #1 common canvas and removable Page menu | T21.11/T21.13/T24; verified automatic-sidebar repair is retained. |
| Feedback #2 playlists and #8 genre shelves | T04.4/T21.4/T21.8/T22.1; demonstrate actual installed picker/shelf/refresh/play and independently editable genre starter shelves. |
| Feedback #3 Settings/VPN theme; #10 sharp artwork | T12.1/T12.4/T22.2/T27 and T21.6/T21.9. TV legibility is reviewed in VM; actual TV-distance hardware acceptance remains H01. |
| Feedback #4 authored screensavers and #11 overlay precedence | T12.5/T21.8/T22/T26. Layering is narrowly verified; timed idle, authoring, rotation and exact return remain required. |
| Feedback #5 universal return; #6 background Kodi | T12.3/T12.6 and T04.2/T04.7/T09; real external focus, failure/retry and exact origin, with no desktop rescue. |
| Feedback #7 unified Games | T30 with shared T21/T22 components; Steam launch tile is not library/gameplay acceptance. |
| Feedback #9 Store button; #12 editable identity/Search/Settings | T26 and T21.13/T24. Store-link repair retained; full configurable gallery/header remains open. |
| Default apps, Android/SmartTube, Store lifecycle, accepted real sources and Chromium remote bridge | T29.1–T29.8, T12.6 and T31. Capability/ABI/DRM/account/region limitations must be explicit and tested; registration cannot stand in for launch/playback. |
| VPN setup and approved switching policy | T12.4. Actual traffic/country and owned-location rollback; no unrelated VPN takeover. |
| Games catalog, Steam, local PC copies/installers, inserted discs, integrated emulators | T30; G0 selects a bounded support matrix, PSP is an initial software target, GameCube/PS2 need explicit assessment. Required routes cannot disappear behind an Unsupported label; unresolved runtime/fixture capability blocks their software gate. H02/H03 retain physical verification. |
| Search, viewing preferences, Guide/PseudoTV | T03/T09/T11/T28; actual media/schedule behavior and offline acceptance. |
| Appliance, recovery, optional online discovery and Linguist | T13/T14/T31; all software stays active, including distributable artifacts and offline dependencies. |
| Beginner installation and setup documentation (user clarification, 2026-10-25) | T13.4 ships a complete illustrated guide for the actual test computer; T31.7 accepts a nontechnical reader walkthrough. Developer setup notes are insufficient. H05 repeats the same guide on the real computer. |
| Code-quality strategy | T31.6 owns pre-hardware tool/check inventory, warnings/defect disclosure, critical-risk regression evidence, separate boundary review and reproducibility. Full Q0–Q5 tooling/adoption remains proposed; do not claim measured coverage, add host tools or enforce unbaselined thresholds. |

## S1 exit contract — reasonable confidence before hardware

T31 is a final integrated gate, not a replacement for task tests. Retain a versioned acceptance matrix with each promised feature/route, exact input journey, real or simulated backend, expected visible result, error/cancel/retry, profile, evidence path and verdict.

1. Freeze source and distributed payload hashes; run all registered Linux tests and meaningful boundary/fault tests. No skipped required suite, stale report, known failing requirement or unreviewed critical visual journey can be hidden by a green aggregate.
2. Install the actual distribution in a clean disposable VM without developer checkout/caches or manual config surgery. Complete first-run setup, reboot, local use and recovery using product controls. Prove signed-artifact rejection, offline dependency coverage, failed upgrade, interrupted write/install and rollback. VM reset/power interruption tests software recovery only.
3. Run actual installed remote workflows: Studio/templates/import/export/apply/restore; Kodi setup/browse/edit/video/audio/subtitles/return; search/guide; app install/update/open/return/remove; SmartTube usable content/playback; CA/US/CBC switching; Games actual supported runtime/gameplay/return. Use lawful fixtures/test accounts when required. Missing accounts, network or runtime capability are recorded blockers, never silently passed or renamed hardware work.
4. Disconnect before cold boot: local media, managed artwork, local search, guide clock/listings, layouts, supported local games, installed subtitle resources and recovery work without downloads. Measure zero OFF-gated Isolde external requests. Test real two-profile state/account/save isolation where promised and teardown/late-callback failures.
5. Review both reference recreation/reset pairs on the frozen source, plus dark/light/Pastel/custom surfaces and bounds at 720p/1080p/4K. Measure declared VM performance budgets and conduct recorded uncoached user/representative journeys. Physical TV readability and target-device performance remain H01; VM usability failures do not.
6. Publish the actual code-quality/risk baseline and separate critical-boundary review under T31.6. Test meaningful failures at persistence/profile/execution/security boundaries; disclose missing analysis or coverage rather than inventing a clean score. Scheduling the full proposed tooling program is a separate decision and cannot replace behavioral acceptance.
7. Deliver and validate the beginner guide below. A nontechnical user must be able to install and set up the test computer by following it; missing instructions, developer-only steps or required coaching prevent 100% completion.

## Beginner installation guide — mandatory for 100%

User clarification, 2026-10-25: installation must be achievable with ordinary technology knowledge. T13.4 delivers `docs/user-setup.md` as the canonical user guide, also available with the downloadable installer and offline in a readable/printable format. The guide is a required deliverable, not a promise that the current developer installer is ready. Do not publish fabricated installation steps while the consumer path is unfinished.

The guide must include:

- A short overview and checklist: supported actual test-computer model/specifications, preparation computer/OS, USB capacity, cables/display, input devices, expected downloads/time, when internet is required and what works offline. Record unknown target details as documentation blockers; do not invent firmware keys or treat a generic Linux guide as target-specific instructions.
- One recommended installation path, with numbered actions from the exact release download through USB creation and safe ejection, connecting equipment, entering the verified target boot menu, selecting the USB, installer screens, explicit disk identification/erase confirmation, completion, removing the USB and restarting. Explain which computer each action occurs on and what should appear next. Any needed preparation utility must have clear legitimate acquisition/use instructions; no hidden preinstalled tool or developer checkout.
- Backup instructions and clear consequences before writing a USB or replacing a target disk, including how to distinguish the USB from the computer's internal disk and how to stop safely if uncertain. Do not require the user to know partition terminology or expose passwords/recovery secrets in examples.
- First-run setup through the real interface: display/audio, network and offline continuation, remote/controller checks, profile creation, library folders and scan, first local playback/return, and supported apps. Identify optional subscriptions/accounts, Android/emulator prerequisites and VPN setup separately so they cannot strand basic local use. Required normal steps cannot depend on compiling, terminals, manual configuration or VM runner commands.
- Screenshots from the matching shipped version, exact button names, one clear action per step, definitions for unavoidable terms, expected progress and wait guidance, success checks and safe Back/Cancel/Retry behavior. Missing screenshots, placeholder links and unexplained branch decisions are incomplete documentation.
- Symptom-based troubleshooting and recovery: USB not listed, no display/sound/network/input, installation failure, first-boot failure, app/backend unavailable, update/rollback and reinstall/reset consequences. Explain precisely what is preserved or erased and how to obtain/share safe diagnostics without developer knowledge. Separate advanced troubleshooting from the normal path.

T31.7 requires a recorded unassisted user/representative walkthrough using only this guide and the actual distribution on a clean disposable installation. Record starting state, versions, completion/time, wrong turns, questions and assistance. Coaching or hidden preparation means that path needs revision and a repeat; screenshots or an author proofreading their own prose do not prove usability.

Before S1, complete the guide including source-verified instructions for the actual computer's preparation/boot/disk choices and validate software installation/setup/recovery in a clean VM. This preserves the software-before-hardware sequence. H05 then runs the same guide on the actual test computer, including real USB boot and physical recovery; any discovered documentation or software defect must be corrected and reverified. Guide availability/usability is a software milestone blocker, not work deferred wholesale to hardware testing.

Software input injection is acceptable for repeatable integration and fault tests; emulator/local-game acceptance still needs actual playable execution, not a process spawn or mock. Physical remote-to-Xbox handoff remains a separately witnessed H02 gate. Virtual optical fixtures must exercise the real adapter path, with hardware-dependent I/O simulated explicitly for failure cases; they cannot certify a drive.

Every S1 blocker must be repaired or have an explicit user-approved scope change. A document-only audit does not complete these gates. Hardware findings can reopen software tasks after S1; the evidence records the candidate that passed, not a promise of zero future bugs.

## Audit accounting

Retain the 25 existing active tasks, add T30 Games and T31 final software acceptance: **27 active tasks**. T02/T06–T08 remain excluded. T29 adds two separately bounded baseline gates; only T29.7 is checked (reviewed Android setup/catalog launch/menu/return), so T29 = round(1/8 × 100) = 13%. All other previous checkbox credit is retained. Sum of rounded task percentages = 860; Overall = round(860/27) = **32%**. This reduction reflects scope coverage and an honest milestone, not deleted implementation.

T10 keeps its 1/3 software-foundation credit while its other gates now require complete software adapters/editors and VM acceptance; physical qualification is H03/H04. T13 now requires the complete software appliance/distribution and clean-VM recovery proof; physical repetition is H05. T12's explicit VPN/screensaver/default-app gates remain unchecked. No other feature completion is granted by this review.

## Planning verification

`tools/verify-backlog.py --evidence` passes: 27 task scopes/table rows in matching priority order, all task checkbox percentages, sum 860 / Overall 32%, synchronized AGENTS/implementation-order counts, acyclic declared dependencies, five separate physical gates, local Markdown targets, retained Android evidence and the Store G/C functional-versus-semantic distinction. Machine-readable result: [backlog review](backlog-review-2026-10-25.json). The default command validates documentation without requiring local VM artifacts.

`git diff --check` passes for the changed tracked planning files. No application build, CTest, provider request, VM action or new screenshot review was run for this documentation-only change. Previous task Verify commands are not newly claimed as passing. Other application work remains untouched.
