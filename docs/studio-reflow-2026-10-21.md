# T19: direct movement and live reflow repair

User request: review the coder's changes and make movement/reflow work. This slice implements application behavior; the separate color-picker gate remains open.

## Review findings

- Recent menu changes added Edit contents without dispatching that entry; it fell through to deletion. Internal action-index changes also made the visible Duplicate entry call deletion. Restore the internal mapping and explicitly dispatch Edit contents.
- Ordered-layout movement already used a reversible proposal model, but the generic manipulation suppression also hid the selected component. Keep the real component visible in its live arrangement and show its insertion position.
- Grid reflow already had a bounded transactional solver. Its empty-cell search always began at the top-left, creating unnecessary visual jumps. Search nearest original positions first, with stable row-major tie-breaking and the existing 64-affected/4096-attempt bounds.
- Ordinary blocked drops still opened the legacy collision-choice dialog. Keep the selected item and blocked target on the canvas with the concrete reason; another direction can find a valid drop, Back cancels. Explicit advanced collision strategies remain available.
- Cancellation revealed the source placement rather than restoring the exact prior viewport. Snapshot page/navigation/nested-container scroll at gesture entry and restore it after proposal delegates and queued selection reveals settle.
- Group/library proposals were generic HomeCards. Render structured components and descendants through the shared renderer. Proposal replicas do not register live focus/geometry/provider clients or hide themselves through the source's suppression list.
- Pointer cancellation did not use gesture rollback. It now restores the same source scope/viewport as remote cancellation.

The inherited, uncommitted constants refactor initially did not compile: HomeLayout helper functions accessed private constants and two contrast thresholds were missing. Its safe-ID expression also lost the end anchor. Those prerequisites were repaired without discarding the refactor or unrelated documentation/evidence. They are maintenance work, not delivery of the requested component semantics.

## Verification

First run `studio-reflow-fix-20261021-a` stopped on the diagnosed HomeLayout compilation errors. No repeated clean builds or VM reset was used. The corrected full run and graphical evidence are recorded below.

`studio-reflow-fix-20261021-b` passed the other 13 suites but the new insertion visibility assertion had no QQuickWindow to instantiate its visual delegates; its wait also pushed the navigation suite past its existing 60-second limit. Correcting the test window produced `studio-reflow-fix-20261021-c`, navigation PASS in 39.30 seconds. The final unchanged-source full run `studio-reflow-release-20261021` passed **14/14** suites in **94.94 seconds**, including the viewport/blocked-drop/menu regressions. The timeout was not raised or disabled.

Final archive SHA-256: `8f9fee390682c6f01ee95a8f31f2af8f9f4c13db2c7b0091e93bfdb5432c42b9`. Verified source digest: `b9099d48530531032c1d1931224c7e1fadab42fb2880e68b4ef3b8dbeab7ee3f`. Host logs and JUnit: `build/vm-oracle/studio-reflow-release-20261021/report/`. The reviewed constants prerequisite was preserved separately as `4e2a41f`; unrelated documentation and PageData header commentary were not staged with it.

Canonical Deploy activated release `5064b3ae1002063c2d68e018`, previous `0c44d5b09fcccc7e1aa5ad96`, with the service active/running and zero restarts. Installed graphical verification uses that exact release, not an older executable. Capture review and final service-restoration evidence follow below.

`Visual -Installed` passed all three cases. All **16 captures were actually viewed**, and both fixture reference images were opened for comparison:

- Studio: `build/vm-oracle/studio-reflow-release-20261021/visual/studio/attempt-20261021-083344-433/` (12 captures). `studio-reflow.png` shows Clock moving into an occupied cell with the neighboring card displaced live and no ordinary collision dialog. Size, Apply/reopen, inspector and nested group journeys retain their established behavior. Existing square selection chrome, instructional text and generic inspector rows remain visible and are still pending their owning UX gates; this is not overall editor design approval.
- Lounge recreation/reset: `visual/lounge/attempt-20261021-083503-071/` under the same run, guest `/tmp/isolde-reference-config-lppwqs6k`. The landscape adaptation retains the reference's imagery, categories, card styling and content; reset returns to packaged defaults.
- Pastel recreation/reset: `visual/pastel/attempt-20261021-083549-554/`, guest `/tmp/isolde-reference-config-b9682wbj`. The established circular launchers, pastel surfaces, wallpaper and card rows remain intact; reset returns to defaults. This appearance fixture still does not prove genuine app/history collection semantics.

Studio guest evidence: `/tmp/isolde-components-acceptance-p3avmxy5`. Final guardian status reports service active/running, PID `1379042`, zero restarts, on the selected release. Normal saved layout hash before/after each graphical case is unchanged: `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e`. No personal layout mutation, reset, reboot or release change during capture occurred. All 12 implementation/prerequisite source files were compared byte-for-byte by SHA-256 with the final frozen manifest before committing the movement slice.

Extended navigation coverage checks blocked-drop recovery without a modal dialog or draft/history mutation; Duplicate dispatch; visible ordered insertion; scroll restoration after cross-section cancellation. Existing suites retain direct reflow/drop/undo, bounds, mixed-size/cascade/locked/full-grid refusal, atomic failure, section/group scope and pointer/remote/inspector parity cases.

T19.6/T20.5 remain open until their entire acceptance scope is met, including broader insertion/drag targets and decorative overlap policy coverage. This slice does not claim every Page Studio feature, color selection, new collection semantics, hardware qualification, disconnected cold boot or uncoached user acceptance. Current task percentages remain T19 83%, T20 80%, T21 36%; Overall round(849/24) = 35%.
