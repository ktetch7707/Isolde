# T20 completion/dependency audit — 2026-10-23

The previous goal turn made concrete progress: the installed nested desktop journey passed and its automation/evidence was committed and pushed as `9c39436`. This audit rechecks the remaining requirements rather than inferring task completion from those passing movement tests.

| Requirement | Current authoritative evidence | Verdict |
| --- | --- | --- |
| Revised pointer threshold, held-drag cancellation, visual neighbor reflow, shared transactions | `tests/studio/tst_gestures.qml`, `tst_policies.qml`, `tst_overlays.qml`; current full native log below; installed root/nested captures | Implemented and covered in the stated scenarios |
| Ordered nested Row/Column/carousel insertion | `tests/studio/tst_ordered.qml`, `studio-ordered-insertion-2026-10-23.md`; current full native checkpoint | Covered; does not imply every product workflow accepted |
| Named group/section targets, locks, cycle/full-target refusal and exact viewport cancellation | `tests/studio/tst_destinations.qml`, `studio-section-targets-2026-10-23.md`; full native checkpoint | Covered |
| Full/mixed/locked grids, decorative overlap and reduced-motion replay | `tests/studio/tst_policies.qml`, `tst_overlays.qml`, `studio-movement-policies-2026-10-23.md` | Covered in the documented matrix |
| Same nested arrangement by remote and desktop, exact Undo and no premature writes | Installed `t20-nested-desktop-20261023-full-a`, `/tmp/isolde-components-acceptance-29avgpol`; all ten captures reviewed; saved-byte assertions | Passed for the actual authored 4x2 group; hardware capture remains unqualified |
| Current native regressions and profile isolation | `build/vm-oracle/t20-nested-desktop-20261023-full-a/report/ctest.log`: 25/25, 275.45 seconds; installed report retains both normal profile hashes | Passed |
| Both standing reference recreations and reset | `studio-installed-2026-10-23.md`, installed Lounge/Pastel captures on the same `d87a5f1475b036da6b5e55a8` executable | Reset functional journeys pass; populated Pastel recreation FAILS acceptance (initial-based lone app, unavailable history) |
| Professional default design, broad TV/user/offline product acceptance | Current screenshots and the owning later task gates | Incomplete/unproven; no new credit |

## Sequencing block

`AGENTS.md` instruction 2 and `docs/agent-backlog.md` require selecting the first unfinished eligible ACTIVE row, reporting blockers rather than silently changing features. `docs/implementation-order.md` preserves T20.5 -> T21. `docs/agent-backlog.md` makes both reference recreations a standing gate and T21 depends on T20.

At the same time, T21.9 and `docs/design/page-studio/component-contracts.md` explicitly require rebuilding the Pastel preset/fixture with a genuine app carousel and Recently viewed carousel plus deterministic provider-backed acceptance. The current watched-library shelf and historical individually authored fixture cards cannot satisfy that requirement. Reverting to those cards, weakening the visual gate, marking T20 100%, or quietly implementing T21.9 would bypass an explicit instruction.

The sequencing question was raised before the root/nested installed follow-ups, remains unanswered, and the same dependency has remained through at least three consecutive goal continuations. Those turns completed independent T20 evidence. No build/test is now running; latest reports are terminal passed/finished. Further passing runs of the unchanged implementation would not resolve the missing component semantics. No remaining independent T20 application change has been identified by this audit that would resolve the standing gate.

Decision required: permit the T21.9 prerequisite work while keeping T20 and its visual gate open, or retain the strict order and leave this dependency blocked. The recommended path preserves every requirement and percentage while implementing the missing prerequisite. No sequencing exception is assumed from elapsed time or an unanswered question.

Task T20 remains 80%; T21 remains 36%; 24 active tasks, sum 866, Overall round(866/24) = 36%. No completed checkbox, deployment, test rerun, normal-profile edit, guest cleanup or host maintenance is part of this documentation audit.