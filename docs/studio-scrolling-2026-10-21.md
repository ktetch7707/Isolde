# T19: optional long pages and no-room Add recovery

User authorized implementation of the planned Studio changes. This slice extends the current movement/Add workflow with optional root-page scrolling. It does not claim the remaining catalog, color picker, header, tool organization, templates or full Studio cutover gates.

## Implemented behavior

- Page appearance → Layout contains a genuine **Allow vertical scrolling** checkbox. Its compact operation panel previews changes through the existing proposal model; Done accepts one draft edit, Cancel restores the previous policy and viewport, and Apply remains explicit.
- New `composition.verticalScrolling` is a strictly validated boolean for composed screen-grid pages. Missing means unchanged existing behavior. Viewport `grid.rows` stays 3–10; enabled root placements can extend to a separate 10,240-row document bound. Each component remains at most ten rows tall, columns remain 4–16 and nested grids retain their existing limits. No startup/read migration or additional layout store is introduced.
- Runtime and Studio use the same cell metrics, content extent, scroll position and focus geometry. New rows extend below the TV instead of compressing existing cards. The canvas previews extra extent during Add/Move/Resize without adding stored empty rows. Root components outside a bounded viewport neighborhood do not instantiate renderers/provider clients; focus uses the complete validated placement model to reveal the appropriate row.
- Add suggests the first vacant suitable position. Occupied placements preview the same reflow as movement. A failed ordinary Add offers No room recovery rather than the legacy collision-choice dialog: enable root scrolling when applicable, choose another position, change size, choose another section/group, or cancel. The configured component survives those recovery choices.
- Enabling scrolling from a blocked Add is provisional. The checkbox policy and new component commit together as one validated draft transaction; cancel and one-step undo preserve the original fixed page. Existing components, sizes and locks are retained.
- Turning scrolling off refuses a layout that still extends below the first screen, with a specific reason; it never hides, shrinks or deletes content. Once the authored placements fit, disabling is previewable/undoable like enabling.
- Scrolling uses a quiet position indicator and the existing remote/spatial navigation and pointer viewport. The primary height slider remains bounded to viewport/container height. Ordinary movement avoids calculating an unused Swap proposal; advanced collision selection retains it.

## Verification

New native cases `studioScrollingPagesAreBoundedAndAtomic` and `studioScrollingAddMoveAndCancel` cover fixed-page refusal, strict typed bounds, policy preview/cancel, configuration retention, combined enable+Add, one-step undo/redo, saved reload, long movement, exact viewport cancellation and unsafe disable refusal. The navigation test checks actual rendered proposal and runtime card rectangles, not just computed geometry.

Intermediate runs and graphical review exposed real issues that were corrected before deployment: cached viewport transforms clipped editor proposals, Main omitted grid/placement bindings, and synchronous selection events during virtualized delegate creation reset runtime scrolling. The shared renderer now explicitly tracks scroll/viewport geometry and defers runtime focus reveal until delegate layout settles. Targeted `studio-scrolling-focus-20261021` passes with the original complete-card visibility assertions retained and no viewport-model binding-loop warnings.

Earlier full runs `studio-scrolling-20261021-b` and `studio-scrolling-final-20261021` passed native tests but their long-page visual captures were rejected. They are superseded and were never deployed. A premature rerun was automatically rejected while a known incorrect Undo-after-Apply test assertion remained; the test was corrected and read back before the authorized rerun. No assertion, approval or timeout boundary was bypassed.

Delivery run **`studio-scrolling-delivery-20261021`** passed **14/14 CTest suites in 97.60 seconds**. Archive SHA-256: `8df4d567cfc02a91ce2e697f446b944810c66c76a17c6085ff46f4698ba8017d`. Source digest: `5755070c97870d30ef7cfa978eb423484cae0b00f697a47ee35a5af7e5baea02`. All 14 implementation/test/tool files match its frozen manifest. Packaging also includes the pre-existing working-tree change to `src/home/PageData.h`; it remains outside this commit. Native logs/JUnit, graphical results and deployment identity are under `build/vm-oracle/studio-scrolling-delivery-20261021/report/`.

All **19 captured images were opened and visually reviewed**: Studio 15, lounge 2, pastel 2. The long-page component is fully visible at unchanged size during movement and after Apply in runtime Home. The checkbox is readable and the indicator tracks the extended page. Both standing recreation/reset pairs retain their established landscape adaptations against the retained references. This accepts this slice's visual regression gate, not all Studio usability: existing verbose inspector/preview copy and broader control redesign remain open.

Capture directories under that run's `visual/`:

- `studio/attempt-20261021-093709-842/`, guest `/tmp/isolde-components-acceptance-t0pcdtmb` (including `studio-scrollingoption.png`, `studio-scrolling.png`, `studio-scrollinghome.png`).
- `lounge/attempt-20261021-093854-698/`, guest `/tmp/isolde-reference-config-y1k0n_sd`.
- `pastel/attempt-20261021-093940-389/`, guest `/tmp/isolde-reference-config-utpr60eo`.

Each graphical case preserved normal `home-layout.json` SHA-256 `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e`. The canonical Deploy command activated the same tested payload as release **`6d558525d9ead04d09f31aa9`**, retained previous release `5064b3ae1002063c2d68e018`, and restarted `isolde.service`: active/running, PID 1405231, NRestarts 0. No VM reset/reboot or normal-profile layout edit occurred.

## Remaining scope and accounting

The new checkbox edits composed screen-grid pages. Existing scrolling-section/automatic layouts are preserved; changing those policies still requires their explicit layout/conversion workflow. Automatic packing into a single screen when disabling, creating a new page from the No room dialog, comprehensive pointer edge-growth/target parity and extended decorative overlap policies remain open. This does not claim uncoached usability, every 10,240-row performance case, hardware or disconnected cold boot acceptance.

T19.6/T20.5/T21.11 and the owning T23/T24 gates remain unchecked until their complete requirements pass. Current T19 83%, T20 80%, T21 36%; Overall round(849/24) = **35%** across 24 active tasks. This slice records concrete implementation/evidence without converting a partial gate into 100%.
