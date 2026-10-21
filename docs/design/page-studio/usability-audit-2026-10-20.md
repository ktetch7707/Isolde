# Studio usability audit and revised interaction contract

User feedback, 2026-10-20. This is a source/workflow audit plus design specification, not a new usability test or an implemented change. It supersedes conflicting Width/Height context entries, Move before/after workflows and mandatory ordinary-drop collision dialogs in earlier plans. Component semantics remain governed by [component contracts](component-contracts.md).

## Diagnosis

The editor currently exposes its implementation model before the user's intent. A person wants to choose a color, put an item somewhere, change its size or delete it. They should not need a color code, an ordering operation, a grid conversion or knowledge of which submenu owns deletion.

| Finding in current source | User cost | Required change |
| --- | --- | --- |
| `StudioInspector.qml` color choices use code strings as labels; Custom color opens a text keyboard | Color selection requires technical knowledge and recall | Named swatches, theme/recent colors and an interactive wheel; codes only in Advanced |
| `PageStudioView.qml` context is Move, Width, Height, Edit component | Size looks like a separate workflow; deletion is not discoverable there | Move, Edit component, Duplicate, Delete; Size is inside Edit component |
| Ordered-layout Move opens Move before/after or Convert to grid | The user translates a visual destination into a list operation | Direct insertion target with live reflow, keeping the existing layout model |
| Neighbor changes require Shift/Swap choice on ordinary collisions | Repeated dialogs interrupt a natural arrange-and-see loop | Visible deterministic reflow preview; accept on drop, cancel as a unit |
| Reading order and visual movement share terminology/operations | Rearranging focus order can appear to move a component, or vice versa | Visual Move is primary; advanced Focus order is explicitly separate |
| Inspector groups, technical terms and long footers compete with the canvas | The user loses the item they are designing and must remember state | A small intent-led context, short status, relevant basic controls, advanced disclosure and exact Back restoration |

Severity: color, moving, size location and deletion discoverability are high-priority authoring blockers. Successful screenshots and byte-parity tests did not establish usability of these workflows.

## One component context

Confirm selects the focused component and opens a compact named context:

```text
HDMI inputs · Button carousel

Move
Edit component
Duplicate
Delete…
```

The same menu is available via Menu on the focused component and pointer context interaction. No long press or hover-only feature is required. Confirm in Studio never launches the component action. Back closes the context to the exact selected item. Empty canvas opens Add component and Page appearance; it does not offer disabled component operations. Parent/child scope is named so Delete cannot ambiguously target a whole carousel instead of a child.

Delete is always discoverable in this context. For a simple component, name it in a compact confirmation, with Cancel initially selected; successful deletion is one undoable draft change and offers Undo. For a container or linked destination, explain child/link consequences before acceptance. Never delete apps, media or external files as a side effect. Locked items explain the lock and expose Unlock. After deletion focus goes to a deterministic sibling or Add component; Undo restores the deleted item and its selection.

## Move by showing the destination

Remote: select → Move → D-pad changes the target → Confirm drops, Back cancels. Pointer: dragging a selected item crosses a small movement threshold before entering the same Move transaction; clicking merely selects. Both display the real component, a clear insertion/drop indicator and a quiet original-position marker. Close property windows during movement so the authored page stays visible.

Neighbors visibly make room as the target changes. These are reversible proposals, not writes or accumulated history. Show which items will move; preserve the original snapshot throughout the gesture. A valid drop accepts the complete arrangement as one draft operation/undo record. Back restores every item and exact selection, including viewport. Reduced motion uses immediate transitions or stationary outlines without losing the target information.

- **Ordered rows/carousels/sections:** move an insertion marker between items; preview the resulting order. No Move before/after picker and no grid conversion are required. Keep collection membership and stable identities intact.
- **Grid layouts:** snap to meaningful targets and preview bounded neighbor displacement using a deterministic, stable-order solver. Preserve sizes, parent scopes and locked/pinned placements. Never overwrite, silently clamp, lose components or move unrelated sections. Do not cascade beyond existing bounds simply to force a fit.
- **Free/decorative layers:** moving a shape behind another component must remain possible. Honor the container/layout's explicit overlap and reflow policy; do not scatter decorative backgrounds as if they were flow items. Describe the target as an overlay/layer operation. Layout policy belongs to validated data and is editable under container/page Layout.
- **No valid target:** keep the invalid ghost visible with a plain reason such as “No room” or “Locked item.” Do not commit or silently change size. Offer Cancel, Choose another position, or a named alternative such as Swap only when needed. A confirmation dialog remains appropriate for ambiguous scope changes or an explicit alternate arrangement, not every ordinary occupied-cell drop.
- **Cross-container movement:** show “Move into [name]” as an explicit target and preview the complete result. Do not infer reparenting from incidental pointer overlap. Respect nesting/depth limits, locks and profile boundaries.

Default reflow is a visible part of the Move interaction, not a silent overwrite. Storage and history change only when the user accepts. Advanced Focus order remains independently inspectable; do not rename it Move before/after or make it the primary visual arrange tool. Existing saved ordered layouts remain ordered unless the user explicitly chooses a validated conversion.

## Size belongs in Edit component

Edit component opens controls for this component: **Content, Appearance, Size & layout, Action** where supported, then advanced controls. Do not present irrelevant groups or raw property names. All existing accessibility fields remain reachable, with sensible defaults.

Size & layout starts with a visible width and height control beside the actual item, not a detached technical form. Offer Smaller/Larger steps, width/height steppers, and a linked-proportions control for appropriate image/shape/card types. The proportions toggle must reflect its real state and refuse impossible sizes. Each adjustment updates the draft preview; accepting the size edit creates one history entry, and Back restores the entry size and affected neighbors.

Use the relevant language: carousel **Visible cards**, **Card size** and **Spacing** are separate from the carousel's **Width/Height** on the page. Auto-sized text exposes its sizing policy instead of a misleading editable height. Show grid units or exact measurements under Advanced; the visible result and plain-language dimensions are primary. Pointer resize handles are an optional accelerator to the same semantic operation. Bounds/collision consequences use the same visible proposal rules as Move. Changing carousel capacity must never delete its overflow items.

## Visual color picker

Every color field shows a labeled swatch with a readable role—Background, Text, Accent, Surface or Focus—and whether it follows the page theme. Open one picker with **Theme colors, Presets, Recent colors, Custom**. Presets have readable names and visible samples; never rely on hue alone to identify selection. Custom provides a color wheel plus brightness and opacity controls and a large before/after swatch. Opacity is explicitly separate from brightness. Keep the affected component/page visible and show contrast feedback for relevant text/focus pairs.

Pointer users can drag the wheel and sliders. Remote users can focus the wheel, Confirm to adjust, Left/Right change hue and Up/Down change saturation with a visible target; Back leaves adjustment before closing the picker. Brightness and opacity are separately focusable steppers/sliders with spoken/readable values. This must not require a mouse, a pixel-perfect target, a physical color key or hex entry. Named presets and grayscale choices remain useful alternatives for color-vision differences.

Within the picker, changes preview only; **Use color** accepts one draft edit, Cancel/Back restores the original inherited or explicit value. **Use page color** removes the override; it does not copy the current parent value. Hex/RGB/HSV input remains optional under Advanced for precision and paste. Mixed multi-selection values, inherited colors and transparent colors have distinct readable states. Recent colors are bounded profile-local convenience data, updated on accepted choices rather than every gesture; canceled colors must not pollute shared history.

Use the same picker for component/page colors, gradients and Studio chrome; no separate hex-only backdoor. A gradient's stops are separately labeled color controls with a visual preview. Do not redesign theme resolution or introduce a new store just to add the picker.

## Other usability corrections

- Name controls by intention: Choose apps, Edit buttons, Recent sources, Image, Label, Background, Size. Distinguish visible label, body text, editor name and accessibility name without requiring all four to be configured.
- Keep basic controls short and type-specific. Advanced reveals numeric positions, technical values, focus order and exceptional layout policies; it is never the only route to essential Move/Edit/Delete, colors or size.
- Show one brief operation/status line; errors sit beside the field or target. Avoid duplicate preview warnings, technical IDs and a long footer behind a context window. A failure keeps the draft and offers a concrete recovery action.
- Maintain full-size page geometry. A context or keyboard must avoid the selected item, or collapse to a small operation control with an explicit way to reopen it. Back restores the exact field, context, selection and viewport.
- Distinguish **Done** (finish this operation) from **Apply** (save the interface). Color, size, movement and deletion all follow the same draft/undo model. Ordinary editing must not require Try interaction.

## Acceptance and backlog ownership

T19.6: compact Move/Edit/Duplicate/Delete context and remote insertion/reflow/cancel behavior, including ordered layouts without forced conversion. T20.5: pointer parity, drag threshold, scope targets, stationary/reduced-motion previews and byte-identical transactions. T21.10: shared visual color picker including remote wheel, inheritance/cancel/history behavior. T21.11: size inside Edit component, understandable type-specific controls and the broader discovery/wording/state cleanup above. Implement in that priority order, then finish the previously reopened T21 carousel semantics; T22 waits for completed dependencies.

Test ordinary and blocked drops, multiple neighbors, mixed sizes, locked/pinned items, full grids, section boundaries, nested containers, carousel overflow, long labels, empty pages and profiles. Verify no write/history effects during previews; one undo record on accept; complete cancellation; schema bounds and failure rollback; remote/pointer parity. Capture both standing reference pairs plus actual first-entry, context, wheel, size, reflow and delete states at supported TV viewports.

T24 must record an uncoached remote and pointer journey: change a background to a chosen color without typing a code; move an item into an occupied area with visible neighbor adjustment; change its size through Edit component; delete it through the original context and undo. Record task completion, time/actions, help needed, wrong turns, accidental launches, unexpected rearrangements and lost focus. Any failure of discovery or consequence understanding reopens the relevant gate; do not invent performance or usability results from a passing screenshot test.
