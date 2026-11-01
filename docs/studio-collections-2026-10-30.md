# T21.6 App/Button collections — accepted 2026-10-30

S03 is complete at VM software scope. T21.6.a–c and the original T21.6 Scope group are accepted. T21's history/provider, remaining catalog/appearance/header and broad usability gates stay open.

## Software correction

Store package installation was still connected to unconditional `HomeLayout::addAppTile`: registering an app could author an additional page placement even when the page already contained a genuine App collection. Collection pages now follow registry membership without that extra placement or a change to explicit Selected membership. Legacy pages retain their prior registration behavior. `studioCollectionInstallDoesNotAuthorPlacements` uses the actual AppCatalog install/remove/reinstall path in both All and Selected modes and asserts unchanged committed data, saved bytes and layout-change signal count.

The earlier `463b250` correction names the data-defined catalog/prototype **App carousel**, keeping compatibility ID `app-bar` and existing stored layouts intact. The installed Studio catalog capture now verifies that name.

## Candidate and checks

- Focused `studio-collections-contracts-20261030-b`: component contracts pass in 5.32 s, including the new native regression. The earlier selection A passes in 38.72 s: actual remote/pointer ordering parity, cancellation, Android eligibility, registry changes while a gallery is open, missing/reinstall recovery and no saved-layout write.
- Full **`studio-collections-full-20261030-e` passes 41/41**, zero failed/skipped, 472.28 s; 53 runner self-tests pass. Source digest `f213fc2601b1f3b0b76f73a0768e82287ce50498a9176ba4e38af8e26664f716`.
- Installed release **`0a3c1c22c2da3ab3282b6b7a`**, executable SHA-256 `6599218c51012441f2ae974ec3c945652043ba0dc776f84ab71e182c2b15134b`. Previous J release `0da0692cbb6b2354bed31852` remains available for rollback. `build/t21-final-review.json` verifies current production bytes against the full E manifest, installed identity, functional receipts and capture hashes.
- Portable helper identity/frozen-dependency, complete lifecycle result predicate and evidence privacy tests pass; `tools/test-test-all.ps1` passes. The new `collections` case uses the existing bounded runner, locks, timeout, service lease and SHA-addressed helper overlay. No frozen source/payload was edited.

## Requirement reconciliation

| Outcome | Evidence and acceptance |
| --- | --- |
| Genuine All/Selected membership, stable IDs and authored Button children | Existing full G native/schema/source audit in `build/t21-collections-reconciliation.json`, now superseded by current full E coverage. `studioButtonCollectionsAreBoundedAndAtomic` accepts 128 children, rejects 129 without mutation, retains previously missing app IDs, refuses newly unknown IDs and verifies exact subtree remove/Undo with no saved write. Duplicate/unsafe app identities and invalid child layouts are refused. |
| Remote ordering, independent capacity and registry updates | Selection A/full E exercise real remote/pointer app ordering and cancellation, explicit bounds, capacity-one overflow, All/Selected/group filtering, Android entries and registry removal/reinstall while More is open. Ordered insertion and locked/overflow cancellation pass for actual Button carousel children. Changing capacity retains the authored collection. |
| Scoped galleries and exact native return | Installed `collections/attempt-20261030-213920-469`, `/tmp/isolde-components-acceptance-d63zukrn`: 17 reviewed captures. Selected capacity one renders only More apps; its gallery contains YouTube and FreeTube in authored order, excluding unrelated apps. More buttons contains precisely its three authored children. Both galleries retain the selected native item after normal exit; immediate Confirm launches that same item again. |
| Actual input, removal, unavailable state, reinstall and no page mutation | Six isolated native client launches traverse real ActionDispatcher/SessionManager/DesktopSession/NativeSandbox. Each receives Right, records state and exits normally on Escape; the shell log contains six normal-exit receipts. Store removal changes All apps from overflow to two members; Selected preserves a repairable unavailable identity and refuses its launch. Store reinstall restores overflow and the original selected app, which launches and returns successfully. Saved page bytes remain identical throughout, with no generated per-app placements. |
| Installed Studio and generic reference/reset gates | Studio `attempt-20261030-214103-485`, `/tmp/isolde-components-acceptance-k1t9nxig`: 15 reviewed captures, including current App carousel label, draft/cancel/Apply, nested scope and scrolling. Lounge `attempt-20261030-214325-579`, `/tmp/isolde-reference-config-zhlio4qy`: four captures. Pastel `attempt-20261030-214422-112`, `/tmp/isolde-reference-config-hdof07kz`: 14 captures, including actual app overflow/Back and history See All/single-focus return. Both original reference JPEGs were reopened and compared with the recreation/reset pairs. |

**50 installed captures and all 31 full-E native captures were opened and reviewed.** The final artifact records their hashes. This accepts collection behavior and the standing regression pairs, not remaining T21 visual/provider fidelity. Pastel's geometric history artwork and installed-state provider responses remain explicit fixtures; they are not actual viewed-media or subscription-client evidence.

The desktop boundary fixture supplies a private interactive X11 client behind the ordinary FreeTube desktop registration. It proves real isolated-process input, exit and Isolde focus restoration; it does **not** claim FreeTube playback, third-party client compatibility, accounts or physical hardware. The production runtime/session adapter is not mocked. Native input updates and receipts are real, with atomic receipt replacement.

Every installed case restores the normal service active/running with zero automatic restarts. Household/Studio Demo layout hashes remain `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e` / `85865ee3f03c6301cb2b3153bacc1bde8fba01b2c005279f7cd86263311cb4cf`. Final bounded Status reports no live guardian, current E release, service PID 2154127 and 2.06 GiB guest free space.

## Failures and testing budget

Full C stopped before configure/build/tests at the 2 GiB guard (1.98 GiB). Verified maintenance D removed 21 recoverable guest source copies, preserved 74 without recovery evidence and all staged payloads/releases/profiles/build cache. It recovered 2.30 GiB but honestly failed its 3 GiB target; this exceeded Test's actual 2 GiB requirement, allowing E. No host maintenance occurred.

The first installed journey failed because its handcrafted v2 fixture omitted mandatory source bindings and fell back to defaults. The second reached the actual client/input but hit a receipt read/write race. Both are recorded failed attempts; explicit shell bindings and atomic fixture receipts correct the harness only. The third passes. There was **one executed full suite**, followed by helper-only retries on the same installed executable; no full run was repeated for those corrections or accounting.

T21 is now **48%**, six original groups accepted; sum **997**, Overall **37%** across 27 active tasks. Next is S04/T21.7: genuine bounded Kodi/app viewing history, provider isolation and actual playback/history updates. S03 has no remaining software blocker.
