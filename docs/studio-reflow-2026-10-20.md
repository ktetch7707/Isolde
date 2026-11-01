# T19 first usability implementation slice

User authorization: begin implementing the revised Studio UX. Sequencing remains T19.6 before T20/T21. This slice changes application code, not just planning; it does not close the entire reopened T19 gate.

## Behavior

- Component context is Move, Edit component, Duplicate, Delete. Deletion remains confirmed and draft-only; failed deletion stays in its dialog. Undo through Studio restores the deleted selection when that component returns.
- Ordered legacy layouts move directly with D-pad insertion and a live proposal model. No Move before/after picker or grid conversion is required. The proposal preserves identities, category and parent validation; Back drops the proposal, Confirm accepts one undo record.
- Grid Move uses the existing bounded shift solver for a visible affected-neighbor preview. Ordinary valid occupied drops accept that complete arrangement in one operation. Explicit alternate collision proposals remain available for exceptional/advanced paths. The actual draft and disk stay unchanged during preview; full/locked/bounds refusal remains in the shared solver.
- Width/height leave the first context and are reached through Edit component → Layout → Size controls. Following the user's follow-up during implementation, both are now sliders, from 1 to the full current grid/container span. Pointer dragging and remote Left/Right preview the same candidate. Growing toward an edge visibly re-anchors within scope instead of silently shortening the range. Done accepts one complete arrangement; Back cancels every affected placement. No-room proposals remain visible and cannot be committed. The compact panel can be hidden to see the page; exact coordinates remain under Advanced.
- Movement freezes data, and routine canvas/movement footers are shorter. No schema/store/dependency or user-layout migration was introduced. Pointer grid release uses the same default placement acceptance; the broader T20.5 parity/target work remains open.

## Source and verification

The previously uncommitted Studio catalog/inspector implementation was preserved as prerequisite commit `1b47457`, using the byte-verified source snapshot of the previously tested `studio-entry-final-20261020` build. Working files were not reset or overwritten. This makes the new slice reproducible without hiding its API dependencies in a dirty working tree. Existing unrelated documentation/evidence was left alone.

Current slice files: `qml/PageStudioView.qml`, `tests/tst_home_navigation.cpp`, `tools/home-components-acceptance.py`. Native tests now exercise the revised actual context and inspector navigation rather than the retired menu indices. They verify exact context choices, a live grid displacement without draft/history changes, cancellation restoring visible neighbors, whole-drop undo, ordered-layout live preview/cancel/accept and pointer/inspector parity. Older exceptional collision and anchor/bounds cases remain covered.

Full run `studio-reflow-full-20261020` passed 14/14 CTest suites. Archive SHA-256 `30fc0123eb9d1a86e0c12ab1aac0a12af268ec6617f04c98eda3b70c99f076b3`; manifest digest `b9da639bdbe83662fdad3c641febcbb5fdd0e9fc917191e5c388330d5d16aea1`. Logs, CTest results and tested payload identity are at `build/vm-oracle/studio-reflow-full-20261020/report/`.

That preliminary reflow build also passed staged Studio (12 captures), lounge and pastel recreation/reset (four captures). All 16 images were viewed. User feedback then added the primary size sliders; `studio-size-controls-20261020` passed the updated native navigation suite, including exact slider ranges, refusing an occupied full-page proposal, cancel preserving draft/history, and optional Advanced position parity. Final release verification uses the newer snapshot below; preliminary screenshots are not presented as slider evidence.

## Final slider release

`studio-slider-release-20261020` passed all 14 native suites (98.58 seconds CTest). Archive SHA-256 `f5c4b139b7e08a4b21d4eaec27f607c9f590be8df16833081b5e7752fbaf428f`; manifest digest `b7e733e03018b13bedd70f6a53ead0cf76707fdee24b8e0843af6884b37def1b`. The three implementation files' current bytes were checked against that exact manifest before committing.

Canonical Deploy installed and restarted release `0c44d5b09fcccc7e1aa5ad96`; previous is `d8c9df6d050177e70eacfe2e`. The runner verified the running executable. `Visual -Installed` exercised this selected release rather than an old binary. Installed Studio's 12 captures passed, including real neighbor reflow and the new size sliders; the size panel sits beside this selected Clock and the full-span endpoints are visible. Installed lounge recreation/reset and skin round-trip also passed. Logs/deployment/payload records: `build/vm-oracle/studio-slider-release-20261020/report/`.

Slider capture: `build/vm-oracle/studio-slider-release-20261020/visual/studio/attempt-20261020-215321-216/studio-resize.png`. Studio capture directory has entry/catalog/edit/reflow/size/Try/Apply/group/inspector/content/inside/Layers evidence. Lounge pair: `visual/lounge/attempt-20261020-215440-338/` under the same run.

Installed pastel recreation/reset and skin round-trip passed too, at `visual/pastel/attempt-20261020-215526-478/`. All 16 final installed captures were viewed. Both reference recreations preserve their established appearance; reset returns to the packaged default. These illustrative fixtures still do not establish the newly planned real app/history carousel semantics.

After all three graphical runs, the normal service was restored active/running, zero restarts (final PID `1254913`), on selected release `0c44d5b09fcccc7e1aa5ad96`. Normal saved layout hash remained `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e`; no normal-profile layout change, release switch during capture, reset or reboot occurred.

## Remaining scope

T19.6 stays unchecked pending complete insertion-target, viewport-cancel and exceptional scope/policy coverage. T20.5 pointer refinement, T21 shared Home selection styling, editable header, tool relocation, visual color wheel, full type-specific/proportional size UX and genuine app/history carousels are still open. Sliders currently apply to grid/container placements; automatically sized legacy ordered pages retain their sizing policy and are not silently converted. Exact-position Advanced controls remain technical. Existing More tools and selection overlays remain for their owning gates. No physical-device qualification, offline Generator Test or user design acceptance is claimed. T19 remains 5/6 = 83%; Overall remains round(849/24) = 35%.
