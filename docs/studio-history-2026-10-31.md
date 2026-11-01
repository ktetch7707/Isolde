# T21.7 Recently viewed — S04 accepted 2026-10-31

S04 is complete on the installed clean-VM candidate below. T21 remains incomplete; S05/T21.8 is next. This acceptance closes only T21.7 and its three evidence milestones.

## Delivered behavior and supported matrix

- Kodi owns durable movie/episode viewing history, playback and progress. Independent descending queries and their merged result are bounded by each instance's validated History count (1–50). Invalid dates are excluded. Visible cards is a separate presentation control; changing capacity does not refetch or change membership. Ordering uses viewing time, provider, media type and stable ID.
- Explicitly selected registered desktop/web/Android applications use **Isolde-managed, session-only app-opening history**. The profile observes openings after opt-in and publishes a reference only after successful session return. At most 50 app-ID/time references are retained in memory. Failed starts preserve prior references. Reopening uses ActionDispatcher and the registered application's normal SessionManager route with exact origin restoration.
- Local-only is the default. Clearing sources disables observations for that instance. No account, browser history, credentials, cookies or parallel media/progress database is read. This local adapter needs no account authentication; source selection grants observation permission. Service-specific viewing-history adapters are unsupported, explicitly explained in the picker/status, and no third-party content playback is claimed.
- Multiple instances retain independent counts, capacity, appearance and source selections. Provider/type/ID identity survives refresh and reorder. Source badges are configurable. Missing applications are honestly unavailable; registry removal/reinstallation works. Kodi failure leaves permitted app references usable without external requests. Retrying a transient outage works; profile teardown permanently retires clients, cancels work, disconnects observation and clears references. Old clients cannot issue requests or read/write collections.
- Studio offers separate bounded History count/Visible cards steppers and a remote-operable source picker. Pointer and remote edits use the same draft, cancel/no-write and one-transaction Undo paths. Empty-to-populated widgets select a visible card; gestures freeze both items and selection.

## Final candidate and automated evidence

Evidence root: `build/vm-oracle/`.

- Full base **studio-history-full-20261031-l**: **41/41 CTests pass**, zero failures/skips, 471.85 s. Source `72a2bcc9a5ae83c18b277cd4f3e3eae945b5297c78285908a25cdd7f00fea0fb`; see `report/ctest.log` and run manifests.
- Final **studio-history-focus-20261031-q**: focused `studio_selection` passes; all **55 runner safety tests pass**. Strict `-ReuseFullRun studio-history-full-20261031-l` proves original full coverage and limits changed application paths to existing `qml/PageLibraryWidget.qml`, `qml/StudioAppPicker.qml` and `qml/StudioInspector.qml`. Q is a focused run, not a second full-suite claim. Source `4c9e5a230f6b16a56f47720745b678ffc7473ee5a7c668eaab796508849cd23c`; installed release **9d866e376432c4c3ae7f4285**, payload/installed identities verified.
- I Home/Studio native checks cover actual shared-dispatch origin, unsafe-origin rejection, failure retention, independent instances/profiles and retirement. J Kodi native checks cover deterministic ties, invalid/duplicate providers, OFF clearing, offline fallback, bounds and preserved selection. K verifies actual pointer/remote stepper input, 1/50 endpoints, cancel and one Undo. M/Q make source selection mandatory with three registry entries and real StudioSession edits; no empty-fixture skip is credited.
- Host wrapper contracts pass at `build/test-all-contract-ff1a861d5ac5484eb0e2c06d4ec75b00/`. Q report-only aggregation passes at `build/vm-oracle/studio-history-focus-20261031-q/report/index.html`.

## Installed runtime and agent review

All five journeys pass on installed Q with unchanged normal profile/layout hashes:

| Case | Accepted evidence | Guest fixture |
| --- | --- | --- |
| history | Seven captures: fresh real Kodi import, ordinary library playback with actual video frames, native lastplayed update, two instances (count5/capacity3 and count1/capacity1), details/back, actual replay from history and repeated exact return; unchanged media/layout | `/tmp/isolde-tv-acceptance-i77p162h` |
| collections | 22 captures: eight actual native input/normal-exit cycles including two history reopens with Kodi unavailable, registry remove/reinstall, exact return and unchanged page bytes | `/tmp/isolde-components-acceptance-m85crqjk` |
| studio | 15 captures: existing Studio/nested/scroll/apply regression | `/tmp/isolde-components-acceptance-87ulm0za` |
| lounge | Four captures: generic reference recreation/reset | `/tmp/isolde-reference-config-dz6ehz_c` |
| pastel | 14 captures: generic reference/reset, galleries/history/See All, music/tracks/Guide/search | `/tmp/isolde-reference-config-7ssmk7ki` |

All **62 installed screenshots** were agent-reviewed, along with native captures and original reference pairs. Hash-mapped contact-sheet manifests are in `build/history-review/` (`installed-history`, `installed-collections`, `installed-studio`, `installed-references`, `native-before-l`, `final-native`, `all-captures`). Duplicate hashes map every capture to reviewed pixels. The native isolated client proves input/session return, not third-party playback. Pastel deterministic artwork proves rendering, not actual viewing; the separate real-Kodi journey supplies viewing proof.

History helper SHA256 `af8a02f5b640b41fd0057707eb57efe6efaca04bd15788886e0e56fb93c68058`; collections helper `a181aec3a2aaeba6c0f6c3153accac2e7911d82b11ab5f90d926958d4741d629`. Hash-addressed overlays retain frozen application/config/dependency identity. Normal `home-layout.json` remains `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e`; Studio Demo profile layout remains `85865ee3f03c6301cb2b3153bacc1bde8fba01b2c005279f7cd86263311cb4cf`.

Final Status: service active/running PID2208508, zero restarts, no active runner/guardian; guest free 1.76 GiB. Current bytes of all 215 application/build/config files match Q's snapshot exactly. Subsequent changes are evidence/docs and removal of one screenshot-output line: that native image showed an obscuring test sibling and was rejected as visual proof; its behavioral assertions remain. Installed history captures provide the actual focus proof.

## Failures, corrections and evidence applicability

B exposed a real count-change selection reset. C passed 41/41 but review found the app reopen bypassed shared origin dispatch; that material lifecycle correction justified the second full run L. H's follow-up failure was a fake-provider sort-before-limit error, corrected without weakening identity assertions. K's empty source fixture could skip selection; M/Q require it. P exposed a real empty-to-populated headless-widget selection defect; Q fixes and tests it. Later QML corrections used strict full-coverage reuse rather than another full run.

Two earlier installed history attempts failed in harness setup (export-relative clip paths/toolbar navigation, then missing strict-v2 bindings causing honest default fallback). Their captures were reviewed and rejected as acceptance. Corrected helpers pass against Q. No failed evidence was relabeled.

Guest space guards stopped pre-test runs. Bounded verified guest maintenance preserved profiles/releases/cache/logs/archives and receipt-pinned/recent payloads, refused mounted or modified trees, and deleted only recoverable sources or obsolete full-manifest-verified payloads. G/O recovered space but honestly failed the requested 3 GiB maintenance target; subsequent Test met its independent 2 GiB guard. Receipts remain in `studio-history-storage-20261031-g` and the later storage run. No host maintenance occurred. Future work must inventory/verify guest cleanup if the guard blocks.

Snapshots include pre-existing Guide/config changes and pre-existing native tests required by current CMake through a temporary packaging index. Their inclusion is not ownership or acceptance of unrelated backlog gates. The actual staging area and unrelated edits are preserved. Progress: T21.7.a–c and original .7 accepted; **T21 55%, sum 1004, Overall 37% across 27 active tasks**.
