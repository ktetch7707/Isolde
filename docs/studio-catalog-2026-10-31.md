# T21.8 clarified catalog — S05, 2026-10-31

**T21.8.a–c and the original T21.8 group are accepted.** T21 is 62% (8/14 groups), sum 1011, Overall 37% across 27 active tasks. No T22 implementation or unrelated Guide/template work belongs to this change.

## Delivered behavior

- Shape decorations and containers, Button links, Image/GIF and independent plain Text use the registered prototypes, shared renderer, inspector and validated declarative data. Optional visible labels remain separate from body text, editor identity and accessibility. Decorative Text/Shape/Image stay out of runtime focus and remain editable through canvas/Layers. Existing media/widgets are retained.
- Plain Text now has independent size, color, weight, alignment, wrapping, line limit, spacing and padding. Theme inheritance and legacy presentation fallbacks remain readable without rewriting stored documents. Replacement removes incompatible body semantics.
- A bounded remote/pointer local browser offers folders, image preview and explicit Import, with Advanced path retained. It inspects at most 512 entries, serves 64 per page, refuses linked/nonlocal folders and regular-file violations, and never writes the applied layout during preview/cancel.
- Visual framing shows source crop/focal point and the actual authored HomeCard geometry, label space, corners, opacity and typography. Fit/Fill segments, crop dimensions, proportions Switch, zoom, full-art reset, Done and Cancel are remote/pointer operable. Cancel restores the complete preview; Done records one transaction; no-op Done records none.
- Real GIF Apply now preserves the validated `.gif` content hash suffix rather than rejecting it as `.jpg`. Runtime Canvas also handles subpixel source crops without rounding them into an empty QImage. Motion is opt-in and obeys existing bounded decode/cache/lease, hidden/offscreen and reduced-motion policies.
- Adding a component clears an earlier header selection, so Confirm edits the newly placed item. Decorative-origin runtime recovery and a queued collection teardown callback also retain valid focus/object ownership.

## Frozen candidate and tests

All evidence roots below are `build/vm-oracle/<run-id>/`; `report/ctest.log`, `ctest.xml`, `test-result.json`, original source archives/manifests and captures are retained.

| Evidence | Result and applicability |
| --- | --- |
| `studio-catalog-full-20261031-ab` | **42/42 full CTests pass**, zero failures/skips, 501.63 s. Source `f50dd1d1015e97882794eefbe933dac58eae274b4440e3c263acc8826da3b341`. Includes schema/atomicity/profile/assets, Kodi request counts and Home/Studio navigation. |
| `studio-catalog-header-20261031-af` | **3/3 focused CTests pass**, zero failures/skips, 57.99 s: Studio quick, selection and artwork. **61 runner safety tests pass**. Source `ccf58223c9d5fc58413c3cb54acbd2e2b08960825a0f6a1ff5e69a6e694d6ebd`. Strict reuse verifies original AB source/payload and permits only four existing QML views plus named tests/harness/docs. AF is not another full-suite claim. |
| AF installed identity | Release `49dae92cfc6de4b97f7b48d0`; previous AE `2a18649a1f0b320bc943fe3f` retained. All **217 current application/build/config files** match AF's frozen raw-byte manifest (`build/catalog-applicability.json`). |
| Native I/Y | `studio-catalog-native-20261031-i` (5.62 s) and `studio-catalog-files-20261031-y` (5.27 s) pass component contracts: real GIF staging, failed-write rollback, Apply/reload, saved-design export/restoration, body bounds and browser no-write/refusal, including an actual POSIX FIFO refused before opening. Included again by full AB. |
| Native T/X/AA | `studio-catalog-raster-20261031-t` proves distinct actual decoded red/blue Canvas/scene pixels; X navigation passes 71.44 s; AA artwork passes 9.65 s. Final AF retains those regressions and adds portrait/unit/caption/opacity/live-zoom geometry, stable controls, empty-folder recovery, header-origin selection, pointer/remote parity and framing cancel/Undo/no-op proofs. |
| Wrapper contracts | `build/test-all-contract-2edf40015e244cfb9acf9087da2a66fd/` passes. No application full rerun for later helper/docs changes. |

The fourth bounded reuse mode refuses C++/configuration/shared raster/build/unknown runtime changes, new/deleted files, focused/failed bases and modified payloads. It runs all three affected Studio registrations together and stages the rebuilt application. This is scoped evidence reuse under the task's testing budget; T31 still freezes one final integrated candidate.

## Installed catalog walkthrough

AF `Visual -Cases catalog -Installed` passes using SHA-addressed helper `4ac311c154eafc9707f2c54d5c718ebbf2863b350a67641eeb30d057a4e01a07`. Guest evidence `/tmp/isolde-components-acceptance-w29pf8jn`; host `visual/catalog/attempt-20261031-085231-001/` and `report/evidence-catalog-1793462139091078215/`.

Actual X11 remote input imports a real two-frame GIF, refuses a malformed GIF without layout writes, previews framing, cancels the complete framing draft, accepts one focal edit and enables motion. Apply persists exact managed GIF bytes (SHA256 `fb0a3bf4a0600526aa3a73a0a73b3eedc0ed4e6766355577bf8c571144965eb6`). The same journey adds real Shape, Text, linked Button and container; independently changes Text body/visible label while preserving its editor name; selects the decorative Image through Layers and adds a Home page link. It asserts all six persisted nodes, exact types/actions/body/label/container structure, then closes and reopens without changing saved bytes.

All **23 catalog captures** reviewed (`build/catalog-review/catalog-final/manifest.json`, 22 distinct hashes). Controls/focus, source/result framing, invalid retry, actual primitive structure and Text independence are readable. Runtime shows the real rose SECOND frame after blue FIRST during import/editor; the two runtime captures are identical to each other and are not treated as a distinct-frame comparison. Native Canvas/scene pixel assertions provide that comparison. Normal Household/Studio Demo layout hashes are unchanged; the service is restored active/running, zero restarts.

Native review: 122 earlier files/42 distinct hashes plus two final unique framing captures reviewed (`build/catalog-review/native/` and `native-final/`). Final portrait framing uses the real card/caption/corners/opacity. All 48 captures from rejected AE attempts are reviewed and rejected as enclosing acceptance (`installed-failed/`). No blank/misrouted capture earns acceptance.

## Diagnosed failures and testing budget

- J/K/L/O/R/S real-GIF pixel failures were retained. An obscuring test sibling was removed, then the actual subpixel Canvas crop defect was fixed; T and AB/AF pass genuine different-pixel assertions.
- Full U fails **41/42** at `studioScrollingAddMoveAndCancel`: it expected runtime focus on now-decorative Text. Explicit valid linked Buttons fix that fixture; V caught malformed initial Button fixtures rather than accepting fallback content. This demonstrated full-suite failure justified corrected full AB, recorded before launching. No additional full run followed small view/helper fixes.
- W/Z exposed changing framing labels rebuilding delegates during pointer input and an invisible duplicate lookup. Stable delegates/unique visible controls pass AA and AB/AF. AD's portrait test matched geometry but edited before source readiness; explicit readiness/binding waits pass AE/AF without weakening assertions. AC's safety fixture used the wrong fake payload path and stopped before configuration; corrected fixture retains the production guard.
- Installed AE attempt 1 selected startup's Ambient folder instead of a GIF; the fixture now enters a dedicated sorted input folder. Attempt 2 double Confirm reopened Review after successful GIF Apply; corrected input sends one Confirm. Attempt 3 exposed a real stale header selection after Add; the production transition and regression are fixed in AF. None of the three enclosing attempts is accepted.
- An explicit `-File -Cases studio,lounge,pastel` shell invocation failed argument validation before VM activity; the default three-case invocation is used. Host Python's WindowsApps alias was unavailable; no host toolchain was installed. Linux safety tests run through the bounded runner.

## Guest storage and supported boundary

Necessary guest-only maintenance preserved profiles, releases, logs/manifests/screenshots and incremental build cache. B removed obsolete staged/source artifacts; E removed disposable fixture caches on native `/tmp` (a separate filesystem). G/H/N restored **74 exact original source archives** from workspace with SHA verification, then removed only matching extracted guest copies. Q verified host-retained original backups and pruned matching obsolete guest archives only where no source/payload/stopped-profile receipt depended on them. Recover originals through `Configure -ConfigureTarget artifacts -RestoreSourceRunIds`; no host archive or personal data was deleted.

Maintenance honestly did not reach its 3 GiB target; later Test's independent 2 GiB guard passed (AF 2.08 GiB before deployment). Both verified archive recovery and backup-receipt pruning retain strict native-path/member/hash/link/mount checks, with bounded IDs and safety regressions. This is VM software proof. Hardware/service/personal usability handoff remains outside S1 percentages; no third-party playback, physical device or full-product T31 acceptance is inferred from this catalog work.

## Final standing acceptance and handoff

Installed AF Studio passes (`/tmp/isolde-components-acceptance-pus18hbw`, 15 captures), including draft cancellation/reflow/size, Undo/Redo, preview interception, atomic Apply/reopen, nested input-slot editing/Layers and scrolling persistence. The input slots explicitly report unavailable capture hardware.

Lounge passes (`/tmp/isolde-reference-config-vv21rn6d`, 4 captures); Pastel passes (`/tmp/isolde-reference-config-7hbg0y25`, 14 captures). Both exercise packaged/saved design round trip and explicit draft reset/Apply while preserving other pages. Agent compared each recreation against its original `tests/visual/home-reference/reference.jpg` / `home-pastel-reference/reference.jpg`, and reviewed both reset/default results. The same generic renderer preserves Lounge's hero/media/sources/Guide composition and Pastel's sky, round editable links, app/history collections, typography, focus and readable contextual controls. Deterministic Kodi artwork/history/music boundary responses in the Pastel fixture exercise production components; they do not claim actual playback or third-party viewing. Its app overflow/exact return, history See all/exact heading return, music/tracks, Games, Guide and Search captures are also reviewed.

All **56 final installed captures** are reviewed: catalog 23, Studio 15, Lounge 4, Pastel 14. Review maps/hashes live in `build/catalog-review/{catalog-final,studio-final,references-final,pastel-final}/manifest.json`; `build/catalog-final-review.json` records source applicability and result paths. The runner correctly leaves subjective/semantic fields pending automatically; this record is the explicit agent verdict accepting the T21.8 software behavior. No acceptance is inferred solely from its process return code.

Final bounded Status: no active run, guardian false; service active/running PID2354701, zero restarts, installed AF release unchanged, normal Household/Studio Demo hashes unchanged. Guest free 1.73 GiB; inspect/maintain only the verified guest artifacts before another Test if the 2 GiB guard blocks. No known required catalog software defect remains. `tools/backlog-progress.ps1 -CheckTable` passes: T21 62%, sum1011, Overall37%, 34/52 credited milestones. Next is S06/T21.9 reconciliation; its original gate remains open until that owning review is complete. Preserve unrelated staged/unstaged Guide/template work when committing this logical catalog change.
