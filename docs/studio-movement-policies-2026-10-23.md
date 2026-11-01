# T20 movement policies and replay matrix — 2026-10-23

The shared placement proposal now honors the existing validated `grid.overlap` flag. Normal Move in an overlapping layout changes only the selected layer, even over a locked background; it does not scatter neighbors. The moving component's own/ancestor locks, grid bounds and atomic validation remain mandatory. Explicit Swap still evaluates its occupants and refuses a locked neighbor.

Page Layout exposes **Allow overlapping layers** as a remote/pointer checkbox with a distinct focus frame and private live preview. It uses the existing schema/default (`false`), not a new layout format. Disabling overlap over existing intersections refuses without changing the draft or overwriting placement. Back restores the original configuration. Move's status identifies a layer operation.

## Replay coverage

- Actual remote movement, pointer drag and menu-started pointer cell → Place produce identical document bytes on full one-cell and full mixed-span grids. Span and neighbor counts are checked, previews stay private, one accepted move is one undo record, Cancel restores the exact viewport, and Undo restores the complete document.
- Both grids replay with `motionEnabled=true` and `motionEnabled=false` through the real appearance model. The same placement semantics, candidate information and input parity remain available with motion disabled.
- A locked neighbor refuses both remote and pointer drops on a full grid. Pointer Cancel restores the viewport, document and history count.
- A real existing container serves as a decorative background panel; a layer moves over it without changing the locked panel. This tests generic overlap policy, not delivery of the separate Shape component semantics under T21.
- The overlap checkbox previews, cancels, accepts once and produces identical remote/pointer output. Disabling it with overlapping nodes keeps the stored/draft document intact and surfaces the validation refusal.
- Native `studioOverlayMovementKeepsNeighborsAndBounds` covers Shift/Cancel overlay proposals, unchanged neighbors, explicit locked Swap refusal, the moving item's own lock, bounds refusal and atomic disable rollback.

## Verification

`t20-policies-20261023-a` passes all seven QML policy scenarios in 29.42 seconds; its overlap-control capture was opened. Review adds a separate focus frame so checked state and focus are distinguishable. Since the combined registration nearly fills its existing 30-second bound, movement and overlay/control cases are partitioned into `studio_policies` and `studio_overlays`, both still bounded to 30 seconds. No scenario is removed and no timeout is increased. Full-b verifies this final batch and the new native boundary test.

`t20-policies-20261023-full-b` passes **25/25 native registrations in 228.58 seconds**. Source digest `473d6b1077fa711c99829875821265a436349bab1d0d7679d2097e336ce04698`; archive SHA256 `7a7e31b5b4032b8d8b0003a08eb0d03a0a6bc26ad77bf37e5b66568cd0594c6e`. Movement policy registration passes in 21.21 seconds; overlays in 8.58 seconds. The retrieved completed CTest detail log, `build/vm-oracle/t20-policies-20261023-full-b/report/LastTest.log`, explicitly records the new native overlap test's PASS.

The native run captures fifteen offscreen scenes. Twelve match already reviewed images byte-for-byte. Opened the three changed captures: page-grid, scrolling sections and focused/checked overlap control. The new control is readable, focus is distinct from checked state, and the selected authored item remains visible. Section controls scroll to remaining actions through the existing remote focus-reveal path. These synthetic scenes do not establish a populated Home's design acceptance; staged graphical review is separate.

No task checkbox, deployment or broad visual acceptance is claimed from the targeted pass. T20 stays 80%; Overall round(866/24) = 36% across 24 active tasks. Cross-section target/installed desktop-TV acceptance and the populated Pastel standing visual gate remain open.

## Graphical review and retained limitations

The same staged full-b executable passes Studio, Lounge and Pastel functional journeys. All 23 graphical captures have been opened and reviewed: Studio attempt `20261023-103225-768` (15), Lounge `20261023-103416-499` (4), Pastel `20261023-103511-425` (4), under `build/vm-oracle/t20-policies-20261023-full-b/visual/`. Guest evidence: `/tmp/isolde-components-acceptance-3xttgy4k`, `/tmp/isolde-reference-config-usgouw63`, `/tmp/isolde-reference-config-6b2mk1cz`.

Lounge retains the populated hero/history/source/guide composition. Pastel retains its background, header and source arrangement but its isolated profile has only an initial-based YouTube app and unavailable Kodi history. Its populated reference recreation is **incomplete**, despite successful save/reapply/reset. Sparse Studio fixtures establish operation visibility, not a polished default Home. The Inspector's verbose source notice, mixed tool organization and existing preview text remain unaccepted owning-task work. Reset's packaged default is also visually basic; no professional default-design acceptance is claimed.

Final report state is passed/finished; the normal service is active/running, PID 1970039, zero restarts. Household layout SHA256 remains `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e`; Studio Demo remains `895804ffc1eb2167f5b723047116564742f70701a6d07eaf705b9928c59efdac`. No deployment, guest cleanup or host maintenance was performed. Installed release stays `fc815f2341cb375a0b809118`.