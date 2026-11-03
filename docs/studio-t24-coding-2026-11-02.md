# T24 coding and deferred VM acceptance

November 3 continuation: [navigation, large-layout and literal-label repairs](studio-t24-followup-2026-11-03.md)
records additional software fixes and 11 passing local JavaScript contracts. The
original coding batch below was not a complete T24 implementation declaration.

User direction on November 2, 2026: implement T24 coding while VM verification
is stalled. No VM command, deployment or application acceptance run was authorized
by this slice. T24 and T15.4 remain open; this document records implementation and
local harness checks, not installed acceptance. T22/T23 acceptance is still a
dependency of final T24 acceptance.

## Shipped entry and navigation changes

`CustomizeHomeView` is the single normal Studio host. Its initial page and stable
component identity now come from the runtime origin. Settings passes the active
named page, if present, instead of always opening root Home. Inline Edit page
captures its component and page/group viewport before opening Studio, then restores
them after closing. If Apply removed the origin, the existing `HomeScreen`
restoration policy chooses a reachable fallback.

The shell now sends input to an active editor before its global Home handler.
Inside the Studio host, Home follows Back's existing gesture/tool/dirty-exit
contract. It cannot silently destroy a dirty draft. Volume and power retain the
shell's existing handling. Explicit profile teardown still invalidates the session.

The application QML resource list no longer packages the superseded
`AppearanceEditor`, `ComponentLayoutEditor`, `HomeDesignEditor`, `PageManager` or
`PageEditor`. Their source files and historical native regression fixtures remain
in the checkout; they are not normal shipped entry points. `LegacyCustomizeHomeView`
was already absent from that resource list. No developer flag was found on the
normal Studio path. This build-time cutover still requires the installed parity
review below before T24.4 is accepted.

## Legacy operation destinations to review

These are code destinations for the full [feature map](design/page-studio/feature-map.md),
not claims that the installed operation passed. T22/T23 features remain owned by
those tasks and must be verified before cutover acceptance.

| Legacy operations | Studio destination / owner |
| --- | --- |
| Earlier/later, placement and collision | Component context Move; shared grid/ordered proposal and history |
| Size | Edit component → Size & layout → Outer size; advanced exact dimensions |
| Remove, duplicate, hide, lock | Component context; draft transaction and Undo |
| Title/type/logo/cover, symbol, image import/removal | Edit component appearance/content; managed artwork picker and framing |
| Item opacity and corners | Edit component shared presentation sliders |
| Brand/title/clock/Search/Settings | Page appearance → Header; Layers → Header for hidden controls |
| Theme, background, wallpaper, type, spacing, global roundness | Page appearance and saved-design tools; T22 inheritance/scope |
| Assign/manage categories | Page appearance structure, sections and component section picker |
| Clock, action tile, app and media components | Six-family Add catalog and validated action/source pickers |
| Source repair | Edit component → Repair source; honest unavailable state |
| Defaults | Editor settings → Reset page; reversible draft and explicit Apply |
| Composition, grid, library presentation | Layout controls and type-specific inspector |
| Page create/copy/rename/reorder/Home/remove/remap | Named page menu and page context; draft commands |
| Page links and Movies/Shows destinations | Named page/action pickers; validated composition routes |
| Save/apply/rename/delete skins, starter-page updates | Saved designs panel/library; T22 portability and T23 navigation safety |
| Apply, cancel, restore, revision conflict | Session-wide T23 summary/atomic persistence and recovery |

## Prepared checks

`tests/studio/tst_cutover.qml` exercises production QML/session behavior for
selected-origin entry, Home during a dirty draft, Home during movement, teardown
and reopen, and named-page entry/return. CTest registers `studio_cutover`; the
bounded runner exposes `-Suite studio_cutover` and includes it in `-Suite studio`.
`studio_cutover_report` registers the local Python auditor contracts in CTest.

`tools/studio-cutover-report.py` prepares 34 pending installed tasks covering all
five T24 groups, the usability/control/scrolling expansions, reference/reset pairs,
legacy parity, isolation, rollback, offline denial and service restoration. It
requires expected/observed state, permitted input, exact focus, actions/time/wrong
turns/help needed, zero unresolved accidental launches/rearrangements/focus loss, and reviewed
SHA-addressed evidence files. Reference/reset tasks require PNG captures; scaling
requires actual 1280×720, 1920×1080 and 3840×2160 PNG dimensions.

The report freezes production sources, configuration/assets, CMake and shipped
helper scripts, including new files. Source drift, missing tasks, stale/unsafe
evidence paths, absent reviews and incomplete/failed/skipped full-suite evidence
prevent an evidence-complete result. Input/frame metrics are required separately
at 720p and 1080p: at least 100 positive finite samples, p95 input-to-visible below
100 ms, mean frame rate at least 30 fps with 100 visible components, 1024 editable
nodes, no unacknowledged wait above 250 ms, and bounded history/clients after ten
open/cancel/preview/teardown cycles. The declared VM/renderer/CPU/memory baseline
must accompany measurements.

This is an evidence inventory auditor. It does not synthesize measurements, run
the installed product, independently authenticate reviewer statements, certify a
screenshot's appearance or award backlog credit. Raw logs/captures and agent review
remain necessary. The report must contain genuine runner results and measurements;
the unit tests' synthetic records are never product evidence.

Prepare a fresh report after freezing the final candidate:

```powershell
python tools/studio-cutover-report.py init build/t24-final/report.json --source-root .
python tools/studio-cutover-report.py check build/t24-final/report.json --source-root .
```

Artifact references are relative to the report directory, with `path`, `sha256`
and explicit `review` fields. Copy preserved runner evidence into that directory;
do not point outside it. Initialization refuses to overwrite an existing report.
An unfilled report deliberately fails `check`.

## Validation and remaining acceptance

Local auditor contracts: **17/17 passed** using the bundled Python 3.12.14 with
temporary fixtures inside the workspace. The existing runner's focused
`test_studio_suite_builds_only_matching_test_targets` passed. The broader oracle
unit run stopped in its process-cancellation test with exit 15 on Windows; it is
not a passing suite. Log: `build/t24-vm-oracle-local.log`. Do not infer a VM failure
from this host-only test interruption.

Native/QML build, CTest, installed journeys, both reference/reset reviews,
real-Kodi isolation, disconnected Generator/network denial, performance measurement
and service restoration are **not run**. Qt/CMake/CTest, qmlformat, clang-format
and Ruff are unavailable on this host. No host tools were installed and no host
settings/services were changed. T24 remains 0% pending evidence; software defects
found by the deferred runs stay software work, not hardware handoff.

Resume with the bounded VM runner after the stalled verification environment is
available. Use one frozen final application candidate, the task's full CTest run
and installed Studio/desktop/catalog/collections/history/pages journeys plus both
reference wrappers with explicit SourceRoot. Reuse applicable earlier evidence
only after comparing source applicability. Complete all report tasks and the
legacy map review, fix observed failures, then record release/source/executable
hashes, restore the normal service/profile state and reconcile T15.4/T24 credit.
Physical hardware and live-account compatibility remain the separate user handoff.
