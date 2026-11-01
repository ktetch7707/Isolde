# T19 remote editing verification — 2026-10-20

## Implemented behavior

Confirm selects a component and opens Move / Width / Height / Edit component. A deliberate Move starts the actual rendered component ghost; the origin stays marked. The operation shows one-based row/column, cell spans, current anchor and boundary feedback. Directional repeat advances exactly one valid cell at a time, stops at the bounded edge and never commits. Back restores the original placement, section and selection without a history entry.

Width/Height use a fixed visible top-left anchor by default. Menu offers position/span entry with the shared on-screen keyboard and four resize anchors. Anchors are transient editor choices; stored data remains validated positions and spans. Invalid numeric values are refused. Existing ordered layouts instead offer Move before/after without grid conversion; grid conversion remains an explicit preview. Grid before/after is labeled Reading order so it does not imply a spatial move.

Named component sections are valid destinations in scrolling layouts. Media shelves manage their own contents and are not drop containers. A proposed transfer changes placement and section together, supports validated cross-section Swap, and stays draft-only until accepted. Cancel and undo restore the source scope and exact selected identity. Unknown sections, locked movement and unsupported destination options are refused at both proposal and command boundaries.

Shift first tries empty regions, then deterministic cascades in original row/column/identity order. The complete branch either validates or is discarded. One proposal affects at most 64 nodes including the selected component, and search stops after 4096 candidate attempts with an explicit explanation. Locked nodes are never displaced. Swap requires one eligible occupant and validates both rectangles/container rules. Unavailable proposals are labeled, cannot commit, and retain a focusable explanation. The affected-items window lists every proposed identity/name, destination and rectangle before accepting the operation.

Duplication now builds and validates the entire copy plus free placement once; it no longer saves an unplaced intermediate component on section grids. Full grids refuse additions/duplicates rather than growing, overwriting or silently changing spans. Reorder refuses affected locks. Accepted add/drop/resize/shift/swap/transfer/order actions use the shared session and one undo record; no applied file is written before Apply.

## Tests and source identity

- Snapshot 31 (`efb556f853d75bc147ff58798dea11dca5e8b025fe428fcf5952fd4009a34592`): targeted testing exposed invalid synthetic v2 widget fixtures and a QJsonValueRef retained across removal in the new order command. Corrected the fixtures through ComponentSchema and retained an owned QJsonValue before mutating the array. Independent section-transfer and preexisting remote/pointer parity tests passed. No deployment.
- Snapshot 32 (`113defe8cd25ad3d8d727abe8bd7ed054fb61d897d133bb0c41278247704dc05`): all 26 component cases passed (2.684 seconds) and all 23 native navigation cases passed (16.535 seconds). Includes indirect cascades, deterministic replay, full-grid refusal, affected-node bound, locks, duplicate placement, cross-section move/swap, order, actual remote collision review/cancel/swap, repeated edge input, anchors, transfer cancel/undo, legacy flow ordering and existing pointer/numeric parity.
- Snapshot 33 (`1d64aef78d33df771aa478054826a7a0f17e8c798a1461f5b6b953a2cb62dae4`): same application code as snapshot 32, with the expanded installed Studio remote oracle and task entry metadata. Full application build and all 14 CTest suites passed in 60.76 seconds; the installed functional journey passed, but its visual review required the corrections below. Guest source/build/logs: `/home/isolde-dev/isolde-runs/studio-implementation-33/`; host source manifests: `build/studio-implementation-33/`.

The existing deterministic write-failure, profile invalidation, canonical replay, Kodi ownership and rollback regressions remain mandatory in full CTest. This task does not close nested/group multi-selection, full Layers, alignment/distribution, catalog/inspector, reference tools, offline Generator, physical device qualification or uncoached user acceptance; those retain their later backlog gates.

## Installed oracle extension

Use the existing sequential wrapper from `docs/vm-oracle.md` with `-Case studio` and an explicit frozen `-SourceRoot`. The Studio oracle now emits EDIT, COLLISION, RESIZE, STUDIO and APPLIED capture phases. The wrapper captures the actual VM framebuffer before acknowledging each phase; never create acknowledgements by hand without a capture.

The isolated journey reviews Swap, cancels it, accepts Swap and undoes it once; cancels a width change, accepts it and undoes it once; sends 20 movement steps into the bounded edge and cancels; then exercises history, optional Try interaction, Apply and reopen. Before Apply, the stored bytes must remain identical. After Apply, the exact expected original positions/spans are asserted, proving the extra gestures were undone/canceled. The original v1 backup must match the input. This uses a disposable source/clock fixture, never the Household layout or real media playback.

## Visual review follow-up

Snapshot 33 was installed as `3e65ccfeed340d8f14cbca3c`. The expanded installed remote oracle passed, including exact final placement assertions, under `/tmp/isolde-components-acceptance-x7y4np86`; host copies are in `build/studio-installed-33/`. Explicit screenshot review rejected the overlapping gesture captions and old base cards appearing beneath proposed collision positions. This was a functional pass, not final visual acceptance.

Snapshot 34 (`e4a23e94e6e97abdc8577dd80d1e789cebdf25d8f0be54739162de0186da00d8`) removes duplicated captions, uses quiet origin outlines, suppresses old rendered cards through transient editor view state, and renders actual affected components at proposed positions with bounded numbered captions. Its targeted visibility/pointer/transfer tests passed, followed by all 14 CTest suites (60.42 seconds). No installation; superseded by matching-scale refinement.

Snapshot 35 (`e59d294e1fbdaf78b2e85cda6c1e9d48d93a52c01389e1387e632328df001d22`) also uses the shared renderer's unit/background for ghosts and proposals, with regression assertions for identical scale. Targeted gesture/visibility/pointer/transfer checks passed (3.882 seconds); full application verification passed all 14 suites in 61.14 seconds. The final installed oracle and both reference pairs remain required.

Snapshot 36 (`4e3dae0879f82742a00a1cc2dfa77258bd78a7c56991c658356137b50773020b`) adds only cancellation test coverage: manifest comparison with snapshot 35 differs solely in `tests/tst_home_navigation.cpp`. All application/configuration/QML/oracle bytes are identical. It tests Back from move, resize, move options, anchors, geometry, keyboard, ordering, removal, Add, new-component placement, collision, affected-items review and destination selection without document/history changes. It was frozen but not built separately; the added cases are included in snapshot 37 instead. No separately installed application is claimed.
Snapshot 37 (`f6ca0071258dc1c31187b014810f1e6023807f2b999d4ade671476b34002188d`) includes the final cancellation tests plus a legacy-mode correction: a disabled composition with retained, fully placed grid metadata still edits its actual flow layout. Merely retaining metadata must not activate grid editing. The regression opens, selects and cancels that configuration without rewriting its bytes. Targeted gesture/cancellation/legacy/pointer tests passed (5.921 seconds), followed by all 14 CTest suites (58.75 seconds). Final installed evidence follows.
## Final installed acceptance

Installed final release `32aa32c0089df0a3684a858e`, previous `3e65ccfeed340d8f14cbca3c`; source snapshot 37. The complete installer independently rebuilt/retested, then the existing service was explicitly restarted. Startup configuration and personal profile/media data were preserved.

| Journey | Guest evidence | Host captures/results |
| --- | --- | --- |
| Edit, collision cancel/swap/undo, resize cancel/accept/undo, repeat/cancel, history, Try interaction, Apply/reopen | `/tmp/isolde-components-acceptance-52_xorqq` | `build/studio-installed-37/studio/` plus exact guest directory copy |
| Dark recreation, personal skin round trip, reset | `/tmp/isolde-reference-config-8hgsoxv8` | `build/studio-installed-37/lounge/` plus exact guest directory copy |
| Pastel recreation, personal skin round trip, reset | `/tmp/isolde-reference-config-9cbg5mra` | `build/studio-installed-37/pastel/` plus exact guest directory copy |

All nine captures were reviewed explicitly. Collision preview now shows the actual components at proposed positions without duplicate base cards or overlapping captions; the affected-items list retains full names/coordinates. Resize has one matching-scale ghost, a quiet original outline, one caption and readable cell/anchor feedback. Ordinary editing keeps the full viewport and named selection. Both reference recreation/reset pairs retain their generic geometry, art, labels and focus, and reset correctly. The tiny Studio fixture is an interaction test, not the finished six-family catalog or proof of Studio-authored reference reconstruction.

After the final run, `restoration.log` verifies active/running, NRestarts=0, correct current/previous links and `/proc/<MainPID>/exe` in the final release. `normal-home.png` was captured/inspected: the existing Household source-notice overlap remains T25; no directories or notices were deleted to conceal it. The optional Try interaction banner and overall cutover presentation remain T23-T24 work. Offline cold-boot Generator, hardware and genuine uncoached user acceptance are not claimed.