# Page Studio review and acceptance journeys (T15.4)

Status: recorded design review, 2026-10-19. Records (a) the uncoached walkthrough of the
[wireframes](wireframes.md) and [state model](interaction-states.md), (b) the confusing
transitions it exposed and how they were resolved, and (c) the measurable acceptance journeys that
gate implementation (T24 cutover). No application build is claimed by these static mockups;
installed acceptance remains T24's own gate.

## 1. Uncoached walkthrough record

Walkthrough script (performed against the artifacts, remote keys only, no pointer): open Studio on
an existing Home; inspect a component; add a widget to a blank page; move it across occupied cells;
resize it; trigger a collision; preview; attempt to leave with a dirty draft; apply; recover from an
induced apply error; enter a nested container and return. Findings:

| # | Observation | Severity | Resolution (applied in) |
| --- | --- | --- | --- |
| W1 | "Preview/apply theme" in the current editor applies immediately despite the label; Studio users could inherit that expectation | High | Studio names it "Templates" with an explicit Apply pipeline; wireframes §10 shows the change summary before any write; feature-map row 12 records the misleading label |
| W2 | "Change size" today relocates the component to row 1/column 1 (anchored 0,0); spatial resize must not inherit that surprise | High | wireframes §7 pins the resize anchor (top-left default, Menu-selectable) and keeps the candidate in place until accept |
| W3 | ComponentLayoutEditor overloads Left/Right (section select vs value adjust) with no visible rule | Medium | interaction-states §2 gives Left/Right exactly one meaning per state; section targeting moved to explicit target rows in the inspector |
| W4 | Nested page editing hides its scope (no indicator) | Medium | Top bar always shows "Edit: Home ▸ Inputs ▸ HDMI panel"; wireframes §11 breadcrumb |
| W5 | Dirty exit is unpredictable in the old workflow — every action already saved; users cannot learn "nothing applied yet" | High | Dirty-exit dialog (wireframes §9) is the only leave path and names affected pages; footer shows "Draft: N changes" at all times |
| W6 | Collision options don't preview consequences before commit | Medium | Collision dialog previews the chosen proposal on the canvas behind (wireframes §8); disabled options explain why |
| W7 | Add-tile "Open with" empty state is informative but buried under a numbered action grid | Low | Add catalog tiles carry availability state directly (wireframes §4); honest unavailable states remain |
| W8 | Search-as-catalog-search could be confused with global Search | Medium | interaction-states binds canvas Search to the Add catalog filter only; global Search stays the shell destination; T26 keeps the two distinct |

All eight findings are resolved in the artifacts; none requires a spec change. The walkthrough
confirmed the done-when gate: every operation completes with D-pad + Confirm + Back + Menu/Search;
no step needs a pointer, number keys or a hidden gesture; movement is one cell per press with
visible origin/ghost/candidate at all times.

## 2. User review

The recorded user/representative review of this workflow is referenced in the backlog T15 evidence
line at commit time. Standing rule from spec §10: the workspace is not accepted merely because its
author can operate it — T24 repeats this walkthrough with the actual user on the installed build,
uncoached, and a failed walkthrough requires iteration before cutover.

## 3. Measurable acceptance journeys (gate T24 cutover)

| # | Journey | Measure |
| --- | --- | --- |
| A1 | Blank → add Clock and app row → move → resize → preview → Apply → restart | 100% completion uncoached; remote-only recording/screenshots; persisted document byte-identical after reload; focus visible at every step |
| A2 | Pick-up → traverse occupied cells → Back | Applied and draft documents byte-identical to gesture start; zero history entries; zero file writes |
| A3 | Shift/swap success; multiple colliders; impossible/full grid; induced save failure | Complete deterministic proposals; one undo record per resolution; no partial save; actionable error with retry |
| A4 | Remote versus mouse versus numeric inspector, same semantic edits | Byte-identical canonical documents; same validation and collision outcomes |
| A5 | Nested input panel + multi-selection group move | Explicit scope/breadcrumb; local-vs-parent movement correct; group atomicity and 32-node bound enforced with explanation |
| A6 | Existing pages/skins + invalid stored layout opened | Original bytes preserved on open/cancel; conversion previews fully; invalid data falls back without rewrite |
| A7 | Page removal, route repair, profile switch, revision conflict | Exact affected-route summary; remap offered; no cross-profile writes; stale clients invalidated; focus restored |
| A8 | Offline + missing backends | Designer, catalog, local artwork, draft/apply/recovery all work disconnected; Web OFF issues zero external requests; honest unavailable states |
| A9 | Entrances and recovery | Enter movement within two confirmations from a selected component; zero accidental media/app launches; Settings/Edit Home reachable from every state including empty Home; no lost edit or focus across launch/playback/dialog returns |
| A10 | Performance on documented VM baseline (1280x720 and 1920x1080) | Remote step visible < 100 ms at p95; ≥ 30 fps moving 100 visible widgets; 1024-node document editable via virtualized layers; no input wait > 250 ms without feedback; repeated open/cancel/preview/profile switches do not grow clients/history unbounded |

Targets adapted from spec §10. Both standing visual fixture pairs must also pass after every
Studio slice that touches rendering, with no fixture special-casing.

