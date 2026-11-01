# Revised interaction authority

[Page-first Studio](page-first-design.md) supersedes conflicting toolbar focus, page selection, modal stacking and exit transitions below. Canvas Confirm picks up; component Menu opens actions; page-list Confirm opens page actions. Back cancels field/gesture, closes context, returns to page menu, then offers dirty exit. Empty canvas and first-row Up always reach Pages/recovery. This revision requires fresh remote walkthrough evidence.

# Page Studio interaction states — remote state model (T15.3)

Status: provisional state model (designer self-review only; user acceptance deferred to T24),
2026-10-19, extending
[page-studio-redesign.md](../../page-studio-redesign.md) §4 into a complete walk-through. Remote
keys are the InputManager commands: `navigateUp/Down/Left/Right` (D-pad), `confirm` (OK/Enter),
`back` (Back/Escape), `menu` and `search` (Menu and Search; on remotes without dedicated keys these
are reachable per existing InputManager bindings). No long press, pointer emulation, number keys,
hidden shortcut or drag gesture is required anywhere. The canvas is in Edit mode: selecting a media
card can never launch it.

## 1. State inventory and transitions

```text
                    ┌────────────── OK on component ──────────────┐
                    ▼                                             │
  [Canvas browsing]──Menu──▶[Context menu]──Customize──▶[Inspector sheet]
        │  ▲                    │Move/Resize                         │
        │  │ OK drop/Back cancel│                                    │
        │  ▼                    ▼                                    │
  [Picked-up moving]     [Resizing]──collision──▶[Collision dialog]  │
        │                                                            │
        │Add (toolbar or Search)                                     │
        ▼                                                            │
  [Add catalog]──OK──▶[Ghost placement]──▶ back to Canvas browsing   │
  [Pages drawer] / [Layers drawer]  (toolbar; Back restores selection)│
  [Preview]  (toolbar; Back returns to exact origin)                 │
  [Apply summary]──▶[Applied] | [Apply error]──Return──▶[Canvas]     │
  [Field editing] (on-screen keyboard, entered from Inspector)       │
  [Dirty-exit dialog] (Back at canvas with unsaved draft)            │
```

Opening the editor writes nothing. Every accepted operation updates the live draft and one bounded
undo history (T16 budget rules; before exceeding 100 ops / 32 MiB: explain, offer Continue-with-
new-baseline or Cancel — never silently lose history).

## 2. Remote key contract by state

| State | D-pad | Confirm | Back | Menu / Search |
| --- | --- | --- | --- | --- |
| Canvas browsing | Spatial focus between components; Up at top row reaches toolbar; Down from bottom stays with boundary hint | Pick up focused component (→ Moving) | Dirty → dirty-exit dialog; clean → close, restore runtime origin | Menu: context menu for selection; Search: open Add catalog search |
| Moving | Ghost moves exactly one cell, span preserved; edge keeps last valid candidate + boundary explanation | Drop if valid; occupied → collision dialog | Cancel: origin and draft byte-identical, no undo entry, focus returns to origin component | Menu: "Move to…" row/column form or permitted destination container list |
| Resizing | Left/Right width, Up/Down height; top-left anchor default | Accept candidate size; collision → dialog | Restore pre-gesture size, no undo entry | Menu: choose resize anchor or numeric size via on-screen keyboard |
| Collision dialog | Left/Right choose Shift / Swap / Cancel; proposal previewed on canvas | Commit chosen valid proposal as one undo entry | Cancel candidate placement; all placements and focus restored | No hidden alternatives; Menu shows why a disabled option is disabled |
| Context menu | Navigate labeled items (Move, Resize, Customize, Duplicate, Hide, Lock, Remove, Enter contents, Move before/after) | Activate item (Move via menu == OK pick-up semantics) | Close menu, restore canvas selection | Context help for the focused item |
| Inspector sheet / drawer | Move between labeled controls; adjust only the active field | Activate or begin editing the field | Cancel field edit, then close sheet, restore selection | Context help; catalog search only when relevant |
| Field editing (keyboard) | Keyboard owns arrows per OnScreenKeyboard contract | Commit text to the active field | Close keyboard, keep field focus, value unchanged | Shift/symbols per keyboard; never obscures the edited field |
| Add catalog | Navigate tiles and category tabs | Choose tile → ghost placement; pick valid cell then confirm | Close catalog, restore previous selection | Search filters the catalog locally |
| Pages / Layers drawer | Navigate rows; Menu on a page offers its actions | Open/edit/act; canvas stays visible behind drawer | Close drawer, restore exact previous canvas selection | Search finds page/layer by name |
| Preview / try navigation | Follow actual page focus routes | Internal navigation; intercepted external actions explained honestly | Return to editor at exact origin (selection + camera) | Restore preview tools; no route into live settings mutation |
| Apply summary | Navigate summary entries and buttons | Apply once (atomic validate → save) | Back to editing, draft untouched | Selecting an error entry focuses its widget in the editor |
| Apply error | Navigate Retry / Return to editing | Activate choice | Return to editing (same as Return) | Error details; draft and previous design both retained |
| Dirty-exit dialog | Choose Keep editing / Discard draft / Review & Apply | Activate choice; Discard asks a second time naming pages | Keep editing (same as first choice) | None |

## 3. Focus restoration rules

Focus is always visible, restorable and never stranded:

| Return path | Restored focus |
| --- | --- |
| Back from Preview | The component that was selected when Preview opened; camera unchanged |
| Back from Inspector/drawer/dialog | The canvas component the sheet was opened from |
| Back from Field editing (keyboard) | The edited field retains focus; value unchanged on cancel |
| Cancel pick-up/resize | The origin component; draft byte-identical, no undo entry |
| Apply success → close | The exact originating runtime page/component; if it was removed, the permanent recovery control |
| Apply error → Return to editing | The widget/control named by the error |
| Profile switch mid-edit | Draft/clients invalidated after an explicit Apply/Discard choice; new profile starts at its own origin |
| Empty canvas / empty page | Toolbar remains reachable (Add first); Settings/recovery reachable from every state |
| Component disappears mid-session (uninstall, page removed elsewhere) | Nearest surviving neighbor with visible focus, plus an explanation |

Up from the first row always reaches the toolbar; the toolbar's Up stays put with a boundary hint;
Down from the toolbar returns to the canvas selection. Key repeat is bounded: it never skips
occupied cells, commits a candidate or auto-scrolls indefinitely.

## 4. Error feedback

- Blocking structural errors (invalid actions, unsafe paths, overflow/collision, bounds, revision
  conflict) prevent Apply; they appear in the Apply summary with a location and a repair link;
  selecting the error focuses the affected widget/control.
- Non-blocking issues (unavailable source, contrast warning) are marked on the component and in
  the inspector with clear locations and repair controls; they never silently disable Apply.
- Backend failures (`errorString()`) surface in the operation footer and in the affected sheet,
  always with the retry path; no error state relies on color alone (icon + text + focus ring).
- All refusals are explanations, never silent clamps: out-of-range typed values, over-limit
  selections, disabled collision options and edge-of-grid movement each say why.

## 5. Accessible movement alternatives (no drag required)

Every drag-equivalent has a non-pointer path, per the Atlassian accessibility guidance cited in
the spec:

1. **Pick-up/drop** — OK pick-up, D-pad, OK drop (primary remote path).
2. **Move to… form** — Menu → Move to… sets row/column numerically via the on-screen keyboard;
   validates and previews like a pick-up drop.
3. **Move before / Move after** — list-relative reorder in Layers and the context menu, for
   reading-order edits that do not need coordinates.
4. **Numeric resize** — Menu → Resize → numeric width/height, same validation as the gesture.
5. **Group selection** — Layers multi-select (≤ 32 nodes; refuse more with an explanation); one
   validated transaction, one undo record; group movement and collision resolution are atomic.
6. **Announcements** — every accepted operation announces the resulting position, collisions,
   successful drop and cancellation; outlines, hatching and text carry the same information as
   color.

Reduced-motion and high-contrast settings are respected in all states; the focus ring, ghost and
candidate hatch have validated contrast against both dark and pastel drafts.
