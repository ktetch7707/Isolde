# Page Studio wireframes — TV and desktop (T15.2)

Status: reviewed wireframe set, 2026-10-19, satisfying the screens listed in backlog T15.2 and
consistent with [page-studio-redesign.md](../../page-studio-redesign.md) §2-§4. ASCII labels
illustrate configurable content; no packaged app, entitlement or media claim is implied. Every
state below exists twice: a TV/remote form (one drawer or sheet at a time, canvas always visible)
and a desktop/pointer form (parallel panels; drag is an accelerator only — the same operations
complete with the remote, and both produce byte-identical canonical layout data).

Shared chrome (both form factors):

```text
Top bar:    Page Studio | Home v | Edit: Home / Sources widget | Draft: 3 changes
Toolbar:    Add | Pages | Layers | Undo | Redo | Grid | Fit | Preview | Apply
Footer:     current operation + applicable buttons, or selected item position readout
```

The top bar always names profile, page, draft state and nested scope. The footer shows the current
operation. Page-level tools live in the top bar; widget actions live beside the selection or in its
context menu. Editor chrome uses shared accessibility tokens independent of the draft preview, so a
bad draft can never make Apply/Discard/recovery unreadable.

## 1. Blank page

TV:

```text
+---------------------------------------------------------------------+
| Page Studio | My page | Edit: My page                  Draft: 0    |
| Add | Pages | Layers | Undo | Redo | Grid | Fit | Preview | Apply   |
+---------------------------------------------------------------------+
|                                                                     |
|   +---+---+---+---+---+---+---+---+   (visible empty-cell grid)     |
|   |   |   |   |   |   |   |   |   |                                 |
|   |   |  +------- Add widget ------+  ← focused, outlined button    |
|   |   |   |   |   |   |   |   |   |                                 |
|   +---+---+---+---+---+---+---+---+                                 |
|                                                                     |
| Footer: Empty page. OK opens Add.  Menu page actions.  Back exit.   |
+---------------------------------------------------------------------+
```

Desktop: same canvas centered; left Pages/Layers panels visible; right Inspector shows page fields
with "No component selected"; the Add button is a dashed drop target.

Rules: the real grid is drawn (not a placeholder); Settings/recovery stay reachable from every
state; empty pages are valid saved documents.

## 2. Existing page

TV: identical chrome; the canvas renders the actual page through the native renderer at fitted
camera; the last-focused runtime component is pre-selected with a visible outline and a close-up
chip when the fit would make it hard to inspect.

```text
|  +----------------+-----------------+                                |
|  | Hero           | Continue        |   ← components at live aspect |
|  |                | watching        |                                |
|  +----------------+-----------------+                                |
|  | [ Sources: selected, 2 x 1 ]     |   ← focus ring on selection   |
|  +--------------------------------------------------------- -------- |
| Footer: Sources · row 3, column 1, size 8 x 1 · OK pick up          |
```

Desktop: three-panel (Pages | canvas | Inspector showing the selection's Layout/Content/
Appearance/Actions/Accessibility groups, Basic first, Advanced expands the same inspector).

Fit/pan/zoom/guides are editor camera state only; they never reflow or rewrite the page.

## 3. Pages drawer (TV sheet / desktop panel)

```text
+---------------------------------------------------------------------+
| PAGES                          [ + New page ]   Search pages        |
| Home        2 changes                                       [edit]  |
| Library                                                [edit]        |
| My page   (thumbnail)                                  [edit]        |
|                                                                     |
| Actions on "My page": Open · Duplicate · Rename · Remove · Use for  |
| Movies · Link from Home                                             |
| OK choose · Menu actions · Back close (restores canvas selection)   |
+---------------------------------------------------------------------+
```

Create/duplicate/rename use the on-screen keyboard; presets (blank, Movies, Shows, standalone
Movies, copy of Home) are a named picker. Remove previews incoming tabs/buttons/library overrides
and offers remap or Cancel; page IDs are never typed. 64 pages/profile bound enforced with an
explanation. Dirty markers list which pages carry draft changes.

## 4. Add (catalog)

```text
+---------------------------------------------------------------------+
| ADD WIDGET          Search   [Layout][Navigation][Media][Text][Info] |
| +-----------+ +-----------+ +-----------+ +-----------+             |
| | Clock     | | App row   | | Movies    | | Text      |             |
| | (preview) | | (preview) | | shelf     | | note      |             |
| | Ready     | | Ready     | | Needs Kodi| | Ready     |             |
| +-----------+ +-----------+ +-----------+ +-----------+             |
| Footer: choose a widget · its source state shown on the tile        |
+---------------------------------------------------------------------+
```

The catalog is a TV sheet on TV, a palette on desktop. Every tile shows a live-ish preview, its
availability state and its variant choices (compact/poster/landscape/circular) before insertion.
Categories are catalog organization only. Only registered component capabilities appear; hardware
that is unavailable says so honestly (e.g. HDMI input panel: "Unavailable — no capture device").
After Confirm, the ghost preview carries the new widget to a valid cell before it joins the draft
(pick-up/drop state below). Search inside the catalog filters locally.

## 5. Selection and inspector

TV (selection first, inspector as one sheet at a time):

```text
|  [ Sources widget selected — row 3, column 1, 8 x 1 ]               |
| Footer: OK pick up · Menu actions · Back deselect                   |

+-- INSPECTOR SHEET (opened from Menu → Customize) -------------------+
| SOURCES WIDGET                       Basic ▸ Advanced               |
| Content    Library: Movies · View: recent                          |
| Action     Open Movies page                       [named picker]   |
| Appearance Variant: source ▸  Label ▸  Opacity ▸  Corners ▸        |
| Accessibility  Reading order: 4  ·  Announce: On                   |
| OK edit field · Left/Right adjust · Back close (restores selection)|
+---------------------------------------------------------------------+
```

Desktop: the same groups in the right panel; Advanced expands in place, never opens a second
editor. Raw IDs never appear — destinations, apps, collections and sources use named pickers.
Numeric fields open the on-screen keyboard; typed invalid values are refused with an explanation,
never clamped.

## 6. Pick-up / drop (moving)

```text
+---------------------------------------------------------------------+
|                 (dashed origin outline stays put)                   |
|   +----------------+  ⇢ ghost follows cell steps                    |
|   | (origin)       |     MOVING Sources · 8 x 1 · row 3, col 4      |
|   +----------------+     Valid position ✓                           |
| Footer: Arrows move · OK drop · Back cancel · Menu move-to/containers|
+---------------------------------------------------------------------+
```

The ghost moves exactly one bounded cell per press, keeping its span; occupied cells may be
traversed but cannot be silently overwritten. Draft changes only on drop; one undo entry per drop
regardless of repeats. Desktop drag produces the identical candidate operation; the origin outline
and ghost are shared visuals, so pointer and remote are visibly the same workflow.

## 7. Resize

```text
|  [ Sources widget RESIZING from top-left anchor ]                   |
|   width 8▸ ◀▶   height 1 ▲▼   (candidate shown hatched)             |
| Footer: Arrows resize · Menu anchor/numeric size · OK accept · Back |
```

Left/Right changes width, Up/Down changes height; top-left anchor by default, Menu offers other
anchors and numeric size via on-screen keyboard. Candidate rectangle is hatched until accepted;
overflow or collision routes to the collision dialog (§8). Back restores the pre-gesture size with
no undo entry.


## 8. Collision

```text
+-- COLLISION DIALOG --------------------------------------------+
| Sources (8 x 1) overlaps Clock at row 3, column 1              |
|                                                                 |
|  [ Shift others ]   [ Swap with Clock ]   [ Cancel ]           |
|                                                                 |
| Preview of the chosen proposal is drawn on the canvas behind.   |
| Footer: Left/Right choose · OK commit (one undo entry) · Back   |
+-----------------------------------------------------------------+
```

Shift shows a deterministic complete proposal for every affected widget (stable row/column/ID
ordering; refuse cascades beyond 64 affected nodes with the reason). Swap is enabled only when one
occupant exists and both resulting rectangles validate. If no valid proposal exists, Shift is
disabled with an explanation. Cancel restores every placement and focus. Occupied cells are never
silently overwritten.

## 9. Dirty exit

```text
+-- LEAVE PAGE STUDIO? -------------------------------------------+
| You have 3 unsaved changes on Home and 1 on My page.            |
|                                                                 |
|  [ Keep editing ]   [ Discard draft ]   [ Review & Apply ]      |
+-----------------------------------------------------------------+
```

Back first cancels an active gesture, then closes the current tool, then reaches this choice.
Nothing implicitly applies on Back; Discard requires a second confirmation naming the affected
pages; Review & Apply opens the Apply flow (§10). Preview/cancel writes nothing.

## 10. Preview and Apply

Preview (no editor chrome; small persistent "Draft preview" label; guaranteed Back return):

```text
+---------------------------------------------------------------------+
|  Draft preview · Back returns to editor (Sources selected)           |
|   (full-viewport native render of the draft)                         |
+---------------------------------------------------------------------+
```

Side-effecting actions (play, launches, installs, settings writes, network discovery) are
intercepted with an honest "Would invoke action…" explanation; try-navigation covers internal page
routes only.

Apply summary → error-possible result:

```text
+-- APPLY 3 CHANGES -----------------------------------------------+
| Home: move Sources, resize Clock                                  |
| My page: add Movies shelf                                         |
| [ Apply once ]                                                    |
+-- APPLY FAILED ----------------------------------+                 |
| Could not write layout (disk full). Your previous design is       |
| intact; the draft is kept.                                        |
| [ Try again ]   [ Return to editing ]                             |
+------------------------------------------------------------------+
```

Validation covers the complete document and managed assets before one atomic save; errors retain
the draft and the previous applied design. On success: "Applied", closed only on request, focus
restored to the exact originating runtime component (or the recovery control if it was removed).

## 11. Nested scope (containers / sub-pages)

```text
| Edit: Home ▸ Inputs ▸ HDMI panel                                   |
|  +--------------------------------+                               |
|  | HDMI 1  HDMI 2  HDMI 3  HDMI 4 |  ← local sub-grid (4x2)        |
|  | HDMI 5  HDMI 6  HDMI 7  HDMI 8 |                                |
|  +--------------------------------+                                |
| Footer: In "HDMI panel" · arrows edit local grid · Back to Home    |
```

Breadcrumbs name the scope; Back returns to the parent selection; container depth ≤ 4; moving a
container moves children intact; cycles/unsupported children/out-of-scope drops are refused.
Widget sub-grids (1-16 cols × 1-10 rows) are presentation data, not page-grid changes. Lock and
Hide are distinct reversible states; hidden components stay reachable in Layers with reveal.

## 12. Parity note

Desktop may show Pages, Layers and Inspector simultaneously and support drag/resize handles; TV
shows one drawer or sheet at a time. Both drive the same typed commands (add, move, resize, set
properties, resolve collision, group, reorder, link, delete) against one draft; canonical layout
bytes must be identical for identical semantic edits. Guides, zoom, panel sizes and pointer
positions are editor camera state and never enter saved data.

