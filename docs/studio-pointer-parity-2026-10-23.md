# T20 pointer movement and resize parity — 2026-10-23

This slice addresses T20.5 without closing its wider acceptance gate.

## Implementation

- Grid dragging uses stable editor coordinates with scroll displacement accounted for. A click remains selection/context; movement starts only after the platform drag threshold.
- Back during a held drag cancels the transaction. A subsequent mouse release cannot accept the canceled proposal or reopen context. Resize release/cancel is also guarded by its active operation.
- Resize now uses the same live deterministic neighbor-reflow proposal as Move and Size. Impossible ordinary resize drops retain the candidate/reason on canvas without forcing a collision dialog. Explicit alternate collision strategies remain available.
- Ordered layouts expose pointer hit targets through the runtime renderer's registered geometry. Dragging previews insertion through the same orderRelative command as remote movement, without conversion. Insertion targets are frozen at gesture start; preview reflow does not retarget a stationary pointer. Pointer delegates use the baseline draft model so a proposal-model reorder cannot destroy the active mouse grab.
- Scope filtering remains explicit: pointer overlap never reparents an item or changes collection membership. Runtime media actions remain intercepted in Studio.
- Pointer edge movement uses the same remote candidate step/reveal path, bounded to one viewport per deliberate edge movement. A stationary held pointer stops growing the document after that budget; release commits the complete placement once.
- Alignment/distribution now has a labeled visual pad and live session proposal, explicit Done/Cancel, complete undo and readable refusal. A rejected proposal cannot leave an older valid arrangement available to accept. Controls are placed opposite the selected component using shared geometry.
- Group layout now has Grid/Row/Column choices, a cell diagram, row/column steppers and gap/padding sliders in `StudioLayoutEditor.qml`. Mode changes explicitly propose the required one-axis dimensions without clamping children. Preview uses the real renderer; Done accepts one transaction and Back cancels. Controls can collapse to reveal a large group; Menu/Confirm provides the remote equivalent.

## Tests and evidence

New production-QML input scenarios cover threshold/click behavior, cancel while held then release, pointer/remote canonical parity, exact undo, full-grid resize refusal, scrolling resize with multiple-neighbor reflow, and ordered insertion/cancel/remote parity. Test profiles are temporary and disk writes are checked separately from draft edits.

- `t20-pointer-parity-20261023-a`: host sandbox denied the local SSH connection; no guest build.
- `t20-pointer-parity-20261023-b`: stopped at the guest 2 GiB capacity guard before build. Three obsolete guest staged payloads were removed under both test locks after resolved-path, symlink and mount checks. Profiles, installed releases and reports retained. Record: `build/t20-pointer-cleanup.log`; free afterward 2,343,591,936 bytes.
- `t20-pointer-parity-20261023-c`: all four focused Studio registrations passed before ordered pointer insertion was added.
- `t20-pointer-parity-20261023-d`: all three new gesture scenarios reported PASS, but the expanded combined Quick Test registration hit its unchanged 30-second deadline before completing all existing controls. This is a failed run. Gesture scenarios are now split into `studio_gestures`, retaining the 30-second bound and full assertions; the aggregate runner includes it.
- `t20-pointer-parity-20261023-full-a`: invalid intermediate snapshot after the host Python alias failed to create the new test file. Not acceptance evidence. File creation was corrected using workspace-only PowerShell file operations; no host toolchain was installed.

## Verified checkpoints and visual limits

`t20-pointer-parity-20261023-full-b` passes 19/19 full CTest registrations in 149.34 seconds. Studio and both skin save/reapply/reset journeys pass against its staged executable. All 23 captures were opened/reviewed. Lounge retains its landscape recreation. **Pastel's isolated-profile capture is not full reference-composition acceptance:** it has one available app and an unavailable Kodi history panel, rather than the populated app/history rows required by the reference. Theme, geometry and reset work, but this is incomplete visual evidence. The populated installed Studio Demo is a separate checkpoint; do not substitute it for this isolated test's missing content. Existing verbose inspector/mixed-menu/default presentation remains unfinished product UI, not accepted polish.

The three graphical cases preserve both normal-profile layout hashes exactly: Household `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e`, Studio Demo `895804ffc1eb2167f5b723047116564742f70701a6d07eaf705b9928c59efdac`. Service restored active/running with zero restarts. Captures/results: `build/vm-oracle/t20-pointer-parity-20261023-full-b/visual/` and `report/`.

Focused follow-ups:

- `t20-arrangement-20261023-a`: gesture tests pass; capture review rejects the panel obscuring selection. Corrected placement and larger labels are verified by `t20-arrangement-20261023-c`, including a selection-visibility assertion and reviewed `report/studio-test-arrange-preview.png`.
- `t20-layout-controls-20261023-a`: fails the new group-mode test because Row was offered without its required single-row dimensions. Fixed at the UI proposal boundary; no schema or assertions weakened. `t20-layout-controls-20261023-b` passes all gesture cases; reviewed `report/studio-test-group-layout.png`.
- `t20-pointer-edge-20261023-a`: gesture registration passes in 22.06 seconds, including actual pointer edge growth, idle budget exhaustion and canonical remote parity.
- Capacity stops are retained in `t20-arrangement-20261023-b` and `t20-controls-20261023-full-a`. Guest-only cleanup preserved old source and Testing logs before removing two obsolete build caches; removed redundant archives only where extracted source remained, plus one obsolete staged payload. Checks include both test locks, resolved paths, mount boundaries and process-use checks for build/payload directories. Records: `build/t20-arrangement-cleanup.log`, `build/t20-arrangement-archives-cleanup.log`, `build/t20-controls-capacity-cleanup.log`. No installed release/profile or host maintenance changed.

`t20-controls-20261023-full-b` passes 19/19 registrations in 174.60 seconds (source digest `3668bac5bb203d1d8906c747fef9d85fb5127ce4fca2cc2fc4d8871adde5e39b`). Its Studio and both save/reapply/reset journeys pass; all 23 captures were opened/reviewed, retaining the explicit Pastel composition limitation above. Both normal-profile hashes remain unchanged and the restored service reports zero restarts. This is an intermediate checkpoint before normal Inspector routing and the final control-input checks.

Integration review caught the old Inspector entry still opening its old layout list. It now enters the new visual editor and returns to the exact original field; the remote test enters through that actual configured field. Existing row visible-card capacity and wrap controls are retained, rather than lost in the replacement. `t20-inspector-layout-route-20261023-b` passes focused gestures in 19.84 seconds. Its preceding `-a` stopped at capacity. Broader inventory found 23 GiB of old pre-runner builds; six obsolete build directories were checked under both locks for resolved paths, mounts and active process use, with their source/ctest logs and copied Testing evidence retained. `build/t20-legacy-build-cleanup.log` records 5,499,600,896 bytes free afterward. No normal profile or installed release changed.

`t20-controls-20261023-full-c` fails the new capacity-pointer assertion; it is not a passing checkpoint. Input was synthesized before the newly visible row's positioner completed. `t20-controls-input-20261023-a` additionally rejects an inappropriate wait for a nonpainting container to render. The test now waits for the concrete capacity-row position below Padding, then clicks the actual controls. `t20-controls-input-20261023-b` passes in 22.55 seconds with capacity=2, wrap=true, both real sliders reaching 100, every child retained, exact cancellation and remote/pointer canonical assertions intact. Media data is frozen throughout arrangement and group-layout previews; stepper endpoints visibly refuse further adjustment.

## Final implementation checkpoint

`t20-controls-20261023-full-d` passes **19/19 registrations in 173.64 seconds**, including the actual Inspector route, capacity/wrap, real slider endpoints, freeze-data states, pointer edge budget and canonical parity. Archive SHA256 `f48974bc203fc8a817ffa372abf392b07a10fedca5c0a5d71d96a41d30d1212f`; source digest `c8cafb637b3b6701f804a12197f94e4fa379f69a5f954dfeb498bab0c44607e7`.

The exact staged executable passes the Studio graphical journey (`/tmp/isolde-components-acceptance-cnrpeov3`). All 15 captures under `build/vm-oracle/t20-controls-20261023-full-d/visual/studio/attempt-20261023-012954-474/` and all nine offscreen control captures in `report/` were opened/reviewed. New arrangement/group controls are readable, named and remote-operable; the alignment panel leaves its selected item visible, and a large group has explicit Hide/Show controls. This is scoped control QA, not acceptance of the existing verbose Inspector, mixed More menu, product default or the incomplete Pastel composition. The preceding full-b lounge/pastel save/reset captures and their explicit limitations remain recorded above; final changes after that checkpoint affect editor controls and data freezing, not the runtime Home renderer or presets.

Both normal-profile layout hashes remain exactly as recorded above. The normal service is restored active/running, PID 1862154, zero automatic restarts. No deployment was performed; installed release remains `fc815f2341cb375a0b809118`. Existing unrelated working-tree changes remain outside this slice.

No additional task credit is claimed. T20 remains 80%; overall remains round(866/24) = 36% across 24 active tasks (recomputed from the current table). T20.5 stays open until its complete input/policy matrix and installed desktop/TV acceptance pass, including the standing visual requirements above. Page-grid conversion still needs its own visual controls; the delivered visual grid editor here is specifically the group's internal layout.
