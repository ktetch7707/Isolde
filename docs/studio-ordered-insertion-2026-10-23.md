# T20 nested ordered insertion — 2026-10-23

Move now recognizes an editable group's Row/Column policy, including a button carousel, and previews insertion directly in that layout. It does not change the policy or require a before/after picker. Remote directions and actual pointer targets use the same `insertRelative` transaction; advanced reading order remains independent.

The command sorts siblings by authored placement, moves the selected identity between slots and preserves each component's span and the existing sequence of gaps. It retains the group's capacity, wrap, style and all members, and refuses moving a locked component or shifting a locked neighbor. Only placement coordinates change; document array/reading order, other groups and unrelated page components stay intact. Invalid targets and non-boolean directions refuse atomically.

All candidate changes are private until Confirm/drop. Cancel restores the source document, history count, selection and exact nested/page viewport. A failed insertion cannot accept a stale previous preview. Stationary outlines and immediate neighbor movement retain the same information with reduced motion.

Carousel edge movement is bounded to a viewport per deliberate pointer movement. Frozen original slots follow the actual nested surface's scroll offset, so reflow does not move the pointer's targets. Remote overflow and pointer edge movement must produce byte-identical results; holding at an exhausted edge cannot grow the document or delete overflow members. Explicit group/section transfer and advanced size/position entry remain separate existing paths.

## Verification

- `t20-ordered-20261023-a`: destination/ordered QML registration passes in 25.63 seconds. Real button-carousel and nested-column pointer/remote parity, locked refusal, mixed spans/gaps, bounded overflow, exact viewport cancellation and no disk writes pass. Both captured images were opened; the column image was taken before the controls painted, so capture timing is corrected with the production Item's actual render condition before final verification.
- Native `studioOrderedInsertionPreservesSlotsAndIsAtomic` adds malformed/cross-scope refusal, complete result inspection, unchanged group/outside node, private preview, one-step undo, locked rollback and independent reading-order coverage. Full-g supplies its execution evidence below.
- Run b fails the native navigation invocation because adding a QML function argument changed its invokable signature. Restored the original one-argument entrypoint with a separate explicit-scope helper. Its capture check also waited on a nonpainting root and timed out; corrected to the actual painted Move controls rectangle. No timeout or semantic assertion is relaxed. The diagnostic row/column images were opened and show the expected controls and neighbor order.
- Run c confirms the navigation correction, but the panel's `waitForRendering` still waits for a later frame instead of establishing the already-painted state. The test now checks an actual background pixel from the rendered control panel, using Qt Quick Test's documented [grabImage/pixel API](https://doc.qt.io/qt-6.10/qml-qttest-testcase.html#grabImage-method), before saving the full capture. Run d passes in 27.04 seconds; both fully painted captures were opened. Run c's fourteen images are byte-identical to already reviewed images. No timeout is increased.
- Full-e passes component contracts and the focused Studio journeys, but the monolithic Home navigation registration reaches its 60-second bound near the final catalog scenario, without an assertion failure. It is not a passing integration checkpoint. Runtime and Studio navigation now have separate bounded registrations; a runner self-test compares their combined names against every native Qt Test slot and refuses omissions/duplicates. The existing `home_component_navigation` runner choice executes both, so its scope is preserved. No timeout is increased. A null-model warning seen during teardown is guarded at the model boundary.
- Menu-started pointer insertion now exposes full-size clickable slots through the same preview command and Place action. Ordered input cases move to their own `studio_ordered` registration, retaining the 30-second bound. Run f passes in **22.05 seconds**, including both row/column menu → pointer slot → Place canonical parity as well as drag, remote and bounded overflow journeys. Both final slot-preview captures were opened. Full-g checks the final source batch and the navigation split.

## Full native checkpoint

`t20-ordered-20261023-full-g` passes **23/23 CTest registrations in 227.23 seconds**. Source digest `5f77c4ebfd4e8f0f09b814137790de044c15a7fe559d7a42557602de7885e72c`; archive SHA256 `d0c702ac115ead1121415c25e6a3b6620b54c2fae2e80a55ad9039ccf0cd66df`. Ordered input passes in 21.92 seconds; runtime navigation in 6.99 seconds and Studio navigation in 49.43 seconds, within their existing bounds. Runner self-tests pass all 20 cases, including exact navigation-slot coverage.

Retrieved the completed CTest detail log through the bounded transport helper to `build/vm-oracle/t20-ordered-20261023-full-g/report/LastTest.log`; it explicitly records `PASS` for the new native insertion test. All fourteen offscreen captures match opened/reviewed images from run f and the preceding group-target checkpoint. The staged graphical journeys remain a separate visual review.

## Graphical checkpoint and remaining gates

The exact staged executable passes all three functional journeys. All **23** captures under `build/vm-oracle/t20-ordered-20261023-full-g/visual/` were opened and reviewed:

- Studio attempt `20261023-100829-752`, `/tmp/isolde-components-acceptance-1cbvpply`: 15 images; movement/reflow, size controls, nested editing, Apply and scrolling remain functional. These sparse windowed diagnostic scenes do not establish finished TV usability. Existing Inspector verbosity and duplicate preview text remain open quality issues.
- Lounge attempt `20261023-101018-584`, `/tmp/isolde-reference-config-ygxurmpp`: four images; configured landscape composition and reset pass against the retained reference, with the previously documented illustrative artwork.
- Pastel attempt `20261023-101114-365`, `/tmp/isolde-reference-config-p9xhfedm`: four images; save/reapply/reset passes, but reference recreation remains **incomplete** because the isolated profile has one fallback-initial app and unavailable history. No design acceptance is inferred from the runner's functional pass.

Household layout SHA256 remains `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e`; Studio Demo remains `895804ffc1eb2167f5b723047116564742f70701a6d07eaf705b9928c59efdac`. The normal service is restored active/running, PID 1962609, zero automatic restarts. Installed release remains `fc815f2341cb375a0b809118`. No deployment or host maintenance occurred; unrelated working-tree changes are preserved.

T20.5 still requires the complete free-grid mixed-size/full/locked pointer replay, decorative overlap policy, enabled reduced-motion replay, cross-section target and installed desktop/TV acceptance. Populated Pastel remains a standing visual gate. No checkbox closes: T20 80%; Overall round(866/24) = 36% across 24 active tasks.

No deployment, task checkbox or broad visual acceptance is claimed. T20 remains 80%; Overall round(866/24) = 36% across 24 active tasks. The remaining complete movement policy matrix, installed acceptance and populated Pastel standing gate remain open.
